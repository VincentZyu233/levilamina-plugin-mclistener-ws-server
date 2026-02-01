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
    
    // 聊天捕获方式: "event", "hook_packet", "both"
    // - event: 使用 LeviLamina 的 PlayerChatEvent (可能被其他插件拦截)
    // - hook_packet: 直接 hook TextPacket 处理 (更可靠，但优先级较低)
    // - both: 同时使用两种方式 (可能导致重复消息)
    std::string chatCaptureMode = "event";
    
    // 消息格式配置
    std::string groupMessageFormat = "§6§l[{group_name}]§r §b({group_id})§r §a§o{nickname}§r§f: {message}";
};

} // namespace mclistener_ws_server
