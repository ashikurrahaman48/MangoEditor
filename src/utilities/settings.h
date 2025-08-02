#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>
#include <QSettings>
#include <QFont>
#include <QMutex>
#include <QStringList>
#include <QVariantMap>
#include <QNetworkAccessManager>
#include <QDateTime>

class SettingsManager : public QObject
{
    Q_OBJECT

public:
    // Singleton instance access
    static SettingsManager* instance();
    
    // Prevent copying and moving
    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;
    SettingsManager(SettingsManager&&) = delete;
    SettingsManager& operator=(SettingsManager&&) = delete;

    // Core settings operations
    QVariant get(const QString& key, const QVariant& defaultValue = QVariant()) const;
    void set(const QString& key, const QVariant& value);
    void remove(const QString& key);
    bool contains(const QString& key) const;
    void setDefault(const QString& key, const QVariant& value);
    void sync();
    void resetToDefaults();

    // Editor specific settings
    QFont editorFont() const;
    void setEditorFont(const QFont& font);
    QString editorTheme() const;
    void setEditorTheme(const QString& theme);
    int editorTabWidth() const;
    void setEditorTabWidth(int width);
    bool editorWordWrap() const;
    void setEditorWordWrap(bool enabled);

    // Window state management
    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray& geometry);
    QByteArray windowState() const;
    void setWindowState(const QByteArray& state);
    bool windowMaximized() const;
    void setWindowMaximized(bool maximized);

    // Recent files management
    QStringList recentFiles() const;
    void setRecentFiles(const QStringList& files);
    void addRecentFile(const QString& filePath);
    int maxRecentFiles() const;
    void setMaxRecentFiles(int count);

    // Auto-save configuration
    bool autoSaveEnabled() const;
    void setAutoSaveEnabled(bool enabled);
    int autoSaveInterval() const;
    void setAutoSaveInterval(int minutes);

    // Cloud synchronization
    bool cloudSyncEnabled() const;
    void setCloudSyncEnabled(bool enabled);
    QDateTime lastCloudSync() const;
    void syncWithCloudStorage();

    // Import/Export functionality
    bool exportSettings(const QString& filePath);
    bool importSettings(const QString& filePath);

    // Settings groups
    void beginGroup(const QString& prefix);
    void endGroup();
    QString group() const;

    // Plugin settings management
    void registerPluginSettings(const QString& pluginId, const QVariantMap& defaults);
    QVariant getPluginSetting(const QString& pluginId, const QString& key, 
                            const QVariant& defaultValue = QVariant()) const;
    void setPluginSetting(const QString& pluginId, const QString& key, const QVariant& value);

    // Performance tracking
    class PerformanceTracker {
    public:
        explicit PerformanceTracker(const QString& operationName);
        ~PerformanceTracker();
    private:
        QString m_operation;
        QElapsedTimer m_timer;
    };

signals:
    void settingChanged(const QString& key, const QVariant& value);
    void settingsImported();
    void settingsExported(bool success);
    void cloudSyncStatusChanged(bool enabled);
    void cloudSyncCompleted(bool success);

protected:
    explicit SettingsManager(QObject* parent = nullptr);
    ~SettingsManager();

private:
    // Private implementation
    void checkForMigration();
    void migrateFromPreviousVersion(int oldVersion);
    bool validateSetting(const QString& key, const QVariant& value);

    static SettingsManager* m_instance;
    static QMutex m_mutex;
    static QNetworkAccessManager* m_networkManager;
    
    QSettings* m_settings;
    QString m_settingsPath;
    bool m_cloudSyncEnabled;
    const int m_settingsVersion;
};

// Macros for easier usage
#define SETTINGS SettingsManager::instance()
#define PERF_TRACK(op) SettingsManager::PerformanceTracker perfTracker(op)

#endif // SETTINGS_H
