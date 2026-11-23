#pragma once
#include "tailscale.hpp"

namespace moonlight::tailscale {
inline std::string validate(const Config& config) {
    if (config.controlUrl.rfind("https://", 0) != 0 || config.controlUrl.size() <= 8 ||
        config.controlUrl.size() > 255 || config.controlUrl[8] == '/' ||
        config.controlUrl.find_first_of("\r\n\t #") != std::string::npos)
        return "控制服务器必须使用 HTTPS 地址，长度不超过 255 个字符。";
    if (config.hostname.empty() || config.hostname.size() > 63 ||
        config.hostname.front() == '-' || config.hostname.back() == '-' ||
        config.hostname.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-") != std::string::npos)
        return "设备名称须为 1～63 个英文字母、数字或连字符，首尾不能为连字符。";
    if (config.authKey.size() > 255 || config.authKey.find_first_of("\r\n\t #") != std::string::npos ||
        (!config.authKey.empty() && config.authKey.rfind("tskey-auth-", 0) != 0))
        return "请输入以 tskey-auth- 开头的授权密钥；留空可使用扫码登录。";
    return {};
}
}
