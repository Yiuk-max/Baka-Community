#pragma once
// ============================================================
// 邮箱验证码仓储 —— 真实的 MySQL 实现（repo 层）
// 定位：实现 I_email_verify_repo，访问 email_verification 表。
// ============================================================
#include "email_verify_repository.h"

class email_verify_repo : public I_email_verify_repo {
public:
    int save_code(int uid, const std::string& email,
                  const std::string& code, const std::string& purpose) override;

    int seconds_until_cooldown_ends(int uid, const std::string& purpose,
                                    int cooldown_seconds) override;

    bool consume_code(int uid, const std::string& code,
                      const std::string& purpose) override;

    bool has_pending(int uid, const std::string& purpose) override;
};
