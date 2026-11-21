#include "catalog.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>
#include <algorithm>
#include <cmath>

namespace {

QString tr(const char *text) { return QCoreApplication::translate("Zap", text); }

QString shortHash(const QString &text) {
    return QString::fromLatin1(QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha1).toHex().left(16));
}

// Xtream panels send ids and numbers as strings on some servers and as
// numbers on others, so read either.
QString text(const QJsonValue &value) {
    if (value.isString())
        return value.toString().trimmed();
    if (value.isDouble()) {
        const double number = value.toDouble();
        return number == std::floor(number) ? QString::number(qint64(number)) : QString::number(number);
    }
    return {};
}

QChar letter(Entry::Kind kind) {
    switch (kind) {
    case Entry::Live: return 'l';
    case Entry::Movie: return 'm';
    case Entry::Series: return 's';
    case Entry::Episode: return 'e';
    }
    return 'l';
}

// "Show - S01E02 - Title" becomes "Title": the series and the numbers are
// shown beside it anyway.
QString episodeTitle(const QString &title, const QString &seriesName) {
    static const QRegularExpression numbering(R"re(^[\s\-–:·|]*S\d{1,2}\s*E\d{1,4}[\s\-–:·|]*)re",
                                              QRegularExpression::CaseInsensitiveOption);
    QString name = title;
    if (!seriesName.isEmpty() && name.startsWith(seriesName, Qt::CaseInsensitive))
        name = name.mid(seriesName.size());
    name.remove(numbering);
    name = name.trimmed();
    return name.isEmpty() ? title : name;
}

QString encode(const QString &value) { return QString::fromUtf8(QUrl::toPercentEncoding(value)); }

class CategoryList {
public:
    explicit CategoryList(Catalog &catalog) : m_catalog(catalog) {}

    QString add(const QString &sourceId, Entry::Kind kind, const QString &raw, const QString &name) {
        const QString id = sourceId + '|' + letter(kind) + '|' + raw;
        if (!m_seen.contains(id)) {
            m_seen.insert(id);
            m_catalog.categories.append({id, sourceId, name, kind});
        }
        return id;
    }

private:
    Catalog &m_catalog;
    QSet<QString> m_seen;
};

} // namespace

QString foldText(const QString &text) {
    const QString decomposed = text.normalized(QString::NormalizationForm_D);
    QString folded;
    folded.reserve(decomposed.size());
    for (const QChar c : decomposed)
        if (c.category() != QChar::Mark_NonSpacing)
            folded.append(c);
    return folded.toCaseFolded();
}

QString Entry::kindName(Kind kind) {
    switch (kind) {
    case Live: return "live";
    case Movie: return "movie";
    case Series: return "series";
    case Episode: return "episode";
    }
    return "live";
}

QJsonObject Entry::toJson() const {
    return {{"kind", kindName(kind)}, {"key", key},         {"source", sourceId}, {"id", id},
            {"name", name},           {"logo", logo},       {"url", url},         {"group", group},
            {"seriesKey", seriesKey}, {"season", season},   {"episode", episode}, {"epgId", epgId}};
}

Entry Entry::fromJson(const QJsonObject &o) {
    Entry e;
    const QString kind = o.value("kind").toString();
    e.kind = kind == "movie" ? Movie : kind == "series" ? Series : kind == "episode" ? Episode : Live;
    e.key = o.value("key").toString();
    e.sourceId = o.value("source").toString();
    e.id = o.value("id").toString();
    e.name = o.value("name").toString();
    e.search = foldText(e.name);
    e.logo = o.value("logo").toString();
    e.url = o.value("url").toString();
    e.group = o.value("group").toString();
    e.seriesKey = o.value("seriesKey").toString();
    e.season = o.value("season").toInt();
    e.episode = o.value("episode").toInt();
    e.epgId = o.value("epgId").toString();
    return e;
}

QString Source::baseUrl() const {
    QString address = server.trimmed();
    if (address.isEmpty())
        return {};
    if (!address.contains("://"))
        address.prepend("http://");
    // Providers often hand out a full get.php or player_api.php link; only the
    // scheme, host and port matter.
    QUrl url(address);
    if (url.isValid() && !url.host().isEmpty()) {
        if (url.path().endsWith(".php"))
            url.setPath({});
        url.setQuery(QString());
        url.setFragment({});
        address = url.toString();
    }
    while (address.endsWith('/'))
        address.chop(1);
    return address;
}

QString Source::xtreamApi(const QString &action, const QString &extra) const {
    return baseUrl() + "/player_api.php?username=" + encode(username) + "&password=" + encode(password) +
           "&action=" + action + extra;
}

QString Source::xtreamStream(const QString &kind, const QString &streamId, const QString &extension) const {
    return baseUrl() + '/' + kind + '/' + encode(username) + '/' + encode(password) + '/' + streamId + '.' +
           (extension.isEmpty() ? QStringLiteral("mp4") : extension);
}

QString Source::guideUrl(const QString &hint) const {
    if (!epg.trimmed().isEmpty())
        return epg.trimmed();
    if (isXtream())
        return baseUrl() + "/xmltv.php?username=" + encode(username) + "&password=" + encode(password);
    return hint;
}

QJsonObject Source::toJson() const {
    return {{"id", id},           {"name", name},         {"type", type},         {"server", server},
            {"username", username}, {"password", password}, {"playlist", playlist}, {"epg", epg}};
}

Source Source::fromJson(const QJsonObject &o) {
    Source s;
    s.id = o.value("id").toString();
    s.name = o.value("name").toString();
    s.type = o.value("type").toString() == "m3u" ? "m3u" : "xtream";
    s.server = o.value("server").toString();
    s.username = o.value("username").toString();
    s.password = o.value("password").toString();
    s.playlist = o.value("playlist").toString();
    s.epg = o.value("epg").toString();
    return s;
}

Catalog CatalogParser::parseM3u(const QByteArray &data, const QString &sourceId) {
    Catalog catalog;
    CategoryList categories(catalog);
    static const QRegularExpression attribute(R"re(([A-Za-z0-9_-]+)\s*=\s*"([^"]*)")re");
    static const QRegularExpression episodeName(R"re(^(.*?)[\s._|-]*S(\d{1,2})[\s._-]*E(\d{1,4})\b)re",
                                                QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression videoFile(R"re(\.(mp4|mkv|avi|mov|m4v|wmv)(\?.*)?$)re",
                                              QRegularExpression::CaseInsensitiveOption);

    QHash<QString, QString> attributes;
    QString title, group;
    QHash<QString, int> seriesIndex;
    int number = 0;

    const QString content = QString::fromUtf8(data);
    for (QString line : content.split('\n')) {
        line = line.trimmed();
        if (line.isEmpty())
            continue;
        if (line.startsWith("#EXTM3U")) {
            for (const auto &match : attribute.globalMatch(line)) {
                const QString name = match.captured(1).toLower();
                if ((name == "url-tvg" || name == "x-tvg-url") && catalog.epgHint.isEmpty())
                    catalog.epgHint = match.captured(2).section(',', 0, 0).trimmed();
            }
            continue;
        }
        if (line.startsWith("#EXTINF")) {
            attributes.clear();
            // The title follows the first comma outside the quoted attributes,
            // and may itself contain commas.
            const qsizetype lastQuote = line.lastIndexOf('"');
            const qsizetype comma = line.indexOf(',', lastQuote < 0 ? 0 : lastQuote);
            const QString head = comma < 0 ? line : line.left(comma);
            for (const auto &match : attribute.globalMatch(head))
                attributes.insert(match.captured(1).toLower(), match.captured(2).trimmed());
            title = comma < 0 ? QString() : line.mid(comma + 1).trimmed();
            group = attributes.value("group-title");
            continue;
        }
        if (line.startsWith("#EXTGRP:")) {
            if (group.isEmpty())
                group = line.mid(8).trimmed();
            continue;
        }
        // Anything else must be a stream address; an HTML error page served in
        // place of the playlist must not turn into channels.
        if (line.startsWith('#') || !(line.contains("://") || line.startsWith('/'))) {
            attributes.clear();
            title.clear();
            group.clear();
            continue;
        }

        Entry e;
        e.sourceId = sourceId;
        e.url = line;
        e.id = shortHash(line);
        e.name = !title.isEmpty() ? title : attributes.value("tvg-name", line.section('/', -1));
        e.logo = attributes.value("tvg-logo");
        e.epgId = attributes.value("tvg-id");
        const QString lower = line.toLower();
        const auto episodeMatch = episodeName.match(e.name);

        if (lower.contains("/series/") || (videoFile.match(line).hasMatch() && episodeMatch.hasMatch())) {
            e.kind = Entry::Episode;
            const QString seriesName = episodeMatch.hasMatch() && !episodeMatch.captured(1).trimmed().isEmpty()
                                           ? episodeMatch.captured(1).trimmed()
                                           : e.name;
            e.season = episodeMatch.hasMatch() ? episodeMatch.captured(2).toInt() : 1;
            e.episode = episodeMatch.hasMatch() ? episodeMatch.captured(3).toInt() : 0;
            e.key = sourceId + ":e:" + e.id;
            e.seriesKey = sourceId + ":s:" + shortHash(group + '|' + seriesName.toLower());
            e.group = categories.add(sourceId, Entry::Series, group, group);
            if (!seriesIndex.contains(e.seriesKey)) {
                Entry series;
                series.kind = Entry::Series;
                series.sourceId = sourceId;
                series.id = e.seriesKey.section(':', 2);
                series.key = e.seriesKey;
                series.name = seriesName;
                series.logo = e.logo;
                series.group = e.group;
                seriesIndex.insert(e.seriesKey, catalog.series.size());
                catalog.series.append(series);
            }
            catalog.episodes[e.seriesKey].append(e);
        } else if (lower.contains("/movie/") || videoFile.match(line).hasMatch()) {
            e.kind = Entry::Movie;
            e.key = sourceId + ":m:" + e.id;
            e.group = categories.add(sourceId, Entry::Movie, group, group);
            catalog.movies.append(e);
        } else {
            e.kind = Entry::Live;
            e.number = attributes.value("tvg-chno").toInt();
            if (e.number <= 0)
                e.number = number + 1;
            number = e.number;
            e.key = sourceId + ":l:" + e.id;
            e.group = categories.add(sourceId, Entry::Live, group, group);
            catalog.live.append(e);
        }
        attributes.clear();
        title.clear();
        group.clear();
    }

    for (auto &episodes : catalog.episodes)
        std::stable_sort(episodes.begin(), episodes.end(), [](const Entry &a, const Entry &b) {
            return a.season != b.season ? a.season < b.season : a.episode < b.episode;
        });
    if (catalog.live.isEmpty() && catalog.movies.isEmpty() && catalog.series.isEmpty() && !data.isEmpty())
        catalog.errors << tr("The playlist has no channels in it. Is it an M3U file?");
    return catalog;
}

QStringList CatalogParser::xtreamActions() {
    return {"get_live_categories", "get_live_streams",      "get_vod_categories",
            "get_vod_streams",     "get_series_categories", "get_series"};
}

Catalog CatalogParser::parseXtream(const Source &source, const QHash<QString, QByteArray> &files) {
    Catalog catalog;
    CategoryList categories(catalog);

    const QJsonDocument liveCheck = QJsonDocument::fromJson(files.value("get_live_streams"));
    if (liveCheck.isObject()) {
        catalog.errors << tr("The server refused the username or password.");
        return catalog;
    }

    struct Section {
        Entry::Kind kind;
        const char *categoriesFile;
        const char *streamsFile;
    };
    const Section sections[] = {{Entry::Live, "get_live_categories", "get_live_streams"},
                                {Entry::Movie, "get_vod_categories", "get_vod_streams"},
                                {Entry::Series, "get_series_categories", "get_series"}};

    for (const auto &section : sections) {
        QHash<QString, QString> names;
        const QJsonArray categoryArray = QJsonDocument::fromJson(files.value(section.categoriesFile)).array();
        for (const auto &value : categoryArray) {
            const QJsonObject o = value.toObject();
            const QString raw = text(o.value("category_id"));
            names.insert(raw, text(o.value("category_name")));
            categories.add(source.id, section.kind, raw, names.value(raw));
        }

        const QByteArray streamsData = files.value(section.streamsFile);
        const QJsonDocument streams = QJsonDocument::fromJson(streamsData);
        if (!streamsData.isEmpty() && !streams.isArray())
            catalog.errors << tr("The provider's list “%1” couldn't be read.").arg(section.streamsFile);

        for (const auto &value : streams.array()) {
            const QJsonObject o = value.toObject();
            Entry e;
            e.kind = section.kind;
            e.sourceId = source.id;
            e.name = text(o.value("name"));
            const QString raw = text(o.value("category_id"));
            e.group = categories.add(source.id, section.kind, raw, names.value(raw));
            e.rating = text(o.value("rating"));
            if (e.rating == "0")
                e.rating.clear();
            if (section.kind == Entry::Live) {
                e.id = text(o.value("stream_id"));
                e.key = source.id + ":l:" + e.id;
                e.logo = text(o.value("stream_icon"));
                e.epgId = text(o.value("epg_channel_id"));
                e.number = text(o.value("num")).toInt();
                e.url = source.xtreamStream("live", e.id, "ts");
                catalog.live.append(e);
            } else if (section.kind == Entry::Movie) {
                e.id = text(o.value("stream_id"));
                e.key = source.id + ":m:" + e.id;
                e.logo = text(o.value("stream_icon"));
                e.year = text(o.value("year"));
                e.url = source.xtreamStream("movie", e.id, text(o.value("container_extension")));
                catalog.movies.append(e);
            } else {
                e.id = text(o.value("series_id"));
                e.key = source.id + ":s:" + e.id;
                e.logo = text(o.value("cover"));
                e.plot = text(o.value("plot"));
                e.year = text(o.value("year"));
                if (e.year.isEmpty())
                    e.year = text(o.value("releaseDate")).left(4);
                catalog.series.append(e);
            }
        }
    }
    return catalog;
}

QVariantMap Details::toMap() const {
    QStringList people;
    for (const QString &name : cast.split(','))
        if (!name.trimmed().isEmpty() && !people.contains(name.trimmed()))
            people << name.trimmed();
    return {{"plot", plot},         {"cast", people},     {"director", director}, {"genre", genre},
            {"released", released}, {"country", country}, {"backdrop", backdrop}, {"cover", cover},
            {"detailRating", rating}, {"trailer", trailer}, {"runtime", double(duration)}};
}

Details CatalogParser::parseXtreamDetails(const QJsonObject &info) {
    // Panels disagree on the names, so take the first one that's there.
    auto first = [&](std::initializer_list<const char *> names) {
        for (const char *name : names) {
            const QJsonValue value = info.value(QLatin1String(name));
            const QJsonArray list = value.toArray();
            const QString found = value.isArray() ? (list.isEmpty() ? QString() : text(list.first())) : text(value);
            if (!found.isEmpty() && found != "N/A" && found != "null")
                return found;
        }
        return QString();
    };
    Details d;
    d.plot = first({"plot", "description"});
    d.cast = first({"cast", "actors"});
    d.director = first({"director"});
    d.genre = first({"genre"});
    d.released = first({"releasedate", "releaseDate", "release_date"});
    d.country = first({"country"});
    d.backdrop = first({"backdrop_path", "backdrop"});
    d.cover = first({"cover_big", "movie_image", "cover"});
    d.rating = first({"rating"});
    if (d.rating == "0")
        d.rating.clear();
    d.trailer = first({"youtube_trailer"});
    d.duration = first({"duration_secs"}).toLongLong();
    if (d.duration <= 0) {
        // "01:52:00", or minutes for series' episode_run_time.
        const QStringList parts = first({"duration"}).split(':');
        if (parts.size() == 3)
            d.duration = parts[0].toLongLong() * 3600 + parts[1].toLongLong() * 60 + parts[2].toLongLong();
        else
            d.duration = first({"episode_run_time"}).toLongLong() * 60;
    }
    return d;
}

Details CatalogParser::parseXtreamVodInfo(const QByteArray &data) {
    const QJsonValue info = QJsonDocument::fromJson(data).object().value("info");
    return info.isObject() ? parseXtreamDetails(info.toObject()) : Details();
}

QVector<Entry> CatalogParser::parseXtreamSeriesInfo(const Source &source, const Entry &series,
                                                    const QByteArray &data, Details *details) {
    QVector<Entry> episodes;
    const QJsonObject root = QJsonDocument::fromJson(data).object();
    if (details)
        *details = parseXtreamDetails(root.value("info").toObject());

    // "episodes" is an object keyed by season on most panels, and an array
    // of seasons on a few.
    QList<QPair<int, QJsonArray>> seasons;
    const QJsonValue all = root.value("episodes");
    if (all.isObject()) {
        const QJsonObject o = all.toObject();
        for (auto it = o.begin(); it != o.end(); ++it)
            seasons.append({it.key().toInt(), it.value().toArray()});
    } else {
        int index = 1;
        for (const auto &season : all.toArray())
            seasons.append({index++, season.toArray()});
    }

    for (const auto &[seasonNumber, list] : seasons) {
        for (const auto &value : list) {
            const QJsonObject o = value.toObject();
            const QJsonObject info = o.value("info").toObject();
            Entry e;
            e.kind = Entry::Episode;
            e.sourceId = source.id;
            e.id = text(o.value("id"));
            e.key = source.id + ":e:" + e.id;
            e.seriesKey = series.key;
            e.group = series.group;
            e.name = episodeTitle(text(o.value("title")), series.name);
            e.season = text(o.value("season")).toInt();
            if (e.season == 0)
                e.season = seasonNumber;
            e.episode = text(o.value("episode_num")).toInt();
            e.plot = text(info.value("plot"));
            e.logo = text(info.value("movie_image"));
            if (e.logo.isEmpty())
                e.logo = series.logo;
            e.duration = text(info.value("duration_secs")).toLongLong();
            e.rating = text(info.value("rating"));
            e.url = source.xtreamStream("series", e.id, text(o.value("container_extension")));
            if (!e.id.isEmpty())
                episodes.append(e);
        }
    }
    std::stable_sort(episodes.begin(), episodes.end(), [](const Entry &a, const Entry &b) {
        return a.season != b.season ? a.season < b.season : a.episode < b.episode;
    });
    return episodes;
}

Catalog CatalogParser::load(const Source &source, const QString &directory) {
    auto read = [&](const QString &name) {
        QFile file(directory + '/' + name);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    };

    Catalog catalog;
    if (source.isXtream()) {
        QHash<QString, QByteArray> files;
        for (const auto &action : xtreamActions())
            files.insert(action, read(action + ".json"));
        catalog = parseXtream(source, files);
    } else {
        catalog = parseM3u(read("playlist.m3u"), source.id);
    }

    for (auto *list : {&catalog.live, &catalog.movies, &catalog.series})
        for (auto &e : *list)
            e.search = foldText(e.name);
    return catalog;
}
