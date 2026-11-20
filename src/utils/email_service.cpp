#include "email_service.h"
#include "server_config.h"

#include <curl/curl.h>
#include <iostream>
#include <sstream>

// ============================================================
// Resend API：POST https://api.resend.com/emails
// 请求体：JSON { from, to, subject, html }
// 鉴权：Authorization: Bearer <api_key>
// 文档：https://resend.com/docs/api-reference/emails/send-email
// ============================================================

namespace {

// libcurl 写回调：把响应体拼到 string
std::size_t write_cb(char* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
    auto* buf = static_cast<std::string*>(userdata);
    buf->append(ptr, size * nmemb);
    return size * nmemb;
}

// 把字符串中的 " 转义为 \"，用于拼 JSON（无需引入额外 JSON 库）
std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '"')       out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else                out += c;
    }
    return out;
}

// 生成 HTML 邮件正文：简洁、好看
std::string build_html(const std::string& username, const std::string& code) {
    std::ostringstream html;
    html << R"(<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>邮箱验证</title>
</head>
<body style="margin:0;padding:0;background:#f4f6f9;font-family:'Helvetica Neue',Helvetica,Arial,sans-serif;">
  <table width="100%" cellpadding="0" cellspacing="0"
         style="background:#f4f6f9;padding:40px 0;">
    <tr>
      <td align="center">
        <table width="480" cellpadding="0" cellspacing="0"
               style="background:#ffffff;border-radius:12px;
                      box-shadow:0 2px 12px rgba(0,0,0,.08);
                      overflow:hidden;">
          <!-- 顶部色带 -->
          <tr>
            <td style="background:#4f8ef7;padding:28px 40px;">
              <p style="margin:0;color:#fff;font-size:22px;font-weight:700;
                        letter-spacing:.5px;">Baka Community</p>
            </td>
          </tr>
          <!-- 正文 -->
          <tr>
            <td style="padding:36px 40px 20px;">
              <p style="margin:0 0 8px;color:#1a1a2e;font-size:16px;font-weight:600;">
                你好，)" << json_escape(username) << R"(！</p>
              <p style="margin:0 0 28px;color:#555;font-size:14px;line-height:1.6;">
                你正在注册 Baka Community 账号。请使用下方验证码完成邮箱验证。
              </p>
              <!-- 验证码框 -->
              <div style="background:#f0f5ff;border:1px solid #d0e0ff;
                          border-radius:10px;padding:20px;text-align:center;
                          margin-bottom:28px;">
                <p style="margin:0 0 6px;color:#888;font-size:12px;
                           letter-spacing:1px;text-transform:uppercase;">验证码</p>
                <p style="margin:0;color:#2563eb;font-size:38px;font-weight:800;
                           letter-spacing:10px;font-variant-numeric:tabular-nums;">)"
       << code << R"(</p>
              </div>
              <p style="margin:0 0 8px;color:#888;font-size:13px;line-height:1.6;">
                验证码 <strong>10 分钟内</strong>有效。如果这不是你本人的操作，请忽略此邮件。
              </p>
            </td>
          </tr>
          <!-- 底部 -->
          <tr>
            <td style="background:#f8f9fb;padding:16px 40px;border-top:1px solid #eee;">
              <p style="margin:0;color:#aaa;font-size:12px;">
                © Baka Community &nbsp;·&nbsp; 此邮件由系统自动发送，请勿回复
              </p>
            </td>
          </tr>
        </table>
      </td>
    </tr>
  </table>
</body>
</html>)";
    return html.str();
}

} // namespace

EmailService& EmailService::get_instance() {
    static EmailService instance;
    return instance;
}

bool EmailService::send_verification(const std::string& to_email,
                                      const std::string& username,
                                      const std::string& code) {
    const std::string api_key = ServerConfig::get_instance().resend_api_key();
    if (api_key.empty()) {
        std::cerr << "[EmailService] resend_api_key not configured." << std::endl;
        return false;
    }

    const std::string html   = build_html(username, code);
    const std::string subject = "Baka Community 邮箱验证码：" + code;

    // 拼 JSON 请求体
    std::string body = R"({"from":"Baka Community <noreply@bakacomm.world>","to":[")"
        + json_escape(to_email) + R"("],"subject":")"
        + json_escape(subject) + R"(","html":")"
        + json_escape(html) + R"("})";

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[EmailService] curl_easy_init failed." << std::endl;
        return false;
    }

    std::string resp_body;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    const std::string auth_header = "Authorization: Bearer " + api_key;
    headers = curl_slist_append(headers, auth_header.c_str());

    curl_easy_setopt(curl, CURLOPT_URL,            "https://api.resend.com/emails");
    curl_easy_setopt(curl, CURLOPT_POST,           1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,     body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,  static_cast<long>(body.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &resp_body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        10L);  // 10s 超时

    const CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "[EmailService] curl error: " << curl_easy_strerror(res) << std::endl;
        return false;
    }
    if (http_code < 200 || http_code >= 300) {
        std::cerr << "[EmailService] Resend API returned HTTP " << http_code
                  << ": " << resp_body << std::endl;
        return false;
    }
    return true;
}
