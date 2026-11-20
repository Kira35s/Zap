#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <functional>

// Fetches a web address or copies a local file into the cache, atomically,
// so a failed refresh never leaves a half-written list behind.
class Downloader : public QObject {
    Q_OBJECT

public:
    explicit Downloader(QObject *parent = nullptr);

    // `done` gets an empty string on success, or what went wrong.
    void fetch(const QString &address, const QString &path, std::function<void(const QString &)> done);

private:
    QNetworkAccessManager m_manager;
};
