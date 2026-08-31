#include "AppController.h"

#include <QDateTime>

AppController::AppController(bool demoMode, QObject *parent)
    : QObject(parent),
      m_demoMode(demoMode)
{
    m_clockTimer.setInterval(1000);
    connect(&m_clockTimer, &QTimer::timeout, this, &AppController::updateClock);
    m_clockTimer.start();

    m_toastTimer.setSingleShot(true);
    connect(&m_toastTimer, &QTimer::timeout, this, [this] {
        m_toastVisible = false;
        emit toastChanged();
    });
    updateClock();
}

void AppController::navigate(const QString &page)
{
    setCurrentPage(page);
}

void AppController::goBack()
{
    if (m_currentPage == QStringLiteral("home"))
        return;

    const QString destination = m_previousPage == m_currentPage
        ? QStringLiteral("home") : m_previousPage;
    m_previousPage = QStringLiteral("home");
    m_currentPage = destination;
    emit currentPageChanged();
}

void AppController::toggleAssistant()
{
    setAssistantOpen(!m_assistantOpen);
}

void AppController::showToast(const QString &message, int durationMs)
{
    m_toastText = message;
    m_toastVisible = true;
    emit toastChanged();
    m_toastTimer.start(qMax(900, durationMs));
}

void AppController::openKeyboard(QObject *target, const QString &mode)
{
    if (!target)
        return;
    m_inputTarget = target;
    m_keyboardMode = mode == QStringLiteral("numeric") ? QStringLiteral("numeric")
        : mode == QStringLiteral("symbols") ? QStringLiteral("symbols") : QStringLiteral("text");
    m_keyboardVisible = true;
    emit keyboardChanged();
}

void AppController::hideKeyboard()
{
    if (!m_keyboardVisible && !m_inputTarget)
        return;
    m_keyboardVisible = false;
    m_inputTarget.clear();
    emit keyboardChanged();
}

void AppController::setKeyboardMode(const QString &mode)
{
    const QString normalized = mode == QStringLiteral("numeric") ? QStringLiteral("numeric")
        : mode == QStringLiteral("symbols") ? QStringLiteral("symbols") : QStringLiteral("text");
    if (normalized == m_keyboardMode)
        return;
    m_keyboardMode = normalized;
    emit keyboardChanged();
}

void AppController::inputText(const QString &value)
{
    if (!m_inputTarget || value.isEmpty())
        return;
    QString content = m_inputTarget->property("text").toString();
    int cursor = m_inputTarget->property("cursorPosition").toInt();
    cursor = qBound(0, cursor, content.size());
    content.insert(cursor, value);
    m_inputTarget->setProperty("text", content);
    m_inputTarget->setProperty("cursorPosition", cursor + value.size());
}

void AppController::backspaceInput()
{
    if (!m_inputTarget)
        return;
    QString content = m_inputTarget->property("text").toString();
    int cursor = qBound(0, m_inputTarget->property("cursorPosition").toInt(), content.size());
    if (cursor <= 0)
        return;
    content.remove(cursor - 1, 1);
    m_inputTarget->setProperty("text", content);
    m_inputTarget->setProperty("cursorPosition", cursor - 1);
}

void AppController::clearInput()
{
    if (!m_inputTarget)
        return;
    m_inputTarget->setProperty("text", QString{});
    m_inputTarget->setProperty("cursorPosition", 0);
}

void AppController::commitInput()
{
    if (m_inputTarget)
        m_inputTarget->setProperty("focus", false);
    hideKeyboard();
}

void AppController::setCurrentPage(const QString &page)
{
    if (page.isEmpty() || page == m_currentPage)
        return;

    m_previousPage = m_currentPage;
    m_currentPage = page;
    m_assistantOpen = false;
    hideKeyboard();
    emit currentPageChanged();
    emit assistantOpenChanged();
}

void AppController::setAssistantOpen(bool open)
{
    if (m_assistantOpen == open)
        return;
    m_assistantOpen = open;
    emit assistantOpenChanged();
}

void AppController::setExpertMode(bool enabled)
{
    if (m_expertMode == enabled)
        return;
    m_expertMode = enabled;
    emit expertModeChanged();
}

void AppController::updateClock()
{
    const QDateTime now = QDateTime::currentDateTime();
    const QString clock = now.toString(QStringLiteral("HH:mm"));
    const QString date = now.toString(QStringLiteral("M月d日 dddd"));
    if (clock == m_clockText && date == m_dateText)
        return;
    m_clockText = clock;
    m_dateText = date;
    emit clockChanged();
}
