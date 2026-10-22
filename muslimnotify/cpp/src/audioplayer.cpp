// audioplayer.cpp — external-player adhan playback.
// SPDX-License-Identifier: MIT
#include "audioplayer.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QVector>

const QVector<AudioPlayer::Candidate> &AudioPlayer::candidates() {
    static const QVector<Candidate> list = {
        // mpv: precise, headless-friendly, exits when done.
        {"mpv", {"--no-video", "--really-quiet", "--no-terminal", "%1"}, "mpv"},
        // ffplay: ships with ffmpeg, exits when done.
        {"ffplay", {"-nodisp", "-autoexit", "-loglevel", "quiet", "%1"}, "ffplay"},
        // cvlc: VLC headless.
        {"cvlc", {"--play-and-exit", "--quiet", "%1"}, "VLC"},
        // macOS afplay.
        {"afplay", {"%1"}, "afplay"},
        // Windows / freedesktop: open with the default handler.
        {"xdg-open", {"%1"}, "default player"},
        // Last resort on Windows: default handler via explorer.
        {"explorer", {"%1"}, "Windows player"},
    };
    return list;
}

AudioPlayer::AudioPlayer(QObject *parent) : QObject(parent) {
    // Route process output to /dev/null semantics: keep the log clean.
}

AudioPlayer::~AudioPlayer() {
    stop();
}

bool AudioPlayer::hasPlayer() const {
    for (const Candidate &c : candidates()) {
        if (!QStandardPaths::findExecutable(QString::fromLatin1(c.exe)).isEmpty()) {
            return true;
        }
    }
    return false;
}

QString AudioPlayer::playerDescription() const {
    for (const Candidate &c : candidates()) {
        if (!QStandardPaths::findExecutable(QString::fromLatin1(c.exe)).isEmpty()) {
            return QString::fromLatin1(c.label);
        }
    }
    return QStringLiteral("none found");
}

bool AudioPlayer::play(const QString &path, QString *errorOut) {
    if (path.isEmpty() || !QFile::exists(path)) {
        const QString reason = QStringLiteral("Audio file missing: %1").arg(path);
        if (errorOut) *errorOut = reason;
        emit failed(reason);
        return false;
    }
    stop();

    const QString native = QDir::toNativeSeparators(path);
    for (const Candidate &c : candidates()) {
        const QString exe = QStandardPaths::findExecutable(QString::fromLatin1(c.exe));
        if (exe.isEmpty()) {
            continue;
        }
        QStringList args;
        for (const QString &a : c.args) {
            args << (a == QStringLiteral("%1") ? native : a);
        }
        auto *proc = new QProcess(this);
        proc->setProgram(exe);
        proc->setArguments(args);
        proc->setProcessChannelMode(QProcess::ProcessChannelMode::ForwardedErrorChannel);
        connect(proc, &QProcess::errorOccurred, this, [this, proc](QProcess::ProcessError) {
            const QString reason = QStringLiteral("Player '%1' failed").arg(m_activeLabel);
            emit failed(reason);
            proc->deleteLater();
            if (m_proc == proc) m_proc = nullptr;
        });
        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
                [this, proc](int, QProcess::ExitStatus) {
                    proc->deleteLater();
                    if (m_proc == proc) m_proc = nullptr;
                });
        proc->start();
        if (proc->waitForStarted(1500)) {
            m_proc = proc;
            m_activeLabel = QString::fromLatin1(c.label);
            emit started(m_activeLabel);
            return true;
        }
        proc->deleteLater();
    }
    const QString reason = QStringLiteral("No audio player found on this system");
    if (errorOut) *errorOut = reason;
    emit failed(reason);
    return false;
}

void AudioPlayer::stop() {
    if (m_proc) {
        m_proc->disconnect(this);
        m_proc->kill();
        m_proc->waitForFinished(800);
        m_proc->deleteLater();
        m_proc = nullptr;
        m_activeLabel.clear();
    }
}
