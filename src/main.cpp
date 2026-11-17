// SPDX-FileCopyrightText: 2026 Pascal Klose
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fieldspec.h"
#include "formcontroller.h"
#include "presetspec.h"
#include "progresscontroller.h"
#include "progressdialogadaptor.h"
#include "progressjob.h"

#include <KAboutData>
#include <KCrash>
#include <KLocalizedQmlContext>
#include <KLocalizedString>
#include <KNotification>
#include <KUiServerV2JobTracker>

#include <QApplication>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QElapsedTimer>
#include <QFile>
#include <QIcon>
#include <QProcess>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QSocketNotifier>
#include <QTextStream>
#include <QThread>
#include <QTimer>

#include <array>
#include <unistd.h>

using namespace Qt::StringLiterals;

namespace
{
    constexpr auto ServicePrefix = "net.kurokyuu.kurodlg-"_L1;
    constexpr auto ObjectPath = "/ProgressDialog"_L1;

    /// The child has to be up on the bus before the handle is printed, or a caller that acts on that
    /// line immediately talks to a name that does not exist yet.
    constexpr int RegistrationTimeoutMs = 15000;

    QTextStream& out()
    {
        static QTextStream stream(stdout);
        return stream;
    }

    QTextStream& err()
    {
        static QTextStream stream(stderr);
        return stream;
    }

    /// The desktop entry this program is installed under. The panel needs it twice: to match the
    /// window to a task at all, and to find that task again when the progress signal names it.
    constexpr auto DesktopEntry = "kurodlg"_L1;

    struct WindowOptions
    {
        QString title;
        QString header;
        QString icon;
        QString launcher;
        int width = 0;
        int height = 0;
    };

    /// `--size 640x420`. An unparsable value is an error rather than a silently ignored argument,
    /// because the whole point of passing it is that the default size was wrong.
    bool readSize(const QString& value, WindowOptions* options)
    {
        static const QRegularExpression pattern(u"^(\\d+)[xX](\\d+)$"_s);
        const QRegularExpressionMatch match = pattern.match(value);

        if (!match.hasMatch())
        {
            return false;
        }

        options->width = match.captured(1).toInt();
        options->height = match.captured(2).toInt();
        return true;
    }

    QString windowTitle(const WindowOptions& options)
    {
        return options.title.isEmpty() ? QGuiApplication::applicationDisplayName() : options.title;
    }

    /// Loads the window `type` of the QML module with `controller` as its `controller` property - none for a window
    /// without one - and applies what the command line said about the window. Null, with the reason on stderr,
    /// when it did not load.
    QQuickWindow* openWindow(QQmlEngine& engine, const QString& type, QObject* controller, const WindowOptions& options)
    {
        auto* localization = new KLocalizedQmlContext(&engine);
        engine.rootContext()->setContextObject(localization);
        QQmlEngine::setContextForObject(localization, engine.rootContext());

        QVariantMap properties;

        if (controller != nullptr)
        {
            properties.insert(u"controller"_s, QVariant::fromValue(controller));
        }

        QQmlComponent component(&engine, "net.kurokyuu.kurodlg", type, QQmlComponent::PreferSynchronous);
        auto* window = qobject_cast<QQuickWindow*>(component.createWithInitialProperties(properties));

        if (window == nullptr)
        {
            err() << i18n("Cannot load the %1 window: %2", type, component.errorString()) << Qt::endl;
            return nullptr;
        }

        // Owned by the engine, so it goes down with it.
        window->QObject::setParent(&engine);

        if (!options.icon.isEmpty())
        {
            window->setIcon(QIcon::fromTheme(options.icon));
        }

        return window;
    }

    int runForm(QCommandLineParser& parser, const WindowOptions& options)
    {
        QList<FieldSpec> fields;

        for (const QString& spec : parser.values(u"field"_s))
        {
            QString error;
            const FieldSpec field = FieldSpec::parse(spec, &error);

            if (field.type == FieldType::Invalid)
            {
                err() << error << Qt::endl;
                return 2;
            }

            fields.append(field);
        }

        if (fields.isEmpty())
        {
            err() << i18n("--form needs at least one --field NAME:TYPE:Label[:Default[:Extra]]") << Qt::endl;
            return 2;
        }

        QList<PresetSpec> presets;

        for (const QString& spec : parser.values(u"preset"_s))
        {
            QString error;
            const PresetSpec preset = PresetSpec::parse(spec, &error);

            if (preset.field.isEmpty())
            {
                err() << error << Qt::endl;
                return 2;
            }

            presets.append(preset);
        }

        if (QString error; !resolvePresets(&presets, fields, &error))
        {
            err() << error << Qt::endl;
            return 2;
        }

        // Declared before the engine, so it outlives the window that points at it.
        FormController controller(parser.value(u"form"_s), fields, presets);
        QQmlEngine engine;
        QQuickWindow* window = openWindow(engine, u"FormWindow"_s, &controller, options);

        if (window == nullptr)
        {
            return 2;
        }

        window->setTitle(windowTitle(options));

        if (options.width > 0 && options.height > 0)
        {
            window->resize(options.width, options.height);
        }

        QApplication::exec();

        if (!controller.accepted())
        {
            return 1;
        }

        const QStringList lines = controller.lines();

        for (const QString& line : lines)
        {
            out() << line << Qt::endl;
        }

        return 0;
    }

    int runTextView(const QCommandLineParser& parser, const WindowOptions& options)
    {
        // Declared before the engine, so it outlives the window that points at it.
        ProgressController controller(ProgressController::Mode::TextView, parser.value(u"textbox"_s), 100);
        controller.setTitle(windowTitle(options));
        controller.setInitialSize(options.width, options.height);
        controller.setHeader(options.header);

        if (const QString fileName = parser.value(u"file"_s); !fileName.isEmpty())
        {
            QFile file(fileName);

            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                err() << i18n("Cannot read \"%1\": %2", fileName, file.errorString()) << Qt::endl;
                return 2;
            }

            controller.appendRaw(QString::fromLocal8Bit(file.readAll()));
            controller.setFinished();
        }
        else
        {
            // stdin stays open while the producing command runs, so the text has to arrive through the
            // event loop rather than a blocking read - otherwise the window never repaints.
            auto* notifier = new QSocketNotifier(STDIN_FILENO, QSocketNotifier::Read, &controller);

            QObject::connect(notifier, &QSocketNotifier::activated, &controller,
                             [notifier, &controller]
                             {
                                 std::array<char, 4096> buffer{};

                                 if (const ssize_t count = ::read(STDIN_FILENO, buffer.data(), buffer.size());
                                     count > 0)
                                 {
                                     controller.appendRaw(QString::fromLocal8Bit(buffer.data(), count));
                                     return;
                                 }

                                 notifier->setEnabled(false);
                                 controller.setFinished();
                             });
        }

        QQmlEngine engine;

        if (openWindow(engine, u"ProgressWindow"_s, &controller, options) == nullptr)
        {
            return 2;
        }

        return QApplication::exec() == 0 && !controller.wasCancelled() ? 0 : 1;
    }

    /// The dialog half of `--progress`: this process owns the window and answers on the bus until the
    /// caller closes it. The name it answers under is handed down by the launcher rather than derived
    /// here - see runProgressLauncher().
    int runProgressServer(const QCommandLineParser& parser, const WindowOptions& options)
    {
        const QStringList arguments = parser.positionalArguments();
        bool ok = false;
        const int totalSteps = arguments.isEmpty() ? 100 : arguments.first().toInt(&ok);

        // Declared before the engine, so it outlives the window that points at it.
        ProgressController controller(ProgressController::Mode::Progress, parser.value(u"progress"_s),
                                      ok ? totalSteps : 100);
        controller.setTitle(windowTitle(options));
        controller.setInitialSize(options.width, options.height);
        controller.setHeader(options.header);
        controller.setDetailsVisible(parser.isSet(u"details"_s));

        const bool asJob = parser.isSet(u"job"_s);

        // A job's progress is drawn on the task icon by Plasma itself, so the Unity signal would only draw it twice.
        controller.setLauncher(asJob                            ? QString()
                                   : options.launcher.isEmpty() ? QString(DesktopEntry)
                                                                : options.launcher);

        new ProgressDialogAdaptor(&controller);

        QDBusConnection bus = QDBusConnection::sessionBus();

        if (const QString service = parser.value(u"progress-server"_s);
            !bus.registerService(service) || !bus.registerObject(ObjectPath, &controller))
        {
            err() << i18n("Cannot register \"%1\" on the session bus", service) << Qt::endl;
            return 2;
        }

        if (asJob)
        {
            // The tracker before the job, so the job is gone first and the tracker sees it go.
            KUiServerV2JobTracker tracker;
            ProgressJob job(&controller);

            // Whose task and notification the job belongs to: the program that started it, with --launcher.
            if (!options.launcher.isEmpty())
            {
                QString launcher = options.launcher;
                launcher.remove(QRegularExpression(u"\\.desktop$"_s));
                job.setProperty("desktopFileName", launcher);
            }

            tracker.registerJob(&job);
            job.start();
            return QApplication::exec();
        }

        QQmlEngine engine;

        if (openWindow(engine, u"ProgressWindow"_s, &controller, options) == nullptr)
        {
            return 2;
        }

        return QApplication::exec();
    }

    /// `--about`: the KDE About page, filled from the KAboutData main() sets up.
    int runAbout(const WindowOptions& options)
    {
        QQmlEngine engine;
        QQuickWindow* window = openWindow(engine, u"AboutWindow"_s, nullptr, options);

        if (window == nullptr)
        {
            return 2;
        }

        if (!options.title.isEmpty())
        {
            window->setTitle(options.title);
        }

        return QApplication::exec();
    }

    /// `--passivepopup TEXT [TIMEOUT]`, as kdialog has it: a notification without a window. The process stays
    /// until the notification is gone, so `kurodlg --passivepopup … &` is how a script goes on meanwhile.
    int runPassivePopup(const QCommandLineParser& parser, const WindowOptions& options)
    {
        const QStringList arguments = parser.positionalArguments();
        bool ok = false;
        const int timeout = arguments.isEmpty() ? 0 : arguments.first().toInt(&ok);

        if (!arguments.isEmpty() && (!ok || timeout < 0))
        {
            err() << i18n("--passivepopup takes its timeout in seconds, for example 5") << Qt::endl;
            return 2;
        }

        KNotification* notification =
            KNotification::event(KNotification::Notification, windowTitle(options), parser.value(u"passivepopup"_s),
                                 options.icon.isEmpty() ? u"dialog-information"_s : options.icon);

        QObject::connect(notification, &KNotification::closed, &QCoreApplication::quit);

        if (timeout > 0)
        {
            QTimer::singleShot(std::chrono::seconds(timeout), notification, &KNotification::close);
        }

        return QApplication::exec();
    }

    /// The caller's half of `--progress`: start the dialog detached, wait for it to appear on the bus,
    /// print `<service> <path>` and get out of the way.
    int runProgressLauncher()
    {
        // The name cannot be derived from the dialog's process id: QProcess::startDetached forks twice
        // and the id it reports is not the one the surviving process ends up with. So the launcher
        // picks the name and hands it down. Its own id is unique for as long as it matters, because it
        // stays alive until the dialog has claimed the name.
        const QString service = ServicePrefix + QString::number(::getpid());

        QStringList arguments = QCoreApplication::arguments().mid(1);
        arguments.prepend(service);
        arguments.prepend(u"--progress-server"_s);

        // The detached dialog inherits stdout and stderr, and it outlives this process by design - so
        // in `handle=$(kurodlg --progress …)` it would hold the command substitution's pipe open and
        // the calling shell would wait for an EOF that never comes. Only the member overload of
        // startDetached can redirect, which is why this is not the static one.
        QProcess dialogProcess;
        dialogProcess.setProgram(QCoreApplication::applicationFilePath());
        dialogProcess.setArguments(arguments);
        dialogProcess.setStandardOutputFile(QProcess::nullDevice());
        dialogProcess.setStandardErrorFile(QProcess::nullDevice());

        if (!dialogProcess.startDetached())
        {
            err() << i18n("Cannot start the progress dialog") << Qt::endl;
            err().flush();
            return 2;
        }

        // Polled rather than watched: a QDBusServiceWatcher only reports the transition, so a dialog
        // that registers before the match rule is in place would never be noticed.
        const QDBusConnectionInterface* bus = QDBusConnection::sessionBus().interface();
        QElapsedTimer waited;
        waited.start();

        while (waited.elapsed() < RegistrationTimeoutMs)
        {
            if (bus && bus->isServiceRegistered(service))
            {
                out() << service << u' ' << ObjectPath << Qt::endl;
                out().flush();
                return 0;
            }

            QThread::msleep(20);
        }

        err() << i18n("The progress dialog did not come up on the session bus") << Qt::endl;
        err().flush();
        return 2;
    }
} // namespace

int main(int argc, char* argv[])
{
    // Not const, although clang-tidy offers it: the only reason it can be is that exec() and quit()
    // are static. The object this names is the running event loop, and it changes throughout.
    // NOLINTNEXTLINE(misc-const-correctness)
    QApplication application(argc, argv);

    KLocalizedString::setApplicationDomain(QByteArrayLiteral("kurodlg"));
    // The Breeze look of Qt Quick Controls; a style the user chose through the environment is theirs to keep.
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE"))
    {
        QQuickStyle::setStyle(u"org.kde.desktop"_s);
    }

    KAboutData about(u"kurodlg"_s, i18n("kurodlg"), QStringLiteral(KURODLG_VERSION),
                     i18n("Dialogs for shell scripts and service menus: a progress window with a details area, "
                          "a multi-field form, a window that shows a command's output, and notifications."),
                     KAboutLicense::GPL_V3, i18n("© 2026 Pascal Klose"));
    about.setLicense(KAboutLicense::GPL_V3, KAboutLicense::OrLaterVersions);
    about.addAuthor(i18n("Pascal Klose"));
    about.setBugAddress(QByteArrayLiteral("https://github.com/Istarlabs/kurodlg/issues"));

    // What the window announces itself as (the Wayland app_id), and therefore the desktop entry the
    // panel looks the task up under. Without it there is no task for a progress bar to sit on.
    about.setDesktopFileName(QString(DesktopEntry));

    // Sets the application name, display name, version and desktop file name in one go.
    KAboutData::setApplicationData(about);
    KCrash::initialize();

    // --help, --version, --author and --license, and the description above.
    QCommandLineParser parser;
    about.setupCommandLine(&parser);

    parser.addOptions({
        {u"progress"_s, i18n("Show a progress window and print \"<service> <path>\" to drive it over D-Bus."),
         i18n("text")},
        {u"form"_s, i18n("Show a form built from the --field arguments."), i18n("text")},
        {u"field"_s, i18n("A form field, NAME:TYPE:Label[:Default[:Extra]]. Repeatable."), i18n("spec")},
        {u"preset"_s,
         i18n("Picking CHOICE in the combo FIELD sets the named fields, FIELD:CHOICE:NAME=VALUE[:NAME=VALUE...]. "
              "Repeatable."),
         i18n("spec")},
        {u"textbox"_s, i18n("Show a window with the text arriving on stdin."), i18n("text")},
        {u"file"_s, i18n("Read --textbox content from this file instead of stdin."), i18n("path")},
        {u"title"_s, i18n("Window title."), i18n("text")},
        {u"header"_s, i18n("A heading shown inside the dialog, above the label."), i18n("text")},
        {u"icon"_s, i18n("Window icon, by icon theme name."), i18n("name")},
        {u"size"_s, i18n("Window size, for example 640x420."), i18n("WxH")},
        {u"details"_s, i18n("Start with the details area expanded.")},
        {u"launcher"_s, i18n("Show the progress on this desktop entry's task icon instead of kurodlg's."),
         i18n("name")},
        {u"about"_s, i18n("Show the About page: version, authors, licence and where to report bugs.")},
        {u"job"_s, i18n("With --progress: no window, show the progress as a Plasma job notification instead.")},
        {u"passivepopup"_s,
         i18n("Show a notification with this text; an optional TIMEOUT in seconds closes it earlier."), i18n("text")},
    });

    // Internal: how the launcher tells the detached dialog which bus name to claim.
    QCommandLineOption serverOption(u"progress-server"_s, QString(), u"service"_s);
    serverOption.setFlags(QCommandLineOption::HiddenFromHelp);
    parser.addOption(serverOption);

    parser.addPositionalArgument(
        u"number"_s,
        i18n("With --progress: the steps the bar counts to, default 100. With --passivepopup: the timeout."),
        u"[number]"_s);

    parser.process(application);
    about.processCommandLine(&parser);

    WindowOptions options;
    options.title = parser.value(u"title"_s);
    options.header = parser.value(u"header"_s);
    options.icon = parser.value(u"icon"_s);
    options.launcher = parser.value(u"launcher"_s);

    if (parser.isSet(u"size"_s) && !readSize(parser.value(u"size"_s), &options))
    {
        err() << i18n("--size takes WIDTHxHEIGHT, for example 640x420") << Qt::endl;
        return 2;
    }

    if (parser.isSet(u"form"_s))
    {
        return runForm(parser, options);
    }

    if (parser.isSet(u"textbox"_s))
    {
        return runTextView(parser, options);
    }

    if (parser.isSet(u"about"_s))
    {
        return runAbout(options);
    }

    if (parser.isSet(u"passivepopup"_s))
    {
        return runPassivePopup(parser, options);
    }

    if (parser.isSet(u"progress-server"_s))
    {
        return runProgressServer(parser, options);
    }

    if (parser.isSet(u"progress"_s))
    {
        return runProgressLauncher();
    }

    parser.showHelp();
    return 0;
}
