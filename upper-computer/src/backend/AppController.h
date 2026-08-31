#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>

class AppController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentPage READ currentPage WRITE setCurrentPage NOTIFY currentPageChanged)
    Q_PROPERTY(QString previousPage READ previousPage NOTIFY currentPageChanged)
    Q_PROPERTY(bool assistantOpen READ assistantOpen WRITE setAssistantOpen NOTIFY assistantOpenChanged)
    Q_PROPERTY(bool expertMode READ expertMode WRITE setExpertMode NOTIFY expertModeChanged)
    Q_PROPERTY(bool toastVisible READ toastVisible NOTIFY toastChanged)
    Q_PROPERTY(QString toastText READ toastText NOTIFY toastChanged)
    Q_PROPERTY(QString clockText READ clockText NOTIFY clockChanged)
    Q_PROPERTY(QString dateText READ dateText NOTIFY clockChanged)
    Q_PROPERTY(bool demoMode READ demoMode CONSTANT)
    Q_PROPERTY(bool keyboardVisible READ keyboardVisible NOTIFY keyboardChanged)
    Q_PROPERTY(QString keyboardMode READ keyboardMode NOTIFY keyboardChanged)
    Q_PROPERTY(QObject *inputTarget READ inputTarget NOTIFY keyboardChanged)

public:
    explicit AppController(bool demoMode = true, QObject *parent = nullptr);

    QString currentPage() const { return m_currentPage; }
    QString previousPage() const { return m_previousPage; }
    bool assistantOpen() const { return m_assistantOpen; }
    bool expertMode() const { return m_expertMode; }
    bool toastVisible() const { return m_toastVisible; }
    QString toastText() const { return m_toastText; }
    QString clockText() const { return m_clockText; }
    QString dateText() const { return m_dateText; }
    bool demoMode() const { return m_demoMode; }
    bool keyboardVisible() const { return m_keyboardVisible; }
    QString keyboardMode() const { return m_keyboardMode; }
    QObject *inputTarget() const { return m_inputTarget.data(); }

    Q_INVOKABLE void navigate(const QString &page);
    Q_INVOKABLE void goBack();
    Q_INVOKABLE void toggleAssistant();
    Q_INVOKABLE void showToast(const QString &message, int durationMs = 2600);
    Q_INVOKABLE void openKeyboard(QObject *target, const QString &mode = QStringLiteral("text"));
    Q_INVOKABLE void hideKeyboard();
    Q_INVOKABLE void setKeyboardMode(const QString &mode);
    Q_INVOKABLE void inputText(const QString &text);
    Q_INVOKABLE void backspaceInput();
    Q_INVOKABLE void clearInput();
    Q_INVOKABLE void commitInput();

public slots:
    void setCurrentPage(const QString &page);
    void setAssistantOpen(bool open);
    void setExpertMode(bool enabled);

signals:
    void currentPageChanged();
    void assistantOpenChanged();
    void expertModeChanged();
    void toastChanged();
    void clockChanged();
    void keyboardChanged();

private:
    void updateClock();

    QString m_currentPage = QStringLiteral("home");
    QString m_previousPage = QStringLiteral("home");
    bool m_assistantOpen = false;
    bool m_expertMode = false;
    bool m_toastVisible = false;
    QString m_toastText;
    QString m_clockText;
    QString m_dateText;
    bool m_demoMode = true;
    QTimer m_clockTimer;
    QTimer m_toastTimer;
    QPointer<QObject> m_inputTarget;
    QString m_keyboardMode = QStringLiteral("text");
    bool m_keyboardVisible = false;
};
