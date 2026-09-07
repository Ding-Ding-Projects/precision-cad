#include "preferences_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QRegularExpression>
#include <cmath>
#include <memory>

namespace precision::preferences {
namespace {
constexpr int kSchemaVersion = 1;
constexpr double kMinFontScale = 0.75;
constexpr double kMaxFontScale = 2.0;
constexpr double kMinNarration = 0.5;
constexpr double kMaxNarration = 2.0;

bool oneOf(const QString &value, std::initializer_list<const char *> accepted) {
    for (const auto *item : accepted) if (value == QLatin1String(item)) return true;
    return false;
}
bool validVoice(const QString &value) { return value.size() <= 256 && !value.contains(QChar::Null); }
bool validAccent(const QString &value) {
    static const QRegularExpression hex(QStringLiteral("^#[0-9A-Fa-f]{6}$"));
    return hex.match(value).hasMatch();
}
bool validFinite(double value, double minimum, double maximum) {
    return std::isfinite(value) && value >= minimum && value <= maximum;
}
QString defaultPath() {
    const auto root = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return QDir(root).filePath(QStringLiteral("preferences.json"));
}
}

struct PreferencesStore::Values {
    QString languageMode = QStringLiteral("en");
    int englishTone = 5;
    int cantoneseTone = 5;
    bool dialogEmojis = true;
    QString theme = QStringLiteral("system");
    double fontScale = 1.0;
    QString accentColor = QStringLiteral("#6750A4");
    bool reducedMotion = false;
    bool adhdMode = false;
    bool narrationEnabled = false;
    QString narrationLanguage = QStringLiteral("en");
    QString englishVoiceId;
    QString cantoneseVoiceId;
    double narrationRate = 1.0;
    double narrationPitch = 1.0;
};

PreferencesStore::~PreferencesStore() = default;

PreferencesStore::PreferencesStore(QString storagePath, QObject *parent)
    : QObject(parent), m_values(std::make_unique<Values>()), m_storagePath(storagePath.isEmpty() ? defaultPath() : std::move(storagePath)) { load(); }

QString PreferencesStore::languageMode() const { return m_values->languageMode; }
int PreferencesStore::englishTone() const { return m_values->englishTone; }
int PreferencesStore::cantoneseTone() const { return m_values->cantoneseTone; }
bool PreferencesStore::dialogEmojis() const { return m_values->dialogEmojis; }
QString PreferencesStore::theme() const { return m_values->theme; }
double PreferencesStore::fontScale() const { return m_values->fontScale; }
QString PreferencesStore::accentColor() const { return m_values->accentColor; }
bool PreferencesStore::reducedMotion() const { return m_values->reducedMotion; }
bool PreferencesStore::adhdMode() const { return m_values->adhdMode; }
bool PreferencesStore::narrationEnabled() const { return m_values->narrationEnabled; }
QString PreferencesStore::narrationLanguage() const { return m_values->narrationLanguage; }
QString PreferencesStore::englishVoiceId() const { return m_values->englishVoiceId; }
QString PreferencesStore::cantoneseVoiceId() const { return m_values->cantoneseVoiceId; }
double PreferencesStore::narrationRate() const { return m_values->narrationRate; }
double PreferencesStore::narrationPitch() const { return m_values->narrationPitch; }
QString PreferencesStore::lastError() const { return m_lastError; }
QString PreferencesStore::storagePath() const { return m_storagePath; }

bool PreferencesStore::setLanguageMode(const QString &v) { if (!oneOf(v, {"en","yue","both"})) { fail("languageMode must be en, yue, or both"); return false; } auto c=*m_values; c.languageMode=v; return replace(c); }
bool PreferencesStore::setEnglishTone(int v) { if (v<1||v>5) { fail("englishTone must be from 1 to 5"); return false; } auto c=*m_values; c.englishTone=v; return replace(c); }
bool PreferencesStore::setCantoneseTone(int v) { if (v<1||v>5) { fail("cantoneseTone must be from 1 to 5"); return false; } auto c=*m_values; c.cantoneseTone=v; return replace(c); }
bool PreferencesStore::setDialogEmojis(bool v) { auto c=*m_values; c.dialogEmojis=v; return replace(c); }
bool PreferencesStore::setTheme(const QString &v) { if (!oneOf(v, {"light","dark","system"})) { fail("theme must be light, dark, or system"); return false; } auto c=*m_values; c.theme=v; return replace(c); }
bool PreferencesStore::setFontScale(double v) { if (!validFinite(v,kMinFontScale,kMaxFontScale)) { fail("fontScale must be from 0.75 to 2.0"); return false; } auto c=*m_values; c.fontScale=v; return replace(c); }
bool PreferencesStore::setAccentColor(const QString &v) { if (!validAccent(v)) { fail("accentColor must be an opaque #RRGGBB color"); return false; } auto x=*m_values; x.accentColor=v.toUpper(); return replace(x); }
bool PreferencesStore::setReducedMotion(bool v) { auto c=*m_values; c.reducedMotion=v; return replace(c); }
bool PreferencesStore::setAdhdMode(bool v) { auto c=*m_values; c.adhdMode=v; return replace(c); }
bool PreferencesStore::setNarrationEnabled(bool v) { auto c=*m_values; c.narrationEnabled=v; return replace(c); }
bool PreferencesStore::setNarrationLanguage(const QString &v) { if (!oneOf(v,{"en","yue","both"})) { fail("narrationLanguage must be en, yue, or both"); return false; } auto c=*m_values; c.narrationLanguage=v; return replace(c); }
bool PreferencesStore::setEnglishVoiceId(const QString &v) { if (!validVoice(v)) { fail("englishVoiceId is invalid"); return false; } auto c=*m_values; c.englishVoiceId=v; return replace(c); }
bool PreferencesStore::setCantoneseVoiceId(const QString &v) { if (!validVoice(v)) { fail("cantoneseVoiceId is invalid"); return false; } auto c=*m_values; c.cantoneseVoiceId=v; return replace(c); }
bool PreferencesStore::setNarrationRate(double v) { if (!validFinite(v,kMinNarration,kMaxNarration)) { fail("narrationRate must be from 0.5 to 2.0"); return false; } auto c=*m_values; c.narrationRate=v; return replace(c); }
bool PreferencesStore::setNarrationPitch(double v) { if (!validFinite(v,kMinNarration,kMaxNarration)) { fail("narrationPitch must be from 0.5 to 2.0"); return false; } auto c=*m_values; c.narrationPitch=v; return replace(c); }
bool PreferencesStore::reset() { return replace(Values{}); }

bool PreferencesStore::replace(const Values &candidate) {
    const Values before=*m_values;
    if (candidate.languageMode==before.languageMode && candidate.englishTone==before.englishTone && candidate.cantoneseTone==before.cantoneseTone && candidate.dialogEmojis==before.dialogEmojis && candidate.theme==before.theme && candidate.fontScale==before.fontScale && candidate.accentColor==before.accentColor && candidate.reducedMotion==before.reducedMotion && candidate.adhdMode==before.adhdMode && candidate.narrationEnabled==before.narrationEnabled && candidate.narrationLanguage==before.narrationLanguage && candidate.englishVoiceId==before.englishVoiceId && candidate.cantoneseVoiceId==before.cantoneseVoiceId && candidate.narrationRate==before.narrationRate && candidate.narrationPitch==before.narrationPitch) return true;
    if (!persist(candidate)) return false;
    *m_values=candidate; publishChanges(before); return true;
}

bool PreferencesStore::persist(const Values &v) {
    const QFileInfo info(m_storagePath); if (!QDir().mkpath(info.absolutePath())) { fail("preferences directory cannot be created"); return false; }
    QLockFile lock(m_storagePath + ".lock"); lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) { fail("preferences are busy in another writer"); return false; }
    QJsonObject root{{"schemaVersion",kSchemaVersion},{"languageMode",v.languageMode},{"englishTone",v.englishTone},{"cantoneseTone",v.cantoneseTone},{"dialogEmojis",v.dialogEmojis},{"theme",v.theme},{"fontScale",v.fontScale},{"accentColor",v.accentColor},{"reducedMotion",v.reducedMotion},{"adhdMode",v.adhdMode},{"narrationEnabled",v.narrationEnabled},{"narrationLanguage",v.narrationLanguage},{"englishVoiceId",v.englishVoiceId},{"cantoneseVoiceId",v.cantoneseVoiceId},{"narrationRate",v.narrationRate},{"narrationPitch",v.narrationPitch}};
    QSaveFile output(m_storagePath); if (!output.open(QIODevice::WriteOnly)) { fail("preferences file cannot be opened for atomic write"); return false; }
    if (output.write(QJsonDocument(root).toJson(QJsonDocument::Compact)) < 0 || !output.commit()) { fail("preferences atomic write failed"); return false; }
    return true;
}

void PreferencesStore::load() {
    QFile input(m_storagePath); if (!input.exists()) return;
    if (!input.open(QIODevice::ReadOnly)) { fail("preferences cannot be read; defaults retained"); return; }
    QJsonParseError error; const QJsonDocument doc=QJsonDocument::fromJson(input.readAll(),&error);
    const QSet<QString> allowed={"schemaVersion","languageMode","englishTone","cantoneseTone","dialogEmojis","theme","fontScale","accentColor","reducedMotion","adhdMode","narrationEnabled","narrationLanguage","englishVoiceId","cantoneseVoiceId","narrationRate","narrationPitch"};
    const QJsonObject o=doc.object();
    const auto keys = o.keys();
    if (error.error!=QJsonParseError::NoError || !doc.isObject() || QSet<QString>(keys.cbegin(), keys.cend())!=allowed || o.value("schemaVersion").toInt(-1)!=kSchemaVersion) { fail("preferences are unsupported or corrupt; defaults retained"); return; }
    Values v;
    auto invalid=[&](){ fail("preferences contain invalid values; defaults retained"); };
    if (!o.value("languageMode").isString()||!o.value("englishTone").isDouble()||!o.value("cantoneseTone").isDouble()||!o.value("dialogEmojis").isBool()||!o.value("theme").isString()||!o.value("fontScale").isDouble()||!o.value("accentColor").isString()||!o.value("reducedMotion").isBool()||!o.value("adhdMode").isBool()||!o.value("narrationEnabled").isBool()||!o.value("narrationLanguage").isString()||!o.value("englishVoiceId").isString()||!o.value("cantoneseVoiceId").isString()||!o.value("narrationRate").isDouble()||!o.value("narrationPitch").isDouble()) { invalid(); return; }
    v.languageMode=o["languageMode"].toString(); v.englishTone=o["englishTone"].toInt(); v.cantoneseTone=o["cantoneseTone"].toInt(); v.dialogEmojis=o["dialogEmojis"].toBool(); v.theme=o["theme"].toString(); v.fontScale=o["fontScale"].toDouble(); v.accentColor=o["accentColor"].toString(); v.reducedMotion=o["reducedMotion"].toBool(); v.adhdMode=o["adhdMode"].toBool(); v.narrationEnabled=o["narrationEnabled"].toBool(); v.narrationLanguage=o["narrationLanguage"].toString(); v.englishVoiceId=o["englishVoiceId"].toString(); v.cantoneseVoiceId=o["cantoneseVoiceId"].toString(); v.narrationRate=o["narrationRate"].toDouble(); v.narrationPitch=o["narrationPitch"].toDouble();
    if (!oneOf(v.languageMode,{"en","yue","both"})||v.englishTone<1||v.englishTone>5||v.cantoneseTone<1||v.cantoneseTone>5||!oneOf(v.theme,{"light","dark","system"})||!validFinite(v.fontScale,kMinFontScale,kMaxFontScale)||!validAccent(v.accentColor)||!oneOf(v.narrationLanguage,{"en","yue","both"})||!validVoice(v.englishVoiceId)||!validVoice(v.cantoneseVoiceId)||!validFinite(v.narrationRate,kMinNarration,kMaxNarration)||!validFinite(v.narrationPitch,kMinNarration,kMaxNarration)) { invalid(); return; }
    *m_values=v;
}

QByteArray PreferencesStore::exportPublicPreferences() const {
    const auto &v=*m_values; return QJsonDocument(QJsonObject{{"schemaVersion",kSchemaVersion},{"languageMode",v.languageMode},{"englishTone",v.englishTone},{"cantoneseTone",v.cantoneseTone},{"dialogEmojis",v.dialogEmojis},{"theme",v.theme},{"fontScale",v.fontScale},{"accentColor",v.accentColor},{"reducedMotion",v.reducedMotion},{"adhdMode",v.adhdMode},{"narrationEnabled",v.narrationEnabled},{"narrationLanguage",v.narrationLanguage},{"narrationRate",v.narrationRate},{"narrationPitch",v.narrationPitch}}).toJson(QJsonDocument::Compact);
}
void PreferencesStore::fail(const QString &message) { m_lastError=message; emit errorOccurred(message); }
void PreferencesStore::publishChanges(const Values &b) { const auto &v=*m_values; if(v.languageMode!=b.languageMode)emit languageModeChanged(); if(v.englishTone!=b.englishTone)emit englishToneChanged(); if(v.cantoneseTone!=b.cantoneseTone)emit cantoneseToneChanged(); if(v.dialogEmojis!=b.dialogEmojis)emit dialogEmojisChanged(); if(v.theme!=b.theme)emit themeChanged(); if(v.fontScale!=b.fontScale)emit fontScaleChanged(); if(v.accentColor!=b.accentColor)emit accentColorChanged(); if(v.reducedMotion!=b.reducedMotion)emit reducedMotionChanged(); if(v.adhdMode!=b.adhdMode)emit adhdModeChanged(); if(v.narrationEnabled!=b.narrationEnabled)emit narrationEnabledChanged(); if(v.narrationLanguage!=b.narrationLanguage)emit narrationLanguageChanged(); if(v.englishVoiceId!=b.englishVoiceId)emit englishVoiceIdChanged(); if(v.cantoneseVoiceId!=b.cantoneseVoiceId)emit cantoneseVoiceIdChanged(); if(v.narrationRate!=b.narrationRate)emit narrationRateChanged(); if(v.narrationPitch!=b.narrationPitch)emit narrationPitchChanged(); }
}
