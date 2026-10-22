// main.cpp — MuslimNotify entry point.
// SPDX-License-Identifier: MIT
#include <QApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QFile>
#include <QTimer>

#include "appstate.h"
#include "audioplayer.h"
#include "icons.h"
#include "mainwindow.h"
#include "settingsdialog.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("MuslimNotify"));
    QApplication::setOrganizationDomain(QStringLiteral("muslimnotify.org"));
    QApplication::setApplicationName(QStringLiteral("MuslimNotify"));
    QApplication::setApplicationVersion(QStringLiteral(MN_VERSION_PROJECT));
    QApplication::setWindowIcon(Icons::appIcon(64));
    QApplication::setQuitOnLastWindowClosed(true);

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("MuslimNotify — prayer times, Adhan and desktop alerts.\n"
                       "Rust core + Qt frontend, open source under the MIT license."));
    parser.addHelpOption();
    parser.addVersionOption();
    // Verification helpers: render the window to a PNG and exit; freeze the
    // scheduler clock so alert firing can be demonstrated deterministically.
    QCommandLineOption shotOpt("screenshot", QStringLiteral("Render the main window to <file> and exit."),
                               QStringLiteral("file"));
    QCommandLineOption simOpt("simulate", QStringLiteral("Freeze the scheduler clock at HH:MM:SS."),
                              QStringLiteral("time"));
    QCommandLineOption liveOpt("live", QStringLiteral("Force a visible live window even under automation."));
    QCommandLineOption shotSettingsOpt("shot-settings",
                                       QStringLiteral("In screenshot mode, capture the settings dialog instead."));
    parser.addOption(shotOpt);
    parser.addOption(simOpt);
    parser.addOption(liveOpt);
    parser.addOption(shotSettingsOpt);
    parser.process(app);

    if (parser.isSet(simOpt)) {
        const QString t = parser.value(simOpt);
        const QTime tt = QTime::fromString(t, QStringLiteral("HH:mm:ss"));
        MainWindow::s_simulateSecs = tt.isValid() ? tt.msecsSinceStartOfDay() / 1000 : 0;
    }
    const bool screenshotMode = parser.isSet(shotOpt);

    AppState state;
    state.load();

    // Optional startup convenience (off by default — privacy first).
    // The IP lookup itself runs from the settings dialog; startup detection
    // simply replays the stored zone so the clock is right before first use.

    AudioPlayer audio;
    MainWindow win(&state, &audio);

    if (screenshotMode) {
        const QString shotPath = parser.value(shotOpt);
        win.show();
        // Let the first ticks run so the hero card fills in.
        QTimer::singleShot(1200, &app, [&app, &win, shotPath, &parser, &state, shotSettingsOpt] {
            QWidget *shot = &win;
            SettingsDialog *dlg = nullptr;
            if (parser.isSet(shotSettingsOpt)) {
                dlg = new SettingsDialog(&state, &win);
                dlg->show();
                shot = dlg;
            }
            const QPixmap pm = shot->grab();
            pm.save(shotPath);
            qInfo("Screenshot saved: %s", qPrintable(shotPath));
            app.quit();
        });
        const int rc = app.exec();
        return rc;
    }

    win.show();
    return app.exec();
}
