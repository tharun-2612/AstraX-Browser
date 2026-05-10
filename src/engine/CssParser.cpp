#include "CssParser.h"

#include <algorithm>

namespace astra::engine
{
StyleSheet CssParser::parse(const QString &css)
{
    m_errors.clear();
    StyleSheet styleSheet;
    const QString cleanCss = stripComments(css);
    qsizetype position = 0;
    int order = 0;

    while (position < cleanCss.size()) {
        const qsizetype openBrace = cleanCss.indexOf(QLatin1Char('{'), position);
        if (openBrace < 0) {
            if (!cleanCss.mid(position).trimmed().isEmpty()) {
                m_errors.append(QStringLiteral("Ignored trailing CSS without a declaration block."));
            }
            break;
        }

        const qsizetype closeBrace = cleanCss.indexOf(QLatin1Char('}'), openBrace + 1);
        if (closeBrace < 0) {
            m_errors.append(QStringLiteral("Missing closing brace for selector \"%1\".")
                .arg(cleanCss.mid(position, openBrace - position).simplified()));
            break;
        }

        const QString selectorList = cleanCss.mid(position, openBrace - position).trimmed();
        const QString declarationBlock = cleanCss.mid(openBrace + 1, closeBrace - openBrace - 1);
        const QVector<CssDeclaration> declarations = parseDeclarations(declarationBlock);

        const QStringList selectors = selectorList.split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (const QString &selectorText : selectors) {
            CssSelector selector = parseSelector(selectorText.trimmed());
            if (!selector.valid || declarations.isEmpty()) {
                continue;
            }

            styleSheet.rules.append(CssRule{ selector, declarations, order++ });
        }

        position = closeBrace + 1;
    }

    return styleSheet;
}

const QStringList &CssParser::errors() const
{
    return m_errors;
}

QString CssParser::stripComments(const QString &css) const
{
    QString output;
    output.reserve(css.size());
    qsizetype position = 0;

    while (position < css.size()) {
        if (css.mid(position, 2) == QStringLiteral("/*")) {
            const qsizetype end = css.indexOf(QStringLiteral("*/"), position + 2);
            if (end < 0) {
                break;
            }
            position = end + 2;
            continue;
        }

        output.append(css.at(position));
        ++position;
    }

    return output;
}

CssSelector CssParser::parseSelector(const QString &selectorText)
{
    CssSelector selector;
    selector.text = selectorText.simplified();
    if (selector.text.isEmpty()) {
        return selector;
    }

    for (const QChar character : selector.text) {
        if (character.isSpace()
            || character == QLatin1Char('>')
            || character == QLatin1Char('+')
            || character == QLatin1Char('~')) {
            m_errors.append(QStringLiteral("Unsupported complex selector \"%1\". The CSS parser currently supports tag, class, id, and compound selectors.")
                .arg(selector.text));
            return selector;
        }
    }

    qsizetype position = 0;
    while (position < selector.text.size()) {
        const QChar marker = selector.text.at(position);
        if (marker == QLatin1Char('#') || marker == QLatin1Char('.')) {
            ++position;
            const qsizetype start = position;
            while (position < selector.text.size() && isIdentifierCharacter(selector.text.at(position))) {
                ++position;
            }

            const QString value = selector.text.mid(start, position - start);
            if (value.isEmpty()) {
                m_errors.append(QStringLiteral("Invalid selector \"%1\".").arg(selector.text));
                return selector;
            }

            if (marker == QLatin1Char('#')) {
                selector.id = value;
            } else {
                selector.classes.append(value);
            }
            continue;
        }

        if (marker == QLatin1Char('*')) {
            selector.tagName = QStringLiteral("*");
            ++position;
            continue;
        }

        if (isIdentifierCharacter(marker)) {
            const qsizetype start = position;
            while (position < selector.text.size() && isIdentifierCharacter(selector.text.at(position))) {
                ++position;
            }
            selector.tagName = selector.text.mid(start, position - start).toLower();
            continue;
        }

        m_errors.append(QStringLiteral("Invalid selector \"%1\".").arg(selector.text));
        return selector;
    }

    selector.specificity = (selector.id.isEmpty() ? 0 : 100)
        + (selector.classes.size() * 10)
        + ((!selector.tagName.isEmpty() && selector.tagName != QStringLiteral("*")) ? 1 : 0);
    selector.valid = true;
    return selector;
}

QVector<CssDeclaration> CssParser::parseDeclarations(const QString &block)
{
    QVector<CssDeclaration> declarations;
    const QStringList parts = block.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        const qsizetype colon = part.indexOf(QLatin1Char(':'));
        if (colon < 0) {
            const QString invalid = part.simplified();
            if (!invalid.isEmpty()) {
                m_errors.append(QStringLiteral("Ignored invalid declaration \"%1\".").arg(invalid));
            }
            continue;
        }

        CssDeclaration declaration;
        declaration.property = part.left(colon).trimmed().toLower();
        declaration.value = part.mid(colon + 1).trimmed();
        if (!declaration.property.isEmpty() && !declaration.value.isEmpty()) {
            declarations.append(declaration);
        }
    }

    return declarations;
}

bool CssParser::isIdentifierCharacter(QChar character) const
{
    return character.isLetterOrNumber()
        || character == QLatin1Char('-')
        || character == QLatin1Char('_');
}

QString attributeValue(const DomNode &node, const QString &name)
{
    for (const HtmlAttribute &attribute : node.attributes) {
        if (attribute.name.compare(name, Qt::CaseInsensitive) == 0) {
            return attribute.value;
        }
    }

    return {};
}

bool matchesSelector(const CssSelector &selector, const DomNode &node)
{
    if (!selector.valid || node.type != DomNode::Type::Element) {
        return false;
    }

    if (!selector.tagName.isEmpty()
        && selector.tagName != QStringLiteral("*")
        && selector.tagName != node.name) {
        return false;
    }

    if (!selector.id.isEmpty() && attributeValue(node, QStringLiteral("id")) != selector.id) {
        return false;
    }

    const QStringList nodeClasses = attributeValue(node, QStringLiteral("class")).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString &requiredClass : selector.classes) {
        if (!nodeClasses.contains(requiredClass)) {
            return false;
        }
    }

    return true;
}

QVector<CssRule> matchingRules(const StyleSheet &styleSheet, const DomNode &node)
{
    QVector<CssRule> rules;
    for (const CssRule &rule : styleSheet.rules) {
        if (matchesSelector(rule.selector, node)) {
            rules.append(rule);
        }
    }

    std::sort(rules.begin(), rules.end(), [](const CssRule &left, const CssRule &right) {
        if (left.selector.specificity == right.selector.specificity) {
            return left.order < right.order;
        }
        return left.selector.specificity < right.selector.specificity;
    });
    return rules;
}
}
