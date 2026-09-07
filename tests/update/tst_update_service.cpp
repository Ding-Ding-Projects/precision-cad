#include <QtTest>
#include "update/update_service.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

using namespace precision::update;

class FakeTransport final : public UpdateTransport {
public:
  QHash<QString, TransferResult> results;
  void get(const QUrl &url, std::function<void(TransferResult)> completed) override { completed(results.value(url.toString())); }
};
class FakeProcess final : public UpdateProcess {
public:
  bool called=false, succeeds=true; QString program; QStringList arguments;
  bool start(const QString &p,const QStringList &a,QString *error) override { called=true; program=p; arguments=a; if(!succeeds && error) *error=QStringLiteral("fake process refusal"); return succeeds; }
};
class UpdateServiceTest final : public QObject {
  Q_OBJECT
private:
  static UpdateConfig config(const QTemporaryDir &dir) { return {QStringLiteral("0.1.0"),QUrl(QStringLiteral("https://updates.example.test/release/metadata.json")),dir.path(),dir.filePath("stage"),1}; }
  static void installShape(const QTemporaryDir &dir) { QFile update(dir.filePath("Update.exe")); QVERIFY(update.open(QIODevice::WriteOnly)); update.write("fake"); QFile releases(dir.filePath("RELEASES")); QVERIFY(releases.open(QIODevice::WriteOnly)); releases.write("local"); }
  static QByteArray metadata(const QByteArray &package, QString version=QStringLiteral("0.1.1")) { return QJsonDocument(QJsonObject{{"version",version},{"notesUrl","https://updates.example.test/release/notes/0.1.1"},{"packageUrl","https://updates.example.test/release/PrecisionCAD-0.1.1-full.nupkg"},{"sha256",QString::fromLatin1(QCryptographicHash::hash(package,QCryptographicHash::Sha256).toHex())}}).toJson(QJsonDocument::Compact); }
private slots:
  void rejectsUntrustedAndDowngradeMetadata() {
    QTemporaryDir dir; UpdateInfo info; QString error; auto c=config(dir);
    QVERIFY(!UpdateService::parseMetadata(metadata("x",QStringLiteral("0.1.0")),c,&info,&error));
    const QByteArray unsafe=R"({"version":"0.1.1","notesUrl":"https://updates.example.test/a","packageUrl":"https://evil.test/PrecisionCAD-0.1.1-full.nupkg","sha256":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"})";
    QVERIFY(!UpdateService::parseMetadata(unsafe,c,&info,&error));
  }
  void rejectsTraversalAndBadManifest() {
    QVERIFY(!UpdateService::isSafePackageName(QStringLiteral("../PrecisionCAD-full.nupkg")));
    QVERIFY(!UpdateService::isSafePackageName(QStringLiteral("PrecisionCAD-delta.nupkg")));
    UpdateInfo info; info.packageUrl=QUrl(QStringLiteral("https://updates.example.test/release/PrecisionCAD-0.1.1-full.nupkg")); QString error;
    QVERIFY(!UpdateService::verifyReleases("0123 ../PrecisionCAD-0.1.1-full.nupkg 7",info,&error));
  }
  void rejectsRedirectToAnotherOrigin() {
    const QUrl feed(QStringLiteral("https://updates.example.test/release/metadata.json"));
    QVERIFY(UpdateService::isApprovedUrl(QUrl(QStringLiteral("https://updates.example.test/release/other")),feed));
    QVERIFY(!UpdateService::isApprovedUrl(QUrl(QStringLiteral("http://updates.example.test/release/other")),feed));
    QVERIFY(!UpdateService::isApprovedUrl(QUrl(QStringLiteral("https://evil.test/release/other")),feed));
  }
  void downloadsOnlyAfterManifestThenRequiresExplicitApproval() {
    QTemporaryDir dir; installShape(dir); const QByteArray package("candidate-package"); auto c=config(dir); auto transport=std::make_unique<FakeTransport>(); auto *raw=transport.get();
    const QString packageUrl=QStringLiteral("https://updates.example.test/release/PrecisionCAD-0.1.1-full.nupkg");
    raw->results.insert(c.feedUrl.toString(),{200,c.feedUrl,metadata(package),{}});
    const QUrl releases=QUrl(QStringLiteral("https://updates.example.test/release/RELEASES"));
    raw->results.insert(releases.toString(),{200,releases,QCryptographicHash::hash(package,QCryptographicHash::Sha1).toHex()+" PrecisionCAD-0.1.1-full.nupkg "+QByteArray::number(package.size()),{}});
    raw->results.insert(packageUrl,{200,QUrl(packageUrl),package,{}});
    UpdateService service(c); service.setTransport(std::move(transport)); auto process=std::make_unique<FakeProcess>(); auto *fake=process.get(); service.setProcess(std::move(process));
    service.checkNow(); QCOMPARE(service.state(),UpdateState::Ready); QVERIFY(QFile::exists(dir.filePath("stage/PrecisionCAD-0.1.1-full.nupkg")));
    QSignalSpy approval(&service,&UpdateService::restartApprovalRequested); service.requestInstall(); QCOMPARE(service.state(),UpdateState::Installing); QCOMPARE(approval.count(),1); QVERIFY(!fake->called);
    service.approveInstall(false); QCOMPARE(service.state(),UpdateState::Ready); QVERIFY(!fake->called);
    service.requestInstall(); service.approveInstall(true); QVERIFY(fake->called); QCOMPARE(fake->program,QDir::cleanPath(dir.filePath("Update.exe"))); QVERIFY(fake->arguments.first().startsWith(QStringLiteral("--update=https://")));
  }
  void badSha1AndCancellationRefuseReady() {
    QTemporaryDir dir; installShape(dir); const QByteArray package("candidate-package"); auto c=config(dir); auto transport=std::make_unique<FakeTransport>(); auto *raw=transport.get();
    raw->results.insert(c.feedUrl.toString(),{200,c.feedUrl,metadata(package),{}}); const QUrl releases(QStringLiteral("https://updates.example.test/release/RELEASES")); raw->results.insert(releases.toString(),{200,releases,"0000000000000000000000000000000000000000 PrecisionCAD-0.1.1-full.nupkg 17",{}}); raw->results.insert(QStringLiteral("https://updates.example.test/release/PrecisionCAD-0.1.1-full.nupkg"),{200,QUrl(QStringLiteral("https://updates.example.test/release/PrecisionCAD-0.1.1-full.nupkg")),package,{}});
    UpdateService service(c); service.setTransport(std::move(transport)); service.checkNow(); QCOMPARE(service.state(),UpdateState::Error); service.cancel(); QCOMPARE(service.state(),UpdateState::Error);
  }
  void standaloneBuildIsHonestUnavailable() { QTemporaryDir dir; UpdateService service(config(dir)); service.startupCheck(); QCOMPARE(service.state(),UpdateState::Unavailable); }
};
QTEST_GUILESS_MAIN(UpdateServiceTest)
#include "tst_update_service.moc"
