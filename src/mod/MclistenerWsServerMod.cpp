#include "mod/MclistenerWsServerMod.h"
#include "mod/WebSocketServer.h"

#include "ll/api/mod/RegisterHelper.h"
#include "ll/api/Config.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/player/PlayerJoinEvent.h"
#include "ll/api/event/player/PlayerLeaveEvent.h"
#include "ll/api/event/player/PlayerChatEvent.h"
#include "ll/api/service/Bedrock.h"

#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"

#include <nlohmann/json.hpp>

namespace mclistener_ws_server {

MclistenerWsServerMod& MclistenerWsServerMod::getInstance() {
    static MclistenerWsServerMod instance;
    return instance;
}

bool MclistenerWsServerMod::load() {
    getSelf().getLogger().info("Loading mclistener-ws-server...");

    // 读取配置文件
    const auto& configFilePath = getSelf().getConfigDir() / "config.json";
    if (!ll::config::loadConfig(mConfig, configFilePath)) {
        getSelf().getLogger().warn("Cannot load configurations from {}", configFilePath.string());
        getSelf().getLogger().info("Saving default configurations...");
        if (!ll::config::saveConfig(mConfig, configFilePath)) {
            getSelf().getLogger().error("Failed to save default configurations!");
        }
    }

    getSelf().getLogger().info("mclistener-ws-server loaded successfully!");
    return true;
}

bool MclistenerWsServerMod::enable() {
    getSelf().getLogger().info("Enabling mclistener-ws-server...");

    // 创建并启动 WebSocket 服务器
    mWsServer = std::make_unique<WebSocketServer>(mConfig.host, mConfig.port, this);
    
    if (!mWsServer->start()) {
        getSelf().getLogger().error("Failed to start WebSocket server!");
        return false;
    }

    // 设置消息回调 - 处理从聊天平台来的消息
    if (mConfig.enableReceiveGroupMessage) {
        mWsServer->setMessageCallback([this](const std::string& message) {
            try {
                auto json = nlohmann::json::parse(message);
                std::string type = json.value("type", "");
                
                if (type == "group_to_server") {
                    std::string groupId = json.value("group_id", "");
                    std::string groupName = json.value("group_name", "");
                    std::string nickname = json.value("nickname", "未知用户");
                    std::string content = json.value("message", "");

                    // 使用配置的消息格式
                    std::string formattedMsg = mConfig.groupMessageFormat;
                    
                    // 替换占位符
                    size_t pos;
                    while ((pos = formattedMsg.find("{group_id}")) != std::string::npos) {
                        formattedMsg.replace(pos, 10, groupId);
                    }
                    while ((pos = formattedMsg.find("{group_name}")) != std::string::npos) {
                        formattedMsg.replace(pos, 12, groupName);
                    }
                    while ((pos = formattedMsg.find("{nickname}")) != std::string::npos) {
                        formattedMsg.replace(pos, 10, nickname);
                    }
                    while ((pos = formattedMsg.find("{message}")) != std::string::npos) {
                        formattedMsg.replace(pos, 9, content);
                    }

                    // 在游戏中广播消息
                    auto level = ll::service::getLevel();
                    if (level) {
                        // 使用 Level::broadcastMessage 或直接告诉所有玩家
                        level->forEachPlayer([&formattedMsg](Player& player) -> bool {
                            player.sendMessage(formattedMsg);
                            return true; // 继续遍历
                        });
                    }

                    getSelf().getLogger().info("Received group message: [{}] {}: {}", groupName, nickname, content);
                }
            } catch (const std::exception& e) {
                getSelf().getLogger().error("Failed to parse message: {}", e.what());
            }
        });
    }

    auto& eventBus = ll::event::EventBus::getInstance();

    // 订阅玩家加入事件
    if (mConfig.enablePlayerJoinBroadcast) {
        mPlayerJoinListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>(
            [this](ll::event::PlayerJoinEvent& event) {
                auto& player = event.self();
                std::string playerName = player.getRealName();

                nlohmann::json msg;
                msg["type"] = "player_join";
                msg["player_name"] = playerName;

                mWsServer->broadcast(msg.dump());
                getSelf().getLogger().info("Player {} joined, broadcasted to WebSocket clients", playerName);
            }
        );
    }

    // 订阅玩家离开事件
    if (mConfig.enablePlayerLeaveBroadcast) {
        mPlayerLeaveListener = eventBus.emplaceListener<ll::event::PlayerLeaveEvent>(
            [this](ll::event::PlayerLeaveEvent& event) {
                auto& player = event.self();
                std::string playerName = player.getRealName();

                nlohmann::json msg;
                msg["type"] = "player_leave";
                msg["player_name"] = playerName;

                mWsServer->broadcast(msg.dump());
                getSelf().getLogger().info("Player {} left, broadcasted to WebSocket clients", playerName);
            }
        );
    }

    // 订阅玩家聊天事件
    if (mConfig.enablePlayerChatBroadcast) {
        mPlayerChatListener = eventBus.emplaceListener<ll::event::PlayerChatEvent>(
            [this](ll::event::PlayerChatEvent& event) {
                auto& player = event.self();
                std::string playerName = player.getRealName();
                std::string message = event.message();

                nlohmann::json msg;
                msg["type"] = "player_msg";
                msg["player_name"] = playerName;
                msg["content"] = message;

                mWsServer->broadcast(msg.dump());
                getSelf().getLogger().debug("Player {} said: {}, broadcasted to WebSocket clients", playerName, message);
            }
        );
    }

    getSelf().getLogger().info("mclistener-ws-server enabled successfully!");
    return true;
}

bool MclistenerWsServerMod::disable() {
    getSelf().getLogger().info("Disabling mclistener-ws-server...");

    auto& eventBus = ll::event::EventBus::getInstance();

    // 取消订阅事件
    if (mPlayerJoinListener) {
        eventBus.removeListener(mPlayerJoinListener);
        mPlayerJoinListener = nullptr;
    }

    if (mPlayerLeaveListener) {
        eventBus.removeListener(mPlayerLeaveListener);
        mPlayerLeaveListener = nullptr;
    }

    if (mPlayerChatListener) {
        eventBus.removeListener(mPlayerChatListener);
        mPlayerChatListener = nullptr;
    }

    // 停止 WebSocket 服务器
    if (mWsServer) {
        mWsServer->stop();
        mWsServer.reset();
    }

    getSelf().getLogger().info("mclistener-ws-server disabled successfully!");
    return true;
}

} // namespace mclistener_ws_server

LL_REGISTER_MOD(mclistener_ws_server::MclistenerWsServerMod, mclistener_ws_server::MclistenerWsServerMod::getInstance());
