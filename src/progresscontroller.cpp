// SPDX-FileCopyrightText: 2026 Pascal Klose
// SPDX-License-Identifier: GPL-3.0-or-later

#include "progresscontroller.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QGuiApplication>
#include <QVariantMap>

using namespace Qt::StringLiterals;

namespace
{
    /// An encode writes a line per second for hours; without a cap the window would hold the whole
    /// ffmpeg log in memory for output nobody scrolls back to.
    constexpr int MaximumDetailLines = 5000;

    /// Where the panel expects the progress of a task to be announced. It is a Canonical interface in
    /// a KDE panel because that is what applications already emitted: org.kde.plasma.taskmanager holds
    /// `com.canonical.Unity` on the session bus and listens for this signal, from any object path.
    constexpr auto UnityInterface = "com.canonical.Unity.LauncherEntry"_L1;
    constexpr auto UnityPath = "/net/kurokyuu/kurodlg"_L1;

    constexpr int ProgressWidth = 520;
    constexpr int ProgressHeight = 160;
    constexpr int TextViewWidth = 720;
    constexpr int TextViewHeight = 480;

    /// Assigns and reports whether anything changed, so a setter emits only for a real change.
    template <typename Type>
    bool assign(Type& member, const Type& value)
    {
        if (member == value)
        {
            return false;
        }

        member = value;
        return true;
    }
} // namespace

ProgressController::ProgressController(const Mode mode, const QString& text, const int totalSteps, QObject* parent)
    : QObject(parent), m_mode(mode), m_maximum(totalSteps), m_text(text)
{
    if (mode == Mode::TextView)
    {
        m_initialWidth = TextViewWidth;
        m_initialHeight = TextViewHeight;
        m_progressVisible = false;

        // A text window has nothing to collapse: the output is the window, and the buttons are there from
        // the start.
        m_cancelVisible = true;
        m_detailsExpanded = true;
        m_copyVisible = true;
    }
    else
    {
        m_initialWidth = ProgressWidth;
        m_initialHeight = ProgressHeight;
    }
}

int ProgressController::maximum() const
{
    return m_maximum;
}

void ProgressController::setMaximum(const int maximum)
{
    if (assign(m_maximum, maximum))
    {
        Q_EMIT maximumChanged();
    }
}

int ProgressController::value() const
{
    return m_value;
}

void ProgressController::setValue(const int value)
{
    // A bar never shows less than nothing or more than its end.
    if (assign(m_value, qBound(0, value, qMax(0, m_maximum))))
    {
        Q_EMIT valueChanged();
    }

    publishTaskProgress(true);

    if (m_autoClose && value >= m_maximum)
    {
        close();
    }
}

bool ProgressController::autoClose() const
{
    return m_autoClose;
}

void ProgressController::setAutoClose(const bool autoClose)
{
    m_autoClose = autoClose;
}

bool ProgressController::isTextView() const
{
    return m_mode == Mode::TextView;
}

int ProgressController::initialWidth() const
{
    return m_initialWidth;
}

int ProgressController::initialHeight() const
{
    return m_initialHeight;
}

void ProgressController::setInitialSize(const int width, const int height)
{
    if (width > 0 && height > 0)
    {
        m_initialWidth = width;
        m_initialHeight = height;
    }
}

QString ProgressController::text() const
{
    return m_text;
}

void ProgressController::setLabelText(const QString& text)
{
    if (assign(m_text, text))
    {
        Q_EMIT textChanged();
    }
}

QString ProgressController::header() const
{
    return m_header;
}

void ProgressController::setHeader(const QString& header)
{
    if (assign(m_header, header))
    {
        Q_EMIT headerChanged();
    }
}

QString ProgressController::title() const
{
    return m_title;
}

void ProgressController::setTitle(const QString& title)
{
    if (assign(m_title, title))
    {
        Q_EMIT titleChanged();
    }
}

bool ProgressController::progressVisible() const
{
    return m_progressVisible;
}

void ProgressController::setProgressVisible(const bool visible)
{
    if (assign(m_progressVisible, visible))
    {
        Q_EMIT progressVisibleChanged();
    }
}

bool ProgressController::cancelVisible() const
{
    return m_cancelVisible;
}

void ProgressController::showCancelButton(const bool show)
{
    if (assign(m_cancelVisible, show))
    {
        Q_EMIT cancelVisibleChanged();
    }
}

bool ProgressController::wasCancelled() const
{
    return m_cancelled;
}

bool ProgressController::isFinished() const
{
    return m_finished;
}

bool ProgressController::detailsAvailable() const
{
    return m_detailsAvailable;
}

bool ProgressController::detailsExpanded() const
{
    return m_detailsExpanded;
}

void ProgressController::setDetailsExpanded(const bool expanded)
{
    if (assign(m_detailsExpanded, expanded))
    {
        Q_EMIT detailsExpandedChanged();
    }
}

bool ProgressController::copyVisible() const
{
    return m_copyVisible;
}

void ProgressController::resizeWindow(const int width, const int height)
{
    if (width > 0 && height > 0)
    {
        Q_EMIT resizeRequested(width, height);
    }
}

/// `kurodlg` by default, so the bar lands on this window's own task. A caller that would rather have
/// it on its icon - the file manager that started the job, say - passes that entry instead.
void ProgressController::setLauncher(const QString& desktopEntry)
{
    if (desktopEntry.isEmpty())
    {
        m_launcherUri.clear();

        return;
    }

    QString name = desktopEntry;

    if (name.endsWith(".desktop"_L1))
    {
        name.chop(8);
    }

    m_launcherUri = u"application://%1.desktop"_s.arg(name);
}

/// The panel matches this to a task through its desktop entry, so a program without an installed
/// .desktop file has nothing for the bar to be drawn on, however correct the signal is.
void ProgressController::publishTaskProgress(const bool visible)
{
    if (m_launcherUri.isEmpty() || m_maximum <= 0)
    {
        return;
    }

    QDBusMessage signal = QDBusMessage::createSignal(UnityPath, UnityInterface, u"Update"_s);
    const QVariantMap properties{
        {u"progress"_s, static_cast<double>(m_value) / m_maximum},
        {u"progress-visible"_s, visible},
    };

    signal << m_launcherUri << properties;
    QDBusConnection::sessionBus().send(signal);
}

QString ProgressController::output() const
{
    return m_output;
}

void ProgressController::copyOutput() const
{
    QGuiApplication::clipboard()->setText(m_output);
}

void ProgressController::appendRaw(const QString& text)
{
    if (text.isEmpty())
    {
        return;
    }

    // Hidden until there is something in it. A "Details" disclosure that opens onto an empty box is
    // worse than no disclosure: it reads as output that failed to arrive.
    if (m_mode == Mode::Progress && assign(m_detailsAvailable, true))
    {
        Q_EMIT detailsAvailableChanged();
    }

    if (assign(m_copyVisible, true))
    {
        Q_EMIT copyVisibleChanged();
    }

    m_output += text;
    m_outputLines += static_cast<int>(text.count(u'\n'));

    if (m_outputLines <= MaximumDetailLines)
    {
        Q_EMIT outputAppended(text);

        return;
    }

    // Drop whole lines from the start until the cap holds again.
    qsizetype cut = 0;

    for (; m_outputLines > MaximumDetailLines; --m_outputLines)
    {
        cut = m_output.indexOf(u'\n', cut) + 1;
    }

    m_output.remove(0, cut);
    Q_EMIT outputReset();
}

void ProgressController::appendDetail(const QString& text)
{
    appendRaw(text.endsWith(u'\n') ? text : text + u'\n');
}

void ProgressController::clearDetails()
{
    m_output.clear();
    m_outputLines = 0;
    Q_EMIT outputReset();
}

void ProgressController::setDetailsVisible(const bool visible)
{
    if (m_mode == Mode::Progress && assign(m_detailsAvailable, visible))
    {
        Q_EMIT detailsAvailableChanged();
    }

    setDetailsExpanded(visible);

    if (assign(m_copyVisible, visible))
    {
        Q_EMIT copyVisibleChanged();
    }
}

void ProgressController::cancel()
{
    // The caller decides what cancelling means and how long it takes, so the window stays up
    // and only stops offering the button again.
    if (assign(m_cancelled, true))
    {
        Q_EMIT cancelledChanged();
    }
}

/// Shown even when the cancel button never was: a window that has said what happened needs a way out
/// that is not the title bar, and in --progress nothing else offers one.
void ProgressController::setFinished()
{
    // The window goes on saying what happened; the task icon has nothing left to count.
    publishTaskProgress(false);

    if (assign(m_finished, true))
    {
        Q_EMIT finishedChanged();
    }
}

void ProgressController::accept()
{
    done(true);
}

void ProgressController::reject()
{
    done(false);
}

void ProgressController::close()
{
    done(false);
}

/// Esc, the window manager's close button and the D-Bus `close` all arrive here, and the process is
/// this one window - so it ends with it. Anything but an explicit accept counts as cancelled, which is
/// what a caller polling wasCancelled() before the window vanishes gets to see.
void ProgressController::done(const bool accepted)
{
    if (m_done)
    {
        return;
    }

    m_done = true;

    if (!accepted && assign(m_cancelled, true))
    {
        Q_EMIT cancelledChanged();
    }

    // Matters for --launcher: that icon belongs to a program that outlives this one, and a bar
    // nobody takes down stays on it. send() only queues, so the loop gets one turn to write it out
    // before the process is gone.
    publishTaskProgress(false);
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    QCoreApplication::quit();
}

#include "moc_progresscontroller.cpp"
