#ifndef TAB_SYSTEM_H
#define TAB_SYSTEM_H

#include <QTabWidget>
#include <QString>
#include <QFileInfo>
#include <QVector>
#include <QPoint>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QContextMenuEvent>
#include "editor_core.h"

class CodeEditor; // Forward declaration

class TabSystem : public QTabWidget {
    Q_OBJECT

public:
    explicit TabSystem(EditorCore* core, QWidget* parent = nullptr);
    ~TabSystem() = default;

    // Tab management
    int addNewTab(const QString& title = "Untitled", const QString& content = "");
    bool loadFileToTab(const QString& filePath);
    bool saveCurrentTab();
    bool saveTabAs(int index);
    void closeTab(int index);
    void closeAllTabs();
    void closeOtherTabs(int index);
    
    // Current tab info
    CodeEditor* currentEditor() const;
    QString currentFilePath() const;
    bool isCurrentTabModified() const;
    int findTabByPath(const QString& path) const;
    
    // Tab state
    void setTabModified(int index, bool modified);
    void updateTabTitle(int index);

    // Split view
    void splitHorizontally();
    void splitVertically();

    // Session management
    void saveSession();
    void restoreSession();

public slots:
    void onEditorContentChanged();
    void updateCursorPosition();
    void showTabPreview(int index);

signals:
    void fileOpened(const QString& path);
    void fileSaved(const QString& path);
    void tabCountChanged(int count);
    void tabContextMenuRequested(int index, const QPoint& pos);
    void splitViewRequested(Qt::Orientation orientation);

protected:
    void tabInserted(int index) override;
    void tabRemoved(int index) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    // Tab control methods
    CodeEditor* createEditor();
    void setupTabConnections(CodeEditor* editor);
    QString generateTabTitle(const QString& filePath) const;
    void setupTabBar();
    
    // Tab data management
    struct TabData {
        QString filePath;
        bool isModified;
    };
    
    EditorCore* m_core;
    QVector<TabData> m_tabData;
    QPoint m_dragStartPos;
};

#endif // TAB_SYSTEM_H
