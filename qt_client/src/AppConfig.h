#ifndef BAKA_APPCONFIG_H
#define BAKA_APPCONFIG_H

// 客户端配置文件
// 文件位置：可执行文件同目录 config.json；由 CMake 从 qt_client/config.json 复制过去。
// 文件缺失时按默认值创建一份，方便直接改；解析失败则保留默认值并记录错误。
#include <QString>

namespace appconfig {

struct Config {
    QString serverUrl;
};

// 默认服务器地址（生产环境，TLS 由 Cloudflare/nginx 终结）
QString defaultServerUrl();

// 配置文件的绝对路径（exe 同目录）
QString filePath();

// 读取配置；文件不存在会写出默认配置
Config load();

} // namespace appconfig

#endif // BAKA_APPCONFIG_H
