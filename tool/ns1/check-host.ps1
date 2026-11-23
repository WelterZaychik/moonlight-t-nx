param([string]$Zig)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (-not $Zig) { $Zig = Join-Path $projectRoot 'tool/zig/zig.exe' }
if (-not (Test-Path $Zig)) { throw 'Supply -Zig with a portable Zig C++ compiler path.' }
$Zig = [IO.Path]::GetFullPath($Zig)
$validation = Join-Path $projectRoot 'build/validation'
New-Item -ItemType Directory -Force $validation | Out-Null
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $projectRoot 'tool/cache/zig-global'
$env:ZIG_LOCAL_CACHE_DIR = Join-Path $projectRoot 'tool/cache/zig-local'
Push-Location $projectRoot
try {
    & $Zig c++ -std=c++20 -Iapp/tailscale tool/ns1/tests/config_test.cpp -o build/validation/config_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Configuration test compilation failed.' }
    & ./build/validation/config_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Configuration tests failed.' }
    $includeDirectories = @(
        'tool/ns1/tests/host-switch-shim', 'app/tailscale', 'app/include',
        'app/src/utils', 'app/src/crypto', 'app/src/libgamestream', 'app/src/streaming',
        'app/src/streaming/audio', 'app/src/streaming/ffmpeg', 'app/src/streaming/video',
        'extern/moonlight-common-c/src', 'lib/switch/include',
        'extern/tailscale-nx/crates/tsnx-ffi/include', 'extern/tailscale-nx/native/driver',
        'extern/tailscale-nx/native/runtime', 'extern/tailscale-nx/native/vsock',
        'extern/tailscale-nx/third_party/qrcodegen', 'extern/borealis/library/include',
        'extern/borealis/library/include/borealis/extern',
        'extern/borealis/library/include/borealis/extern/nanovg',
        'extern/borealis/library/lib/extern/fmt/include',
        'extern/borealis/library/lib/extern/tweeny/include',
        'extern/borealis/library/lib/extern/yoga')
    $flags = @('-target', 'aarch64-linux-musl', '-std=c++20', '-Wall', '-Wextra', '-DBRLS_RESOURCES="romfs:/"', '-DMOONLIGHT_TAILSCALE')
    $flags += $includeDirectories | ForEach-Object { "-I$_" }
    $sources = @('app/tailscale/sockets.cpp', 'app/tailscale/tailscale.cpp',
        'app/tailscale/tailscale_tab.cpp', 'app/src/add_host_tab.cpp', 'app/src/main_tabs_view.cpp', 'app/src/settings_tab.cpp',
        'extern/tailscale-nx/native/runtime/runtime.cpp', 'extern/tailscale-nx/native/vsock/vsock.cpp')
    foreach ($source in $sources) {
        $object = 'build/validation/' + ($source -replace '[/\\]', '_') + '.o'
        & $Zig c++ @flags -c $source -o $object
        if ($LASTEXITCODE -ne 0) { throw "Host compilation failed: $source" }
        Write-Output "Host compile passed: $source"
    }
    Write-Output 'These are host C++ checks with a Switch API shim, not a Horizon build or runtime test.'
} finally { Pop-Location }
