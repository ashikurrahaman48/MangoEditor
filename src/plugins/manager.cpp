#include "manager.h"
#include "interface.h"
#include "utilities/logger.h"
#include <QDir>
#include <QPluginLoader>
#include <QCoreApplication>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QLibrary>
#include <QStandardPaths>
#include <QThread>
#include <QProcess>
#include <QTimer>
#include <QRegularExpression>

// PluginSandbox Implementation
PluginSandbox::PluginSandbox(IPlugin* plugin, PluginManager* manager)
    : QThread(manager), m_plugin(plugin), m_manager(manager) 
{
    connect(this, &QThread::finished, this, &QThread::deleteLater);
}

void PluginSandbox::run() 
{
    try {
        QElapsedTimer timer;
        timer.start();
        
        if (m_plugin) {
            m_plugin->initialize(m_manager->core());
            m_plugin->setState(IPlugin::Running);
            emit m_manager->pluginStarted(m_plugin->pluginId());
            qInfo() << "Plugin" << m_plugin->pluginName() << "started in sandbox";
        }
        
        exec();
    } catch (const std::exception& e) {
        qCritical() << "Plugin sandbox crashed:" << e.what();
        emit m_manager->pluginCrashed(m_plugin->pluginId(), e.what());
    }
}

// PluginManager Implementation
PluginManager::PluginManager(EditorCore* core, QObject* parent)
    : QObject(parent), m_core(core), m_fileWatcher(new QFileSystemWatcher(this)) 
{
    m_pluginSearchPaths = {
        QCoreApplication::applicationDirPath() + "/plugins",
        QDir::homePath() + "/.mangoeditor/plugins",
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/plugins"
    };
    
    connect(m_fileWatcher, &QFileSystemWatcher::directoryChanged,
            this, &PluginManager::onPluginDirectoryChanged);
}

PluginManager::~PluginManager()
{
    unloadAllPlugins();
    m_fileWatcher->deleteLater();
}

// Plugin Loading Methods
void PluginManager::loadAllPlugins()
{
    QMutexLocker locker(&m_mutex);
    loadBuiltinPlugins();
    loadExternalPlugins();
    emit pluginsReady();
}

void PluginManager::loadBuiltinPlugins()
{
    QVector<IPlugin*> builtins = {
        new GitIntegration(this),
        new LinterPlugin(this),
        new ThemeManager(this)
    };

    for (auto plugin : builtins) {
        registerPlugin(plugin);
    }
}

void PluginManager::loadExternalPlugins()
{
    QMutexLocker locker(&m_mutex);
    
    for (const auto& path : m_pluginSearchPaths) {
        QDir pluginDir(path);
        if (!pluginDir.exists()) {
            pluginDir.mkpath(".");
            continue;
        }

        for (const auto& file : pluginDir.entryList(QDir::Files)) {
            if (!QLibrary::isLibrary(file) || isBlacklisted(file)) {
                continue;
            }
            
            PluginLoadInfo loadInfo;
            loadInfo.filePath = pluginDir.absoluteFilePath(file);
            QElapsedTimer timer;
            timer.start();
            
            std::unique_ptr<QPluginLoader> loader(new QPluginLoader(loadInfo.filePath, this));
            QObject* pluginInstance = loader->instance();
            
            if (!pluginInstance) {
                loadInfo.loadedSuccessfully = false;
                loadInfo.errorString = loader->errorString();
                logPluginLoad(loadInfo);
                qWarning() << "Failed to load plugin:" << loader->errorString();
                continue;
            }

            IPlugin* plugin = qobject_cast<IPlugin*>(pluginInstance);
            if (plugin) {
                if (!isCompatible(plugin->pluginVersion())) {
                    loadInfo.loadedSuccessfully = false;
                    loadInfo.errorString = "Version incompatible";
                    logPluginLoad(loadInfo);
                    loader->unload();
                    continue;
                }

                plugin->setParent(this);
                loadInfo.loadTimeMs = timer.elapsed();
                loadInfo.loadedSuccessfully = true;
                logPluginLoad(loadInfo);
                
                registerPlugin(plugin);
                m_pluginLoaders.insert(plugin->pluginId(), loader.release());
                
                if (plugin->isThreadSafe()) {
                    createPluginSandbox(plugin);
                }
            } else {
                loader->unload();
            }
        }
    }
    
    resolveDependencies();
    monitorPluginPerformance();
}

// Plugin Registration and Lifecycle
void PluginManager::registerPlugin(IPlugin* plugin)
{
    if (!plugin || m_plugins.contains(plugin->pluginId())) {
        return;
    }

    try {
        QElapsedTimer timer;
        timer.start();
        
        initializePlugin(plugin);
        m_plugins.insert(plugin->pluginId(), plugin);
        
        connect(plugin, &IPlugin::statusMessageRequested,
                this, &PluginManager::forwardStatusMessage);
        connect(plugin, &IPlugin::destroyed,
                this, [this, plugin]() { unregisterPlugin(plugin->pluginId()); });
                
        qInfo() << "Initialized plugin:" << plugin->pluginName() 
                << "in" << timer.elapsed() << "ms";
        emit pluginLoaded(plugin->pluginId());
    } catch (const std::exception& e) {
        qCritical() << "Plugin initialization failed:" << plugin->pluginName() << e.what();
        handlePluginCrash(plugin->pluginId());
    }
}

void PluginManager::initializePlugin(IPlugin* plugin)
{
    plugin->initialize(m_core);
    plugin->setState(IPlugin::Initialized);
    QTimer::singleShot(0, this, &PluginManager::updateCommandStates);
}

void PluginManager::unloadAllPlugins()
{
    QMutexLocker locker(&m_mutex);
    
    for (auto it = m_plugins.begin(); it != m_plugins.end(); ) {
        shutdownPlugin(it.value());
        it = m_plugins.erase(it);
    }

    for (auto loader : m_pluginLoaders) {
        loader->unload();
        delete loader;
    }
    m_pluginLoaders.clear();

    for (auto sandbox : m_pluginSandboxes) {
        sandbox->quit();
        sandbox->wait();
        delete sandbox;
    }
    m_pluginSandboxes.clear();
}

void PluginManager::shutdownPlugin(IPlugin* plugin)
{
    if (!plugin) return;
    
    plugin->shutdown();
    plugin->setState(IPlugin::NotLoaded);
    emit pluginUnloaded(plugin->pluginId());
}

// Plugin Control Methods
bool PluginManager::enablePlugin(const QString& pluginId, bool enable)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_plugins.contains(pluginId)) {
        return false;
    }
    
    IPlugin* plugin = m_plugins[pluginId];
    try {
        if (enable) {
            initializePlugin(plugin);
        } else {
            shutdownPlugin(plugin);
        }
        emit pluginToggled(pluginId, enable);
        return true;
    } catch (const std::exception& e) {
        qCritical() << "Failed to toggle plugin:" << pluginId << e.what();
        return false;
    }
}

bool PluginManager::startPlugin(const QString& pluginId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_plugins.contains(pluginId) || 
        m_plugins[pluginId]->state() == IPlugin::Running) {
        return false;
    }
    
    IPlugin* plugin = m_plugins[pluginId];
    if (plugin->isThreadSafe() && !m_pluginSandboxes.contains(pluginId)) {
        createPluginSandbox(plugin);
    } else {
        plugin->setState(IPlugin::Running);
    }
    
    emit pluginStarted(pluginId);
    return true;
}

bool PluginManager::stopPlugin(const QString& pluginId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_plugins.contains(pluginId) || 
        m_plugins[pluginId]->state() != IPlugin::Running) {
        return false;
    }
    
    IPlugin* plugin = m_plugins[pluginId];
    if (m_pluginSandboxes.contains(pluginId)) {
        m_pluginSandboxes[pluginId]->quit();
        m_pluginSandboxes[pluginId]->wait();
        delete m_pluginSandboxes[pluginId];
        m_pluginSandboxes.remove(pluginId);
    }
    
    plugin->setState(IPlugin::Suspended);
    emit pluginStopped(pluginId);
    return true;
}

// Dependency Management
bool PluginManager::checkDependencies(const QString& pluginId) const
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_plugins.contains(pluginId)) {
        return false;
    }
    
    auto deps = m_plugins[pluginId]->dependencies();
    return std::all_of(deps.begin(), deps.end(), [this](const PluginDependency& dep) {
        return dep.isOptional || m_plugins.contains(dep.pluginId);
    });
}

QVector<PluginDependency> PluginManager::unmetDependencies(const QString& pluginId) const
{
    QMutexLocker locker(&m_mutex);
    QVector<PluginDependency> unmet;
    
    if (m_plugins.contains(pluginId)) {
        auto deps = m_plugins[pluginId]->dependencies();
        std::copy_if(deps.begin(), deps.end(), std::back_inserter(unmet),
            [this](const PluginDependency& dep) {
                return !dep.isOptional && !m_plugins.contains(dep.pluginId);
            });
    }
    
    return unmet;
}

// Blacklist Management
void PluginManager::addToBlacklist(const QString& pluginId, const QString& reason)
{
    QMutexLocker locker(&m_mutex);
    m_blacklist[pluginId] = reason;
    
    if (m_plugins.contains(pluginId)) {
        IPlugin* plugin = m_plugins[pluginId];
        shutdownPlugin(plugin);
        m_plugins.remove(pluginId);
        
        if (m_pluginLoaders.contains(pluginId)) {
            m_pluginLoaders[pluginId]->unload();
            delete m_pluginLoaders[pluginId];
            m_pluginLoaders.remove(pluginId);
        }
    }
}

bool PluginManager::isBlacklisted(const QString& pluginId) const
{
    QMutexLocker locker(&m_mutex);
    return m_blacklist.contains(pluginId);
}

QString PluginManager::blacklistReason(const QString& pluginId) const
{
    QMutexLocker locker(&m_mutex);
    return m_blacklist.value(pluginId, "");
}

// Hot Reload Support
void PluginManager::onPluginDirectoryChanged(const QString& path)
{
    if (!m_autoReloadEnabled) return;
    
    QTimer::singleShot(500, this, [this, path]() {
        qInfo() << "Plugin directory changed:" << path;
        reloadPlugins();
    });
}

void PluginManager::reloadPlugins()
{
    QMutexLocker locker(&m_mutex);
    
    for (const auto& path : m_pluginSearchPaths) {
        QDir dir(path);
        for (const auto& file : dir.entryList(QDir::Files)) {
            if (QLibrary::isLibrary(file)) {
                QString absPath = dir.absoluteFilePath(file);
                QFileInfo fileInfo(absPath);
                
                if (!m_loadHistory.contains(absPath) || 
                    m_loadHistory[absPath].loadTime < fileInfo.lastModified()) {
                    loadPluginFromPath(absPath);
                }
            }
        }
    }
}

void PluginManager::loadPluginFromPath(const QString& path)
{
    std::unique_ptr<QPluginLoader> loader(new QPluginLoader(path, this));
    QObject* pluginInstance = loader->instance();
    
    if (!pluginInstance) {
        qWarning() << "Failed to load plugin:" << loader->errorString();
        return;
    }

    IPlugin* plugin = qobject_cast<IPlugin*>(pluginInstance);
    if (plugin && isCompatible(plugin->pluginVersion())) {
        if (m_plugins.contains(plugin->pluginId())) {
            unregisterPlugin(plugin->pluginId());
        }
        
        plugin->setParent(this);
        registerPlugin(plugin);
        m_pluginLoaders.insert(plugin->pluginId(), loader.release());
    } else {
        loader->unload();
    }
}

// Utility Methods
void PluginManager::setPluginSearchPaths(const QStringList& paths)
{
    QMutexLocker locker(&m_mutex);
    
    for (const auto& path : m_pluginSearchPaths) {
        m_fileWatcher->removePath(path);
    }
    
    m_pluginSearchPaths = paths;
    
    for (const auto& path : m_pluginSearchPaths) {
        m_fileWatcher->addPath(path);
    }
}

QVector<IPlugin*> PluginManager::plugins() const
{
    QMutexLocker locker(&m_mutex);
    return m_plugins.values().toVector();
}

IPlugin* PluginManager::plugin(const QString& pluginId) const
{
    QMutexLocker locker(&m_mutex);
    return m_plugins.value(pluginId, nullptr);
}

void PluginManager::logPluginLoad(const PluginLoadInfo& info)
{
    m_loadHistory[info.filePath] = info;
    
    if (!info.loadedSuccessfully) {
        qWarning() << "Plugin load failed:" << info.filePath << "Error:" << info.errorString;
    }
}

void PluginManager::cleanupCrashedPlugin(const QString& pluginId)
{
    if (m_pluginLoaders.contains(pluginId)) {
        m_pluginLoaders[pluginId]->unload();
        delete m_pluginLoaders[pluginId];
        m_pluginLoaders.remove(pluginId);
    }
    
    if (m_pluginSandboxes.contains(pluginId)) {
        m_pluginSandboxes[pluginId]->quit();
        m_pluginSandboxes[pluginId]->wait();
        delete m_pluginSandboxes[pluginId];
        m_pluginSandboxes.remove(pluginId);
    }
}

// Performance Monitoring
void PluginManager::monitorPluginPerformance()
{
    QTimer::singleShot(5000, this, [this]() {
        QMutexLocker locker(&m_mutex);
        
        for (auto plugin : m_plugins) {
            QElapsedTimer timer;
            timer.start();
            plugin->benchmark();
            qint64 elapsed = timer.elapsed();
            
            if (elapsed > 100) {
                emit pluginPerformanceWarning(plugin->pluginId(), 
                    QString("Slow performance: %1ms").arg(elapsed));
            }
        }
        
        if (!m_plugins.isEmpty()) {
            monitorPluginPerformance();
        }
    });
}

// Version Compatibility
bool PluginManager::isCompatible(const QString& pluginVersion) const
{
    return isVersionCompatible(pluginVersion, qApp->applicationVersion());
}

bool PluginManager::isVersionCompatible(const QString& pluginVersion, const QString& editorVersion)
{
    QRegularExpression re("^(\\d+)\\.(\\d+)");
    auto pluginMatch = re.match(pluginVersion);
    auto editorMatch = re.match(editorVersion);
    
    return pluginMatch.hasMatch() && editorMatch.hasMatch() &&
           pluginMatch.captured(1) == editorMatch.captured(1);
}

// Command Management
void PluginManager::updateCommandStates()
{
    QMutexLocker locker(&m_mutex);
    emit commandRegistryUpdated();
}

QVector<PluginCommand> PluginManager::allCommands() const
{
    QMutexLocker locker(&m_mutex);
    QVector<PluginCommand> commands;
    
    for (auto plugin : m_plugins) {
        if (plugin->state() == IPlugin::Running) {
            commands.append(plugin->commands());
        }
    }
    return commands;
}

// Status Message Forwarding
void PluginManager::forwardStatusMessage(const QString& msg, int timeout)
{
    emit statusMessageRequested(msg, timeout);
}
