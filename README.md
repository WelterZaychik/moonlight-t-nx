# Moonlight-T-NX · NS1 简体中文版

面向第一代 Nintendo Switch 的 Moonlight 串流客户端，在同一个 NRO 中嵌入 Tailscale，方便通过 Tailnet 连接远程 Sunshine 电脑。

本项目以 [Moonlight-Switch](https://github.com/XITRIX/Moonlight-Switch) 为基础，使用 [tailscale-nx](https://github.com/UnderwaterOverground/tailscale-nx) 的组网引擎，集成思路参考 [super-moonlight-nx2](https://github.com/davidkalil10/super-moonlight-nx2)。目标设备为 NS1 的 Erista、Mariko、Lite 和 OLED。

**当前状态：已完成真实 NS1 SDK 交叉编译、链接与 NRO 封装，默认简体中文。尚未进行 NS1 真机登录和串流验收，当前属于测试构建。**

## 功能

- 应用内启停 Tailscale，支持二维码登录、授权密钥、HTTPS 控制服务器 / Headscale、设备名称和子网路由设置。
- WireGuard、smoltcp 与 DERP 运行在应用进程内，无需单独运行 tailscale-nx 系统模块。
- 从组网设备列表选择 Sunshine 电脑，沿用 Moonlight 的 PIN 配对和应用列表流程。
- 包含 TCP / UDP 套接字适配，供配对、应用列表与音视频串流使用；保留局域网直连和组网设备名称解析。
- 默认简体中文，新增 Tailscale 页面、连接状态、操作提示和配置错误均已汉化。
- 分辨率、FPS、视频码率可在设置页调整并保存，无需修改源码或重新编译；停止串流后重新连接生效。

默认串流参数为 **720p / 60 FPS / 10 Mbps**。设置页保留上游的分辨率、30 / 40 / 60 / 120 FPS 和 0.5–100 Mbps 码率选项；可选档位不代表 NS1 已验证支持或能稳定运行。组网直连、中继、休眠恢复和长时间串流仍需实机测试。

## 安装与使用

本仓库提交源码、构建脚本和文档；工具安装包、工具链、缓存、登录状态与本地生成的 NRO / ZIP 不纳入 Git。

完成本地构建后：

1. 将 `dist/Moonlight-T-NX-NS1.zip` 解压到 SD 卡根目录；也可将 `dist/Moonlight-T-NX.nro` 放入 `sdmc:/switch/Moonlight-T-NX/`。
2. 按住 **R** 启动游戏，以标题接管方式进入 hbmenu，再打开本应用。相册的 applet 模式不启用内嵌组网功能。
3. 电脑运行 Sunshine，并加入同一个 Tailnet。
4. 打开应用侧栏 **Tailscale**，选择 **扫描二维码登录**，或通过 **输入授权密钥** 登录。
5. 连接成功后，在设备列表选择 Sunshine 电脑，按照 PIN 提示配对，然后选择应用开始串流。
6. 在 **设置 → 画质** 调整分辨率、FPS 与码率，离开设置页保存。

配置和身份分别保存在应用目录的 `tailscale.ini`、`tailscale.state`；安装包提供的 `tailscale.example.ini` 不会覆盖已有配置。完整操作说明和验收清单见 [NS1 中文说明](docs/NS1-Tailscale.zh-CN.md)。

## 源码与工具目录

| 路径 | 内容 |
| --- | --- |
| `app/` | Moonlight 应用源码；`app/tailscale/` 为内嵌组网适配与界面 |
| `extern/` | 源码依赖；`extern/tailscale-nx/` 固定版本纳入仓库 |
| `resources/` | 中文翻译、界面 XML、图标和着色器 |
| `lib/switch/` | 上游提供的 NS1 目标库与头文件，构建需要保留 |
| `cmake/` | 内嵌 Rust 引擎的构建配置 |
| `tool/ns1/` | 构建、依赖准备、检查脚本和版本锁文件 |
| `tool/devkitpro/`、`tool/cache/`、`tool/downloads/` | 本地工具链、缓存和工具归档，不提交 |
| `downloads/` | 上游源码归档，不提交 |
| `build/`、`dist/` | 构建中间文件和最终产物，不提交 |

## Windows 构建

需要预先安装 Git 和 Node.js 22 或更高版本，并在项目根目录运行：

```powershell
git clone https://github.com/WelterZaychik/moonlight-t-nx.git
cd moonlight-t-nx

# 下载并校验 NS1 所需的源码依赖
node tool/ns1/fetch-dependencies.mjs

# 首次准备便携工具和 Rust，全部写入本项目的 tool 文件夹
node tool/ns1/setup-windows.mjs --rust-mirror

# 编译、封装并检查 NRO
./tool/ns1/build.ps1
```

后续构建直接运行 `tool/ns1/build.ps1`，无需再次下载全部工具。工具来源和 SHA-256 固定在 [Windows 工具锁文件](tool/ns1/windows-tools.lock.json)，源码依赖固定在 [依赖锁文件](tool/ns1/dependencies.json)。使用已有 SDK 时可传入 `-DevkitPro <目录>`。

本次本地构建使用 devkitA64 GCC 16.1.0、libnx 4.12.0、CMake 3.31.6、Ninja 1.11.1 和 Rust `nightly-2026-09-25`。Windows 使用 GNU 主机工具链；Linux 环境入口为 `bash tool/ns1/build.sh`，需先准备相应平台的 devkitPro 依赖与 Rust。

成功后生成：

- `dist/Moonlight-T-NX.nro`：独立主程序。
- `dist/Moonlight-T-NX-NS1.zip`：按 SD 卡目录组织的安装包。
- `dist/SHA256SUMS`、`dist/build-info.json`：校验值和构建检查记录。
- `build/ns1/Moonlight.elf`：用于调试的 AArch64 ELF。

已完成的本地检查包括 18 项配置校验、NS1 交叉编译、ELF 链接、NRO 文件头、嵌入中文资源和安装包结构检查。真机状态详见 [验收清单](docs/NS1-Tailscale.zh-CN.md#必须完成的验收)。

## 致谢

本项目建立在以下三个项目的工作之上，感谢各位作者与贡献者：

| 项目 | 作者 / 维护者 | 在本项目中的用途 |
| --- | --- | --- |
| [Moonlight-Switch](https://github.com/XITRIX/Moonlight-Switch) | **XITRIX** 及贡献者 | 应用基础，提供串流、配对、音频、输入、NS1 解码渲染和设置界面。 |
| [tailscale-nx](https://github.com/UnderwaterOverground/tailscale-nx) | **UnderwaterOverground** 及贡献者 | 提供内嵌 Tailscale 的 Rust 引擎、原生运行时、虚拟套接字与网络驱动。 |
| [super-moonlight-nx2](https://github.com/davidkalil10/super-moonlight-nx2) | **davidkalil10** 及贡献者 | 提供 Moonlight 与 Tailscale 进程内集成的参考思路；本项目按 NS1 平台实现适配。 |

同时感谢 Moonlight、Sunshine、Borealis、Tailscale / tailscale-rs、devkitPro、libnx 和 Project Nayuki 等依赖项目的作者与贡献者。上游完整介绍保留在 [上游 README](docs/README.upstream.md)。

源码基线：

- Moonlight-Switch：`841fc353aa5152df659aad4f6e1907d4ad3ba1d5`。
- tailscale-nx：`70fe63f1be50bf41e12fd5f26dd45a867ad95ac5`（1.4.1）。

## 许可证与来源

保留 Moonlight-Switch 的 [GPL-3.0 许可证](LICENSE) 和原作者版权声明。第三方代码的许可证继续适用于各自组件，详情见 [VPN 引擎来源与本地修改](extern/tailscale-nx/VENDORED.md)、[tailscale-nx 许可证](extern/tailscale-nx/LICENSE) 和 [第三方声明](extern/tailscale-nx/THIRD_PARTY_NOTICES.md)。

远端仓库初始化时的 Apache-2.0 文件原样归档在 [初始化许可证](docs/LICENSE.repository-init.Apache-2.0)，作为历史记录保留；其不替代上游代码和第三方组件已有的许可证。
