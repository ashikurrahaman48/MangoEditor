#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QStatusBar>
#include <QLabel>
#include <QTimer>
#include <QProgressBar>
#include <QFrame>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() = default;

    // Status Bar Component Methods
    void showStatusMessage(const QString& message, int timeout = 0);
    void setStatusLineCol(int line, int col);
    void setStatusEncoding(const QString& encoding);
    void setStatusFileType(const QString& fileType);
    void setStatusCursorPosition(int pos);
    void setStatusZoomFactor(int percent);

    // Progress Bar Control
    void showStatusProgressBar(int maximum);
    void updateStatusProgress(int value);
    void hideStatusProgressBar();

    // Theme Support
    void applyTheme(const QString& theme);

    // Memory Indicator
    void updateStatusMemoryUsage();

signals:
    void statusEncodingClicked();
    void statusLineColClicked();

private slots:
    void clearStatusTemporaryMessage();

private:
    // Main Window Components
    QMenuBar* m_menuBar;
    QToolBar* m_toolBar;
    
    // Status Bar Components
    QLabel* m_statusLineColLabel;
    QLabel* m_statusEncodingLabel;
    QLabel* m_statusFileTypeLabel;
    QLabel* m_statusCursorPosLabel;
    QLabel* m_statusZoomLabel;
    QLabel* m_statusMemoryLabel;
    QProgressBar* m_statusProgressBar;
    QTimer* m_statusMessageTimer;

    // UI Setup Methods
    void setupStatusBar();
    QFrame* createStatusSeparator();
    void makeLabelsInteractive();
};

#endif // MAIN_WINDOW_H
