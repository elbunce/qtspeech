/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Speech module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/




#ifndef QTEXTTOSPEECH_H
#define QTEXTTOSPEECH_H

#include <QtTextToSpeech/qtexttospeech_global.h>
#include <QtCore/qobject.h>
#include <QtCore/qshareddata.h>
#include <QtCore/QSharedDataPointer>
#include <QtCore/qlocale.h>
#include <QtTextToSpeech/qvoice.h>

#include <QtQmlIntegration/qqmlintegration.h>

QT_BEGIN_NAMESPACE

class QTextToSpeechPrivate;
class Q_TEXTTOSPEECH_EXPORT QTextToSpeech : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString engine READ engine WRITE setEngine NOTIFY engineChanged)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(double rate READ rate WRITE setRate NOTIFY rateChanged)
    Q_PROPERTY(double pitch READ pitch WRITE setPitch NOTIFY pitchChanged)
    Q_PROPERTY(QLocale locale READ locale WRITE setLocale NOTIFY localeChanged)
    Q_PROPERTY(QVoice voice READ voice WRITE setVoice NOTIFY voiceChanged)
    Q_DECLARE_PRIVATE(QTextToSpeech)
public:
    enum State {
        Ready,
        Speaking,
        Paused,
        Error
    };
    Q_ENUM(State)

    enum class ErrorReason {
        NoError,
        Initialization,
        Configuration,
        Input,
        Playback
    };
    Q_ENUM(ErrorReason)

    enum class BoundaryHint {
        Default,
        Immediate,
        Word,
        Sentence
    };

    explicit QTextToSpeech(QObject *parent = nullptr);
    explicit QTextToSpeech(const QString &engine, QObject *parent = nullptr);
    explicit QTextToSpeech(const QString &engine, const QVariantMap &params,
                           QObject *parent = nullptr);
    ~QTextToSpeech() override;

    Q_INVOKABLE bool setEngine(const QString &engine, const QVariantMap &params = QVariantMap());
    QString engine() const;

    QTextToSpeech::State state() const;
    Q_INVOKABLE QTextToSpeech::ErrorReason errorReason() const;
    Q_INVOKABLE QString errorString() const;

    Q_INVOKABLE QList<QLocale> availableLocales() const;
    QLocale locale() const;

    QVoice voice() const;
    Q_INVOKABLE QList<QVoice> availableVoices() const;

    double rate() const;
    double pitch() const;
    double volume() const;

    Q_INVOKABLE static QStringList availableEngines();

public Q_SLOTS:
    void say(const QString &text);
    void stop(QTextToSpeech::BoundaryHint boundaryHint = QTextToSpeech::BoundaryHint::Default);
    void pause(QTextToSpeech::BoundaryHint boundaryHint = QTextToSpeech::BoundaryHint::Default);
    void resume();

    void setLocale(const QLocale &locale);

    void setRate(double rate);
    void setPitch(double pitch);
    void setVolume(double volume);
    void setVoice(const QVoice &voice);

Q_SIGNALS:
    void engineChanged(const QString &engine);
    void stateChanged(QTextToSpeech::State state);
    void errorOccurred(QTextToSpeech::ErrorReason error, const QString &errorString);
    void localeChanged(const QLocale &locale);
    void rateChanged(double rate);
    void pitchChanged(double pitch);
    void volumeChanged(double volume);
    void voiceChanged(const QVoice &voice);

private:
    Q_DISABLE_COPY(QTextToSpeech)
};

QT_END_NAMESPACE

#endif
