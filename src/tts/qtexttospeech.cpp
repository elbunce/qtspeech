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



#include "qtexttospeech.h"
#include "qtexttospeech_p.h"

#include <qdebug.h>

#include <QtCore/private/qfactoryloader_p.h>

QT_BEGIN_NAMESPACE

Q_GLOBAL_STATIC_WITH_ARGS(QFactoryLoader, loader,
        ("org.qt-project.qt.speech.tts.plugin/6.0",
         QLatin1String("/texttospeech")))

QMutex QTextToSpeechPrivate::m_mutex;

QTextToSpeechPrivate::QTextToSpeechPrivate(QTextToSpeech *speech)
    : q_ptr(speech)
{
    qRegisterMetaType<QTextToSpeech::State>();
    qRegisterMetaType<QTextToSpeech::ErrorReason>();
}

QTextToSpeechPrivate::~QTextToSpeechPrivate()
{
    delete m_engine;
}

void QTextToSpeechPrivate::setEngineProvider(const QString &engine, const QVariantMap &params)
{
    Q_Q(QTextToSpeech);

    q->stop(QTextToSpeech::BoundaryHint::Immediate);
    delete m_engine;

    m_providerName = engine;
    if (m_providerName.isEmpty()) {
        const auto plugins = QTextToSpeechPrivate::plugins();
        int priority = -1;
        for (const auto &&[provider, metadata] : plugins.asKeyValueRange()) {
            const int pluginPriority = metadata.value(QStringLiteral("Priority")).toInteger();
            if (pluginPriority > priority) {
                priority = pluginPriority;
                m_providerName = provider;
            }
        }
        if (m_providerName.isEmpty()) {
            qCritical() << "No text-to-speech plug-ins were found.";
            return;
        }
    }
    if (!loadMeta()) {
        qCritical() << "Text-to-speech plug-in" << m_providerName << "is not supported.";
        return;
    }
    loadPlugin();
    if (m_plugin) {
        QString errorString;
        m_engine = m_plugin->createTextToSpeechEngine(params, nullptr, &errorString);
        if (!m_engine) {
            qCritical() << "Error creating text-to-speech engine" << m_providerName
                        << (errorString.isEmpty() ? QStringLiteral("") : (QStringLiteral(": ") + errorString));
        }
    } else {
        qCritical() << "Error loading text-to-speech plug-in" << m_providerName;
    }

    // Connect state and error change signals directly from the engine to the public API signals
    if (m_engine) {
        QObject::connect(m_engine, &QTextToSpeechEngine::stateChanged, q, &QTextToSpeech::stateChanged);
        QObject::connect(m_engine, &QTextToSpeechEngine::errorOccurred, q, &QTextToSpeech::errorOccurred);
    }
}

bool QTextToSpeechPrivate::loadMeta()
{
    m_plugin = nullptr;
    m_metaData = QCborMap();

    QList<QCborMap> candidates = QTextToSpeechPrivate::plugins().values(m_providerName);

    int versionFound = -1;

    // figure out which version of the plugin we want
    for (int i = 0; i < candidates.size(); ++i) {
        QCborMap meta = candidates[i];
        if (int ver = meta.value(QLatin1String("Version")).toInteger(); ver > versionFound) {
            versionFound = ver;
            m_metaData = std::move(meta);
        }
    }

    if (m_metaData.isEmpty()) {
        m_metaData.insert(QLatin1String("index"), -1); // not found
        return false;
    }

    return true;
}

void QTextToSpeechPrivate::loadPlugin()
{
    int idx = m_metaData.value(QLatin1String("index")).toInteger();
    if (idx < 0) {
        m_plugin = nullptr;
        return;
    }
    m_plugin = qobject_cast<QTextToSpeechPlugin *>(loader()->instance(idx));
}

QMultiHash<QString, QCborMap> QTextToSpeechPrivate::plugins(bool reload)
{
    static QMultiHash<QString, QCborMap> plugins;
    static bool alreadyDiscovered = false;
    QMutexLocker lock(&m_mutex);

    if (reload == true)
        alreadyDiscovered = false;

    if (!alreadyDiscovered) {
        loadPluginMetadata(plugins);
        alreadyDiscovered = true;
    }
    return plugins;
}

void QTextToSpeechPrivate::loadPluginMetadata(QMultiHash<QString, QCborMap> &list)
{
    QFactoryLoader *l = loader();
    QList<QPluginParsedMetaData> meta = l->metaData();
    for (int i = 0; i < meta.size(); ++i) {
        QCborMap obj = meta.at(i).value(QtPluginMetaDataKeys::MetaData).toMap();
        obj.insert(QLatin1String("index"), i);
        list.insert(obj.value(QLatin1String("Provider")).toString(), obj);
    }
}

/*!
    \class QTextToSpeech
    \brief The QTextToSpeech class provides a convenient access to text-to-speech engines.
    \inmodule QtTextToSpeech

    Use \l say() to start synthesizing text, and \l stop(), \l pause(), and \l resume()
    to control the reading of the text.

    It is possible to specify the language with \l setLocale(). To set a voice, get the
    list of \l availableVoices() and set the desired voice using \l setVoice(). The list
    of available voices depends on the active locale on most platforms.

    The languages and voices depend on the available synthesizers on each platform.
    On Linux, \c speech-dispatcher is used by default.
*/

/*!
    \qmltype TextToSpeech
    \inqmlmodule QtTextToSpeech
    \ingroup texttospeech_qml
    \brief The TextToSpeech type provides access to text-to-speech engines.
*/

/*!
    \enum QTextToSpeech::State

    \brief This enum describes the current state of the text-to-speech engine.

    \value Ready      The synthesizer is ready to start a new text. This is
                      also the state after a text was finished.
    \value Speaking   Text is being spoken.
    \value Paused     The synthesis was paused and can be resumed with \l resume().
    \value Error      An error has occurred. Details are given by \l errorReason().

    \sa QTextToSpeech::ErrorReason errorReason() errorString()
*/

/*!
    \enum QTextToSpeech::ErrorReason

    \brief This enum describes the current error, if any, of the QTextToSpeech engine.

    \value NoError          No error has occurred.
    \value Initialization   The backend could not be initialized, e.g. due to
                            a missing driver or operating system requirement.
    \value Configuration    The given backend configuration is inconsistent, e.g.
                            due to wrong voice name or parameters.
    \value Input            The given text could not be synthesized, e.g. due to
                            invalid size or characters.
    \value Playback         Audio playback failed e.g. due to missing audio device,
                            wrong format or audio streaming interruption.

    Use \l errorReason() to obtain the current error and \l errorString() to get the
    related error message.

    \sa errorOccurred()
*/

/*!
    \enum QTextToSpeech::BoundaryHint

    \brief describes when speech should be stopped and paused.

    \value Default          Uses the engine specific default behavior.
    \value Immediate        The engine should stop playback immediately.
    \value Word             Stop speech when the current word is finished.
    \value Sentence         Stop speech when the current sentence is finished.

    \note These are hints to the engine. The current engine might not support
    all options.
*/

/*!
    Loads a text-to-speech engine from a plug-in that uses the default
    engine plug-in and constructs a QTextToSpeech object as the child
    of \a parent.

    The default engine is platform-specific.

    If the engine initializes correctly, then the \l state of the engine will
    change to QTextToSpeech::Ready; note that this might happen asynchronously.
    If the plugin fails to load, then \l state will be set to QTextToSpeech::Error.

    \sa availableEngines()
*/
QTextToSpeech::QTextToSpeech(QObject *parent)
    : QTextToSpeech(QString(), QVariantMap(), parent)
{
}

/*!
    Loads a text-to-speech engine from a plug-in that matches parameter \a engine and
    constructs a QTextToSpeech object as the child of \a parent.

    If \a engine is empty, the default engine plug-in is used. The default
    engine is platform-specific.

    If the engine initializes correctly, the \l state of the engine will be set
    to QTextToSpeech::Ready. If the plugin fails to load, or if the engine fails to
    initialize, the engine's \l state will be set to QTextToSpeech::Error.

    \sa availableEngines()
*/
QTextToSpeech::QTextToSpeech(const QString &engine, QObject *parent)
    : QTextToSpeech(engine, QVariantMap(), parent)
{
}

/*!
    \since 6.4

    Loads a text-to-speech engine from a plug-in that matches parameter \a engine and
    constructs a QTextToSpeech object as the child of \a parent, passing \a params
    through to the engine.

    If \a engine is empty, the default engine plug-in is used. The default
    engine is platform-specific. Which key/value pairs in \a params are supported
    depends on the engine. See \l{Qt TextToSpeech Engines}{the engine documentation}
    for details. Unsupported entries will be ignored.

    If the engine initializes correctly, the \l state of the engine will be set
    to QTextToSpeech::Ready. If the plugin fails to load, or if the engine fails to
    initialize, the engine's \l state will be set to QTextToSpeech::Error.

    \sa availableEngines()
*/
QTextToSpeech::QTextToSpeech(const QString &engine, const QVariantMap &params, QObject *parent)
    : QObject(*new QTextToSpeechPrivate(this), parent)
{
    Q_D(QTextToSpeech);
    d->setEngineProvider(engine, params);
}

/*!
    Destroys this QTextToSpeech object, stopping any speech.
*/
QTextToSpeech::~QTextToSpeech()
{
    stop(QTextToSpeech::BoundaryHint::Immediate);
}

/*!
    \property QTextToSpeech::engine
    \brief the engine used to synthesize text to speech.
    \since 6.4

    Changing the engine stops any ongoing speech.

    On most platforms, changing the engine will update the list of
    \l{availableLocales()}{available locales} and \l{availableVoices()}{available voices}.
*/

/*!
    \since 6.4
    Sets the engine used by this QTextToSpeech object to \a engine, passing
    \a params through to the engine constructor.

    \return whether \a engine could be set successfully.

    Which key/value pairs in \a params are supported depends on the engine.
    See \l{Qt TextToSpeech Engines}{the engine documentation} for details.
    Unsupported entries will be ignored.
*/
bool QTextToSpeech::setEngine(const QString &engine, const QVariantMap &params)
{
    Q_D(QTextToSpeech);
    if (d->m_providerName == engine)
        return true;

    d->setEngineProvider(engine, params);

    emit engineChanged(d->m_providerName);
    return d->m_engine;
}

QString QTextToSpeech::engine() const
{
    Q_D(const QTextToSpeech);
    return d->m_providerName;
}

/*!
    Gets the list of supported text-to-speech engine plug-ins.

    \sa engine
*/
QStringList QTextToSpeech::availableEngines()
{
    return QTextToSpeechPrivate::plugins().keys();
}

/*!
    \property QTextToSpeech::state
    \brief the current state of the speech synthesizer.

    Use \l say() to start synthesizing text with the current \l voice and \l locale.
*/
QTextToSpeech::State QTextToSpeech::state() const
{
    Q_D(const QTextToSpeech);
    if (d->m_engine)
        return d->m_engine->state();
    return QTextToSpeech::Error;
}

/*!
    \fn void QTextToSpeech::errorOccurred(QTextToSpeech::ErrorReason reason, const QString &errorString)

    This signal is emitted after an error occurred and the \l state has been set to
    QTextToSpeech::Error. The \a reason parameter specifies the type of error,
    and the \a errorString provides a human-readable error description.

    QTextToSpeech::ErrorReason is not a registered metatype, so for queued
    connections, you will have to register it with Q_DECLARE_METATYPE() and
    qRegisterMetaType().

    \sa errorReason(), errorString(), {Creating Custom Qt Types}
*/

/*!
    \return the reason why the engine has reported an error

    \sa state errorOccurred()
*/
QTextToSpeech::ErrorReason QTextToSpeech::errorReason() const
{
    Q_D(const QTextToSpeech);
    if (d->m_engine)
        return d->m_engine->errorReason();
    return QTextToSpeech::ErrorReason::Initialization;
}

/*!
    \return the current engine error message

    \sa errorOccurred()
*/
QString QTextToSpeech::errorString() const
{
    Q_D(const QTextToSpeech);
    if (d->m_engine)
        return d->m_engine->errorString();
    return tr("Text to speech engine not initialized");
}

/*!
    Starts synthesizing the \a text.

    This function starts sythesizing the speech asynchronously, and reads the text to the
    default audio output device.

    \note All in-progress readings are stopped before beginning to read the recently
    synthesized text.

    The current state is available using the \l state property, and is
    set to \l Speaking once the reading starts. When the reading is done,
    \l state will be set to \l Ready.

    \sa stop() pause() resume()
*/
void QTextToSpeech::say(const QString &text)
{
    Q_D(QTextToSpeech);
    if (d->m_engine)
        d->m_engine->say(text);
}

/*!
    Stops the current reading at \a boundaryHint.

    The reading cannot be resumed. Whether the \a boundaryHint is
    respected depends on the engine.

    \sa say() pause()
*/
void QTextToSpeech::stop(BoundaryHint boundaryHint)
{
    Q_D(QTextToSpeech);
    if (d->m_engine)
        d->m_engine->stop(boundaryHint);
}

/*!
    Pauses the current speech at \a boundaryHint.

    Whether the \a boundaryHint is respected depends on the  \l engine.

    \sa resume()
*/
void QTextToSpeech::pause(BoundaryHint boundaryHint)
{
    Q_D(QTextToSpeech);
    if (d->m_engine)
        d->m_engine->pause(boundaryHint);
}

/*!
    Resume speaking after \l pause() has been called.

    \sa pause()
*/
void QTextToSpeech::resume()
{
    Q_D(QTextToSpeech);
    if (d->m_engine)
        d->m_engine->resume();
}

/*!
    \property QTextToSpeech::pitch
    \brief the voice pitch, ranging from -1.0 to 1.0.

    The default of 0.0 is the normal speech pitch.
*/

void QTextToSpeech::setPitch(double pitch)
{
    Q_D(QTextToSpeech);
    if (!d->m_engine)
        return;

    pitch = qBound(-1.0, pitch, 1.0);
    if (d->m_engine->pitch() == pitch)
        return;
    if (d->m_engine && d->m_engine->setPitch(pitch))
        emit pitchChanged(pitch);
}

double QTextToSpeech::pitch() const
{
    Q_D(const QTextToSpeech);
    if (d->m_engine)
        return d->m_engine->pitch();
    return 0.0;
}

/*!
    \property QTextToSpeech::rate
    \brief the current voice rate, ranging from -1.0 to 1.0.

    The default value of 0.0 is normal speech flow.
*/
void QTextToSpeech::setRate(double rate)
{
    Q_D(QTextToSpeech);
    if (!d->m_engine)
        return;
    rate = qBound(-1.0, rate, 1.0);
    if (d->m_engine->rate() == rate)
        return;
    if (d->m_engine && d->m_engine->setRate(rate))
        emit rateChanged(rate);
}

double QTextToSpeech::rate() const
{
    Q_D(const QTextToSpeech);
    if (d->m_engine)
        return d->m_engine->rate();
    return 0.0;
}

/*!
    \property QTextToSpeech::volume
    \brief the current volume, ranging from 0.0 to 1.0.

    The default value is the platform's default volume.
*/
void QTextToSpeech::setVolume(double volume)
{
    Q_D(QTextToSpeech);
    if (!d->m_engine)
        return;

    volume = qBound(0.0, volume, 1.0);
    if (d->m_engine->volume() == volume)
        return;

    if (d->m_engine->setVolume(volume))
        emit volumeChanged(volume);
}

double QTextToSpeech::volume() const
{
    Q_D(const QTextToSpeech);
    if (d->m_engine)
        return d->m_engine->volume();
    return 0.0;
}

/*!
    \property QTextToSpeech::locale
    \brief the current locale in use. By default, the system locale is used.

    On some platforms, changing the locale will update the list of
    \l{availableVoices()}{available voices}, and if the current voice is not
    available with the new locale, a new voice will be set.

    \sa voice
*/
void QTextToSpeech::setLocale(const QLocale &locale)
{
    Q_D(QTextToSpeech);
    if (!d->m_engine)
        return;

    if (d->m_engine->locale() == locale)
        return;

    const QVoice oldVoice = voice();
    if (d->m_engine->setLocale(locale)) {
        emit localeChanged(locale);
        if (const QVoice newVoice = d->m_engine->voice(); oldVoice != newVoice)
            emit voiceChanged(newVoice);
    }
}

QLocale QTextToSpeech::locale() const
{
    Q_D(const QTextToSpeech);
    if (d->m_engine)
        return d->m_engine->locale();
    return QLocale();
}

/*!
    \return the list of locales that are supported by the active \l engine.

    \note On some platforms these can change, for example,
    when the backend changes synthesizers.
*/
QList<QLocale> QTextToSpeech::availableLocales() const
{
    Q_D(const QTextToSpeech);
    if (d->m_engine)
        return d->m_engine->availableLocales();
    return QList<QLocale>();
}

/*!
    \property QTextToSpeech::voice
    \brief the voice that will be used for the speech.

    On some platforms, setting the voice changes other voice attributes such
    as \l locale, \l pitch, and so on. These changes trigger the emission of
    signals.
*/
void QTextToSpeech::setVoice(const QVoice &voice)
{
    Q_D(QTextToSpeech);
    if (!d->m_engine)
        return;

    if (d->m_engine->voice() == voice)
        return;

    const QLocale oldLocale = locale();
    if (d->m_engine->setVoice(voice)) {
        emit voiceChanged(voice);
        if (const QLocale newLocale = d->m_engine->locale(); newLocale != oldLocale)
            emit localeChanged(newLocale);
    }
}

QVoice QTextToSpeech::voice() const
{
    Q_D(const QTextToSpeech);
    if (d->m_engine)
        return d->m_engine->voice();
    return QVoice();
}

/*!
    \return the list of voices available for the current \l locale.

    \note If no locale has been set, the system locale is used.
*/
QList<QVoice> QTextToSpeech::availableVoices() const
{
    Q_D(const QTextToSpeech);
    if (d->m_engine)
        return d->m_engine->availableVoices();
    return QList<QVoice>();
}

QT_END_NAMESPACE
