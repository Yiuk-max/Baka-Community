#pragma once

// 下载断点记录：<exe>/users/<UID>/transfers/<taskId>.json。
// 只在“暂停 / 断线中断”时落盘，完成或取消时删除；重启后由 FileDownloader::restorePending 读回。
#include "model/DownloadTask.h"

#include <QList>
#include <QString>

namespace downloadjournal {

void save(const DownloadTask &task);
void remove(int uid, const QString &taskId);
// 读这个账号所有可用于续传的记录；临时文件已经不在的会顺手把记录删掉
QList<DownloadTask> load(int uid);

} // namespace downloadjournal
