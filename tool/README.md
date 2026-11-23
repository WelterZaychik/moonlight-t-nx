# 本项目的打包工具

应用源码保留在项目根目录的 `app/`、`extern/`、`resources/`、`lib/` 内。

- `ns1/`：NS1 构建入口、依赖版本锁文件、示例配置与本地检查。
- `devkitpro/`：devkitA64 编译器、libnx、Switch 工具与目标依赖库。
- `cmake/`、`ninja/`、`pkgconf/`、`zig/`：Windows 便携工具。
- `cache/`：Rustup、Cargo、Zig 下载与缓存。
- `downloads/`：工具安装包、下载来源与 SHA-256 记录。
- `support/`：下载与初始化辅助脚本。

编译中间文件保存在根目录的 `build/`，最终 NRO 和安装包保存在 `dist/`。
不需要安装到系统目录或修改系统 PATH。

从项目根目录运行：`./tool/ns1/build.ps1`。
首次准备工具：`node tool/ns1/setup-windows.mjs --rust-mirror`。
