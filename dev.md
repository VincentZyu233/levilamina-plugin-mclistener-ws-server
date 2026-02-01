# 开发文档

## 关于 LeviLamina 插件开发

### 为什么必须用 C++？

LeviLamina 是一个 **native mod loader**，它通过 DLL 注入的方式加载插件。插件必须编译成 **Windows DLL (.dll)** 格式，因为：

1. Minecraft 基岩版专用服务器 (BDS) **只有 Windows 版本**
2. LeviLamina 使用 C++ 直接 hook 游戏的函数
3. 需要调用 BDS 的内部 API，这些 API 没有其他语言的绑定

### 编译产物

编译后会生成：
```
bin/mclistener-ws-server/
├── mclistener-ws-server.dll    # 插件主体（Windows 动态链接库）
└── manifest.json               # 插件清单文件
```

### 如何给服务端使用

将整个 `mclistener-ws-server` 文件夹复制到服务端的 `plugins/` 目录：
```
BDS服务端/
├── bedrock_server.exe
├── bedrock_server_mod.exe      # LeviLamina 启动器
├── plugins/
│   └── mclistener-ws-server/   # 复制到这里
│       ├── mclistener-ws-server.dll
│       └── manifest.json
└── ...
```

---

## 编译环境

### ❌ WSL 不能用于编译

很遗憾，**WSL 不能直接用于编译 LeviLamina 插件**，原因是：

1. LeviLamina 依赖 **MSVC (Microsoft Visual C++)** 编译器
2. 需要 Windows SDK 头文件和库
3. xmake 的 LeviLamina 包只支持 Windows 平台
4. 即使用 MinGW 交叉编译，也无法正确链接 LeviLamina 的库

### ✅ 必须在 Windows 上编译

#### 方法一：Visual Studio 2022（推荐）

1. 安装 [Visual Studio 2022 Community](https://visualstudio.microsoft.com/)
   - 勾选 "C++ 桌面开发" 工作负载
   - 确保安装最新的 MSVC 和 Windows SDK

2. 安装 [xmake](https://xmake.io/#/guide/installation)
   ```powershell
   # 使用 winget 安装
   winget install xmake
   
   # 或者使用 scoop
   scoop install xmake
   ```

3. 编译
   ```powershell
   cd D:\aaaStuffsaaa\from_git\gitee\levilamina-plugin-mclistener-ws-server
   
   # 更新仓库
   xmake repo -u
   
   # 配置（首次需要下载依赖，可能需要代理）
   xmake f -y -p windows -a x64 -m release
   
   # 编译
   xmake
   ```

#### 方法二：只安装 Build Tools（轻量一点）

如果不想装完整的 Visual Studio，可以只装 Build Tools：

1. 下载 [Visual Studio Build Tools 2022](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022)
2. 安装时选择 "C++ 生成工具"
3. 同样需要安装 xmake

#### 方法三：GitHub Actions 云编译（推荐懒人）

不想配置本地环境？可以用 GitHub Actions 自动编译！

```shell
git commit -m "build version 0.0.1-alpha.1"
# 或者
git commit -m "fix: 修复bug build version 0.0.2"
```

创建 `.github/workflows/build.yml`：

```yaml
name: Build

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]
  workflow_dispatch:

jobs:
  build:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4

      - name: Setup xmake
        uses: xmake-io/github-action-setup-xmake@v1

      - name: Update xmake repo
        run: xmake repo -u

      - name: Configure
        run: xmake f -y -p windows -a x64 -m release

      - name: Build
        run: xmake

      - name: Upload artifact
        uses: actions/upload-artifact@v4
        with:
          name: mclistener-ws-server
          path: bin/mclistener-ws-server/
```

然后：
1. 把代码推送到 GitHub
2. 每次 push 会自动编译
3. 在 Actions 页面下载编译好的插件

---

## 如果实在不想用 C++...

### 替代方案：使用脚本语言插件

LeviLamina 有一些脚本语言支持插件，但功能有限：

1. **[legacy-script-engine-quickjs](https://github.com/LiteLDev/legacy-script-engine-quickjs)** - JavaScript 支持
2. **[legacy-script-engine-lua](https://github.com/LiteLDev/legacy-script-engine-lua)** - Lua 支持

但是这些脚本引擎：
- 功能不如原生 C++ 完整
- 文档较少
- WebSocket 服务器可能不容易实现

### 替代方案：外部程序 + RCON

另一个思路是：
1. 用 Python/Node.js 写一个独立的 WebSocket 服务器
2. 通过 BDS 的 RCON 协议与服务器通信
3. 但这样无法直接监听玩家事件，需要解析服务器日志

---

## 关于 Windows C++ 开发环境

我理解 Windows 上配置 C++ 开发环境确实比较麻烦，但 LeviLamina 开发就是需要这些：

### 最小安装清单

| 组件 | 大小 | 必需？ |
|------|------|--------|
| Visual Studio Build Tools 2022 | ~2-3 GB | ✅ 必需 |
| Windows 11 SDK | ~2 GB | ✅ 必需 |
| xmake | ~10 MB | ✅ 必需 |
| VS Code + clangd 扩展 | ~500 MB | ❌ 可选（用于代码补全） |

总共约 **5-6 GB**，确实不小...

### 我的建议

如果你：
- **不想配置本地环境** → 用 GitHub Actions 云编译
- **偶尔需要编译** → 用 VS Build Tools + xmake
- **经常开发调试** → 装完整的 VS 2022

---

## 快速开始

假设你已经安装好了环境：

```powershell
# 1. 进入项目目录
cd D:\aaaStuffsaaa\from_git\gitee\levilamina-plugin-mclistener-ws-server

# 2. 首次配置（会下载 LeviLamina SDK，需要一段时间）
xmake repo -u
xmake f -y -p windows -a x64 -m release

# 3. 编译
xmake

# 4. 编译产物在 bin/ 目录
dir bin\mclistener-ws-server\
```

如果下载依赖失败，可能需要配置代理：
```powershell
# 使用 GitHub 镜像
xmake g --proxy_pac=github_mirror.lua

# 或者使用 HTTP 代理
xmake g --proxy="http://127.0.0.1:7890"
```

---

## 常见问题

### Q: 编译报错 "xxx is not a member of std"
A: MSVC 或 Windows SDK 版本太旧，更新到最新版本

### Q: xmake 下载依赖失败
A: 配置代理或使用镜像，参考上面的命令

### Q: 插件加载失败
A: 检查：
1. LeviLamina 版本是否匹配（本插件需要 1.8.x）
2. DLL 和 manifest.json 是否都在 plugins/mclistener-ws-server/ 目录下
3. 查看服务器日志的错误信息

---

## 参考链接

- [LeviLamina 文档](https://lamina.levimc.org/)
- [创建你的第一个模组](https://lamina.levimc.org/zh/developer_guides/tutorials/create_your_first_mod/)
- [xmake 官网](https://xmake.io/)
- [LeviLamina 模板仓库](https://github.com/LiteLDev/levilamina-mod-template)
