#ifndef HIGHLIGHTER_H
#define HIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QVector>
#include <QJsonObject>
#include <QFuture>
#include <QElapsedTimer>
#include <QMap>

class SyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    struct HighlightRule {
        QRegularExpression pattern;
        QTextCharFormat format;
        int captureGroup = 0;
    };

    struct ThemeColors {
        QColor keyword;
        QColor string;
        QColor comment;
        QColor number;
        QColor function;
        QColor type;
        QColor background;
    };

    struct HighlightCache {
        int position;
        int length;
        QTextCharFormat format;
    };

    explicit SyntaxHighlighter(QTextDocument *parent = nullptr);

    // Language and theme management
    void loadLanguage(const QString &language);
    QString currentLanguage() const;
    void setTheme(const QString &themeName);
    void reloadCurrentLanguage();

    // Performance monitoring
    qint64 lastHighlightTime() const;

    // Custom rule management
    void addCustomRule(const HighlightRule &rule);
    void removeCustomRule(const QRegularExpression &pattern);
    void clearCustomRules();

    // Syntax checking
    enum SyntaxError {
        NoError,
        UnclosedString,
        UnclosedComment,
        InvalidSyntax
    };
    SyntaxError checkSyntaxErrors(const QString &text) const;

signals:
    void highlightingPerformance(qint64 milliseconds);
    void languageLoaded(const QString &language);
    void themeChanged(const QString &theme);
    void syntaxErrorDetected(SyntaxError error, int position);

public slots:
    void precompilePatterns();
    void highlightInParallel(const QString &text);

protected:
    void highlightBlock(const QString &text) override;

private:
    // Language loading methods
    void loadDefaultRules();
    void loadKeywords(const QJsonObject &json);
    void loadStrings(const QJsonObject &json);
    void loadComments(const QJsonObject &json);
    void loadHighlightingRules(const QJsonObject &json);
    void loadSpecialRules(const QJsonObject &json);
    void loadTheme(const QJsonObject &json);

    // Helper methods
    QTextCharFormat createFormatFromStyle(const QJsonObject &style);
    void handleMultiLine(const QString &text);
    void updateThemeColors();
    void cacheHighlighting(const QString &text);

    // Member variables
    QVector<HighlightRule> m_rules;
    QVector<HighlightRule> m_customRules;
    QVector<HighlightCache> m_cache;
    QString m_currentLanguage;
    QString m_currentTheme;
    ThemeColors m_themeColors;
    qint64 m_lastHighlightTime;

    // Multi-line comment handling
    QRegularExpression m_blockCommentStart;
    QRegularExpression m_blockCommentEnd;
    QTextCharFormat m_blockCommentFormat;

    // Performance tracking
    QElapsedTimer m_highlightTimer;
};

#endif // HIGHLIGHTER_H
