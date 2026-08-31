#include "PressureProtocolV1.h"

#include <QList>

#include <algorithm>
#include <limits>

namespace {
constexpr int kMaximumFrameBytes = 128;
constexpr quint32 kMaximumAdcCode = 0x00FFFFFFu;

PressureProtocolV1::Event invalidEvent(const QString &message, bool crcError = false)
{
    PressureProtocolV1::Event event;
    event.text = message;
    event.crcError = crcError;
    return event;
}

bool parseUnsigned32(const QByteArray &field, quint32 *value, int base = 10)
{
    if (!value || field.isEmpty())
        return false;
    bool ok = false;
    const quint64 parsed = field.toULongLong(&ok, base);
    if (!ok || parsed > std::numeric_limits<quint32>::max())
        return false;
    *value = static_cast<quint32>(parsed);
    return true;
}

bool isSafeToken(const QByteArray &field)
{
    return !field.isEmpty()
        && std::all_of(field.cbegin(), field.cend(), [](char character) {
               return (character >= 'A' && character <= 'Z')
                   || (character >= 'a' && character <= 'z')
                   || (character >= '0' && character <= '9')
                   || character == '.' || character == '_' || character == '-';
           });
}
}

PressureProtocolV1::Event PressureProtocolV1::parseLine(const QByteArray &input) const
{
    const QByteArray line = input.trimmed();
    if (line.size() > kMaximumFrameBytes)
        return invalidEvent(QStringLiteral("V1 帧超过 128 字节"));
    if (!line.startsWith("@PS1,"))
        return invalidEvent(QStringLiteral("不是 PressureOS V1 帧"));

    const int star = line.lastIndexOf('*');
    if (star <= 1 || line.size() - star - 1 != 4)
        return invalidEvent(QStringLiteral("V1 帧缺少四位 CRC"));

    bool crcOk = false;
    const quint16 receivedCrc = line.mid(star + 1).toUShort(&crcOk, 16);
    if (!crcOk)
        return invalidEvent(QStringLiteral("V1 CRC 字段格式错误"), true);

    const QByteArray payload = line.mid(1, star - 1);
    const quint16 calculatedCrc = crc16CcittFalse(payload);
    if (receivedCrc != calculatedCrc) {
        return invalidEvent(
            QStringLiteral("V1 CRC 不匹配：收到 %1，计算 %2")
                .arg(receivedCrc, 4, 16, QLatin1Char('0'))
                .arg(calculatedCrc, 4, 16, QLatin1Char('0'))
                .toUpper(),
            true);
    }

    const QList<QByteArray> fields = payload.split(',');
    if (fields.size() < 2 || fields.at(0) != QByteArrayLiteral("PS1"))
        return invalidEvent(QStringLiteral("V1 协议标识错误"));

    Event event;
    const QByteArray type = fields.at(1);
    if (type == QByteArrayLiteral("M")) {
        if (fields.size() != 7
            || !parseUnsigned32(fields.at(2), &event.sequence)
            || !parseUnsigned32(fields.at(3), &event.uptimeMs)
            || !parseUnsigned32(fields.at(4), &event.pressureRaw)
            || !parseUnsigned32(fields.at(5), &event.temperatureRaw)
            || fields.at(6).size() != 8
            || !parseUnsigned32(fields.at(6), &event.statusFlags, 16)
            || event.pressureRaw > kMaximumAdcCode
            || event.temperatureRaw > kMaximumAdcCode)
            return invalidEvent(QStringLiteral("V1 测量帧字段无效"));
        event.type = EventType::Measurement;
        return event;
    }

    if (type == QByteArrayLiteral("I")) {
        if (fields.size() != 9
            || !parseUnsigned32(fields.at(2), &event.sequence)
            || !parseUnsigned32(fields.at(3), &event.uptimeMs)
            || !isSafeToken(fields.at(4)) || !isSafeToken(fields.at(5))
            || !isSafeToken(fields.at(6)) || !isSafeToken(fields.at(7))
            || fields.at(8).size() != 8
            || !parseUnsigned32(fields.at(8), &event.statusFlags, 16))
            return invalidEvent(QStringLiteral("V1 INFO 帧字段无效"));
        event.type = EventType::Info;
        event.firmwareVersion = QString::fromLatin1(fields.at(4));
        event.deviceId = QString::fromLatin1(fields.at(5));
        event.adcModel = QString::fromLatin1(fields.at(6));
        event.resetReason = QString::fromLatin1(fields.at(7));
        return event;
    }

    if (type == QByteArrayLiteral("A")) {
        if (fields.size() != 7
            || !parseUnsigned32(fields.at(2), &event.commandId)
            || !isSafeToken(fields.at(3)) || fields.at(4) != QByteArrayLiteral("OK")
            || !isSafeToken(fields.at(5)) || fields.at(6).size() != 8
            || !parseUnsigned32(fields.at(6), &event.statusFlags, 16))
            return invalidEvent(QStringLiteral("V1 ACK 帧字段无效"));
        event.type = EventType::Ack;
        event.command = QString::fromLatin1(fields.at(3));
        event.result = QString::fromLatin1(fields.at(5));
        return event;
    }

    if (type == QByteArrayLiteral("N")) {
        if (fields.size() != 6
            || !parseUnsigned32(fields.at(2), &event.commandId)
            || !isSafeToken(fields.at(3)) || !isSafeToken(fields.at(4))
            || fields.at(5).size() != 8
            || !parseUnsigned32(fields.at(5), &event.statusFlags, 16))
            return invalidEvent(QStringLiteral("V1 NACK 帧字段无效"));
        event.type = EventType::Nack;
        event.command = QString::fromLatin1(fields.at(3));
        event.result = QString::fromLatin1(fields.at(4));
        return event;
    }

    return invalidEvent(QStringLiteral("未知 V1 消息类型：%1").arg(QString::fromLatin1(type)));
}

quint16 PressureProtocolV1::crc16CcittFalse(const QByteArray &payload)
{
    quint16 crc = 0xFFFFu;
    for (char character : payload) {
        crc ^= static_cast<quint16>(static_cast<uchar>(character)) << 8;
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc & 0x8000u) ? static_cast<quint16>((crc << 1) ^ 0x1021u)
                                  : static_cast<quint16>(crc << 1);
    }
    return crc;
}

QByteArray PressureProtocolV1::encodeCommand(quint32 commandId, const QByteArray &command)
{
    if (!isSafeToken(command))
        return {};
    const QByteArray payload = QByteArrayLiteral("PS1,C,") + QByteArray::number(commandId)
        + ',' + command.toUpper();
    return '@' + payload + '*' + QByteArray::number(crc16CcittFalse(payload), 16)
        .rightJustified(4, '0').toUpper() + "\r\n";
}
