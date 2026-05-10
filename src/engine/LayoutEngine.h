#pragma once

#include "CssParser.h"

#include <QHash>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QVector>

namespace astra::engine
{
struct LayoutBox
{
    const DomNode *node = nullptr;
    QString label;
    QRect rect;
    int marginTop = 0;
    int marginRight = 0;
    int marginBottom = 0;
    int marginLeft = 0;
    int paddingTop = 0;
    int paddingRight = 0;
    int paddingBottom = 0;
    int paddingLeft = 0;
    QVector<LayoutBox> children;
};

struct LayoutResult
{
    LayoutBox root;
    QStringList notes;
};

class LayoutEngine final
{
public:
    LayoutResult layout(const DomNode &document, const StyleSheet &styleSheet, int viewportWidth = 800);

private:
    struct BoxMetrics
    {
        int width = 0;
        int explicitHeight = -1;
        int marginTop = 0;
        int marginRight = 0;
        int marginBottom = 0;
        int marginLeft = 0;
        int paddingTop = 0;
        int paddingRight = 0;
        int paddingBottom = 0;
        int paddingLeft = 0;
        int fontSize = 16;
        int lineHeight = 20;
    };

    [[nodiscard]] bool createsLayoutBox(const DomNode &node) const;
    [[nodiscard]] QString layoutLabel(const DomNode &node) const;
    [[nodiscard]] QHash<QString, QString> computedStyles(const DomNode &node) const;
    [[nodiscard]] BoxMetrics metricsFor(const DomNode &node, int availableWidth) const;
    [[nodiscard]] int cssPixels(const QHash<QString, QString> &styles, const QString &property, int fallback) const;
    [[nodiscard]] int parsePixels(const QString &value, int fallback) const;

    bool layoutNode(const DomNode &node, int x, int y, int availableWidth, LayoutBox *outBox, int *consumedHeight);

    const StyleSheet *m_styleSheet = nullptr;
};
}
