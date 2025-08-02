#include "highlighter.h"
#include "utilities/logger.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFuture>
#include <QtConcurrent>
#include <QToolTip>

SyntaxHighlighter::SyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent), m_lastHighlightTime(0)
{
    // Initialize default theme colors
    m_themeColors = {
        QColor("#569CD6"), // keyword
        QColor("#CE9178"), // string
        QColor("#6A9955"), // comment
        QColor("#B5CEA8"), // number
        QColor("#DCDCAA"), // function
        QColor("#4EC9B0"), // type
        QColor("#FFFFFF")  // background
    };

    loadDefaultRules();
    m_highlightTimer.start();
}

void SyntaxHighlighter::loadLanguage(const QString &language) {
    QString path = QString(":/syntax/language_defs/%1.json").arg(language);
    QFile file(path);
    
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open language file:" << path;
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject json = doc.object();
    file.close();

    m_rules.clear();
    m_currentLanguage = language;

    // Load all syntax components
    loadKeywords(json);
    loadStrings(json);
    loadComments(json);
    loadHighlightingRules(json);
    loadSpecialRules(json);
    
    // Load theme if specified in language file
    if (json.contains("theme")) {
        loadTheme(json["theme"].toObject());
    }

    precompilePatterns();
    emit languageLoaded(language);
}

void SyntaxHighlighter::highlightBlock(const QString &text) {
    m_highlightTimer.restart();

    // Apply all highlighting rules
    for (const HighlightRule &rule : qAsConst(m_rules)) {
        QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            setFormat(match.capturedStart(rule.captureGroup),
                     match.capturedLength(rule.captureGroup),
                     rule.format);
        }
    }

    // Apply custom rules
    for (const HighlightRule &rule : qAsConst(m_customRules)) {
        QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            setFormat(match.capturedStart(rule.captureGroup),
                     match.capturedLength(rule.captureGroup),
                     rule.format);
        }
    }

    // Handle multi-line constructs
    handleMultiLine(text);

    // Check for syntax errors
    SyntaxError error = checkSyntaxErrors(text);
    if (error != NoError) {
        emit syntaxErrorDetected(error, 0);
    }

    m_lastHighlightTime = m_highlightTimer.elapsed();
    emit highlightingPerformance(m_lastHighlightTime);
}

void SyntaxHighlighter::loadKeywords(const QJsonObject &json) {
    if (!json.contains("keywords")) return;
    
    QJsonObject keywords = json["keywords"].toObject();
    QTextCharFormat format;
    format.setForeground(m_themeColors.keyword);

    QStringList keywordTypes = {"primary", "secondary", "operators"};
    for (const QString &type : keywordTypes) {
        if (keywords.contains(type)) {
            QStringList words = keywords[type].toVariant().toStringList();
            for (const QString &word : words) {
                HighlightRule rule;
                rule.pattern = QRegularExpression(QString("\\b%1\\b").arg(word));
                rule.format = format;
                m_rules.append(rule);
            }
        }
    }
}

void SyntaxHighlighter::loadStrings(const QJsonObject &json) {
    if (!json.contains("strings")) return;
    
    QJsonObject strings = json["strings"].toObject();
    QTextCharFormat format;
    format.setForeground(m_themeColors.string);

    QStringList delimiters = strings["delimiters"].toVariant().toStringList();
    for (const QString &delim : delimiters) {
        HighlightRule rule;
        QString pattern = QString("%1[^%1]*%1").arg(QRegularExpression::escape(delim));
        rule.pattern = QRegularExpression(pattern);
        rule.format = format;
        m_rules.append(rule);
    }

    if (strings["f_strings"].toBool()) {
        HighlightRule fStringRule;
        fStringRule.pattern = QRegularExpression(R"(f[\"'][^\"']*\{[^}]*\}[^\"']*[\"'])");
        fStringRule.format = format;
        m_rules.append(fStringRule);
    }
}

void SyntaxHighlighter::loadComments(const QJsonObject &json) {
    if (!json.contains("comments")) return;
    
    QJsonObject comments = json["comments"].toObject();
    QTextCharFormat format;
    format.setForeground(m_themeColors.comment);
    format.setFontItalic(true);

    if (comments.contains("line")) {
        HighlightRule rule;
        rule.pattern = QRegularExpression(QString("%1.*").arg(comments["line"].toString()));
        rule.format = format;
        m_rules.append(rule);
    }

    if (comments.contains("block")) {
        QJsonObject block = comments["block"].toObject();
        m_blockCommentStart = QRegularExpression(block["start"].toString());
        m_blockCommentEnd = QRegularExpression(block["end"].toString());
        m_blockCommentFormat = format;
    }
}

void SyntaxHighlighter::loadHighlightingRules(const QJsonObject &json) {
    if (!json.contains("highlighting_rules")) return;
    
    for (const QJsonValue &ruleValue : json["highlighting_rules"].toArray()) {
        QJsonObject ruleObj = ruleValue.toObject();
        HighlightRule rule;
        
        rule.pattern = QRegularExpression(ruleObj["pattern"].toString());
        rule.captureGroup = ruleObj.value("capture_group").toInt(0);
        
        QJsonObject style = ruleObj["style"].toObject();
        rule.format = createFormatFromStyle(style);
        
        m_rules.append(rule);
    }
}

void SyntaxHighlighter::loadSpecialRules(const QJsonObject &json) {
    if (!json.contains("special_rules")) return;
    
    QJsonObject specials = json["special_rules"].toObject();
    for (const QString &key : specials.keys()) {
        QJsonObject ruleObj = specials[key].toObject();
        HighlightRule rule;
        
        rule.pattern = QRegularExpression(ruleObj["pattern"].toString());
        rule.captureGroup = ruleObj.value("capture_group").toInt(0);
        
        QJsonObject style = ruleObj["style"].toObject();
        rule.format = createFormatFromStyle(style);
        
        m_rules.append(rule);
    }
}

QTextCharFormat SyntaxHighlighter::createFormatFromStyle(const QJsonObject &style) {
    QTextCharFormat format;
    
    // Priority 1: Explicit color from JSON
    if (style.contains("color")) {
        format.setForeground(QColor(style["color"].toString()));
    }
    // Priority 2: Theme color based on type
    else if (style.contains("type")) {
        QString type = style["type"].toString();
        if (type == "keyword") format.setForeground(m_themeColors.keyword);
        else if (type == "string") format.setForeground(m_themeColors.string);
        else if (type == "comment") format.setForeground(m_themeColors.comment);
        else if (type == "number") format.setForeground(m_themeColors.number);
        else if (type == "function") format.setForeground(m_themeColors.function);
        else if (type == "type") format.setForeground(m_themeColors.type);
    }
    
    if (style.contains("background")) {
        format.setBackground(QColor(style["background"].toString()));
    }
    
    if (style.contains("fontStyle")) {
        QString fontStyle = style["fontStyle"].toString().toLower();
        if (fontStyle == "bold") format.setFontWeight(QFont::Bold);
        else if (fontStyle == "italic") format.setFontItalic(true);
        else if (fontStyle == "underline") format.setUnderlineStyle(QTextCharFormat::SingleUnderline);
    }
    
    return format;
}

void SyntaxHighlighter::handleMultiLine(const QString &text) {
    if (m_blockCommentStart.pattern().isEmpty()) return;

    setCurrentBlockState(previousBlockState());

    int startIndex = 0;
    if (previousBlockState() != 1) {
        startIndex = text.indexOf(m_blockCommentStart);
    }

    while (startIndex >= 0) {
        QRegularExpressionMatch match = m_blockCommentEnd.match(text, startIndex);
        int endIndex = match.capturedStart();
        int commentLength = 0;

        if (endIndex == -1) {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        } else {
            commentLength = endIndex - startIndex + match.capturedLength();
        }

        setFormat(startIndex, commentLength, m_blockCommentFormat);
        startIndex = text.indexOf(m_blockCommentStart, startIndex + commentLength);
    }
}

void SyntaxHighlighter::loadDefaultRules() {
    HighlightRule rule;
    QTextCharFormat defaultFormat;
    defaultFormat.setForeground(Qt::black);
    defaultFormat.setBackground(m_themeColors.background);
    
    rule.pattern = QRegularExpression(".");
    rule.format = defaultFormat;
    m_rules.append(rule);
}

void SyntaxHighlighter::setTheme(const QString &themeName) {
    QString path = QString(":/themes/%1.json").arg(themeName);
    QFile file(path);
    
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open theme file:" << path;
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    loadTheme(doc.object());
    file.close();

    m_currentTheme = themeName;
    rehighlight();
    emit themeChanged(themeName);
}

void SyntaxHighlighter::loadTheme(const QJsonObject &json) {
    if (json.contains("colors")) {
        QJsonObject colors = json["colors"].toObject();
        if (colors.contains("keyword")) m_themeColors.keyword = QColor(colors["keyword"].toString());
        if (colors.contains("string")) m_themeColors.string = QColor(colors["string"].toString());
        if (colors.contains("comment")) m_themeColors.comment = QColor(colors["comment"].toString());
        if (colors.contains("number")) m_themeColors.number = QColor(colors["number"].toString());
        if (colors.contains("function")) m_themeColors.function = QColor(colors["function"].toString());
        if (colors.contains("type")) m_themeColors.type = QColor(colors["type"].toString());
        if (colors.contains("background")) m_themeColors.background = QColor(colors["background"].toString());
    }
    updateThemeColors();
}

void SyntaxHighlighter::updateThemeColors() {
    for (auto &rule : m_rules) {
        if (rule.format.foreground().color() == QColor("#569CD6")) {
            rule.format.setForeground(m_themeColors.keyword);
        }
        // Update other color mappings similarly...
    }
    
    // Update block comment format
    m_blockCommentFormat.setForeground(m_themeColors.comment);
}

void SyntaxHighlighter::reloadCurrentLanguage() {
    if (!m_currentLanguage.isEmpty()) {
        loadLanguage(m_currentLanguage);
        if (!m_currentTheme.isEmpty()) {
            setTheme(m_currentTheme);
        }
    }
}

SyntaxHighlighter::SyntaxError SyntaxHighlighter::checkSyntaxErrors(const QString &text) const {
    if (!m_blockCommentStart.pattern().isEmpty() && 
        text.contains(m_blockCommentStart) && 
        !text.contains(m_blockCommentEnd)) {
        return UnclosedComment;
    }

    // Add more syntax checks here
    return NoError;
}

void SyntaxHighlighter::addCustomRule(const HighlightRule &rule) {
    m_customRules.append(rule);
    rehighlight();
}

void SyntaxHighlighter::removeCustomRule(const QRegularExpression &pattern) {
    auto it = std::remove_if(m_customRules.begin(), m_customRules.end(),
        [&pattern](const HighlightRule &rule) {
            return rule.pattern.pattern() == pattern.pattern();
        });
    m_customRules.erase(it, m_customRules.end());
    rehighlight();
}

void SyntaxHighlighter::clearCustomRules() {
    m_customRules.clear();
    rehighlight();
}

void SyntaxHighlighter::precompilePatterns() {
    for (auto &rule : m_rules) {
        rule.pattern.optimize();
    }
    for (auto &rule : m_customRules) {
        rule.pattern.optimize();
    }
    m_blockCommentStart.optimize();
    m_blockCommentEnd.optimize();
}

void SyntaxHighlighter::highlightInParallel(const QString &text) {
    QFuture<void> future = QtConcurrent::map(m_rules, [this, &text](HighlightRule &rule) {
        QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            setFormat(match.capturedStart(rule.captureGroup),
                     match.capturedLength(rule.captureGroup),
                     rule.format);
        }
    });
    future.waitForFinished();
}

qint64 SyntaxHighlighter::lastHighlightTime() const {
    return m_lastHighlightTime;
}
