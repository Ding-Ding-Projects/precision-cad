#pragma once
#include <QObject>
#include <QUrl>
#include "preferences_store.h"
#include "personal_vocabulary_store.h"
class UiText final : public QObject {
 Q_OBJECT
public:
 UiText(precision::preferences::PreferencesStore *p, precision::preferences::PersonalVocabularyStore *v, QObject *parent=nullptr):QObject(parent),prefs(p),vocab(v) {}
 Q_INVOKABLE QString apply(const QString &text) const { return vocab->applyPrivateUiText(text); }
 Q_INVOKABLE bool loadVocabulary(const QUrl &url) { return url.isLocalFile() && vocab->loadFile(url.toLocalFile()); }
private:
 precision::preferences::PreferencesStore *prefs;
 precision::preferences::PersonalVocabularyStore *vocab;
};
