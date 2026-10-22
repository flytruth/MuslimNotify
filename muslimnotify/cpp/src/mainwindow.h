// mainwindow.h — MuslimNotify main window, tray and scheduler.
// SPDX-License-Identifier: MIT
#pragma once

#include <QMainWindow>
#include <QSystemTrayIcon>

#include "appstate.h"

class AudioPlayer;
class QLabel;
class QProgressBar;
class QPushButton;

// One interactive row of the prayer table.
struct PrayerRow {
    QWidget *root = nullptr;
    QLabel *dot = nullptr;
    QLabel *name = nullptr;
    QLabel *arabic = nullptr;
    QLabel *time = nullptr;
    QLabel *badge = nullptr;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(AppState *state, AudioPlayer *audio, QWidget *parent = nullptr);

    // Verification hooks (also used by --screenshot / --simulate).
    static int s_simulateSecs;  // -1 = live clock; otherwise the base the
                                // simulated clock runs forward from (1:1).

protected:
    void closeEvent(QCloseEvent *e) override;

private slots:
    void onTick();
    void refreshDay();
    void openSettings();
    void showAbout();
    void testAdhan();
    void toggleVisible();

private:
    void buildUi();
    void applyTheme();
    void updateRows(const DayTimes &day, int nowSecs);
    void updateHero(const DayTimes &day, int nowSecs);
    void checkAlarm(const DayTimes &day, const QDate &date, int nowSecs);
    void fireAlert(int slot, const QDate &date, bool preAlert);
    void pushBanner(const QString &text);

    AppState *m_state;
    AudioPlayer *m_audio;
    QSystemTrayIcon *m_tray = nullptr;

    qint64 m_simStartReal = -1;  // real-clock anchor for simulated runs
    int m_simBase = 0;

    QTimer *m_timer = nullptr;
    QDate m_day;
    DayTimes m_today;
    int m_lastTick = -1;
    QString m_lastFiredKey;
    QString m_lastPreKey;

    // hero card
    QLabel *m_heroEyebrow = nullptr;
    QLabel *m_heroName = nullptr;
    QLabel *m_heroArabic = nullptr;
    QLabel *m_heroTime = nullptr;
    QLabel *m_heroCountdown = nullptr;
    QLabel *m_heroSub = nullptr;
    QProgressBar *m_heroProgress = nullptr;
    QWidget *m_heroCard = nullptr;

    // chrome
    QLabel *m_locLabel = nullptr;
    QLabel *m_banner = nullptr;
    QLabel *m_playerLabel = nullptr;
    QVector<PrayerRow> m_rows;
};
