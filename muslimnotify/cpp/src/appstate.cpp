// appstate.cpp — settings persistence + Rust engine bridge.
// SPDX-License-Identifier: MIT
#include "appstate.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

AppState::AppState(QObject *parent)
    : QObject(parent),
      m_settings(QSettings::defaultFormat(), QSettings::UserScope,
                 QStringLiteral("MuslimNotify"), QStringLiteral("MuslimNotify")) {}

void AppState::load() {
    latitude = m_settings.value("location/lat", latitude).toDouble();
    longitude = m_settings.value("location/lng", longitude).toDouble();
    // QSettings turns INI values containing commas into QStringList variants,
    // so "Makkah, Saudi Arabia" would read back as an empty string via
    // toString(). Rejoin list-shaped values explicitly.
    const QVariant labelVar = m_settings.value("location/label", cityLabel);
    cityLabel = labelVar.typeId() == QMetaType::QStringList
                    ? labelVar.toStringList().join(QStringLiteral(", "))
                    : labelVar.toString();
    tzId = m_settings.value("location/tzId", tzId).toString();
    methodId = m_settings.value("calc/method", methodId).toInt();
    madhabId = m_settings.value("calc/madhab", madhabId).toInt();
    highLatId = m_settings.value("calc/highLat", highLatId).toInt();
    use24h = m_settings.value("display/use24h", use24h).toBool();
    theme = m_settings.value("display/theme", theme).toInt();
    notifMaster = m_settings.value("alerts/master", notifMaster).toBool();
    preAlertMinutes = m_settings.value("alerts/preMinutes", preAlertMinutes).toInt();
    adhanIndex = m_settings.value("audio/index", adhanIndex).toInt();
    adhanCustomPath = m_settings.value("audio/customPath", adhanCustomPath).toString();
    autoDetectOnStart = m_settings.value("location/autoDetect", autoDetectOnStart).toBool();
    for (int i = 0; i < MN_PRAYER_COUNT; ++i) {
        notifPerPrayer[i] =
            m_settings.value(QString("alerts/p%1").arg(i), notifPerPrayer[i]).toBool();
    }
    qInfo("AppState::load -> label='%s' tz='%s' method=%d status=%s",
          qPrintable(cityLabel), qPrintable(tzId), methodId,
          m_settings.status() == QSettings::NoError ? "ok" : "ERROR");
}

void AppState::save() {
    m_settings.setValue("location/lat", latitude);
    m_settings.setValue("location/lng", longitude);
    m_settings.setValue("location/label", cityLabel);
    m_settings.setValue("location/tzId", tzId);
    m_settings.setValue("calc/method", methodId);
    m_settings.setValue("calc/madhab", madhabId);
    m_settings.setValue("calc/highLat", highLatId);
    m_settings.setValue("display/use24h", use24h);
    m_settings.setValue("display/theme", theme);
    m_settings.setValue("alerts/master", notifMaster);
    m_settings.setValue("alerts/preMinutes", preAlertMinutes);
    m_settings.setValue("audio/index", adhanIndex);
    m_settings.setValue("audio/customPath", adhanCustomPath);
    m_settings.setValue("location/autoDetect", autoDetectOnStart);
    for (int i = 0; i < MN_PRAYER_COUNT; ++i) {
        m_settings.setValue(QString("alerts/p%1").arg(i), notifPerPrayer[i]);
    }
    m_settings.sync();
    emit changed();
}

long long AppState::tzOffsetFor(const QDate &date) const {
    QTimeZone zone;
    if (!tzId.isEmpty()) {
        zone = QTimeZone(tzId.toUtf8());
    }
    if (!zone.isValid()) {
        zone = QTimeZone::systemTimeZone();
    }
    // Evaluate the offset at local noon so DST cut-offs at midnight cannot
    // poison the whole day.
    const QDateTime noon(date, QTime(12, 0), zone);
    return zone.offsetFromUtc(noon);
}

QString AppState::zoneDisplayName() const {
    QTimeZone zone;
    if (!tzId.isEmpty()) {
        zone = QTimeZone(tzId.toUtf8());
    }
    if (!zone.isValid()) {
        zone = QTimeZone::systemTimeZone();
    }
    const long long off = zone.offsetFromUtc(QDateTime::currentDateTime());
    const int h = static_cast<int>(off / 3600);
    const int m = static_cast<int>((qAbs(off) % 3600) / 60);
    return QStringLiteral("UTC%1%2:%3")
        .arg(off >= 0 ? "+" : "-")
        .arg(qAbs(h), 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'));
}

DayTimes AppState::computeDay(const QDate &date) const {
    DayTimes out;
    if (!date.isValid()) {
        out.status = MN_ERR_INVALID_DATE;
        return out;
    }
    long long secs[MN_PRAYER_COUNT] = {0, 0, 0, 0, 0, 0};
    const int rc = ::mn_compute_times(
        date.year(), date.month(), date.day(), tzOffsetFor(date), latitude, longitude,
        methodId, madhabId, highLatId, secs);
    out.status = rc;
    for (int i = 0; i < MN_PRAYER_COUNT; ++i) {
        out.has[i] = (rc == MN_OK && secs[i] != MN_NO_TIME);
        if (out.has[i]) {
            const long long s = secs[i];
            out.times[i] = QTime(static_cast<int>(s / 3600),
                                 static_cast<int>((s % 3600) / 60),
                                 static_cast<int>(s % 60));
        } else {
            out.times[i] = QTime();
        }
    }
    return out;
}

QString AppState::clockText(const QTime &t) const {
    return t.toString(use24h ? QStringLiteral("HH:mm") : QStringLiteral("h:mm AP"));
}

QString AppState::clockText(long long secsSinceMidnight) const {
    const QTime t(static_cast<int>(secsSinceMidnight / 3600) % 24,
                  static_cast<int>((secsSinceMidnight % 3600) / 60), 0);
    return clockText(t);
}

QStringList AppState::bundledAdhanNames() {
    return {QStringLiteral("Adhan — full call (Aaqib Azeez)"),
            QStringLiteral("Adhan — Abdulfattah style"),
            QStringLiteral("Adhan — short call")};
}

QStringList AppState::bundledAdhanResources() {
    return {QStringLiteral(":/adhan/adhan_mishary_style.mp3"),
            QStringLiteral(":/adhan/adhan_abdulfattah.mp3"),
            QStringLiteral(":/adhan/adhan_lms.mp3")};
}

QString AppState::resolveAdhanFile(QString *errorOut) const {
    if (adhanIndex >= 0 && adhanIndex < bundledAdhanResources().size()) {
        const QString resource = bundledAdhanResources().at(adhanIndex);
        // Extract bundled resources to a stable app-data path so any external
        // player process can read them.
        const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir dir(base);
        if (!dir.exists()) {
            QDir().mkpath(base);
        }
        const QString target = dir.filePath(QFileInfo(resource).fileName());
        if (!QFile::exists(target)) {
            QFile res(resource);
            if (!res.open(QIODevice::ReadOnly)) {
                if (errorOut) *errorOut = QStringLiteral("Cannot read bundled resource %1").arg(resource);
                return QString();
            }
            QFile out(target);
            if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                if (errorOut) *errorOut = QStringLiteral("Cannot write %1").arg(target);
                return QString();
            }
            out.write(res.readAll());
        }
        return target;
    }
    if (!adhanCustomPath.isEmpty() && QFile::exists(adhanCustomPath)) {
        return adhanCustomPath;
    }
    if (errorOut) {
        *errorOut = QStringLiteral("Custom adhan file not found: %1").arg(adhanCustomPath);
    }
    return QString();
}
