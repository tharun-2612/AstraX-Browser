#include "EngineLabWindow.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QStringList>
#include <QTabWidget>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace
{
QString sampleHtml()
{
    return QStringLiteral(R"(<!doctype html>
<html lang="en">
      <head>
        <title>AstraX Engine Lab</title>
        <meta charset="utf-8">
      </head>
      <body>
        <header class="hero">
          <h1>AstraX Browser Engine</h1>
          <p data-role="subtitle">HTML and CSS parser lab</p>
        </header>
        <main>
          <section id="features">
        <h2>Step 3</h2>
        <ul>
          <li>Tokenize tags</li>
          <li>Read attributes</li>
          <li>Match CSS selectors</li>
          <li>Calculate layout boxes</li>
        </ul>
      </section>
    </main>
  </body>
</html>)");
}

QString sampleCss()
{
    return QStringLiteral(R"(body {
  font-family: Inter, Segoe UI, sans-serif;
  color: #102033;
  background: #f6f8fb;
}

.hero {
  padding: 24px;
  background: #e0f2fe;
  margin-bottom: 14px;
}

header {
  border-radius: 12px;
}

h1 {
  color: #0f766e;
  font-size: 32px;
  letter-spacing: 0;
}

#features {
  border: 1px solid #94a3b8;
  padding: 16px;
  width: 620px;
}

li {
  margin-bottom: 6px;
})");
}

QString compact(QString value, qsizetype maxLength)
{
    value = value.simplified();
    if (value.size() <= maxLength) {
        return value;
    }

    return value.left(maxLength - 3) + QStringLiteral("...");
}
}

namespace astra
{
EngineLabWindow::EngineLabWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("AstraX Engine Lab - HTML/CSS Parser"));
    resize(1220, 820);

    auto *root = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(14, 14, 14, 14);
    rootLayout->setSpacing(10);

    auto *topRow = new QHBoxLayout;
    auto *parseButton = new QPushButton(QStringLiteral("Parse HTML + CSS"), root);
    auto *sampleButton = new QPushButton(QStringLiteral("Load Sample"), root);
    auto *clearButton = new QPushButton(QStringLiteral("Clear"), root);
    m_statusLabel = new QLabel(QStringLiteral("Ready"), root);
    m_statusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    topRow->addWidget(parseButton);
    topRow->addWidget(sampleButton);
    topRow->addWidget(clearButton);
    topRow->addStretch();
    topRow->addWidget(m_statusLabel, 1);
    rootLayout->addLayout(topRow);

    auto *splitter = new QSplitter(Qt::Horizontal, root);

    auto *sourceTabs = new QTabWidget(splitter);
    sourceTabs->setDocumentMode(true);

    m_htmlEdit = new QTextEdit(sourceTabs);
    m_htmlEdit->setAcceptRichText(false);
    m_htmlEdit->setLineWrapMode(QTextEdit::NoWrap);
    m_htmlEdit->setPlaceholderText(QStringLiteral("Paste HTML here, then click Parse HTML + CSS"));
    m_htmlEdit->setFontFamily(QStringLiteral("Cascadia Mono"));
    sourceTabs->addTab(m_htmlEdit, QStringLiteral("HTML"));

    m_cssEdit = new QTextEdit(sourceTabs);
    m_cssEdit->setAcceptRichText(false);
    m_cssEdit->setLineWrapMode(QTextEdit::NoWrap);
    m_cssEdit->setPlaceholderText(QStringLiteral("Paste CSS here, then click Parse HTML + CSS"));
    m_cssEdit->setFontFamily(QStringLiteral("Cascadia Mono"));
    sourceTabs->addTab(m_cssEdit, QStringLiteral("CSS"));

    auto *rightPanel = new QWidget(splitter);
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(8);

    auto *treeLabel = new QLabel(QStringLiteral("DOM Tree"), rightPanel);
    treeLabel->setStyleSheet(QStringLiteral("font-weight: 700;"));
    rightLayout->addWidget(treeLabel);

    m_domTree = new QTreeWidget(rightPanel);
    m_domTree->setColumnCount(2);
    m_domTree->setHeaderLabels(QStringList{ QStringLiteral("Node"), QStringLiteral("Details") });
    m_domTree->header()->setStretchLastSection(true);
    m_domTree->setAlternatingRowColors(true);
    rightLayout->addWidget(m_domTree, 1);

    auto *layoutLabel = new QLabel(QStringLiteral("Layout Boxes"), rightPanel);
    layoutLabel->setStyleSheet(QStringLiteral("font-weight: 700;"));
    rightLayout->addWidget(layoutLabel);

    m_layoutTree = new QTreeWidget(rightPanel);
    m_layoutTree->setColumnCount(2);
    m_layoutTree->setHeaderLabels(QStringList{ QStringLiteral("Box"), QStringLiteral("Geometry") });
    m_layoutTree->header()->setStretchLastSection(true);
    m_layoutTree->setAlternatingRowColors(true);
    m_layoutTree->setMaximumHeight(180);
    rightLayout->addWidget(m_layoutTree);

    auto *errorsLabel = new QLabel(QStringLiteral("Parser Notes"), rightPanel);
    errorsLabel->setStyleSheet(QStringLiteral("font-weight: 700;"));
    rightLayout->addWidget(errorsLabel);

    m_errorsEdit = new QPlainTextEdit(rightPanel);
    m_errorsEdit->setReadOnly(true);
    m_errorsEdit->setMaximumHeight(120);
    m_errorsEdit->setPlainText(QStringLiteral("No parse run yet."));
    rightLayout->addWidget(m_errorsEdit);

    auto *stylesLabel = new QLabel(QStringLiteral("Matched Styles"), rightPanel);
    stylesLabel->setStyleSheet(QStringLiteral("font-weight: 700;"));
    rightLayout->addWidget(stylesLabel);

    m_styleInspector = new QPlainTextEdit(rightPanel);
    m_styleInspector->setReadOnly(true);
    m_styleInspector->setMaximumHeight(170);
    m_styleInspector->setPlainText(QStringLiteral("Select an element node to inspect matched CSS rules."));
    rightLayout->addWidget(m_styleInspector);

    splitter->addWidget(sourceTabs);
    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    rootLayout->addWidget(splitter, 1);
    setCentralWidget(root);

    setStyleSheet(QStringLiteral(R"(
        QMainWindow, QWidget {
            background: #f6f8fb;
            color: #172033;
        }

        QTextEdit, QPlainTextEdit, QTreeWidget {
            background: #ffffff;
            border: 1px solid #d6dee9;
            border-radius: 8px;
            padding: 8px;
            selection-background-color: #2563eb;
        }

        QPushButton {
            min-height: 34px;
            border: 1px solid #c8d4e3;
            border-radius: 8px;
            padding: 0 14px;
            background: #ffffff;
            font-weight: 700;
        }

        QPushButton:hover {
            background: #e8f0fb;
            border-color: #9bb7d8;
        }

        QHeaderView::section {
            background: #eef2f7;
            border: 0;
            border-bottom: 1px solid #d6dee9;
            padding: 7px;
            font-weight: 700;
        }
    )"));

    connect(parseButton, &QPushButton::clicked, this, &EngineLabWindow::parseInput);
    connect(sampleButton, &QPushButton::clicked, this, &EngineLabWindow::loadSample);
    connect(clearButton, &QPushButton::clicked, this, &EngineLabWindow::clearInput);
    connect(m_domTree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *currentItem) {
        updateStyleInspector(currentItem);
    });

    loadSample();
}

void EngineLabWindow::parseInput()
{
    engine::HtmlParser parser;
    engine::CssParser cssParser;
    engine::LayoutEngine layoutEngine;
    m_document = parser.parse(m_htmlEdit->toPlainText());
    m_styleSheet = cssParser.parse(m_cssEdit->toPlainText());
    m_layoutResult = layoutEngine.layout(*m_document, m_styleSheet, 760);

    m_treeNodes.clear();
    m_domTree->clear();
    addDomNode(nullptr, *m_document);
    m_domTree->expandToDepth(2);
    m_domTree->resizeColumnToContents(0);

    m_layoutTree->clear();
    addLayoutBox(nullptr, m_layoutResult.root);
    m_layoutTree->expandToDepth(2);
    m_layoutTree->resizeColumnToContents(0);

    QStringList notes;
    if (parser.errors().isEmpty() && cssParser.errors().isEmpty()) {
        notes.append(QStringLiteral("Parse completed without structural errors."));
    }
    for (const QString &error : parser.errors()) {
        notes.append(QStringLiteral("HTML: %1").arg(error));
    }
    for (const QString &error : cssParser.errors()) {
        notes.append(QStringLiteral("CSS: %1").arg(error));
    }
    for (const QString &note : m_layoutResult.notes) {
        notes.append(QStringLiteral("Layout: %1").arg(note));
    }
    m_errorsEdit->setPlainText(notes.join(QLatin1Char('\n')));
    updateStyleInspector(m_domTree->currentItem());

    m_statusLabel->setText(QStringLiteral("%1 top-level node%2, %3 CSS rule%4, layout %5px high")
        .arg(QString::number(static_cast<qulonglong>(m_document->children.size())))
        .arg(m_document->children.size() == 1 ? QString() : QStringLiteral("s"))
        .arg(QString::number(m_styleSheet.rules.size()))
        .arg(m_styleSheet.rules.size() == 1 ? QString() : QStringLiteral("s"))
        .arg(QString::number(m_layoutResult.root.rect.height())));
}

void EngineLabWindow::loadSample()
{
    m_htmlEdit->setPlainText(sampleHtml());
    m_cssEdit->setPlainText(sampleCss());
    parseInput();
}

void EngineLabWindow::clearInput()
{
    m_htmlEdit->clear();
    m_cssEdit->clear();
    m_document.reset();
    m_styleSheet.rules.clear();
    m_layoutResult = {};
    m_treeNodes.clear();
    m_domTree->clear();
    m_layoutTree->clear();
    m_errorsEdit->setPlainText(QStringLiteral("Input cleared."));
    m_styleInspector->setPlainText(QStringLiteral("Select an element node to inspect matched CSS rules."));
    m_statusLabel->setText(QStringLiteral("Ready"));
}

void EngineLabWindow::addDomNode(QTreeWidgetItem *parentItem, const engine::DomNode &node)
{
    auto *item = parentItem
        ? new QTreeWidgetItem(parentItem)
        : new QTreeWidgetItem(m_domTree);
    item->setText(0, nodeLabel(node));
    item->setText(1, nodeDetails(node));
    m_treeNodes.insert(item, &node);

    for (const std::unique_ptr<engine::DomNode> &child : node.children) {
        addDomNode(item, *child);
    }
}

void EngineLabWindow::addLayoutBox(QTreeWidgetItem *parentItem, const engine::LayoutBox &box)
{
    auto *item = parentItem
        ? new QTreeWidgetItem(parentItem)
        : new QTreeWidgetItem(m_layoutTree);

    item->setText(0, box.label);
    item->setText(1, QStringLiteral("x:%1 y:%2 w:%3 h:%4  margin:%5/%6/%7/%8  padding:%9/%10/%11/%12")
        .arg(QString::number(box.rect.x()), QString::number(box.rect.y()))
        .arg(QString::number(box.rect.width()), QString::number(box.rect.height()))
        .arg(QString::number(box.marginTop), QString::number(box.marginRight), QString::number(box.marginBottom), QString::number(box.marginLeft))
        .arg(QString::number(box.paddingTop), QString::number(box.paddingRight), QString::number(box.paddingBottom), QString::number(box.paddingLeft)));

    for (const engine::LayoutBox &child : box.children) {
        addLayoutBox(item, child);
    }
}

void EngineLabWindow::updateStyleInspector(QTreeWidgetItem *currentItem)
{
    const engine::DomNode *node = m_treeNodes.value(currentItem, nullptr);
    if (!node) {
        m_styleInspector->setPlainText(QStringLiteral("Select an element node to inspect matched CSS rules."));
        return;
    }

    m_styleInspector->setPlainText(matchedStyleText(*node));
}

QString EngineLabWindow::nodeLabel(const engine::DomNode &node) const
{
    switch (node.type) {
    case engine::DomNode::Type::Document:
        return QStringLiteral("#document");
    case engine::DomNode::Type::Element:
        return QStringLiteral("<%1>").arg(node.name);
    case engine::DomNode::Type::Text:
        return QStringLiteral("#text");
    case engine::DomNode::Type::Comment:
        return QStringLiteral("#comment");
    }

    return QStringLiteral("#unknown");
}

QString EngineLabWindow::nodeDetails(const engine::DomNode &node) const
{
    switch (node.type) {
    case engine::DomNode::Type::Document:
        return QStringLiteral("%1 child node%2")
            .arg(QString::number(static_cast<qulonglong>(node.children.size())))
            .arg(node.children.size() == 1 ? QString() : QStringLiteral("s"));
    case engine::DomNode::Type::Element: {
        QStringList attributes;
        for (const engine::HtmlAttribute &attribute : node.attributes) {
            attributes.append(attribute.value.isEmpty()
                ? attribute.name
                : QStringLiteral("%1=\"%2\"").arg(attribute.name, attribute.value));
        }
        return attributes.isEmpty() ? QStringLiteral("No attributes") : attributes.join(QStringLiteral("  "));
    }
    case engine::DomNode::Type::Text:
        return compact(node.text, 120);
    case engine::DomNode::Type::Comment:
        return compact(node.text, 120);
    }

    return {};
}

QString EngineLabWindow::matchedStyleText(const engine::DomNode &node) const
{
    if (node.type != engine::DomNode::Type::Element) {
        return QStringLiteral("CSS rules apply to element nodes. Select an element like <body>, <h1>, or <section>.");
    }

    const QVector<engine::CssRule> rules = engine::matchingRules(m_styleSheet, node);
    if (rules.isEmpty()) {
        return QStringLiteral("No CSS rules matched <%1>.").arg(node.name);
    }

    QStringList lines;
    for (const engine::CssRule &rule : rules) {
        lines.append(QStringLiteral("%1  specificity %2")
            .arg(rule.selector.text, QString::number(rule.selector.specificity)));
        for (const engine::CssDeclaration &declaration : rule.declarations) {
            lines.append(QStringLiteral("  %1: %2;").arg(declaration.property, declaration.value));
        }
    }

    return lines.join(QLatin1Char('\n'));
}
}
