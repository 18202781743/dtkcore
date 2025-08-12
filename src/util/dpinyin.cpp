// SPDX-FileCopyrightText: 2019 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dpinyin.h"

#include <QFile>
#include <QSet>
#include <QTextStream>
#include <QMap>
#include <QDebug>
#include <QLoggingCategory>

DCORE_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(logUtil, "dtk.core.util")

static QHash<uint, QString> dict = {};
const char kDictFile[] = ":/dpinyin/resources/dpinyin.dict";

static void InitDict() {
    qCDebug(logUtil) << "Initializing pinyin dictionary";
    if (!dict.isEmpty()) {
        qCDebug(logUtil) << "Dictionary already initialized";
        return;
    }

    dict.reserve(25333);
    qCDebug(logUtil) << "Dictionary reserved for 25333 entries";

    QFile file(kDictFile);
    qCDebug(logUtil) << "Opening dictionary file:" << kDictFile;

    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(logUtil) << "Failed to open dictionary file:" << kDictFile;
        return;
    }

    QByteArray content = file.readAll();
    qCDebug(logUtil) << "Read" << content.size() << "bytes from dictionary file";

    file.close();

    QTextStream stream(&content, QIODevice::ReadOnly);

    int lineCount = 0;
    int validEntries = 0;
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        lineCount++;
        // comment
        if (line.startsWith("#"))
            continue;
        const QStringList items = line.left(line.indexOf("#")).split(QChar(':'));

        if (items.size() == 2) {
            dict.insert(items[0].toUInt(nullptr, 16), items[1].trimmed());
            validEntries++;
        }
    }
    qCDebug(logUtil) << "Dictionary initialized with" << validEntries << "valid entries from" << lineCount << "lines";
}

static void initToneTable(QMap<QChar, QString> &toneTable)
{
    qCDebug(logUtil) << "Initializing tone table";
    if (toneTable.size() > 0) {
        qCDebug(logUtil) << "Tone table already initialized";
        return;
    }

    const QString ts = "aāáǎà,oōóǒò,eēéěè,iīíǐì,uūúǔù,vǖǘǚǜ";

    for (const QString &s : ts.split(",")) {
        for (int i = 1; i < s.length(); ++i) {
            toneTable.insert(s.at(i), QString("%1%2").arg(s.at(0)).arg(i));
        }
    }
    qCDebug(logUtil) << "Tone table initialized with" << toneTable.size() << "entries";
}

static QString toned(const QString &str, ToneStyle ts)
{
    qCDebug(logUtil) << "Converting tone for string:" << str << "style:" << ts;
    // TS_Tone is default
    if (ts == TS_Tone) {
        qCDebug(logUtil) << "Using default tone style, returning original string";
        return str;
    }

    static QMap<QChar, QString> toneTable; // {ā, a1}
    initToneTable(toneTable);

    QString newStr = str;
    QString cv;
    for (QChar c : str) {
        if (!toneTable.contains(c)) {
            qCDebug(logUtil) << "Character not in tone table:" << c.toLatin1();
            continue;
        }

        cv = toneTable.value(c);
        qCDebug(logUtil) << "Processing character:" << c.toLatin1() << "tone value:" << cv;
        switch (ts) {
        case TS_NoneTone:
            qCDebug(logUtil) << "Using no tone style";
            newStr.replace(c, cv.left(1));
            break;
        case TS_ToneNum:
            qCDebug(logUtil) << "Using tone number style";
            newStr.replace(c, cv);
            break;
        default:
            qCDebug(logUtil) << "Unknown tone style:" << ts;
            break;
        }
    }
    qCDebug(logUtil) << "Tone conversion result:" << newStr;
    return newStr;
}

static QStringList toned(const QStringList &words, ToneStyle ts)
{
    qCDebug(logUtil) << "Converting tones for" << words.size() << "words, style:" << ts;
    QStringList result;
    for (const QString &word : words) {
        result << toned(word, ts);
    }
    qCDebug(logUtil) << "Tone conversion completed, result size:" << result.size();
    return result;
}

static QStringList permutations(const QStringList &list1, const QStringList &list2)
{
    qCDebug(logUtil) << "Creating permutations for lists of sizes:" << list1.size() << list2.size();
    QStringList result;
    for (const QString &item1 : list1) {
        for (const QString &item2 : list2) {
            result << item1 + item2;
        }
    }
    qCDebug(logUtil) << "Permutations created, result size:" << result.size();
    return result;
}

static QStringList permutations(const QList<QStringList> &pyList)
{
    qCDebug(logUtil) << "Creating permutations for" << pyList.size() << "pinyin lists";
    if (pyList.isEmpty()) {
        qCDebug(logUtil) << "Empty pinyin list, returning empty result";
        return QStringList();
    }

    QStringList result = pyList.first();
    for (int i = 1; i < pyList.size(); ++i) {
        result = permutations(result, pyList[i]);
    }
    qCDebug(logUtil) << "Permutations completed, result size:" << result.size();
    return result;
}

static QStringList deduplication(const QStringList &list)
{
    qCDebug(logUtil) << "Deduplicating list of size:" << list.size();
    QSet<QString> set;
    QStringList result;
    for (const QString &item : list) {
        if (!set.contains(item)) {
            set.insert(item);
            result << item;
        }
    }
    qCDebug(logUtil) << "Deduplication completed, result size:" << result.size();
    return result;
}

/*!
  \fn QString Dtk::Core::Chinese2Pinyin(const QString &words)
  \brief Convert Chinese characters to Pinyin
  \note this function not support polyphonic characters support.
  \return pinyin of the words
  \sa Dtk::Core::pinyin
 */
QString Chinese2Pinyin(const QString &words)
{
    qCDebug(logUtil) << "Converting Chinese to Pinyin:" << words;
    QStringList res = pinyin(words, TS_ToneNum);
    QString result = res.value(0);
    qCDebug(logUtil) << "Chinese to Pinyin conversion completed:" << result;
    return result;
}

/*!
 * @~english
  \enum Dtk::Core::ToneStyle
   pinyin tone style

  \value TS_NoneTone pinyin without tone

  \value TS_Tone pinyin with tone, default style in dictory file

  \value TS_ToneNum pinyin tone number

 */

/*!
  \fn QStringList Dtk::Core::pinyin(const QString &words, ToneStyle ts, bool *ok)
  \brief Convert Chinese characters to Pinyin with polyphonic characters support.

  \return pinyin list of the words
 */
QStringList pinyin(const QString &words, ToneStyle ts, bool *ok)
{
    qCDebug(logUtil) << "Getting pinyin for:" << words << "tone style:" << ts;
    if (words.length() < 1) {
        qCDebug(logUtil) << "Words length is less than 1, returning empty list";
        return QStringList();
    }

    InitDict();

    if (ok) {
        *ok = true;
    }
    QList<QStringList> pyList;
    for (int i = 0; i < words.length(); ++i) {
        const uint key = words.at(i).unicode();
        auto find_result = dict.find(key);

        if (find_result != dict.end()) {
            const QString &ret = find_result.value();
            pyList << toned(ret.split(","), ts);
            qCDebug(logUtil) << "Found pinyin for character:" << words.at(i).toLatin1() << "->" << ret;
        } else {
            pyList << QStringList(words.at(i));
            qCDebug(logUtil) << "No pinyin found for character:" << words.at(i).toLatin1() << "using character itself";
            // 部分字没有在词典中找到，使用字本身， ok 可以判断结果
            if (ok) {
                *ok = false;
            }
        }
    }

    QStringList result = deduplication(permutations(pyList));
    qCDebug(logUtil) << "Pinyin conversion completed, result size:" << result.size();
    return result;
}

/*!
  \fn QStringList Dtk::Core::firstLetters(const QString &words)
  \brief Convert Chinese characters to Pinyin firstLetters list
  \brief with polyphonic characters support.

  \return pinyin first letters list of the words
 */
QStringList firstLetters(const QString &words)
{
    qCDebug(logUtil) << "Getting first letters for:" << words;
    QList<QStringList> result;
    bool ok = false;
    for (const QChar &w : words) {
        QStringList pys = pinyin(w, TS_Tone, &ok);
        if (!ok) {
            result << QStringList(w);
            qCDebug(logUtil) << "Failed to get pinyin for character:" << w.toLatin1() << "using character itself";
            continue;
        }

        for (QString &py : pys) {
            py = py.left(1);
        }

        result << pys;
    }

    QStringList finalResult = deduplication(permutations(result));
    qCDebug(logUtil) << "First letters completed, result size:" << finalResult.size();
    return finalResult;
}

DCORE_END_NAMESPACE
