#pragma once
#include <QDate>
#include <QDateTime>
#include <QString>

namespace timeformat {
// 把服务端的 UTC "YYYY-MM-DD HH:MM:SS" 转成本地时区：今天 → "HH:mm"，其他 → "MM/dd HH:mm"
inline QString toLocal(const QString &utc) {
    QDateTime dt = QDateTime::fromString(utc, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!dt.isValid()) return utc;
    dt.setTimeSpec(Qt::UTC);
    const QDateTime local = dt.toLocalTime();
    if (local.date() == QDate::currentDate())
        return local.toString(QStringLiteral("HH:mm"));
    return local.toString(QStringLiteral("MM/dd HH:mm"));
}
} // namespace timeformat
