#include "mod/WebSocketServer.h"
#include "mod/MclistenerWsServerMod.h"

#include <sstream>
#include <algorithm>
#include <cstring>

namespace mclistener_ws_server {

// WebSocket GUID (RFC 6455)
static const char* WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

WebSocketServer::WebSocketServer(const std::string& host, int port, MclistenerWsServerMod* mod)
    : mHost(host), mPort(port), mMod(mod) {
}

WebSocketServer::~WebSocketServer() {
    stop();
}

bool WebSocketServer::start() {
    // 初始化 Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        mMod->getSelf().getLogger().error("WSAStartup failed: {}", result);
        return false;
    }

    // 创建服务器 socket
    mServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (mServerSocket == INVALID_SOCKET) {
        mMod->getSelf().getLogger().error("Failed to create socket: {}", WSAGetLastError());
        WSACleanup();
        return false;
    }

    // 设置 socket 选项
    int opt = 1;
    setsockopt(mServerSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    // 绑定地址
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(static_cast<u_short>(mPort));
    
    if (mHost == "0.0.0.0") {
        serverAddr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, mHost.c_str(), &serverAddr.sin_addr);
    }

    if (bind(mServerSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        mMod->getSelf().getLogger().error("Bind failed: {}", WSAGetLastError());
        closesocket(mServerSocket);
        WSACleanup();
        return false;
    }

    // 开始监听
    if (listen(mServerSocket, SOMAXCONN) == SOCKET_ERROR) {
        mMod->getSelf().getLogger().error("Listen failed: {}", WSAGetLastError());
        closesocket(mServerSocket);
        WSACleanup();
        return false;
    }

    mRunning = true;
    mAcceptThread = std::thread(&WebSocketServer::acceptLoop, this);

    mMod->getSelf().getLogger().info("WebSocket server started on ws://{}:{}", mHost, mPort);
    return true;
}

void WebSocketServer::stop() {
    if (!mRunning) return;
    
    mRunning = false;

    // 关闭服务器 socket，这会让 accept() 返回
    if (mServerSocket != INVALID_SOCKET) {
        closesocket(mServerSocket);
        mServerSocket = INVALID_SOCKET;
    }

    // 关闭所有客户端连接
    {
        std::lock_guard<std::mutex> lock(mClientsMutex);
        for (SOCKET client : mClients) {
            closesocket(client);
        }
        mClients.clear();
    }

    // 等待接受线程结束
    if (mAcceptThread.joinable()) {
        mAcceptThread.join();
    }

    WSACleanup();
    mMod->getSelf().getLogger().info("WebSocket server stopped");
}

void WebSocketServer::broadcast(const std::string& message) {
    std::lock_guard<std::mutex> lock(mClientsMutex);
    
    std::vector<SOCKET> disconnected;
    
    for (SOCKET client : mClients) {
        if (!sendFrame(client, message)) {
            disconnected.push_back(client);
        }
    }
    
    // 移除断开的连接
    for (SOCKET client : disconnected) {
        mClients.erase(client);
        closesocket(client);
    }
}

void WebSocketServer::setMessageCallback(MessageCallback callback) {
    mMessageCallback = std::move(callback);
}

void WebSocketServer::acceptLoop() {
    while (mRunning) {
        sockaddr_in clientAddr{};
        int clientAddrLen = sizeof(clientAddr);
        
        SOCKET clientSocket = accept(mServerSocket, (sockaddr*)&clientAddr, &clientAddrLen);
        
        if (clientSocket == INVALID_SOCKET) {
            if (mRunning) {
                // 真正的错误
                int error = WSAGetLastError();
                if (error != WSAEINTR && error != WSAENOTSOCK) {
                    mMod->getSelf().getLogger().warn("Accept failed: {}", error);
                }
            }
            continue;
        }

        // 获取客户端 IP
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
        mMod->getSelf().getLogger().info("New connection from {}:{}", clientIP, ntohs(clientAddr.sin_port));

        // 在新线程中处理客户端
        std::thread([this, clientSocket]() {
            handleClient(clientSocket);
        }).detach();
    }
}

void WebSocketServer::handleClient(SOCKET clientSocket) {
    // 执行 WebSocket 握手
    if (!performHandshake(clientSocket)) {
        mMod->getSelf().getLogger().warn("WebSocket handshake failed");
        closesocket(clientSocket);
        return;
    }

    // 添加到客户端列表
    {
        std::lock_guard<std::mutex> lock(mClientsMutex);
        mClients.insert(clientSocket);
    }

    mMod->getSelf().getLogger().info("WebSocket client connected, total clients: {}", mClients.size());

    // 接收消息循环
    while (mRunning) {
        std::string message = receiveFrame(clientSocket);
        if (message.empty()) {
            break; // 连接关闭或错误
        }

        mMod->getSelf().getLogger().debug("Received message: {}", message);
        
        if (mMessageCallback) {
            try {
                mMessageCallback(message);
            } catch (const std::exception& e) {
                mMod->getSelf().getLogger().error("Error in message callback: {}", e.what());
            }
        }
    }

    // 从客户端列表中移除
    {
        std::lock_guard<std::mutex> lock(mClientsMutex);
        mClients.erase(clientSocket);
    }

    closesocket(clientSocket);
    mMod->getSelf().getLogger().info("WebSocket client disconnected, remaining clients: {}", mClients.size());
}

bool WebSocketServer::performHandshake(SOCKET clientSocket) {
    char buffer[4096];
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytesReceived <= 0) {
        return false;
    }
    
    buffer[bytesReceived] = '\0';
    std::string request(buffer);

    // 检查是否是 HTTP GET 请求
    if (request.find("GET ") != 0) {
        return false;
    }

    // 查找 Sec-WebSocket-Key
    std::string keyHeader = "Sec-WebSocket-Key: ";
    size_t keyPos = request.find(keyHeader);
    if (keyPos == std::string::npos) {
        return false;
    }

    size_t keyStart = keyPos + keyHeader.length();
    size_t keyEnd = request.find("\r\n", keyStart);
    if (keyEnd == std::string::npos) {
        return false;
    }

    std::string clientKey = request.substr(keyStart, keyEnd - keyStart);
    std::string acceptKey = computeAcceptKey(clientKey);

    // 构建响应
    std::ostringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n";
    response << "Upgrade: websocket\r\n";
    response << "Connection: Upgrade\r\n";
    response << "Sec-WebSocket-Accept: " << acceptKey << "\r\n";
    response << "\r\n";

    std::string responseStr = response.str();
    int bytesSent = send(clientSocket, responseStr.c_str(), static_cast<int>(responseStr.length()), 0);
    
    return bytesSent == static_cast<int>(responseStr.length());
}

bool WebSocketServer::sendFrame(SOCKET clientSocket, const std::string& message) {
    std::vector<unsigned char> frame;
    
    // FIN + Text frame opcode
    frame.push_back(0x81);
    
    size_t length = message.length();
    
    if (length <= 125) {
        frame.push_back(static_cast<unsigned char>(length));
    } else if (length <= 65535) {
        frame.push_back(126);
        frame.push_back(static_cast<unsigned char>((length >> 8) & 0xFF));
        frame.push_back(static_cast<unsigned char>(length & 0xFF));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<unsigned char>((length >> (i * 8)) & 0xFF));
        }
    }
    
    // 添加消息内容
    frame.insert(frame.end(), message.begin(), message.end());
    
    int bytesSent = send(clientSocket, reinterpret_cast<const char*>(frame.data()), static_cast<int>(frame.size()), 0);
    return bytesSent == static_cast<int>(frame.size());
}

std::string WebSocketServer::receiveFrame(SOCKET clientSocket) {
    unsigned char header[2];
    int bytesReceived = recv(clientSocket, reinterpret_cast<char*>(header), 2, 0);
    
    if (bytesReceived != 2) {
        return "";
    }

    // 检查是否是关闭帧
    unsigned char opcode = header[0] & 0x0F;
    if (opcode == 0x08) {
        return ""; // 连接关闭
    }

    bool masked = (header[1] & 0x80) != 0;
    uint64_t payloadLength = header[1] & 0x7F;

    if (payloadLength == 126) {
        unsigned char extLength[2];
        if (recv(clientSocket, reinterpret_cast<char*>(extLength), 2, 0) != 2) {
            return "";
        }
        payloadLength = (static_cast<uint64_t>(extLength[0]) << 8) | extLength[1];
    } else if (payloadLength == 127) {
        unsigned char extLength[8];
        if (recv(clientSocket, reinterpret_cast<char*>(extLength), 8, 0) != 8) {
            return "";
        }
        payloadLength = 0;
        for (int i = 0; i < 8; ++i) {
            payloadLength = (payloadLength << 8) | extLength[i];
        }
    }

    // 读取掩码（如果有）
    unsigned char mask[4] = {0};
    if (masked) {
        if (recv(clientSocket, reinterpret_cast<char*>(mask), 4, 0) != 4) {
            return "";
        }
    }

    // 读取负载
    if (payloadLength > 1024 * 1024) { // 限制最大 1MB
        return "";
    }

    std::vector<char> payload(static_cast<size_t>(payloadLength));
    size_t totalReceived = 0;
    
    while (totalReceived < payloadLength) {
        int remaining = static_cast<int>(payloadLength - totalReceived);
        int received = recv(clientSocket, payload.data() + totalReceived, remaining, 0);
        if (received <= 0) {
            return "";
        }
        totalReceived += received;
    }

    // 解码消息（如果有掩码）
    if (masked) {
        for (size_t i = 0; i < payloadLength; ++i) {
            payload[i] ^= mask[i % 4];
        }
    }

    return std::string(payload.begin(), payload.end());
}

std::string WebSocketServer::computeAcceptKey(const std::string& clientKey) {
    std::string combined = clientKey + WS_GUID;
    
    unsigned char hash[20];
    sha1(combined, hash);
    
    return base64Encode(hash, 20);
}

// 简单的 SHA1 实现
void WebSocketServer::sha1(const std::string& input, unsigned char output[20]) {
    uint32_t h0 = 0x67452301;
    uint32_t h1 = 0xEFCDAB89;
    uint32_t h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476;
    uint32_t h4 = 0xC3D2E1F0;

    // 填充消息
    std::vector<unsigned char> msg(input.begin(), input.end());
    uint64_t originalBitLen = msg.size() * 8;
    
    msg.push_back(0x80);
    while ((msg.size() % 64) != 56) {
        msg.push_back(0x00);
    }
    
    for (int i = 7; i >= 0; --i) {
        msg.push_back(static_cast<unsigned char>((originalBitLen >> (i * 8)) & 0xFF));
    }

    // 处理每个 512 位块
    for (size_t chunk = 0; chunk < msg.size(); chunk += 64) {
        uint32_t w[80];
        
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint32_t>(msg[chunk + i * 4]) << 24) |
                   (static_cast<uint32_t>(msg[chunk + i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(msg[chunk + i * 4 + 2]) << 8) |
                   (static_cast<uint32_t>(msg[chunk + i * 4 + 3]));
        }
        
        for (int i = 16; i < 80; ++i) {
            uint32_t temp = w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16];
            w[i] = (temp << 1) | (temp >> 31);
        }

        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;

        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }

            uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
            e = d;
            d = c;
            c = (b << 30) | (b >> 2);
            b = a;
            a = temp;
        }

        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }

    // 输出哈希值
    output[0] = static_cast<unsigned char>((h0 >> 24) & 0xFF);
    output[1] = static_cast<unsigned char>((h0 >> 16) & 0xFF);
    output[2] = static_cast<unsigned char>((h0 >> 8) & 0xFF);
    output[3] = static_cast<unsigned char>(h0 & 0xFF);
    output[4] = static_cast<unsigned char>((h1 >> 24) & 0xFF);
    output[5] = static_cast<unsigned char>((h1 >> 16) & 0xFF);
    output[6] = static_cast<unsigned char>((h1 >> 8) & 0xFF);
    output[7] = static_cast<unsigned char>(h1 & 0xFF);
    output[8] = static_cast<unsigned char>((h2 >> 24) & 0xFF);
    output[9] = static_cast<unsigned char>((h2 >> 16) & 0xFF);
    output[10] = static_cast<unsigned char>((h2 >> 8) & 0xFF);
    output[11] = static_cast<unsigned char>(h2 & 0xFF);
    output[12] = static_cast<unsigned char>((h3 >> 24) & 0xFF);
    output[13] = static_cast<unsigned char>((h3 >> 16) & 0xFF);
    output[14] = static_cast<unsigned char>((h3 >> 8) & 0xFF);
    output[15] = static_cast<unsigned char>(h3 & 0xFF);
    output[16] = static_cast<unsigned char>((h4 >> 24) & 0xFF);
    output[17] = static_cast<unsigned char>((h4 >> 16) & 0xFF);
    output[18] = static_cast<unsigned char>((h4 >> 8) & 0xFF);
    output[19] = static_cast<unsigned char>(h4 & 0xFF);
}

std::string WebSocketServer::base64Encode(const unsigned char* data, size_t length) {
    static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string result;
    result.reserve(((length + 2) / 3) * 4);

    for (size_t i = 0; i < length; i += 3) {
        unsigned int n = static_cast<unsigned int>(data[i]) << 16;
        if (i + 1 < length) n |= static_cast<unsigned int>(data[i + 1]) << 8;
        if (i + 2 < length) n |= static_cast<unsigned int>(data[i + 2]);

        result += chars[(n >> 18) & 0x3F];
        result += chars[(n >> 12) & 0x3F];
        result += (i + 1 < length) ? chars[(n >> 6) & 0x3F] : '=';
        result += (i + 2 < length) ? chars[n & 0x3F] : '=';
    }

    return result;
}

} // namespace mclistener_ws_server
