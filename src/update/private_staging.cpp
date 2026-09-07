#include "private_staging.h"
#include <QDir>
#include <QFileInfo>
#include <QUuid>
#include <vector>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#endif

namespace precision::update::staging {
#ifdef Q_OS_WIN
namespace {
struct Handle {
  HANDLE value = INVALID_HANDLE_VALUE;
  explicit Handle(HANDLE handle) : value(handle) {}
  ~Handle() { if (value != INVALID_HANDLE_VALUE && value != nullptr) CloseHandle(value); }
  Handle(const Handle &) = delete;
  Handle &operator=(const Handle &) = delete;
  bool valid() const { return value != INVALID_HANDLE_VALUE && value != nullptr; }
};
struct LocalMemory {
  PVOID value = nullptr;
  ~LocalMemory() { if (value) LocalFree(value); }
};
struct Identity {
  std::vector<BYTE> bytes;
  BYTE system[SECURITY_MAX_SID_SIZE]{}, administrators[SECURITY_MAX_SID_SIZE]{};
  PSID user = nullptr;
  bool load() {
    HANDLE raw = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &raw)) return false;
    Handle token(raw);
    DWORD count = 0;
    GetTokenInformation(token.value, TokenUser, nullptr, 0, &count);
    if (!count || count > 64 * 1024) return false;
    bytes.resize(count);
    if (!GetTokenInformation(token.value, TokenUser, bytes.data(), count, &count)) return false;
    user = reinterpret_cast<TOKEN_USER *>(bytes.data())->User.Sid;
    DWORD systemBytes = sizeof(system), administratorBytes = sizeof(administrators);
    return IsValidSid(user)
      && CreateWellKnownSid(WinLocalSystemSid, nullptr, system, &systemBytes)
      && CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, administrators, &administratorBytes);
  }
};
const wchar_t *wide(const QString &path) { return reinterpret_cast<const wchar_t *>(path.utf16()); }
bool plainLocalPath(const QString &path) {
  return path.size() >= 3 && path[0].isLetter() && path[1] == u':' && path[2] == u'/'
    && !path.mid(2).contains(u':') && QDir::isAbsolutePath(path)
    && QDir::cleanPath(path) == path;
}
bool noReparseAncestors(const QString &path) {
  QString current = path;
  while (!current.isEmpty()) {
    const DWORD attributes = GetFileAttributesW(wide(QDir::toNativeSeparators(current)));
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)
        || (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
    const QString parent = QFileInfo(current).absolutePath();
    if (parent == current || current.size() == 3) break;
    current = parent;
  }
  const auto canonical = QFileInfo(path).canonicalFilePath();
  return !canonical.isEmpty() && QDir::cleanPath(canonical).compare(path, Qt::CaseInsensitive) == 0;
}
bool inspect(const QString &path, const Identity &identity, bool strict) {
  if (!plainLocalPath(path) || !noReparseAncestors(path)) return false;
  // Exclude delete sharing while inspecting a directory's own security descriptor.
  Handle directory(CreateFileW(wide(QDir::toNativeSeparators(path)), READ_CONTROL | FILE_READ_ATTRIBUTES,
    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
  if (!directory.valid()) return false;
  BY_HANDLE_FILE_INFORMATION file{};
  if (!GetFileInformationByHandle(directory.value, &file) || (file.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
  PSID owner = nullptr;
  PACL acl = nullptr;
  LocalMemory descriptor;
  if (GetSecurityInfo(directory.value, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
      &owner, nullptr, &acl, nullptr, reinterpret_cast<PSECURITY_DESCRIPTOR *>(&descriptor.value)) != ERROR_SUCCESS
      || !owner || !acl || !IsValidAcl(acl)) return false;
  if (!EqualSid(owner, identity.user) && (strict ||
      (!EqualSid(owner, const_cast<BYTE *>(identity.system)) && !EqualSid(owner, const_cast<BYTE *>(identity.administrators))))) return false;
  SECURITY_DESCRIPTOR_CONTROL control{};
  DWORD revision = 0;
  if (!GetSecurityDescriptorControl(descriptor.value, &control, &revision)
      || (strict && !(control & SE_DACL_PROTECTED))) return false;
  bool userAllowed = false, systemAllowed = false;
  constexpr DWORD writes = GENERIC_ALL | GENERIC_WRITE | WRITE_DAC | WRITE_OWNER | DELETE
    | FILE_DELETE_CHILD | FILE_ADD_FILE | FILE_ADD_SUBDIRECTORY | FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA;
  if (strict && acl->AceCount != 2) return false;
  for (DWORD index = 0; index < acl->AceCount; ++index) {
    PVOID raw = nullptr;
    if (!GetAce(acl, index, &raw)) return false;
    const auto *header = static_cast<ACE_HEADER *>(raw);
    if (!strict && (header->AceFlags & INHERIT_ONLY_ACE)) continue;
    if (header->AceType == ACCESS_DENIED_ACE_TYPE && !strict) continue;
    // Unknown/object/callback ACEs are ambiguous and fail closed.
    if (header->AceType != ACCESS_ALLOWED_ACE_TYPE || header->AceSize < sizeof(ACCESS_ALLOWED_ACE)) return false;
    const auto *ace = static_cast<ACCESS_ALLOWED_ACE *>(raw);
    PSID sid = const_cast<DWORD *>(&ace->SidStart);
    if (!IsValidSid(sid)) return false;
    const bool user = EqualSid(sid, identity.user), system = EqualSid(sid, const_cast<BYTE *>(identity.system));
    const bool admin = EqualSid(sid, const_cast<BYTE *>(identity.administrators));
    if (strict) {
      if ((!user && !system) || (header->AceFlags != (OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE))
          || (ace->Mask & FILE_ALL_ACCESS) != FILE_ALL_ACCESS) return false;
      userAllowed |= user; systemAllowed |= system;
    } else if (!user && !system && !admin && !IsWellKnownSid(sid, WinCreatorOwnerRightsSid) && (ace->Mask & writes)) return false;
  }
  return !strict || (userAllowed && systemAllowed);
}
bool create(const QString &path, const Identity &identity) {
  LocalMemory sidText, descriptor;
  if (!ConvertSidToStringSidW(identity.user, reinterpret_cast<LPWSTR *>(&sidText.value))) return false;
  const QString sid = QString::fromWCharArray(static_cast<LPWSTR>(sidText.value));
  const QString sddl = QStringLiteral("O:%1D:P(A;OICI;FA;;;%1)(A;OICI;FA;;;SY)").arg(sid);
  if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(wide(sddl), SDDL_REVISION_1,
      reinterpret_cast<PSECURITY_DESCRIPTOR *>(&descriptor.value), nullptr)) return false;
  SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), descriptor.value, FALSE};
  if (!CreateDirectoryW(wide(QDir::toNativeSeparators(path)), &attributes)) return false;
  return inspect(path, identity, true);
}
bool trustedAncestry(const QString &path, const Identity &identity) {
  const QString home = QDir::fromNativeSeparators(QDir::homePath());
  if (!plainLocalPath(home) || (path.compare(home, Qt::CaseInsensitive) != 0
      && !path.startsWith(home + u'/', Qt::CaseInsensitive))) return false;
  QString current = path;
  for (;;) {
    if (!inspect(current, identity, false)) return false;
    if (current.compare(home, Qt::CaseInsensitive) == 0) return true;
    const QString parent = QFileInfo(current).absolutePath();
    if (parent == current) return false;
    current = parent;
  }
}
} // namespace
#endif

bool prepareRoot(const QString &root) {
#ifdef Q_OS_WIN
  Identity identity;
  if (!identity.load() || !plainLocalPath(root)) return false;
  QString existing = root;
  QStringList missing;
  while (!QFileInfo::exists(existing)) {
    missing.prepend(existing);
    const QString parent = QFileInfo(existing).absolutePath();
    if (parent == existing) return false;
    existing = parent;
  }
  // The nearest existing parent must belong to this user and exclude other-user writes.
  if (!trustedAncestry(existing, identity) || !inspect(existing, identity, missing.isEmpty())) return false;
  if (missing.isEmpty() && !inspect(QFileInfo(root).absolutePath(), identity, false)) return false;
  for (const auto &directory : missing) if (!create(directory, identity)) return false;
  return inspect(root, identity, true);
#else
  Q_UNUSED(root);
  return false;
#endif
}
bool createCandidate(const QString &root, const QString &candidate) {
#ifdef Q_OS_WIN
  Identity identity;
  if (!identity.load() || !plainLocalPath(candidate)
      || QFileInfo(candidate).absolutePath().compare(root, Qt::CaseInsensitive) != 0
      || !trustedAncestry(root, identity) || !inspect(root, identity, true) || QFileInfo::exists(candidate)) return false;
  return create(candidate, identity);
#else
  Q_UNUSED(root); Q_UNUSED(candidate);
  return false;
#endif
}
bool verifyPrivateDirectory(const QString &path) {
#ifdef Q_OS_WIN
  Identity identity;
  return identity.load() && trustedAncestry(path, identity) && inspect(path, identity, true);
#else
  Q_UNUSED(path);
  return false;
#endif
}
Directory::Directory(const QString &root) {
  const QString candidate = QDir(root).filePath(QStringLiteral("candidate-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
  if (createCandidate(root, candidate)) m_path = candidate;
}
Directory::~Directory() {
  if (m_autoRemove && !m_path.isEmpty() && verifyPrivateDirectory(m_path)) QDir(m_path).removeRecursively();
}
} // namespace precision::update::staging
