#include "entrymodel.h"

EntryModel::EntryModel(const EntryContext *context, QObject *parent)
    : QAbstractListModel(parent), m_context(context) {}

int EntryModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : count(); }

QHash<int, QByteArray> EntryModel::roleNames() const {
    return {{KeyRole, "key"},           {KindRole, "kind"},         {NameRole, "name"},
            {LogoRole, "logo"},         {NumberRole, "number"},     {GroupNameRole, "groupName"},
            {PlotRole, "plot"},         {YearRole, "year"},         {RatingRole, "rating"},
            {SeasonRole, "season"},     {EpisodeRole, "episode"},   {FavoriteRole, "favorite"},
            {ProgressRole, "progress"}, {NowTitleRole, "nowTitle"}, {NowStartRole, "nowStart"},
            {NowStopRole, "nowStop"},   {NextTitleRole, "nextTitle"}, {NextStartRole, "nextStart"},
            {SeriesNameRole, "seriesName"}};
}

QVariant EntryModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_entries.size())
        return {};
    const Entry &e = m_entries.at(index.row());
    switch (role) {
    case KeyRole: return e.key;
    case KindRole: return Entry::kindName(e.kind);
    case NameRole: return e.name;
    case LogoRole: return e.logo;
    case NumberRole: return e.number;
    case GroupNameRole: return m_context->categoryName(e.group);
    case PlotRole: return e.plot;
    case YearRole: return e.year;
    case RatingRole: return e.rating;
    case SeasonRole: return e.season;
    case EpisodeRole: return e.episode;
    case FavoriteRole: return m_context->isFavorite(e.key);
    case ProgressRole: return m_context->progress(e.key);
    case SeriesNameRole: return e.kind == Entry::Episode ? m_context->seriesName(e) : QString();
    default: break;
    }
    if (e.kind != Entry::Live)
        return role == NowTitleRole || role == NextTitleRole ? QVariant(QString()) : QVariant(0.0);
    const NowNext guide = m_context->nowNext(e);
    switch (role) {
    case NowTitleRole: return guide.now ? guide.now->title : QString();
    case NowStartRole: return guide.now ? double(guide.now->start) : 0.0;
    case NowStopRole: return guide.now ? double(guide.now->stop) : 0.0;
    case NextTitleRole: return guide.next ? guide.next->title : QString();
    case NextStartRole: return guide.next ? double(guide.next->start) : 0.0;
    default: return {};
    }
}

void EntryModel::setEntries(QVector<Entry> entries) {
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
    emit countChanged();
}

void EntryModel::touch() {
    if (!m_entries.isEmpty())
        emit dataChanged(index(0), index(count() - 1), {FavoriteRole, ProgressRole, NowTitleRole, NowStartRole,
                                                         NowStopRole, NextTitleRole, NextStartRole});
}

QString EntryModel::keyAt(int row) const {
    return row >= 0 && row < m_entries.size() ? m_entries.at(row).key : QString();
}

int EntryModel::indexOf(const QString &key) const {
    for (int i = 0; i < m_entries.size(); ++i)
        if (m_entries.at(i).key == key)
            return i;
    return -1;
}

QVariantMap EntryModel::toMap(const Entry &e) {
    return {{"key", e.key},       {"kind", Entry::kindName(e.kind)}, {"name", e.name},
            {"logo", e.logo},     {"url", e.url},                    {"number", e.number},
            {"plot", e.plot},     {"year", e.year},                  {"rating", e.rating},
            {"season", e.season}, {"episode", e.episode},            {"seriesKey", e.seriesKey},
            {"duration", double(e.duration)}, {"source", e.sourceId}};
}
