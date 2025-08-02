#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QString>
#include <QMutex>
#include <QScopedPointer>
#include <QFile>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QVector>
#include <QRegularExpression>
#include <QNetworkAccessManager>
#include <QTcpSocket>

class Logger : public QObject
{
    Q_OBJECT

public:
    enum class LogLevel {
        Trace,    // Very detailed tracing
        Debug,    // Debugging messages
        Info,     // Informational messages
        Warning,  // Warning conditions
        Error,    // Error conditions
        Fatal     // Critical failures
    };
    Q_ENUM(LogLevel)

    struct LogEntry {
        QDateTime timestamp;
        QString level;
        QString sourceFile;
        int lineNumber;
        qint64 threadId;
        QString message;
    };

    // Singleton instance
    static Logger* instance();

    // Core logging function
    void log(LogLevel level, const QString& message, const QString& file = "", int line = 0);

    // Configuration methods
    void setLogLevel(LogLevel level);
    void setLogFilePath(const QString& path);
    void setMaxFileSize(qint64 size); // in bytes
    void setMaxFiles(int count);      // maximum log files to keep
    void setMaxDatabaseSize(qint64 size); // maximum database size in bytes

    // Output control methods
    void enableConsoleOutput(bool enable);
    void enableFileOutput(bool enable);
    void enableNetworkLogging(bool enable, const QUrl& serverUrl = QUrl());
    void enableDatabaseLogging(bool enable, const QString& connectionName = QString());
    void enablePerformanceMetrics(bool enable);

    // Filtering methods
    void addFilter(const QString& filterPattern);
    void clearFilters();

    // Helper functions (static access)
    static void trace(const QString& message, const QString& file = "", int line = 0);
    static void debug(const QString& message, const QString& file = "", int line = 0);
    static void info(const QString& message, const QString& file = "", int line = 0);
    static void warning(const QString& message, const QString& file = "", int line = 0);
    static void error(const QString& message, const QString& file = "", int line = 0);
    static void fatal(const QString& message, const QString& file = "", int line = 0);
    static void logPerformance(const QString& operation, qint64 elapsedMs);

    // Query methods
    QVector<LogEntry> queryLogs(const QDateTime& from = QDateTime(), 
                               const QDateTime& to = QDateTime::currentDateTime(),
                               LogLevel minLevel = LogLevel::Trace,
                               const QString& filter = QString(),
                               int limit = 1000);

    // Getters
    QString getLogFilePath() const;
    LogLevel getLogLevel() const;
    QMap<QString, int> getPerformanceMetrics() const;

signals:
    void logMessagePosted(Logger::LogLevel level, const QString& message);

protected:
    explicit Logger(QObject* parent = nullptr);
    ~Logger() override;

private:
    // Private implementation
    void openLogFile();
    void checkLogRotation();
    void initializeDatabase();
    void logToDatabase(const QString& message, LogLevel level, const QString& file, int line);
    void checkDatabaseSize();
    void archiveOldLogs();
    void sendToNetwork(const QString& logEntry);
    bool shouldFilterMessage(const QString& message);
    void updatePerformanceMetrics(const QString& level, const QString& message);
    static void crashHandler(int signal);

    // Member variables
    QString m_logFilePath;
    QFile m_logFile;
    LogLevel m_logLevel;
    qint64 m_maxFileSize;
    int m_maxFiles;
    bool m_enableConsoleOutput;
    bool m_enableFileOutput;
    bool m_enableNetworkOutput;
    bool m_enableDatabaseLogging;
    bool m_performanceMetricsEnabled;
    qint64 m_maxDatabaseSize;
    QString m_dbConnectionName;
    QUrl m_remoteLogServerUrl;
    QSqlDatabase m_database;
    QMap<QString, int> m_performanceMetrics;

    // Static members
    static QMutex m_mutex;
    static QScopedPointer<Logger> m_instance;
    static QNetworkAccessManager* m_networkManager;
    static QTcpSocket* m_logSocket;
    static QVector<QRegularExpression> m_logFilters;

    Q_DISABLE_COPY(Logger)

public:
    // Performance tracking utility class
    class PerformanceTracker {
    public:
        explicit PerformanceTracker(const QString& operationName) : 
            m_operation(operationName) {
            m_timer.start();
        }
        
        ~PerformanceTracker() {
            Logger::logPerformance(m_operation, m_timer.elapsed());
        }
        
    private:
        QString m_operation;
        QElapsedTimer m_timer;
    };

    // Log handler interface for plugins
    class LogHandlerInterface {
    public:
        virtual ~LogHandlerInterface() = default;
        virtual void handleLog(Logger::LogLevel level, const QString& message) = 0;
    };

    void registerLogHandler(LogHandlerInterface* handler) {
        QMutexLocker locker(&m_mutex);
        m_logHandlers.append(handler);
    }

    void unregisterLogHandler(LogHandlerInterface* handler) {
        QMutexLocker locker(&m_mutex);
        m_logHandlers.removeAll(handler);
    }

private:
    QVector<LogHandlerInterface*> m_logHandlers;
};

// Macros for easier logging
#define LOG_TRACE(msg) Logger::trace(msg, __FILE__, __LINE__)
#define LOG_DEBUG(msg) Logger::debug(msg, __FILE__, __LINE__)
#define LOG_INFO(msg) Logger::info(msg, __FILE__, __LINE__)
#define LOG_WARN(msg) Logger::warning(msg, __FILE__, __LINE__)
#define LOG_ERROR(msg) Logger::error(msg, __FILE__, __LINE__)
#define LOG_FATAL(msg) Logger::fatal(msg, __FILE__, __LINE__)
#define LOG_PERF(msg, time) Logger::logPerformance(msg, time)
#define PERF_TRACK(op) Logger::PerformanceTracker perfTracker(op)

#endif // LOGGER_H
