#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QPlainTextEdit>
#include <QTabWidget>
#include <QDockWidget>
#include <QTreeWidget>
#include <QToolBar>
#include <QMenu>
#include <QLabel>
#include <QStringList>
#include <QActionGroup>
#include <QSettings>
#include <QElapsedTimer>
#include "editor_core.h"
#include "syntax/highlighter.h"
#include "utilities/settings.h"
#include "plugins/plugin_interface.h"
#include "version_control/git_integration.h"
#include "debugger/debug_interface.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(EditorCore* core, QWidget* parent = nullptr);
    ~MainWindow();

    // Core document operations
    bool loadFile(const QString& fileName);
    bool saveFile(const QString& fileName);
    void setCurrentFile(const QString& fileName);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    // File operations
    void newDocument();
    void openDocument();
    bool saveDocument();
    bool saveAsDocument();
    void openRecentFile();
    void newProject();
    void openProject();
    void closeProject();

    // Edit operations
    void undo();
    void redo();
    void cut();
    void copy();
    void paste();
    void showFindDialog();
    void findNext();
    void findPrevious();
    void showReplaceDialog();
    void showCommandPalette();

    // View operations
    void zoomIn();
    void zoomOut();
    void changeTheme(QAction* action);

    // Version control
    void initRepository();
    void commitChanges();
    void pushChanges();
    void pullChanges();
    void showHistory();
    void showBranches();
    void updateVcsStatus();

    // Debugging
    void startDebugging();
    void stopDebugging();
    void stepOver();
    void stepInto();
    void stepOut();
    void toggleBreakpoint();
    void onDebuggingStarted();
    void onDebuggingStopped();
    void onBreakpointHit(const QString& file, int line);

    // Plugins
    void managePlugins();
    void reloadPlugins();

    // Help
    void about();
    void showDocumentation();
    void updateCursorPosition();
    void documentModified();
    void tabChanged(int index);
    void closeTab(int index);
    void closeCurrentTab();
    void saveAllDocuments();

private:
    // UI setup methods
    void initializeUI();
    void setupMenuBar();
    void setupToolBars();
    void setupStatusBar();
    void setupEditor();
    void setupTabSystem();
    void setupDockWidgets();
    void setupPluginUI();
    void setupShortcuts();
    void setupConnections();
    void setupThemeMenu();
    void detectVersionControl();

    // Utility methods
    bool maybeSave();
    void loadSettings();
    void saveSettings();
    void applySettings();
    void applyTheme(const QString& theme);
    void addToRecentFiles(const QString& filePath);
    void updateRecentFilesMenu();
    void updateLineColDisplay(int line, int col);

    // UI components
    Ui::MainWindow* ui;
    EditorCore* m_core;
    SyntaxHighlighter* m_highlighter;
    SearchHighlighter* m_searchHighlighter;
    QLabel* m_lineColLabel;
    QLabel* m_encodingLabel;
    QLabel* m_vcsBranchLabel;
    QLabel* m_debugStatusLabel;
    QTabWidget* m_tabWidget;

    // Menus
    QMenu* m_fileMenu;
    QMenu* m_editMenu;
    QMenu* m_viewMenu;
    QMenu* m_vcsMenu;
    QMenu* m_debugMenu;
    QMenu* m_pluginMenu;
    QMenu* m_helpMenu;
    QMenu* m_themeMenu;

    // Toolbars
    QToolBar* m_mainToolBar;
    QToolBar* m_vcsToolBar;
    QToolBar* m_debugToolBar;

    // Dock widgets
    QDockWidget* m_projectDock;
    QDockWidget* m_pluginDock;
    QDockWidget* m_debugDock;
    QDockWidget* m_vcsDock;
    QTreeWidget* m_projectTree;
    QToolBar* m_pluginToolbar;
    QTreeWidget* m_debugStack;
    QTreeWidget* m_debugVars;
    QTreeWidget* m_vcsChanges;

    // State management
    QString m_currentFile;
    QStringList m_recentFiles;
    QList<QAction*> m_recentFileActions;
    SettingsManager* m_settings;
    PluginManager* m_pluginManager;
    GitIntegration* m_git;
    DebugInterface* m_debugger;

    // Performance tracking utility class
    class PerformanceTracker {
    public:
        explicit PerformanceTracker(const QString& operationName) : 
            m_operation(operationName) {
            m_timer.start();
        }
        
        ~PerformanceTracker() {
            qDebug() << m_operation << "took" << m_timer.elapsed() << "ms";
        }
        
    private:
        QString m_operation;
        QElapsedTimer m_timer;
    };
};

// Macros for easier usage
#define PERF_TRACK(op) MainWindow::PerformanceTracker perfTracker(op)

#endif // MAIN_WINDOW_H
