// settingsdialog.cpp — settings UI implementation.
// SPDX-License-Identifier: MIT
#include "settingsdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QSpinBox>
#include <QTimeZone>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>

namespace {
struct CityPreset {
    const char *name;
    double lat, lng;
    const char *tz;  // IANA zone
};
// Famous cities, offline fallback so the app is usable without any network.
const CityPreset kPresets[] = {
    {"Makkah, Saudi Arabia", 21.4225, 39.8262, "Asia/Riyadh"},
    {"Madinah, Saudi Arabia", 24.4686, 39.6142, "Asia/Riyadh"},
    {"Jerusalem, Palestine", 31.7683, 35.2137, "Asia/Jerusalem"},
    {"Istanbul, Türkiye", 41.0082, 28.9784, "Europe/Istanbul"},
    {"Cairo, Egypt", 30.0444, 31.2357, "Africa/Cairo"},
    {"Dubai, UAE", 25.2048, 55.2708, "Asia/Dubai"},
    {"Doha, Qatar", 25.2854, 51.531, "Asia/Qatar"},
    {"Kuwait City, Kuwait", 29.3759, 47.9774, "Asia/Kuwait"},
    {"Amman, Jordan", 31.9454, 35.9284, "Asia/Amman"},
    {"Casablanca, Morocco", 33.5731, -7.5898, "Africa/Casablanca"},
    {"Jakarta, Indonesia", -6.2088, 106.8456, "Asia/Jakarta"},
    {"Kuala Lumpur, Malaysia", 3.139, 101.6869, "Asia/Kuala_Lumpur"},
    {"Karachi, Pakistan", 24.8607, 67.0011, "Asia/Karachi"},
    {"Delhi, India", 28.6139, 77.209, "Asia/Kolkata"},
    {"Dhaka, Bangladesh", 23.8103, 90.4125, "Asia/Dhaka"},
    {"London, United Kingdom", 51.5074, -0.1278, "Europe/London"},
    {"Paris, France", 48.8566, 2.3522, "Europe/Paris"},
    {"Berlin, Germany", 52.52, 13.405, "Europe/Berlin"},
    {"New York, USA", 40.7128, -74.006, "America/New_York"},
    {"Chicago, USA", 41.8781, -87.6298, "America/Chicago"},
    {"Houston, USA", 29.7604, -95.3698, "America/Chicago"},
    {"Los Angeles, USA", 34.0522, -118.2437, "America/Los_Angeles"},
    {"Toronto, Canada", 43.6532, -79.3832, "America/Toronto"},
    {"Sydney, Australia", -33.8688, 151.2093, "Australia/Sydney"},
};
constexpr int kPresetCount = sizeof(kPresets) / sizeof(kPresets[0]);
}  // namespace

SettingsDialog::SettingsDialog(AppState *state, QWidget *parent)
    : QDialog(parent), m_state(state) {
    setWindowTitle(QStringLiteral("MuslimNotify — Settings"));
    setMinimumWidth(460);
    buildUi();
    populateLocation();
    populateCalc();
    populateAudio();
    populateAlerts();
}

void SettingsDialog::buildUi() {
    auto *root = new QVBoxLayout(this);

    // ---- location ------------------------------------------------------------
    auto *locGroup = new QGroupBox(QStringLiteral("Location"));
    auto *locForm = new QFormLayout(locGroup);

    m_cityEdit = new QLineEdit();
    locForm->addRow(QStringLiteral("Place label:"), m_cityEdit);

    auto *coordRow = new QHBoxLayout();
    m_lat = new QDoubleSpinBox();
    m_lat->setRange(-90.0, 90.0);
    m_lat->setDecimals(4);
    m_lat->setSingleStep(0.01);
    m_lng = new QDoubleSpinBox();
    m_lng->setRange(-180.0, 180.0);
    m_lng->setDecimals(4);
    m_lng->setSingleStep(0.01);
    coordRow->addWidget(new QLabel(QStringLiteral("Lat")));
    coordRow->addWidget(m_lat);
    coordRow->addWidget(new QLabel(QStringLiteral("Lng")));
    coordRow->addWidget(m_lng);
    coordRow->addStretch(1);
    locForm->addRow(QStringLiteral("Coordinates:"), coordRow);

    m_presets = new QComboBox();
    for (const auto &p : kPresets) {
        m_presets->addItem(QString::fromUtf8(p.name));
    }
    connect(m_presets, &QComboBox::activated, this, [this](int idx) {
        const auto &p = kPresets[idx];
        applyDetected(p.lat, p.lng, QString::fromUtf8(p.name), QString::fromUtf8(p.tz));
        setStatus(QStringLiteral("Preset applied: %1").arg(p.name));
    });
    locForm->addRow(QStringLiteral("Famous cities:"), m_presets);

    auto *netRow = new QHBoxLayout();
    m_detectBtn = new QPushButton(QStringLiteral("Detect by IP"));
    m_detectBtn->setToolTip(
        QStringLiteral("One optional lookup at ip-api.com. Nothing is stored on any server."));
    connect(m_detectBtn, &QPushButton::clicked, this, &SettingsDialog::detectByIp);
    m_searchBtn = new QPushButton(QStringLiteral("Search city online"));
    m_searchBtn->setToolTip(
        QStringLiteral("Search Open-Meteo's free geocoder. Optional; the app works fully offline."));
    connect(m_searchBtn, &QPushButton::clicked, this, &SettingsDialog::searchCity);
    netRow->addWidget(m_detectBtn);
    netRow->addWidget(m_searchBtn);
    netRow->addStretch(1);
    locForm->addRow(QString(), netRow);

    m_autoDetect = new QCheckBox(QStringLiteral("Detect location automatically at startup (IP-based)"));
    locForm->addRow(QString(), m_autoDetect);

    root->addWidget(locGroup);

    // ---- calculation ------------------------------------------------------------
    auto *calcGroup = new QGroupBox(QStringLiteral("Calculation"));
    auto *calcForm = new QFormLayout(calcGroup);
    m_method = new QComboBox();
    calcForm->addRow(QStringLiteral("Method:"), m_method);
    m_madhab = new QComboBox();
    m_madhab->addItem(QStringLiteral("Shafi, Maliki, Hanbali & Ja'fari (standard)"), 0);
    m_madhab->addItem(QStringLiteral("Hanafi (later Asr)"), 1);
    calcForm->addRow(QStringLiteral("Asr juristic school:"), m_madhab);
    m_highLat = new QComboBox();
    calcForm->addRow(QStringLiteral("High-latitude rule:"), m_highLat);
    m_theme = new QComboBox();
    m_theme->addItem(QStringLiteral("Follow system"), 0);
    m_theme->addItem(QStringLiteral("Light"), 1);
    m_theme->addItem(QStringLiteral("Dark"), 2);
    calcForm->addRow(QStringLiteral("Theme:"), m_theme);
    m_24h = new QCheckBox(QStringLiteral("Use 24-hour clock"));
    calcForm->addRow(QString(), m_24h);
    root->addWidget(calcGroup);

    // ---- audio ------------------------------------------------------------------
    auto *audioGroup = new QGroupBox(QStringLiteral("Adhan audio"));
    auto *audioForm = new QFormLayout(audioGroup);
    m_adhan = new QComboBox();
    audioForm->addRow(QStringLiteral("Adhan:"), m_adhan);
    auto *audioBtnRow = new QHBoxLayout();
    m_browse = new QPushButton(QStringLiteral("Choose file…"));
    connect(m_browse, &QPushButton::clicked, this, &SettingsDialog::browseCustomAdhan);
    m_testBtn = new QPushButton(QStringLiteral("Test"));
    audioBtnRow->addWidget(m_browse);
    audioBtnRow->addWidget(m_testBtn);
    audioBtnRow->addStretch(1);
    audioForm->addRow(QString(), audioBtnRow);
    auto *audioHint = new QLabel(
        QStringLiteral("Bundled recordings are public-domain takes stored inside the app. "
                       "Playback uses your system player (mpv, ffplay, VLC, afplay…)."));
    audioHint->setWordWrap(true);
    audioForm->addRow(QString(), audioHint);
    root->addWidget(audioGroup);

    // ---- alerts -----------------------------------------------------------------
    auto *alertGroup = new QGroupBox(QStringLiteral("Alerts"));
    auto *alertForm = new QFormLayout(alertGroup);
    m_notifMaster = new QCheckBox(QStringLiteral("Desktop notifications (via system tray)"));
    alertForm->addRow(QString(), m_notifMaster);
    m_preAlert = new QSpinBox();
    m_preAlert->setRange(0, 30);
    m_preAlert->setSuffix(QStringLiteral(" min"));
    m_preAlert->setSpecialValueText(QStringLiteral("Off"));
    alertForm->addRow(QStringLiteral("Remind before prayer:"), m_preAlert);
    auto *perGrid = new QGridLayout();
    for (int i = 0; i < MN_PRAYER_COUNT; ++i) {
        m_perPrayer[i] = new QCheckBox(QString::fromUtf8(::mn_prayer_name_en(i)));
        if (i == MN_SUNRISE) {
            m_perPrayer[i]->setToolTip(QStringLiteral("Informational alert one minute before sunrise"));
        }
        perGrid->addWidget(m_perPrayer[i], i / 3, i % 3);
    }
    alertForm->addRow(QStringLiteral("Per prayer:"), perGrid);
    root->addWidget(alertGroup);

    // ---- status + buttons ----------------------------------------------------------
    m_status = new QLabel();
    m_status->setWordWrap(true);
    root->addWidget(m_status);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel |
                                         QDialogButtonBox::Apply);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::apply);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this,
            &SettingsDialog::apply);
    root->addWidget(buttons);
}

void SettingsDialog::populateLocation() {
    m_cityEdit->setText(m_state->cityLabel);
    m_lat->setValue(m_state->latitude);
    m_lng->setValue(m_state->longitude);
    m_autoDetect->setChecked(m_state->autoDetectOnStart);
}

void SettingsDialog::populateCalc() {
    for (int idx = 0; idx < ::mn_method_count(); ++idx) {
        const int id = ::mn_method_id(idx);
        m_method->addItem(QString::fromUtf8(::mn_method_display_name(id)), id);
    }
    const int mpos = m_method->findData(m_state->methodId);
    m_method->setCurrentIndex(mpos < 0 ? 0 : mpos);
    m_madhab->setCurrentIndex(m_state->madhabId == 1 ? 1 : 0);
    for (int id = 0; id < ::mn_high_lat_rule_count(); ++id) {
        m_highLat->addItem(QString::fromUtf8(::mn_high_lat_rule_name(id)), id);
    }
    const int hpos = m_highLat->findData(m_state->highLatId);
    m_highLat->setCurrentIndex(hpos < 0 ? 0 : hpos);
    m_theme->setCurrentIndex(m_state->theme);
    m_24h->setChecked(m_state->use24h);
}

void SettingsDialog::populateAudio() {
    const QStringList names = AppState::bundledAdhanNames();
    for (const QString &n : names) {
        m_adhan->addItem(n, m_adhan->count());
    }
    m_adhan->addItem(QStringLiteral("Custom file…"), -1);
    if (m_state->adhanIndex >= 0) {
        const int pos = m_adhan->findData(m_state->adhanIndex);
        m_adhan->setCurrentIndex(pos < 0 ? 0 : pos);
    } else {
        m_adhan->setCurrentIndex(m_adhan->count() - 1);
    }
    connect(m_testBtn, &QPushButton::clicked, this, [this] {
        apply();
        emit adhanTestRequested();
    });
}

void SettingsDialog::populateAlerts() {
    m_notifMaster->setChecked(m_state->notifMaster);
    m_preAlert->setValue(m_state->preAlertMinutes);
    for (int i = 0; i < MN_PRAYER_COUNT; ++i) {
        m_perPrayer[i]->setChecked(m_state->notifPerPrayer[i]);
    }
}

void SettingsDialog::setStatus(const QString &text, bool error) {
    m_status->setText(error ? QStringLiteral("⚠ %1").arg(text) : text);
    m_status->setStyleSheet(error ? QStringLiteral("color:#b0483a;") : QString());
}

void SettingsDialog::applyDetected(double lat, double lng, const QString &city,
                                   const QString &tzId) {
    m_lat->setValue(lat);
    m_lng->setValue(lng);
    if (!city.isEmpty()) m_cityEdit->setText(city);
    if (!tzId.isEmpty() && QTimeZone(tzId.toUtf8()).isValid()) {
        m_state->tzId = tzId;  // applied immediately; saved on Apply
    }
}

void SettingsDialog::detectByIp() {
    m_detectBtn->setEnabled(false);
    setStatus(QStringLiteral("Detecting location…"));
    QNetworkRequest req(QUrl(QStringLiteral(
        "http://ip-api.com/json/?fields=status,message,lat,lon,city,regionName,country,timezone")));
    req.setTransferTimeout(8000);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("MuslimNotify/1.0"));
    QNetworkReply *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        m_detectBtn->setEnabled(true);
        if (reply->error() != QNetworkReply::NoError) {
            setStatus(QStringLiteral("IP lookup failed: %1").arg(reply->errorString()), true);
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        if (obj.value("status").toString() != "success") {
            setStatus(QStringLiteral("IP lookup unavailable: %1")
                          .arg(obj.value("message").toString()), true);
            return;
        }
        const double lat = obj.value("lat").toDouble();
        const double lng = obj.value("lon").toDouble();
        QString city = obj.value("city").toString();
        const QString region = obj.value("regionName").toString();
        const QString country = obj.value("country").toString();
        if (!region.isEmpty()) city += QStringLiteral(", ") + region;
        if (!country.isEmpty()) city += QStringLiteral(", ") + country;
        applyDetected(lat, lng, city, obj.value("timezone").toString());
        setStatus(QStringLiteral("Detected: %1 (%2, %3)").arg(city).arg(lat).arg(lng));
    });
}

void SettingsDialog::searchCity() {
    const QString query = m_cityEdit->text().trimmed();
    if (query.isEmpty()) {
        setStatus(QStringLiteral("Type a city name in the place label first."), true);
        return;
    }
    m_searchBtn->setEnabled(false);
    setStatus(QStringLiteral("Searching…"));
    QUrl url(QStringLiteral("https://geocoding-api.open-meteo.com/v1/search"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("name"), query);
    q.addQueryItem(QStringLiteral("count"), QStringLiteral("1"));
    q.addQueryItem(QStringLiteral("language"), QStringLiteral("en"));
    url.setQuery(q);
    QNetworkRequest req(url);
    req.setTransferTimeout(8000);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("MuslimNotify/1.0"));
    QNetworkReply *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        m_searchBtn->setEnabled(true);
        if (reply->error() != QNetworkReply::NoError) {
            setStatus(QStringLiteral("City search failed: %1").arg(reply->errorString()), true);
            return;
        }
        const QJsonArray results = QJsonDocument::fromJson(reply->readAll())
                                       .object()
                                       .value("results")
                                       .toArray();
        if (results.isEmpty()) {
            setStatus(QStringLiteral("No city matched that name."), true);
            return;
        }
        const QJsonObject top = results.first().toObject();
        QStringList cityParts{top.value("name").toString(),
                              top.value("admin1").toString(),
                              top.value("country").toString()};
        cityParts.removeAll(QString());
        const QString city = cityParts.join(QStringLiteral(", "));
        applyDetected(top.value("latitude").toDouble(), top.value("longitude").toDouble(),
                      city, top.value("timezone").toString());
        setStatus(QStringLiteral("Found: %1 (%2, %3)")
                      .arg(city)
                      .arg(top.value("latitude").toDouble())
                      .arg(top.value("longitude").toDouble()));
    });
}

void SettingsDialog::browseCustomAdhan() {
    const QString file = QFileDialog::getOpenFileName(
        this, QStringLiteral("Choose adhan audio"), QString(),
        QStringLiteral("Audio (*.mp3 *.ogg *.wav *.flac *.m4a)"));
    if (file.isEmpty()) return;
    m_state->adhanIndex = -1;
    m_state->adhanCustomPath = file;
    m_state->save();
    m_adhan->setCurrentIndex(m_adhan->count() - 1);
    setStatus(QStringLiteral("Custom adhan set: %1").arg(file));
}

void SettingsDialog::apply() {
    m_state->cityLabel = m_cityEdit->text().trimmed().isEmpty()
                             ? QStringLiteral("%1, %2").arg(m_lat->value()).arg(m_lng->value())
                             : m_cityEdit->text().trimmed();
    m_state->latitude = m_lat->value();
    m_state->longitude = m_lng->value();
    m_state->methodId = m_method->currentData().toInt();
    m_state->madhabId = m_madhab->currentIndex() == 1 ? 1 : 0;
    m_state->highLatId = m_highLat->currentData().toInt();
    m_state->theme = m_theme->currentData().toInt();
    m_state->use24h = m_24h->isChecked();
    m_state->notifMaster = m_notifMaster->isChecked();
    m_state->preAlertMinutes = m_preAlert->value();
    m_state->autoDetectOnStart = m_autoDetect->isChecked();
    const QVariant adhanData = m_adhan->currentData();
    m_state->adhanIndex = adhanData.toInt();
    for (int i = 0; i < MN_PRAYER_COUNT; ++i) {
        m_state->notifPerPrayer[i] = m_perPrayer[i]->isChecked();
    }
    m_state->save();
    setStatus(QStringLiteral("Settings saved."));
}
