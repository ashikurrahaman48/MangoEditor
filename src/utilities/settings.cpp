#include "settings.h"
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMessageBox>

// Singleton instance initialization
SettingsManager* SettingsManager::m_instance = nullptr;
QMutex SettingsManager::m_mutex;
QNetworkAccessManager* SettingsManager::m_networkManager = nullptr;

SettingsManager::SettingsManager(QObject *parent) 
    : QObject(parent),
      m_cloudSyncEnabled(false),
      m_settingsVersion(1)
{
    // Set up configuration directory
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(configDir);
    m_settingsPath = configDir + "/mangoeditor.ini";
    
    // Initialize settings
    m_settings = new QSettings(m_settingsPath, QSettings::IniFormat);
    
    // Initialize network manager for cloud sync
    m_networkManager = new QNetworkAccessManager(this);
    
    // Check for settings migration
    checkForMigration();
    
    // Set default values
    setDefault("editor/font_family", "Consolas");
    setDefault("editor/font_size", 12);
    setDefault("editor/theme", "dark");
    setDefault("editor/tab_width", 4);
    setDefault("editor/word_wrap", false);
    setDefault("window/geometry", QByteArray());
    setDefault("window/state", QByteArray());
    setDefault("window/maximized", false);
    setDefault("recent_files", QStringList());
    setDefault("recent_files/max_count", 10);
    setDefault("auto_save/enabled", true);
    setDefault("auto_save/interval", 5); // minutes
    setDefault("cloud_sync/enabled", false);
    setDefault("cloud_sync/last_sync", QDateTime());
    setDefault("version", m_settingsVersion);
}

SettingsManager::~SettingsManager()
{
    m_settings->sync();
    delete m_settings;
}

SettingsManager* SettingsManager::instance()
{
    QMutexLocker locker(&m_mutex);
    if (!m_instance) {
        m_instance = new SettingsManager();
    }
    return m_instance;
}

QVariant SettingsManager::get(const QString &key, const QVariant &defaultValue) const
{
    QMutexLocker locker(&m_mutex);
    return m_settings->value(key, defaultValue);
}

void SettingsManager::set(const QString &key, const QVariant &value)
{
    if (!validateSetting(key, value)) {
        qWarning() << "Invalid setting value for key" << key << "value" << value;
        return;
    }

    QMutexLocker locker(&m_mutex);
    m_settings->setValue(key, value);
    emit settingChanged(key, value);
    
    // Auto-save to cloud if enabled
    if (m_cloudSyncEnabled && key.startsWith("cloud_sync/")) {
        QTimer::singleShot(1000, this, [this]() {
            syncWithCloudStorage();
        });
    }
}

void SettingsManager::remove(const QString &key)
{
    QMutexLocker locker(&m_mutex);
    m_settings->remove(key);
    emit settingChanged(key, QVariant());
}

void SettingsManager::setDefault(const QString &key, const QVariant &value)
{
    if (!m_settings->contains(key)) {
        set(key, value);
    }
}

void SettingsManager::sync()
{
    QMutexLocker locker(&m_mutex);
    m_settings->sync();
}

// Editor settings
QFont SettingsManager::editorFont() const
{
    QString family = get("editor/font_family", "Consolas").toString();
    int size = get("editor/font_size", 12).toInt();
    return QFont(family, size);
}

void SettingsManager::setEditorFont(const QFont &font)
{
    set("editor/font_family", font.family());
    set("editor/font_size", font.pointSize());
}

QString SettingsManager::editorTheme() const
{
    return get("editor/theme", "dark").toString();
}

void SettingsManager::setEditorTheme(const QString &theme)
{
    set("editor/theme", theme);
}

int SettingsManager::editorTabWidth() const
{
    return get("editor/tab_width", 4).toInt();
}

void SettingsManager::setEditorTabWidth(int width)
{
    set("editor/tab_width", width);
}

bool SettingsManager::editorWordWrap() const
{
    return get("editor/word_wrap", false).toBool();
}

void SettingsManager::setEditorWordWrap(bool enabled)
{
    set("editor/word_wrap", enabled);
}

// Window state management
QByteArray SettingsManager::windowGeometry() const
{
    return get("window/geometry").toByteArray();
}

void SettingsManager::setWindowGeometry(const QByteArray &geometry)
{
    set("window/geometry", geometry);
}

QByteArray SettingsManager::windowState() const
{
    return get("window/state").toByteArray();
}

void SettingsManager::setWindowState(const QByteArray &state)
{
    set("window/state", state);
}

bool SettingsManager::windowMaximized() const
{
    return get("window/maximized", false).toBool();
}

void SettingsManager::setWindowMaximized(bool maximized)
{
    set("window/maximized", maximized);
}

// Recent files management
QStringList SettingsManager::recentFiles() const
{
    return get("recent_files").toStringList();
}

void SettingsManager::setRecentFiles(const QStringList &files)
{
    set("recent_files", files);
}

int SettingsManager::maxRecentFiles() const
{
    return get("recent_files/max_count", 10).toInt();
}

void SettingsManager::setMaxRecentFiles(int count)
{
    set("recent_files/max_count", count);
}

void SettingsManager::addRecentFile(const QString &filePath)
{
    QStringList files = recentFiles();
    files.removeAll(filePath);
    files.prepend(filePath);
    
    // Keep only max allowed files
    while (files.size() > maxRecentFiles()) {
        files.removeLast();
    }
    
    setRecentFiles(files);
}

// Auto-save settings
bool SettingsManager::autoSaveEnabled() const
{
    return get("auto_save/enabled", true).toBool();
}

void SettingsManager::setAutoSaveEnabled(bool enabled)
{
    set("auto_save/enabled", enabled);
}

int SettingsManager::autoSaveInterval() const
{
    return get("auto_save/interval", 5).toInt();
}

void SettingsManager::setAutoSaveInterval(int minutes)
{
    set("auto_save/interval", minutes);
}

// Cloud sync settings
bool SettingsManager::cloudSyncEnabled() const
{
    return get("cloud_sync/enabled", false).toBool();
}

void SettingsManager::setCloudSyncEnabled(bool enabled)
{
    if (enabled != m_cloudSyncEnabled) {
        m_cloudSyncEnabled = enabled;
        set("cloud_sync/enabled", enabled);
        if (enabled) {
            syncWithCloudStorage();
        }
    }
}

QDateTime SettingsManager::lastCloudSync() const
{
    return get("cloud_sync/last_sync").toDateTime();
}

// Migration functions
void SettingsManager::checkForMigration()
{
    int storedVersion = get("version", 0).toInt();
    if (storedVersion < m_settingsVersion) {
        migrateFromPreviousVersion(storedVersion);
        set("version", m_settingsVersion);
    }
}

void SettingsManager::migrateFromPreviousVersion(int oldVersion)
{
    qInfo() << "Migrating settings from version" << oldVersion << "to" << m_settingsVersion;
    
    // Migration from version 0 to 1
    if (oldVersion < 1) {
        // Convert old theme format to new format
        QString oldTheme = get("theme").toString();
        if (!oldTheme.isEmpty()) {
            set("editor/theme", oldTheme);
            remove("theme");
        }
    }
}

// Import/Export functions
bool SettingsManager::exportSettings(const QString &filePath)
{
    QMutexLocker locker(&m_mutex);
    
    try {
        // Get all settings as a map
        QVariantMap settingsMap;
        foreach (const QString &key, m_settings->allKeys()) {
            settingsMap[key] = m_settings->value(key);
        }
        
        // Create JSON document
        QJsonDocument doc(QJsonObject::fromVariantMap(settingsMap));
        
        // Write to file
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            qWarning() << "Failed to open file for writing:" << filePath;
            return false;
        }
        
        file.write(doc.toJson());
        file.close();
        return true;
    } catch (...) {
        qWarning() << "Exception during settings export";
        return false;
    }
}

bool SettingsManager::importSettings(const QString &filePath)
{
    QMutexLocker locker(&m_mutex);
    
    try {
        // Read file
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "Failed to open file for reading:" << filePath;
            return false;
        }
        
        QByteArray data = file.readAll();
        file.close();
        
        // Parse JSON
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull()) {
            qWarning() << "Invalid JSON in settings file";
            return false;
        }
        
        // Apply settings
        QVariantMap settingsMap = doc.object().toVariantMap();
        foreach (const QString &key, settingsMap.keys()) {
            if (validateSetting(key, settingsMap[key])) {
                m_settings->setValue(key, settingsMap[key]);
                emit settingChanged(key, settingsMap[key]);
            }
        }
        
        return true;
    } catch (...) {
        qWarning() << "Exception during settings import";
        return false;
    }
}

// Cloud sync functions
void SettingsManager::syncWithCloudStorage()
{
    if (!m_cloudSyncEnabled) return;

    // Get all settings as a map
    QVariantMap settingsMap;
    foreach (const QString &key, m_settings->allKeys()) {
        settingsMap[key] = m_settings->value(key);
    }
    
    // Create JSON document
    QJsonDocument doc(QJsonObject::fromVariantMap(settingsMap));
    
    // Send to cloud
    QNetworkRequest request(QUrl("https://api.mangoeditor.com/sync/settings"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QNetworkReply *reply = m_networkManager->post(request, doc.toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            set("cloud_sync/last_sync", QDateTime::currentDateTime());
            qInfo() << "Settings synced with cloud";
        } else {
            qWarning() << "Cloud sync failed:" << reply->errorString();
        }
        reply->deleteLater();
    });
}

// Validation functions
bool SettingsManager::validateSetting(const QString &key, const QVariant &value)
{
    // Validate editor/font_size
    if (key == "editor/font_size") {
        int size = value.toInt();
        return size >= 6 && size <= 72;
    }
    
    // Validate editor/tab_width
    if (key == "editor/tab_width") {
        int width = value.toInt();
        return width >= 1 && width <= 8;
    }
    
    // Validate auto_save/interval
    if (key == "auto_save/interval") {
        int minutes = value.toInt();
        return minutes >= 1 && minutes <= 60;
    }
    
    // Validate recent_files/max_count
    if (key == "recent_files/max_count") {
        int count = value.toInt();
        return count >= 1 && count <= 50;
    }
    
    // Basic type validation for other settings
    if (key.startsWith("editor/")) {
        if (key.endsWith("font_family")) return value.canConvert<QString>();
        if (key.endsWith("theme")) return value.canConvert<QString>();
        if (key.endsWith("word_wrap")) return value.canConvert<bool>();
    }
    
    return true;
}

// Plugin settings
void SettingsManager::registerPluginSettings(const QString &pluginId, const QVariantMap &defaults)
{
    QMutexLocker locker(&m_mutex);
    
    // Set defaults if not already set
    foreach (const QString &key, defaults.keys()) {
        QString fullKey = QString("plugins/%1/%2").arg(pluginId).arg(key);
        setDefault(fullKey, defaults[key]);
    }
}

QVariant SettingsManager::getPluginSetting(const QString &pluginId, const QString &key, const QVariant &defaultValue) const
{
    QString fullKey = QString("plugins/%1/%2").arg(pluginId).arg(key);
    return get(fullKey, defaultValue);
}

void SettingsManager::setPluginSetting(const QString &pluginId, const QString &key, const QVariant &value)
{
    QString fullKey = QString("plugins/%1/%2").arg(pluginId).arg(key);
    set(fullKey, value);
}
