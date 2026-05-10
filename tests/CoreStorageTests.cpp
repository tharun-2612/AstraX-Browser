#include "../src/core/UrlTools.h"
#include "../src/engine/CssParser.h"
#include "../src/engine/HtmlParser.h"
#include "../src/engine/LayoutEngine.h"
#include "../src/storage/BookmarksStore.h"
#include "../src/storage/HistoryStore.h"
#include "../src/storage/SessionStore.h"
#include "../src/storage/SettingsStore.h"

#include <QTemporaryDir>
#include <QTest>
#include <QUrlQuery>

#include <memory>

class CoreStorageTests final : public QObject
{
    Q_OBJECT

private slots:
    void resolvesHostNames();
    void resolvesSearchTerms();
    void resolvesCustomSearchTemplates();
    void hidesInternalUrls();
    void persistsBookmarks();
    void compactsHistoryDuplicates();
    void persistsSettings();
    void persistsSessions();
    void parsesHtmlIntoDomTree();
    void parsesCssAndMatchesDomStyles();
    void calculatesBasicLayoutBoxes();
};

void CoreStorageTests::resolvesHostNames()
{
    const QUrl url = astra::UrlTools::resolveUserInput(QStringLiteral("example.com"));
    QCOMPARE(url.host(), QStringLiteral("example.com"));
    QVERIFY(url.scheme() == QStringLiteral("http") || url.scheme() == QStringLiteral("https"));
}

void CoreStorageTests::resolvesSearchTerms()
{
    const QUrl url = astra::UrlTools::resolveUserInput(QStringLiteral("modern c++ browser"));
    QCOMPARE(url.host(), QStringLiteral("duckduckgo.com"));

    const QUrlQuery query(url);
    QCOMPARE(query.queryItemValue(QStringLiteral("q")), QStringLiteral("modern c++ browser"));
}

void CoreStorageTests::resolvesCustomSearchTemplates()
{
    const QUrl url = astra::UrlTools::resolveUserInput(
        QStringLiteral("qt webengine"),
        QStringLiteral("https://www.google.com/search?q={query}"));

    QCOMPARE(url.host(), QStringLiteral("www.google.com"));
    const QUrlQuery query(url);
    QCOMPARE(query.queryItemValue(QStringLiteral("q")), QStringLiteral("qt webengine"));
}

void CoreStorageTests::hidesInternalUrls()
{
    QCOMPARE(astra::UrlTools::displayUrl(QUrl(QStringLiteral("about:blank"))), QString());
}

void CoreStorageTests::persistsBookmarks()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString filePath = tempDir.filePath(QStringLiteral("bookmarks.json"));

    astra::BookmarksStore store(filePath);
    QVERIFY(store.addBookmark(QStringLiteral("Qt"), QUrl(QStringLiteral("https://www.qt.io/"))));
    QVERIFY(!store.addBookmark(QStringLiteral("Qt Duplicate"), QUrl(QStringLiteral("https://www.qt.io/"))));
    QCOMPARE(store.bookmarks().size(), 1);

    astra::BookmarksStore loaded(filePath);
    QVERIFY(loaded.load());
    QCOMPARE(loaded.bookmarks().size(), 1);
    QCOMPARE(loaded.bookmarks().first().title, QStringLiteral("Qt"));
}

void CoreStorageTests::compactsHistoryDuplicates()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    astra::HistoryStore history(tempDir.filePath(QStringLiteral("history.json")));
    history.appendVisit(QStringLiteral("First"), QUrl(QStringLiteral("https://example.com/docs#intro")));
    history.appendVisit(QStringLiteral("Second"), QUrl(QStringLiteral("https://example.com/docs#details")));

    QCOMPARE(history.entries().size(), 1);
    QCOMPARE(history.entries().first().title, QStringLiteral("Second"));
}

void CoreStorageTests::persistsSettings()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    astra::SettingsStore store(tempDir.filePath(QStringLiteral("settings.json")));
    astra::BrowserSettings settings;
    settings.homePage = QUrl(QStringLiteral("https://example.com/"));
    settings.searchUrlTemplate = QStringLiteral("https://search.example.com/?q={query}");
    settings.restoreSession = false;
    settings.blockTrackers = false;

    QVERIFY(store.update(settings));

    astra::SettingsStore loaded(tempDir.filePath(QStringLiteral("settings.json")));
    QVERIFY(loaded.load());
    QCOMPARE(loaded.settings().homePage, QUrl(QStringLiteral("https://example.com/")));
    QCOMPARE(loaded.settings().searchUrlTemplate, QStringLiteral("https://search.example.com/?q={query}"));
    QVERIFY(!loaded.settings().restoreSession);
    QVERIFY(!loaded.settings().blockTrackers);
}

void CoreStorageTests::persistsSessions()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    astra::BrowserSession session;
    session.tabs = {
        QUrl(QStringLiteral("https://example.com/")),
        QUrl(QStringLiteral("about:blank")),
        QUrl(QStringLiteral("https://www.qt.io/")),
    };
    session.currentIndex = 1;

    astra::SessionStore store(tempDir.filePath(QStringLiteral("session.json")));
    QVERIFY(store.save(session));

    astra::SessionStore loaded(tempDir.filePath(QStringLiteral("session.json")));
    QVERIFY(loaded.load());
    QCOMPARE(loaded.session().tabs.size(), 2);
    QCOMPARE(loaded.session().tabs.first(), QUrl(QStringLiteral("https://example.com/")));
    QCOMPARE(loaded.session().tabs.last(), QUrl(QStringLiteral("https://www.qt.io/")));
}

void CoreStorageTests::parsesHtmlIntoDomTree()
{
    astra::engine::HtmlParser parser;
    const std::unique_ptr<astra::engine::DomNode> document = parser.parse(QStringLiteral(R"(
        <!doctype html>
        <html>
          <body>
            <h1 class="title">AstraX</h1>
            <img src="logo.png">
          </body>
        </html>
    )"));

    QVERIFY(parser.errors().isEmpty());
    QCOMPARE(document->children.size(), static_cast<size_t>(1));

    const astra::engine::DomNode *html = document->children.at(0).get();
    QCOMPARE(html->name, QStringLiteral("html"));
    QCOMPARE(html->children.size(), static_cast<size_t>(1));

    const astra::engine::DomNode *body = html->children.at(0).get();
    QCOMPARE(body->name, QStringLiteral("body"));
    QCOMPARE(body->children.size(), static_cast<size_t>(2));

    const astra::engine::DomNode *heading = body->children.at(0).get();
    QCOMPARE(heading->name, QStringLiteral("h1"));
    QCOMPARE(heading->attributes.first().name, QStringLiteral("class"));
    QCOMPARE(heading->attributes.first().value, QStringLiteral("title"));
    QCOMPARE(heading->children.front()->text, QStringLiteral("AstraX"));
}

void CoreStorageTests::parsesCssAndMatchesDomStyles()
{
    astra::engine::HtmlParser htmlParser;
    const std::unique_ptr<astra::engine::DomNode> document = htmlParser.parse(QStringLiteral(R"(
        <html>
          <body>
            <h1 id="main-title" class="title">AstraX</h1>
          </body>
        </html>
    )"));

    astra::engine::CssParser cssParser;
    const astra::engine::StyleSheet styleSheet = cssParser.parse(QStringLiteral(R"(
        h1 { color: blue; }
        .title { font-weight: 700; }
        #main-title { color: teal; }
    )"));

    QVERIFY(htmlParser.errors().isEmpty());
    QVERIFY(cssParser.errors().isEmpty());
    QCOMPARE(styleSheet.rules.size(), 3);

    const astra::engine::DomNode *heading = document->children.at(0)->children.at(0)->children.at(0).get();
    const QVector<astra::engine::CssRule> rules = astra::engine::matchingRules(styleSheet, *heading);

    QCOMPARE(rules.size(), 3);
    QCOMPARE(rules.at(0).selector.tagName, QStringLiteral("h1"));
    QCOMPARE(rules.at(1).selector.classes.first(), QStringLiteral("title"));
    QCOMPARE(rules.at(2).selector.id, QStringLiteral("main-title"));
}

void CoreStorageTests::calculatesBasicLayoutBoxes()
{
    astra::engine::HtmlParser htmlParser;
    const std::unique_ptr<astra::engine::DomNode> document = htmlParser.parse(QStringLiteral(R"(
        <html>
          <body>
            <section id="panel">
              <p>Hello layout</p>
            </section>
          </body>
        </html>
    )"));

    astra::engine::CssParser cssParser;
    const astra::engine::StyleSheet styleSheet = cssParser.parse(QStringLiteral(R"(
        body { padding: 10px; }
        #panel { width: 300px; padding: 12px; margin-bottom: 8px; }
        p { height: 30px; }
    )"));

    astra::engine::LayoutEngine layoutEngine;
    const astra::engine::LayoutResult layout = layoutEngine.layout(*document, styleSheet, 640);

    QVERIFY(layout.notes.isEmpty());
    QVERIFY(!layout.root.children.isEmpty());
    QVERIFY(layout.root.rect.height() > 0);

    const astra::engine::LayoutBox &html = layout.root.children.first();
    const astra::engine::LayoutBox &body = html.children.first();
    const astra::engine::LayoutBox &section = body.children.first();

    QCOMPARE(section.rect.width(), 300);
    QCOMPARE(section.paddingTop, 12);
    QVERIFY(section.rect.height() >= 54);
}

QTEST_MAIN(CoreStorageTests)
#include "CoreStorageTests.moc"
