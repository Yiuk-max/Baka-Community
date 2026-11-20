#include "email_verify_repo.h"
#include "mysql_conn_pool.h"

#include <iostream>
#include "cppconn/prepared_statement.h"
#include "cppconn/resultset.h"
#include "cppconn/exception.h"

namespace {
// 连接 RAII（和其他 repo 同一套做法）
class ConnGuard {
public:
    ConnGuard() : conn_(MySQL_Conn_Pool::get_instance().get_connection()) {}
    ~ConnGuard() {
        if (conn_) MySQL_Conn_Pool::get_instance().return_connection(conn_);
    }
    sql::Connection* get() { return conn_; }
    explicit operator bool() const { return conn_ != nullptr; }
private:
    sql::Connection* conn_;
};
} // namespace

// 写入一条验证码记录，返回新行 id（>0），DB 失败返回 0
int email_verify_repo::save_code(int uid, const std::string& email,
                                  const std::string& code, const std::string& purpose) {
    ConnGuard guard;
    if (!guard) {
        std::cerr << "[email_verify_repo] save_code: no DB connection." << std::endl;
        return 0;
    }
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            guard.get()->prepareStatement(
                "INSERT INTO email_verification (email, uid, code, purpose) VALUES (?, ?, ?, ?)"));
        pstmt->setString(1, email);
        pstmt->setInt(2, uid);
        pstmt->setString(3, code);
        pstmt->setString(4, purpose);
        pstmt->executeUpdate();

        // 取自增 ID
        std::unique_ptr<sql::Statement> stmt(guard.get()->createStatement());
        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery("SELECT LAST_INSERT_ID() AS id"));
        if (rs->next()) return static_cast<int>(rs->getUInt64("id"));
        return 0;
    } catch (const sql::SQLException& e) {
        std::cerr << "[email_verify_repo] save_code failed: " << e.what()
                  << " (ERRNO=" << e.getErrorCode() << ")" << std::endl;
        return 0;
    }
}

// 冷却检查：查最近一条发送记录的 sent_at，返回剩余冷却秒数（已过冷却期或无记录返回 0）
int email_verify_repo::seconds_until_cooldown_ends(int uid, const std::string& purpose,
                                                    int cooldown_seconds) {
    ConnGuard guard;
    if (!guard) {
        std::cerr << "[email_verify_repo] seconds_until_cooldown_ends: no DB connection." << std::endl;
        return 0;
    }
    try {
        // TIMESTAMPDIFF 计算"现在距上次发送已过多少秒"，直接在 DB 算，不依赖时钟同步
        std::unique_ptr<sql::PreparedStatement> pstmt(
            guard.get()->prepareStatement(
                "SELECT TIMESTAMPDIFF(SECOND, sent_at, UTC_TIMESTAMP()) AS elapsed "
                "FROM email_verification "
                "WHERE uid = ? AND purpose = ? "
                "ORDER BY sent_at DESC LIMIT 1"));
        pstmt->setInt(1, uid);
        pstmt->setString(2, purpose);
        std::unique_ptr<sql::ResultSet> rs(pstmt->executeQuery());
        if (!rs->next()) return 0;              // 没有记录，不在冷却期
        const int elapsed = rs->getInt("elapsed");
        const int remaining = cooldown_seconds - elapsed;
        return remaining > 0 ? remaining : 0;
    } catch (const sql::SQLException& e) {
        std::cerr << "[email_verify_repo] seconds_until_cooldown_ends failed: " << e.what()
                  << " (ERRNO=" << e.getErrorCode() << ")" << std::endl;
        return 0;
    }
}

// 核销验证码：找最新一条未核销且 code 匹配的行，标记 verified_at=NOW() 返回 true；
// 不匹配或已全部核销返回 false
bool email_verify_repo::consume_code(int uid, const std::string& code,
                                      const std::string& purpose) {
    ConnGuard guard;
    if (!guard) {
        std::cerr << "[email_verify_repo] consume_code: no DB connection." << std::endl;
        return false;
    }
    try {
        // 先查：uid + purpose 下最新一条未核销的记录，检查 code 是否匹配
        std::unique_ptr<sql::PreparedStatement> pstmt(
            guard.get()->prepareStatement(
                "SELECT id, code FROM email_verification "
                "WHERE uid = ? AND purpose = ? AND verified_at IS NULL "
                "ORDER BY sent_at DESC LIMIT 1"));
        pstmt->setInt(1, uid);
        pstmt->setString(2, purpose);
        std::unique_ptr<sql::ResultSet> rs(pstmt->executeQuery());
        if (!rs->next()) return false;           // 无未核销记录

        const int row_id = rs->getInt("id");
        const std::string stored = rs->getString("code");
        if (stored != code) return false;        // 验证码不匹配

        // 核销
        std::unique_ptr<sql::PreparedStatement> upd(
            guard.get()->prepareStatement(
                "UPDATE email_verification SET verified_at = UTC_TIMESTAMP() WHERE id = ?"));
        upd->setInt(1, row_id);
        upd->executeUpdate();
        return true;
    } catch (const sql::SQLException& e) {
        std::cerr << "[email_verify_repo] consume_code failed: " << e.what()
                  << " (ERRNO=" << e.getErrorCode() << ")" << std::endl;
        return false;
    }
}

// 登录门禁：该 uid 在 purpose 下是否存在"未核销"的验证码记录。
// 历史账号（本功能上线前注册，从没发过码）没有任何记录 → 返回 false，正常放行。
bool email_verify_repo::has_pending(int uid, const std::string& purpose) {
    ConnGuard guard;
    if (!guard) {
        std::cerr << "[email_verify_repo] has_pending: no DB connection." << std::endl;
        return false;   // 查不到就不拦，避免 DB 抖动导致所有人都登不上
    }
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            guard.get()->prepareStatement(
                "SELECT 1 FROM email_verification "
                "WHERE uid = ? AND purpose = ? AND verified_at IS NULL LIMIT 1"));
        pstmt->setInt(1, uid);
        pstmt->setString(2, purpose);
        std::unique_ptr<sql::ResultSet> rs(pstmt->executeQuery());
        return rs->next();
    } catch (const sql::SQLException& e) {
        std::cerr << "[email_verify_repo] has_pending failed: " << e.what()
                  << " (ERRNO=" << e.getErrorCode() << ")" << std::endl;
        return false;
    }
}
