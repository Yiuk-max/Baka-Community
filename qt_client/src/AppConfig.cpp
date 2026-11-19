#include "AppConfig.h"

#include <QCoreApplication>
#include <QDebug> //qDebug
#include <QDir> //文件夹、目录
#include <QFile> // 文件操作
#include <QJsonDocument> //json文件处理
#include <QJsonObject> //json键值对

namespace appconfig {

namespace {
const char *kServerUrlKey = "server_url";
}

QString defaultServerUrl() { return QString("wss://bakacomm.world/ws"); }

QString filePath() {//返回可执行文件同目录下的 config.json 文件路径，绝对路径
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("config.json"));
}

Config load() {
    Config config{defaultServerUrl()};

    QFile file(filePath());
    if (!file.exists()) {
        // 首次运行：写出一份默认配置，方便用户直接打开修改
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QJsonObject root;
            root.insert(QLatin1String(kServerUrlKey), config.serverUrl);
            //Indented 让 json 文件格式化输出。Compact 紧凑
            // 创建json文档对象，写入json对象，toJson()返回json格式的字节数组
            file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
            file.close();
            qInfo("[config] 未找到配置，已写出默认文件: %s", qPrintable(filePath()));
        }
        return config;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("[config] 无法读取 %s: %s", qPrintable(file.fileName()), qPrintable(file.errorString()));
        return config;
    }

    QJsonParseError error{};
    // 读取 json 文件所有内容，返回 QJsonDocument JSON对象
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        qWarning("[config] %s 解析失败（%s），使用默认服务器地址", qPrintable(file.fileName()),
                 qPrintable(error.errorString()));
        return config;
    }
    // 读取 json 对象中 server_url 键的值，去掉首尾空格
    const QString url = document.object().value(QLatin1String(kServerUrlKey)).toString().trimmed();
    if (url.isEmpty()) {
        qWarning("[config] %s 缺少 %s，使用默认服务器地址", qPrintable(file.fileName()), kServerUrlKey);
        return config;
    }

    config.serverUrl = url;
    return config;
}

} // namespace appconfig
