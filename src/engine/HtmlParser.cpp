#include "HtmlParser.h"

#include <QSet>

#include <algorithm>

namespace astra::engine
{
DomNode *DomNode::appendChild(std::unique_ptr<DomNode> child)
{
    DomNode *rawNode = child.get();
    children.push_back(std::move(child));
    return rawNode;
}

std::unique_ptr<DomNode> HtmlParser::parse(const QString &html)
{
    m_html = html;
    m_position = 0;
    m_errors.clear();
    m_stack.clear();

    auto document = std::make_unique<DomNode>();
    document->type = DomNode::Type::Document;
    document->name = QStringLiteral("#document");
    m_stack.push_back(document.get());

    while (!atEnd()) {
        DomNode *parent = m_stack.back();
        if (startsWith(QStringLiteral("<!--"))) {
            parseComment(parent);
        } else if (startsWith(QStringLiteral("</"))) {
            parseClosingTag();
        } else if (startsWith(QStringLiteral("<!"))) {
            parseDeclaration();
        } else if (startsWith(QStringLiteral("<"))) {
            parseStartTag(parent);
        } else {
            parseText(parent);
        }
    }

    while (m_stack.size() > 1) {
        m_errors.append(QStringLiteral("Unclosed tag <%1>").arg(m_stack.back()->name));
        m_stack.pop_back();
    }

    return document;
}

const QStringList &HtmlParser::errors() const
{
    return m_errors;
}

bool HtmlParser::atEnd() const
{
    return m_position >= m_html.size();
}

bool HtmlParser::startsWith(const QString &value) const
{
    return m_html.mid(m_position, value.size()) == value;
}

QChar HtmlParser::peek() const
{
    return atEnd() ? QChar() : m_html.at(m_position);
}

void HtmlParser::skipWhitespace()
{
    while (!atEnd() && peek().isSpace()) {
        ++m_position;
    }
}

void HtmlParser::parseText(DomNode *parent)
{
    const qsizetype start = m_position;
    while (!atEnd() && !startsWith(QStringLiteral("<"))) {
        ++m_position;
    }

    const QString text = m_html.mid(start, m_position - start).simplified();
    if (text.isEmpty()) {
        return;
    }

    auto node = std::make_unique<DomNode>();
    node->type = DomNode::Type::Text;
    node->name = QStringLiteral("#text");
    node->text = text;
    parent->appendChild(std::move(node));
}

void HtmlParser::parseComment(DomNode *parent)
{
    m_position += 4;
    const qsizetype start = m_position;
    const qsizetype end = m_html.indexOf(QStringLiteral("-->"), m_position);
    if (end < 0) {
        m_errors.append(QStringLiteral("Unclosed HTML comment"));
        m_position = m_html.size();
        return;
    }

    auto node = std::make_unique<DomNode>();
    node->type = DomNode::Type::Comment;
    node->name = QStringLiteral("#comment");
    node->text = m_html.mid(start, end - start).trimmed();
    parent->appendChild(std::move(node));
    m_position = end + 3;
}

void HtmlParser::parseDeclaration()
{
    const qsizetype end = m_html.indexOf(QLatin1Char('>'), m_position);
    if (end < 0) {
        m_errors.append(QStringLiteral("Unclosed declaration"));
        m_position = m_html.size();
        return;
    }

    m_position = end + 1;
}

void HtmlParser::parseClosingTag()
{
    m_position += 2;
    skipWhitespace();
    const QString tagName = readName().toLower();
    const qsizetype end = m_html.indexOf(QLatin1Char('>'), m_position);
    m_position = end < 0 ? m_html.size() : end + 1;

    if (tagName.isEmpty()) {
        m_errors.append(QStringLiteral("Found a closing tag without a name"));
        return;
    }

    for (qsizetype index = static_cast<qsizetype>(m_stack.size()) - 1; index > 0; --index) {
        if (m_stack.at(static_cast<size_t>(index))->name == tagName) {
            m_stack.resize(static_cast<size_t>(index));
            return;
        }
    }

    m_errors.append(QStringLiteral("Closing tag </%1> has no matching start tag").arg(tagName));
}

void HtmlParser::parseStartTag(DomNode *parent)
{
    ++m_position;
    skipWhitespace();
    QString tagName = readName().toLower();
    if (tagName.isEmpty()) {
        m_errors.append(QStringLiteral("Found a start tag without a name"));
        const qsizetype end = m_html.indexOf(QLatin1Char('>'), m_position);
        m_position = end < 0 ? m_html.size() : end + 1;
        return;
    }

    auto node = std::make_unique<DomNode>();
    node->type = DomNode::Type::Element;
    node->name = tagName;
    bool selfClosing = false;

    while (!atEnd()) {
        skipWhitespace();
        if (startsWith(QStringLiteral("/>"))) {
            selfClosing = true;
            m_position += 2;
            break;
        }
        if (startsWith(QStringLiteral(">"))) {
            ++m_position;
            break;
        }

        HtmlAttribute attribute;
        attribute.name = readName().toLower();
        if (attribute.name.isEmpty()) {
            ++m_position;
            continue;
        }

        skipWhitespace();
        if (startsWith(QStringLiteral("="))) {
            ++m_position;
            skipWhitespace();
            attribute.value = readAttributeValue();
        }
        node->attributes.append(attribute);
    }

    DomNode *created = parent->appendChild(std::move(node));
    if (!selfClosing && !isVoidElement(tagName)) {
        m_stack.push_back(created);
    }
}

QString HtmlParser::readName()
{
    const qsizetype start = m_position;
    while (!atEnd()) {
        const QChar character = peek();
        if (character.isLetterOrNumber()
            || character == QLatin1Char('-')
            || character == QLatin1Char('_')
            || character == QLatin1Char(':')
            || character == QLatin1Char('.')) {
            ++m_position;
            continue;
        }
        break;
    }

    return m_html.mid(start, m_position - start);
}

QString HtmlParser::readAttributeValue()
{
    if (atEnd()) {
        return {};
    }

    const QChar quote = peek();
    if (quote == QLatin1Char('"') || quote == QLatin1Char('\'')) {
        ++m_position;
        const qsizetype start = m_position;
        while (!atEnd() && peek() != quote) {
            ++m_position;
        }

        const QString value = m_html.mid(start, m_position - start);
        if (!atEnd()) {
            ++m_position;
        }
        return value;
    }

    const qsizetype start = m_position;
    while (!atEnd() && !peek().isSpace() && peek() != QLatin1Char('>') && !startsWith(QStringLiteral("/>"))) {
        ++m_position;
    }

    return m_html.mid(start, m_position - start);
}

bool HtmlParser::isVoidElement(const QString &tagName) const
{
    static const QSet<QString> voidElements = {
        QStringLiteral("area"),
        QStringLiteral("base"),
        QStringLiteral("br"),
        QStringLiteral("col"),
        QStringLiteral("embed"),
        QStringLiteral("hr"),
        QStringLiteral("img"),
        QStringLiteral("input"),
        QStringLiteral("link"),
        QStringLiteral("meta"),
        QStringLiteral("param"),
        QStringLiteral("source"),
        QStringLiteral("track"),
        QStringLiteral("wbr"),
    };

    return voidElements.contains(tagName);
}
}
