#pragma once

// 前后端约定的 WS 业务帧编解码：
//   binary 帧 payload = 4 字节 payload_len（大端） + protobuf Envelope
// 完整协议见 ../-----server/Docs/客户端接口文档.txt 2.2 节。
// WebSocket 自带消息边界，所以这里不需要处理粘包/半包
#include <QByteArray>   //字节数组类

namespace chat_proto {
class Envelope;
}

namespace packetcodec {

// Envelope -> 4 字节长度前缀 + protobuf 字节
QByteArray encode(const chat_proto::Envelope &envelope);
// 同上，后面再跟一段原始二进制（file_chunk 上传分片用；长度前缀只算 protobuf）
QByteArray encode(const chat_proto::Envelope &envelope, const QByteArray &fileData);

// 4 字节长度前缀 + protobuf 字节 -> Envelope；长度不符或解析失败返回 false
// 文件分片（file_chunk）的帧在 protobuf 后面还跟着文件数据，传了 fileData 就把这部分放进去
bool decode(const QByteArray &payload, chat_proto::Envelope &envelope, QByteArray *fileData = nullptr);

} // namespace packetcodec
