#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

// One thing the user can pick: a live channel, a movie, a series, or one of
// its episodes. Keys stay the same across refreshes, so favourites and resume
// points survive a provider reshuffling its lists.
struct Entry {
    enum Kind { Live, Movie, Series, Episode };

    Kind kind = Live;
    QString key;       // "<source>:<kind letter>:<id>"
    QString sourceId;
    QString id;        // the provider's stream or series id, or a hash of the M3U URL
    QString name;
    QString search;    // name folded for accent- and case-insensitive search
    QString logo;
    QString url;
    QString group;     // category id, "<source>|<kind letter>|<provider category>"
    QString epgId;
    QString plot;
    QString year;
    QString rating;
    QString seriesKey; // episodes point back at their series
    int number = 0;
    int season = 0;
    int episode = 0;
    qint64 duration = 0; // seconds, when the provider says

    static QString kindName(Kind kind);
    QJsonObject toJson() const;
    static Entry fromJson(const QJsonObject &object);
};

struct Category {
    QString id;
    QString sourceId;
    QString name;
    Entry::Kind kind = Entry::Live;
};

// A provider: either an Xtream Codes account or an M3U playlist, each with an
// optional XMLTV guide address.
struct Source {
    QString id;
    QString name;
    QString type = "xtream"; // "xtream" or "m3u"
    QString server;
    QString username;
    QString password;
    QString playlist;
    QString epg;

    bool isXtream() const { return type == "xtream"; }
    QString baseUrl() const;
    QString xtreamApi(const QString &action, const QString &extra = {}) const;
    QString xtreamStream(const QString &type, const QString &id, const QString &extension) const;
    QString guideUrl(const QString &hint = {}) const;

    QJsonObject toJson() const;
    static Source fromJson(const QJsonObject &object);
};

// What a provider says about a movie or a series beyond its list entry.
struct Details {
    QString plot;
    QString cast;     // comma-separated, as providers send it
    QString director;
    QString genre;
    QString released; // a date or a year
    QString country;
    QString backdrop;
    QString cover;
    QString rating;
    QString trailer;  // a YouTube video id
    qint64 duration = 0; // seconds

    bool isEmpty() const { return plot.isEmpty() && cast.isEmpty() && director.isEmpty() && genre.isEmpty(); }
    QVariantMap toMap() const;
};

struct Catalog {
    QVector<Entry> live;
    QVector<Entry> movies;
    QVector<Entry> series;
    QHash<QString, QVector<Entry>> episodes; // M3U only; Xtream episodes load on demand
    QVector<Category> categories;            // in the provider's order
    QString epgHint;                         // url-tvg from an M3U header
    QStringList errors;
};

// Lowercase, without accents, so "tele" finds "Télé".
QString foldText(const QString &text);

namespace CatalogParser {
Catalog parseM3u(const QByteArray &data, const QString &sourceId);
Catalog parseXtream(const Source &source, const QHash<QString, QByteArray> &files);
QVector<Entry> parseXtreamSeriesInfo(const Source &source, const Entry &series, const QByteArray &data,
                                     Details *details = nullptr);
// The "info" object of get_vod_info or get_series_info.
Details parseXtreamDetails(const QJsonObject &info);
Details parseXtreamVodInfo(const QByteArray &data);
// The file names an Xtream source downloads, without ".json".
QStringList xtreamActions();
// Reads whatever a source has downloaded into its cache directory.
Catalog load(const Source &source, const QString &directory);
}
