#include "LegacyAsciiProtocol.h"

#include <algorithm>

namespace {
constexpr int kMaximumBufferedBytes = 8192;
constexpr int kMaximumLineBytes = 128;
constexpr quint32 kMaximumAdcCode = 0x00FFFFFFu;
}

LegacyAsciiProtocol::LegacyAsciiProtocol(quint32 channelThreshold)
{
    setChannelThreshold(channelThreshold);
}

QList<LegacyAsciiProtocol::Event> LegacyAsciiProtocol::feed(const QByteArray &bytes)
{
    QList<Event> events;
    if (bytes.isEmpty())
        return events;

    m_buffer.append(bytes);
    if (m_buffer.size() > kMaximumBufferedBytes) {
        const int lastLineBreak = m_buffer.lastIndexOf('\n');
        m_buffer = lastLineBreak >= 0 ? m_buffer.mid(lastLineBreak + 1) : QByteArray{};
        events.push_back({EventType::Invalid, 0u,
                          QStringLiteral("接收缓存超过上限，已丢弃未完成数据")});
    }

    int lineBreak = -1;
    while ((lineBreak = m_buffer.indexOf('\n')) >= 0) {
        QByteArray line = m_buffer.left(lineBreak);
        m_buffer.remove(0, lineBreak + 1);
        line = line.trimmed();
        if (line.isEmpty())
            continue;
        events.push_back(parseLine(line));
    }
    return events;
}

void LegacyAsciiProtocol::reset()
{
    m_buffer.clear();
}

void LegacyAsciiProtocol::setChannelThreshold(quint32 threshold)
{
    m_channelThreshold = std::clamp(threshold, 1u, kMaximumAdcCode);
}

LegacyAsciiProtocol::Event LegacyAsciiProtocol::parseLine(const QByteArray &line) const
{
    if (line.size() > kMaximumLineBytes) {
        return {EventType::Invalid, 0u,
                QStringLiteral("串口行长度超过 %1 字节").arg(kMaximumLineBytes)};
    }

    const bool decimalOnly = std::all_of(line.cbegin(), line.cend(), [](char character) {
        return character >= '0' && character <= '9';
    });
    if (decimalOnly) {
        bool ok = false;
        const quint64 value = line.toULongLong(&ok, 10);
        if (!ok || value > kMaximumAdcCode) {
            return {EventType::Invalid, 0u,
                    QStringLiteral("ADC 原始码超出 24 位范围：%1")
                        .arg(QString::fromLatin1(line))};
        }
        const quint32 rawCode = static_cast<quint32>(value);
        return {rawCode < m_channelThreshold ? EventType::Pressure : EventType::Temperature,
                rawCode, {}};
    }

    const bool printable = std::all_of(line.cbegin(), line.cend(), [](char character) {
        const uchar byte = static_cast<uchar>(character);
        return byte == '\t' || (byte >= 0x20 && byte <= 0x7e);
    });
    if (printable) {
        return {EventType::Diagnostic, 0u,
                QString::fromLatin1(line).trimmed()};
    }

    return {EventType::Invalid, 0u,
            QStringLiteral("收到无法识别的非 ASCII 数据：%1")
                .arg(QString::fromLatin1(line.toHex(' ')))};
}
