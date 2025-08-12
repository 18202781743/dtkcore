// SPDX-FileCopyrightText: 2022-2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dtextencoding.h"

#include <QtMath>
#include <QFile>
#include <QLibrary>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#else
#include <QTextCodec>
#endif

#include <climits>
#include <unicode/ucsdet.h>
#include <uchardet/uchardet.h>
#include <iconv.h>
#include <QLoggingCategory>

DCORE_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(logUtil, "dtk.core.util")

class LibICU
{
public:
    LibICU();
    ~LibICU();

    bool isValid();
    bool detectEncoding(const QByteArray &content, QByteArrayList &charset);

    UCharsetDetector *(*icu_ucsdet_open)(UErrorCode *status);
    void (*icu_ucsdet_close)(UCharsetDetector *ucsd);
    void (*icu_ucsdet_setText)(UCharsetDetector *ucsd, const char *textIn, int32_t len, UErrorCode *status);
    const UCharsetMatch **(*icu_ucsdet_detectAll)(UCharsetDetector *ucsd, int32_t *matchesFound, UErrorCode *status);
    const char *(*icu_ucsdet_getName)(const UCharsetMatch *ucsm, UErrorCode *status);
    int32_t (*icu_ucsdet_getConfidence)(const UCharsetMatch *ucsm, UErrorCode *status);

private:
    QLibrary *icuuc = nullptr;

    Q_DISABLE_COPY(LibICU)
};

class Libuchardet
{
public:
    Libuchardet();
    ~Libuchardet();

    bool isValid();
    QByteArray detectEncoding(const QByteArray &content);

    uchardet_t (*uchardet_new)(void);
    void (*uchardet_delete)(uchardet_t ud);
    int (*uchardet_handle_data)(uchardet_t ud, const char *data, size_t len);
    void (*uchardet_data_end)(uchardet_t ud);
    void (*uchardet_reset)(uchardet_t ud);
    const char *(*uchardet_get_charset)(uchardet_t ud);

private:
    QLibrary *uchardet = nullptr;

    Q_DISABLE_COPY(Libuchardet)
};

Q_GLOBAL_STATIC(LibICU, LibICUInstance);
Q_GLOBAL_STATIC(Libuchardet, LibuchardetInstance);

LibICU::LibICU()
{
    qCDebug(logUtil) << "LibICU constructor called";
    // Load libicuuc.so
    icuuc = new QLibrary("libicuuc");
    if (!icuuc->load()) {
        qCWarning(logUtil) << "Failed to load libicuuc library";
        delete icuuc;
        icuuc = nullptr;
        return;
    }

    auto initFunctionError = [this]() {
        qCWarning(logUtil) << "Failed to resolve ICU functions";
        icuuc->unload();
        delete icuuc;
        icuuc = nullptr;
    };

#define INIT_ICUUC(Name)                                                                                                         \
    icu_##Name = reinterpret_cast<decltype(icu_##Name)>(icuuc->resolve(#Name));                                                  \
    if (!icu_##Name) {                                                                                                           \
        initFunctionError();                                                                                                     \
        return;                                                                                                                  \
    }

    // Note: use prefix 'icu' to avoid "disabled expansion of recursive macro" warning.
    INIT_ICUUC(ucsdet_open);
    INIT_ICUUC(ucsdet_close);
    INIT_ICUUC(ucsdet_setText);
    INIT_ICUUC(ucsdet_detectAll);
    INIT_ICUUC(ucsdet_getName);
    INIT_ICUUC(ucsdet_getConfidence);

    qCDebug(logUtil) << "LibICU initialized successfully";
}

LibICU::~LibICU()
{
    qCDebug(logUtil) << "LibICU destructor called";
    if (icuuc) {
        icuuc->unload();
        delete icuuc;
        icuuc = nullptr;
    }
}

bool LibICU::isValid()
{
    auto valid = icuuc != nullptr;
    qCDebug(logUtil) << "LibICU isValid:" << valid;
    return valid;
}

bool LibICU::detectEncoding(const QByteArray &content, QByteArrayList &charset)
{
    qCDebug(logUtil) << "LibICU detectEncoding called with content size:" << content.size();
    UErrorCode status = U_ZERO_ERROR;
    UCharsetDetector *detector = icu_ucsdet_open(&status);
    if (U_FAILURE(status)) {
        qCWarning(logUtil) << "LibICU failed to open charset detector";
        return false;
    }

    icu_ucsdet_setText(detector, content.data(), content.size(), &status);
    if (U_FAILURE(status)) {
        qCWarning(logUtil) << "LibICU failed to set text for detection";
        icu_ucsdet_close(detector);
        return false;
    }

    int32_t matchCount = 0;
    const UCharsetMatch **charsetMatch = icu_ucsdet_detectAll(detector, &matchCount, &status);
    if (U_FAILURE(status)) {
        qCWarning(logUtil) << "LibICU failed to detect all charsets";
        icu_ucsdet_close(detector);
        return false;
    }

    qCDebug(logUtil) << "LibICU found" << matchCount << "charset matches";
    int recordCount = qMin(3, matchCount);
    for (int i = 0; i < recordCount; i++) {
        const char *encoding = icu_ucsdet_getName(charsetMatch[i], &status);
        if (U_FAILURE(status)) {
            qCWarning(logUtil) << "LibICU failed to get charset name for match" << i;
            icu_ucsdet_close(detector);
            return false;
        }
        charset << QByteArray(encoding);
        qCDebug(logUtil) << "LibICU added charset:" << charset.last();
    }

    icu_ucsdet_close(detector);
    qCDebug(logUtil) << "LibICU detectEncoding completed successfully, found" << charset.size() << "charsets";
    return true;
}

Libuchardet::Libuchardet()
{
    qCDebug(logUtil) << "Libuchardet constructor called";
    // Load libuchardet.so
    uchardet = new QLibrary("libuchardet");
    if (!uchardet->load()) {
        qCWarning(logUtil) << "Failed to load libuchardet library";
        delete uchardet;
        uchardet = nullptr;
        return;
    }

    auto initFunctionError = [this]() {
        qCWarning(logUtil) << "Failed to resolve uchardet functions";
        uchardet->unload();
        delete uchardet;
        uchardet = nullptr;
    };

#define INIT_UCHARDET(Name)                                                                                                      \
    Name = reinterpret_cast<decltype(Name)>(uchardet->resolve(#Name));                                                           \
    if (!Name) {                                                                                                                 \
        initFunctionError();                                                                                                     \
        return;                                                                                                                  \
    }

    INIT_UCHARDET(uchardet_new);
    INIT_UCHARDET(uchardet_delete);
    INIT_UCHARDET(uchardet_handle_data);
    INIT_UCHARDET(uchardet_data_end);
    INIT_UCHARDET(uchardet_reset);
    INIT_UCHARDET(uchardet_get_charset);

    qCDebug(logUtil) << "Libuchardet initialized successfully";
}

Libuchardet::~Libuchardet()
{
    qCDebug(logUtil) << "Libuchardet destructor called";
    if (uchardet) {
        uchardet->unload();
        delete uchardet;
        uchardet = nullptr;
    }
}

bool Libuchardet::isValid()
{
    bool valid = uchardet != nullptr;
    qCDebug(logUtil) << "Libuchardet isValid:" << valid;
    return valid;
}

QByteArray Libuchardet::detectEncoding(const QByteArray &content)
{
    qCDebug(logUtil) << "Libuchardet detectEncoding called with content size:" << content.size();
    QByteArray charset;

    uchardet_t handle = uchardet_new();
    if (0 == uchardet_handle_data(handle, content.data(), static_cast<size_t>(content.size()))) {
        uchardet_data_end(handle);
        charset = QByteArray(uchardet_get_charset(handle));
        qCDebug(logUtil) << "Libuchardet detected charset:" << charset.constData();
    } else {
        qCWarning(logUtil) << "Libuchardet failed to handle data";
    }
    uchardet_delete(handle);

    qCDebug(logUtil) << "Libuchardet detectEncoding result:" << charset.constData();
    return charset;
}

QByteArray selectCharset(const QByteArray &charset, const QByteArrayList &icuCharsetList)
{
    qCDebug(logUtil) << "selectCharset called with charset:" << charset.constData() << "icuCharsetList size:" << icuCharsetList.size();
    if (icuCharsetList.isEmpty()) {
        qCDebug(logUtil) << "selectCharset icuCharsetList is empty, returning original charset";
        return charset;
    }

    static QByteArray encodingGB18030("GB18030");
    if (charset.isEmpty()) {
        QByteArray result = icuCharsetList.contains(encodingGB18030) ? encodingGB18030 : icuCharsetList[0];
        qCDebug(logUtil) << "selectCharset charset is empty, returning:" << result.constData();
        return result;
    } else {
        if (charset.contains(icuCharsetList[0])) {
            qCDebug(logUtil) << "selectCharset charset contains icuCharsetList[0], returning original charset";
            return charset;
        } else {
            QByteArray result = icuCharsetList[0].contains(charset) ? icuCharsetList[0] : charset;
            qCDebug(logUtil) << "selectCharset returning:" << result.constData();
            return result;
        }
    }
}

QByteArray DTextEncoding::detectTextEncoding(const QByteArray &content)
{
    qCDebug(logUtil) << "DTextEncoding detectTextEncoding called with content size:" << content.size();
    if (content.isEmpty()) {
        qCDebug(logUtil) << "DTextEncoding content is empty, returning UTF-8";
        return QByteArray("UTF-8");
    }

    QByteArray charset;
    if (LibuchardetInstance()->isValid()) {
        qCDebug(logUtil) << "DTextEncoding using Libuchardet for detection";
        charset = LibuchardetInstance()->detectEncoding(content);
        qCDebug(logUtil) << "DTextEncoding Libuchardet result:" << charset.constData();
    } else {
        qCDebug(logUtil) << "DTextEncoding Libuchardet not available";
    }

    if (LibICUInstance()->isValid()) {
        qCDebug(logUtil) << "DTextEncoding using LibICU for detection";
        QByteArrayList icuCharsetList;
        if (LibICUInstance()->detectEncoding(content, icuCharsetList)) {
            qCDebug(logUtil) << "DTextEncoding LibICU found" << icuCharsetList.size() << "charsets";
            if (charset.isEmpty() && !icuCharsetList.isEmpty()) {
                charset = icuCharsetList.first();
                qCDebug(logUtil) << "DTextEncoding using first ICU charset:" << charset.constData();
            } else {
                // Improve GB18030 encoding recognition rate.
                charset = selectCharset(charset, icuCharsetList);
                qCDebug(logUtil) << "DTextEncoding selected charset:" << charset.constData();
            }
        } else {
            qCDebug(logUtil) << "DTextEncoding LibICU detection failed";
        }
    } else {
        qCDebug(logUtil) << "DTextEncoding LibICU not available";
    }

    if (charset.isEmpty()) {
        qCDebug(logUtil) << "DTextEncoding charset is empty, trying fallback methods";
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        std::optional<QStringConverter::Encoding> encoding = QStringConverter::encodingForData(content);
        if (encoding) {
            charset = QStringConverter::nameForEncoding(encoding.value());
            qCDebug(logUtil) << "DTextEncoding fallback detected charset:" << charset.constData();
        }
#else
        QTextCodec *codec = QTextCodec::codecForUtfText(content);
        if (codec) {
            charset = codec->name();
            qCDebug(logUtil) << "DTextEncoding fallback detected charset:" << charset.constData();
        }
#endif
    }

    // Use default encoding.
    if (charset.isEmpty() || charset.contains("ASCII")) {
        qCDebug(logUtil) << "DTextEncoding using default UTF-8 encoding";
        charset = QByteArray("UTF-8");
    }

    qCDebug(logUtil) << "DTextEncoding final result:" << charset.constData();
    return charset;
}

QByteArray DTextEncoding::detectFileEncoding(const QString &fileName, bool *isOk)
{
    qCDebug(logUtil) << "DTextEncoding detectFileEncoding called for file:" << fileName;
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        qCWarning(logUtil) << "DTextEncoding failed to open file:" << fileName;
        if (isOk) {
            *isOk = false;
        }
        return QByteArray();
    }

    // At most 64Kb data.
    QByteArray content = file.read(qMin<int>(static_cast<int>(file.size()), USHRT_MAX));
    file.close();
    qCDebug(logUtil) << "DTextEncoding read" << content.size() << "bytes from file";

    QByteArray result = detectTextEncoding(content);
    if (isOk) {
        *isOk = true;
    }
    qCDebug(logUtil) << "DTextEncoding detectFileEncoding result:" << result.constData();
    return result;
}

bool DTextEncoding::convertTextEncoding(
    QByteArray &content, QByteArray &outContent, const QByteArray &toEncoding, const QByteArray &fromEncoding, QString *errString)
{
    return convertTextEncodingEx(content, outContent, toEncoding, fromEncoding, errString);
}

bool DTextEncoding::convertTextEncodingEx(QByteArray &content,
                                          QByteArray &outContent,
                                          const QByteArray &toEncoding,
                                          const QByteArray &fromEncoding,
                                          QString *errString,
                                          int *convertedBytes)
{
    qCDebug(logUtil) << "DTextEncoding convertTextEncodingEx called with fromEncoding:" << fromEncoding.constData() << "toEncoding:" << toEncoding.constData();
    if (content.isEmpty() || fromEncoding == toEncoding) {
        qCDebug(logUtil) << "DTextEncoding content is empty or encodings are same, returning true";
        return true;
    }

    if (toEncoding.isEmpty()) {
        qCWarning(logUtil) << "DTextEncoding toEncoding is empty";
        if (errString) {
            *errString = QStringLiteral("The encode that convert to is empty.");
        }
        return false;
    }

    QByteArray contentEncoding = fromEncoding;
    if (contentEncoding.isEmpty()) {
        qCDebug(logUtil) << "DTextEncoding fromEncoding is empty, detecting encoding";
        contentEncoding = detectTextEncoding(content);
        qCDebug(logUtil) << "DTextEncoding detected encoding:" << contentEncoding.constData();
    }

    // iconv set errno when failed.
    iconv_t handle = iconv_open(toEncoding.data(), contentEncoding.data());
    if (reinterpret_cast<iconv_t>(-1) != handle) {
        qCDebug(logUtil) << "DTextEncoding iconv handle created successfully";
        size_t inBytesLeft = static_cast<size_t>(content.size());
        char *inbuf = content.data();
        size_t outBytesLeft = inBytesLeft * 4;
        char *outbuf = new char[outBytesLeft];

        char *bufferHeader = outbuf;
        size_t maxBufferSize = outBytesLeft;

        size_t ret = iconv(handle, &inbuf, &inBytesLeft, &outbuf, &outBytesLeft);
        int convertError = 0;
        if (static_cast<size_t>(-1) == ret) {
            convertError = errno;
            int converted = content.size() - static_cast<int>(inBytesLeft);
            qCDebug(logUtil) << "DTextEncoding iconv conversion error:" << convertError << "converted bytes:" << converted;
            if (convertedBytes) {
                *convertedBytes = converted;
            }

            if (errString) {
                switch (convertError) {
                    case EILSEQ:
                        qCDebug(logUtil) << "DTextEncoding EILSEQ error: invalid multibyte sequence";
                        *errString = QString("An invalid multibyte sequence has been encountered in the input."
                                             "Converted byte index: %1")
                                         .arg(converted);
                        break;
                    case EINVAL:
                        qCDebug(logUtil) << "DTextEncoding EINVAL error: incomplete multibyte sequence";
                        *errString = QString("An incomplete multibyte sequence has been encountered in the input. "
                                             "Converted byte index: %1")
                                         .arg(converted);
                        break;
                    case E2BIG:
                        qCDebug(logUtil) << "DTextEncoding E2BIG error: insufficient room in output buffer";
                        *errString = QString("There is not sufficient room at *outbuf. Converted byte index: %1").arg(converted);
                        break;
                    default:
                        qCDebug(logUtil) << "DTextEncoding unknown conversion error:" << convertError;
                        break;
                }
            }
        } else {
            qCDebug(logUtil) << "DTextEncoding iconv conversion completed successfully";
        }
        iconv_close(handle);

        // Use iconv converted byte count.
        size_t realConvertSize = maxBufferSize - outBytesLeft;
        outContent = QByteArray(bufferHeader, static_cast<int>(realConvertSize));
        qCDebug(logUtil) << "DTextEncoding output content size:" << outContent.size();

        delete[] bufferHeader;
        // For errors, user decides to keep or remove converted text.
        bool result = (0 == convertError);
        qCDebug(logUtil) << "DTextEncoding convertTextEncodingEx result:" << result;
        return result;

    } else {
        qCWarning(logUtil, "DTextEncoding failed to create iconv handle, errno: %d", errno);
        if (EINVAL == errno && errString) {
            *errString = QStringLiteral("The conversion from fromcode to tocode is not supported by the implementation.");
        }

        return false;
    }
}

bool DTextEncoding::convertFileEncoding(const QString &fileName,
                                        const QByteArray &toEncoding,
                                        const QByteArray &fromEncoding,
                                        QString *errString)
{
    if (fromEncoding == toEncoding) {
        return true;
    }

    QFile file(fileName);
    if (!file.open(QFile::ReadWrite | QFile::Text)) {
        if (errString) {
            *errString = file.errorString();
            file.error();
        }
        return false;
    }

    QByteArray content = file.readAll();
    QByteArray outContent;
    if (!convertTextEncoding(content, outContent, toEncoding, fromEncoding, errString)) {
        file.close();
        return false;
    }

    file.seek(0);
    file.write(outContent);
    file.resize(outContent.size());
    file.close();

    if (QFile::NoError != file.error()) {
        if (errString) {
            *errString = file.errorString();
        }
        return false;
    }
    return true;
}

bool DTextEncoding::convertFileEncodingTo(const QString &fromFile,
                                          const QString &toFile,
                                          const QByteArray &toEncoding,
                                          const QByteArray &fromEncoding,
                                          QString *errString)
{
    if (fromEncoding == toEncoding) {
        return true;
    }

    if (fromFile == toFile) {
        return convertFileEncoding(fromFile, toEncoding, fromEncoding, errString);
    }

    // Check from file and to file before convert.
    QFile readFile(fromFile);
    if (!readFile.open(QFile::ReadOnly | QFile::Text)) {
        if (errString) {
            *errString = QString("Open convert from file failed, %1").arg(readFile.errorString());
        }
        return false;
    }

    QFile writeFile(toFile);
    if (!writeFile.open(QFile::WriteOnly | QFile::Text)) {
        readFile.close();
        if (errString) {
            *errString = QString("Open convert to file failed, %1").arg(writeFile.errorString());
        }
        return false;
    }

    QByteArray content = readFile.readAll();
    readFile.close();
    QByteArray outContent;

    if (!convertTextEncoding(content, outContent, toEncoding, fromEncoding, errString)) {
        writeFile.close();
        writeFile.remove();
        return false;
    }

    writeFile.write(outContent);
    writeFile.close();

    if (QFile::NoError != writeFile.error()) {
        if (errString) {
            *errString = writeFile.errorString();
        }
        return false;
    }
    return true;
}

DCORE_END_NAMESPACE
