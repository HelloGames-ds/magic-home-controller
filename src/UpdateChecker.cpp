#include "UpdateChecker.h"

#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QStringList>

namespace elkbledom {
namespace {

const QString RepositoryOwner = QStringLiteral("HelloGames-ds");
const QString RepositoryName = QStringLiteral("magic-home-controller");
const QString InstallerPrefix = QStringLiteral("Magic-Home-Controller-Setup-");

QNetworkRequest apiRequest(const QUrl& url)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
        QStringLiteral("MagicHomeController/%1").arg(UpdateChecker::currentVersion()));
    request.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/vnd.github+json"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(15000);
    return request;
}

QString cleanVersion(QString value)
{
    value = value.trimmed();
    if (value.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
        value.remove(0, 1);
    }
    return value;
}

QString lastPathSegment(const QUrl& url)
{
    const QString path = url.path();
    const int slash = path.lastIndexOf(QLatin1Char('/'));
    return slash >= 0 ? path.mid(slash + 1) : path;
}

} // namespace

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent)
    , network_(new QNetworkAccessManager(this))
{
}

UpdateChecker::~UpdateChecker() = default;

QString UpdateChecker::currentVersion()
{
    return QString::fromLatin1(APP_VERSION);
}

bool UpdateChecker::isNewer(const QString& candidate, const QString& current)
{
    const QStringList left = cleanVersion(candidate).split(QLatin1Char('.'));
    const QStringList right = cleanVersion(current).split(QLatin1Char('.'));
    const int count = qMax(left.size(), right.size());
    for (int i = 0; i < count; ++i) {
        const int a = i < left.size() ? left.at(i).toInt() : 0;
        const int b = i < right.size() ? right.at(i).toInt() : 0;
        if (a != b) {
            return a > b;
        }
    }
    return false;
}

void UpdateChecker::check()
{
    stopDownload();
    Q_EMIT checkStarted();
    const QUrl url(QStringLiteral("https://api.github.com/repos/%1/%2/releases/latest")
        .arg(RepositoryOwner, RepositoryName));
    reply_ = network_->get(apiRequest(url));
    connect(reply_, &QNetworkReply::finished, this, [this] {
        QNetworkReply* reply = reply_;
        reply_ = nullptr;
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            Q_EMIT failed(reply->errorString());
            return;
        }
        parseRelease(reply->readAll());
    });
}

void UpdateChecker::parseRelease(const QByteArray& payload)
{
    QJsonParseError parseError {};
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        Q_EMIT failed(QStringLiteral("Unexpected answer from GitHub"));
        return;
    }

    const QJsonObject release = document.object();
    const QString latest = cleanVersion(release.value(QStringLiteral("tag_name")).toString());
    if (latest.isEmpty()) {
        Q_EMIT failed(QStringLiteral("Release information is missing"));
        return;
    }

    const QString current = currentVersion();
    if (!isNewer(latest, current)) {
        Q_EMIT upToDate(current);
        return;
    }

    for (const QJsonValue& value : release.value(QStringLiteral("assets")).toArray()) {
        const QJsonObject asset = value.toObject();
        const QString name = asset.value(QStringLiteral("name")).toString();
        if (name.endsWith(QLatin1String(".sha256"))) {
            checksumUrl_ = asset.value(QStringLiteral("browser_download_url")).toString();
        } else if (name.endsWith(QLatin1String(".exe")) && name.startsWith(InstallerPrefix)) {
            assetUrl_ = asset.value(QStringLiteral("browser_download_url")).toString();
            assetName_ = name;
            expectedBytes_ = qint64(asset.value(QStringLiteral("size")).toDouble());
        }
    }

    if (assetUrl_.isEmpty()) {
        Q_EMIT failed(QStringLiteral("The release has no installer for Windows"));
        return;
    }

    version_ = latest;
    pageUrl_ = release.value(QStringLiteral("html_url")).toString();
    Q_EMIT updateAvailable(latest, pageUrl_, release.value(QStringLiteral("body")).toString().simplified());
}

void UpdateChecker::download(const QString& version)
{
    if (assetUrl_.isEmpty() || version != version_) {
        Q_EMIT failed(QStringLiteral("Update information is outdated, check again"));
        return;
    }
    startDownload(assetUrl_, version);
}

void UpdateChecker::startDownload(const QUrl& url, const QString& version)
{
    version_ = version;
    const QString fileName = lastPathSegment(url);
    if (fileName.isEmpty()) {
        Q_EMIT failed(QStringLiteral("Cannot determine the installer file name"));
        return;
    }

    const QString folder = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        .filePath(QStringLiteral("magic-home-controller"));
    QDir().mkpath(folder);
    const QString path = QDir(folder).filePath(fileName);

    stopDownload();
    file_ = new QFile(path, this);
    if (!file_->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        stopDownload();
        Q_EMIT failed(QStringLiteral("Cannot write %1").arg(path));
        return;
    }

    reply_ = network_->get(apiRequest(url));
    connect(reply_, &QNetworkReply::downloadProgress, this, &UpdateChecker::downloadProgress);
    connect(reply_, &QNetworkReply::readyRead, this, [this] {
        if (file_) {
            file_->write(reply_->readAll());
        }
    });
    connect(reply_, &QNetworkReply::finished, this, [this] {
        QNetworkReply* reply = reply_;
        reply_ = nullptr;
        if (file_) {
            file_->write(reply->readAll());
        }
        const bool failedDownload = reply->error() != QNetworkReply::NoError;
        reply->deleteLater();
        if (failedDownload) {
            failDownload();
            return;
        }
        if (file_) {
            file_->close();
        }
        downloadChecksum(checksumUrl_);
    });
}

void UpdateChecker::downloadChecksum(const QUrl& url)
{
    if (url.isEmpty()) {
        finishWithChecksum(QString());
        return;
    }

    reply_ = network_->get(apiRequest(url));
    connect(reply_, &QNetworkReply::finished, this, [this] {
        QNetworkReply* reply = reply_;
        reply_ = nullptr;
        const QByteArray payload = reply->readAll();
        const bool failedRequest = reply->error() != QNetworkReply::NoError;
        reply->deleteLater();
        if (failedRequest) {
            failDownload();
            return;
        }
        finishWithChecksum(readChecksum(payload, assetName_));
    });
}

QString UpdateChecker::readChecksum(const QByteArray& payload, const QString& fileName)
{
    for (const QString& line : QString::fromUtf8(payload).split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QLatin1Char('#'))) {
            continue;
        }
        const int separator = trimmed.indexOf(QLatin1Char('*'));
        if (separator < 0) {
            continue;
        }
        if (trimmed.mid(separator + 1).trimmed().compare(fileName, Qt::CaseInsensitive) == 0) {
            return trimmed.left(separator).trimmed().toLower();
        }
    }
    return QString();
}

void UpdateChecker::finishWithChecksum(const QString& expectedHex)
{
    const QString path = file_ ? file_->fileName() : QString();

    if (!expectedHex.isEmpty() && file_) {
        QCryptographicHash hash(QCryptographicHash::Sha256);
        if (file_->open(QIODevice::ReadOnly)) {
            hash.addData(file_->readAll());
            file_->close();
        }
        if (QString::fromLatin1(hash.result().toHex()) != expectedHex) {
            QFile::remove(path);
            failDownload();
            return;
        }
    }

    stopDownload();
    if (path.isEmpty()) {
        Q_EMIT failed(QStringLiteral("The installer was not downloaded"));
        return;
    }
    Q_EMIT updateReady(path, version_);
}

void UpdateChecker::openReleasePage()
{
    if (!pageUrl_.isEmpty()) {
        QDesktopServices::openUrl(pageUrl_);
    }
}

void UpdateChecker::failDownload()
{
    stopDownload();
    Q_EMIT failed(QStringLiteral("Download failed, please try again later"));
}

void UpdateChecker::stopDownload()
{
    if (reply_) {
        reply_->abort();
        reply_->deleteLater();
        reply_ = nullptr;
    }
    if (file_) {
        file_->close();
        file_->deleteLater();
        file_ = nullptr;
    }
}

} // namespace elkbledom
