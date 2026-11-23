param([string]$DevkitPro = $env:DEVKITPRO, [int]$Jobs = 4)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (-not $DevkitPro) { $DevkitPro = Join-Path $projectRoot 'tool/devkitpro' }
if (-not (Test-Path (Join-Path $DevkitPro 'cmake/Switch.cmake'))) {
    throw "Missing devkitPro/libnx in $DevkitPro. See docs/NS1-Tailscale.zh-CN.md."
}
$env:DEVKITPRO = [IO.Path]::GetFullPath($DevkitPro).Replace('\', '/')
$env:CARGO_HOME = Join-Path $projectRoot 'tool/cache/cargo'
$env:RUSTUP_HOME = Join-Path $projectRoot 'tool/cache/rustup'
$portablePaths = @('tool/cmake/cmake/data/bin', 'tool/ninja/ninja-1.11.1.4.data/scripts', 'tool/pkgconf/ucrt64/bin') | ForEach-Object { Join-Path $projectRoot $_ }
$env:PATH = "$(Join-Path $env:CARGO_HOME 'bin');$($portablePaths -join ';');$env:PATH"
$env:PKG_CONFIG_LIBDIR = "$env:DEVKITPRO/portlibs/switch/lib/pkgconfig"
$rustToolchain = 'nightly-2026-09-25-x86_64-pc-windows-gnu'
$hostLinker = Join-Path $env:RUSTUP_HOME "toolchains/$rustToolchain/lib/rustlib/x86_64-pc-windows-gnu/bin/self-contained/x86_64-w64-mingw32-gcc.exe"
$env:CARGO_TARGET_X86_64_PC_WINDOWS_GNU_LINKER = $hostLinker
foreach ($command in @('cmake', 'cargo', 'ninja', 'node')) {
    if (-not (Get-Command $command -ErrorAction SilentlyContinue)) { throw "Missing command: $command" }
}
$env:TEMP = Join-Path $projectRoot 'build/tmp'
$env:TMP = $env:TEMP
New-Item -ItemType Directory -Force $env:TEMP,$env:CARGO_HOME,$env:RUSTUP_HOME | Out-Null
Push-Location $projectRoot
try {
    & cargo "+$rustToolchain" --version
    if ($LASTEXITCODE -ne 0) { throw 'Install the pinned Rust toolchain into the workspace cache; see the build guide.' }
    $buildDirectory = Join-Path $projectRoot 'build/ns1'
    $pkgConfig = (Get-Command pkg-config -ErrorAction Stop).Source.Replace('\', '/')
    $cargo = (Get-Command cargo -ErrorAction Stop).Source.Replace('\', '/')
    & cmake -S $projectRoot -B $buildDirectory -G Ninja -DCMAKE_BUILD_TYPE=Release -DPLATFORM_SWITCH=ON -DPLATFORM_SWITCH2=OFF -DUSE_DEKO3D=ON -DENABLE_EMBEDDED_TAILSCALE=ON "-DPKG_CONFIG_EXECUTABLE=$pkgConfig" "-DMOONLIGHT_CARGO=$cargo" "-DMOONLIGHT_RUST_TOOLCHAIN=$rustToolchain"
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    & cmake --build $buildDirectory --target Moonlight.nro --parallel $Jobs
    if ($LASTEXITCODE -ne 0) { throw 'NS1 build failed.' }
    $staging = Join-Path $buildDirectory 'sdcard/switch/Moonlight-T-NX'
    New-Item -ItemType Directory -Force $staging,(Join-Path $projectRoot 'dist') | Out-Null
    Copy-Item -LiteralPath (Join-Path $buildDirectory 'Moonlight.nro') -Destination (Join-Path $staging 'Moonlight-T-NX.nro')
    Copy-Item -LiteralPath (Join-Path $buildDirectory 'Moonlight.nro') -Destination (Join-Path $projectRoot 'dist/Moonlight-T-NX.nro')
    Copy-Item -LiteralPath (Join-Path $projectRoot 'tool/ns1/tailscale.example.ini') -Destination $staging
    Copy-Item -LiteralPath (Join-Path $projectRoot 'docs/NS1-Tailscale.zh-CN.md') -Destination $staging
    Compress-Archive -Path (Join-Path $buildDirectory 'sdcard/switch') -DestinationPath (Join-Path $projectRoot 'dist/Moonlight-T-NX-NS1.zip') -Force
    & node tool/ns1/verify-nro.mjs
    if ($LASTEXITCODE -ne 0) { throw 'NRO verification failed.' }
    Write-Output 'Created dist/Moonlight-T-NX-NS1.zip'
} finally { Pop-Location }
