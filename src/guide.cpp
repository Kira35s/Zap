#include "guide.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QRegularExpression>
#include <QTimeZone>
#include <QXmlStreamReader>
#include <algorithm>
#include <zlib.h>

namespace {

QString tr(const char *text) { return QCoreApplication::translate("Zap", text); }

// Inflates a gzip stream as it's read, so the XML reader never needs the
// whole (often several hundred megabyte) guide in memory.
class InflateDevice : public QIODevice {
public:
    explicit InflateDevice(QIODevice *source) : m_source(source) {
        m_ok = inflateInit2(&m_stream, 16 + MAX_WBITS) == Z_OK;
    }
    ~InflateDevice() override { inflateEnd(&m_stream); }

    bool isSequential() const override { return true; }
    bool atEnd() const override { return m_finished; }
    bool failed() const { return m_failed || !m_ok; }

protected:
    qint64 readData(char *data, qint64 maxSize) override {
        if (!m_ok || m_finished)
            return 0;
        const uInt want = uInt(std::min<qint64>(maxSize, 1 << 20));
        m_stream.next_out = reinterpret_cast<Bytef *>(data);
        m_stream.avail_out = want;
        while (m_stream.avail_out == want) {
            if (m_stream.avail_in == 0) {
                m_input = m_source->read(1 << 16);
                if (m_input.isEmpty()) {
                    m_finished = true;
                    break;
                }
                m_stream.next_in = reinterpret_cast<Bytef *>(m_input.data());
                m_stream.avail_in = uInt(m_input.size());
            }
            const int result = inflate(&m_stream, Z_NO_FLUSH);
            if (result == Z_STREAM_END) {
                // A gzip file can hold several members back to back.
                if (m_stream.avail_in == 0 && m_source->atEnd()) {
                    m_finished = true;
                    break;
                }
                inflateReset(&m_stream);
            } else if (result != Z_OK && result != Z_BUF_ERROR) {
                m_failed = true;
                m_finished = true;
                break;
            }
        }
        return qint64(want - m_stream.avail_out);
    }
    qint64 writeData(const char *, qint64) override { return -1; }

private:
    QIODevice *m_source;
    z_stream m_stream{};
    QByteArray m_input;
    bool m_ok = false;
    bool m_finished = false;
    bool m_failed = false;
};

} // namespace

qint64 Xmltv::parseTime(QStringView text) {
    text = text.trimmed();
    if (text.size() < 12)
        return 0;
    bool ok = true;
    auto number = [&](qsizetype position, qsizetype length) {
        bool valid = false;
        const int value = text.mid(position, length).toInt(&valid);
        ok = ok && valid;
        return value;
    };
    const int year = number(0, 4), month = number(4, 2), day = number(6, 2);
    const int hour = number(8, 2), minute = number(10, 2);
    const int second = text.size() >= 14 && text[12].isDigit() ? number(12, 2) : 0;
    if (!ok)
        return 0;
    const QDateTime time(QDate(year, month, day), QTime(hour, minute, second), QTimeZone::UTC);
    if (!time.isValid())
        return 0;

    // "20240101200000 +0100": the offset says how far the local time is
    // ahead of UTC. Without one, XMLTV times are UTC.
    qint64 offset = 0;
    const qsizetype sign = std::max(text.lastIndexOf(u'+'), text.lastIndexOf(u'-'));
    if (sign >= 12 && text.size() >= sign + 5) {
        const int hours = text.mid(sign + 1, 2).toInt(), minutes = text.mid(sign + 3, 2).toInt();
        offset = qint64(hours * 3600 + minutes * 60) * (text[sign] == u'-' ? -1 : 1);
    }
    return time.toSecsSinceEpoch() - offset;
}

QString Xmltv::normalize(const QString &name) {
    QString s = foldText(name);
    static const QRegularExpression prefix(R"re(^\s*[|\[(]?\s*[a-z]{2,3}\s*[|\]):]\s*)re");
    static const QRegularExpression brackets(R"re([\[(][^\])]*[\])])re");
    static const QRegularExpression quality(R"re(\b(uhd|fhd|hd|sd|4k|hevc|h265|1080p?|720p?|50fps|backup)\b)re");
    static const QRegularExpression countrySuffix(R"re(\.[a-z]{2}$)re");
    static const QRegularExpression other(R"re([^\p{L}\p{N}]+)re");
    s.remove(prefix);
    s.remove(brackets);
    s.remove(quality);
    s.remove(countrySuffix);
    s.remove(other);
    return s;
}

EpgData Xmltv::parse(const QString &path, qint64 from, qint64 to) {
    EpgData data;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        data.errors << tr("The programme guide couldn't be opened.");
        return data;
    }
    const bool gzip = file.peek(2) == QByteArray("\x1f\x8b", 2);
    InflateDevice inflater(&file);
    if (gzip)
        inflater.open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    QXmlStreamReader xml(gzip ? static_cast<QIODevice *>(&inflater) : &file);

    auto addName = [&](const QString &name, const QString &id) {
        const QString key = normalize(name);
        if (!key.isEmpty() && !data.names.contains(key))
            data.names.insert(key, id);
    };

    while (!xml.atEnd()) {
        if (xml.readNext() != QXmlStreamReader::StartElement)
            continue;
        if (xml.name() == u"channel") {
            const QString id = xml.attributes().value("id").toString();
            while (xml.readNextStartElement()) {
                if (xml.name() == u"display-name")
                    addName(xml.readElementText(), id);
                else
                    xml.skipCurrentElement();
            }
            addName(id, id);
        } else if (xml.name() == u"programme") {
            const auto attributes = xml.attributes();
            const QString channel = attributes.value("channel").toString();
            Programme p;
            p.start = parseTime(attributes.value("start"));
            p.stop = parseTime(attributes.value("stop"));
            if (channel.isEmpty() || p.start == 0 || p.start > to || (p.stop != 0 && p.stop < from)) {
                xml.skipCurrentElement();
                continue;
            }
            while (xml.readNextStartElement()) {
                if (xml.name() == u"title" && p.title.isEmpty())
                    p.title = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
                else if (xml.name() == u"desc" && p.desc.isEmpty())
                    p.desc = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
                else
                    xml.skipCurrentElement();
            }
            data.channels[channel].append(p);
        }
    }
    if (inflater.failed() && gzip)
        data.errors << tr("The programme guide is damaged.");
    else if (xml.hasError())
        data.errors << tr("The programme guide is damaged: %1").arg(xml.errorString());

    for (auto it = data.channels.begin(); it != data.channels.end(); ++it) {
        auto &list = it.value();
        std::stable_sort(list.begin(), list.end(), [](const Programme &a, const Programme &b) { return a.start < b.start; });
        // A programme without a stop time runs until the next one starts.
        for (qsizetype i = 0; i < list.size(); ++i)
            if (list[i].stop <= list[i].start)
                list[i].stop = i + 1 < list.size() ? list[i + 1].start : list[i].start + 3600;
        data.lowerIds.insert(it.key().toLower(), it.key());
    }
    return data;
}

const QVector<Programme> *Guide::find(const Entry &channel) const {
    const auto source = m_sources.constFind(channel.sourceId);
    if (source == m_sources.cend())
        return nullptr;
    auto lookup = [&](const QString &id) -> const QVector<Programme> * {
        const auto found = source->channels.constFind(id);
        return found == source->channels.cend() ? nullptr : &*found;
    };
    if (!channel.epgId.isEmpty()) {
        if (const auto *list = lookup(channel.epgId))
            return list;
        const auto lower = source->lowerIds.constFind(channel.epgId.toLower());
        if (lower != source->lowerIds.cend())
            return lookup(*lower);
    }
    const auto named = source->names.constFind(Xmltv::normalize(channel.name));
    return named == source->names.cend() ? nullptr : lookup(*named);
}

NowNext Guide::at(const Entry &channel, qint64 time) const {
    NowNext result;
    const auto *list = find(channel);
    if (!list || list->isEmpty())
        return result;
    // The first programme starting after `time`; the one before it is on now
    // if it hasn't ended.
    const auto after = std::upper_bound(list->cbegin(), list->cend(), time,
                                        [](qint64 t, const Programme &p) { return t < p.start; });
    if (after != list->cbegin() && std::prev(after)->stop > time)
        result.now = &*std::prev(after);
    if (after != list->cend())
        result.next = &*after;
    return result;
}

QVector<Programme> Guide::between(const Entry &channel, qint64 from, qint64 to) const {
    QVector<Programme> result;
    if (const auto *list = find(channel))
        for (const auto &p : *list)
            if (p.stop > from && p.start < to)
                result.append(p);
    return result;
}
