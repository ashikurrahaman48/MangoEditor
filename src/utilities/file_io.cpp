#include "file_io.h"
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QTextCodec>
#include <QLockFile>
#include <QTemporaryFile>
#include <QFileSystemWatcher>
#include <QCryptographicHash>
#include <QMessageBox>
#include <QDebug>
#include <chrono>

// বাংলাদেশী ডেভেলপারদের জন্য বিশেষ UTF-8 ভ্যালিডেশন
const QByteArray BANGLA_UTF8_SIGNATURE = QByteArray::fromHex("e0a6a4"); // "ত" character

FileIO::FileIO(QObject *parent) : QObject(parent) 
{
    m_fileWatcher = new QFileSystemWatcher(this);
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, 
            this, &FileIO::onWatchedFileChanged);
}

bool FileIO::readTextFile(const QString &filePath, QString &content, QString &detectedEncoding)
{
    QElapsedTimer timer;
    timer.start();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "File open error:" << file.errorString();
        return false;
    }

    // Step 1: Check for BOM (Byte Order Mark)
    QByteArray bom = file.peek(4);
    if (bom.startsWith("\xEF\xBB\xBF")) {
        detectedEncoding = "UTF-8";
        file.seek(3); // Skip UTF-8 BOM
    } else if (bom.startsWith("\xFF\xFE")) {
        detectedEncoding = "UTF-16LE";
        file.seek(2); // Skip UTF-16LE BOM
    } else if (bom.startsWith("\xFE\xFF")) {
        detectedEncoding = "UTF-16BE";
        file.seek(2); // Skip UTF-16BE BOM
    } else {
        // Step 2: Auto-detect encoding
        QByteArray data = file.peek(1024);
        detectedEncoding = detectEncodingFromContent(data);
    }

    // Step 3: Read with proper encoding
    QTextStream in(&file);
    in.setCodec(detectedEncoding.toUtf8());
    content = in.readAll();

    // Step 4: Validate Bangla UTF-8 content
    if (detectedEncoding == "UTF-8" && content.contains(QRegularExpression("[ঀ-৿]"))) {
        if (!validateBanglaUtf8(content)) {
            qWarning() << "Invalid Bangla UTF-8 sequence detected";
            // Fallback to system locale
            detectedEncoding = QTextCodec::codecForLocale()->name();
            file.seek(0);
            in.setCodec(detectedEncoding.toUtf8());
            content = in.readAll();
        }
    }

    qDebug() << "Read" << filePath << "in" << timer.elapsed() << "ms with encoding:" << detectedEncoding;
    return true;
}

bool FileIO::writeTextFile(const QString &filePath, const QString &content, const QString &encoding, bool backup)
{
    QElapsedTimer timer;
    timer.start();

    // Step 1: Create backup if requested
    if (backup && QFile::exists(filePath)) {
        QString backupPath = filePath + ".bak";
        if (!QFile::copy(filePath, backupPath)) {
            qWarning() << "Failed to create backup file";
        }
    }

    // Step 2: Atomic write using QSaveFile
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "File open error:" << file.errorString();
        return false;
    }

    QTextStream out(&file);
    if (!encoding.isEmpty()) {
        out.setCodec(encoding.toUtf8());
        
        // Add BOM for UTF encodings
        if (encoding.contains("UTF-16")) {
            out.setGenerateByteOrderMark(true);
        } else if (encoding == "UTF-8" && content.contains(QRegularExpression("[ঀ-৿]"))) {
            // Explicitly write UTF-8 BOM for Bangla content
            file.write("\xEF\xBB\xBF");
        }
    }
    
    out << content;

    // Step 3: Finalize write operation
    if (!file.commit()) {
        qWarning() << "File write error:" << file.errorString();
        return false;
    }

    qDebug() << "Wrote" << filePath << "in" << timer.elapsed() << "ms with encoding:" << encoding;
    return true;
}

QString FileIO::detectEncodingFromContent(const QByteArray &data)
{
    // Bangladesh-specific: Check for Bangla UTF-8 patterns
    if (data.contains(BANGLA_UTF8_SIGNATURE)) {
        return "UTF-8";
    }

    // Check for null bytes (likely UTF-16)
    if (data.contains('\0')) {
        return "UTF-16LE"; // Default to little-endian
    }

    // Use system locale as fallback
    return QTextCodec::codecForLocale()->name();
}

bool FileIO::validateBanglaUtf8(const QString &content)
{
    QTextCodec::ConverterState state;
    QTextCodec *codec = QTextCodec::codecForName("UTF-8");
    codec->toUnicode(content.toUtf8(), content.length(), &state);
    
    if (state.invalidChars > 0) {
        qWarning() << "Invalid UTF-8 sequences found in Bangla text";
        return false;
    }
    return true;
}

bool FileIO::createDirectory(const QString &path)
{
    QDir dir;
    if (!dir.mkpath(path)) {
        qWarning() << "Failed to create directory:" << path;
        return false;
    }
    return true;
}

QStringList FileIO::getFilesInDirectory(const QString &path, const QStringList &filters, bool recursive)
{
    QDir dir(path);
    if (!dir.exists()) {
        qWarning() << "Directory does not exist:" << path;
        return QStringList();
    }

    QStringList files;
    QDirIterator::IteratorFlags flags = recursive ? 
        QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;

    QDirIterator it(path, filters, QDir::Files | QDir::NoDotAndDotDot, flags);
    while (it.hasNext()) {
        files.append(it.next());
    }

    return files;
}

bool FileIO::copyFile(const QString &source, const QString &destination, bool overwrite)
{
    if (QFile::exists(destination)) {
        if (overwrite) {
            QFile::remove(destination);
        } else {
            qWarning() << "Destination file exists:" << destination;
            return false;
        }
    }

    // Use QSaveFile for atomic copy operation
    QFile srcFile(source);
    QSaveFile destFile(destination);

    if (!srcFile.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open source file:" << srcFile.errorString();
        return false;
    }

    if (!destFile.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open destination file:" << destFile.errorString();
        return false;
    }

    // Copy in chunks for large files
    const qint64 bufferSize = 1024 * 1024; // 1MB
    char *buffer = new char[bufferSize];

    while (!srcFile.atEnd()) {
        qint64 bytesRead = srcFile.read(buffer, bufferSize);
        if (bytesRead == -1) {
            qWarning() << "Error reading file:" << srcFile.errorString();
            delete[] buffer;
            return false;
        }

        if (destFile.write(buffer, bytesRead) == -1) {
            qWarning() << "Error writing file:" << destFile.errorString();
            delete[] buffer;
            return false;
        }
    }

    delete[] buffer;
    return destFile.commit();
}

bool FileIO::lockFile(const QString &filePath, int timeoutMs)
{
    QLockFile *lockFile = new QLockFile(filePath + ".lock");
    lockFile->setStaleLockTime(30000); // 30 seconds stale lock timeout

    if (!lockFile->tryLock(timeoutMs)) {
        qWarning() << "Failed to lock file:" << lockFile->error();
        delete lockFile;
        return false;
    }

    // Store lock file in map for later release
    m_activeLocks[filePath] = lockFile;
    return true;
}

bool FileIO::unlockFile(const QString &filePath)
{
    if (m_activeLocks.contains(filePath)) {
        m_activeLocks[filePath]->unlock();
        delete m_activeLocks[filePath];
        m_activeLocks.remove(filePath);
        return true;
    }
    return false;
}

void FileIO::watchFile(const QString &filePath)
{
    if (!m_fileWatcher->files().contains(filePath)) {
        m_fileWatcher->addPath(filePath);
    }
}

void FileIO::stopWatchingFile(const QString &filePath)
{
    m_fileWatcher->removePath(filePath);
}

QTemporaryFile *FileIO::createTempFile(const QString &pattern)
{
    QTemporaryFile *tempFile = new QTemporaryFile(pattern, this);
    tempFile->setAutoRemove(false);
    if (tempFile->open()) {
        return tempFile;
    }
    delete tempFile;
    return nullptr;
}

QString FileIO::calculateFileHash(const QString &filePath, QCryptographicHash::Algorithm method)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    QCryptographicHash hash(method);
    if (hash.addData(&file)) {
        return QString(hash.result().toHex());
    }
    return QString();
}

void FileIO::onWatchedFileChanged(const QString &path)
{
    qDebug() << "Watched file changed:" << path;
    emit fileChangedExternally(path);
}

// বাংলাদেশী ডেভেলপারদের জন্য বিশেষ ইউটিলিটি
bool FileIO::convertToUnicode(const QString &sourcePath, const QString &destPath, const QString &targetEncoding)
{
    QString content;
    QString sourceEncoding;
    
    if (!readTextFile(sourcePath, content, sourceEncoding)) {
        return false;
    }

    return writeTextFile(destPath, content, targetEncoding);
}

bool FileIO::isBanglaTextFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray data = file.read(1024);
    file.close();

    // Check for Bangla Unicode range
    return data.contains(BANGLA_UTF8_SIGNATURE) || 
           QRegularExpression("[ঀ-৿]").match(QString::fromUtf8(data)).hasMatch();
}
