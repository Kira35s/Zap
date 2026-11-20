#pragma once

#include <QAbstractListModel>
#include <QVariantMap>
#include <QVector>

#include "catalog.h"
#include "guide.h"

// What a row needs to know beyond the entry itself, answered by the Backend.
class EntryContext {
public:
    virtual ~EntryContext() = default;
    virtual qint64 now() const = 0;
    virtual bool isFavorite(const QString &key) const = 0;
    virtual double progress(const QString &key) const = 0;
    virtual NowNext nowNext(const Entry &entry) const = 0;
    virtual QString categoryName(const QString &id) const = 0;
    virtual QString seriesName(const Entry &episode) const = 0;
};

class EntryModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        KeyRole = Qt::UserRole + 1,
        KindRole,
        NameRole,
        LogoRole,
        NumberRole,
        GroupNameRole,
        PlotRole,
        YearRole,
        RatingRole,
        SeasonRole,
        EpisodeRole,
        FavoriteRole,
        ProgressRole,
        NowTitleRole,
        NowStartRole,
        NowStopRole,
        NextTitleRole,
        NextStartRole,
        SeriesNameRole,
    };

    explicit EntryModel(const EntryContext *context, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setEntries(QVector<Entry> entries);
    const QVector<Entry> &entries() const { return m_entries; }
    int count() const { return int(m_entries.size()); }
    // Re-reads the guide, favourites and progress for every row.
    void touch();

    Q_INVOKABLE QString keyAt(int row) const;
    Q_INVOKABLE int indexOf(const QString &key) const;

    static QVariantMap toMap(const Entry &entry);

signals:
    void countChanged();

private:
    const EntryContext *m_context;
    QVector<Entry> m_entries;
};
