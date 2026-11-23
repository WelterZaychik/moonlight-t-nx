#pragma once
#include <string>
#include <vector>

namespace moonlight::tailscale {
enum class State { Off, Starting, Login, Connected, Error };
struct Config {
    bool enabled = false;
    bool acceptRoutes = false;
    std::string controlUrl = "https://controlplane.tailscale.com";
    std::string hostname = "moonlight-switch1";
    std::string authKey;
};
struct Peer { std::string address, name; };
struct Snapshot {
    State state = State::Off;
    Config config;
    std::string address, loginUrl, path, error;
    unsigned peerCount = 0;
};
void init(const std::string& directory);
void shutdown();
Snapshot snapshot();
// Queues a restart on the VPN worker. Returns a validation/persistence error.
std::string configure(const Config& config);
std::vector<Peer> peers();
}
