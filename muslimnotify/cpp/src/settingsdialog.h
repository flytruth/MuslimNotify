// settingsdialog.h — location, calculation, audio and alert settings.
// SPDX-License-Identifier: MIT
#pragma once

#include <QDialog>
#include <QNetworkAccessManager>

#include "appstate.h"

class QComboBox;
class QDoubleSpinBox;
class QCheckBox;
class QSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(AppState *state, QWidget *parent = nullptr);

signals:
    void adhanTestRequested();

private slots:
    void detectByIp();
    void searchCity();
    void browseCustomAdhan();
    void apply();

private:
    void buildUi();
    void populateLocation();
    void populateCalc();
    void populateAudio();
    void populateAlerts();
    void setStatus(const QString &text, bool error = false);
    void applyDetected(double lat, double lng, const QString &city, const QString &tzId);

    AppState *m_state;
    QNetworkAccessManager m_nam;

    // location
    QLabel *m_status = nullptr;
    QLineEdit *m_cityEdit = nullptr;
    QDoubleSpinBox *m_lat = nullptr;
    QDoubleSpinBox *m_lng = nullptr;
    QComboBox *m_presets = nullptr;
    QCheckBox *m_autoDetect = nullptr;
    QPushButton *m_detectBtn = nullptr;
    QPushButton *m_searchBtn = nullptr;

    // calculation
    QComboBox *m_method = nullptr;
    QComboBox *m_madhab = nullptr;
    QComboBox *m_highLat = nullptr;
    QCheckBox *m_24h = nullptr;
    QComboBox *m_theme = nullptr;

    // audio
    QComboBox *m_adhan = nullptr;
    QPushButton *m_browse = nullptr;
    QPushButton *m_testBtn = nullptr;

    // alerts
    QCheckBox *m_notifMaster = nullptr;
    QSpinBox *m_preAlert = nullptr;
    QCheckBox *m_perPrayer[MN_PRAYER_COUNT] = {nullptr, nullptr, nullptr, nullptr,
                                               nullptr, nullptr};
};
