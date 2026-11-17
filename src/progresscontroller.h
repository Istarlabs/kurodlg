// SPDX-FileCopyrightText: 2026 Pascal Klose
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QtQmlIntegration>

/// What the progress window shows, without the window - and, with the bar switched off, the console
/// output window as well. Both are one window because Dolphin's own long-running operations are: a bar
/// with a "Details" area underneath that holds the command's output.
///
/// The D-Bus adaptor is attached to this object, so the slots below are the interface the callers see.
class ProgressController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by main()")

    // The first three are the interface's own properties.
    Q_PROPERTY(int maximum READ maximum WRITE setMaximum NOTIFY maximumChanged)
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(bool autoClose READ autoClose WRITE setAutoClose)

    Q_PROPERTY(bool textView READ isTextView CONSTANT)
    Q_PROPERTY(int initialWidth READ initialWidth CONSTANT)
    Q_PROPERTY(int initialHeight READ initialHeight CONSTANT)
    Q_PROPERTY(QString text READ text NOTIFY textChanged)
    Q_PROPERTY(QString header READ header NOTIFY headerChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(bool progressVisible READ progressVisible NOTIFY progressVisibleChanged)
    Q_PROPERTY(bool cancelVisible READ cancelVisible NOTIFY cancelVisibleChanged)
    Q_PROPERTY(bool cancelled READ wasCancelled NOTIFY cancelledChanged)
    Q_PROPERTY(bool finished READ isFinished NOTIFY finishedChanged)
    Q_PROPERTY(bool detailsAvailable READ detailsAvailable NOTIFY detailsAvailableChanged)
    Q_PROPERTY(bool detailsExpanded READ detailsExpanded WRITE setDetailsExpanded NOTIFY detailsExpandedChanged)
    Q_PROPERTY(bool copyVisible READ copyVisible NOTIFY copyVisibleChanged)

public:
    enum class Mode
    {
        Progress,
        TextView,
    };

    ProgressController(Mode mode, const QString& text, int totalSteps, QObject* parent = nullptr);

    [[nodiscard]] int maximum() const;
    void setMaximum(int maximum);
    [[nodiscard]] int value() const;
    void setValue(int value);
    [[nodiscard]] bool autoClose() const;
    void setAutoClose(bool autoClose);

    [[nodiscard]] bool isTextView() const;
    [[nodiscard]] int initialWidth() const;
    [[nodiscard]] int initialHeight() const;
    [[nodiscard]] QString text() const;
    [[nodiscard]] QString header() const;
    [[nodiscard]] QString title() const;
    [[nodiscard]] bool progressVisible() const;
    [[nodiscard]] bool cancelVisible() const;
    [[nodiscard]] bool isFinished() const;
    [[nodiscard]] bool detailsAvailable() const;
    [[nodiscard]] bool detailsExpanded() const;
    void setDetailsExpanded(bool expanded);
    [[nodiscard]] bool copyVisible() const;

    /// The size the window opens with, before it is shown. `--size` lands here.
    void setInitialSize(int width, int height);

    /// Appends without touching the text - what arrives from a pipe already carries its newlines.
    void appendRaw(const QString& text);

    /// Whose task icon carries the progress, as a desktop entry name. Empty switches it off.
    void setLauncher(const QString& desktopEntry);

    /// What the window shows for "the output so far".
    Q_INVOKABLE [[nodiscard]] QString output() const;

    /// The whole output, to the clipboard.
    Q_INVOKABLE void copyOutput() const;

    /// The button the window shows while the work runs; the caller decides what cancelling means.
    Q_INVOKABLE void cancel();

    /// The button the window shows once the work is over.
    Q_INVOKABLE void accept();

    /// Esc, the window manager's close button: anything but accept() counts as cancelled.
    Q_INVOKABLE void reject();

public Q_SLOTS:
    void setLabelText(const QString& text);
    void showCancelButton(bool show);
    [[nodiscard]] bool wasCancelled() const;

    /// The D-Bus `close` and autoClose arrive here, and the process is this one window - so it ends with it.
    void close();

    void setTitle(const QString& title);
    void setHeader(const QString& header);
    void resizeWindow(int width, int height);
    void appendDetail(const QString& text);
    void clearDetails();
    void setDetailsVisible(bool visible);
    void setProgressVisible(bool visible);

    /// The work is over: the button turns from Cancel into Close, and the window waits for it.
    void setFinished();

Q_SIGNALS:
    void maximumChanged();
    void valueChanged();
    void textChanged();
    void headerChanged();
    void titleChanged();
    void progressVisibleChanged();
    void cancelVisibleChanged();
    void cancelledChanged();
    void finishedChanged();
    void detailsAvailableChanged();
    void detailsExpandedChanged();
    void copyVisibleChanged();

    /// New output to append to what the window shows.
    void outputAppended(const QString& text);

    /// The output changed in a way an append cannot express (cleared, or its start dropped): show `output()`.
    void outputReset();

    /// The caller asked for another size through D-Bus.
    void resizeRequested(int width, int height);

private:
    void done(bool accepted);

    /// The bar on the task icon, over the Unity LauncherEntry interface the panel listens on.
    void publishTaskProgress(bool visible);

    Mode m_mode = Mode::Progress;
    bool m_cancelled = false;
    bool m_autoClose = false;
    bool m_done = false;
    QString m_launcherUri;

    int m_maximum = 0;
    int m_value = 0;
    int m_initialWidth = 0;
    int m_initialHeight = 0;
    QString m_text;
    QString m_header;
    QString m_title;
    bool m_progressVisible = true;
    bool m_cancelVisible = false;
    bool m_finished = false;
    bool m_detailsAvailable = false;
    bool m_detailsExpanded = false;
    bool m_copyVisible = false;

    QString m_output;
    int m_outputLines = 0;
};
