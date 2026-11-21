#include "PacketCodec.h"

#include "message.pb.h"

#include <QtEndian>

namespace packetcodec {

QByteArray encode(const chat_proto::Envelope &envelope) {
    const std::string bytes = envelope.SerializeAsString();

    QByteArray payload(4, Qt::Uninitialized);//4字节不初始化
    //写成大端，static_cast<quint32>对应转换->无符号4字节payload_len，reinterpret_cast用无符号char*指针类型解释
    qToBigEndian<quint32>(static_cast<quint32>(bytes.size()),
                          reinterpret_cast<uchar *>(payload.data()));
    payload.append(bytes.data(), static_cast<int>(bytes.size()));
    return payload;
}

bool decode(const QByteArray &payload, chat_proto::Envelope &envelope, QByteArray *fileData) {
    if (payload.size() < 4) return false;

    const quint32 length =
        qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(payload.constData()));
    // protobuf 后面允许再跟文件数据，所以只要求 protobuf 这一段没超出帧
    if (length > static_cast<quint32>(payload.size() - 4)) return false;
    if (fileData) *fileData = payload.mid(4 + static_cast<int>(length));

    return envelope.ParseFromArray(payload.constData() + 4, static_cast<int>(length));
}

}
