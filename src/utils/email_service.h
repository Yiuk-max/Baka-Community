#pragma once
#include <string>

// ============================================================
// EmailService：通过 Resend API 发送邮件（libcurl HTTP POST）。
//
// 只暴露一个对外接口 send_verification()，业务层无需关心 HTTP 细节。
// API Key 在 ServerConfig 中读取（configure.json → email.api_key）。
// ============================================================
class EmailService {
public:
    static EmailService& get_instance();

    // 向 to_email 发送注册邮箱验证码。
    // 成功返回 true；网络/API 失败打印错误日志并返回 false。
    bool send_verification(const std::string& to_email,
                           const std::string& username,
                           const std::string& code);

private:
    EmailService() = default;
};
