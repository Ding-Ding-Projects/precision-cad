#pragma once

#include <QObject>
#include <QHash>

namespace precision::preferences {

class PersonalVocabularyStore final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)
public:
    explicit PersonalVocabularyStore(QString cachePath = {}, QObject *parent = nullptr);
    bool loaded() const;
    Q_INVOKABLE bool loadFile(const QString &userChosenPath);
    Q_INVOKABLE bool clear();
    QString applyPrivateUiText(const QString &text) const;
signals:
    void loadedChanged();
    void errorOccurred(const QString &message);
private:
    bool loadBytes(const QByteArray &bytes, bool persist);
    void fail(const QString &message);
    QString m_cachePath;
    QHash<QString, QString> m_entries;
};

} // namespace precision::preferences
