// audioplayer.h — external-player adhan playback.
// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

// Plays an audio file by delegating to whatever media player the platform
// provides. Keeps MuslimNotify dependency-light (no QtMultimedia) and lets
// each OS pick its best backend. Order matters: first found wins.
class AudioPlayer : public QObject {
    Q_OBJECT
public:
    explicit AudioPlayer(QObject *parent = nullptr);
    ~AudioPlayer() override;

    // Start playing `path`. Returns false with a reason in `errorOut` when no
    // player exists or the file is missing. Playing again stops the previous
    // playback first.
    bool play(const QString &path, QString *errorOut = nullptr);
    void stop();

    // True when at least one known player binary exists on PATH.
    bool hasPlayer() const;
    QString playerDescription() const;

signals:
    void started(const QString &player);
    void failed(const QString &reason);

private:
    struct Candidate {
        const char *exe;
        QStringList args;   // %1 = file path placeholder
        const char *label;
    };
    static const QVector<Candidate> &candidates();

    QProcess *m_proc = nullptr;
    QString m_activeLabel;
};
