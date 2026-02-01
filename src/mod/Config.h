#pragma once

#include <string>

namespace mclistener_ws_server {

struct Config {
    int version = 1;
    
    // 日志级别: "silent", "fatal", "error", "warn", "info", "debug", "trace"
    std::string logLevel = "info";
    
    // WebSocket 服务器配置
    std::string host = "0.0.0.0";
    int port = 60201;
    
    // 功能开关
    bool enablePlayerJoinBroadcast = true;
    bool enablePlayerLeaveBroadcast = true;
    bool enablePlayerChatBroadcast = true;
    bool enableReceiveGroupMessage = true;
    
    // 消息格式配置
    std::string groupMessageFormat = "§6§l[{group_name}]§r §b({group_id})§r §a§o{nickname}§r§f: {message}";
};

} // namespace mclistener_ws_server
