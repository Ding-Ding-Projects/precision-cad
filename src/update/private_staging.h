#pragma once
#include <QString>

namespace precision::update::staging {
// Creates only missing directories. Existing ACLs and ownership are never modified.
bool prepareRoot(const QString &root);
bool createCandidate(const QString &root, const QString &candidate);
bool verifyPrivateDirectory(const QString &path);
class Directory {
public:
  explicit Directory(const QString &root);
  ~Directory();
  bool isValid() const { return !m_path.isEmpty(); }
  const QString &path() const { return m_path; }
  void setAutoRemove(bool enabled) { m_autoRemove = enabled; }
private:
  QString m_path;
  bool m_autoRemove = true;
};
}
