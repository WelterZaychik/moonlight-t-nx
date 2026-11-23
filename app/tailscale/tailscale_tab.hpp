#pragma once
#include <borealis.hpp>
#include <chrono>

class TailscaleTab : public brls::Box {
public:
    TailscaleTab();
    static brls::View* create() { return new TailscaleTab(); }
    void draw(NVGcontext*, float, float, float, float, brls::Style, brls::FrameContext*) override;
private:
    BRLS_BIND(brls::Label, status, "status");
    BRLS_BIND(brls::Label, loginUrl, "login_url");
    BRLS_BIND(brls::DetailCell, enabled, "enabled");
    BRLS_BIND(brls::DetailCell, login, "login");
    BRLS_BIND(brls::DetailCell, authKey, "auth_key");
    BRLS_BIND(brls::DetailCell, control, "control_url");
    BRLS_BIND(brls::DetailCell, hostname, "hostname");
    BRLS_BIND(brls::DetailCell, routes, "routes");
    BRLS_BIND(brls::Box, devices, "devices");
    BRLS_BIND(brls::Box, qrContainer, "qr_container");
    BRLS_BIND(brls::DetailCell, refreshPeers, "refresh_peers");
    void refresh();
    bool canConfigure();
    std::chrono::steady_clock::time_point updated{};
    std::string peerSignature;
};
