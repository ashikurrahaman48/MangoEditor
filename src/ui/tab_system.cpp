#include "tab_system.h"
#include "code_editor.h"
#include <QFileInfo>
#include <QMessageBox>
#include <QFileDialog>
#include <QPushButton>
#include <QMenu>
#include <QApplication>
#include <QMimeData>

TabSystem::TabSystem(EditorCore* core, QWidget* parent)
    : QTabWidget(parent), m_core(core)
{
    // Basic tab configuration
    setTabsClosable(true);
    setMovable(true);
    setDocumentMode(true);
    setElideMode(Qt::ElideRight);
    setAcceptDrops(true);
    
    // Connect signals
    connect(this, &QTabWidget::tabCloseRequested, this, &TabSystem::closeTab);
    setupTabBar();
    
    // Add default tab
    addNewTab("Welcome");
}

void TabSystem::setupTabBar()
{
    tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tabBar(), &QTabBar::customContextMenuRequested, [this](const QPoint& pos) {
        int index = tabBar()->tabAt(pos);
        if (index >= 0) {
            emit tabContextMenuRequested(index, tabBar()->mapToGlobal(pos));
        }
    });
}

int TabSystem::addNewTab(const QString& title, const QString& content)
{
    CodeEditor* editor = createEditor();
    editor->setPlainText(content);
    
    int index = addTab(editor, title);
    setCurrentIndex(index);
    m_tabData.insert(index, {QString(), false});
    
    return index;
}

bool TabSystem::loadFileToTab(const QString& filePath)
{
    int existingTab = findTabByPath(filePath);
    if (existingTab >= 0) {
        setCurrentIndex(existingTab);
        return true;
    }
    
    QString content;
    if (!m_core->loadFile(filePath, content)) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to load file"));
        return false;
    }
    
    int index = addNewTab(QFileInfo(filePath).fileName(), content);
    m_tabData[index].filePath = filePath;
    updateTabTitle(index);
    
    emit fileOpened(filePath);
    return true;
}

bool TabSystem::saveCurrentTab()
{
    int index = currentIndex();
    if (index < 0) return false;
    
    if (m_tabData[index].filePath.isEmpty()) {
        return saveTabAs(index);
    }
    return saveTabContent(index);
}

bool TabSystem::saveTabAs(int index)
{
    QString filePath = QFileDialog::getSaveFileName(this, tr("Save As"));
    if (filePath.isEmpty()) return false;
    
    m_tabData[index].filePath = filePath;
    if (saveTabContent(index)) {
        updateTabTitle(index);
        return true;
    }
    return false;
}

bool TabSystem::saveTabContent(int index)
{
    CodeEditor* editor = qobject_cast<CodeEditor*>(widget(index));
    if (!editor) return false;
    
    if (m_core->saveFile(m_tabData[index].filePath, editor->toPlainText())) {
        setTabModified(index, false);
        emit fileSaved(m_tabData[index].filePath);
        return true;
    }
    
    QMessageBox::warning(this, tr("Error"), tr("Failed to save file"));
    return false;
}

void TabSystem::closeTab(int index)
{
    if (index < 0 || index >= count()) return;
    
    if (m_tabData[index].isModified) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, tr("Unsaved Changes"),
            tr("You have unsaved changes. Do you want to save them?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
            
        if (reply == QMessageBox::Save && !saveCurrentTab()) {
            return;
        } else if (reply == QMessageBox::Cancel) {
            return;
        }
    }
    
    QWidget* tabWidget = widget(index);
    removeTab(index);
    tabWidget->deleteLater();
    m_tabData.removeAt(index);
}

void TabSystem::closeAllTabs()
{
    while (count() > 0) {
        closeTab(0);
    }
}

void TabSystem::closeOtherTabs(int index)
{
    for (int i = count() - 1; i >= 0; --i) {
        if (i != index) {
            closeTab(i);
        }
    }
}

CodeEditor* TabSystem::createEditor()
{
    CodeEditor* editor = new CodeEditor(this);
    setupTabConnections(editor);
    return editor;
}

void TabSystem::setupTabConnections(CodeEditor* editor)
{
    connect(editor, &CodeEditor::textChanged, this, &TabSystem::onEditorContentChanged);
    connect(editor, &CodeEditor::cursorPositionChanged, this, &TabSystem::updateCursorPosition);
}

void TabSystem::onEditorContentChanged()
{
    int index = currentIndex();
    if (index >= 0) {
        setTabModified(index, true);
    }
}

void TabSystem::updateTabTitle(int index)
{
    if (index < 0 || index >= count()) return;
    
    QString title = generateTabTitle(m_tabData[index].filePath);
    if (m_tabData[index].isModified) {
        title += "*";
    }
    setTabText(index, title);
}

void TabSystem::setTabModified(int index, bool modified)
{
    if (index < 0 || index >= m_tabData.size()) return;
    m_tabData[index].isModified = modified;
    updateTabTitle(index);
}

int TabSystem::findTabByPath(const QString& path) const
{
    if (path.isEmpty()) return -1;
    for (int i = 0; i < m_tabData.size(); ++i) {
        if (m_tabData[i].filePath == path) {
            return i;
        }
    }
    return -1;
}

void TabSystem::tabInserted(int index)
{
    QTabWidget::tabInserted(index);
    emit tabCountChanged(count());
}

void TabSystem::tabRemoved(int index)
{
    QTabWidget::tabRemoved(index);
    emit tabCountChanged(count());
}

CodeEditor* TabSystem::currentEditor() const
{
    return qobject_cast<CodeEditor*>(currentWidget());
}

QString TabSystem::currentFilePath() const
{
    int index = currentIndex();
    return (index >= 0) ? m_tabData[index].filePath : QString();
}

bool TabSystem::isCurrentTabModified() const
{
    int index = currentIndex();
    return (index >= 0) ? m_tabData[index].isModified : false;
}

void TabSystem::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu;
    int index = tabBar()->tabAt(event->pos());
    
    if (index >= 0) {
        menu.addAction(tr("Close Tab"), [this, index]() { closeTab(index); });
        menu.addAction(tr("Close Other Tabs"), [this, index]() { closeOtherTabs(index); });
        menu.addAction(tr("Save Tab"), [this, index]() { saveTabContent(index); });
        menu.addSeparator();
        menu.addAction(tr("Split Horizontally"), [this]() { splitHorizontally(); });
        menu.addAction(tr("Split Vertically"), [this]() { splitVertically(); });
    } else {
        menu.addAction(tr("New Tab"), [this]() { addNewTab(); });
    }
    
    menu.exec(event->globalPos());
}

void TabSystem::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void TabSystem::dropEvent(QDropEvent* event)
{
    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        for (const QUrl& url : mimeData->urls()) {
            loadFileToTab(url.toLocalFile());
        }
        event->acceptProposedAction();
    }
}

void TabSystem::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragStartPos = event->pos();
    }
    QTabWidget::mousePressEvent(event);
}

void TabSystem::mouseMoveEvent(QMouseEvent* event)
{
    if (!(event->buttons() & Qt::LeftButton)) return;
    if ((event->pos() - m_dragStartPos).manhattanLength() < QApplication::startDragDistance()) return;
    
    QDrag* drag = new QDrag(this);
    QMimeData* mimeData = new QMimeData;
    mimeData->setText(currentEditor()->toPlainText());
    drag->setMimeData(mimeData);
    drag->exec(Qt::CopyAction);
}

void TabSystem::splitHorizontally()
{
    emit splitViewRequested(Qt::Horizontal);
}

void TabSystem::splitVertically()
{
    emit splitViewRequested(Qt::Vertical);
}

void TabSystem::saveSession()
{
    QStringList openFiles;
    for (const auto& data : m_tabData) {
        if (!data.filePath.isEmpty()) {
            openFiles << data.filePath;
        }
    }
    m_core->saveSession(openFiles);
}

void TabSystem::restoreSession()
{
    QStringList files = m_core->restoreSession();
    for (const QString& file : files) {
        loadFileToTab(file);
    }
}

void TabSystem::showTabPreview(int index)
{
    if (index >= 0 && index < count()) {
        CodeEditor* editor = qobject_cast<CodeEditor*>(widget(index));
        if (editor) {
            QToolTip::showText(QCursor::pos(), editor->toPlainText().left(500), this);
        }
    }
}
