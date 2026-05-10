#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>
#include <vector>

namespace astra::engine
{
struct HtmlAttribute
{
    QString name;
    QString value;
};

struct DomNode
{
    enum class Type
    {
        Document,
        Element,
        Text,
        Comment,
    };

    Type type = Type::Document;
    QString name;
    QString text;
    QVector<HtmlAttribute> attributes;
    std::vector<std::unique_ptr<DomNode>> children;

    DomNode *appendChild(std::unique_ptr<DomNode> child);
};

class HtmlParser final
{
public:
    std::unique_ptr<DomNode> parse(const QString &html);
    [[nodiscard]] const QStringList &errors() const;

private:
    [[nodiscard]] bool atEnd() const;
    [[nodiscard]] bool startsWith(const QString &value) const;
    [[nodiscard]] QChar peek() const;

    void skipWhitespace();
    void parseText(DomNode *parent);
    void parseComment(DomNode *parent);
    void parseDeclaration();
    void parseClosingTag();
    void parseStartTag(DomNode *parent);

    [[nodiscard]] QString readName();
    [[nodiscard]] QString readAttributeValue();
    [[nodiscard]] bool isVoidElement(const QString &tagName) const;

    QString m_html;
    qsizetype m_position = 0;
    QStringList m_errors;
    std::vector<DomNode *> m_stack;
};
}
