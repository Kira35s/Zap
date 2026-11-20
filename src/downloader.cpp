#include "downloader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <QSaveFile>
#include <QTimer>
#include <QUrl>
#include <memory>

Downloader::Downloader(QObject *parent) : QObject(parent) {}

void Downloader::fetch(const QString &address, const QString &path, std::function<void(const QString &)> done) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    auto finish = [this, done](const QString &error) { QTimer::singleShot(0, this, [done, error] { done(error); }); };

    QString local = address.trimmed();
    if (local.startsWith("file://"))
        local = QUrl(local).toLocalFile();
    else if (local.startsWith("~/"))
        local = QDir::homePath() + local.mid(1);

    if (local.startsWith('/')) {
        QFile in(local);
        if (!in.open(QIODevice::ReadOnly))
            return finish(tr("%1 can't be read: %2").arg(local, in.errorString()));
        QSaveFile out(path);
        if (!out.open(QIODevice::WriteOnly))
            return finish(tr("the cache can't be written: %1").arg(out.errorString()));
        while (!in.atEnd())
            out.write(in.read(1 << 20));
        return finish(out.commit() ? QString() : tr("the cache can't be written: %1").arg(out.errorString()));
    }

    const QUrl url(local);
    if (!url.isValid() || (url.scheme() != "http" && url.scheme() != "https"))
        return finish(tr("this isn't a web address or a file path"));

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("zap/0.1"));
    request.setTransferTimeout(120000);

    auto out = std::make_shared<QSaveFile>(path);
    if (!out->open(QIODevice::WriteOnly))
        return finish(tr("the cache can't be written: %1").arg(out->errorString()));

    auto written = std::make_shared<qint64>(0);
    QNetworkReply *reply = m_manager.get(request);
    connect(reply, &QNetworkReply::readyRead, this, [reply, out, written] {
        *written += out->write(reply->readAll());
    });
    connect(reply, &QNetworkReply::finished, this, [reply, out, written, done] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError) {
            // Never committing the save file throws the partial download away.
            done(status >= 400 ? tr("the server answered %1").arg(status) : reply->errorString());
            return;
        }
        *written += out->write(reply->readAll());
        if (*written == 0) {
            done(tr("the server sent nothing"));
            return;
        }
        done(out->commit() ? QString() : tr("the cache can't be written: %1").arg(out->errorString()));
    });
}
