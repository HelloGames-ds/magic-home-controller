#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class QFile;
class QNetworkAccessManager;
class QNetworkReply;

namespace elkbledom {

class UpdateChecker final : public QObject
{
    Q_OBJECT

public:
    explicit UpdateChecker(QObject* parent = nullptr);
    ~UpdateChecker() override;

    static QString currentVersion();

public Q_SLOTS:
    void check();
    void download(const QString& version);
    void openReleasePage();

Q_SIGNALS:
    void checkStarted();
    void upToDate(const QString& version);
    void updateAvailable(const QString& version, const QUrl& pageUrl, const QString& notes);
    void downloadProgress(qint64 received, qint64 total);
    void updateReady(const QString& installerPath, const QString& version);
    void failed(const QString& message);

private:
    void parseRelease(const QByteArray& payload);
    void startDownload(const QUrl& url, const QString& version);
    void downloadChecksum(const QUrl& url);
    void finishWithChecksum(const QString& expectedHex);
    void failDownload();
    void stopDownload();
    static bool isNewer(const QString& candidate, const QString& current);
    static QString readChecksum(const QByteArray& payload, const QString& fileName);

    QNetworkAccessManager* network_ = nullptr;
    QNetworkReply* reply_ = nullptr;
    QFile* file_ = nullptr;
    QString version_;
    QUrl pageUrl_;
    QUrl assetUrl_;
    QUrl checksumUrl_;
    QString assetName_;
    qint64 expectedBytes_ = 0;
};

} // namespace elkbledom
