// mainwindow.cpp — main window implementation.
// SPDX-License-Identifier: MIT
#include "mainwindow.h"

#include <cstring>

#include <QApplication>
#include <QCloseEvent>
#include <QDate>
#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPalette>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QStyle>
#include <QStyleHints>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>

#include "audioplayer.h"
#include "icons.h"
#include "settingsdialog.h"

int MainWindow::s_simulateSecs = -1;

namespace {

QString countdownText(int secs) {
    if (secs < 0) secs = 0;
    const int h = secs / 3600;
    const int m = (secs % 3600) / 60;
    const int s = secs % 60;
    return QStringLiteral("%1:%2:%3")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));
}

}  // namespace

MainWindow::MainWindow(AppState *state, AudioPlayer *audio, QWidget *parent)
    : QMainWindow(parent), m_state(state), m_audio(audio) {
    setWindowTitle(QStringLiteral("MuslimNotify"));
    setWindowIcon(Icons::appIcon());
    setMinimumSize(400, 600);
    resize(430, 700);

    buildUi();
    applyTheme();
    refreshDay();

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &MainWindow::onTick);
    m_timer->start(1000);
    onTick();

    // Tray presence.
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        m_tray = new QSystemTrayIcon(Icons::appIcon(48), this);
        auto *menu = new QMenu(this);
        menu->addAction(QStringLiteral("Show / Hide"), this, &MainWindow::toggleVisible);
        menu->addAction(QStringLiteral("Test Adhan"), this, &MainWindow::testAdhan);
        menu->addSeparator();
        menu->addAction(QStringLiteral("Settings…"), this, &MainWindow::openSettings);
        menu->addAction(QStringLiteral("About"), this, &MainWindow::showAbout);
        menu->addSeparator();
        menu->addAction(QStringLiteral("Quit"), qApp, &QCoreApplication::quit);
        m_tray->setContextMenu(menu);
        m_tray->setToolTip(QStringLiteral("MuslimNotify — loading…"));
        connect(m_tray, &QSystemTrayIcon::activated, this,
                [this](QSystemTrayIcon::ActivationReason r) {
                    if (r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick) {
                        toggleVisible();
                    }
                });
        m_tray->show();
    }
}

void MainWindow::buildUi() {
    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(18, 14, 18, 14);
    root->setSpacing(12);

    // ---- header -----------------------------------------------------------
    auto *head = new QHBoxLayout();
    auto *title = new QLabel(QStringLiteral("MuslimNotify"));
    title->setObjectName("appTitle");
    auto *subtitle = new QLabel(QStringLiteral("Prayer times, Adhan & alerts"));
    subtitle->setObjectName("appSubtitle");

    auto *headLeft = new QVBoxLayout();
    headLeft->setSpacing(0);
    headLeft->addWidget(title);
    headLeft->addWidget(subtitle);

    auto *settingsBtn = new QPushButton(QStringLiteral("Settings"));
    settingsBtn->setObjectName("settingsBtn");
    connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::openSettings);
    auto *aboutBtn = new QPushButton(QStringLiteral("About"));
    aboutBtn->setObjectName("aboutBtn");
    connect(aboutBtn, &QPushButton::clicked, this, &MainWindow::showAbout);

    head->addLayout(headLeft, 1);
    head->addWidget(aboutBtn);
    head->addWidget(settingsBtn);
    root->addLayout(head);

    // ---- location strip ------------------------------------------------------
    m_locLabel = new QLabel();
    m_locLabel->setObjectName("locStrip");
    m_locLabel->setWordWrap(true);
    root->addWidget(m_locLabel);

    // ---- hero card -----------------------------------------------------------
    m_heroCard = new QFrame();
    m_heroCard->setObjectName("heroCard");
    auto *heroLay = new QVBoxLayout(m_heroCard);
    heroLay->setContentsMargins(20, 18, 20, 18);
    heroLay->setSpacing(4);

    m_heroEyebrow = new QLabel(QStringLiteral("NEXT PRAYER"));
    m_heroEyebrow->setObjectName("heroEyebrow");
    m_heroName = new QLabel(QStringLiteral("—"));
    m_heroName->setObjectName("heroName");
    m_heroArabic = new QLabel(QStringLiteral("—"));
    m_heroArabic->setObjectName("heroArabic");
    m_heroArabic->setAlignment(Qt::AlignRight);
    m_heroTime = new QLabel(QStringLiteral("--:--"));
    m_heroTime->setObjectName("heroTime");
    m_heroCountdown = new QLabel(QStringLiteral("00:00:00"));
    m_heroCountdown->setObjectName("heroCountdown");
    m_heroSub = new QLabel();
    m_heroSub->setObjectName("heroSub");
    m_heroProgress = new QProgressBar();
    m_heroProgress->setObjectName("heroProgress");
    m_heroProgress->setRange(0, 1000);
    m_heroProgress->setTextVisible(false);
    m_heroProgress->setFixedHeight(6);

    heroLay->addWidget(m_heroEyebrow);
    auto *nameRow = new QHBoxLayout();
    nameRow->addWidget(m_heroName);
    nameRow->addWidget(m_heroArabic, 1, Qt::AlignRight);
    heroLay->addLayout(nameRow);
    heroLay->addWidget(m_heroTime);
    heroLay->addWidget(m_heroCountdown);
    heroLay->addWidget(m_heroSub);
    heroLay->addSpacing(6);
    heroLay->addWidget(m_heroProgress);
    root->addWidget(m_heroCard);

    // ---- prayer rows ---------------------------------------------------------
    auto *scroll = new QScrollArea();
    scroll->setObjectName("prayerScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *listHost = new QWidget();
    auto *listLay = new QVBoxLayout(listHost);
    listLay->setContentsMargins(0, 0, 0, 0);
    listLay->setSpacing(8);
    for (int i = 0; i < MN_PRAYER_COUNT; ++i) {
        PrayerRow row;
        row.root = new QFrame();
        row.root->setObjectName("rowCard");
        row.root->setProperty("slotIndex", i);
        auto *lay = new QHBoxLayout(row.root);
        lay->setContentsMargins(12, 9, 14, 9);
        lay->setSpacing(10);
        row.dot = new QLabel();
        row.dot->setFixedSize(30, 30);
        row.dot->setAlignment(Qt::AlignCenter);
        row.name = new QLabel(QString::fromUtf8(::mn_prayer_name_en(i)));
        row.name->setObjectName("rowName");
        row.time = new QLabel(QStringLiteral("--:--"));
        row.time->setObjectName("rowTime");
        row.badge = new QLabel();
        row.badge->setObjectName("rowBadge");
        row.badge->setProperty("kind", "none");
        row.arabic = new QLabel(QString::fromUtf8(::mn_prayer_name_ar(i)));
        row.arabic->setObjectName("rowArabic");
        lay->addWidget(row.dot);
        lay->addWidget(row.name);
        lay->addWidget(row.badge);
        lay->addStretch(1);
        lay->addWidget(row.arabic);
        lay->addWidget(row.time);
        listLay->addWidget(row.root);
        m_rows.push_back(row);
    }
    listLay->addStretch(1);
    scroll->setWidget(listHost);
    root->addWidget(scroll, 1);

    // ---- footer ---------------------------------------------------------------
    auto *foot = new QHBoxLayout();
    auto *testBtn = new QPushButton(QStringLiteral("Test Adhan"));
    testBtn->setObjectName("testBtn");
    connect(testBtn, &QPushButton::clicked, this, &MainWindow::testAdhan);
    m_playerLabel = new QLabel();
    m_playerLabel->setObjectName("playerLabel");
    foot->addWidget(testBtn);
    foot->addWidget(m_playerLabel, 1);
    root->addLayout(foot);

    m_banner = new QLabel();
    m_banner->setObjectName("banner");
    m_banner->setWordWrap(true);
    m_banner->hide();
    root->addWidget(m_banner);

    connect(m_state, &AppState::changed, this, &MainWindow::refreshDay);
}

void MainWindow::applyTheme() {
    const bool dark =
        (m_state->theme == 2) ||
        (m_state->theme == 0 &&
         QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);
    const char *qss = dark ? R"(

        QMainWindow, QDialog, #prayerScroll > QWidget > QWidget { background: #0e1c17; }
        QWidget { color: #e7f2ec; font-size: 13px; }
        QGroupBox { border: 1px solid #1c3a2e; border-radius: 10px; margin-top: 14px;
            padding: 10px 8px 8px 8px; font-weight: 600; }
        QGroupBox::title { subcontrol-origin: margin; left: 12px; color: #7fe0bd; }
        QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox {
            background: #12251e; border: 1px solid #1c3a2e; border-radius: 8px;
            padding: 6px 8px; color: #e7f2ec; selection-background-color: #1d4234; }
        QComboBox:hover, QSpinBox:hover, QDoubleSpinBox:hover, QLineEdit:hover { border-color: #2a5c47; }
        QComboBox QAbstractItemView { background: #12251e; border: 1px solid #1c3a2e;
            selection-background-color: #1d4234; color: #e7f2ec; }
        QCheckBox { spacing: 7px; }
        QCheckBox::indicator, QRadioButton::indicator { width: 15px; height: 15px;
            border: 1px solid #2a5c47; border-radius: 4px; background: #12251e; }
        QCheckBox::indicator:checked { background: #34b58a; border-color: #34b58a;
            image: none; }
        #appTitle { font-size: 21px; font-weight: 700; color: #ffffff; }
        #appSubtitle { color: #8fb8a8; font-size: 12px; }
        QPushButton {
            background: #163026; border: 1px solid #1f4a3a; border-radius: 8px;
            padding: 7px 14px; color: #cfe8dc;
        }
        QPushButton:hover { background: #1d4234; }
        QPushButton:pressed { background: #143024; }
        #locStrip {
            background: #12251e; border: 1px solid #1c3a2e; border-radius: 8px;
            padding: 8px 12px; color: #9cc6b4;
        }
        #heroCard { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #123528, stop:1 #0e2a20);
            border: 1px solid #2a5c47; border-radius: 14px; }
        #heroCard[firing="1"] { border: 2px solid #e7c14d; }
        #heroEyebrow { color: #6f9c8a; font-size: 11px; letter-spacing: 2px; font-weight: 600; }
        #heroName { font-size: 26px; font-weight: 800; color: #ffffff; }
        #heroArabic { font-size: 22px; color: #e7c14d; }
        #heroTime { font-size: 40px; font-weight: 800; color: #7fe0bd; }
        #heroCountdown { font-size: 18px; color: #cfe8dc; }
        #heroSub { color: #8fb8a8; font-size: 12px; }
        QProgressBar { background: #0b211a; border: none; border-radius: 3px; }
        QProgressBar::chunk { background: #34b58a; border-radius: 3px; }
        #rowCard { background: #12251e; border: 1px solid #1c3a2e; border-radius: 10px; }
        #rowCard[state="current"] { border: 1px solid #34b58a; background: #143024; }
        #rowCard[state="next"] { border: 1px solid #e7c14d66; }
        #rowName { font-weight: 700; }
        #rowArabic { color: #9cc6b4; font-size: 14px; }
        #rowTime { font-weight: 700; font-size: 15px; color: #e7f2ec; }
        #rowBadge[kind="none"] { color: transparent; }
        #rowBadge[kind="next"] { color: #e7c14d; font-size: 11px; font-weight: 700;
            background: #e7c14d22; border-radius: 6px; padding: 2px 8px; }
        #rowBadge[kind="now"] { color: #0d241b; font-size: 11px; font-weight: 700;
            background: #34b58a; border-radius: 6px; padding: 2px 8px; }
        #playerLabel { color: #6f9c8a; font-size: 12px; }
        #banner { background: #e7c14d22; border: 1px solid #e7c14d66; border-radius: 8px;
            padding: 8px 12px; color: #f1d987; }
        QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }
        QScrollBar::handle:vertical { background: #1f4a3a; border-radius: 4px; min-height: 30px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    )" : R"(

        QMainWindow, QDialog, #prayerScroll > QWidget > QWidget { background: #f2f6f3; }
        QWidget { color: #14231d; font-size: 13px; }
        QGroupBox { border: 1px solid #dfeae2; border-radius: 10px; margin-top: 14px;
            padding: 10px 8px 8px 8px; font-weight: 600; }
        QGroupBox::title { subcontrol-origin: margin; left: 12px; color: #0e7a5f; }
        QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox {
            background: #ffffff; border: 1px solid #cfe0d6; border-radius: 8px;
            padding: 6px 8px; color: #14231d; selection-background-color: #dceed4; }
        QComboBox:hover, QSpinBox:hover, QDoubleSpinBox:hover, QLineEdit:hover { border-color: #0e7a5f; }
        QComboBox QAbstractItemView { background: #ffffff; border: 1px solid #cfe0d6;
            selection-background-color: #eaf7f1; color: #14231d; }
        QCheckBox { spacing: 7px; }
        QCheckBox::indicator, QRadioButton::indicator { width: 15px; height: 15px;
            border: 1px solid #b8cfc2; border-radius: 4px; background: #ffffff; }
        QCheckBox::indicator:checked { background: #0e7a5f; border-color: #0e7a5f;
            image: none; }
        #appTitle { font-size: 21px; font-weight: 700; color: #0c1f17; }
        #appSubtitle { color: #5d7c6e; font-size: 12px; }
        QPushButton {
            background: #ffffff; border: 1px solid #cfe0d6; border-radius: 8px;
            padding: 7px 14px; color: #14503c;
        }
        QPushButton:hover { background: #e8f4ee; }
        QPushButton:pressed { background: #dcede4; }
        #locStrip {
            background: #e7f1ea; border: 1px solid #d3e5da; border-radius: 8px;
            padding: 8px 12px; color: #3c6653;
        }
        #heroCard { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #ffffff, stop:1 #eef8f2);
            border: 1px solid #cfe0d6; border-radius: 14px; }
        #heroCard[firing="1"] { border: 2px solid #b8860b; }
        #heroEyebrow { color: #5d7c6e; font-size: 11px; letter-spacing: 2px; font-weight: 600; }
        #heroName { font-size: 26px; font-weight: 800; color: #0c1f17; }
        #heroArabic { font-size: 22px; color: #a97d0e; }
        #heroTime { font-size: 40px; font-weight: 800; color: #0e7a5f; }
        #heroCountdown { font-size: 18px; color: #14231d; }
        #heroSub { color: #5d7c6e; font-size: 12px; }
        QProgressBar { background: #dbe8e0; border: none; border-radius: 3px; }
        QProgressBar::chunk { background: #0e7a5f; border-radius: 3px; }
        #rowCard { background: #ffffff; border: 1px solid #dfeae2; border-radius: 10px; }
        #rowCard[property="current"] { border: 1px solid #0e7a5f; background: #eaf7f1; }
        #rowCard[property="next"] { border: 1px solid #b8860b55; }
        #rowName { font-weight: 700; }
        #rowArabic { color: #5d7c6e; font-size: 14px; }
        #rowTime { font-weight: 700; font-size: 15px; }
        #rowBadge[kind="none"] { color: transparent; }
        #rowBadge[kind="next"] { color: #8a6409; font-size: 11px; font-weight: 700;
            background: #f6ecc9; border-radius: 6px; padding: 2px 8px; }
        #rowBadge[kind="now"] { color: #ffffff; font-size: 11px; font-weight: 700;
            background: #0e7a5f; border-radius: 6px; padding: 2px 8px; }
        #playerLabel { color: #5d7c6e; font-size: 12px; }
        #banner { background: #f6ecc9; border: 1px solid #b8860b55; border-radius: 8px;
            padding: 8px 12px; color: #6e4f07; }
        QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }
        QScrollBar::handle:vertical { background: #c4d8cc; border-radius: 4px; min-height: 30px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    )";
    qApp->setStyleSheet(qss);

    // Row dots.
    for (int i = 0; i < m_rows.size(); ++i) {
        const QColor base = dark ? QColor("#1d4234") : QColor("#e0efe7");
        const QColor ink = dark ? QColor("#7fe0bd") : QColor("#0e7a5f");
        m_rows[i].dot->setPixmap(Icons::prayerDot(30, QString::fromUtf8(::mn_prayer_name_ar(i)), base, ink));
    }
}

void MainWindow::refreshDay() {
    applyTheme();
    m_day = QDate::currentDate();
    m_today = m_state->computeDay(m_day);
    m_lastTick = -1;  // force alarm window reset

    const QString method = QString::fromUtf8(::mn_method_display_name(m_state->methodId));
    m_locLabel->setText(QStringLiteral("Location: %1   ·   %2, %3   ·   %4")
                            .arg(m_state->cityLabel,
                                 m_state->zoneDisplayName(), method,
                                 QString::fromUtf8(::mn_version())
                                     .prepend("core v")));
    m_playerLabel->setText(
        QStringLiteral("Audio via: %1").arg(m_audio->playerDescription()));
    onTick();
}

void MainWindow::onTick() {
    const QDate today = QDate::currentDate();
    if (today != m_day) {
        refreshDay();
    }
    const int nowSecs = [this] {
        const int real = QTime::currentTime().msecsSinceStartOfDay() / 1000;
        if (s_simulateSecs < 0) {
            return real;
        }
        // Simulated clock: starts at s_simulateSecs and advances 1:1 with the
        // real clock so alarm crossing is exercisable deterministically.
        if (m_simStartReal < 0) {
            m_simStartReal = real;
            m_simBase = s_simulateSecs;
        }
        return m_simBase + static_cast<int>(real - m_simStartReal);
    }();
    updateHero(m_today, nowSecs);
    updateRows(m_today, nowSecs);
    checkAlarm(m_today, today, nowSecs);
    m_lastTick = nowSecs;
}

void MainWindow::updateHero(const DayTimes &day, int nowSecs) {
    // Find the next prayer among the five (skip sunrise slot as a target).
    int nextSlot = -1;
    int target = -1;
    for (int i = 0; i < MN_PRAYER_COUNT; ++i) {
        if (i == MN_SUNRISE || !day.has[i]) continue;
        const int t = day.times[i].msecsSinceStartOfDay() / 1000;
        if (t > nowSecs) {
            nextSlot = i;
            target = t;
            break;
        }
    }
    bool tomorrow = false;
    if (nextSlot < 0) {
        // After Isha: countdown to tomorrow's Fajr.
        const QDate tomorrowDate = m_day.addDays(1);
        const DayTimes tm = m_state->computeDay(tomorrowDate);
        if (tm.has[MN_FAJR]) {
            nextSlot = MN_FAJR;
            target = tm.times[MN_FAJR].msecsSinceStartOfDay() / 1000;
            tomorrow = true;
        }
    }

    if (nextSlot < 0) {
        m_heroName->setText(QStringLiteral("Unavailable"));
        m_heroArabic->setText(QStringLiteral("—"));
        m_heroTime->setText(QStringLiteral("--:--"));
        m_heroCountdown->setText(QStringLiteral("00:00:00"));
        m_heroSub->setText(
            QStringLiteral("No times for this date/location — check settings."));
        m_heroProgress->setValue(0);
        return;
    }

    const QString name = QString::fromUtf8(::mn_prayer_name_en(nextSlot));
    const QString ar = QString::fromUtf8(::mn_prayer_name_ar(nextSlot));
    m_heroName->setText(name);
    m_heroArabic->setText(ar);
    m_heroTime->setText(m_state->clockText(target));

    // Progress denominator: previous boundary of the same day (or midnight).
    int prev = 0;
    for (int i = MN_PRAYER_COUNT - 1; i >= 0; --i) {
        if (!day.has[i]) continue;
        const int t = day.times[i].msecsSinceStartOfDay() / 1000;
        if (t <= nowSecs) {
            prev = t;
            break;
        }
    }
    if (tomorrow) {
        // From today's Isha (or midnight fallback) to tomorrow's Fajr.
        for (int i = MN_PRAYER_COUNT - 1; i >= 0; --i) {
            if (!day.has[i]) continue;
            prev = day.times[i].msecsSinceStartOfDay() / 1000;
            break;
        }
        m_heroEyebrow->setText(QStringLiteral("TOMORROW"));
    } else {
        m_heroEyebrow->setText(QStringLiteral("NEXT PRAYER"));
    }

    const int span = tomorrow ? (86400 - prev + target) : (target - prev);
    const int done = tomorrow ? (nowSecs - prev + 86400) : (nowSecs - prev);
    m_heroProgress->setValue(span > 0 ? static_cast<int>(1000.0 * done / span) : 0);

    const int remaining = tomorrow ? (86400 - nowSecs + target) : (target - nowSecs);
    m_heroCountdown->setText(QStringLiteral("in %1").arg(countdownText(remaining)));
    m_heroSub->setText(m_state->cityLabel + QStringLiteral(" · ") +
                       QString::fromUtf8(::mn_method_short_name(m_state->methodId)));

    if (m_tray) {
        m_tray->setToolTip(QStringLiteral("MuslimNotify — %1 at %2 (in %3)")
                               .arg(name, m_state->clockText(target),
                                    countdownText(remaining)));
    }
}

void MainWindow::updateRows(const DayTimes &day, int nowSecs) {
    int nextSlot = -1;
    for (int i = 0; i < MN_PRAYER_COUNT; ++i) {
        if (i == MN_SUNRISE || !day.has[i]) continue;
        const int t = day.times[i].msecsSinceStartOfDay() / 1000;
        if (t > nowSecs) {
            nextSlot = i;
            break;
        }
    }
    int currentSlot = -1;
    for (int i = 0; i < MN_PRAYER_COUNT; ++i) {
        if (i == MN_SUNRISE || !day.has[i]) continue;
        const int t = day.times[i].msecsSinceStartOfDay() / 1000;
        if (t <= nowSecs) currentSlot = i;
    }

    for (int i = 0; i < m_rows.size(); ++i) {
        PrayerRow &row = m_rows[i];
        row.time->setText(day.has[i] ? m_state->clockText(day.times[i])
                                     : QStringLiteral("—"));
        const char *kind = "none";
        if (i == nextSlot) kind = "next";
        else if (i == currentSlot) kind = "now";
        row.badge->setText(strcmp(kind, "next") == 0 ? QStringLiteral("NEXT")
                          : strcmp(kind, "now") == 0 ? QStringLiteral("NOW")
                                                     : QString());
        row.badge->setProperty("kind", kind);
        row.badge->style()->unpolish(row.badge);
        row.badge->style()->polish(row.badge);

        const char *state = i == nextSlot ? "next" : i == currentSlot ? "current" : "idle";
        row.root->setProperty("state", state);
        row.root->style()->unpolish(row.root);
        row.root->style()->polish(row.root);
    }
}

void MainWindow::checkAlarm(const DayTimes &day, const QDate &date, int nowSecs) {
    if (!day.ok() || m_lastTick < 0) {
        return;
    }
    const int grace = 120;  // tolerate resume-from-sleep gaps
    for (int i = 0; i < MN_PRAYER_COUNT; ++i) {
        if (i == MN_SUNRISE) {
            // Sunrise is informational only; optional quiet notification.
            if (!day.has[i] || !m_state->notifPerPrayer[i]) continue;
            const int t = day.times[i].msecsSinceStartOfDay() / 1000;
            const QString key = QStringLiteral("pre:%1:%2").arg(date.toString(Qt::ISODate)).arg(i);
            const int pre = t - 60;
            if (m_lastTick < pre && pre <= nowSecs && nowSecs - pre <= grace &&
                m_lastPreKey != key) {
                m_lastPreKey = key;
                pushBanner(QStringLiteral("Sunrise in 1 minute — Fajr time is ending."));
                if (m_tray && m_state->notifMaster) {
                    m_tray->showMessage(QStringLiteral("MuslimNotify"),
                                        QStringLiteral("Sunrise in 1 minute."),
                                        QSystemTrayIcon::Information, 8000);
                }
            }
            continue;
        }
        if (!day.has[i]) continue;
        const int t = day.times[i].msecsSinceStartOfDay() / 1000;

        // Pre-alert.
        const int preMin = m_state->preAlertMinutes;
        if (preMin > 0 && m_state->notifMaster && m_state->notifPerPrayer[i]) {
            const int pre = t - preMin * 60;
            const QString pkey =
                QStringLiteral("pre:%1:%2").arg(date.toString(Qt::ISODate)).arg(i);
            if (m_lastTick < pre && pre <= nowSecs && nowSecs - pre <= grace &&
                m_lastPreKey != pkey) {
                m_lastPreKey = pkey;
                fireAlert(i, date, true);
            }
        }
        // Adhan moment.
        const QString key =
            QStringLiteral("adhan:%1:%2").arg(date.toString(Qt::ISODate)).arg(i);
        if (m_lastTick < t && t <= nowSecs && nowSecs - t <= grace &&
            m_lastFiredKey != key) {
            m_lastFiredKey = key;
            fireAlert(i, date, false);
        }
    }
}

void MainWindow::fireAlert(int slot, const QDate &date, bool preAlert) {
    const QString name = QString::fromUtf8(::mn_prayer_name_en(slot));
    const QString ar = QString::fromUtf8(::mn_prayer_name_ar(slot));
    const QString when = preAlert
                             ? QStringLiteral("in %1 minutes").arg(m_state->preAlertMinutes)
                             : QStringLiteral("now");
    const QString title = preAlert ? QStringLiteral("%1 %2").arg(name, when)
                                   : QStringLiteral("It is time for %1").arg(name);
    const QString body = QStringLiteral("%1 — %2, %3")
                             .arg(ar, m_state->cityLabel,
                                  QLocale().toString(QDate::currentDate(), QLocale::LongFormat));

    pushBanner(QStringLiteral("%1  ·  %2").arg(title, body));
    if (m_tray && m_state->notifMaster) {
        m_tray->showMessage(QStringLiteral("MuslimNotify"), title + QStringLiteral("\n") + body,
                            QSystemTrayIcon::Information, 12000);
    }
    if (!preAlert && m_state->notifPerPrayer[slot]) {
        QString err;
        const QString file = m_state->resolveAdhanFile(&err);
        if (!file.isEmpty()) {
            m_audio->play(file, &err);
        }
        if (!err.isEmpty()) {
            qWarning("Adhan playback: %s", qPrintable(err));
        }
    }
    // Gold flash on the hero card.
    m_heroCard->setProperty("firing", "1");
    m_heroCard->style()->unpolish(m_heroCard);
    m_heroCard->style()->polish(m_heroCard);
    QTimer::singleShot(30000, this, [this] {
        m_heroCard->setProperty("firing", "0");
        m_heroCard->style()->unpolish(m_heroCard);
        m_heroCard->style()->polish(m_heroCard);
    });
}

void MainWindow::pushBanner(const QString &text) {
    m_banner->setText(text);
    m_banner->show();
    qInfo() << "MuslimNotify:" << text;
}

void MainWindow::openSettings() {
    SettingsDialog dlg(m_state, this);
    connect(&dlg, &SettingsDialog::adhanTestRequested, this, &MainWindow::testAdhan);
    dlg.exec();
}

void MainWindow::showAbout() {
    const QString coreV = QString::fromUtf8(::mn_version());
    QMessageBox::about(
        this, QStringLiteral("About MuslimNotify"),
        QStringLiteral(
            "<h3>MuslimNotify %1</h3>"
            "<p>Open-source prayer times, Adhan playback and desktop alerts.</p>"
            "<p><b>Architecture:</b> Rust core (<code>mn-core</code> on the "
            "<code>adhan</code> crate) exposing a C ABI, linked into this Qt "
            "Widgets frontend. No cloud, no telemetry — everything computes "
            "locally on your machine.</p>"
            "<p><b>Core version:</b> %2</p>"
            "<p><b>Privacy:</b> your coordinates never leave this device. The "
            "optional IP-based location lookup and city search are the only "
            "network calls, and only when you press the button.</p>"
            "<p>MIT License · github.com/muslimnotify/muslimnotify</p>")
            .arg(QCoreApplication::applicationVersion(), coreV));
}

void MainWindow::testAdhan() {
    QString err;
    const QString file = m_state->resolveAdhanFile(&err);
    if (file.isEmpty()) {
        pushBanner(err);
        return;
    }
    if (!m_audio->play(file, &err)) {
        pushBanner(err);
    } else {
        pushBanner(QStringLiteral("Playing adhan via %1…").arg(m_audio->playerDescription()));
    }
}

void MainWindow::toggleVisible() {
    if (isVisible() && s_simulateSecs < 0) {
        setVisible(false);  // minimise to tray on normal runs
        return;
    }
    showNormal();
    raise();
    activateWindow();
}

void MainWindow::closeEvent(QCloseEvent *e) {
    // Minimise to tray instead of quitting when a tray exists.
    if (m_tray && s_simulateSecs < 0) {
        hide();
        e->ignore();
        return;
    }
    e->accept();
}
