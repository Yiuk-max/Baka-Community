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

bool decode(const QByteArray &payload, chat_proto::Envelope &envelope) {
    if (payload.size() < 4) return false;

    const quint32 length =
        qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(payload.constData()));
    if (static_cast<int>(length) != payload.size() - 4) return false;

    return envelope.ParseFromArray(payload.constData() + 4, static_cast<int>(length));
}

} // namespace packetcodec
