#include "browserwindow.h"
#include "adaptivetabbar.h"
#include "animatedaddressbar.h"
#include "hidbridge.h"
#include "passwordstore.h"
#include "trackerblocker.h"
#include "webview.h"

#include <QAction>
#include <QAbstractItemView>
#include <QApplication>
#include <QBoxLayout>
#include <QCloseEvent>
#include <QColor>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QGuiApplication>
#include <QIcon>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListView>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QMouseEvent>
#include <QParallelAnimationGroup>
#include <QProgressBar>
#include <QProgressDialog>
#include <QPointer>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRegularExpression>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSettings>
#include <QSet>
#include <QShortcut>
#include <QStackedWidget>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QStyleHints>
#include <QTimer>
#include <QToolButton>
#include <QVariantAnimation>
#include <QUrlQuery>
#include <QWebEngineCookieStore>
#include <QWebEngineDownloadRequest>
#include <QWebEngineFullScreenRequest>
#include <QWebEngineHistory>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineSettings>
#include <QWebChannel>
#include <QWindow>

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
#include <QWebEngineExtensionInfo>
#include <QWebEngineExtensionManager>
#endif

namespace {
constexpr int kResizeMargin = 7;
constexpr int kEdgeLeft = 1;
constexpr int kEdgeTop = 2;
constexpr int kEdgeRight = 4;
constexpr int kEdgeBottom = 8;
constexpr int kHistoryLimit = 500;

class HoverAnimator final : public QObject
{
public:
    explicit HoverAnimator(QToolButton *button)
        : QObject(button), m_effect(new QGraphicsOpacityEffect(button)),
          m_animation(new QVariantAnimation(this))
    {
        m_effect->setOpacity(0.82);
        button->setGraphicsEffect(m_effect);
        button->installEventFilter(this);
        m_animation->setDuration(125);
        m_animation->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_animation, &QVariantAnimation::valueChanged, this,
                [this](const QVariant &value) { m_effect->setOpacity(value.toReal()); });
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        Q_UNUSED(watched)
        qreal target = -1.0;
        if (event->type() == QEvent::Enter)
            target = 1.0;
        else if (event->type() == QEvent::Leave)
            target = 0.82;
        else if (event->type() == QEvent::MouseButtonPress)
            target = 0.58;
        else if (event->type() == QEvent::MouseButtonRelease)
            target = 1.0;

        if (target >= 0.0) {
            m_animation->stop();
            m_animation->setStartValue(m_effect->opacity());
            m_animation->setEndValue(target);
            m_animation->start();
        }
        return false;
    }

private:
    QGraphicsOpacityEffect *m_effect;
    QVariantAnimation *m_animation;
};

QToolButton *makeToolbarButton(const QString &text, const QString &toolTip, QWidget *parent)
{
    auto *button = new QToolButton(parent);
    button->setText(text);
    button->setToolTip(toolTip);
    button->setCursor(Qt::PointingHandCursor);
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    button->setFixedSize(36, 36);
    new HoverAnimator(button);
    return button;
}

QString normalizedHost(QString host)
{
    host = host.trimmed().toLower();
    if (host.startsWith(QStringLiteral("www.")))
        host.remove(0, 4);
    return host;
}

bool hostMatches(const QString &pageHost, const QString &savedHost)
{
    const QString page = normalizedHost(pageHost);
    const QString saved = normalizedHost(savedHost);
    return page == saved || page.endsWith(QStringLiteral(".") + saved);
}

QString featureName(QWebEnginePage::Feature feature)
{
    switch (feature) {
    case QWebEnginePage::MediaAudioCapture:
        return QStringLiteral("microphone");
    case QWebEnginePage::MediaVideoCapture:
        return QStringLiteral("camera");
    case QWebEnginePage::MediaAudioVideoCapture:
        return QStringLiteral("camera and microphone");
    case QWebEnginePage::DesktopVideoCapture:
    case QWebEnginePage::DesktopAudioVideoCapture:
        return QStringLiteral("screen sharing");
    case QWebEnginePage::Geolocation:
        return QStringLiteral("location");
    case QWebEnginePage::Notifications:
        return QStringLiteral("notifications");
    case QWebEnginePage::MouseLock:
        return QStringLiteral("pointer lock");
    default:
        return QStringLiteral("this permission");
    }
}


QVector<QStringList> parseCsv(const QString &text)
{
    QVector<QStringList> rows;
    QStringList row;
    QString field;
    bool quoted = false;

    const auto finishRow = [&] {
        row.append(field);
        field.clear();
        bool hasContent = false;
        for (const QString &cell : std::as_const(row)) {
            if (!cell.trimmed().isEmpty()) {
                hasContent = true;
                break;
            }
        }
        if (hasContent)
            rows.append(row);
        row.clear();
    };

    for (qsizetype index = 0; index < text.size(); ++index) {
        const QChar character = text.at(index);
        if (quoted) {
            if (character == QLatin1Char('"')) {
                if (index + 1 < text.size() && text.at(index + 1) == QLatin1Char('"')) {
                    field.append(QLatin1Char('"'));
                    ++index;
                } else {
                    quoted = false;
                }
            } else {
                field.append(character);
            }
            continue;
        }

        if (character == QLatin1Char('"') && field.isEmpty()) {
            quoted = true;
        } else if (character == QLatin1Char(',')) {
            row.append(field);
            field.clear();
        } else if (character == QLatin1Char('\n')) {
            finishRow();
        } else if (character == QLatin1Char('\r')) {
            if (index + 1 >= text.size() || text.at(index + 1) != QLatin1Char('\n'))
                finishRow();
        } else {
            field.append(character);
        }
    }

    if (!field.isEmpty() || !row.isEmpty())
        finishRow();
    return rows;
}

QString normalizedCsvHeader(QString value)
{
    value = value.trimmed().toLower();
    value.remove(QRegularExpression(QStringLiteral("[^a-z0-9]")));
    return value;
}

int csvColumn(const QStringList &headers, const QStringList &candidates)
{
    for (int index = 0; index < headers.size(); ++index) {
        if (candidates.contains(normalizedCsvHeader(headers.at(index))))
            return index;
    }
    return -1;
}

QString hostFromCsvValue(QString value)
{
    value = value.trimmed();
    if (value.isEmpty())
        return {};

    QUrl url = QUrl::fromUserInput(value);
    QString host = url.host();
    if (host.isEmpty()) {
        value.remove(QRegularExpression(QStringLiteral("^[a-zA-Z][a-zA-Z0-9+.-]*://")));
        value = value.section(QLatin1Char('/'), 0, 0);
        value = value.section(QLatin1Char('@'), -1);
        value = value.section(QLatin1Char(':'), 0, 0);
        host = value;
    }
    return normalizedHost(host);
}

QStringList extensionEnabledIds()
{
    return QSettings().value(QStringLiteral("extensions/enabledIds")).toStringList();
}

void setExtensionEnabledId(const QString &id, bool enabled)
{
    QStringList ids = extensionEnabledIds();
    if (enabled && !ids.contains(id))
        ids.append(id);
    else if (!enabled)
        ids.removeAll(id);
    QSettings().setValue(QStringLiteral("extensions/enabledIds"), ids);
}
}

BrowserWindow::BrowserWindow(bool privateMode, QWidget *parent)
    : QMainWindow(parent), m_privateMode(privateMode)
{
    setWindowTitle(privateMode ? QStringLiteral("Private — Cachy Surf")
                               : QStringLiteral("Cachy Surf"));
    setWindowIcon(QIcon(QStringLiteral(":/assets/cachysurf.svg")));
    setMinimumSize(780, 560);
    resize(1260, 820);
    setMouseTracking(true);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    QSettings settings;
    // v0.3.2 intentionally starts dark, even when an older prototype saved
    // a light preference. The menu can still change it after launch.
    m_darkUi = true;
    m_darkWebsites = true;
    if (!m_privateMode) {
        settings.setValue(QStringLiteral("appearance/darkMode"), true);
        settings.setValue(QStringLiteral("privacy/forceDarkWebsites"), true);
    }
    m_passwordAutofill = settings.value(QStringLiteral("passwords/autofill"), true).toBool();

    if (m_privateMode) {
        m_profile = new QWebEngineProfile(this);
    } else {
        m_profile = new QWebEngineProfile(QStringLiteral("CachySurf"), this);
        const QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        m_profile->setPersistentStoragePath(dataPath + QStringLiteral("/profile"));
        m_profile->setCachePath(dataPath + QStringLiteral("/cache"));
    }

    configureProfile();

    m_trackerBlocker = new TrackerBlocker(m_profile);
    m_trackerBlocker->setEnabled(settings.value(QStringLiteral("privacy/trackerBlocking"), true).toBool());
    m_profile->setUrlRequestInterceptor(m_trackerBlocker);
    m_passwordStore = new PasswordStore(this);
    m_hidBridge = new HidBridge(this, this);

    connect(m_profile, &QWebEngineProfile::downloadRequested,
            this, &BrowserWindow::handleDownload);

    buildUi();
    buildMenu();
    loadBookmarks();
    loadHistory();
    refreshSidebar();
    applyStyle();
    QTimer::singleShot(0, this, &BrowserWindow::restoreExtensionStates);

    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
            this, [this](Qt::ColorScheme) {
        QTimer::singleShot(0, this, [this] { applyStyle(); });
    });

    createTab(QUrl(QStringLiteral("qrc:/assets/newtab.html")), true);

    new QShortcut(QKeySequence::AddTab, this, [this] { createTab(); });
    new QShortcut(QKeySequence::Close, this, [this] { closeTab(m_tabBar->currentIndex()); });
    new QShortcut(QKeySequence(QStringLiteral("Ctrl+L")), this, [this] {
        m_addressBar->setFocus();
        m_addressBar->selectAll();
    });
    new QShortcut(QKeySequence::Refresh, this, [this] {
        if (currentView()) currentView()->reload();
    });
    new QShortcut(QKeySequence(QStringLiteral("Alt+Left")), this, [this] {
        if (currentView()) currentView()->back();
    });
    new QShortcut(QKeySequence(QStringLiteral("Alt+Right")), this, [this] {
        if (currentView()) currentView()->forward();
    });
    new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+N")), this, [this] {
        openPrivateWindow();
    });
    new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+P")), this, [this] {
        showPasswordManager();
    });
    new QShortcut(QKeySequence(QStringLiteral("Ctrl+H")), this, [this] {
        showHistory();
    });
    new QShortcut(QKeySequence(QStringLiteral("Ctrl+F")), this, [this] {
        findOnPage();
    });
    new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+T")), this, [this] {
        reopenClosedTab();
    });
    new QShortcut(QKeySequence::FullScreen, this, [this] {
        toggleFullScreen();
    });
}

void BrowserWindow::configureProfile()
{
    m_profile->setHttpAcceptLanguage(QStringLiteral("en-GB,en;q=0.9"));
    m_profile->setSpellCheckEnabled(true);

    QString userAgent = m_profile->httpUserAgent();
    userAgent.remove(QRegularExpression(QStringLiteral(R"(\s+QtWebEngine/[^\s]+)")));
    if (!userAgent.isEmpty())
        m_profile->setHttpUserAgent(userAgent);

    if (m_privateMode) {
        m_profile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
        m_profile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
    } else {
        m_profile->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
        m_profile->setHttpCacheMaximumSize(512 * 1024 * 1024);
        m_profile->setPersistentCookiesPolicy(QWebEngineProfile::AllowPersistentCookies);
    }
}

void BrowserWindow::buildUi()
{
    auto *outer = new QWidget(this);
    auto *outerLayout = new QVBoxLayout(outer);
    outerLayout->setContentsMargins(8, 8, 8, 8);
    outerLayout->setSpacing(0);
    setCentralWidget(outer);

    m_root = new QFrame(outer);
    m_root->setObjectName(QStringLiteral("root"));
    outerLayout->addWidget(m_root);

    auto *rootLayout = new QVBoxLayout(m_root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_topBar = new QFrame(m_root);
    m_topBar->setObjectName(QStringLiteral("toolbar"));
    m_topBar->setFixedHeight(74);
    m_topBar->setMouseTracking(true);
    rootLayout->addWidget(m_topBar);

    auto *toolbarLayout = new QHBoxLayout(m_topBar);
    toolbarLayout->setContentsMargins(14, 12, 14, 12);
    toolbarLayout->setSpacing(10);

    auto *leftControls = new QFrame(m_topBar);
    leftControls->setObjectName(QStringLiteral("toolbarControls"));
    leftControls->setFixedWidth(164);
    auto *leftLayout = new QHBoxLayout(leftControls);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(5);

    m_sidebarButton = makeToolbarButton(QStringLiteral("▥"), QStringLiteral("Show sidebar"), leftControls);
    m_backButton = makeToolbarButton(QStringLiteral("‹"), QStringLiteral("Back"), leftControls);
    m_forwardButton = makeToolbarButton(QStringLiteral("›"), QStringLiteral("Forward"), leftControls);
    m_reloadButton = makeToolbarButton(QStringLiteral("↻"), QStringLiteral("Reload"), leftControls);
    connect(m_sidebarButton, &QToolButton::clicked, this, &BrowserWindow::toggleSidebar);
    connect(m_backButton, &QToolButton::clicked, this, [this] { if (currentView()) currentView()->back(); });
    connect(m_forwardButton, &QToolButton::clicked, this, [this] { if (currentView()) currentView()->forward(); });
    connect(m_reloadButton, &QToolButton::clicked, this, [this] { if (currentView()) currentView()->reload(); });
    leftLayout->addWidget(m_sidebarButton);
    leftLayout->addWidget(m_backButton);
    leftLayout->addWidget(m_forwardButton);
    leftLayout->addWidget(m_reloadButton);
    leftLayout->addStretch(1);
    toolbarLayout->addWidget(leftControls);

    m_addressBar = new AnimatedAddressBar(m_topBar);
    m_addressBar->setObjectName(QStringLiteral("addressBar"));
    m_addressBar->setPlaceholderText(QStringLiteral("Search or enter website name"));
    m_addressBar->setClearButtonEnabled(true);
    m_addressBar->setMinimumWidth(420);
    m_addressBar->setMaximumWidth(QWIDGETSIZE_MAX);
    m_addressBar->setMinimumHeight(46);
    m_addressBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_addressBar->setAlignment(Qt::AlignCenter);
    connect(m_addressBar, &AnimatedAddressBar::submitRequested,
            this, &BrowserWindow::navigateFromAddressBar);
    connect(m_addressBar, &AnimatedAddressBar::dismissSuggestionsRequested,
            this, &BrowserWindow::dismissAddressSuggestions);
    connect(m_addressBar, &AnimatedAddressBar::suggestionStepRequested,
            this, &BrowserWindow::selectAddressSuggestion);
    connect(m_addressBar, &QLineEdit::textEdited,
            this, &BrowserWindow::updateAddressSuggestions);
    connect(m_addressBar, &AnimatedAddressBar::focusStateChanged,
            this, &BrowserWindow::animateAddressFocus);
    toolbarLayout->addWidget(m_addressBar, 1);

    auto *rightControls = new QFrame(m_topBar);
    rightControls->setObjectName(QStringLiteral("toolbarControls"));
    rightControls->setFixedWidth(96);
    auto *rightLayout = new QHBoxLayout(rightControls);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(5);
    rightLayout->addStretch(1);

    auto *bookmarkButton = makeToolbarButton(QStringLiteral("☆"), QStringLiteral("Add bookmark"), rightControls);
    connect(bookmarkButton, &QToolButton::clicked, this, &BrowserWindow::addCurrentBookmark);
    rightLayout->addWidget(bookmarkButton);

    m_menuButton = makeToolbarButton(QStringLiteral("•••"), QStringLiteral("Browser menu"), rightControls);
    connect(m_menuButton, &QToolButton::clicked, this, &BrowserWindow::showMainMenu);
    rightLayout->addWidget(m_menuButton);
    toolbarLayout->addWidget(rightControls);

    m_suggestionModel = new QStandardItemModel(this);
    m_suggestionView = new QListView(m_root);
    m_suggestionView->setObjectName(QStringLiteral("addressSuggestions"));
    m_suggestionView->setModel(m_suggestionModel);
    m_suggestionView->setUniformItemSizes(true);
    m_suggestionView->setMouseTracking(true);
    m_suggestionView->setFocusPolicy(Qt::NoFocus);
    m_suggestionView->setFrameShape(QFrame::NoFrame);
    m_suggestionView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_suggestionView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_suggestionView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_suggestionView->hide();
    connect(m_suggestionView, &QListView::clicked,
            this, &BrowserWindow::activateSuggestion);

    m_network = new QNetworkAccessManager(this);
    m_suggestionTimer = new QTimer(this);
    m_suggestionTimer->setSingleShot(true);
    m_suggestionTimer->setInterval(170);
    connect(m_suggestionTimer, &QTimer::timeout,
            this, &BrowserWindow::requestRemoteSuggestions);

    m_progress = new QProgressBar(m_root);
    m_progress->setObjectName(QStringLiteral("progress"));
    m_progress->setRange(0, 100);
    m_progress->setFixedHeight(3);
    m_progress->setTextVisible(false);
    m_progress->hide();
    m_progressAnimation = new QPropertyAnimation(m_progress, "value", this);
    m_progressAnimation->setDuration(140);
    m_progressAnimation->setEasingCurve(QEasingCurve::OutCubic);
    rootLayout->addWidget(m_progress);

    m_tabStrip = new QFrame(m_root);
    m_tabStrip->setObjectName(QStringLiteral("tabStrip"));
    m_tabStrip->setFixedHeight(52);
    rootLayout->addWidget(m_tabStrip);

    auto *tabLayout = new QHBoxLayout(m_tabStrip);
    tabLayout->setContentsMargins(12, 7, 10, 7);
    tabLayout->setSpacing(8);

    m_tabBar = new AdaptiveTabBar(m_tabStrip);
    m_tabBar->setObjectName(QStringLiteral("tabBar"));
    m_tabBar->setMovable(true);
    m_tabBar->setTabsClosable(false);
    m_tabBar->setDocumentMode(true);
    m_tabBar->setSelectionBehaviorOnRemove(QTabBar::SelectPreviousTab);
    m_tabBar->setMinimumWidth(220);
    connect(m_tabBar, &QTabBar::currentChanged, this, &BrowserWindow::activateTab);
    connect(m_tabBar, &QTabBar::tabCloseRequested, this, &BrowserWindow::closeTab);
    tabLayout->addWidget(m_tabBar, 1);

    m_newTabButton = makeToolbarButton(QStringLiteral("+"), QStringLiteral("New tab"), m_tabStrip);
    m_newTabButton->setObjectName(QStringLiteral("newTabButton"));
    connect(m_newTabButton, &QToolButton::clicked, this, [this] { createTab(); });
    tabLayout->addWidget(m_newTabButton);

    auto *content = new QFrame(m_root);
    content->setObjectName(QStringLiteral("content"));
    auto *contentLayout = new QHBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    m_sidebar = new QListWidget(content);
    m_sidebar->setObjectName(QStringLiteral("sidebar"));
    m_sidebar->setMinimumWidth(0);
    m_sidebar->setMaximumWidth(0);
    m_sidebar->setFrameShape(QFrame::NoFrame);
    m_sidebar->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_sidebar->hide();
    connect(m_sidebar, &QListWidget::itemActivated, this, &BrowserWindow::openSidebarItem);
    contentLayout->addWidget(m_sidebar);

    m_stack = new QStackedWidget(content);
    m_stack->setObjectName(QStringLiteral("stack"));
    contentLayout->addWidget(m_stack, 1);
    rootLayout->addWidget(content, 1);
}

void BrowserWindow::buildMenu()
{
    m_mainMenu = new QMenu(this);
    m_mainMenu->setObjectName(QStringLiteral("mainMenu"));

    auto *newTab = m_mainMenu->addAction(QStringLiteral("New Tab"));
    newTab->setShortcut(QKeySequence::AddTab);
    connect(newTab, &QAction::triggered, this, [this] { createTab(); });

    auto *privateWindow = m_mainMenu->addAction(QStringLiteral("New Private Window"));
    privateWindow->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+N")));
    connect(privateWindow, &QAction::triggered, this, &BrowserWindow::openPrivateWindow);

    m_mainMenu->addSeparator();

    auto *sidebarAction = m_mainMenu->addAction(QStringLiteral("Show Sidebar"));
    sidebarAction->setCheckable(true);
    sidebarAction->setChecked(m_sidebarVisible);
    connect(sidebarAction, &QAction::triggered, this, &BrowserWindow::toggleSidebar);

    auto *bookmarkAction = m_mainMenu->addAction(QStringLiteral("Add Bookmark"));
    connect(bookmarkAction, &QAction::triggered, this, &BrowserWindow::addCurrentBookmark);

    auto *historyAction = m_mainMenu->addAction(QStringLiteral("History"));
    historyAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+H")));
    connect(historyAction, &QAction::triggered, this, &BrowserWindow::showHistory);

    auto *reopenTab = m_mainMenu->addAction(QStringLiteral("Reopen Closed Tab"));
    reopenTab->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+T")));
    connect(reopenTab, &QAction::triggered, this, &BrowserWindow::reopenClosedTab);

    auto *findAction = m_mainMenu->addAction(QStringLiteral("Find on Page…"));
    findAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+F")));
    connect(findAction, &QAction::triggered, this, &BrowserWindow::findOnPage);

    auto *printAction = m_mainMenu->addAction(QStringLiteral("Save Page as PDF…"));
    connect(printAction, &QAction::triggered, this, &BrowserWindow::printCurrentPage);

    auto *downloads = m_mainMenu->addAction(QStringLiteral("Open Downloads Folder"));
    connect(downloads, &QAction::triggered, this, [] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(
            QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)));
    });

    m_mainMenu->addSeparator();

    auto *passwords = m_mainMenu->addAction(QStringLiteral("Passwords…"));
    passwords->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+P")));
    passwords->setEnabled(!m_privateMode);
    connect(passwords, &QAction::triggered, this, &BrowserWindow::showPasswordManager);

    auto *savePassword = m_mainMenu->addAction(QStringLiteral("Save Login for This Site…"));
    savePassword->setEnabled(!m_privateMode);
    connect(savePassword, &QAction::triggered, this, &BrowserWindow::addPasswordForCurrentSite);

    auto *fillPassword = m_mainMenu->addAction(QStringLiteral("Fill Saved Login"));
    fillPassword->setEnabled(!m_privateMode);
    connect(fillPassword, &QAction::triggered, this, &BrowserWindow::fillPasswordForCurrentSite);

    auto *importPasswords = m_mainMenu->addAction(QStringLiteral("Import Passwords from CSV…"));
    importPasswords->setEnabled(!m_privateMode);
    connect(importPasswords, &QAction::triggered, this, &BrowserWindow::importPasswordsFromCsv);

    m_passwordAutofillAction = m_mainMenu->addAction(QStringLiteral("Autofill Passwords"));
    m_passwordAutofillAction->setCheckable(true);
    m_passwordAutofillAction->setChecked(m_passwordAutofill);
    m_passwordAutofillAction->setEnabled(!m_privateMode);
    connect(m_passwordAutofillAction, &QAction::toggled,
            this, &BrowserWindow::togglePasswordAutofill);

    m_mainMenu->addSeparator();

    auto *extensions = m_mainMenu->addAction(QStringLiteral("Extensions…"));
    extensions->setEnabled(!m_privateMode);
    connect(extensions, &QAction::triggered, this, &BrowserWindow::showExtensionManager);

    auto *devices = m_mainMenu->addAction(QStringLiteral("Connected Devices…"));
    connect(devices, &QAction::triggered, this, &BrowserWindow::showDeviceManager);

    m_mainMenu->addSeparator();

    m_darkWebsitesAction = m_mainMenu->addAction(QStringLiteral("Dark Mode"));
    m_darkWebsitesAction->setCheckable(true);
    m_darkWebsitesAction->setChecked(m_darkUi && m_darkWebsites);
    connect(m_darkWebsitesAction, &QAction::toggled,
            this, &BrowserWindow::toggleDarkWebsites);

    m_trackerBlockingAction = m_mainMenu->addAction(QStringLiteral("Basic Tracker Blocking"));
    m_trackerBlockingAction->setCheckable(true);
    m_trackerBlockingAction->setChecked(m_trackerBlocker->isEnabled());
    connect(m_trackerBlockingAction, &QAction::toggled,
            this, &BrowserWindow::toggleTrackerBlocking);

    auto *zoomIn = m_mainMenu->addAction(QStringLiteral("Zoom In"));
    zoomIn->setShortcut(QKeySequence::ZoomIn);
    connect(zoomIn, &QAction::triggered, this, [this] {
        if (currentView()) currentView()->setZoomFactor(qMin(5.0, currentView()->zoomFactor() + 0.1));
    });

    auto *zoomOut = m_mainMenu->addAction(QStringLiteral("Zoom Out"));
    zoomOut->setShortcut(QKeySequence::ZoomOut);
    connect(zoomOut, &QAction::triggered, this, [this] {
        if (currentView()) currentView()->setZoomFactor(qMax(0.25, currentView()->zoomFactor() - 0.1));
    });

    auto *actualSize = m_mainMenu->addAction(QStringLiteral("Actual Size"));
    actualSize->setShortcut(QKeySequence(QStringLiteral("Ctrl+0")));
    connect(actualSize, &QAction::triggered, this, [this] {
        if (currentView()) currentView()->setZoomFactor(1.0);
    });

    auto *fullscreen = m_mainMenu->addAction(QStringLiteral("Enter Full Screen"));
    fullscreen->setShortcut(QKeySequence::FullScreen);
    connect(fullscreen, &QAction::triggered, this, &BrowserWindow::toggleFullScreen);

    auto *clearData = m_mainMenu->addAction(QStringLiteral("Clear Browsing Data…"));
    connect(clearData, &QAction::triggered, this, &BrowserWindow::clearBrowsingData);

    m_mainMenu->addSeparator();

    auto *about = m_mainMenu->addAction(QStringLiteral("About Cachy Surf"));
    connect(about, &QAction::triggered, this, [this] {
        QMessageBox::about(this, QStringLiteral("About Cachy Surf"),
            QStringLiteral("<b>Cachy Surf %1</b><br><br>"
                           "An unofficial CachyOS-styled browser built with Qt WebEngine."
                           "<br><br>Passwords are stored through your Linux system keyring."
                           "<br><br>This project is not affiliated with Apple or the CachyOS team.")
                .arg(QStringLiteral(CACHYSURF_VERSION)));
    });
}

void BrowserWindow::applyStyle()
{
    if (m_applyingStyle)
        return;
    QScopedValueRollback<bool> styleGuard(m_applyingStyle, true);

    const bool dark = m_darkUi;
    const QString rootBg = dark ? QStringLiteral("#17181b") : QStringLiteral("#f3f3f5");
    const QString chromeBg = dark ? QStringLiteral("#232429") : QStringLiteral("#e8e8eb");
    const QString fieldBg = dark ? QStringLiteral("#303238") : QStringLiteral("#ffffff");
    const QString text = dark ? QStringLiteral("#f5f5f7") : QStringLiteral("#18181a");
    const QString muted = dark ? QStringLiteral("#b6b8bf") : QStringLiteral("#5b5c60");
    const QString border = dark ? QStringLiteral("#3b3d44") : QStringLiteral("#d0d0d5");
    const QString tab = dark ? QStringLiteral("#393b42") : QStringLiteral("#ffffff");
    const QString popup = dark ? QStringLiteral("#292b31") : QStringLiteral("#ffffff");

    if (m_tabBar) {
        m_tabBar->setProperty("indicatorColor", tab);
        m_tabBar->setProperty("hoverColor", dark ? QStringLiteral("#a8abb4")
                                                  : QStringLiteral("#62646b"));
        m_tabBar->update();
    }

    setStyleSheet(QStringLiteral(R"(
        QMainWindow { background: transparent; }
        QFrame#root {
            background: %1;
            border: 1px solid %6;
            border-radius: 16px;
        }
        QFrame#toolbar {
            background: %2;
            border-top-left-radius: 15px;
            border-top-right-radius: 15px;
        }
        QFrame#toolbarControls { background: transparent; }
        QFrame#tabStrip {
            background: %2;
            border-top: 1px solid rgba(128, 128, 128, 0.10);
            border-bottom: 1px solid %6;
        }
        QFrame#content {
            background: %1;
            border-bottom-left-radius: 15px;
            border-bottom-right-radius: 15px;
        }
        QToolButton {
            color: %4;
            background: transparent;
            border: none;
            border-radius: 10px;
            font-size: 19px;
        }
        QToolButton:hover { background: rgba(128, 128, 128, 0.16); }
        QToolButton:pressed { background: rgba(128, 128, 128, 0.27); }
        QToolButton:disabled { color: rgba(128, 128, 128, 0.42); }
        QToolButton#newTabButton { font-size: 24px; font-weight: 400; }
        QLineEdit#addressBar {
            background: %3;
            color: %4;
            border: 1px solid %6;
            border-radius: 17px;
            padding: 10px 44px;
            font-size: 15px;
            font-weight: 500;
            selection-background-color: #4d89ff;
        }
        QLineEdit#addressBar:hover {
            border-color: rgba(128, 128, 128, 0.62);
        }
        QLineEdit#addressBar:focus {
            border: 2px solid #4d89ff;
            padding: 9px 43px;
            background: %8;
        }
        QTabBar#tabBar { background: transparent; }
        QTabBar#tabBar::tab {
            background: transparent;
            color: %5;
            height: 38px;
            border: none;
            margin: 0 1px;
            padding: 0 36px;
        }
        QTabBar#tabBar::tab:selected {
            background: transparent;
            color: %4;
        }
        QTabBar#tabBar::tab:hover:!selected { background: transparent; }
        QTabBar#tabBar::close-button {
            subcontrol-position: right;
            margin-right: 9px;
        }
        QListView#addressSuggestions {
            background: %8;
            color: %4;
            border: 1px solid %6;
            border-radius: 13px;
            padding: 6px;
            outline: none;
            selection-background-color: rgba(77, 137, 255, 0.28);
        }
        QListView#addressSuggestions::item {
            min-height: 34px;
            padding: 5px 11px;
            border-radius: 8px;
        }
        QListView#addressSuggestions::item:hover {
            background: rgba(128, 128, 128, 0.12);
        }
        QListWidget#sidebar {
            background: %2;
            color: %4;
            border-right: 1px solid %6;
            padding: 10px;
            outline: none;
        }
        QListWidget#sidebar::item { padding: 9px 10px; border-radius: 8px; }
        QListWidget#sidebar::item:hover { background: rgba(128, 128, 128, 0.14); }
        QListWidget#sidebar::item:selected { background: rgba(77, 137, 255, 0.28); }
        QMenu#mainMenu {
            background: %8;
            color: %4;
            border: 1px solid %6;
            border-radius: 11px;
            padding: 7px;
        }
        QMenu#mainMenu::item { padding: 8px 34px 8px 12px; border-radius: 7px; }
        QMenu#mainMenu::item:selected { background: rgba(77, 137, 255, 0.28); }
        QProgressBar#progress { border: none; background: transparent; }
        QProgressBar#progress::chunk { background: #4d89ff; border-radius: 1px; }
    )").arg(rootBg, chromeBg, fieldBg, text, muted, border, tab, popup));
}

WebView *BrowserWindow::createTab(const QUrl &url, bool activate)
{
    auto *view = new WebView(this, m_stack);
    auto *page = new QWebEnginePage(m_profile, view);
    view->setPage(page);
    configurePage(view);
    view->setContextMenuPolicy(Qt::DefaultContextMenu);

    m_stack->addWidget(view);
    const int tabIndex = m_tabBar->addTab(QStringLiteral("Start Page"));
    m_tabBar->setTabData(tabIndex, QVariant::fromValue<void *>(view));

    connectView(view);

    m_tabBar->refreshAnimatedLayout();

    if (activate) {
        m_tabBar->setCurrentIndex(tabIndex);
        setCurrentView(view);
    }

    const QUrl target = url.isEmpty() ? QUrl(QStringLiteral("qrc:/assets/newtab.html")) : url;
    view->load(target);
    return view;
}

void BrowserWindow::configurePage(WebView *view)
{
    view->page()->setBackgroundColor(QColor(QStringLiteral("#111214")));
    auto *settings = view->page()->settings();
    settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    settings->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, true);
    settings->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, true);
    settings->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
    settings->setAttribute(QWebEngineSettings::WebGLEnabled, true);
    settings->setAttribute(QWebEngineSettings::Accelerated2dCanvasEnabled, true);
    settings->setAttribute(QWebEngineSettings::ScrollAnimatorEnabled, true);
    settings->setAttribute(QWebEngineSettings::DnsPrefetchEnabled, true);
    settings->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);
    settings->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, false);
    settings->setAttribute(QWebEngineSettings::PdfViewerEnabled, true);
    settings->setAttribute(QWebEngineSettings::ErrorPageEnabled, true);
    settings->setAttribute(QWebEngineSettings::AllowWindowActivationFromJavaScript, true);
    settings->setAttribute(QWebEngineSettings::ForceDarkMode, m_darkWebsites);
    configureWebHid(view);
}

void BrowserWindow::configureWebHid(WebView *view)
{
    if (!view || !view->page() || !m_hidBridge)
        return;

    auto *channel = new QWebChannel(view->page());
    channel->registerObject(QStringLiteral("cachyHid"), m_hidBridge->createPageBridge(view->page()));
    view->page()->setWebChannel(channel, QWebEngineScript::MainWorld);

    QFile webChannelSource(QStringLiteral(":/qtwebchannel/qwebchannel.js"));
    QFile polyfillSource(QStringLiteral(":/assets/webhid-polyfill.js"));
    if (!webChannelSource.open(QIODevice::ReadOnly | QIODevice::Text)
        || !polyfillSource.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    QWebEngineScript script;
    script.setName(QStringLiteral("CachySurfWebHID"));
    script.setInjectionPoint(QWebEngineScript::DocumentCreation);
    script.setWorldId(QWebEngineScript::MainWorld);
    script.setRunsOnSubFrames(false);
    script.setSourceCode(QString::fromUtf8(webChannelSource.readAll())
                         + QLatin1Char('\n')
                         + QString::fromUtf8(polyfillSource.readAll()));
    view->page()->scripts().insert(script);
}

void BrowserWindow::restoreExtensionStates()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    if (m_privateMode || !m_profile)
        return;

    QWebEngineExtensionManager *manager = m_profile->extensionManager();
    if (!manager)
        return;

    const auto enableRemembered = [manager](const QWebEngineExtensionInfo &extension) {
        if (extension.isLoaded() && extension.isInstalled()
            && extensionEnabledIds().contains(extension.id())) {
            manager->setExtensionEnabled(extension, true);
        }
    };

    connect(manager, &QWebEngineExtensionManager::loadFinished, this,
            [enableRemembered](const QWebEngineExtensionInfo &extension) {
        enableRemembered(extension);
    });
    connect(manager, &QWebEngineExtensionManager::installFinished, this,
            [this, manager](const QWebEngineExtensionInfo &extension) {
        if (!extension.error().isEmpty())
            return;
        if (extension.isLoaded()) {
            manager->setExtensionEnabled(extension, true);
            setExtensionEnabledId(extension.id(), true);
        }
    });
    connect(manager, &QWebEngineExtensionManager::uninstallFinished, this,
            [](const QWebEngineExtensionInfo &extension) {
        setExtensionEnabledId(extension.id(), false);
    });

    for (const QWebEngineExtensionInfo &extension : manager->extensions())
        enableRemembered(extension);
    QTimer::singleShot(350, this, [manager, enableRemembered] {
        for (const QWebEngineExtensionInfo &extension : manager->extensions())
            enableRemembered(extension);
    });
#endif
}

void BrowserWindow::connectView(WebView *view)
{
    connect(view, &QWebEngineView::titleChanged, this, [this, view](const QString &title) {
        const int index = tabIndexForView(view);
        if (index >= 0)
            m_tabBar->setTabText(index, title.isEmpty() ? QStringLiteral("New Tab") : title);
        if (view == currentView())
            setWindowTitle((m_privateMode ? QStringLiteral("Private — ") : QString()) +
                           (title.isEmpty() ? QStringLiteral("Cachy Surf") : title));
    });

    connect(view, &QWebEngineView::iconChanged, this, [this, view](const QIcon &icon) {
        const int index = tabIndexForView(view);
        if (index >= 0 && !icon.isNull())
            m_tabBar->setTabIcon(index, icon);
    });

    connect(view, &QWebEngineView::urlChanged, this, [this, view](const QUrl &) {
        if (view == currentView()) {
            if (!m_addressBar->hasFocus())
                dismissAddressSuggestions();
            updateFromCurrentView();
        }
    });

    connect(view, &QWebEngineView::loadProgress, this, [this, view](int progress) {
        if (view != currentView())
            return;
        animateProgressTo(progress);
        m_progress->setVisible(progress > 0 && progress < 100);
    });

    connect(view, &QWebEngineView::loadFinished, this, [this, view](bool ok) {
        if (view == currentView()) {
            m_progress->hide();
            updateNavigationState();
        }
        if (ok) {
            applyWebDarkMode(view);
            recordHistory(view);
            if (view == currentView())
                QTimer::singleShot(250, this, &BrowserWindow::autofillCurrentSite);
        }
    });

    connect(view->page(), &QWebEnginePage::fullScreenRequested, this,
            [this](QWebEngineFullScreenRequest request) {
        request.accept();
        request.toggleOn() ? showFullScreen() : showNormal();
    });

    connect(view->page(), &QWebEnginePage::featurePermissionRequested, this,
            [this, page = view->page()](const QUrl &origin, QWebEnginePage::Feature feature) {
        const auto result = QMessageBox::question(
            this,
            QStringLiteral("Website permission"),
            QStringLiteral("Allow %1 to use %2?")
                .arg(origin.host(), featureName(feature)),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        page->setFeaturePermission(
            origin,
            feature,
            result == QMessageBox::Yes
                ? QWebEnginePage::PermissionGrantedByUser
                : QWebEnginePage::PermissionDeniedByUser);
    });
}

void BrowserWindow::animateProgressTo(int value)
{
    m_progressAnimation->stop();
    m_progressAnimation->setStartValue(m_progress->value());
    m_progressAnimation->setEndValue(value);
    m_progressAnimation->start();
}

void BrowserWindow::closeTab(int index)
{
    if (index < 0 || index >= m_tabBar->count())
        return;

    auto *view = static_cast<WebView *>(m_tabBar->tabData(index).value<void *>());
    if (view && view->url().isValid() && view->url().scheme() != QStringLiteral("qrc")) {
        m_closedTabs.prepend(view->url());
        while (m_closedTabs.size() > 20)
            m_closedTabs.removeLast();
    }
    m_tabBar->removeTab(index);
    removeView(view);

    if (m_tabBar->count() == 0)
        close();
}

void BrowserWindow::removeView(WebView *view)
{
    if (!view)
        return;
    m_stack->removeWidget(view);
    view->deleteLater();
}

void BrowserWindow::activateTab(int index)
{
    if (index < 0)
        return;
    auto *view = static_cast<WebView *>(m_tabBar->tabData(index).value<void *>());
    setCurrentView(view);
}

void BrowserWindow::setCurrentView(WebView *view)
{
    if (!view)
        return;
    m_stack->setCurrentWidget(view);
    updateFromCurrentView();
}

WebView *BrowserWindow::currentView() const
{
    return qobject_cast<WebView *>(m_stack->currentWidget());
}

void BrowserWindow::navigateFromAddressBar()
{
    if (m_addressCommitPending || !currentView())
        return;

    if (m_suggestionView && m_suggestionView->isVisible() &&
        m_selectedSuggestionRow >= 0 &&
        m_selectedSuggestionRow < m_suggestionModel->rowCount()) {
        activateSuggestion(m_suggestionModel->index(m_selectedSuggestionRow, 0));
        return;
    }

    m_addressCommitPending = true;
    const QString input = m_addressBar->text();
    dismissAddressSuggestions();
    currentView()->load(normalizedUrl(input));
    currentView()->setFocus(Qt::OtherFocusReason);
    QTimer::singleShot(0, this, [this] { m_addressCommitPending = false; });
}

void BrowserWindow::animateAddressFocus(bool focused)
{
    // The Smart Search field always owns all free toolbar width. Focus only
    // changes its height and text alignment, matching Safari's separate-tab UI.
    m_addressBar->setMaximumWidth(QWIDGETSIZE_MAX);

    if (!m_addressHeightAnimation) {
        m_addressHeightAnimation = new QPropertyAnimation(m_addressBar, "minimumSize", this);
        m_addressHeightAnimation->setDuration(180);
        m_addressHeightAnimation->setEasingCurve(QEasingCurve::OutCubic);
    }

    m_addressHeightAnimation->stop();
    m_addressHeightAnimation->setStartValue(m_addressBar->minimumSize());
    m_addressHeightAnimation->setEndValue(QSize(420, focused ? 50 : 46));
    m_addressHeightAnimation->start();

    m_addressBar->setAlignment(focused ? Qt::AlignLeft : Qt::AlignCenter);

    if (focused) {
        if (!m_addressBar->text().trimmed().isEmpty())
            updateAddressSuggestions(m_addressBar->text());
    } else {
        QTimer::singleShot(90, this, [this] {
            if (!m_addressBar->hasFocus())
                dismissAddressSuggestions();
        });
    }
}

void BrowserWindow::updateAddressSuggestions(const QString &text)
{
    const QString query = text.trimmed();
    m_pendingSuggestionQuery = query;
    m_selectedSuggestionRow = -1;

    if (m_suggestionReply) {
        disconnect(m_suggestionReply, nullptr, this, nullptr);
        m_suggestionReply->abort();
        m_suggestionReply->deleteLater();
        m_suggestionReply = nullptr;
    }

    // Explicit URLs should never be covered by a suggestion popup. This keeps
    // Ctrl+A, editing and Enter completely predictable.
    const bool explicitUrl = query.contains(QStringLiteral("://")) ||
                             query.startsWith(QStringLiteral("about:"), Qt::CaseInsensitive) ||
                             query.startsWith(QStringLiteral("qrc:"), Qt::CaseInsensitive);
    if (query.size() < 2 || explicitUrl) {
        if (m_suggestionTimer)
            m_suggestionTimer->stop();
        if (m_suggestionModel)
            m_suggestionModel->clear();
        if (m_suggestionView)
            m_suggestionView->hide();
        return;
    }

    rebuildSuggestionModel(query);
    m_suggestionTimer->start();
}

void BrowserWindow::rebuildSuggestionModel(const QString &query,
                                           const QStringList &remoteSuggestions)
{
    if (!m_suggestionModel || !m_suggestionView)
        return;

    m_suggestionModel->clear();
    m_selectedSuggestionRow = -1;
    QSet<QString> seen;

    auto addSuggestion = [this, &seen](const QString &label,
                                       const QString &value,
                                       const QString &kind) {
        const QString key = kind + QLatin1Char(':') + value.trimmed().toLower();
        if (value.trimmed().isEmpty() || seen.contains(key) || m_suggestionModel->rowCount() >= 8)
            return;
        seen.insert(key);
        auto *item = new QStandardItem(label);
        item->setEditable(false);
        item->setData(value, Qt::UserRole + 1);
        item->setData(kind, Qt::UserRole + 2);
        m_suggestionModel->appendRow(item);
    };

    const QString lowered = query.toLower();
    const bool looksLikeAddress = query.contains(QLatin1Char('.')) ||
                                  query.startsWith(QStringLiteral("localhost"), Qt::CaseInsensitive);

    if (looksLikeAddress) {
        const QUrl url = QUrl::fromUserInput(query);
        addSuggestion(QStringLiteral("Go to %1").arg(url.toDisplayString()),
                      url.toString(), QStringLiteral("url"));
    }

    int localMatches = 0;
    auto addLocalMatches = [&](const QList<QPair<QString, QUrl>> &entries) {
        for (const auto &entry : entries) {
            if (localMatches >= 4)
                break;
            const QString title = entry.first.isEmpty() ? entry.second.host() : entry.first;
            const QString address = entry.second.toDisplayString();
            if (!title.toLower().contains(lowered) && !address.toLower().contains(lowered))
                continue;
            addSuggestion(QStringLiteral("%1   —   %2").arg(title, address),
                          entry.second.toString(), QStringLiteral("url"));
            ++localMatches;
        }
    };

    addLocalMatches(m_bookmarks);
    addLocalMatches(m_history);

    addSuggestion(QStringLiteral("Search Google for “%1”").arg(query),
                  query, QStringLiteral("search"));

    for (const QString &suggestion : remoteSuggestions) {
        addSuggestion(QStringLiteral("Search Google for %1").arg(suggestion),
                      suggestion, QStringLiteral("search"));
    }

    if (m_suggestionModel->rowCount() == 0) {
        m_suggestionView->hide();
        return;
    }

    m_suggestionView->clearSelection();
    m_suggestionView->setCurrentIndex(QModelIndex());
    positionSuggestionPopup();
    m_suggestionView->show();
    m_suggestionView->raise();
    animateSuggestionPopup();
}

void BrowserWindow::requestRemoteSuggestions()
{
    const QString query = m_pendingSuggestionQuery.trimmed();
    if (query.size() < 2 || query.contains(QStringLiteral("://")) || !m_addressBar->hasFocus())
        return;

    QUrl endpoint(QStringLiteral("https://suggestqueries.google.com/complete/search"));
    QUrlQuery urlQuery;
    urlQuery.addQueryItem(QStringLiteral("client"), QStringLiteral("firefox"));
    urlQuery.addQueryItem(QStringLiteral("q"), query);
    endpoint.setQuery(urlQuery);

    QNetworkRequest request(endpoint);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("CachySurf/%1").arg(QStringLiteral(CACHYSURF_VERSION)));

    QNetworkReply *reply = m_network->get(request);
    reply->setProperty("cachysurfQuery", query);
    m_suggestionReply = reply;

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        const QString requestedQuery = reply->property("cachysurfQuery").toString();
        if (m_suggestionReply == reply)
            m_suggestionReply = nullptr;

        QStringList suggestions;
        if (reply->error() == QNetworkReply::NoError) {
            const QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
            if (document.isArray()) {
                const QJsonArray rootArray = document.array();
                if (rootArray.size() > 1 && rootArray.at(1).isArray()) {
                    for (const QJsonValue &value : rootArray.at(1).toArray()) {
                        const QString phrase = value.toString().trimmed();
                        if (!phrase.isEmpty() && !suggestions.contains(phrase, Qt::CaseInsensitive))
                            suggestions.append(phrase);
                        if (suggestions.size() >= 6)
                            break;
                    }
                }
            }
        }
        reply->deleteLater();

        if (requestedQuery == m_pendingSuggestionQuery && m_addressBar->hasFocus())
            rebuildSuggestionModel(requestedQuery, suggestions);
    });
}

void BrowserWindow::activateSuggestion(const QModelIndex &index)
{
    if (!index.isValid() || !currentView() || m_addressCommitPending)
        return;

    const QString value = index.data(Qt::UserRole + 1).toString();
    const QString kind = index.data(Qt::UserRole + 2).toString();
    if (value.isEmpty())
        return;

    m_addressCommitPending = true;
    dismissAddressSuggestions();
    m_addressBar->setText(value);
    currentView()->load(kind == QStringLiteral("url") ? QUrl(value) : normalizedUrl(value));
    currentView()->setFocus(Qt::OtherFocusReason);
    QTimer::singleShot(0, this, [this] { m_addressCommitPending = false; });
}

void BrowserWindow::selectAddressSuggestion(int delta)
{
    if (!m_suggestionView || !m_suggestionView->isVisible() ||
        !m_suggestionModel || m_suggestionModel->rowCount() == 0) {
        return;
    }

    const int rows = m_suggestionModel->rowCount();
    if (m_selectedSuggestionRow < 0)
        m_selectedSuggestionRow = delta > 0 ? 0 : rows - 1;
    else
        m_selectedSuggestionRow = (m_selectedSuggestionRow + delta + rows) % rows;

    const QModelIndex index = m_suggestionModel->index(m_selectedSuggestionRow, 0);
    m_suggestionView->setCurrentIndex(index);
    m_suggestionView->scrollTo(index, QAbstractItemView::PositionAtCenter);
}

void BrowserWindow::positionSuggestionPopup()
{
    if (!m_suggestionView || !m_addressBar || !m_root || !m_suggestionModel)
        return;

    const QPoint position = m_addressBar->mapTo(m_root, QPoint(0, m_addressBar->height() + 7));
    const int rows = std::min(8, m_suggestionModel->rowCount());
    const int height = rows * 44 + 12;
    m_suggestionView->setGeometry(position.x(), position.y(), m_addressBar->width(), height);
}

void BrowserWindow::dismissAddressSuggestions()
{
    m_pendingSuggestionQuery.clear();
    m_selectedSuggestionRow = -1;
    if (m_suggestionTimer)
        m_suggestionTimer->stop();
    if (m_suggestionReply) {
        disconnect(m_suggestionReply, nullptr, this, nullptr);
        m_suggestionReply->abort();
        m_suggestionReply->deleteLater();
        m_suggestionReply = nullptr;
    }
    if (m_suggestionView) {
        m_suggestionView->hide();
        m_suggestionView->clearSelection();
        m_suggestionView->setCurrentIndex(QModelIndex());
    }
    if (m_suggestionModel)
        m_suggestionModel->clear();
}

void BrowserWindow::animateSuggestionPopup()
{
    if (!m_suggestionView || !m_suggestionView->isVisible())
        return;

    auto *effect = qobject_cast<QGraphicsOpacityEffect *>(m_suggestionView->graphicsEffect());
    if (!effect) {
        effect = new QGraphicsOpacityEffect(m_suggestionView);
        m_suggestionView->setGraphicsEffect(effect);
    }
    effect->setOpacity(0.0);

    auto *animation = new QPropertyAnimation(effect, "opacity", effect);
    animation->setDuration(135);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(animation, &QPropertyAnimation::finished,
            animation, &QObject::deleteLater);
    animation->start();
}

QUrl BrowserWindow::normalizedUrl(const QString &input) const
{
    const QString trimmed = input.trimmed();
    if (trimmed.isEmpty())
        return QUrl(QStringLiteral("qrc:/assets/newtab.html"));

    QUrl url = QUrl::fromUserInput(trimmed);
    const bool looksLikeSearch = trimmed.contains(QLatin1Char(' ')) ||
                                 (!trimmed.contains(QLatin1Char('.')) &&
                                  !trimmed.startsWith(QStringLiteral("localhost")));
    if (looksLikeSearch) {
        return QUrl(QStringLiteral("https://www.google.com/search?q=") +
                    QString::fromUtf8(QUrl::toPercentEncoding(trimmed)));
    }
    return url;
}

void BrowserWindow::updateFromCurrentView()
{
    auto *view = currentView();
    if (!view)
        return;

    const QUrl url = view->url();
    m_addressBar->setText(url.scheme() == QStringLiteral("qrc") ? QString() : url.toDisplayString());
    m_addressBar->setCursorPosition(0);
    updateNavigationState();

    const QString title = view->title().isEmpty() ? QStringLiteral("Cachy Surf") : view->title();
    setWindowTitle((m_privateMode ? QStringLiteral("Private — ") : QString()) + title);
}

void BrowserWindow::updateNavigationState()
{
    auto *view = currentView();
    const bool hasView = view != nullptr;
    m_backButton->setEnabled(hasView && view->history()->canGoBack());
    m_forwardButton->setEnabled(hasView && view->history()->canGoForward());
}

void BrowserWindow::handleDownload(QWebEngineDownloadRequest *download)
{
    const QString downloads = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    const QString suggested = download->suggestedFileName().isEmpty()
        ? QStringLiteral("download")
        : download->suggestedFileName();
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save Download"),
                                                       QDir(downloads).filePath(suggested));
    if (path.isEmpty()) {
        download->cancel();
        return;
    }

    const QFileInfo info(path);
    download->setDownloadDirectory(info.absolutePath());
    download->setDownloadFileName(info.fileName());
    download->accept();
}

void BrowserWindow::showMainMenu()
{
    m_mainMenu->popup(m_menuButton->mapToGlobal(QPoint(0, m_menuButton->height())));
}

void BrowserWindow::toggleSidebar()
{
    m_sidebarVisible = !m_sidebarVisible;

    auto *effect = qobject_cast<QGraphicsOpacityEffect *>(m_sidebar->graphicsEffect());
    if (!effect) {
        effect = new QGraphicsOpacityEffect(m_sidebar);
        effect->setOpacity(m_sidebarVisible ? 0.0 : 1.0);
        m_sidebar->setGraphicsEffect(effect);
    }

    if (m_sidebarVisible) {
        effect->setOpacity(0.0);
        m_sidebar->show();
    }

    auto *group = new QParallelAnimationGroup(this);
    auto *widthAnimation = new QPropertyAnimation(m_sidebar, "maximumSize", group);
    widthAnimation->setDuration(220);
    widthAnimation->setStartValue(m_sidebar->maximumSize());
    widthAnimation->setEndValue(QSize(m_sidebarVisible ? 245 : 0, QWIDGETSIZE_MAX));
    widthAnimation->setEasingCurve(QEasingCurve::OutCubic);

    auto *opacityAnimation = new QPropertyAnimation(effect, "opacity", group);
    opacityAnimation->setDuration(180);
    opacityAnimation->setStartValue(effect->opacity());
    opacityAnimation->setEndValue(m_sidebarVisible ? 1.0 : 0.0);
    opacityAnimation->setEasingCurve(QEasingCurve::OutCubic);

    connect(group, &QParallelAnimationGroup::finished, this, [this, group] {
        if (!m_sidebarVisible)
            m_sidebar->hide();
        group->deleteLater();
    });
    group->start();

    m_sidebarButton->setToolTip(m_sidebarVisible ? QStringLiteral("Hide sidebar")
                                                 : QStringLiteral("Show sidebar"));
    for (QAction *action : m_mainMenu->actions()) {
        if (action->text().contains(QStringLiteral("Sidebar"))) {
            action->setChecked(m_sidebarVisible);
            action->setText(m_sidebarVisible ? QStringLiteral("Hide Sidebar")
                                             : QStringLiteral("Show Sidebar"));
            break;
        }
    }
}

void BrowserWindow::addCurrentBookmark()
{
    auto *view = currentView();
    if (!view || view->url().scheme() == QStringLiteral("qrc"))
        return;

    QString title = view->title().trimmed();
    if (title.isEmpty())
        title = view->url().host();

    for (const auto &bookmark : std::as_const(m_bookmarks)) {
        if (bookmark.second == view->url())
            return;
    }

    m_bookmarks.append({title, view->url()});
    saveBookmarks();
    refreshSidebar();
}

void BrowserWindow::loadBookmarks()
{
    if (m_privateMode)
        return;

    QSettings settings;
    const int count = settings.beginReadArray(QStringLiteral("bookmarks"));
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        const QString title = settings.value(QStringLiteral("title")).toString();
        const QUrl url = settings.value(QStringLiteral("url")).toUrl();
        if (!title.isEmpty() && url.isValid())
            m_bookmarks.append({title, url});
    }
    settings.endArray();
}

void BrowserWindow::saveBookmarks() const
{
    if (m_privateMode)
        return;

    QSettings settings;
    settings.beginWriteArray(QStringLiteral("bookmarks"));
    for (int i = 0; i < m_bookmarks.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue(QStringLiteral("title"), m_bookmarks.at(i).first);
        settings.setValue(QStringLiteral("url"), m_bookmarks.at(i).second);
    }
    settings.endArray();
}

void BrowserWindow::refreshSidebar()
{
    m_sidebar->clear();

    const QList<QPair<QString, QUrl>> builtIns = {
        {QStringLiteral("CachyOS"), QUrl(QStringLiteral("https://cachyos.org"))},
        {QStringLiteral("Packages"), QUrl(QStringLiteral("https://packages.cachyos.org"))},
        {QStringLiteral("Wiki"), QUrl(QStringLiteral("https://wiki.cachyos.org"))},
        {QStringLiteral("Forum"), QUrl(QStringLiteral("https://discuss.cachyos.org"))},
        {QStringLiteral("GitHub"), QUrl(QStringLiteral("https://github.com/CachyOS"))}
    };

    auto *start = new QListWidgetItem(QStringLiteral("Start Page"), m_sidebar);
    start->setData(Qt::UserRole, QUrl(QStringLiteral("qrc:/assets/newtab.html")));

    for (const auto &entry : builtIns) {
        auto *item = new QListWidgetItem(entry.first, m_sidebar);
        item->setData(Qt::UserRole, entry.second);
    }

    if (!m_bookmarks.isEmpty()) {
        auto *separator = new QListWidgetItem(QStringLiteral("BOOKMARKS"), m_sidebar);
        separator->setFlags(Qt::NoItemFlags);
        for (const auto &bookmark : std::as_const(m_bookmarks)) {
            auto *item = new QListWidgetItem(bookmark.first, m_sidebar);
            item->setData(Qt::UserRole, bookmark.second);
        }
    }
}

void BrowserWindow::openSidebarItem()
{
    const auto *item = m_sidebar->currentItem();
    if (!item || !(item->flags() & Qt::ItemIsEnabled))
        return;
    const QUrl url = item->data(Qt::UserRole).toUrl();
    if (currentView() && url.isValid())
        currentView()->load(url);
}

void BrowserWindow::toggleDarkWebsites(bool enabled)
{
    m_darkUi = enabled;
    m_darkWebsites = enabled;
    if (!m_privateMode) {
        QSettings settings;
        settings.setValue(QStringLiteral("appearance/darkMode"), enabled);
        settings.setValue(QStringLiteral("privacy/forceDarkWebsites"), enabled);
    }

    applyStyle();
    for (int i = 0; i < m_stack->count(); ++i) {
        if (auto *view = qobject_cast<WebView *>(m_stack->widget(i))) {
            view->settings()->setAttribute(QWebEngineSettings::ForceDarkMode, enabled);
            applyWebDarkMode(view);
            view->reload();
        }
    }
}

void BrowserWindow::applyWebDarkMode(WebView *view)
{
    if (!view || !view->page())
        return;

    view->settings()->setAttribute(QWebEngineSettings::ForceDarkMode, m_darkWebsites);
    const QString script = m_darkWebsites
        ? QStringLiteral(R"JS(
            (() => {
                const id = 'cachysurf-dark-mode';
                let style = document.getElementById(id);
                if (!style) {
                    style = document.createElement('style');
                    style.id = id;
                    style.textContent = `
                        html { color-scheme: dark !important; background: #111214 !important; }
                        html.cachysurf-invert-dark { filter: invert(.90) hue-rotate(180deg) !important; }
                        html.cachysurf-invert-dark img,
                        html.cachysurf-invert-dark picture,
                        html.cachysurf-invert-dark video,
                        html.cachysurf-invert-dark canvas,
                        html.cachysurf-invert-dark svg,
                        html.cachysurf-invert-dark iframe {
                            filter: invert(1) hue-rotate(180deg) !important;
                        }
                    `;
                    (document.head || document.documentElement).appendChild(style);
                }
                document.documentElement.setAttribute('dark', '');
                document.documentElement.style.colorScheme = 'dark';
                const rgb = getComputedStyle(document.body || document.documentElement)
                    .backgroundColor.match(/\d+/g);
                if (rgb && rgb.length >= 3) {
                    const lightness = (Number(rgb[0]) * 299 + Number(rgb[1]) * 587 + Number(rgb[2]) * 114) / 1000;
                    document.documentElement.classList.toggle('cachysurf-invert-dark', lightness > 185);
                }
            })();
        )JS")
        : QStringLiteral(R"JS(
            (() => {
                document.getElementById('cachysurf-dark-mode')?.remove();
                document.documentElement.classList.remove('cachysurf-invert-dark');
                document.documentElement.removeAttribute('dark');
                document.documentElement.style.colorScheme = '';
            })();
        )JS");
    view->page()->runJavaScript(script);
}

void BrowserWindow::toggleTrackerBlocking(bool enabled)
{
    m_trackerBlocker->setEnabled(enabled);
    if (!m_privateMode)
        QSettings().setValue(QStringLiteral("privacy/trackerBlocking"), enabled);
}

void BrowserWindow::togglePasswordAutofill(bool enabled)
{
    m_passwordAutofill = enabled;
    if (!m_privateMode)
        QSettings().setValue(QStringLiteral("passwords/autofill"), enabled);
}

void BrowserWindow::showPasswordManager()
{
    if (m_privateMode)
        return;

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Passwords"));
    dialog.resize(580, 390);

    auto *layout = new QVBoxLayout(&dialog);
    auto *message = new QLabel(
        QStringLiteral("Saved passwords are kept in your Linux system keyring. "
                       "Cachy Surf stores only the website and username in its settings."),
        &dialog);
    message->setWordWrap(true);
    layout->addWidget(message);

    auto *list = new QListWidget(&dialog);
    for (const PasswordEntry &entry : m_passwordStore->entries()) {
        auto *item = new QListWidgetItem(
            QStringLiteral("%1   —   %2").arg(entry.host, entry.username), list);
        item->setData(Qt::UserRole, entry.id);
        item->setData(Qt::UserRole + 1, entry.username);
    }
    layout->addWidget(list, 1);

    auto *buttons = new QDialogButtonBox(&dialog);
    auto *addButton = buttons->addButton(QStringLiteral("Add"), QDialogButtonBox::ActionRole);
    auto *importButton = buttons->addButton(QStringLiteral("Import CSV…"), QDialogButtonBox::ActionRole);
    auto *fillButton = buttons->addButton(QStringLiteral("Fill current page"), QDialogButtonBox::ActionRole);
    auto *deleteButton = buttons->addButton(QStringLiteral("Delete"), QDialogButtonBox::DestructiveRole);
    buttons->addButton(QDialogButtonBox::Close);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(addButton, &QPushButton::clicked, &dialog, [&dialog, this] {
        dialog.accept();
        addPasswordForCurrentSite();
    });
    connect(importButton, &QPushButton::clicked, &dialog, [&dialog, this] {
        dialog.accept();
        importPasswordsFromCsv();
    });
    connect(fillButton, &QPushButton::clicked, &dialog, [&dialog, list, this] {
        const auto *item = list->currentItem();
        if (!item)
            return;
        fillCredential(item->data(Qt::UserRole).toString(),
                       item->data(Qt::UserRole + 1).toString());
        dialog.accept();
    });
    connect(deleteButton, &QPushButton::clicked, &dialog, [list, this] {
        auto *item = list->currentItem();
        if (!item)
            return;
        const QString id = item->data(Qt::UserRole).toString();
        QPointer<QListWidget> safeList(list);
        m_passwordStore->remove(id, [this, safeList, id](bool ok, const QString &error) {
            if (!ok) {
                QMessageBox::warning(this, QStringLiteral("Could not delete password"), error);
                return;
            }
            if (!safeList)
                return;
            for (int row = 0; row < safeList->count(); ++row) {
                QListWidgetItem *candidate = safeList->item(row);
                if (candidate->data(Qt::UserRole).toString() == id) {
                    delete safeList->takeItem(row);
                    break;
                }
            }
        });
    });

    dialog.exec();
}

void BrowserWindow::importPasswordsFromCsv()
{
    if (m_privateMode)
        return;

    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Import Passwords"),
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
        QStringLiteral("CSV files (*.csv);;All files (*)"));
    if (path.isEmpty())
        return;

    const auto confirmation = QMessageBox::question(
        this,
        QStringLiteral("Import password file"),
        QStringLiteral("CSV password exports contain readable passwords. Cachy Surf will copy them into your Linux system keyring, "
                       "but the CSV file will remain on disk until you delete it. Continue?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (confirmation != QMessageBox::Yes)
        return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("Could not open CSV"), file.errorString());
        return;
    }

    QByteArray raw = file.readAll();
    if (raw.startsWith(QByteArray::fromHex("EFBBBF")))
        raw.remove(0, 3);
    const QVector<QStringList> rows = parseCsv(QString::fromUtf8(raw));
    if (rows.size() < 2) {
        QMessageBox::warning(this, QStringLiteral("Invalid CSV"),
                             QStringLiteral("The file does not contain a header and password rows."));
        return;
    }

    const QStringList headers = rows.first();
    const int siteColumn = csvColumn(headers, {
        QStringLiteral("url"), QStringLiteral("website"), QStringLiteral("origin"),
        QStringLiteral("host"), QStringLiteral("hostname"), QStringLiteral("loginuri")
    });
    const int usernameColumn = csvColumn(headers, {
        QStringLiteral("username"), QStringLiteral("user"), QStringLiteral("login"),
        QStringLiteral("email"), QStringLiteral("loginusername")
    });
    const int passwordColumn = csvColumn(headers, {
        QStringLiteral("password"), QStringLiteral("pass"), QStringLiteral("passwordvalue")
    });

    if (siteColumn < 0 || passwordColumn < 0) {
        QMessageBox::warning(
            this,
            QStringLiteral("Unsupported CSV columns"),
            QStringLiteral("Cachy Surf could not find the website and password columns. Supported headings include URL, Website, Host, Username, and Password."));
        return;
    }

    struct ImportRecord {
        QString host;
        QString username;
        QString password;
    };
    struct ImportState {
        QVector<ImportRecord> records;
        int index = 0;
        int imported = 0;
        int updated = 0;
        int skipped = 0;
        int failed = 0;
        QStringList errors;
    };

    auto state = std::make_shared<ImportState>();
    for (int rowIndex = 1; rowIndex < rows.size(); ++rowIndex) {
        const QStringList &row = rows.at(rowIndex);
        const auto cell = [&row](int column) -> QString {
            return column >= 0 && column < row.size() ? row.at(column) : QString();
        };
        ImportRecord record;
        record.host = hostFromCsvValue(cell(siteColumn));
        record.username = cell(usernameColumn).trimmed();
        record.password = cell(passwordColumn);
        if (record.host.isEmpty() || record.password.isEmpty()) {
            ++state->skipped;
            continue;
        }
        state->records.append(std::move(record));
    }

    if (state->records.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Nothing to import"),
                                 QStringLiteral("No usable website and password rows were found."));
        return;
    }

    auto *progress = new QProgressDialog(
        QStringLiteral("Importing passwords into the system keyring…"),
        QStringLiteral("Cancel"), 0, state->records.size(), this);
    progress->setWindowTitle(QStringLiteral("Import Passwords"));
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->setValue(0);

    auto processNext = std::make_shared<std::function<void()>>();
    *processNext = [this, state, progress, processNext] {
        if (progress->wasCanceled() || state->index >= state->records.size()) {
            const bool canceled = progress->wasCanceled();
            progress->setValue(state->records.size());
            progress->deleteLater();

            QString summary = QStringLiteral("Imported: %1\nUpdated: %2\nSkipped: %3\nFailed: %4")
                                  .arg(state->imported)
                                  .arg(state->updated)
                                  .arg(state->skipped)
                                  .arg(state->failed);
            if (canceled)
                summary.prepend(QStringLiteral("Import canceled.\n\n"));
            if (!state->errors.isEmpty())
                summary += QStringLiteral("\n\nFirst errors:\n• ") + state->errors.join(QStringLiteral("\n• "));
            QMessageBox::information(this, QStringLiteral("Password import finished"), summary);
            *processNext = {};
            return;
        }

        const ImportRecord record = state->records.at(state->index);
        progress->setLabelText(QStringLiteral("Importing %1…").arg(record.host));
        m_passwordStore->upsert(
            record.host,
            record.username,
            record.password,
            [this, state, progress, processNext, record](bool ok, bool updated, const QString &error) {
                if (ok) {
                    updated ? ++state->updated : ++state->imported;
                } else {
                    ++state->failed;
                    if (state->errors.size() < 5)
                        state->errors.append(QStringLiteral("%1: %2").arg(record.host, error));
                }
                ++state->index;
                progress->setValue(state->index);
                QTimer::singleShot(0, this, [processNext] {
                    if (*processNext)
                        (*processNext)();
                });
            });
    };

    (*processNext)();
}

void BrowserWindow::addPasswordForCurrentSite()
{
    if (m_privateMode)
        return;

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Save Login"));
    auto *layout = new QFormLayout(&dialog);

    auto *site = new QLineEdit(&dialog);
    auto *username = new QLineEdit(&dialog);
    auto *password = new QLineEdit(&dialog);
    password->setEchoMode(QLineEdit::Password);
    site->setText(currentView() ? normalizedHost(currentView()->url().host()) : QString());

    layout->addRow(QStringLiteral("Website:"), site);
    layout->addRow(QStringLiteral("Username or email:"), username);
    layout->addRow(QStringLiteral("Password:"), password);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return;
    if (site->text().trimmed().isEmpty() || password->text().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Missing information"),
                             QStringLiteral("Enter a website and password."));
        return;
    }

    m_passwordStore->save(site->text(), username->text(), password->text(),
                          [this](bool ok, const QString &error) {
        if (ok) {
            QMessageBox::information(this, QStringLiteral("Password saved"),
                                     QStringLiteral("The login was saved in your system keyring."));
        } else {
            QMessageBox::warning(this, QStringLiteral("Could not save password"), error);
        }
    });
}

void BrowserWindow::fillPasswordForCurrentSite()
{
    if (!currentView() || m_privateMode)
        return;

    QVector<PasswordEntry> matches;
    for (const PasswordEntry &entry : m_passwordStore->entries()) {
        if (hostMatches(currentView()->url().host(), entry.host))
            matches.append(entry);
    }

    if (matches.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("No saved login"),
                                 QStringLiteral("No password is saved for this website."));
        return;
    }

    PasswordEntry selected = matches.first();
    if (matches.size() > 1) {
        QStringList choices;
        for (const PasswordEntry &entry : std::as_const(matches))
            choices.append(QStringLiteral("%1 — %2").arg(entry.host, entry.username));
        bool ok = false;
        const QString choice = QInputDialog::getItem(this, QStringLiteral("Choose login"),
                                                      QStringLiteral("Login:"), choices, 0, false, &ok);
        if (!ok)
            return;
        selected = matches.at(choices.indexOf(choice));
    }

    fillCredential(selected.id, selected.username);
}

void BrowserWindow::autofillCurrentSite()
{
    if (!m_passwordAutofill || m_privateMode || !currentView())
        return;

    for (const PasswordEntry &entry : m_passwordStore->entries()) {
        if (hostMatches(currentView()->url().host(), entry.host)) {
            fillCredential(entry.id, entry.username);
            return;
        }
    }
}

void BrowserWindow::fillCredential(const QString &id, const QString &username)
{
    m_passwordStore->read(id, [this, username](bool ok, const QString &password, const QString &error) {
        if (!ok) {
            QMessageBox::warning(this, QStringLiteral("Could not read password"), error);
            return;
        }
        fillCredentialData(username, password);
    });
}

void BrowserWindow::fillCredentialData(const QString &username, const QString &password)
{
    if (!currentView())
        return;

    const QJsonArray values{username, password};
    const QString json = QString::fromUtf8(QJsonDocument(values).toJson(QJsonDocument::Compact));
    const QString script = QStringLiteral(R"JS(
(() => {
    const values = %1;
    const username = values[0];
    const password = values[1];
    const inputs = Array.from(document.querySelectorAll('input'))
        .filter(input => !input.disabled && !input.readOnly && input.offsetParent !== null);
    const passwordInput = inputs.find(input => input.type === 'password');
    if (!passwordInput) return false;
    const passwordIndex = inputs.indexOf(passwordInput);
    const usernameInput = inputs.slice(0, Math.max(passwordIndex, 0)).reverse().find(input =>
        ['email', 'text', 'tel'].includes(input.type) ||
        /user|email|login|account/i.test(`${input.name} ${input.id} ${input.autocomplete}`)
    );
    const setValue = (input, value) => {
        if (!input || !value) return;
        const setter = Object.getOwnPropertyDescriptor(HTMLInputElement.prototype, 'value').set;
        setter.call(input, value);
        input.dispatchEvent(new Event('input', { bubbles: true }));
        input.dispatchEvent(new Event('change', { bubbles: true }));
    };
    setValue(usernameInput, username);
    setValue(passwordInput, password);
    return true;
})()
)JS").arg(json);
    currentView()->page()->runJavaScript(script);
}

void BrowserWindow::showExtensionManager()
{
    if (m_privateMode) {
        QMessageBox::information(this, QStringLiteral("Extensions unavailable"),
                                 QStringLiteral("Extensions are disabled in private windows."));
        return;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    QWebEngineExtensionManager *manager = m_profile ? m_profile->extensionManager() : nullptr;
    if (!manager) {
        QMessageBox::warning(this, QStringLiteral("Extensions unavailable"),
                             QStringLiteral("Qt WebEngine did not provide an extension manager."));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Extensions"));
    dialog.resize(720, 520);
    auto *layout = new QVBoxLayout(&dialog);

    auto *message = new QLabel(
        QStringLiteral("Install Manifest V3 Chrome extensions from a ZIP file or an unpacked folder. "
                       "Chrome Web Store one-click installation is not supported, so download the extension package first."),
        &dialog);
    message->setWordWrap(true);
    layout->addWidget(message);

    auto *list = new QListWidget(&dialog);
    list->setAlternatingRowColors(true);
    layout->addWidget(list, 1);

    const auto isInternal = [](const QWebEngineExtensionInfo &extension) {
        return extension.id() == QStringLiteral("mhjfbmdgcfjbbpaeojofohoefgiehjai")
            || extension.id() == QStringLiteral("nkeimhogjdpnpccoofpliimaahmaaome");
    };

    std::function<void()> refresh = [list, manager, isInternal] {
        QSignalBlocker blocker(list);
        const QString selectedId = list->currentItem()
            ? list->currentItem()->data(Qt::UserRole).toString()
            : QString();
        list->clear();
        int selectedRow = -1;
        for (const QWebEngineExtensionInfo &extension : manager->extensions()) {
            if (isInternal(extension) || extension.id().isEmpty())
                continue;
            QString title = extension.name().isEmpty() ? extension.id() : extension.name();
            if (!extension.description().isEmpty())
                title += QStringLiteral("\n") + extension.description();
            auto *item = new QListWidgetItem(title, list);
            item->setData(Qt::UserRole, extension.id());
            item->setData(Qt::UserRole + 1, extension.actionPopupUrl());
            item->setData(Qt::UserRole + 2, extension.isInstalled());
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(extension.isEnabled() ? Qt::Checked : Qt::Unchecked);
            item->setToolTip(QStringLiteral("%1\n%2")
                                 .arg(extension.path(),
                                      extension.isInstalled() ? QStringLiteral("Installed permanently")
                                                              : QStringLiteral("Loaded for this session")));
            if (extension.id() == selectedId)
                selectedRow = list->row(item);
        }
        if (selectedRow >= 0)
            list->setCurrentRow(selectedRow);
        else if (list->count() > 0)
            list->setCurrentRow(0);
    };
    refresh();

    auto *buttons = new QDialogButtonBox(&dialog);
    auto *installZip = buttons->addButton(QStringLiteral("Install ZIP…"), QDialogButtonBox::ActionRole);
    auto *installFolder = buttons->addButton(QStringLiteral("Install Folder…"), QDialogButtonBox::ActionRole);
    auto *popupButton = buttons->addButton(QStringLiteral("Open Extension"), QDialogButtonBox::ActionRole);
    auto *folderButton = buttons->addButton(QStringLiteral("Open Install Folder"), QDialogButtonBox::ActionRole);
    auto *removeButton = buttons->addButton(QStringLiteral("Remove"), QDialogButtonBox::DestructiveRole);
    buttons->addButton(QDialogButtonBox::Close);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(list, &QListWidget::itemChanged, &dialog,
            [manager](QListWidgetItem *item) {
        if (!item)
            return;
        const QString id = item->data(Qt::UserRole).toString();
        for (const QWebEngineExtensionInfo &extension : manager->extensions()) {
            if (extension.id() != id)
                continue;
            const bool enabled = item->checkState() == Qt::Checked;
            manager->setExtensionEnabled(extension, enabled);
            setExtensionEnabledId(id, enabled);
            break;
        }
    });

    connect(installZip, &QPushButton::clicked, &dialog, [this, manager] {
        const QString path = QFileDialog::getOpenFileName(
            this,
            QStringLiteral("Install Extension ZIP"),
            QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
            QStringLiteral("Extension ZIP (*.zip);;All files (*)"));
        if (!path.isEmpty())
            manager->installExtension(path);
    });
    connect(installFolder, &QPushButton::clicked, &dialog, [this, manager] {
        const QString path = QFileDialog::getExistingDirectory(
            this,
            QStringLiteral("Install Unpacked Extension"),
            QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
        if (!path.isEmpty())
            manager->installExtension(path);
    });
    connect(folderButton, &QPushButton::clicked, &dialog, [manager] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(manager->installPath()));
    });
    connect(popupButton, &QPushButton::clicked, &dialog, [this, list, manager] {
        if (!list->currentItem())
            return;
        const QString id = list->currentItem()->data(Qt::UserRole).toString();
        for (const QWebEngineExtensionInfo &extension : manager->extensions()) {
            if (extension.id() != id)
                continue;
            if (!extension.isEnabled()) {
                QMessageBox::information(this, QStringLiteral("Extension disabled"),
                                         QStringLiteral("Enable the extension before opening it."));
                return;
            }
            if (!extension.actionPopupUrl().isValid() || extension.actionPopupUrl().isEmpty()) {
                QMessageBox::information(this, QStringLiteral("No extension window"),
                                         QStringLiteral("This extension does not provide a toolbar popup."));
                return;
            }
            createTab(extension.actionPopupUrl(), true);
            return;
        }
    });
    connect(removeButton, &QPushButton::clicked, &dialog, [this, list, manager] {
        if (!list->currentItem())
            return;
        const QString id = list->currentItem()->data(Qt::UserRole).toString();
        for (const QWebEngineExtensionInfo &extension : manager->extensions()) {
            if (extension.id() != id)
                continue;
            const auto answer = QMessageBox::question(
                this,
                QStringLiteral("Remove extension"),
                QStringLiteral("Remove %1?").arg(extension.name().isEmpty() ? extension.id() : extension.name()),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
            if (answer != QMessageBox::Yes)
                return;
            setExtensionEnabledId(id, false);
            if (extension.isInstalled())
                manager->uninstallExtension(extension);
            else
                manager->unloadExtension(extension);
            return;
        }
    });

    connect(manager, &QWebEngineExtensionManager::installFinished, &dialog,
            [&dialog, &refresh](const QWebEngineExtensionInfo &extension) {
        if (!extension.error().isEmpty()) {
            QMessageBox::warning(&dialog, QStringLiteral("Extension install failed"), extension.error());
        }
        refresh();
    });
    connect(manager, &QWebEngineExtensionManager::loadFinished, &dialog,
            [&refresh](const QWebEngineExtensionInfo &) { refresh(); });
    connect(manager, &QWebEngineExtensionManager::uninstallFinished, &dialog,
            [&refresh](const QWebEngineExtensionInfo &) { refresh(); });
    connect(manager, &QWebEngineExtensionManager::unloadFinished, &dialog,
            [&refresh](const QWebEngineExtensionInfo &) { refresh(); });

    dialog.exec();
#else
    QMessageBox::information(
        this,
        QStringLiteral("Extensions require a newer Qt"),
        QStringLiteral("Install Qt WebEngine 6.10 or newer to use Manifest V3 extensions."));
#endif
}

void BrowserWindow::showDeviceManager()
{
    if (m_hidBridge)
        m_hidBridge->showDeviceManager(this);
}

void BrowserWindow::loadHistory()
{
    if (m_privateMode)
        return;

    QSettings settings;
    const int count = settings.beginReadArray(QStringLiteral("history"));
    for (int index = 0; index < count; ++index) {
        settings.setArrayIndex(index);
        const QString title = settings.value(QStringLiteral("title")).toString();
        const QUrl url = settings.value(QStringLiteral("url")).toUrl();
        if (url.isValid())
            m_history.append({title, url});
    }
    settings.endArray();
}

void BrowserWindow::saveHistory() const
{
    if (m_privateMode)
        return;

    QSettings settings;
    settings.beginWriteArray(QStringLiteral("history"));
    for (int index = 0; index < m_history.size(); ++index) {
        settings.setArrayIndex(index);
        settings.setValue(QStringLiteral("title"), m_history.at(index).first);
        settings.setValue(QStringLiteral("url"), m_history.at(index).second);
    }
    settings.endArray();
}

void BrowserWindow::recordHistory(WebView *view)
{
    if (m_privateMode || !view)
        return;
    const QUrl url = view->url();
    if (!url.isValid() || url.scheme() == QStringLiteral("qrc") || url.scheme() == QStringLiteral("data"))
        return;

    for (int index = m_history.size() - 1; index >= 0; --index) {
        if (m_history.at(index).second == url)
            m_history.removeAt(index);
    }

    const QString title = view->title().isEmpty() ? url.host() : view->title();
    m_history.prepend({title, url});
    while (m_history.size() > kHistoryLimit)
        m_history.removeLast();
    saveHistory();
}

void BrowserWindow::showHistory()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("History"));
    dialog.resize(700, 500);
    auto *layout = new QVBoxLayout(&dialog);
    auto *list = new QListWidget(&dialog);
    for (const auto &entry : std::as_const(m_history)) {
        auto *item = new QListWidgetItem(
            QStringLiteral("%1\n%2").arg(entry.first, entry.second.toDisplayString()), list);
        item->setData(Qt::UserRole, entry.second);
    }
    layout->addWidget(list, 1);

    auto *buttons = new QDialogButtonBox(&dialog);
    auto *clearButton = buttons->addButton(QStringLiteral("Clear History"), QDialogButtonBox::DestructiveRole);
    buttons->addButton(QDialogButtonBox::Close);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(list, &QListWidget::itemActivated, &dialog, [this, &dialog](QListWidgetItem *item) {
        if (currentView())
            currentView()->load(item->data(Qt::UserRole).toUrl());
        dialog.accept();
    });
    connect(clearButton, &QPushButton::clicked, &dialog, [this, list] {
        m_history.clear();
        saveHistory();
        list->clear();
    });
    dialog.exec();
}

void BrowserWindow::findOnPage()
{
    if (!currentView())
        return;
    bool ok = false;
    const QString text = QInputDialog::getText(this, QStringLiteral("Find on Page"),
                                               QStringLiteral("Text:"), QLineEdit::Normal,
                                               QString(), &ok);
    if (ok && !text.isEmpty())
        currentView()->findText(text);
}

void BrowserWindow::printCurrentPage()
{
    if (!currentView())
        return;
    const QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString suggested = QDir(documents).filePath(QStringLiteral("Cachy-Surf-page.pdf"));
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save Page as PDF"),
                                                       suggested, QStringLiteral("PDF files (*.pdf)"));
    if (!path.isEmpty())
        currentView()->page()->printToPdf(path);
}

void BrowserWindow::clearBrowsingData()
{
    if (m_privateMode)
        return;
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Clear Browsing Data"),
        QStringLiteral("Clear cookies, cache, and browsing history? Saved passwords and bookmarks will stay."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    m_profile->clearHttpCache();
    m_profile->cookieStore()->deleteAllCookies();
    m_profile->clearAllVisitedLinks();
    m_history.clear();
    saveHistory();
    QMessageBox::information(this, QStringLiteral("Browsing data cleared"),
                             QStringLiteral("Cookies, cache, and history were cleared."));
}

void BrowserWindow::reopenClosedTab()
{
    if (m_closedTabs.isEmpty())
        return;
    createTab(m_closedTabs.takeFirst(), true);
}

void BrowserWindow::openPrivateWindow()
{
    auto *window = new BrowserWindow(true);
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->show();
}

void BrowserWindow::toggleFullScreen()
{
    isFullScreen() ? showNormal() : showFullScreen();
}

bool BrowserWindow::event(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange ||
        event->type() == QEvent::Resize ||
        event->type() == QEvent::Show) {
        QTimer::singleShot(0, this, [this] {
            updateWindowShape();
            if (m_suggestionView && m_suggestionView->isVisible())
                positionSuggestionPopup();
        });
    }
    return QMainWindow::event(event);
}

void BrowserWindow::updateWindowShape()
{
    if (!m_root)
        return;

    const int margin = (isMaximized() || isFullScreen()) ? 0 : 8;
    if (auto *layout = centralWidget()->layout())
        layout->setContentsMargins(margin, margin, margin, margin);

    if (isMaximized() || isFullScreen())
        m_root->setStyleSheet(QStringLiteral("QFrame#root { border-radius: 0px; }"));
    else
        m_root->setStyleSheet(QString());
}

int BrowserWindow::tabIndexForView(const WebView *view) const
{
    for (int i = 0; i < m_tabBar->count(); ++i) {
        const auto *candidate = static_cast<WebView *>(m_tabBar->tabData(i).value<void *>());
        if (candidate == view)
            return i;
    }
    return -1;
}

int BrowserWindow::edgeAt(const QPoint &position) const
{
    int edge = 0;
    if (position.x() <= kResizeMargin) edge |= kEdgeLeft;
    if (position.x() >= width() - kResizeMargin) edge |= kEdgeRight;
    if (position.y() <= kResizeMargin) edge |= kEdgeTop;
    if (position.y() >= height() - kResizeMargin) edge |= kEdgeBottom;
    return edge;
}

void BrowserWindow::beginSystemResize(int edgeMask)
{
    if (!windowHandle() || edgeMask == 0)
        return;

    Qt::Edges edges;
    if (edgeMask & kEdgeLeft) edges |= Qt::LeftEdge;
    if (edgeMask & kEdgeTop) edges |= Qt::TopEdge;
    if (edgeMask & kEdgeRight) edges |= Qt::RightEdge;
    if (edgeMask & kEdgeBottom) edges |= Qt::BottomEdge;
    windowHandle()->startSystemResize(edges);
}

void BrowserWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressedEdge = edgeAt(event->position().toPoint());
        if (m_pressedEdge) {
            beginSystemResize(m_pressedEdge);
            event->accept();
            return;
        }

        const QPoint topPos = m_topBar->mapFrom(this, event->position().toPoint());
        if (m_topBar->rect().contains(topPos) && windowHandle()) {
            windowHandle()->startSystemMove();
            event->accept();
            return;
        }
    }
    QMainWindow::mousePressEvent(event);
}

void BrowserWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (!isMaximized() && !isFullScreen()) {
        const int edge = edgeAt(event->position().toPoint());
        if ((edge & (kEdgeLeft | kEdgeRight)) && (edge & (kEdgeTop | kEdgeBottom)))
            setCursor(Qt::SizeFDiagCursor);
        else if (edge & (kEdgeLeft | kEdgeRight))
            setCursor(Qt::SizeHorCursor);
        else if (edge & (kEdgeTop | kEdgeBottom))
            setCursor(Qt::SizeVerCursor);
        else
            unsetCursor();
    }
    QMainWindow::mouseMoveEvent(event);
}

void BrowserWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    const QPoint topPos = m_topBar->mapFrom(this, event->position().toPoint());
    if (event->button() == Qt::LeftButton && m_topBar->rect().contains(topPos)) {
        isMaximized() ? showNormal() : showMaximized();
        event->accept();
        return;
    }
    QMainWindow::mouseDoubleClickEvent(event);
}
