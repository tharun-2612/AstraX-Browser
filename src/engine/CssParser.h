#pragma once

#include "HtmlParser.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace astra::engine
{
struct CssDeclaration
{
    QString property;
    QString value;
};

struct CssSelector
{
    QString text;
    QString tagName;
    QString id;
    QStringList classes;
    int specificity = 0;
    bool valid = false;
};

struct CssRule
{
    CssSelector selector;
    QVector<CssDeclaration> declarations;
    int order = 0;
};

struct StyleSheet
{
    QVector<CssRule> rules;
};

class CssParser final
{
public:
    StyleSheet parse(const QString &css);
    [[nodiscard]] const QStringList &errors() const;

private:
    [[nodiscard]] QString stripComments(const QString &css) const;
    [[nodiscard]] CssSelector parseSelector(const QString &selectorText);
    [[nodiscard]] QVector<CssDeclaration> parseDeclarations(const QString &block);
    [[nodiscard]] bool isIdentifierCharacter(QChar character) const;

    QStringList m_errors;
};

[[nodiscard]] bool matchesSelector(const CssSelector &selector, const DomNode &node);
[[nodiscard]] QString attributeValue(const DomNode &node, const QString &name);
[[nodiscard]] QVector<CssRule> matchingRules(const StyleSheet &styleSheet, const DomNode &node);
}
