/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
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

#include <AVFoundation/AVFoundation.h>

#include "qtexttospeech_darwin.h"

#include <QtCore/QCoreApplication>

@interface QDarwinSpeechSynthesizerDelegate : NSObject <AVSpeechSynthesizerDelegate>
@end

@implementation QDarwinSpeechSynthesizerDelegate
{
    QTextToSpeechEngineDarwin *_engine;
}

- (instancetype)initWithTextToSpeechEngineDarwin:(QTextToSpeechEngineDarwin *)engine
{
    if ((self = [self init]))
        _engine = engine;
    return self;
}

- (void)speechSynthesizer:(AVSpeechSynthesizer *)synthesizer didCancelSpeechUtterance:(AVSpeechUtterance *)utterance
{
    Q_UNUSED(synthesizer);
    Q_UNUSED(utterance);
    if (_engine->ignoreNextUtterance)
        return;

    _engine->setState(QTextToSpeech::Ready);
}

- (void)speechSynthesizer:(AVSpeechSynthesizer *)synthesizer didContinueSpeechUtterance:(AVSpeechUtterance *)utterance
{
    Q_UNUSED(synthesizer);
    Q_UNUSED(utterance);
    if (_engine->ignoreNextUtterance)
        return;

    _engine->setState(QTextToSpeech::Speaking);
}

- (void)speechSynthesizer:(AVSpeechSynthesizer *)synthesizer didFinishSpeechUtterance:(AVSpeechUtterance *)utterance
{
    Q_UNUSED(synthesizer);
    Q_UNUSED(utterance);
    if (_engine->ignoreNextUtterance) {
        _engine->ignoreNextUtterance = false;
        return;
    }
    _engine->setState(QTextToSpeech::Ready);
}

- (void)speechSynthesizer:(AVSpeechSynthesizer *)synthesizer didPauseSpeechUtterance:(AVSpeechUtterance *)utterance
{
    Q_UNUSED(synthesizer);
    Q_UNUSED(utterance);
    _engine->setState(QTextToSpeech::Paused);
}

- (void)speechSynthesizer:(AVSpeechSynthesizer *)synthesizer didStartSpeechUtterance:(AVSpeechUtterance *)utterance
{
    Q_UNUSED(synthesizer);
    Q_UNUSED(utterance);
    if (_engine->ignoreNextUtterance)
        return;

    _engine->setState(QTextToSpeech::Speaking);
}

@end

// -------------------------------------------------------------------------

QT_BEGIN_NAMESPACE

QTextToSpeechEngineDarwin::QTextToSpeechEngineDarwin(const QVariantMap &/*parameters*/, QObject *parent)
    : QTextToSpeechEngine(parent)
    , m_speechSynthesizer([AVSpeechSynthesizer new])
{
    m_speechSynthesizer.delegate = [[QDarwinSpeechSynthesizerDelegate alloc] initWithTextToSpeechEngineDarwin:this];
    if (setLocale(QLocale()) || setLocale(QLocale().language())) {
        m_state = QTextToSpeech::Ready;
        m_errorReason = QTextToSpeech::ErrorReason::NoError;
    } else {
        setError(QTextToSpeech::ErrorReason::Configuration,
                 QCoreApplication::translate("QTextToSpeech", "Failed to initialize default locale and voice."));
    }
}

QTextToSpeechEngineDarwin::~QTextToSpeechEngineDarwin()
{
    [m_speechSynthesizer.delegate autorelease];
    [m_speechSynthesizer release];
}

void QTextToSpeechEngineDarwin::say(const QString &text)
{
    stop(QTextToSpeech::BoundaryHint::Default);

    // Qt pitch: [-1.0, 1.0], 0 is normal
    // AVF range: [0.5, 2.0], 1.0 is normal
    const double desiredPitch = 1.0 + (m_pitch >= 0 ? m_pitch : (m_pitch * 0.5));

    // As the name suggests, pitchMultiplier accumulates with each utterance, so when speaking
    // multiple times with the same != 1.0 pitch, the pitch goes higher or lower with each step!
    // So we keep track of the actual pitch after the last utterance, and multiply the target
    // pitch with that value to compensate.
    // With the compensation, we might now have a pitch multipler outside of the AVF range, e.g.
    // to get from 2.0 to 0.5 we need a pitch multiplier of 1/4th. Sadly, the API blocks values
    // lower than 0.5, but does allow values larger than 2.0. So we need to play a silent
    // utterance (the string can't be empty) with a pitchMultiplier of 0.5 first!
    if (desiredPitch / m_actualPitch < 0.5) {
        ignoreNextUtterance = true; // signal delegate to ignore the next one
        NSString *empty = @" ";
        AVSpeechUtterance *correctionUtterance = [AVSpeechUtterance speechUtteranceWithString:empty];
        correctionUtterance.volume = 0;
        correctionUtterance.rate = AVSpeechUtteranceMaximumSpeechRate;
        correctionUtterance.voice = fromQVoice(m_voice);
        correctionUtterance.pitchMultiplier = 0.5;
        m_actualPitch *= 0.5;
        [m_speechSynthesizer speakUtterance:correctionUtterance];
    }
    AVSpeechUtterance *utterance = [AVSpeechUtterance speechUtteranceWithString:text.toNSString()];
    utterance.pitchMultiplier = desiredPitch / m_actualPitch;
    m_actualPitch *= utterance.pitchMultiplier;

    // Qt range: [-1.0, 1.0], 0 is normal
    // AVF range: [AVSpeechUtteranceMinimumSpeechRate, AVSpeechUtteranceMaximumSpeechRate],
    //             AVSpeechUtteranceDefaultSpeechRate is normal
    // The QtTextToSpeech documentation states that a rate of 0.0 represents normal speech flow.
    // To map that to AVSpeechUtteranceDefaultSpeechRate while at the same time preserve the Qt
    // range [-1.0, 1.0], we choose to operate with two differente rate convertions; one for
    // values in the range [-1, 0), and for [0, 1].
    const float range = m_rate >= 0
                      ? AVSpeechUtteranceMaximumSpeechRate - AVSpeechUtteranceDefaultSpeechRate
                      : AVSpeechUtteranceDefaultSpeechRate - AVSpeechUtteranceMinimumSpeechRate;
    utterance.rate = AVSpeechUtteranceDefaultSpeechRate + (m_rate * range);

    utterance.volume = m_volume;
    utterance.voice = fromQVoice(m_voice);

    [m_speechSynthesizer speakUtterance:utterance];
}

void QTextToSpeechEngineDarwin::stop(QTextToSpeech::BoundaryHint boundaryHint)
{
    Q_UNUSED(boundaryHint);
    const AVSpeechBoundary atBoundary = (boundaryHint == QTextToSpeech::BoundaryHint::Immediate
                                      || boundaryHint == QTextToSpeech::BoundaryHint::Default)
                                      ? AVSpeechBoundaryImmediate
                                      : AVSpeechBoundaryWord;
    [m_speechSynthesizer stopSpeakingAtBoundary:atBoundary];
}

void QTextToSpeechEngineDarwin::pause(QTextToSpeech::BoundaryHint boundaryHint)
{
    const AVSpeechBoundary atBoundary = boundaryHint == QTextToSpeech::BoundaryHint::Immediate
                                      ? AVSpeechBoundaryImmediate
                                      : AVSpeechBoundaryWord;
    [m_speechSynthesizer pauseSpeakingAtBoundary:atBoundary];
}

void QTextToSpeechEngineDarwin::resume()
{
    [m_speechSynthesizer continueSpeaking];
}

bool QTextToSpeechEngineDarwin::setRate(double rate)
{
    m_rate = rate;
    return true;
}

double QTextToSpeechEngineDarwin::rate() const
{
    return m_rate;
}

bool QTextToSpeechEngineDarwin::setPitch(double pitch)
{
    m_pitch = pitch;
    return true;
}

double QTextToSpeechEngineDarwin::pitch() const
{
    return m_pitch;
}

bool QTextToSpeechEngineDarwin::setVolume(double volume)
{
    m_volume = volume;
    return true;
}

double QTextToSpeechEngineDarwin::volume() const
{
    return m_volume;
}

QList<QLocale> QTextToSpeechEngineDarwin::availableLocales() const
{
    QSet<QLocale> locales;
    for (AVSpeechSynthesisVoice *voice in [AVSpeechSynthesisVoice speechVoices]) {
        QString language = QString::fromNSString(voice.language);
        locales << QLocale(language);
    }

    return locales.values();
}

bool QTextToSpeechEngineDarwin::setLocale(const QLocale &locale)
{
    AVSpeechSynthesisVoice *defaultAvVoice = [AVSpeechSynthesisVoice voiceWithLanguage:locale.bcp47Name().toNSString()];

    if (!defaultAvVoice) {
        setError(QTextToSpeech::ErrorReason::Configuration,
                 QCoreApplication::translate("QTextToSpeech", "No voice available for locale %1.")
                    .arg(locale.bcp47Name()));
        return false;
    }

    m_voice = toQVoice(defaultAvVoice);
    // workaround for AVFoundation bug: the default voice doesn't have the gender flag set, but
    // the same voice we get via identifier (or from the availableVoices list) does.
    if (m_voice.gender() == QVoice::Unknown) {
        const QString identifier = voiceData(m_voice).toString();
        AVSpeechSynthesisVoice *defaultAvVoice = [AVSpeechSynthesisVoice voiceWithIdentifier:identifier.toNSString()];
        m_voice = toQVoice(defaultAvVoice);
    }
    return true;
}

QLocale QTextToSpeechEngineDarwin::locale() const
{
    return m_voice.locale();
}

QList<QVoice> QTextToSpeechEngineDarwin::availableVoices() const
{
    QList<QVoice> voices;

    for (AVSpeechSynthesisVoice *avVoice in [AVSpeechSynthesisVoice speechVoices]) {
        const QLocale voiceLocale(QString::fromNSString(avVoice.language));
        if (m_voice.locale() == voiceLocale)
            voices << toQVoice(avVoice);
    }

    return voices;
}

bool QTextToSpeechEngineDarwin::setVoice(const QVoice &voice)
{
    AVSpeechSynthesisVoice *avVoice = fromQVoice(voice);
    if (!avVoice) {
        setError(QTextToSpeech::ErrorReason::Configuration,
                 QCoreApplication::translate("QTextToSpeech", "Voice is not available with this engine."));
        return false;
    }

    m_voice = voice;
    return true;
}

QVoice QTextToSpeechEngineDarwin::voice() const
{
    return m_voice;
}

AVSpeechSynthesisVoice *QTextToSpeechEngineDarwin::fromQVoice(const QVoice &voice) const
{
    const QString identifier = voiceData(voice).toString();
    AVSpeechSynthesisVoice *avVoice = [AVSpeechSynthesisVoice voiceWithIdentifier:identifier.toNSString()];
    return avVoice;
}

QVoice QTextToSpeechEngineDarwin::toQVoice(AVSpeechSynthesisVoice *avVoice) const
{
    // only from macOS 10.15 and iOS 13 on
    const QVoice::Gender gender = [avVoice]{
        if (@available(macos 10.15, ios 13, *)) {
            switch (avVoice.gender) {
            case AVSpeechSynthesisVoiceGenderMale:
                return QVoice::Male;
            case AVSpeechSynthesisVoiceGenderFemale:
                return QVoice::Female;
            default:
                break;
            };
        }
        return QVoice::Unknown;
    }();

    return createVoice(QString::fromNSString(avVoice.name),
                       QLocale(QString::fromNSString(avVoice.language)),
                       gender, QVoice::Other, QString::fromNSString(avVoice.identifier));
}

void QTextToSpeechEngineDarwin::setState(QTextToSpeech::State state)
{
    if (m_state == state)
        return;

    m_state = state;
    emit stateChanged(m_state);
}

QTextToSpeech::State QTextToSpeechEngineDarwin::state() const
{
    return m_state;
}

void QTextToSpeechEngineDarwin::setError(QTextToSpeech::ErrorReason reason, const QString &string)
{
    m_errorReason = reason;
    m_errorString = string;
    if (reason != QTextToSpeech::ErrorReason::NoError)
        return;
    if (m_state != QTextToSpeech::Error) {
        m_state = QTextToSpeech::Error;
        emit stateChanged(m_state);
    }
    emit errorOccurred(m_errorReason, m_errorString);
}

QTextToSpeech::ErrorReason QTextToSpeechEngineDarwin::errorReason() const
{
    return m_errorReason;
}

QString QTextToSpeechEngineDarwin::errorString() const
{
    return m_errorString;
}

QT_END_NAMESPACE
