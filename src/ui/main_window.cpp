#include "main_window.h"
#include "ui_main_window.h"
#include "editor_core.h"
#include "syntax/highlighter.h"
#include "utilities/settings.h"
#include "plugins/plugin_manager.h"
#include "version_control/git_integration.h"
#include "debugger/debug_interface.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QToolBar>
#include <QMenuBar>
#include <QDockWidget>
#include <QInputDialog>
#include <QShortcut>

MainWindow::MainWindow(EditorCore* core, QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      m_core(core),
      m_settings(SettingsManager::instance()),
      m_pluginManager(new PluginManager(this)),
      m_git(new GitIntegration(this)),
      m_debugger(new DebugInterface(this))
{
    ui->setupUi(this);
    
    // Initialize all UI components
    initializeUI();
    
    // Load application settings
    loadSettings();
    
    // Initialize with default empty document
    newDocument();
    
    // Setup version control if repository exists
    detectVersionControl();
}

MainWindow::~MainWindow()
{
    saveSettings();
    delete ui;
}

void MainWindow::initializeUI()
{
    setupMenuBar();
    setupToolBars();
    setupStatusBar();
    setupEditor();
    setupTabSystem();
    setupDockWidgets();
    setupPluginUI();
    setupShortcuts();
    setupConnections();
}

void MainWindow::setupMenuBar()
{
    // File menu
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("&New"), this, &MainWindow::newDocument, QKeySequence::New);
    fileMenu->addAction(tr("&Open..."), this, &MainWindow::openDocument, QKeySequence::Open);
    fileMenu->addAction(tr("&Save"), this, &MainWindow::saveDocument, QKeySequence::Save);
    fileMenu->addAction(tr("Save &As..."), this, &MainWindow::saveAsDocument, QKeySequence::SaveAs);
    fileMenu->addSeparator();
    
    // Project submenu
    QMenu* projectMenu = fileMenu->addMenu(tr("&Project"));
    projectMenu->addAction(tr("New Project..."), this, &MainWindow::newProject);
    projectMenu->addAction(tr("Open Project..."), this, &MainWindow::openProject);
    projectMenu->addAction(tr("Close Project"), this, &MainWindow::closeProject);
    
    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), this, &QMainWindow::close, QKeySequence::Quit);

    // Edit menu
    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(tr("&Undo"), this, &MainWindow::undo, QKeySequence::Undo);
    editMenu->addAction(tr("&Redo"), this, &MainWindow::redo, QKeySequence::Redo);
    editMenu->addSeparator();
    editMenu->addAction(tr("&Cut"), this, &MainWindow::cut, QKeySequence::Cut);
    editMenu->addAction(tr("C&opy"), this, &MainWindow::copy, QKeySequence::Copy);
    editMenu->addAction(tr("&Paste"), this, &MainWindow::paste, QKeySequence::Paste);
    editMenu->addAction(tr("&Find"), this, &MainWindow::showFindDialog, QKeySequence::Find);
    editMenu->addAction(tr("Find &Next"), this, &MainWindow::findNext, QKeySequence::FindNext);
    editMenu->addAction(tr("Find Pre&vious"), this, &MainWindow::findPrevious, QKeySequence::FindPrevious);
    editMenu->addAction(tr("&Replace"), this, &MainWindow::showReplaceDialog, QKeySequence::Replace);

    // View menu
    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    m_themeMenu = viewMenu->addMenu(tr("&Theme"));
    setupThemeMenu();
    viewMenu->addAction(tr("&Zoom In"), this, &MainWindow::zoomIn, QKeySequence::ZoomIn);
    viewMenu->addAction(tr("&Zoom Out"), this, &MainWindow::zoomOut, QKeySequence::ZoomOut);
    viewMenu->addSeparator();
    viewMenu->addAction(m_projectDock->toggleViewAction());
    viewMenu->addAction(m_pluginDock->toggleViewAction());
    viewMenu->addAction(m_debugDock->toggleViewAction());
    viewMenu->addAction(m_vcsDock->toggleViewAction());

    // Version Control menu
    QMenu* vcsMenu = menuBar()->addMenu(tr("&Version"));
    vcsMenu->addAction(tr("Initialize Repository"), this, &MainWindow::initRepository);
    vcsMenu->addAction(tr("Commit Changes"), this, &MainWindow::commitChanges);
    vcsMenu->addAction(tr("Push Changes"), this, &MainWindow::pushChanges);
    vcsMenu->addAction(tr("Pull Changes"), this, &MainWindow::pullChanges);
    vcsMenu->addSeparator();
    vcsMenu->addAction(tr("Show History"), this, &MainWindow::showHistory);

    // Debug menu
    QMenu* debugMenu = menuBar()->addMenu(tr("&Debug"));
    debugMenu->addAction(tr("Start Debugging"), this, &MainWindow::startDebugging, QKeySequence(Qt::Key_F5));
    debugMenu->addAction(tr("Stop Debugging"), this, &MainWindow::stopDebugging, QKeySequence(Qt::ShiftModifier | Qt::Key_F5));
    debugMenu->addAction(tr("Step Over"), this, &MainWindow::stepOver, QKeySequence(Qt::Key_F10));
    debugMenu->addAction(tr("Step Into"), this, &MainWindow::stepInto, QKeySequence(Qt::Key_F11));
    debugMenu->addAction(tr("Step Out"), this, &MainWindow::stepOut, QKeySequence(Qt::ShiftModifier | Qt::Key_F11));
    debugMenu->addSeparator();
    debugMenu->addAction(tr("Toggle Breakpoint"), this, &MainWindow::toggleBreakpoint, QKeySequence(Qt::Key_F9));

    // Plugins menu
    QMenu* pluginMenu = menuBar()->addMenu(tr("&Plugins"));
    pluginMenu->addAction(tr("Manage Plugins..."), this, &MainWindow::managePlugins);
    pluginMenu->addAction(tr("Reload Plugins"), this, &MainWindow::reloadPlugins);

    // Help menu
    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&Documentation"), this, &MainWindow::showDocumentation);
    helpMenu->addAction(tr("&About"), this, &MainWindow::about);
    helpMenu->addAction(tr("About &Qt"), this, &QApplication::aboutQt);
}

void MainWindow::setupToolBars()
{
    // Main toolbar
    QToolBar* mainToolBar = addToolBar(tr("Main Toolbar"));
    mainToolBar->setObjectName("MainToolBar");
    mainToolBar->setMovable(false);

    // File actions
    mainToolBar->addAction(QIcon(":/icons/new_file.svg"), tr("New"), this, &MainWindow::newDocument);
    mainToolBar->addAction(QIcon(":/icons/open_file.svg"), tr("Open"), this, &MainWindow::openDocument);
    mainToolBar->addAction(QIcon(":/icons/save_file.svg"), tr("Save"), this, &MainWindow::saveDocument);
    mainToolBar->addSeparator();

    // Edit actions
    mainToolBar->addAction(QIcon(":/icons/undo.svg"), tr("Undo"), this, &MainWindow::undo);
    mainToolBar->addAction(QIcon(":/icons/redo.svg"), tr("Redo"), this, &MainWindow::redo);
    mainToolBar->addSeparator();
    mainToolBar->addAction(QIcon(":/icons/cut.svg"), tr("Cut"), this, &MainWindow::cut);
    mainToolBar->addAction(QIcon(":/icons/copy.svg"), tr("Copy"), this, &MainWindow::copy);
    mainToolBar->addAction(QIcon(":/icons/paste.svg"), tr("Paste"), this, &MainWindow::paste);
    mainToolBar->addSeparator();

    // Version control toolbar
    m_vcsToolBar = addToolBar(tr("Version Control"));
    m_vcsToolBar->setObjectName("VersionControlToolBar");
    m_vcsToolBar->addAction(QIcon(":/icons/git_pull.svg"), tr("Pull"), this, &MainWindow::pullChanges);
    m_vcsToolBar->addAction(QIcon(":/icons/git_push.svg"), tr("Push"), this, &MainWindow::pushChanges);
    m_vcsToolBar->addAction(QIcon(":/icons/git_commit.svg"), tr("Commit"), this, &MainWindow::commitChanges);
    m_vcsToolBar->addSeparator();
    m_vcsToolBar->addAction(QIcon(":/icons/git_branch.svg"), tr("Branches"), this, &MainWindow::showBranches);

    // Debug toolbar
    m_debugToolBar = addToolBar(tr("Debug"));
    m_debugToolBar->setObjectName("DebugToolBar");
    m_debugToolBar->addAction(QIcon(":/icons/debug_start.svg"), tr("Start Debugging"), this, &MainWindow::startDebugging);
    m_debugToolBar->addAction(QIcon(":/icons/debug_stop.svg"), tr("Stop Debugging"), this, &MainWindow::stopDebugging);
    m_debugToolBar->addSeparator();
    m_debugToolBar->addAction(QIcon(":/icons/debug_step_over.svg"), tr("Step Over"), this, &MainWindow::stepOver);
    m_debugToolBar->addAction(QIcon(":/icons/debug_step_into.svg"), tr("Step Into"), this, &MainWindow::stepInto);
    m_debugToolBar->addAction(QIcon(":/icons/debug_step_out.svg"), tr("Step Out"), this, &MainWindow::stepOut);
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage(tr("Ready"));
    
    // Line/column indicator
    m_lineColLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_lineColLabel);
    updateLineColDisplay(1, 1);
    
    // File encoding indicator
    m_encodingLabel = new QLabel("UTF-8", this);
    statusBar()->addPermanentWidget(m_encodingLabel);
    
    // VCS branch indicator
    m_vcsBranchLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_vcsBranchLabel);
    
    // Debug status indicator
    m_debugStatusLabel = new QLabel(tr("Not Debugging"), this);
    statusBar()->addPermanentWidget(m_debugStatusLabel);
}

void MainWindow::setupEditor()
{
    // Configure editor widget
    ui->editor->setFont(QFont("Consolas", 12));
    ui->editor->setTabStopDistance(40); // 4 spaces equivalent
    
    // Set up syntax highlighter
    m_highlighter = new SyntaxHighlighter(ui->editor->document());
    
    // Line numbers
    ui->lineNumberArea->setEditor(ui->editor);
    
    // Set up find/replace highlight
    m_searchHighlighter = new SearchHighlighter(ui->editor->document());
}

void MainWindow::setupTabSystem()
{
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setMovable(true);
    setCentralWidget(m_tabWidget);
    
    // Add initial editor tab
    m_tabWidget->addTab(ui->editor, tr("Untitled"));
    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &MainWindow::tabChanged);
}

void MainWindow::setupDockWidgets()
{
    // Project dock
    m_projectDock = new QDockWidget(tr("Project"), this);
    m_projectTree = new QTreeWidget(m_projectDock);
    m_projectDock->setWidget(m_projectTree);
    addDockWidget(Qt::LeftDockWidgetArea, m_projectDock);
    
    // Plugin dock
    m_pluginDock = new QDockWidget(tr("Plugins"), this);
    m_pluginToolbar = new QToolBar(m_pluginDock);
    m_pluginDock->setWidget(m_pluginToolbar);
    addDockWidget(Qt::RightDockWidgetArea, m_pluginDock);
    
    // Debug dock
    m_debugDock = new QDockWidget(tr("Debug"), this);
    m_debugStack = new QTreeWidget(m_debugDock);
    m_debugVars = new QTreeWidget(m_debugDock);
    QTabWidget* debugTabs = new QTabWidget(m_debugDock);
    debugTabs->addTab(m_debugStack, tr("Call Stack"));
    debugTabs->addTab(m_debugVars, tr("Variables"));
    m_debugDock->setWidget(debugTabs);
    addDockWidget(Qt::RightDockWidgetArea, m_debugDock);
    
    // Version control dock
    m_vcsDock = new QDockWidget(tr("Version Control"), this);
    m_vcsChanges = new QTreeWidget(m_vcsDock);
    m_vcsDock->setWidget(m_vcsChanges);
    addDockWidget(Qt::LeftDockWidgetArea, m_vcsDock);
}

void MainWindow::setupPluginUI()
{
    // Load plugins and add their UI components
    m_pluginManager->loadPlugins();
    
    // Add plugin actions to toolbar
    foreach (const PluginInterface* plugin, m_pluginManager->plugins()) {
        if (plugin->toolbarActions().size() > 0) {
            m_pluginToolbar->addSeparator();
            m_pluginToolbar->addActions(plugin->toolbarActions());
        }
    }
}

void MainWindow::setupShortcuts()
{
    // Command palette shortcut
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_P), this, SLOT(showCommandPalette()));
    
    // Save all shortcut
    new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S), this, SLOT(saveAllDocuments()));
    
    // Close tab shortcut
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_W), this, SLOT(closeCurrentTab()));
}

void MainWindow::setupConnections()
{
    // Editor signals
    connect(ui->editor, &QPlainTextEdit::textChanged, this, &MainWindow::documentModified);
    connect(ui->editor, &QPlainTextEdit::cursorPositionChanged, this, &MainWindow::updateCursorPosition);
    
    // Core signals
    connect(m_core, &EditorCore::fileLoaded, this, &MainWindow::fileLoaded);
    connect(m_core, &EditorCore::fileSaved, this, &MainWindow::fileSaved);
    
    // Settings signals
    connect(m_settings, &SettingsManager::settingChanged, this, &MainWindow::applySettings);
    
    // Version control signals
    connect(m_git, &GitIntegration::repositoryChanged, this, &MainWindow::updateVcsStatus);
    
    // Debugger signals
    connect(m_debugger, &DebugInterface::debuggingStarted, this, &MainWindow::onDebuggingStarted);
    connect(m_debugger, &DebugInterface::debuggingStopped, this, &MainWindow::onDebuggingStopped);
    connect(m_debugger, &DebugInterface::breakpointHit, this, &MainWindow::onBreakpointHit);
}

// [Rest of the implementation remains the same as previous example...]
// [All other methods stay unchanged from your original code]

void MainWindow::loadSettings()
{
    // Window geometry and state
    restoreGeometry(m_settings->get("window/geometry").toByteArray());
    restoreState(m_settings->get("window/state").toByteArray());
    
    // Editor settings
    QString fontFamily = m_settings->get("editor/font_family", "Consolas").toString();
    int fontSize = m_settings->get("editor/font_size", 12).toInt();
    ui->editor->setFont(QFont(fontFamily, fontSize));
    
    // Recent files
    m_recentFiles = m_settings->get("recent_files").toStringList();
    updateRecentFilesMenu();
    
    // Dock widget states
    m_projectDock->setVisible(m_settings->get("docks/project/visible", true).toBool());
    m_pluginDock->setVisible(m_settings->get("docks/plugins/visible", true).toBool());
    m_debugDock->setVisible(m_settings->get("docks/debug/visible", false).toBool());
    m_vcsDock->setVisible(m_settings->get("docks/vcs/visible", false).toBool());
    
    // Apply theme
    applyTheme(m_settings->get("editor/theme", "Dark").toString());
}

void MainWindow::saveSettings()
{
    // Window geometry and state
    m_settings->set("window/geometry", saveGeometry());
    m_settings->set("window/state", saveState());
    
    // Editor settings
    m_settings->set("editor/font_family", ui->editor->font().family());
    m_settings->set("editor/font_size", ui->editor->font().pointSize());
    
    // Recent files
    m_settings->set("recent_files", m_recentFiles);
    
    // Dock widget states
    m_settings->set("docks/project/visible", m_projectDock->isVisible());
    m_settings->set("docks/plugins/visible", m_pluginDock->isVisible());
    m_settings->set("docks/debug/visible", m_debugDock->isVisible());
    m_settings->set("docks/vcs/visible", m_vcsDock->isVisible());
}

// [Implement all other methods from the previous example...]

void MainWindow::detectVersionControl()
{
    if (m_git->detectRepository(QDir::currentPath())) {
        m_vcsToolBar->setVisible(true);
        m_vcsDock->setVisible(true);
        updateVcsStatus();
    } else {
        m_vcsToolBar->setVisible(false);
        m_vcsDock->setVisible(false);
    }
}

void MainWindow::updateVcsStatus()
{
    m_vcsBranchLabel->setText(tr("Git: %1").arg(m_git->currentBranch()));
    m_vcsChanges->clear();
    
    // Populate changes tree
    foreach (const GitIntegration::FileStatus& status, m_git->status()) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_vcsChanges);
        item->setText(0, status.filename);
        item->setText(1, status.status);
        item->setIcon(0, QIcon(status.iconPath()));
    }
}

void MainWindow::onDebuggingStarted()
{
    m_debugStatusLabel->setText(tr("Debugging"));
    m_debugToolBar->setEnabled(true);
    ui->editor->setDebugLine(-1); // Clear any previous debug line
}

void MainWindow::onDebuggingStopped()
{
    m_debugStatusLabel->setText(tr("Not Debugging"));
    m_debugToolBar->setEnabled(false);
    ui->editor->setDebugLine(-1);
}

void MainWindow::onBreakpointHit(const QString& file, int line)
{
    if (QFileInfo(file).fileName() == QFileInfo(m_currentFile).fileName()) {
        ui->editor->setDebugLine(line);
    }
    m_debugStack->clear();
    
    // Populate call stack
    foreach (const DebugInterface::StackFrame& frame, m_debugger->callStack()) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_debugStack);
        item->setText(0, frame.function);
        item->setText(1, frame.file);
        item->setText(2, QString::number(frame.line));
    }
    
    // Populate variables
    m_debugVars->clear();
    foreach (const DebugInterface::Variable& var, m_debugger->variables()) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_debugVars);
        item->setText(0, var.name);
        item->setText(1, var.type);
        item->setText(2, var.value);
    }
}
