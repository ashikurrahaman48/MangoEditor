#ifndef PLUGIN_INTERFACE_H
#define PLUGIN_INTERFACE_H

#include <QObject>
#include <QtPlugin>
#include <QString>
#include <QVector>
#include <QWidget>
#include <QMenu>
#include <QIcon>
#include <QKeySequence>
#include <functional>

class EditorCore;

struct PluginCommand {
    QString name;
    QString id;
    QString category;
    QKeySequence shortcut;
    std::function<void()> execute;
    QString iconPath;
    QString toolTip;
    bool isEnabled = true;
};

struct PluginDependency {
    QString pluginId;
    QString minVersion;
    QString maxVersion;
    bool isOptional = false;
    QString description;
};

class IPlugin : public QObject
{
    Q_OBJECT

public:
    enum PluginType {
        BuiltIn,
        Extension,
        Theme,
        LanguageSupport,
        Documentation,
        Debugger
    };

    enum PluginState {
        NotLoaded,
        Initialized,
        Running,
        Suspended,
        Crashed
    };

    virtual ~IPlugin() = default;

    // Metadata
    virtual QString pluginName() const = 0;
    virtual QString pluginId() const { return pluginName().toLower().replace(" ", "_"); }
    virtual QString pluginVersion() const = 0;
    virtual QString author() const { return ""; }
    virtual QString description() const { return ""; }
    virtual QIcon pluginIcon() const { return QIcon(); }
    virtual QString license() const { return ""; }
    virtual QString website() const { return ""; }

    // Lifecycle
    virtual void initialize(EditorCore* core) = 0;
    virtual void shutdown() {}
    virtual bool canShutdown() const { return true; }
    virtual PluginState state() const { return m_state; }

    // UI Integration
    virtual QVector<PluginCommand> commands() const { return {}; }
    virtual QWidget* createToolWidget() { return nullptr; }
    virtual QMenu* createContextMenu() { return nullptr; }
    virtual QDockWidget* createDockWidget() { return nullptr; }
    virtual QToolBar* createToolBar() { return nullptr; }

    // Configuration
    virtual bool hasConfiguration() const { return false; }
    virtual void showConfigurationDialog(QWidget* parent) { Q_UNUSED(parent); }
    virtual QJsonObject saveConfiguration() const { return {}; }
    virtual void loadConfiguration(const QJsonObject& config) { Q_UNUSED(config); }

    // Plugin System
    virtual PluginType pluginType() const { return Extension; }
    virtual QVector<PluginDependency> dependencies() const { return {}; }
    virtual QStringList requiredExtensions() const { return {}; }
    virtual bool isHotLoadable() const { return false; }
    virtual bool isThreadSafe() const { return false; }

    // Performance
    virtual void benchmark() {}
    virtual int performanceImpact() const { return 0; }

signals:
    void requestReload();
    void statusMessageRequested(const QString& message, int timeout);
    void commandStateChanged(const QString& commandId, bool enabled);
    void pluginError(const QString& errorMessage);

public slots:
    virtual void onEditorTextChanged() {}
    virtual void onFileOpened(const QString& filePath) { Q_UNUSED(filePath); }
    virtual void onFileSaved(const QString& filePath) { Q_UNUSED(filePath); }
    virtual void onCursorPositionChanged(int line, int col) { Q_UNUSED(line); Q_UNUSED(col); }
    virtual void onProjectLoaded(const QString& projectPath) { Q_UNUSED(projectPath); }
    virtual void onThemeChanged(const QString& themeName) { Q_UNUSED(themeName); }

protected:
    void setState(PluginState state) { m_state = state; }

private:
    PluginState m_state = NotLoaded;
};

Q_DECLARE_INTERFACE(IPlugin, "org.mangoeditor.IPlugin/3.0")

#endif // PLUGIN_INTERFACE_H
