#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QStringView>
#include <QVector>

#include "catalog.h"

struct Programme {
    qint64 start = 0; // seconds since the epoch, UTC
    qint64 stop = 0;
    QString title;
    QString desc;
};

struct EpgData {
    QHash<QString, QVector<Programme>> channels; // XMLTV channel id -> programmes by start
    QHash<QString, QString> names;               // normalized display name -> channel id
    QHash<QString, QString> lowerIds;            // lowercased channel id -> channel id
    QStringList errors;
};

namespace Xmltv {
// Reads a plain or gzipped XMLTV file, keeping only programmes that overlap
// [from, to] so a week-long guide for thousands of channels stays small.
EpgData parse(const QString &path, qint64 from, qint64 to);
qint64 parseTime(QStringView text);
// Strips what providers decorate channel names with ("FR: TF1 HD") so a
// playlist name can find its guide channel.
QString normalize(const QString &name);
}

struct NowNext {
    const Programme *now = nullptr;
    const Programme *next = nullptr;
};

// Every source's guide, and the lookup from a channel to its programmes.
class Guide {
public:
    void set(const QString &sourceId, EpgData data) { m_sources.insert(sourceId, std::move(data)); }
    void remove(const QString &sourceId) { m_sources.remove(sourceId); }
    bool isEmpty() const { return m_sources.isEmpty(); }

    const QVector<Programme> *find(const Entry &channel) const;
    NowNext at(const Entry &channel, qint64 time) const;
    QVector<Programme> between(const Entry &channel, qint64 from, qint64 to) const;

private:
    QHash<QString, EpgData> m_sources;
};
