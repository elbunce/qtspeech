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

#ifndef QTEXTTOSPEECHENGINE_H
#define QTEXTTOSPEECHENGINE_H

#include <QtTextToSpeech/qtexttospeech.h>

#include <QtCore/QObject>
#include <QtCore/QLocale>
#include <QtCore/QDir>

QT_BEGIN_NAMESPACE

class Q_TEXTTOSPEECH_EXPORT QTextToSpeechEngine : public QObject
{
    Q_OBJECT

public:
    explicit QTextToSpeechEngine(QObject *parent = nullptr);
    ~QTextToSpeechEngine();

    virtual QList<QLocale> availableLocales() const = 0;
    virtual QList<QVoice> availableVoices() const = 0;

    virtual void say(const QString &text) = 0;
    virtual void stop(QTextToSpeech::BoundaryHint boundaryHint) = 0;
    virtual void pause(QTextToSpeech::BoundaryHint boundaryHint) = 0;
    virtual void resume() = 0;

    virtual double rate() const = 0;
    virtual bool setRate(double rate) = 0;
    virtual double pitch() const = 0;
    virtual bool setPitch(double pitch) = 0;
    virtual QLocale locale() const = 0;
    virtual bool setLocale(const QLocale &locale) = 0;
    virtual double volume() const = 0;
    virtual bool setVolume(double volume) = 0;
    virtual QVoice voice() const = 0;
    virtual bool setVoice(const QVoice &voice) = 0;
    virtual QTextToSpeech::State state() const = 0;
    virtual QTextToSpeech::ErrorReason errorReason() const = 0;
    virtual QString errorString() const = 0;

protected:
    static QVoice createVoice(const QString &name, const QLocale &locale, QVoice::Gender gender,
                              QVoice::Age age, const QVariant &data);
    static QVariant voiceData(const QVoice &voice);

Q_SIGNALS:
    void stateChanged(QTextToSpeech::State state);
    void errorOccurred(QTextToSpeech::ErrorReason error, const QString &errorString);
};

QT_END_NAMESPACE

#endif
