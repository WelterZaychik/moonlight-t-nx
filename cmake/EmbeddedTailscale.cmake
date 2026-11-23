set(TSNX_SOURCE "${CMAKE_SOURCE_DIR}/extern/tailscale-nx")
find_program(MOONLIGHT_CARGO cargo)
if (NOT MOONLIGHT_CARGO)
    message(FATAL_ERROR "Embedded Tailscale requires Rust nightly-2026-09-25 with rust-src. See docs/NS1-Tailscale.zh-CN.md")
endif ()
set(MOONLIGHT_RUST_TOOLCHAIN "nightly-2026-09-25" CACHE STRING "Rust toolchain used for the embedded VPN")
set(TSNX_TARGET "aarch64-nintendo-switch-freestanding")
set(TSNX_TARGET_DIR "${CMAKE_BINARY_DIR}/rust-target")
set(TSNX_ARCHIVE "${TSNX_TARGET_DIR}/${TSNX_TARGET}/release/libtsnx_ffi.a")
file(GLOB_RECURSE TSNX_RUST_INPUTS CONFIGURE_DEPENDS
    "${TSNX_SOURCE}/crates/*.rs" "${TSNX_SOURCE}/crates/*/Cargo.toml"
    "${TSNX_SOURCE}/third_party/tailscale-rs/*.rs" "${TSNX_SOURCE}/third_party/tailscale-rs/*/Cargo.toml")
if (NOT DEFINED TSNX_BUILD_UNIX)
    string(TIMESTAMP TSNX_BUILD_UNIX "%s" UTC)
    set(TSNX_BUILD_UNIX "${TSNX_BUILD_UNIX}" CACHE STRING "Build time used for Tailscale clock sanity checks")
endif ()
add_custom_command(OUTPUT "${TSNX_ARCHIVE}"
    COMMAND ${CMAKE_COMMAND} -E env "CARGO_TARGET_DIR=${TSNX_TARGET_DIR}"
        ${MOONLIGHT_CARGO} +${MOONLIGHT_RUST_TOOLCHAIN} build
        --manifest-path "${TSNX_SOURCE}/Cargo.toml" --locked -p tsnx-ffi
        --release --target ${TSNX_TARGET} -Zbuild-std=core,alloc
    WORKING_DIRECTORY "${TSNX_SOURCE}"
    DEPENDS ${TSNX_RUST_INPUTS} "${TSNX_SOURCE}/Cargo.toml" "${TSNX_SOURCE}/Cargo.lock" "${TSNX_SOURCE}/.cargo/config.toml"
    COMMENT "Building the embedded NS1 Tailscale engine" VERBATIM)
add_custom_target(moonlight_tailscale_rust DEPENDS "${TSNX_ARCHIVE}")
add_library(moonlight_tailscale_ffi STATIC IMPORTED)
set_target_properties(moonlight_tailscale_ffi PROPERTIES IMPORTED_LOCATION "${TSNX_ARCHIVE}")
add_dependencies(${PROJECT_NAME} moonlight_tailscale_rust)
target_sources(${PROJECT_NAME} PRIVATE
    app/tailscale/tailscale.cpp app/tailscale/sockets.cpp app/tailscale/tailscale_tab.cpp
    "${TSNX_SOURCE}/native/runtime/runtime.cpp" "${TSNX_SOURCE}/native/vsock/vsock.cpp"
    "${TSNX_SOURCE}/native/driver/tsnx_app.c" "${TSNX_SOURCE}/native/driver/tsnx_driver.c"
    "${TSNX_SOURCE}/native/driver/tsnx_dns.c" "${TSNX_SOURCE}/native/driver/tsnx_drop.c"
    "${TSNX_SOURCE}/native/driver/tsnx_ssl.c" "${TSNX_SOURCE}/third_party/qrcodegen/qrcodegen.c")
target_include_directories(${PROJECT_NAME} PRIVATE app/tailscale
    "${TSNX_SOURCE}/crates/tsnx-ffi/include" "${TSNX_SOURCE}/native/driver"
    "${TSNX_SOURCE}/native/runtime" "${TSNX_SOURCE}/native/vsock" "${TSNX_SOURCE}/third_party/qrcodegen")
target_compile_definitions(${PROJECT_NAME} PRIVATE MOONLIGHT_TAILSCALE TSNX_BUILD_UNIX=${TSNX_BUILD_UNIX})
target_link_libraries(${PROJECT_NAME} PRIVATE moonlight_tailscale_ffi)
foreach(symbol socket close connect bind listen accept send sendto recv recvfrom sendmsg recvmsg
        poll shutdown getsockname getpeername getsockopt setsockopt fcntl ioctl select read write getaddrinfo)
    target_link_options(${PROJECT_NAME} PRIVATE "-Wl,--wrap=${symbol}")
endforeach ()
