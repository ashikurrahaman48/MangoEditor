#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

#include "interface.h"
#include <QObject>
#include <QMap>
#include <QVector>
#include <QString>
#include <QPluginLoader>
#include <QFileSystemWatcher>
#include <QThread>
#include <QMutex>
#include <QElapsedTimer>

class EditorCore;
class PluginSandbox;

struct PluginLoadInfo {
    QString filePath;
    qint64 loadTimeMs;
    QDateTime loadTime;
    bool loadedSuccessfully;
    QString errorString;
};

struct PluginPerformance {
    QString pluginId;
    QString name;
    qint64 loadTimeMs;
    qint64 initTimeMs;
    qint64 avgResponseMs;
    int memoryUsageKB;
    int crashCount;
};

class PluginManager : public QObject
{
    Q_OBJECT

public:
    explicit PluginManager(EditorCore* core, QObject* parent = nullptr);
    ~PluginManager() override;

    // Plugin loading
    void loadAllPlugins();
    void loadBuiltinPlugins();
    void loadExternalPlugins();
    void unloadAllPlugins();
    void reloadPlugins();
    void scanForPlugins();

    // Plugin information
    QVector<IPlugin*> plugins() const;
    IPlugin* plugin(const QString& pluginId) const;
    QVector<PluginCommand> allCommands() const;
    QVector<PluginPerformance> performanceMetrics() const;
    QVector<PluginLoadInfo> pluginLoadHistory() const;
    bool isPluginLoaded(const QString& pluginId) const;

    // Plugin control
    bool enablePlugin(const QString& pluginId, bool enable);
    bool startPlugin(const QString& pluginId);
    bool stopPlugin(const QString& pluginId);
    void setPluginSearchPaths(const QStringList& paths);
    void setPluginAutoReload(bool enabled);

    // Dependency management
    bool checkDependencies(const QString& pluginId) const;
    QVector<PluginDependency> unmetDependencies(const QString& pluginId) const;

    // Safety mechanisms
    void addToBlacklist(const QString& pluginId, const QString& reason);
    void removeFromBlacklist(const QString& pluginId);
    bool isBlacklisted(const QString& pluginId) const;
    QString blacklistReason(const QString& pluginId) const;

    // Version compatibility
    bool isCompatible(const QString& pluginVersion) const;
    static bool isVersionCompatible(const QString& pluginVersion, const QString& editorVersion);

signals:
    void pluginLoaded(const QString& pluginId);
    void pluginUnloaded(const QString& pluginId);
    void pluginStarted(const QString& pluginId);
    void pluginStopped(const QString& pluginId);
    void pluginCrashed(const QString& pluginId, const QString& errorMessage);
    void pluginPerformanceWarning(const QString& pluginId, const QString& warning);
    void pluginsReady();
    void commandRegistryUpdated();

public slots:
    void handlePluginCrash(const QString& pluginId);
    void updateCommandStates();

private slots:
    void onPluginDirectoryChanged(const QString& path);
    void forwardStatusMessage(const QString& msg, int timeout);

private:
    void registerPlugin(IPlugin* plugin);
    void unregisterPlugin(const QString& pluginId);
    void resolveDependencies();
    void initializePlugin(IPlugin* plugin);
    void shutdownPlugin(IPlugin* plugin);
    void createPluginSandbox(IPlugin* plugin);
    void monitorPluginPerformance();
    void logPluginLoad(const PluginLoadInfo& info);
    void cleanupCrashedPlugin(const QString& pluginId);

    EditorCore* m_core;
    QMap<QString, IPlugin*> m_plugins;
    QMap<QString, QPluginLoader*> m_pluginLoaders;
    QMap<QString, PluginSandbox*> m_pluginSandboxes;
    QMap<QString, PluginLoadInfo> m_loadHistory;
    QMap<QString, QString> m_blacklist;
    QStringList m_pluginSearchPaths;
    QFileSystemWatcher* m_fileWatcher;
    mutable QMutex m_mutex;
    bool m_autoReloadEnabled = true;
};

#endif // PLUGIN_MANAGER_H
