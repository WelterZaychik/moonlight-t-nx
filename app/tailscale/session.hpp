#pragma once
#include <atomic>
#include <memory>
#include "runtime.hpp"
#include "vsock.hpp"

namespace moonlight::tailscale {
struct Session {
    std::unique_ptr<tsnx::Runtime> runtime;
    std::unique_ptr<tsnx::VSock> sockets;
    std::atomic<bool> alive{true};
    ~Session() {
        alive = false;
        if (runtime) runtime->StopAndJoin();
        if (sockets) sockets->CloseAll();
        sockets.reset();
        runtime.reset();
    }
};
tsnx::Backend& backend();
std::shared_ptr<Session> activeSession();
}
