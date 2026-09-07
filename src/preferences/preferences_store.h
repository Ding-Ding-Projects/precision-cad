#pragma once

#include <QObject>
#include <QString>
#include <memory>

namespace precision::preferences {

class PreferencesStore final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString languageMode READ languageMode NOTIFY languageModeChanged)
    Q_PROPERTY(int englishTone READ englishTone NOTIFY englishToneChanged)
    Q_PROPERTY(int cantoneseTone READ cantoneseTone NOTIFY cantoneseToneChanged)
    Q_PROPERTY(bool dialogEmojis READ dialogEmojis NOTIFY dialogEmojisChanged)
    Q_PROPERTY(QString theme READ theme NOTIFY themeChanged)
    Q_PROPERTY(double fontScale READ fontScale NOTIFY fontScaleChanged)
    Q_PROPERTY(QString accentColor READ accentColor NOTIFY accentColorChanged)
    Q_PROPERTY(bool reducedMotion READ reducedMotion NOTIFY reducedMotionChanged)
    Q_PROPERTY(bool adhdMode READ adhdMode NOTIFY adhdModeChanged)
    Q_PROPERTY(bool narrationEnabled READ narrationEnabled NOTIFY narrationEnabledChanged)
    Q_PROPERTY(QString narrationLanguage READ narrationLanguage NOTIFY narrationLanguageChanged)
    Q_PROPERTY(QString englishVoiceId READ englishVoiceId NOTIFY englishVoiceIdChanged)
    Q_PROPERTY(QString cantoneseVoiceId READ cantoneseVoiceId NOTIFY cantoneseVoiceIdChanged)
    Q_PROPERTY(double narrationRate READ narrationRate NOTIFY narrationRateChanged)
    Q_PROPERTY(double narrationPitch READ narrationPitch NOTIFY narrationPitchChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorOccurred)

public:
    explicit PreferencesStore(QString storagePath = {}, QObject *parent = nullptr);
    ~PreferencesStore() override;

    QString languageMode() const;
    int englishTone() const;
    int cantoneseTone() const;
    bool dialogEmojis() const;
    QString theme() const;
    double fontScale() const;
    QString accentColor() const;
    bool reducedMotion() const;
    bool adhdMode() const;
    bool narrationEnabled() const;
    QString narrationLanguage() const;
    QString englishVoiceId() const;
    QString cantoneseVoiceId() const;
    double narrationRate() const;
    double narrationPitch() const;
    QString lastError() const;
    QString storagePath() const;

    Q_INVOKABLE bool setLanguageMode(const QString &value);
    Q_INVOKABLE bool setEnglishTone(int value);
    Q_INVOKABLE bool setCantoneseTone(int value);
    Q_INVOKABLE bool setDialogEmojis(bool value);
    Q_INVOKABLE bool setTheme(const QString &value);
    Q_INVOKABLE bool setFontScale(double value);
    Q_INVOKABLE bool setAccentColor(const QString &value);
    Q_INVOKABLE bool setReducedMotion(bool value);
    Q_INVOKABLE bool setAdhdMode(bool value);
    Q_INVOKABLE bool setNarrationEnabled(bool value);
    Q_INVOKABLE bool setNarrationLanguage(const QString &value);
    Q_INVOKABLE bool setEnglishVoiceId(const QString &value);
    Q_INVOKABLE bool setCantoneseVoiceId(const QString &value);
    Q_INVOKABLE bool setNarrationRate(double value);
    Q_INVOKABLE bool setNarrationPitch(double value);
    Q_INVOKABLE bool reset();
    Q_INVOKABLE QByteArray exportPublicPreferences() const;

signals:
    void languageModeChanged(); void englishToneChanged(); void cantoneseToneChanged();
    void dialogEmojisChanged(); void themeChanged(); void fontScaleChanged();
    void accentColorChanged(); void reducedMotionChanged(); void adhdModeChanged();
    void narrationEnabledChanged(); void narrationLanguageChanged(); void englishVoiceIdChanged();
    void cantoneseVoiceIdChanged(); void narrationRateChanged(); void narrationPitchChanged();
    void errorOccurred(const QString &message);

private:
    struct Values;
    bool replace(const Values &candidate);
    bool persist(const Values &candidate);
    void load();
    void fail(const QString &message);
    void publishChanges(const Values &before);
    std::unique_ptr<Values> m_values;
    QString m_storagePath;
    QString m_lastError;
};

} // namespace precision::preferences
