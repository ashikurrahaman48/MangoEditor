#include "logger.h"
#include <QDateTime>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QStandardPaths>
#include <QNetworkAccessManager>
#include <QTcpSocket>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <iostream>
#include <csignal>

// Static member initialization
QScopedPointer<Logger> Logger::m_instance;
QMutex Logger::m_mutex;
QNetworkAccessManager* Logger::m_networkManager = nullptr;
QTcpSocket* Logger::m_logSocket = nullptr;
QVector<QRegularExpression> Logger::m_logFilters;
QSqlDatabase Logger::m_database;

Logger::Logger(QObject *parent) : QObject(parent),
    m_logLevel(LogLevel::Info),
    m_maxFileSize(5 * 1024 * 1024), // 5 MB
    m_maxFiles(5),
    m_enableConsoleOutput(true),
    m_enableFileOutput(true),
    m_enableNetworkOutput(false),
    m_enableDatabaseLogging(false),
    m_maxDatabaseSize(50 * 1024 * 1024) // 50 MB
{
    // Default log directory setup
    QString logDirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";
    QDir logDir(logDirPath);
    if (!logDir.exists()) {
        logDir.mkpath(".");
    }

    // Log file path
    m_logFilePath = logDirPath + "/mangoeditor_" + QDateTime::currentDateTime().toString("yyyyMMdd") + ".log";
    openLogFile();

    // Initialize network components
    m_networkManager = new QNetworkAccessManager(this);
    m_logSocket = new QTcpSocket(this);

    // Initialize database if enabled
    if (m_enableDatabaseLogging) {
        initializeDatabase();
    }

    // Install crash handler
    installCrashHandler();
}

Logger::~Logger()
{
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
    if (m_database.isOpen()) {
        m_database.close();
    }
}

void Logger::initializeDatabase()
{
    if (m_database.isOpen()) {
        return;
    }

    QString dbPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs/mangoeditor_logs.db";
    QDir().mkpath(QFileInfo(dbPath).absolutePath());

    m_database = QSqlDatabase::addDatabase("QSQLITE", "logs_connection");
    m_database.setDatabaseName(dbPath);

    if (!m_database.open()) {
        qWarning() << "Failed to open log database:" << m_database.lastError().text();
        return;
    }

    // Create tables if they don't exist
    QSqlQuery query(m_database);
    query.exec("PRAGMA journal_mode = WAL");
    query.exec("PRAGMA synchronous = NORMAL");

    if (!query.exec(
        "CREATE TABLE IF NOT EXISTS logs ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "timestamp DATETIME NOT NULL,"
        "level VARCHAR(10) NOT NULL,"
        "source_file VARCHAR(255),"
        "line_number INTEGER,"
        "thread_id BIGINT,"
        "message TEXT NOT NULL"
        ")")) {
        qWarning() << "Failed to create logs table:" << query.lastError().text();
    }

    // Create indexes for better performance
    query.exec("CREATE INDEX IF NOT EXISTS idx_logs_timestamp ON logs(timestamp)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_logs_level ON logs(level)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_logs_source ON logs(source_file)");
}

void Logger::logToDatabase(const QString &message, LogLevel level, const QString &file, int line)
{
    if (!m_enableDatabaseLogging) return;

    QMutexLocker locker(&m_mutex);
    
    // Initialize database if not already done
    if (!m_database.isOpen()) {
        initializeDatabase();
        if (!m_database.isOpen()) return;
    }

    QString levelStr;
    switch (level) {
        case LogLevel::Trace:   levelStr = "TRACE"; break;
        case LogLevel::Debug:   levelStr = "DEBUG"; break;
        case LogLevel::Info:    levelStr = "INFO"; break;
        case LogLevel::Warning: levelStr = "WARNING"; break;
        case LogLevel::Error:   levelStr = "ERROR"; break;
        case LogLevel::Fatal:   levelStr = "FATAL"; break;
    }

    QSqlQuery query(m_database);
    query.prepare(
        "INSERT INTO logs (timestamp, level, source_file, line_number, thread_id, message) "
        "VALUES (:timestamp, :level, :source_file, :line_number, :thread_id, :message)"
    );

    query.bindValue(":timestamp", QDateTime::currentDateTime());
    query.bindValue(":level", levelStr);
    query.bindValue(":source_file", file);
    query.bindValue(":line_number", line);
    query.bindValue(":thread_id", reinterpret_cast<quintptr>(QThread::currentThreadId()));
    query.bindValue(":message", message);

    if (!query.exec()) {
        qWarning() << "Failed to insert log into database:" << query.lastError().text();
        
        // Attempt to reconnect if connection was lost
        if (m_database.isOpenError()) {
            m_database.close();
            initializeDatabase();
        }
    }

    // Perform log rotation/archiving if needed
    checkDatabaseSize();
}

void Logger::checkDatabaseSize()
{
    if (m_maxDatabaseSize <= 0) return;

    QSqlQuery query(m_database);
    if (query.exec("SELECT page_count * page_size FROM pragma_page_count(), pragma_page_size()")) {
        if (query.next()) {
            qint64 dbSize = query.value(0).toLongLong();
            if (dbSize > m_maxDatabaseSize) {
                archiveOldLogs();
            }
        }
    }
}

void Logger::archiveOldLogs()
{
    QMutexLocker locker(&m_mutex);

    // 1. Create archive database
    QString archivePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + 
                         "/logs/archives/mangoeditor_logs_" + 
                         QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".db";
    QDir().mkpath(QFileInfo(archivePath).absolutePath());

    // 2. Move old logs to archive
    QSqlQuery query(m_database);
    query.exec("BEGIN TRANSACTION");
    
    // Get the cutoff date (keep last 30 days by default)
    QDateTime cutoffDate = QDateTime::currentDateTime().addDays(-30);
    
    // Create archive database
    QSqlDatabase archiveDb = QSqlDatabase::addDatabase("QSQLITE", "archive_connection");
    archiveDb.setDatabaseName(archivePath);
    if (archiveDb.open()) {
        // Copy schema
        query.exec("SELECT sql FROM sqlite_master WHERE type='table'");
        while (query.next()) {
            archiveDb.exec(query.value(0).toString());
        }

        // Move old records
        query.exec(QString(
            "ATTACH DATABASE '%1' AS archive;"
            "INSERT INTO archive.logs SELECT * FROM main.logs WHERE timestamp < '%2';"
            "DELETE FROM main.logs WHERE timestamp < '%2';"
            "DETACH DATABASE archive;"
        ).arg(archivePath).arg(cutoffDate.toString(Qt::ISODate)));

        // Vacuum to reclaim space
        query.exec("VACUUM");
    }
    archiveDb.close();
    QSqlDatabase::removeDatabase("archive_connection");

    query.exec("COMMIT");
}

// [Rest of the existing implementation remains exactly the same...]
// [All other methods stay unchanged from your original code]

void Logger::enableDatabaseLogging(bool enable, const QString &connectionName)
{
    QMutexLocker locker(&m_mutex);
    m_enableDatabaseLogging = enable;
    m_dbConnectionName = connectionName.isEmpty() ? "logs_connection" : connectionName;

    if (enable && !m_database.isOpen()) {
        initializeDatabase();
    } else if (!enable && m_database.isOpen()) {
        m_database.close();
    }
}

void Logger::setMaxDatabaseSize(qint64 size)
{
    QMutexLocker locker(&m_mutex);
    m_maxDatabaseSize = size;
}

QVector<Logger::LogEntry> Logger::queryLogs(const QDateTime &from, const QDateTime &to, 
                                          LogLevel minLevel, const QString &filter, int limit)
{
    QVector<LogEntry> results;
    if (!m_database.isOpen()) return results;

    QString queryStr = 
        "SELECT timestamp, level, source_file, line_number, thread_id, message "
        "FROM logs "
        "WHERE timestamp BETWEEN :from AND :to ";

    if (minLevel != LogLevel::Trace) {
        QString levelStr;
        switch (minLevel) {
            case LogLevel::Debug:   levelStr = "DEBUG"; break;
            case LogLevel::Info:    levelStr = "INFO"; break;
            case LogLevel::Warning: levelStr = "WARNING"; break;
            case LogLevel::Error:   levelStr = "ERROR"; break;
            case LogLevel::Fatal:   levelStr = "FATAL"; break;
            default: break;
        }
        queryStr += "AND level >= :level ";
    }

    if (!filter.isEmpty()) {
        queryStr += "AND message LIKE :filter ";
    }

    queryStr += "ORDER BY timestamp DESC LIMIT :limit";

    QSqlQuery query(m_database);
    query.prepare(queryStr);
    query.bindValue(":from", from);
    query.bindValue(":to", to);
    query.bindValue(":limit", limit);

    if (minLevel != LogLevel::Trace) {
        query.bindValue(":level", levelStr);
    }

    if (!filter.isEmpty()) {
        query.bindValue(":filter", QString("%%1%").arg(filter));
    }

    if (query.exec()) {
        while (query.next()) {
            LogEntry entry;
            entry.timestamp = query.value("timestamp").toDateTime();
            entry.level = query.value("level").toString();
            entry.sourceFile = query.value("source_file").toString();
            entry.lineNumber = query.value("line_number").toInt();
            entry.threadId = query.value("thread_id").toLongLong();
            entry.message = query.value("message").toString();
            results.append(entry);
        }
    } else {
        qWarning() << "Failed to query logs:" << query.lastError().text();
    }

    return results;
}
