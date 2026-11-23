#include "config_validation.hpp"
#include <cstdio>
#include <cstdlib>
using namespace moonlight::tailscale;
int checks = 0;
void expect(bool value, const char* label) {
    ++checks;
    if (!value) { std::fprintf(stderr,"FAIL: %s\n",label); std::exit(1); }
}
int main() {
    Config config;
    expect(validate(config).empty(),"default QR configuration");
    config.authKey = "tskey-auth-example-secret";
    expect(validate(config).empty(),"Auth Key login");
    config.controlUrl = "https://headscale.example.com:443/control";
    expect(validate(config).empty(),"HTTPS Headscale endpoint");
    for (const auto* url : {"http://headscale.example.com", "https://", "https:///", "https://host\nhostname=injected", "https://host #fragment"}) {
        auto value = config; value.controlUrl = url;
        expect(!validate(value).empty(),"invalid control URL rejected");
    }
    for (const auto* name : {"", "-switch", "switch-", "switch_1", "switch\nauth_key=secret"}) {
        auto value = config; value.hostname = name;
        expect(!validate(value).empty(),"invalid hostname / INI injection rejected");
    }
    for (const auto* key : {"wrong-token", "tskey-auth-one\nenabled=1", "tskey-auth-one\rtwo"}) {
        auto value = config; value.authKey = key;
        expect(!validate(value).empty(),"invalid key / INI injection rejected");
    }
    config.hostname = std::string(63,'a');
    expect(validate(config).empty(),"63-byte hostname accepted");
    config.hostname += 'a';
    expect(!validate(config).empty(),"overlong hostname rejected");
    std::printf("Config validation: %d checks passed\n",checks);
}
