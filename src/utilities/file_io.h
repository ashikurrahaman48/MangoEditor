#ifndef FILE_IO_H
#define FILE_IO_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QFileInfoList>
#include <QByteArray>
#include <QCryptographicHash>
#include <QFileSystemWatcher>
#include <QLockFile>
#include <QFuture>

/**
 * @brief The FileIO class provides comprehensive file operations
 * 
 * This class handles all file I/O operations with support for:
 * - Multiple encodings (UTF-8, UTF-16, Bangla-specific)
 * - Thread-safe operations
 * - File monitoring
 * - Bangladesh-specific text handling
 */
class FileIO : public QObject
{
    Q_OBJECT

public:
    explicit FileIO(QObject *parent = nullptr);
    ~FileIO();

    // ==================== Basic File Operations ====================
    bool readTextFile(const QString &filePath, QString &content, QString &detectedEncoding);
    bool writeTextFile(const QString &filePath, const QString &content, 
                      const QString &encoding = "UTF-8", bool backup = false);
    
    // ==================== Encoding Detection ====================
    QString detectEncodingFromContent(const QByteArray &data);
    bool validateBanglaUtf8(const QString &content);
    
    // ==================== File System Operations ====================
    bool createDirectory(const QString &path);
    QStringList getFilesInDirectory(const QString &path, 
                                   const QStringList &filters = QStringList(),
                                   bool recursive = false);
    bool copyFile(const QString &source, const QString &destination, bool overwrite = false);
    bool deleteFile(const QString &filePath);
    bool renameFile(const QString &oldPath, const QString &newPath);
    
    // ==================== File Information ====================
    QFileInfoList getFileInfoList(const QString &directory, bool recursive = false);
    bool isBinaryFile(const QString &filePath);
    bool isBanglaTextFile(const QString &filePath);
    QString getFileSizeHumanReadable(qint64 bytes);
    QString calculateFileHash(const QString &filePath, 
                            QCryptographicHash::Algorithm method = QCryptographicHash::Sha256);

    // ==================== Advanced Features ====================
    bool lockFile(const QString &filePath, int timeoutMs = 1000);
    bool unlockFile(const QString &filePath);
    void watchFile(const QString &filePath);
    void stopWatchingFile(const QString &filePath);
    QTemporaryFile* createTempFile(const QString &pattern = "mango_XXXXXX");
    
    // ==================== Bangladesh-Specific ====================
    bool convertToUnicode(const QString &sourcePath, const QString &destPath, 
                         const QString &targetEncoding = "UTF-8");
    bool convertBijoyToUnicode(const QString &input, QString &output);

    // ==================== Async Operations ====================
    QFuture<bool> readTextFileAsync(const QString &filePath);
    QFuture<bool> writeTextFileAsync(const QString &filePath, const QString &content,
                                    const QString &encoding = "UTF-8");

signals:
    void fileOperationStarted(const QString &operation, const QString &filePath);
    void fileOperationCompleted(const QString &filePath, bool success);
    void fileChangedExternally(const QString &filePath);
    void encodingDetected(const QString &filePath, const QString &encoding);
    void banglaTextDetected(const QString &filePath);

public slots:
    void cancelAllOperations();

private:
    QFileSystemWatcher *m_fileWatcher;
    QHash<QString, QLockFile*> m_activeLocks;
    
    QString generateBackupName(const QString &originalPath, int version = 0);
    void cleanupLocks();
    bool validateFileEncoding(const QByteArray &data, const QString &encoding);
};

#endif // FILE_IO_H
