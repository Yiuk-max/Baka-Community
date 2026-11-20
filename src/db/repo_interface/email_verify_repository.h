#pragma once
#include <ctime>
#include <string>

// ============================================================
// 邮箱验证码仓储接口层（repo_interface）
// 定位：定义验证码的写入/冷却查询/核销契约，不含 SQL。
//
// 分层约定：
//   - 本文件只声明接口，禁止出现 SQL 与 mysql 头。
//   - 真正实现位于 repo 层 class email_verify_repo（见 repo/email_verify_repo.h）。
//   - 由 RepositoryHub 组合根装配，业务层经 repo_hub_->email_verify() 调用。
//
// 表结构约定（见 sql/create_table.sql email_verification 表）：
//   - email_verification(id PK, email, uid, code, sent_at, verified_at, purpose)
// ============================================================
class I_email_verify_repo {
public:
    virtual ~I_email_verify_repo() = default;

    // 记录一次发送：写入 (uid, email, code, sent_at=NOW())。
    // 成功返回新行 id（>0），DB 失败返回 0。
    virtual int save_code(int uid, const std::string& email,
                          const std::string& code, const std::string& purpose) = 0;

    // 冷却检查：查询最近一次发送时间，判断是否还在 cooldown_seconds 冷却期内。
    // 仍在冷却期返回剩余秒数（>0），不在冷却期（或从未发过）返回 0。
    virtual int seconds_until_cooldown_ends(int uid, const std::string& purpose,
                                            int cooldown_seconds) = 0;

    // 核销验证码：uid + purpose 下最新一条未核销的 code 匹配则更新 verified_at=NOW() 返回 true，
    // 不匹配或已核销返回 false。
    virtual bool consume_code(int uid, const std::string& code,
                              const std::string& purpose) = 0;
};
