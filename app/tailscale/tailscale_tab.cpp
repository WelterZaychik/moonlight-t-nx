#include <switch.h>
#include "tailscale_tab.hpp"
#include "tailscale.hpp"
#include "qrcodegen.h"
#include "Settings.hpp"
#include "MoonlightSession.hpp"
#include "GameStreamClient.hpp"
#include "main_tabs_view.hpp"
#include "helper.hpp"
#include "add_host_tab.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace ts = moonlight::tailscale;
namespace {
class LoginQr : public brls::Box {
    std::array<uint8_t,qrcodegen_BUFFER_LEN_MAX> code{};
    std::string last;
    int size = 0;
public:
    LoginQr() { setDimensions(260,260); setMargins(16,0,16,0); setVisibility(brls::Visibility::GONE); }
    void update(const std::string& url) {
        if (url == last) return;
        last = url; size = 0;
        std::array<uint8_t,qrcodegen_BUFFER_LEN_MAX> temporary{};
        if (!url.empty() && qrcodegen_encodeText(url.c_str(),temporary.data(),code.data(),qrcodegen_Ecc_MEDIUM,
            qrcodegen_VERSION_MIN,11,qrcodegen_Mask_AUTO,true)) size = qrcodegen_getSize(code.data());
        setVisibility(size ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    }
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style, brls::FrameContext*) override {
        if (!size) return;
        const float scale = std::max(1.0f,std::floor(std::min(width,height) / (size + 8)));
        nvgBeginPath(vg); nvgRect(vg,x,y,(size + 8) * scale,(size + 8) * scale); nvgFillColor(vg,nvgRGB(255,255,255)); nvgFill(vg);
        nvgBeginPath(vg);
        for (int row = 0; row < size; ++row) for (int col = 0; col < size; ++col)
            if (qrcodegen_getModule(code.data(),col,row)) nvgRect(vg,x + (col + 4) * scale,y + (row + 4) * scale,scale,scale);
        nvgFillColor(vg,nvgRGB(0,0,0)); nvgFill(vg);
    }
};
bool prompt(const char* title, const std::string& initial, std::string& value, bool password = false) {
    SwkbdConfig keyboard;
    if (R_FAILED(swkbdCreate(&keyboard,0))) return false;
    swkbdConfigMakePresetDefault(&keyboard);
    swkbdConfigSetHeaderText(&keyboard,title);
    swkbdConfigSetInitialText(&keyboard,initial.c_str());
    swkbdConfigSetStringLenMax(&keyboard,255);
    if (password) swkbdConfigSetPasswordFlag(&keyboard,1);
    std::array<char,1024> text{};
    const auto result = swkbdShow(&keyboard,text.data(),text.size());
    swkbdClose(&keyboard);
    if (R_FAILED(result)) return false;
    value = text.data(); return true;
}
void apply(const ts::Config& config) {
    auto error = ts::configure(config);
    if (!error.empty()) showError(error);
}
}
TailscaleTab::TailscaleTab() {
    inflateFromXMLRes("xml/tabs/tailscale.xml");
    status->setIsWrapping(true);
    loginUrl->setIsWrapping(true);
    qrContainer->addView(new LoginQr());
    enabled->registerClickAction([this](brls::View*) {
        if (canConfigure()) { auto config = ts::snapshot().config; config.enabled = !config.enabled; apply(config); }
        return true;
    });
    login->registerClickAction([this](brls::View*) {
        if (canConfigure()) { auto config = ts::snapshot().config; config.enabled = true; config.authKey.clear(); apply(config); }
        return true;
    });
    authKey->registerClickAction([this](brls::View*) {
        if (canConfigure()) {
            auto config = ts::snapshot().config;
            if (prompt("Tailscale 授权密钥（留空使用扫码登录）","",config.authKey,true)) { config.enabled = true; apply(config); }
        }
        return true;
    });
    control->registerClickAction([this](brls::View*) {
        if (canConfigure()) { auto config = ts::snapshot().config; if (prompt("HTTPS 控制服务器 / Headscale",config.controlUrl,config.controlUrl)) apply(config); }
        return true;
    });
    hostname->registerClickAction([this](brls::View*) {
        if (canConfigure()) { auto config = ts::snapshot().config; if (prompt("Tailscale 设备名称",config.hostname,config.hostname)) apply(config); }
        return true;
    });
    routes->registerClickAction([this](brls::View*) {
        if (canConfigure()) { auto config = ts::snapshot().config; config.acceptRoutes = !config.acceptRoutes; apply(config); }
        return true;
    });
    refreshPeers->registerClickAction([this](brls::View*) {
        peerSignature.clear(); devices->clearViews(); refresh(); return true;
    });
    refresh();
}
bool TailscaleTab::canConfigure() {
    if (MoonlightSession::activeSession()) { showAlert("请先停止串流，再修改 Tailscale 设置。"); return false; }
    return true;
}
void TailscaleTab::refresh() {
    auto value = ts::snapshot();
    std::string text;
    switch (value.state) {
        case ts::State::Off: text = "已关闭"; break;
        case ts::State::Starting: text = "正在连接……"; break;
        case ts::State::Login: text = "请扫描二维码登录"; break;
        case ts::State::Connected: text = "已连接：" + value.address; break;
        case ts::State::Error: text = "连接失败"; break;
    }
    text += "\n组网设备数：" + std::to_string(value.peerCount);
    if (!value.path.empty()) text += "\n" + value.path;
    if (!value.error.empty()) text += "\n" + value.error;
    status->setText(text);
    enabled->setDetailText(value.config.enabled ? "已开启" : "已关闭");
    authKey->setDetailText(value.config.authKey.empty() ? "未设置" : "已设置");
    control->setDetailText(value.config.controlUrl);
    hostname->setDetailText(value.config.hostname);
    routes->setDetailText(value.config.acceptRoutes ? "已开启" : "已关闭");
    loginUrl->setText(value.loginUrl);
    static_cast<LoginQr*>(qrContainer->getChildren()[0])->update(value.loginUrl);
    // Avoid rebuilding focusable cells in the user's hands. Refresh this list
    // when entering the tab again (a new tab instance), or when first available.
    if (!peerSignature.empty()) return;
    auto peers = ts::peers();
    if (peers.empty()) return;
    peerSignature = "loaded";
    for (const auto& peer : peers) {
        if (peer.address.find(':') != std::string::npos) continue;
        auto cell = new brls::DetailCell();
        cell->setText(peer.name); cell->setDetailText(peer.address);
        // Reuse Moonlight's discovery probe and PIN pairing workflow.
        cell->registerClickAction([peer](brls::View*) {
            if (ts::snapshot().state != ts::State::Connected) { showAlert("请先连接 Tailscale，再选择电脑配对。"); return true; }
            Host host;
            host.hostname = peer.name; host.remoteAddress = peer.address;
            AddHostTab::connectHost(host);
            return true;
        });
        devices->addView(cell);
    }
}
void TailscaleTab::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* context) {
    auto now = std::chrono::steady_clock::now();
    if (now - updated >= std::chrono::seconds(1)) { updated = now; refresh(); }
    brls::Box::draw(vg,x,y,width,height,style,context);
}
