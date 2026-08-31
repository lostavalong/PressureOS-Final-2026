#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QtGlobal>

class LegacyAsciiProtocol
{
public:
    enum class EventType {
        Pressure,
        Temperature,
        Diagnostic,
        Invalid
    };

    struct Event {
        EventType type = EventType::Invalid;
        quint32 rawCode = 0;
        QString text;
    };

    explicit LegacyAsciiProtocol(quint32 channelThreshold = 8000000u);

    QList<Event> feed(const QByteArray &bytes);
    void reset();

    quint32 channelThreshold() const { return m_channelThreshold; }
    void setChannelThreshold(quint32 threshold);

private:
    Event parseLine(const QByteArray &line) const;

    QByteArray m_buffer;
    quint32 m_channelThreshold = 8000000u;
};
