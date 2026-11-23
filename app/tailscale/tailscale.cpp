#include "tailscale.hpp"
#include "config_validation.hpp"
#include "session.hpp"
#include "tsnx_ssl.h"
#include <switch.h>
#include <algorithm>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <mutex>
#include <sstream>
#include <sys/stat.h>
#include <thread>
#include <arpa/inet.h>

extern "C" void tsnx_platform_random(uint8_t* buffer, size_t size) { randomGet(buffer, size); }
extern "C" [[noreturn]] void tsnx_platform_panic(const uint8_t* message, size_t size) {
    std::fprintf(stderr, "Tailscale panic: %.*s\n", static_cast<int>(size), message);
    std::abort();
}

namespace moonlight::tailscale {
namespace {
// Lifecycle/configuration and event snapshots never share the engine's lock.
std::mutex mutex;
std::condition_variable changed;
Snapshot status;
std::shared_ptr<Session> active;
std::thread worker;
std::string directory;
bool pending = false, quitting = false;
uint64_t generation = 0;

void updateLocalNetwork() {
    u32 ip = 0, mask = 0, gateway = 0, dns1 = 0, dns2 = 0;
    if (R_SUCCEEDED(nifmGetCurrentIpConfigInfo(&ip,&mask,&gateway,&dns1,&dns2)))
        tsnx::VSock::SetLocalNet(ntohl(ip),static_cast<unsigned>(__builtin_popcount(mask)));
    else tsnx::VSock::SetLocalNet(0,32);
}

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \r\n\t");
    if (first == std::string::npos) return {};
    return value.substr(first, value.find_last_not_of(" \r\n\t") - first + 1);
}
void load() {
    FILE* file = std::fopen((directory + "/tailscale.ini").c_str(), "r");
    if (!file) return;
    char line[1024];
    while (std::fgets(line, sizeof line, file)) {
        std::string text = trim(line);
        if (text.empty() || text[0] == '#' || text[0] == ';') continue;
        auto equals = text.find('=');
        if (equals == std::string::npos) continue;
        auto key = trim(text.substr(0, equals)), value = trim(text.substr(equals + 1));
        auto& config = status.config;
        if (key == "enabled") config.enabled = value == "1" || value == "true";
        if (key == "accept_routes") config.acceptRoutes = value == "1" || value == "true";
        if (key == "control_url") config.controlUrl = value;
        if (key == "hostname") config.hostname = value;
        if (key == "auth_key") config.authKey = value;
    }
    std::fclose(file);
}
bool save(const Config& config) {
    const auto target = directory + "/tailscale.ini", temporary = target + ".tmp";
    FILE* file = std::fopen(temporary.c_str(), "w");
    if (!file) return false;
    int written = std::fprintf(file, "enabled=%d\naccept_routes=%d\ncontrol_url=%s\nhostname=%s\nauth_key=%s\n",
        config.enabled, config.acceptRoutes, config.controlUrl.c_str(), config.hostname.c_str(), config.authKey.c_str());
    bool ok = written >= 0;
    if (std::fclose(file) != 0) ok = false;
    if (!ok || std::rename(temporary.c_str(), target.c_str()) != 0) {
        std::remove(temporary.c_str());
        return false;
    }
    return true;
}
void event(uint64_t id, const TsnxEvent& ev) {
    std::lock_guard<std::mutex> lock(mutex);
    if (id != generation || quitting) return;
    const std::string text = ev.text ? ev.text : "";
    switch (ev.kind) {
        case TSNX_EVENT_LOGIN_URL: status.loginUrl = text; status.state = State::Login; break;
        case TSNX_EVENT_ADDRESSES:
            // The current FFI separates IPv4/IPv6 addresses with spaces.
            status.address = text.substr(0, text.find_first_of(" ,"));
            status.loginUrl.clear(); status.error.clear(); status.state = State::Connected; break;
        case TSNX_EVENT_PEERS: status.peerCount = ev.value; break;
        case TSNX_EVENT_PEER_PATH: status.path = (ev.value ? "UDP 直连：" : "DERP 中继：") + text; break;
        case TSNX_EVENT_ERROR:
            std::fprintf(stderr, "Tailscale: %s\n", text.c_str());
            status.error = "组网连接出现错误，请检查网络、授权状态和控制服务器设置。";
            if (status.address.empty()) status.state = State::Error;
            break;
        default: break;
    }
}
void loop() {
    for (;;) {
        Config config;
        uint64_t id;
        bool stop;
        {
            std::unique_lock<std::mutex> lock(mutex);
            changed.wait_for(lock, std::chrono::seconds(1), [] { return pending || quitting; });
            if (!pending && !quitting) { lock.unlock(); updateLocalNetwork(); continue; }
            pending = false; stop = quitting; config = status.config; id = generation;
            status.address.clear(); status.loginUrl.clear(); status.path.clear(); status.peerCount = 0; status.error.clear();
            status.state = config.enabled && !stop ? State::Starting : State::Off;
        }
        auto old = std::atomic_exchange(&active, std::shared_ptr<Session>{});
        if (old) {
            old->alive = false;
            old->runtime->Suspend(tsnx::Runtime::kStopping, std::chrono::milliseconds(1500));
            old->runtime->StopAndJoin();
        }
        old.reset();
        tsnx::VSock::SetRoutes(nullptr,0);
        updateLocalNetwork();
        // Retired sockets retain their old Session until close(). Their engine
        // thread has stopped; the SSL context must outlive their final owner.
        if (stop) break;
        if (!config.enabled) continue;
        auto error = validate(config);
        if (error.empty()) {
            tsnx::RuntimeConfig runtimeConfig;
            runtimeConfig.control_url = config.controlUrl;
            runtimeConfig.auth_key = config.authKey;
            runtimeConfig.hostname = config.hostname;
            runtimeConfig.state_path = directory + "/tailscale.state";
            runtimeConfig.accept_routes = config.acceptRoutes;
            runtimeConfig.on_event = [id](const TsnxEvent& ev) { event(id, ev); };
            auto session = std::make_shared<Session>();
            session->runtime = tsnx::Runtime::Start(std::move(runtimeConfig), &error);
            if (session->runtime) {
                session->runtime->With([](TsnxEngine* engine) { tsnx_net_set_budget(engine, 8 * 1024 * 1024); });
                session->sockets = std::make_unique<tsnx::VSock>(backend(), *session->runtime);
                session->sockets->SetEagerDualUdp(false);
                session->sockets->SetAliveCheck([weak = std::weak_ptr<Session>(session)] {
                    auto value = weak.lock(); return value && value->alive.load();
                });
                std::lock_guard<std::mutex> lock(mutex);
                if (id == generation && !quitting) std::atomic_store(&active, session);
            }
        }
        if (!error.empty()) {
            std::lock_guard<std::mutex> lock(mutex);
            if (id == generation) {
                std::fprintf(stderr, "Tailscale: %s\n", error.c_str());
                status.error = error.rfind("cannot read/write state", 0) == 0
                    ? "无法读写登录状态文件，请检查 SD 卡是否可写。"
                    : "无法启动组网连接，请检查网络与配置后重试。";
                status.state = State::Error;
            }
        }
    }
}
}

std::shared_ptr<Session> activeSession() {
    if (tsnx::Runtime::InInternal()) return {};
    return std::atomic_load(&active);
}
void init(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex);
    if (worker.joinable()) return;
    const auto applet = appletGetAppletType();
    if (applet != AppletType_Application && applet != AppletType_SystemApplication) {
        status.state = State::Error;
        status.error = "请按住 R 启动游戏进入 hbmenu，以完整内存模式打开本应用。";
        return;
    }
    directory = path;
    mkdir(directory.c_str(), 0777);
    load();
    // Keep certificate chain, hostname and date verification enabled.
    const auto rc = tsnx_ssl_init(1);
    if (rc) { status.state = State::Error; status.error = "无法初始化 Switch 加密连接，错误码：" + std::to_string(rc); return; }
    quitting = false; pending = true; ++generation;
    worker = std::thread(loop);
}
void shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!worker.joinable()) return;
        quitting = true; ++generation;
    }
    changed.notify_one();
    worker.join();
    // SSL is process scoped: retired socket owners can still be unwinding.
    // libnx releases the service at process exit after these owners disappear.
}
Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(mutex);
    auto result = status;
    // The UI only needs to know whether a key exists.
    if (!result.config.authKey.empty()) result.config.authKey = "[configured]";
    return result;
}
std::string configure(const Config& config) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!worker.joinable() || quitting) return "Tailscale 暂不可用，请重新启动 Moonlight。";
    Config next = config;
    if (next.authKey == "[configured]") next.authKey = status.config.authKey;
    auto error = validate(next);
    if (!error.empty()) return error;
    if (!save(next)) return "无法将 Tailscale 配置保存到 SD 卡。";
    status.config = next; pending = true; ++generation;
    changed.notify_one();
    return {};
}
std::vector<Peer> peers() {
    auto session = activeSession();
    if (!session || !session->alive) return {};
    std::string hosts;
    session->runtime->Read([&](TsnxEngine* engine) {
        std::vector<char> data(4096);
        auto size = tsnx_engine_hosts(engine, data.data(), data.size());
        if (size >= data.size() && size < 1024 * 1024) {
            data.resize(size + 1); tsnx_engine_hosts(engine, data.data(), data.size());
        }
        hosts = data.data();
    });
    auto self = snapshot().address;
    std::vector<Peer> result;
    std::istringstream input(hosts);
    std::string address, name;
    while (input >> address >> name) if (address != self) result.push_back({address, name});
    return result;
}
}
