/**
 * MangoEditor - Main Application Entry Point
 * Version: 2.1.0
 * License: MIT
 * Description: Primary execution point for MangoEditor application
 */

#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>
#include <QStyleFactory>
#include <QSharedMemory>
#include <QTranslator>
#include <QElapsedTimer>
#include <csignal>

#ifdef QT_DEBUG
#include <vld.h> // Visual Leak Detector for Windows
#endif

#include "editor_core.h"
#include "ui/main_window.h"
#include "utilities/logger.h"
#include "utilities/settings.h"
#include "utilities/crash_handler.h"

// Global pointers for crash handling
MainWindow* g_mainWindow = nullptr;
EditorCore* g_editorCore = nullptr;

void signalHandler(int signal) {
    Logger::saveCrashReport(signal);
    if (g_mainWindow) {
        g_mainWindow->emergencySave();
    }
    exit(signal);
}

int main(int argc, char *argv[]) {
    // High DPI support
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    
    // Single instance check
    QSharedMemory shared("MangoEditorInstance");
    if (!shared.create(1)) {
        QMessageBox::warning(nullptr, 
            QObject::tr("Application Already Running"),
            QObject::tr("MangoEditor is already running. Only one instance is allowed."));
        return EXIT_SUCCESS;
    }

    // Initialize application
    QApplication app(argc, argv);
    app.setApplicationName("MangoEditor");
    app.setApplicationVersion("2.1.0");
    app.setOrganizationName("MangoSoft");
    app.setOrganizationDomain("mangoeditor.org.bd");
    app.setWindowIcon(QIcon(":/icons/app_icon.png"));

    // Setup signal handlers
    std::signal(SIGSEGV, signalHandler);
    std::signal(SIGABRT, signalHandler);
    std::signal(SIGFPE, signalHandler);

    // Performance monitoring
    QElapsedTimer startupTimer;
    startupTimer.start();

    // Initialize logging system
    Logger::init();
    qInfo().noquote() << QString("Starting MangoEditor %1").arg(app.applicationVersion());
    qInfo() << "System locale:" << QLocale::system().name();

    // Load translations
    QTranslator translator;
    if (translator.load(":/translations/mangoeditor_" + QLocale::system().name())) {
        app.installTranslator(&translator);
        qInfo() << "Loaded translation for locale:" << QLocale::system().name();
    }

    // Show splash screen (min 1.5s)
    QPixmap splashPix(":/splash.png");
    QSplashScreen splash(splashPix.scaled(600, 400, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    splash.show();
    app.processEvents();

    // Load application settings
    SettingsManager settings;
    QString theme = settings.get("ui/theme", "dark").toString();
    qDebug() << "Loaded theme:" << theme;

    // Parse command line arguments
    QCommandLineParser parser;
    parser.setApplicationDescription("MangoEditor - A modern cross-platform code editor");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("files", QObject::tr("Files to open"), "[files...]");

    QCommandLineOption newWindowOption("n", QObject::tr("Open in new window"));
    QCommandLineOption portableOption("p", QObject::tr("Run in portable mode"));
    QCommandLineOption safeModeOption("safe-mode", QObject::tr("Run without plugins"));

    parser.addOption(newWindowOption);
    parser.addOption(portableOption);
    parser.addOption(safeModeOption);
    parser.process(app);

    try {
        // Initialize core components
        EditorCore core;
        g_editorCore = &core;
        
        if (!parser.isSet(safeModeOption)) {
            core.initializePlugins();
            qInfo() << "Plugins initialized successfully";
        }

        // Create main window with proper theme
        MainWindow window(&core);
        g_mainWindow = &window;
        window.applyTheme(theme);

        // Handle file opening
        const QStringList args = parser.positionalArguments();
        if (!args.empty()) {
            for (const QString& file : args) {
                if (parser.isSet(newWindowOption)) {
                    MainWindow *newWindow = new MainWindow(&core);
                    newWindow->applyTheme(theme);
                    newWindow->openFile(file);
                    newWindow->show();
                } else {
                    window.openFile(file);
                }
            }
        }

        // Check for updates on startup
        if (settings.get("updates/check_on_startup", true).toBool()) {
            QTimer::singleShot(3000, [&window]() {
                window.checkForUpdates(false); // Silent check
            });
        }

        // Finalize startup
        splash.finish(&window);
        window.show();
        qInfo() << "Application started in" << startupTimer.elapsed() << "ms";

        return app.exec();

    } catch (const std::exception& e) {
        qCritical() << "Fatal error:" << e.what();
        QMessageBox::critical(nullptr, 
                            QObject::tr("Application Error"), 
                            QObject::tr("A critical error occurred:\n%1").arg(e.what()));
        return EXIT_FAILURE;
    }
}
