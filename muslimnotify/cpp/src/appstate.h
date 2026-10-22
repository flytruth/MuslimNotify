// appstate.h — Settings model + Rust-core engine wrapper.
// SPDX-License-Identifier: MIT
#pragma once

#include <QDate>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QTime>
#include <QTimeZone>

// C ABI of the Rust core.
extern "C" {
#include "muslimnotify.h"
}

// ---------------------------------------------------------------------------
// DayTimes — one civil day's six boundaries, ready for the UI.
// ---------------------------------------------------------------------------
struct DayTimes {
    QTime times[MN_PRAYER_COUNT];
    bool has[MN_PRAYER_COUNT];
    int status = MN_ERR_INVALID_DATE;

    bool ok() const { return status == MN_OK; }
    QTime at(int slot) const { return times[slot]; }
};

// ---------------------------------------------------------------------------
// AppState — persisted settings + derived engine calls.
// Owns a QSettings bound to the org/app identity.
// ---------------------------------------------------------------------------
class AppState : public QObject {
    Q_OBJECT
public:
    explicit AppState(QObject *parent = nullptr);

    // ---- persisted settings ------------------------------------------------
    double latitude = 21.4225;
    double longitude = 39.8262;
    QString cityLabel = "Makkah";
    QString tzId;                 // IANA id from IP lookup; empty = system zone
    int methodId = 3;             // Umm al-Qura by default (Makkah default city)
    int madhabId = 0;             // Shafi
    int highLatId = 0;            // Auto
    bool use24h = true;
    int theme = 0;                // 0 system, 1 light, 2 dark
    bool notifMaster = true;
    bool notifPerPrayer[MN_PRAYER_COUNT] = {true, false, true, true, true, true};
    int preAlertMinutes = 10;
    int adhanIndex = 0;           // index into bundled list, -1 = custom file
    QString adhanCustomPath;
    bool autoDetectOnStart = false;

    // ---- persistence ---------------------------------------------------------
    void load();
    void save();

    // ---- engine access ---------------------------------------------------------
    // UTC offset (seconds east) in effect on `date` under the configured zone.
    long long tzOffsetFor(const QDate &date) const;
    QString zoneDisplayName() const;

    DayTimes computeDay(const QDate &date) const;

    // Clock formatting honouring use24h ("05:02" / "5:02 AM").
    QString clockText(const QTime &t) const;
    QString clockText(long long secsSinceMidnight) const;

    // Bundled adhan choices (resource paths + display names).
    static QStringList bundledAdhanNames();
    static QStringList bundledAdhanResources();
    // Resolve the active adhan file to a playable local path, extracting the
    // bundled resource to app-data on first use.
    QString resolveAdhanFile(QString *errorOut) const;

signals:
    void changed();

private:
    QSettings m_settings;
};
