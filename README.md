![levilamina-plugin-mclistener-ws-server](https://socialify.git.ci/VincentZyuApps/levilamina-plugin-mclistener-ws-server/image?custom_description=%E7%BE%A4%E6%9C%8D%E4%BA%92%E9%80%9A%EF%BC%8C%E6%9C%8D%E5%8A%A1%E7%AB%AF%E6%98%AFLeviMania%EF%BC%8C%E5%9F%BA%E5%B2%A9%E7%89%88%E6%9C%8D%E5%8A%A1%E7%AB%AF%EF%BC%8C%E6%9C%8D%E5%8A%A1%E7%AB%AF%E7%8E%A9%E5%AE%B6%E8%BF%9B%E5%87%BA%E5%92%8C%E8%81%8A%E5%A4%A9%E8%BD%AC%E5%8F%91%E5%88%B0%E8%81%8A%E5%A4%A9%E5%B9%B3%E5%8F%B0%EF%BC%8C%E8%81%8A%E5%A4%A9%E5%B9%B3%E5%8F%B0%E4%BF%A1%E6%81%AF%E8%BD%AC%E5%8F%91%E5%88%B0%E6%9C%8D%E5%8A%A1%E7%AB%AF+&description=1&font=Bitter&forks=1&issues=1&language=1&logo=https%3A%2F%2Favatars.githubusercontent.com%2Fu%2F78095377%3Fs%3D200%26v%3D4&name=1&owner=1&pulls=1&stargazers=1&theme=Auto)

# mclistener-ws-server

```shell

 _      _______      ________  ______   _____  _____
| | /| / / ___/_____/ ___/ _ \/ ___/ | / / _ \/ ___/
| |/ |/ (__  )_____(__  )  __/ /   | |/ /  __/ /
|__/|__/____/     /____/\___/_/    |___/\___/_/

                         ___      __
   ____ ___  _____      / (_)____/ /____  ____  ___  _____
  / __ `__ \/ ___/_____/ / / ___/ __/ _ \/ __ \/ _ \/ ___/
 / / / / / / /__/_____/ / (__  ) /_/  __/ / / /  __/ /
/_/ /_/ /_/\___/     /_/_/____/\__/\___/_/ /_/\___/_/
```

一个用于 LeviLamina (基岩版 Minecraft 服务端) 的 WebSocket 服务端插件，用于实现群服互通功能。

> 💡 **快速开始**：本插件是 WebSocket 服务端，需要配合客户端使用。推荐使用 [koishi-plugin-mclistener-ws-client](https://gitee.com/vincent-zyu/koishi-plugin-mclistener-ws-client) 对接 QQ、Discord 等聊天平台！

## 功能特性

- 🎮 **玩家进出服务器通知** - 当玩家加入或离开服务器时，通过 WebSocket 广播消息
- 💬 **玩家聊天转发** - 将服务器内的玩家聊天消息转发到 WebSocket 客户端
- 📨 **群聊消息接收** - 接收来自聊天平台（如 QQ 群）的消息并在游戏内广播
- ⚙️ **可配置** - 支持自定义端口、消息格式等

## 兼容性

本插件设计用于与 [koishi-plugin-mclistener-ws-client](https://gitee.com/vincent-zyu/koishi-plugin-mclistener-ws-client) 配合使用，实现 Minecraft 基岩版服务器与 QQ 群等聊天平台的消息互通。

## WebSocket 协议

### 服务端 → 客户端消息

**玩家加入服务器：**
```json
{
    "type": "player_join",
    "player_name": "玩家名"
}
```

**玩家离开服务器：**
```json
{
    "type": "player_leave",
    "player_name": "玩家名"
}
```

**玩家聊天消息：**
```json
{
    "type": "player_msg",
    "player_name": "玩家名",
    "content": "消息内容"
}
```

### 客户端 → 服务端消息

**群聊消息转发到服务器：**
```json
{
    "type": "group_to_server",
    "group_id": "群号",
    "group_name": "群名",
    "nickname": "发送者昵称",
    "message": "消息内容"
}
```

## 安装

### 前置要求

- LeviLamina 1.8.x 或更高版本
- Minecraft 基岩版专用服务器 (BDS)

### 安装步骤

1. 下载最新的 Release 版本
2. 将 `mclistener-ws-server` 文件夹复制到服务器的 `plugins/` 目录
3. 重启服务器

### 使用 lip 安装

```bash
lip install gitee.com/vincent-zyu/levilamina-plugin-mclistener-ws-server
```

## 配置文件

配置文件位于 `plugins/mclistener-ws-server/config/config.json`：

```json
{
    "version": 1,
    "host": "0.0.0.0",
    "port": 8766,
    "enablePlayerJoinBroadcast": true,
    "enablePlayerLeaveBroadcast": true,
    "enablePlayerChatBroadcast": true,
    "enableReceiveGroupMessage": true,
    "groupMessageFormat": "§6§l[{group_name}]§r §b({group_id})§r §a§o{nickname}§r§f: {message}"
}
```

### 配置项说明

| 配置项 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `version` | int | 1 | 配置文件版本号 |
| `host` | string | "0.0.0.0" | WebSocket 服务器监听地址 |
| `port` | int | 8766 | WebSocket 服务器端口 |
| `enablePlayerJoinBroadcast` | bool | true | 是否广播玩家加入消息 |
| `enablePlayerLeaveBroadcast` | bool | true | 是否广播玩家离开消息 |
| `enablePlayerChatBroadcast` | bool | true | 是否广播玩家聊天消息 |
| `enableReceiveGroupMessage` | bool | true | 是否接收群聊消息 |
| `groupMessageFormat` | string | 见上方 | 群聊消息在游戏内的显示格式 |

### 消息格式占位符

在 `groupMessageFormat` 中可以使用以下占位符：

- `{group_id}` - 群号
- `{group_name}` - 群名
- `{nickname}` - 发送者昵称
- `{message}` - 消息内容

### Minecraft 颜色代码

- `§0` - 黑色
- `§1` - 深蓝色
- `§2` - 深绿色
- `§3` - 深青色
- `§4` - 深红色
- `§5` - 紫色
- `§6` - 金色
- `§7` - 灰色
- `§8` - 深灰色
- `§9` - 蓝色
- `§a` - 绿色
- `§b` - 青色
- `§c` - 红色
- `§d` - 粉色
- `§e` - 黄色
- `§f` - 白色
- `§l` - 粗体
- `§o` - 斜体
- `§r` - 重置格式

## 构建

### 前置要求

- [xmake](https://xmake.io/)
- Visual Studio 2022 (MSVC)
- Windows 11 SDK

### 构建步骤

```bash
# 更新仓库
xmake repo -u

# 配置构建
xmake f -y -p windows -a x64 -m release

# 构建
xmake
```

构建完成后，插件文件将位于 `bin/mclistener-ws-server/` 目录。

## 与 koishi 插件配合使用

1. 在 koishi 中安装 `mclistener-ws-client` 插件
2. 配置 koishi 插件的 WebSocket 服务器地址为 `ws://你的服务器IP:8766`
3. 配置消息转发规则
4. 启动服务器和 koishi 机器人

## 许可证

MIT License

## 鸣谢

- [LeviLamina](https://github.com/LiteLDev/LeviLamina) - 基岩版模组加载器
- [koishi-plugin-mclistener-ws-client](https://gitee.com/vincent-zyu/koishi-plugin-mclistener-ws-client) - koishi 群服互通客户端插件
