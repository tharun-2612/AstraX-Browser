#pragma once

#include "../engine/CssParser.h"
#include "../engine/LayoutEngine.h"

#include <QHash>
#include <QMainWindow>

#include <memory>

class QLabel;
class QPlainTextEdit;
class QTextEdit;
class QTreeWidget;
class QTreeWidgetItem;

namespace astra::engine
{
struct DomNode;
}

namespace astra
{
class EngineLabWindow final : public QMainWindow
{
public:
    explicit EngineLabWindow(QWidget *parent = nullptr);

private:
    void parseInput();
    void loadSample();
    void clearInput();
    void addDomNode(QTreeWidgetItem *parentItem, const engine::DomNode &node);
    void addLayoutBox(QTreeWidgetItem *parentItem, const engine::LayoutBox &box);
    void updateStyleInspector(QTreeWidgetItem *currentItem);
    [[nodiscard]] QString nodeLabel(const engine::DomNode &node) const;
    [[nodiscard]] QString nodeDetails(const engine::DomNode &node) const;
    [[nodiscard]] QString matchedStyleText(const engine::DomNode &node) const;

    QTextEdit *m_htmlEdit = nullptr;
    QTextEdit *m_cssEdit = nullptr;
    QTreeWidget *m_domTree = nullptr;
    QTreeWidget *m_layoutTree = nullptr;
    QPlainTextEdit *m_errorsEdit = nullptr;
    QPlainTextEdit *m_styleInspector = nullptr;
    QLabel *m_statusLabel = nullptr;
    std::unique_ptr<engine::DomNode> m_document;
    engine::StyleSheet m_styleSheet;
    engine::LayoutResult m_layoutResult;
    QHash<QTreeWidgetItem *, const engine::DomNode *> m_treeNodes;
};
}
