#pragma once

#include <QList>
#include <QMainWindow>
#include <QPair>
#include <QPoint>
#include <QUrl>
#include <QStringList>

class QAction;
class AdaptiveTabBar;
class AnimatedAddressBar;
class QEvent;
class QFrame;
class QModelIndex;
class QNetworkAccessManager;
class QNetworkReply;
class QStandardItemModel;
class QTimer;
class QListView;
class QListWidget;
class QMenu;
class QMouseEvent;
class QProgressBar;
class QPropertyAnimation;
class QStackedWidget;
class QToolButton;
class QWebEngineDownloadRequest;
class QWebEngineProfile;
class PasswordStore;
class HidBridge;
class TrackerBlocker;
class WebView;

class BrowserWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit BrowserWindow(bool privateMode = false, QWidget *parent = nullptr);
    WebView *createTab(const QUrl &url = QUrl(), bool activate = true);

protected:
    bool event(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private slots:
    void closeTab(int index);
    void activateTab(int index);
    void navigateFromAddressBar();
    void updateFromCurrentView();
    void handleDownload(QWebEngineDownloadRequest *download);
    void showMainMenu();
    void addCurrentBookmark();
    void openSidebarItem();
    void toggleSidebar();
    void toggleDarkWebsites(bool enabled);
    void toggleTrackerBlocking(bool enabled);
    void togglePasswordAutofill(bool enabled);
    void openPrivateWindow();
    void toggleFullScreen();
    void showPasswordManager();
    void addPasswordForCurrentSite();
    void fillPasswordForCurrentSite();
    void importPasswordsFromCsv();
    void showExtensionManager();
    void showDeviceManager();
    void showHistory();
    void findOnPage();
    void printCurrentPage();
    void clearBrowsingData();
    void reopenClosedTab();
    void updateAddressSuggestions(const QString &text);

private:
    void buildUi();
    void buildMenu();
    void configureProfile();
    void applyStyle();
    void connectView(WebView *view);
    void configurePage(WebView *view);
    void configureWebHid(WebView *view);
    void restoreExtensionStates();
    void removeView(WebView *view);
    void loadBookmarks();
    void saveBookmarks() const;
    void refreshSidebar();
    void setCurrentView(WebView *view);
    void updateWindowShape();
    void updateNavigationState();
    void animateProgressTo(int value);
    void animateAddressFocus(bool focused);
    void rebuildSuggestionModel(const QString &query, const QStringList &remoteSuggestions = {});
    void requestRemoteSuggestions();
    void activateSuggestion(const QModelIndex &index);
    void selectAddressSuggestion(int delta);
    void positionSuggestionPopup();
    void animateSuggestionPopup();
    void dismissAddressSuggestions();
    void applyWebDarkMode(WebView *view);
    void recordHistory(WebView *view);
    void loadHistory();
    void saveHistory() const;
    void autofillCurrentSite();
    void fillCredential(const QString &id, const QString &username);
    void fillCredentialData(const QString &username, const QString &password);
    QUrl normalizedUrl(const QString &input) const;
    WebView *currentView() const;
    int tabIndexForView(const WebView *view) const;
    int edgeAt(const QPoint &position) const;
    void beginSystemResize(int edgeMask);

    bool m_privateMode = false;
    bool m_darkWebsites = true;
    bool m_darkUi = true;
    bool m_addressCommitPending = false;
    bool m_passwordAutofill = true;
    bool m_sidebarVisible = false;
    bool m_applyingStyle = false;
    int m_pressedEdge = 0;

    QFrame *m_root = nullptr;
    QFrame *m_topBar = nullptr;
    QFrame *m_tabStrip = nullptr;
    AdaptiveTabBar *m_tabBar = nullptr;
    AnimatedAddressBar *m_addressBar = nullptr;
    QStackedWidget *m_stack = nullptr;
    QListWidget *m_sidebar = nullptr;
    QProgressBar *m_progress = nullptr;
    QPropertyAnimation *m_progressAnimation = nullptr;
    QPropertyAnimation *m_addressWidthAnimation = nullptr;
    QPropertyAnimation *m_addressHeightAnimation = nullptr;
    QToolButton *m_backButton = nullptr;
    QToolButton *m_forwardButton = nullptr;
    QToolButton *m_reloadButton = nullptr;
    QToolButton *m_sidebarButton = nullptr;
    QToolButton *m_menuButton = nullptr;
    QToolButton *m_newTabButton = nullptr;
    QAction *m_darkWebsitesAction = nullptr;
    QAction *m_trackerBlockingAction = nullptr;
    QAction *m_passwordAutofillAction = nullptr;
    QMenu *m_mainMenu = nullptr;
    QListView *m_suggestionView = nullptr;
    QStandardItemModel *m_suggestionModel = nullptr;
    QTimer *m_suggestionTimer = nullptr;
    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_suggestionReply = nullptr;
    QString m_pendingSuggestionQuery;
    int m_selectedSuggestionRow = -1;
    QWebEngineProfile *m_profile = nullptr;
    TrackerBlocker *m_trackerBlocker = nullptr;
    PasswordStore *m_passwordStore = nullptr;
    HidBridge *m_hidBridge = nullptr;

    QList<QPair<QString, QUrl>> m_bookmarks;
    QList<QPair<QString, QUrl>> m_history;
    QList<QUrl> m_closedTabs;
};
