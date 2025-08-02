#include "editor_core.h"
#include "utilities/logger.h"
#include "utilities/file_io.h"
#include "utilities/text_utils.h"
#include "plugins/manager.h"
#include "syntax/highlighter.h"
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>
#include <QElapsedTimer>
#include <thread>

// Private Implementation Classes ===============================================

class EditorCore::DocumentBuffer {
public:
    QStringList lines;
    QString encoding = "UTF-8";
    QHash<int, QByteArray> lineHashes; // For change detection
    
    void updateHash(int line) {
        lineHashes[line] = qHash(lines[line]);
    }
};

class EditorCore::UndoStack {
public:
    struct ComplexEditAction {
        QVector<TextChange> changes;
        QString description;
        QDateTime timestamp;
    };
    
    QVector<ComplexEditAction> stack;
    int index = -1;
    bool inMacro = false;
    ComplexEditAction currentMacro;
};

class EditorCore::PluginManager : public QObject {
    Q_OBJECT
public:
    QHash<QString, IPlugin*> plugins;
    
    void loadBuiltinPlugins() {
        loadPlugin(":/plugins/spellcheck.plugin");
        loadPlugin(":/plugins/bangla_nlp.plugin");
    }
    
    // ... plugin management implementation
};

class EditorCore::MacroRecorder {
public:
    QVector<TextChange> changes;
    bool isRecording = false;
    
    void record(const TextChange& change) {
        if (isRecording) {
            changes.append(change);
        }
    }
};

// EditorCore Implementation ==================================================

EditorCore::EditorCore(QObject *parent) 
    : QObject(parent),
      m_buffer(std::make_unique<DocumentBuffer>()),
      m_undoStack(std::make_unique<UndoStack>()),
      m_highlighter(std::make_unique<SyntaxHighlighter>()),
      m_pluginManager(std::make_unique<PluginManager>()),
      m_macroRecorder(std::make_unique<MacroRecorder>()),
      m_modified(false),
      m_bulkOperation(false)
{
    qInfo().nospace() << "Initializing EditorCore (v" << MANGOEDITOR_VERSION << ")";
    
    // Initialize with one empty line
    m_buffer->lines.append("");
    m_buffer->updateHash(0);
    
    setupDefaultLanguages();
    connectSignals();
    
    // Async initialization
    QTimer::singleShot(0, this, &EditorCore::delayedInitialization);
}

EditorCore::~EditorCore() {
    qDebug() << "Shutting down EditorCore";
}

// Document Management ========================================================

bool EditorCore::loadFile(const QString &filePath) {
    QWriteLocker locker(&m_docLock);
    QElapsedTimer timer;
    timer.start();
    
    if (filePath.isEmpty()) {
        qWarning() << "Empty file path provided";
        return false;
    }

    QString content;
    if (!FileIO::readTextFile(filePath, content, m_buffer->encoding)) {
        qCritical() << "Failed to read file:" << filePath;
        return false;
    }

    beginBulkOperation();
    m_buffer->lines = TextUtils::splitPreserveNewlines(content);
    m_buffer->lineHashes.clear();
    
    for (int i = 0; i < m_buffer->lines.size(); ++i) {
        m_buffer->updateHash(i);
    }
    
    m_currentFile = filePath;
    m_modified = false;
    endBulkOperation();

    // Auto-detect language
    QString ext = QFileInfo(filePath).suffix();
    setLanguage(ext);

    emit fileLoaded(filePath);
    qInfo() << "Loaded" << filePath << "in" << timer.elapsed() << "ms";
    return true;
}

bool EditorCore::saveFile(const QString &filePath) {
    QWriteLocker locker(&m_docLock);
    QString savePath = filePath.isEmpty() ? m_currentFile : filePath;
    
    if (savePath.isEmpty()) {
        qWarning() << "No file path specified for saving";
        return false;
    }

    QString content = m_buffer->lines.join('\n');
    if (!FileIO::writeTextFile(savePath, content, m_buffer->encoding)) {
        qCritical() << "Failed to write file:" << savePath;
        return false;
    }

    if (filePath != m_currentFile) {
        m_currentFile = filePath;
    }

    m_modified = false;
    emit fileSaved(savePath);
    return true;
}

// Text Operations ============================================================

void EditorCore::insertText(int line, int column, const QString &text) {
    if (line < 0 || line >= m_buffer->lines.size()) {
        qWarning() << "Invalid line number:" << line;
        return;
    }

    QString &currentLine = m_buffer->lines[line];
    if (column < 0 || column > currentLine.length()) {
        qWarning() << "Invalid column position:" << column;
        return;
    }

    // Create undo action
    TextChange change;
    change.position = {line, column};
    change.newText = text;
    change.timestamp = QDateTime::currentMSecsSinceEpoch();
    
    if (m_undoStack->inMacro) {
        m_undoStack->currentMacro.changes.append(change);
    } else {
        UndoStack::ComplexEditAction action;
        action.changes = {change};
        action.timestamp = QDateTime::currentDateTime();
        
        // Truncate redo stack
        m_undoStack->stack.truncate(m_undoStack->index + 1);
        m_undoStack->stack.append(action);
        m_undoStack->index++;
    }

    // Record for macro
    m_macroRecorder->record(change);

    // Perform edit
    currentLine.insert(column, text);
    m_buffer->updateHash(line);
    m_modified = true;

    if (!m_bulkOperation) {
        emit textChanged();
        emit modificationChanged(true);
        emit cursorPositionChanged(line, column + text.length());
    }
}

// Multi-Cursor Support ======================================================

void EditorCore::addSecondaryCursor(int line, int column) {
    QWriteLocker locker(&m_docLock);
    // ... implementation
}

// Undo/Redo System ==========================================================

void EditorCore::undo() {
    QWriteLocker locker(&m_docLock);
    
    if (!canUndo()) return;
    
    const auto &action = m_undoStack->stack[m_undoStack->index--];
    beginBulkOperation();
    
    for (const auto &change : action.changes) {
        auto &line = m_buffer->lines[change.position.line];
        line.replace(change.position.column, change.newText.length(), change.oldText);
        m_buffer->updateHash(change.position.line);
    }
    
    endBulkOperation();
    emit modificationChanged(true);
}

// Plugin System =============================================================

void EditorCore::initializePlugins() {
    QWriteLocker locker(&m_docLock);
    
    // Load Bangla NLP plugin for Bangladeshi users
    if (QLocale::system().language() == QLocale::Bengali) {
        loadPlugin(":/plugins/bangla_nlp.plugin");
    }
    
    m_pluginManager->loadBuiltinPlugins();
    qInfo() << "Initialized" << m_pluginManager->plugins.size() << "plugins";
}

// Macro System ==============================================================

void EditorCore::startMacroRecording() {
    QWriteLocker locker(&m_docLock);
    m_macroRecorder->isRecording = true;
    m_macroRecorder->changes.clear();
    qDebug() << "Started macro recording";
}

void EditorCore::stopMacroRecording(bool execute) {
    QWriteLocker locker(&m_docLock);
    m_macroRecorder->isRecording = false;
    
    if (execute && !m_macroRecorder->changes.empty()) {
        // ... execute recorded changes
    }
}

// Private Helpers ===========================================================

void EditorCore::connectSignals() {
    connect(this, &EditorCore::textChanged, this, [this]() {
        if (!m_bulkOperation) {
            m_highlighter->highlightBuffer(m_buffer->lines);
        }
    });
    
    // Auto-save snapshot every 5 minutes
    QTimer *snapshotTimer = new QTimer(this);
    connect(snapshotTimer, &QTimer::timeout, this, [this]() {
        if (m_modified) {
            createSnapshot("auto_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmm"));
        }
    });
    snapshotTimer->start(5 * 60 * 1000);
}

void EditorCore::setupDefaultLanguages() {
    m_highlighter->addLanguage("cpp", ":/syntax/cpp.json");
    m_highlighter->addLanguage("python", ":/syntax/python.json");
    m_highlighter->addLanguage("bn", ":/syntax/bangla.json"); // Bangla syntax support
}

void EditorCore::delayedInitialization() {
    std::thread([this]() {
        initializePlugins();
        checkForBanglaSupport();
    }).detach();
}

void EditorCore::checkForBanglaSupport() {
    if (QLocale::system().language() == QLocale::Bengali) {
        qInfo() << "Detected Bangla locale - enabling enhanced support";
        // Load additional Bangla-specific resources
    }
}

// Debug Operators ============================================================

QDebug operator<<(QDebug debug, const EditorCore::CursorPosition &pos) {
    debug.nospace() << "Line " << pos.line << ", Col " << pos.column;
    return debug;
}

QDebug operator<<(QDebug debug, const EditorCore::SelectionRange &range) {
    debug.nospace() << "Start: " << range.start << " - End: " << range.end;
    return debug;
}
