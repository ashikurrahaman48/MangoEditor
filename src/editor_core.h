#ifndef EDITOR_CORE_H
#define EDITOR_CORE_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QReadWriteLock>
#include <memory>
#include <variant>
#include "syntax/highlighter.h"
#include "plugins/interface.h"

/**
 * @brief The EditorCore class - Central controller for all editor functionality
 * 
 * Manages document state, text operations, plugins, and provides thread-safe
 * access to editor features. Designed for high-performance editing operations.
 */
class EditorCore : public QObject
{
    Q_OBJECT

public:
    // Document position/selection types
    struct CursorPosition {
        int line = 0;
        int column = 0;
        
        bool operator==(const CursorPosition& other) const {
            return line == other.line && column == other.column;
        }
    };

    struct SelectionRange {
        CursorPosition start;
        CursorPosition end;
        
        bool isValid() const { return !(start == end); }
    };

    struct TextChange {
        CursorPosition position;
        QString oldText;
        QString newText;
        int timestamp;
    };

    explicit EditorCore(QObject *parent = nullptr);
    ~EditorCore() override;

    // ==================== Document Management ====================
    bool loadFile(const QString &filePath);
    bool saveFile(const QString &filePath = QString());
    bool saveAs(const QString &filePath);
    QString currentText() const;
    QString currentFilePath() const;
    bool isModified() const;
    void setModified(bool modified);

    // Version control
    bool createSnapshot(const QString &tag = QString());
    QVector<QString> availableSnapshots() const;
    bool restoreSnapshot(const QString &tag);
    
    // ==================== Text Operations ====================
    void insertText(int line, int column, const QString &text);
    void deleteText(int startLine, int startCol, int endLine, int endCol);
    QString getText(int startLine, int startCol, int endLine, int endCol) const;
    QString getLine(int line) const;
    int lineCount() const;
    
    // Bulk operations
    void beginBulkOperation();
    void endBulkOperation();
    bool isBulkOperationActive() const;
    
    // ==================== Cursor/Selection ====================
    CursorPosition cursorPosition() const;
    void setCursorPosition(int line, int column);
    void setCursorPosition(const CursorPosition& pos);
    
    QVector<SelectionRange> selections() const;
    void setSelections(const QVector<SelectionRange>& ranges);
    void addSelection(const SelectionRange& range);
    
    // Multi-caret editing
    void addSecondaryCursor(int line, int column);
    void clearSecondaryCursors();
    QVector<CursorPosition> allCursors() const;
    
    // ==================== Syntax Highlighting ====================
    void setLanguage(const QString &language);
    QString currentLanguage() const;
    SyntaxHighlighter* highlighter() const;
    QVector<QString> availableLanguages() const;
    
    // ==================== Plugin System ====================
    void initializePlugins();
    void loadPlugin(const QString &pluginPath);
    void unloadPlugin(const QString &pluginId);
    QVector<IPlugin*> plugins() const;
    IPlugin* plugin(const QString &pluginId) const;
    
    void notifyPlugins(const QString &event, const QVariantMap &data = {});
    
    // ==================== Undo/Redo ====================
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;
    void clearUndoStack();
    
    // ==================== Macro System ====================
    void startMacroRecording();
    void stopMacroRecording(bool execute = false);
    void playMacro();
    bool isRecordingMacro() const;
    
    // ==================== Thread Safety ====================
    QReadWriteLock &documentLock() const;
    
    // ==================== Signals ====================
signals:
    void textChanged();
    void fileLoaded(const QString &filePath);
    void fileSaved(const QString &filePath);
    void modificationChanged(bool modified);
    void cursorPositionChanged(int line, int column);
    void languageChanged(const QString &language);
    void pluginLoaded(const QString &pluginId);
    void pluginUnloaded(const QString &pluginId);
    
    // For async operations
    void operationCompleted(const QString &operationId, bool success);
    
    // ==================== Public Slots ====================
public slots:
    void updateSettings();
    void applyTextChanges(const QVector<TextChange> &changes);
    void delayedInitialization();

private:
    // PIMPL Pattern for implementation details
    class DocumentBuffer;
    class UndoStack;
    class PluginManager;
    class MacroRecorder;
    
    std::unique_ptr<DocumentBuffer> m_buffer;
    std::unique_ptr<UndoStack> m_undoStack;
    std::unique_ptr<SyntaxHighlighter> m_highlighter;
    std::unique_ptr<PluginManager> m_pluginManager;
    std::unique_ptr<MacroRecorder> m_macroRecorder;
    mutable QReadWriteLock m_docLock;
    
    QString m_currentFile;
    bool m_modified = false;
    bool m_bulkOperation = false;
    
    // Private helpers
    void connectSignals();
    void setupDefaultLanguages();
    void setupDefaultPlugins();
    void emitChangeSignals();
    
    // Async operations
    void performAsyncOperation(const QString &operationId, 
                             std::function<void()> operation);
};

// Utility functions
QDebug operator<<(QDebug debug, const EditorCore::CursorPosition &pos);
QDebug operator<<(QDebug debug, const EditorCore::SelectionRange &range);

#endif // EDITOR_CORE_H
