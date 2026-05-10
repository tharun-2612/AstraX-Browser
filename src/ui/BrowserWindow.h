#pragma once

#include "../storage/BookmarksStore.h"
#include "../storage/HistoryStore.h"
#include "../storage/SessionStore.h"
#include "../storage/SettingsStore.h"

#include <QHash>
#include <QMainWindow>
#include <QString>
#include <QUrl>

class QAction;
class QCloseEvent;
class QCompleter;
class QDockWidget;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QMenu;
class QNetworkAccessManager;
class QNetworkReply;
class QProgressBar;
class QStringListModel;
class QTabWidget;
class QToolBar;
class QToolButton;
class QWebEngineDownloadRequest;
class QWebEngineProfile;

namespace astra
{
class BrowserView;
class RequestBlocker;

class BrowserWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit BrowserWindow(bool privateMode = false, QWidget *parent = nullptr);

private:
    void closeEvent(QCloseEvent *event) override;

    void setupProfile();
    void setupUi();
    void applyModernStyle();
    void setupLibrarySidebar();
    void setupMenus();
    void setupShortcuts();
    void connectStoreSignals();

    BrowserView *createTab(const QUrl &url = QUrl(), bool makeCurrent = true);
    BrowserView *currentView() const;

    void navigateToUserInput();
    void navigateHome();
    void closeTab(int index);
    void updateWindowForCurrentTab();
    void updateNavigationState();
    void applySettings(const BrowserSettings &settings);

    void addCurrentBookmark();
    void rebuildBookmarksMenu();
    void rebuildHistoryMenu();
    void rebuildLibrarySidebar();
    void rebuildBookmarksList();
    void rebuildHistoryList();
    void rebuildSearchSuggestions();
    void openLibraryUrl(QListWidgetItem *item);
    void showLibraryTab(int index);

    void showFindBar();
    void findInPage(bool backwards = false);
    void openEngineLab();
    void openDevTools();
    void openPageInfoDialog();
    void openSettingsDialog();
    void openPrivateWindow();
    void handleDownload(QWebEngineDownloadRequest *download);
    void refreshInternetLocation();
    void requestInternetLocation(const QUrl &url);
    void handleInternetLocationReply(QNetworkReply *reply);
    void applyDashboardLocation();
    void applyDashboardLocationTo(BrowserView *view);
    void showAstraSearch(BrowserView *view, const QString &query);
    bool isStartDashboard(const BrowserView *view) const;
    bool isInternalPage(const BrowserView *view) const;
    bool restorePreviousSession();
    [[nodiscard]] BrowserSession currentSession() const;
    void saveSession();

    bool m_privateMode = false;
    QWebEngineProfile *m_profile = nullptr;
    RequestBlocker *m_requestBlocker = nullptr;

    QTabWidget *m_tabs = nullptr;
    QLineEdit *m_urlEdit = nullptr;
    QCompleter *m_addressCompleter = nullptr;
    QStringListModel *m_suggestionModel = nullptr;
    QHash<QString, QUrl> m_suggestionTargets;
    QProgressBar *m_progress = nullptr;
    QLabel *m_securityLabel = nullptr;
    QToolButton *m_pageInfoButton = nullptr;

    QToolBar *m_findToolBar = nullptr;
    QLineEdit *m_findEdit = nullptr;

    QDockWidget *m_libraryDock = nullptr;
    QTabWidget *m_libraryTabs = nullptr;
    QListWidget *m_bookmarksList = nullptr;
    QListWidget *m_historyList = nullptr;
    QListWidget *m_downloadsList = nullptr;

    QNetworkAccessManager *m_networkAccess = nullptr;
    QString m_locationMain;
    QString m_locationDetail;
    QString m_locationBadge;
    QString m_locationSource;
    QString m_locationStatus;
    QString m_locationCodeText;
    bool m_locationLookupInFlight = false;

    QMenu *m_bookmarksMenu = nullptr;
    QMenu *m_historyMenu = nullptr;

    QAction *m_backAction = nullptr;
    QAction *m_forwardAction = nullptr;
    QAction *m_reloadAction = nullptr;
    QAction *m_stopAction = nullptr;
    QAction *m_addBookmarkAction = nullptr;
    QAction *m_blockTrackersAction = nullptr;

    SettingsStore m_settingsStore;
    SessionStore m_sessionStore;
    BookmarksStore m_bookmarks;
    HistoryStore m_history;
};
}
