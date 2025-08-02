#include "status_bar.h"
#include <QLabel>
#include <QProgressBar>
#include <QHBoxLayout>
#include <QFrame>
#include <QTimer>
#include <QApplication>
#include <QStyle>
#include <QMouseEvent>

StatusBar::StatusBar(QWidget *parent) 
    : QStatusBar(parent),
      m_lineColLabel(new QLabel(this)),
      m_encodingLabel(new QLabel(this)),
      m_fileTypeLabel(new QLabel(this)),
      m_cursorPosLabel(new QLabel(this)),
      m_zoomLabel(new QLabel(this)),
      m_memoryLabel(new QLabel(this)),
      m_progressBar(new QProgressBar(this)),
      m_messageTimer(new QTimer(this)),
      m_memoryTimer(new QTimer(this))
{
    // Basic setup
    setSizeGripEnabled(false);
    setContentsMargins(2, 0, 2, 0);
    
    // Initialize UI components
    setupWidgets();
    setupConnections();
    
    // Set default values
    setLineCol(1, 1);
    setCursorPosition(0);
    setZoomFactor(100);
    setEncoding("UTF-8");
    setFileType("Plain Text");
    
    // Start memory monitoring
    m_memoryTimer->start(5000); // Update every 5 seconds
    updateMemoryUsage();
}

void StatusBar::setupWidgets()
{
    // Apply default style
    applyTheme(false);
    
    // Configure labels
    m_lineColLabel->setMinimumWidth(100);
    m_lineColLabel->setAlignment(Qt::AlignCenter);
    m_lineColLabel->setToolTip("Current line and column");
    
    m_encodingLabel->setMinimumWidth(80);
    m_encodingLabel->setAlignment(Qt::AlignCenter);
    m_encodingLabel->setToolTip("File encoding");
    m_encodingLabel->setCursor(Qt::PointingHandCursor);
    
    m_fileTypeLabel->setMinimumWidth(100);
    m_fileTypeLabel->setAlignment(Qt::AlignCenter);
    m_fileTypeLabel->setToolTip("File type");
    
    m_cursorPosLabel->setMinimumWidth(80);
    m_cursorPosLabel->setAlignment(Qt::AlignCenter);
    m_cursorPosLabel->setToolTip("Cursor position");
    
    m_zoomLabel->setMinimumWidth(60);
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    m_zoomLabel->setToolTip("Zoom level");
    
    m_memoryLabel->setMinimumWidth(100);
    m_memoryLabel->setAlignment(Qt::AlignCenter);
    m_memoryLabel->setToolTip("Memory usage");

    // Configure progress bar
    m_progressBar->setRange(0, 100);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(14);
    m_progressBar->setFixedWidth(200);
    m_progressBar->hide();

    // Add widgets with separators
    addPermanentWidget(createSeparator());
    addPermanentWidget(m_lineColLabel);
    addPermanentWidget(createSeparator());
    addPermanentWidget(m_encodingLabel);
    addPermanentWidget(createSeparator());
    addPermanentWidget(m_fileTypeLabel);
    addPermanentWidget(createSeparator());
    addPermanentWidget(m_cursorPosLabel);
    addPermanentWidget(createSeparator());
    addPermanentWidget(m_zoomLabel);
    addPermanentWidget(createSeparator());
    addPermanentWidget(m_memoryLabel);
    
    // Progress bar (stretches)
    addPermanentWidget(m_progressBar, 1);
}

QFrame* StatusBar::createSeparator()
{
    QFrame *separator = new QFrame(this);
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);
    separator->setStyleSheet("color: palette(mid);");
    separator->setFixedWidth(1);
    return separator;
}

void StatusBar::setupConnections()
{
    // Message timer (auto-clear)
    m_messageTimer->setSingleShot(true);
    connect(m_messageTimer, &QTimer::timeout, this, &QStatusBar::clearMessage);
    
    // Memory usage timer
    connect(m_memoryTimer, &QTimer::timeout, this, &StatusBar::updateMemoryUsage);
    
    // Clickable encoding label
    connect(m_encodingLabel, &QLabel::linkActivated, this, [this]() {
        emit encodingClicked();
    });
}

void StatusBar::applyTheme(bool darkMode)
{
    if (darkMode) {
        setStyleSheet(R"(
            QStatusBar {
                background-color: #2d2d2d;
                color: #dddddd;
                border-top: 1px solid #1a1a1a;
            }
            QLabel {
                padding: 0 8px;
            }
            QProgressBar {
                border: 1px solid #444;
                border-radius: 3px;
                background: #333;
            }
            QProgressBar::chunk {
                background-color: #5050ff;
            }
        )");
    } else {
        setStyleSheet(R"(
            QStatusBar {
                background-color: #f0f0f0;
                color: #333333;
                border-top: 1px solid #cccccc;
            }
            QLabel {
                padding: 0 8px;
            }
            QProgressBar {
                border: 1px solid #ccc;
                border-radius: 3px;
                background: #fff;
            }
            QProgressBar::chunk {
                background-color: #5050ff;
            }
        )");
    }
}

// Public methods
void StatusBar::setLineCol(int line, int col)
{
    m_lineColLabel->setText(QString("Line: %1, Col: %2").arg(line).arg(col));
}

void StatusBar::setEncoding(const QString &encoding)
{
    m_encodingLabel->setText(QString("<a href=\"#\">%1</a>").arg(encoding));
}

void StatusBar::setFileType(const QString &fileType)
{
    m_fileTypeLabel->setText(fileType.isEmpty() ? "Plain Text" : fileType);
}

void StatusBar::setCursorPosition(int pos)
{
    m_cursorPosLabel->setText(QString("Pos: %1").arg(pos));
}

void StatusBar::setZoomFactor(int percent)
{
    m_zoomLabel->setText(QString("%1%").arg(percent));
}

void StatusBar::showProgressBar(int maximum)
{
    m_progressBar->setRange(0, maximum);
    m_progressBar->setValue(0);
    m_progressBar->show();
}

void StatusBar::updateProgress(int value)
{
    m_progressBar->setValue(value);
    if (value >= m_progressBar->maximum()) {
        QTimer::singleShot(1000, this, [this]() {
            m_progressBar->hide();
        });
    }
}

void StatusBar::hideProgressBar()
{
    m_progressBar->hide();
}

void StatusBar::showMessage(const QString &message, int timeout)
{
    QStatusBar::showMessage(message, timeout);
    if (timeout > 0) {
        m_messageTimer->start(timeout);
    }
}

void StatusBar::updateMemoryUsage()
{
#ifdef Q_OS_WIN
    // Windows-specific memory calculation
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        double usedMB = pmc.WorkingSetSize / (1024.0 * 1024.0);
        m_memoryLabel->setText(QString("Mem: %1 MB").arg(usedMB, 0, 'f', 1));
    }
#elif defined(Q_OS_LINUX)
    // Linux-specific memory calculation
    QFile file("/proc/self/status");
    if (file.open(QIODevice::ReadOnly)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (line.startsWith("VmRSS:")) {
                QStringList parts = line.split(" ", Qt::SkipEmptyParts);
                if (parts.size() >= 2) {
                    double usedMB = parts[1].toDouble() / 1024.0;
                    m_memoryLabel->setText(QString("Mem: %1 MB").arg(usedMB, 0, 'f', 1));
                }
            }
        }
    }
#elif defined(Q_OS_MAC)
    // macOS-specific memory calculation
    struct task_basic_info t_info;
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&t_info, &t_info_count) == KERN_SUCCESS) {
        double usedMB = t_info.resident_size / (1024.0 * 1024.0);
        m_memoryLabel->setText(QString("Mem: %1 MB").arg(usedMB, 0, 'f', 1));
    }
#endif
}

void StatusBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        emit rightClicked();
    }
    QStatusBar::mousePressEvent(event);
}
