#include "LayoutEngine.h"

#include <QHash>
#include <QRegularExpression>

#include <algorithm>
#include <memory>

namespace astra::engine
{
LayoutResult LayoutEngine::layout(const DomNode &document, const StyleSheet &styleSheet, int viewportWidth)
{
    m_styleSheet = &styleSheet;

    LayoutResult result;
    result.root.node = &document;
    result.root.label = QStringLiteral("#viewport");
    result.root.rect = QRect(0, 0, std::max(320, viewportWidth), 0);

    int currentY = 0;
    for (const std::unique_ptr<DomNode> &child : document.children) {
        LayoutBox childBox;
        int consumedHeight = 0;
        if (layoutNode(*child, 0, currentY, result.root.rect.width(), &childBox, &consumedHeight)) {
            currentY += consumedHeight;
            result.root.children.append(childBox);
        }
    }

    result.root.rect.setHeight(currentY);
    if (result.root.children.isEmpty()) {
        result.notes.append(QStringLiteral("No renderable DOM nodes were found."));
    }

    return result;
}

bool LayoutEngine::createsLayoutBox(const DomNode &node) const
{
    if (node.type == DomNode::Type::Text) {
        return !node.text.trimmed().isEmpty();
    }

    if (node.type != DomNode::Type::Element) {
        return false;
    }

    static const QStringList nonRenderable = {
        QStringLiteral("head"),
        QStringLiteral("meta"),
        QStringLiteral("title"),
        QStringLiteral("link"),
        QStringLiteral("script"),
        QStringLiteral("style"),
    };
    if (nonRenderable.contains(node.name)) {
        return false;
    }

    return computedStyles(node).value(QStringLiteral("display")) != QStringLiteral("none");
}

QString LayoutEngine::layoutLabel(const DomNode &node) const
{
    if (node.type == DomNode::Type::Text) {
        return QStringLiteral("#text \"%1\"").arg(node.text.simplified().left(42));
    }

    return QStringLiteral("<%1>").arg(node.name);
}

QHash<QString, QString> LayoutEngine::computedStyles(const DomNode &node) const
{
    QHash<QString, QString> styles;
    if (!m_styleSheet || node.type != DomNode::Type::Element) {
        return styles;
    }

    const QVector<CssRule> rules = matchingRules(*m_styleSheet, node);
    for (const CssRule &rule : rules) {
        for (const CssDeclaration &declaration : rule.declarations) {
            styles.insert(declaration.property, declaration.value);
        }
    }

    const QString inlineStyle = attributeValue(node, QStringLiteral("style"));
    const QStringList declarations = inlineStyle.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &declaration : declarations) {
        const qsizetype colon = declaration.indexOf(QLatin1Char(':'));
        if (colon < 0) {
            continue;
        }
        styles.insert(declaration.left(colon).trimmed().toLower(), declaration.mid(colon + 1).trimmed());
    }

    return styles;
}

LayoutEngine::BoxMetrics LayoutEngine::metricsFor(const DomNode &node, int availableWidth) const
{
    const QHash<QString, QString> styles = computedStyles(node);
    BoxMetrics metrics;
    const int fallbackWidth = std::max(0, availableWidth);

    const int margin = cssPixels(styles, QStringLiteral("margin"), 0);
    metrics.marginTop = cssPixels(styles, QStringLiteral("margin-top"), margin);
    metrics.marginRight = cssPixels(styles, QStringLiteral("margin-right"), margin);
    metrics.marginBottom = cssPixels(styles, QStringLiteral("margin-bottom"), margin);
    metrics.marginLeft = cssPixels(styles, QStringLiteral("margin-left"), margin);

    const int padding = cssPixels(styles, QStringLiteral("padding"), 0);
    metrics.paddingTop = cssPixels(styles, QStringLiteral("padding-top"), padding);
    metrics.paddingRight = cssPixels(styles, QStringLiteral("padding-right"), padding);
    metrics.paddingBottom = cssPixels(styles, QStringLiteral("padding-bottom"), padding);
    metrics.paddingLeft = cssPixels(styles, QStringLiteral("padding-left"), padding);

    metrics.width = cssPixels(
        styles,
        QStringLiteral("width"),
        std::max(0, fallbackWidth - metrics.marginLeft - metrics.marginRight));
    metrics.width = std::min(metrics.width, std::max(0, fallbackWidth - metrics.marginLeft - metrics.marginRight));
    metrics.explicitHeight = cssPixels(styles, QStringLiteral("height"), -1);
    metrics.fontSize = cssPixels(styles, QStringLiteral("font-size"), 16);
    metrics.lineHeight = cssPixels(styles, QStringLiteral("line-height"), std::max(18, metrics.fontSize + 4));
    return metrics;
}

int LayoutEngine::cssPixels(const QHash<QString, QString> &styles, const QString &property, int fallback) const
{
    return parsePixels(styles.value(property), fallback);
}

int LayoutEngine::parsePixels(const QString &value, int fallback) const
{
    const QString trimmed = value.trimmed().toLower();
    if (trimmed.isEmpty() || trimmed == QStringLiteral("auto")) {
        return fallback;
    }

    static const QRegularExpression numberPattern(QStringLiteral("^(-?\\d+)(px)?$"));
    const QRegularExpressionMatch match = numberPattern.match(trimmed);
    if (!match.hasMatch()) {
        return fallback;
    }

    bool ok = false;
    const int pixels = match.captured(1).toInt(&ok);
    return ok ? std::max(0, pixels) : fallback;
}

bool LayoutEngine::layoutNode(const DomNode &node, int x, int y, int availableWidth, LayoutBox *outBox, int *consumedHeight)
{
    if (!createsLayoutBox(node)) {
        int skippedHeight = 0;
        LayoutBox anonymous;
        for (const std::unique_ptr<DomNode> &child : node.children) {
            LayoutBox childBox;
            int childConsumed = 0;
            if (layoutNode(*child, x, y + skippedHeight, availableWidth, &childBox, &childConsumed)) {
                skippedHeight += childConsumed;
                anonymous.children.append(childBox);
            }
        }

        if (anonymous.children.isEmpty()) {
            *consumedHeight = 0;
            return false;
        }

        anonymous.node = &node;
        anonymous.label = layoutLabel(node);
        anonymous.rect = QRect(x, y, availableWidth, skippedHeight);
        *outBox = anonymous;
        *consumedHeight = skippedHeight;
        return true;
    }

    const BoxMetrics metrics = metricsFor(node, availableWidth);
    LayoutBox box;
    box.node = &node;
    box.label = layoutLabel(node);
    box.marginTop = metrics.marginTop;
    box.marginRight = metrics.marginRight;
    box.marginBottom = metrics.marginBottom;
    box.marginLeft = metrics.marginLeft;
    box.paddingTop = metrics.paddingTop;
    box.paddingRight = metrics.paddingRight;
    box.paddingBottom = metrics.paddingBottom;
    box.paddingLeft = metrics.paddingLeft;

    const int boxX = x + metrics.marginLeft;
    const int boxY = y + metrics.marginTop;
    int boxHeight = 0;

    if (node.type == DomNode::Type::Text) {
        const int usableWidth = std::max(1, metrics.width);
        const int approxCharactersPerLine = std::max(1, usableWidth / 8);
        const int textLength = static_cast<int>(node.text.simplified().size());
        const int lineCount = std::max(1, (textLength + approxCharactersPerLine - 1) / approxCharactersPerLine);
        boxHeight = lineCount * metrics.lineHeight;
    } else {
        int childY = boxY + metrics.paddingTop;
        const int childX = boxX + metrics.paddingLeft;
        const int childWidth = std::max(0, metrics.width - metrics.paddingLeft - metrics.paddingRight);

        for (const std::unique_ptr<DomNode> &child : node.children) {
            LayoutBox childBox;
            int childConsumed = 0;
            if (layoutNode(*child, childX, childY, childWidth, &childBox, &childConsumed)) {
                childY += childConsumed;
                box.children.append(childBox);
            }
        }

        const int contentHeight = childY - boxY;
        boxHeight = metrics.explicitHeight >= 0
            ? metrics.explicitHeight
            : std::max(metrics.lineHeight, contentHeight + metrics.paddingBottom);
    }

    box.rect = QRect(boxX, boxY, metrics.width, boxHeight);
    *outBox = box;
    *consumedHeight = metrics.marginTop + boxHeight + metrics.marginBottom;
    return true;
}
}
