# Moonlight-T-NX：NS1 使用、构建与验收

## 当前状态

这是以 Moonlight-Switch 为基础的进程内 Tailscale 集成源码，面向第一代 Nintendo Switch
（Erista、Mariko、Lite、OLED）。Switch 2 的 kaiHEN/libounce、4K/120 Hz 和 AV1 硬解方案
没有移植到此分支。已完成真实 NS1 SDK 交叉编译、链接与 NRO 封装；当前为测试构建。

当前开发环境是 Windows。使用项目内 devkitA64 GCC 16.1.0、libnx 4.12.0、CMake 3.31.6、
Ninja 1.11.1 和指定 Rust nightly 生成 NRO。ELF 为 AArch64，链接无未解析符号；尚未进行
NS1 真机验收，不能据此保证登录、串流、休眠恢复或性能。

本构建默认使用简体中文，不依赖 Switch 系统语言。新增 Tailscale 页面和操作提示均为中文。

## 安装与使用

1. 解压 `dist/Moonlight-T-NX-NS1.zip` 到 SD 卡根目录。主程序路径为
   `sdmc:/switch/Moonlight-T-NX/Moonlight-T-NX.nro`。
2. 使用 Atmosphère 的标题接管方式打开 hbmenu：按住 R 启动游戏，再打开 Moonlight-T-NX。
   相册启动的 applet 模式不启用内嵌 VPN。
3. PC 上运行 Sunshine，并让 PC 加入同一个 Tailnet。
4. 打开侧栏 **Tailscale**，选择 **扫描二维码登录**，用手机扫描二维码完成登录；
   也可以使用 **输入授权密钥** 输入 `tskey-auth-...`。登录成功以分配到地址为准。
5. 在 Tailnet 设备列表选择 Sunshine PC，按 Moonlight 的 PIN 提示在 Sunshine 配对。
   列表包含其他种类的 Tailnet 设备；只有响应 Sunshine/GameStream 协议的设备才能配对。
6. 从侧栏进入已保存的 PC，选择应用开始串流。列表变化后使用 **刷新组网设备列表**。

登录状态、机器/节点密钥保存在 `tailscale.state`；配置保存在 `tailscale.ini`，均位于
`sdmc:/switch/Moonlight-T-NX/`。示例文件名为 `tailscale.example.ini`，更新安装包不会覆盖
已有 `tailscale.ini`。不要公开包含 Auth Key 或机器密钥的这两个文件。

“二维码登录”使用当前节点身份重新连接控制服务器。已授权节点可能直接恢复连接，不一定
生成新二维码。更换账号时，先在 Tailscale 管理端移除旧节点、退出应用，再删除 SD 卡上的
`tailscale.state`，然后重新登录。应用不读取独立 tailscale-nx 系统模块的密钥。

## NS1 设置

分辨率、FPS、视频码率均可在侧栏 **设置 → 画质** 调整，LAN 与 Tailscale 使用同一套配置，
无需改代码或重新编译。离开设置页时保存到 `sdmc:/switch/Moonlight-T-NX/settings.json`，
下次开始串流时生效；调整已建立的串流后，需要停止并重新连接。

- 分辨率：原生（可设置缩放）、360p、480p、540p、720p、1080p、1440p 等现有选项。
- FPS：保留上游的 30、40、60、120 选项；可选数值不表示 NS1 已验证支持该档位。
- 视频码率：保留上游 NS1 的 0.5–100 Mbps 滑块，默认 10 Mbps。

先用原项目默认的 720p / 60 FPS / 10 Mbps 验收，PC 使用有线网络，再根据实测逐步调整。
1080p 能否稳定运行还受解码器、网络、主机负载和散热影响；本分支不自动超频。
DERP 的延迟与吞吐需要实测，不能保证任意网络都能稳定串流。

子网路由默认关闭。确有 Tailnet subnet router 时再启用；与 Switch 当前 LAN 重叠的 IPv4
子网保留 LAN 直连。Tailscale 自身 IPv4 地址为 `100.64.0.0/10`，并非所有 `100.x` 地址。
应用内名称解析只使用当前引擎的设备列表，不安装系统级 DNS。

切换 Tailscale、登录方式或控制服务器会重建隧道，旧虚拟连接返回断网状态；请在停止串流后
操作。关闭应用会停止 VPN。内嵌模式仅服务本应用，不为其他 NRO 提供联网能力。
同时启用会拦截本应用网络的 tailscale-nx MITM 系统模块的组合尚未验证。

TLS 保留证书链、主机名和日期校验。遇到证书日期错误时先检查日期与网络，代码不会通过
关闭证书校验来绕过问题。自签名私有 CA 的 Headscale 暂无界面导入入口。

## 目录与依赖

| 路径 | 用途 |
|---|---|
| `app/tailscale/` | 生命周期、套接字包装、Tailscale 界面 |
| `extern/tailscale-nx/` | 固定版本的 Rust/C/C++ 引擎源码 |
| `downloads/` | 上游项目与依赖源码包、归档哈希 |
| `tool/ns1/dependencies.json` | NS1 C/C++ 依赖的版本与 SHA-256 |
| `tool/devkitpro/` | 可选的本项目工具链目录 |
| `tool/downloads/` | 工具归档、下载来源和 SHA-256 |
| `tool/cmake/`、`tool/ninja/`、`tool/pkgconf/`、`tool/zig/` | Windows 便携工具 |
| `tool/cache/cargo/`、`tool/cache/rustup/` | Rust 下载及缓存 |
| `build/ns1/` | NRO、ELF、Rust 静态库与 SD 卡暂存目录 |
| `dist/` | 成功构建后的安装包 |


ode tool/ns1/fetch-dependencies.mjs` 校验并解压依赖，只写本项目目录。
归档版本和 SHA-256 固定，不跟随分支变化。已有 Git 子模块 checkout 也可使用，先确认版本
与锁文件一致。`extern/vcpkg` 和 Borealis 的 SDL3 子模块不是 NS1/deko3d 构建所需依赖。

## 构建

需要 devkitPro 的 devkitA64、libnx、Switch CMake 工具、Switch portlibs，以及原项目使用的
mbedTLS、jansson、expat、opus、png、zstd、dav1d、deko3d 等开发包。FFmpeg 和修改过的
mbedTLS libcurl 静态库使用仓库自带的 `lib/switch/`。需要 CMake 3.25+；Windows 入口使用
Ninja。devkitPro 依赖组合应先确认能构建原版 Moonlight-Switch。

本次 Windows 构建的工具归档固定在 `tool/ns1/windows-tools.lock.json`，包含来源与 SHA-256。
devkitPro 官方包服务器在当前网络返回 403，因此使用 Leseratte 的 devkitPro 归档镜像；
Rustup 安装程序与官方 SHA-256 比对，Rust 组件由 Rustup 校验。便携 CMake/Ninja 来自各自
PyPI 分发包。SDK 内的 CMake 入口做了本项目 Windows/Ninja 路径适配，不改系统安装。
首次准备可运行 
ode tool/ns1/setup-windows.mjs --rust-mirror`；所有下载、SDK 和缓存均在
`tool/` 内。`--sdk-only` 仅准备 C/C++ 工具，不安装 Rust。

Rust 固定为 
ightly-2026-09-25`，组件需要 `rust-src`，目标为
`aarch64-nintendo-switch-freestanding`；通过 `-Zbuild-std=core,alloc` 生成静态库。
无需另外安装该目标的预编译标准库。

在已准备 Rustup 的终端内，将下载缓存设置到本项目，再安装工具链：

```powershell
$env:RUSTUP_HOME = "$PWD/tool/cache/rustup"
$env:CARGO_HOME = "$PWD/tool/cache/cargo"
rustup toolchain install nightly-2026-09-25-x86_64-pc-windows-gnu --profile minimal --component rust-src
node tool/ns1/fetch-dependencies.mjs
.\tool\ns1\build.ps1 -DevkitPro "$PWD/tool/devkitpro"
```

如 Rustup 尚未安装，把安装程序下载到 `tool/downloads/`，安装时使用上述两个本地缓存路径并
禁用修改系统 PATH。已有项目外 devkitPro 可作为只读工具链传给 `-DevkitPro`；新增下载与
编译文件仍应留在本项目内。

Linux / 已有 WSL 环境：

```bash
export DEVKITPRO="$PWD/tool/devkitpro"
export RUSTUP_HOME="$PWD/tool/cache/rustup" CARGO_HOME="$PWD/tool/cache/cargo"
export PATH="$CARGO_HOME/bin:$PATH"
rustup toolchain install nightly-2026-09-25 --profile minimal --component rust-src
bash tool/ns1/build.sh
```

成功后应同时存在 `build/ns1/Moonlight.nro`、`build/ns1/Moonlight.elf` 和
`dist/Moonlight-T-NX-NS1.zip`。命令失败时脚本退出，旧文件不能当作本次产物。
原版对照组在独立构建目录使用 `-DENABLE_EMBEDDED_TAILSCALE=OFF`。

## 必须完成的验收

- NS1 真机 NRO 加载；相册模式提示标题接管。
- VPN 关闭时：LAN 配对、应用列表、控制器、视频和音频正常。
- 全新节点：二维码、Auth Key、重启身份恢复；取消登录和授权过期提示。
- Tailnet：IP 和名称、PIN 配对、HTTPS 应用列表、TCP RTSP、UDP 音视频。
- Direct UDP 与强制 DERP 各持续串流至少 30 分钟，记录延迟、丢包、CPU 和内存。
- 停止串流后反复启停、更改控制服务器、退出应用，无死锁或描述符泄漏。
- Wi-Fi 断开恢复、飞行模式、休眠唤醒，检查视频和控制连接是否均恢复。
- 已保存 LAN 主机与新增 Tailnet 主机并存；子网路由与 LAN 重叠时仍走本地。
- SD 卡写入失败、配置非法、时钟不正确、服务器不可达时提示可理解的错误。

本地检查结果记录在 `build/validation/`，未经上述验收不标记为稳定版。

工具目录与构建方式见上文；本次完整交叉构建已成功。仍需完成以上 NS1 真机验收。
