#pragma once

#include <QMap>
#include <QObject>
#include <QStringList>

class PinyinInputEngine final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool chineseMode READ chineseMode WRITE setChineseMode NOTIFY stateChanged)
    Q_PROPERTY(QString composition READ composition NOTIFY stateChanged)
    Q_PROPERTY(QStringList candidates READ candidates NOTIFY stateChanged)
    Q_PROPERTY(bool ready READ ready CONSTANT)
    Q_PROPERTY(QString statusText READ statusText CONSTANT)

public:
    explicit PinyinInputEngine(QObject *parent = nullptr);

    bool chineseMode() const { return m_chineseMode; }
    QString composition() const { return m_composition; }
    QStringList candidates() const { return m_candidates; }
    bool ready() const { return m_ready; }
    QString statusText() const;

    Q_INVOKABLE void setChineseMode(bool enabled);
    Q_INVOKABLE void appendLetter(const QString &letter);
    Q_INVOKABLE void backspace();
    Q_INVOKABLE void clear();
    Q_INVOKABLE QString takeCandidate(int index);
    Q_INVOKABLE QString takeFirstCandidate();

signals:
    void stateChanged();

private:
    void loadLexicon();
    void addCoreEntries();
    void updateCandidates();

    QMap<QString, QStringList> m_entries;
    QString m_composition;
    QStringList m_candidates;
    bool m_chineseMode = true;
    bool m_ready = false;
};
