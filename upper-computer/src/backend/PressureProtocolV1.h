#pragma once

#include <QByteArray>
#include <QString>
#include <QtGlobal>

class PressureProtocolV1
{
public:
    enum class EventType {
        Measurement,
        Info,
        Ack,
        Nack,
        Invalid
    };

    struct Event {
        EventType type = EventType::Invalid;
        quint32 sequence = 0;
        quint32 uptimeMs = 0;
        quint32 pressureRaw = 0;
        quint32 temperatureRaw = 0;
        quint32 statusFlags = 0;
        quint32 commandId = 0;
        QString command;
        QString result;
        QString firmwareVersion;
        QString deviceId;
        QString adcModel;
        QString resetReason;
        QString text;
        bool crcError = false;
    };

    Event parseLine(const QByteArray &line) const;

    static quint16 crc16CcittFalse(const QByteArray &payload);
    static QByteArray encodeCommand(quint32 commandId, const QByteArray &command);
};
