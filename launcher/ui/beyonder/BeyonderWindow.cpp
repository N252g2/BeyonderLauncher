// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Beyonder Launcher - a fork of Prism Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "BeyonderWindow.h"

#include <algorithm>

#include <QButtonGroup>
#include <QCloseEvent>
#include <QDebug>
#include <QDesktopServices>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLinearGradient>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPushButton>
#include <QSizeGrip>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

#include "Application.h"
#include "BuildConfig.h"
#include "DesktopServices.h"
#include "InstanceImportTask.h"
#include "InstanceList.h"
#include "Version.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/auth/AccountList.h"
#include "minecraft/launch/MinecraftTarget.h"
#include "net/Request.h"
#include "ui/dialogs/MSALoginDialog.h"
#include "ui/beyonder/BeyonderAccountPage.h"
#include "ui/beyonder/BeyonderSettingsPage.h"
#include "ui/beyonder/BeyonderStyle.h"

using namespace Beyonder;

// =====================================================================================================================
// Look and feel
// =====================================================================================================================

namespace {

// Window root: paints the background and border, and turns its outer margin into resize handles.
class FrameRoot : public QWidget {
   public:
    explicit FrameRoot(QWidget* parent) : QWidget(parent) { setMouseTracking(true); }
    static constexpr int Margin = 5;

    int margin() const { return window()->isMaximized() || window()->isFullScreen() ? 0 : Margin; }

   protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.fillRect(rect(), QColor("#0e0e10"));
        if (margin() > 0) {
            p.setPen(QColor("#26262b"));
            p.drawRect(rect().adjusted(0, 0, -1, -1));
        }
    }

    Qt::Edges edgesAt(const QPoint& pos) const
    {
        const int m = margin();
        if (m == 0) {
            return {};
        }
        Qt::Edges e;
        if (pos.x() <= m)
            e |= Qt::LeftEdge;
        if (pos.x() >= width() - m)
            e |= Qt::RightEdge;
        if (pos.y() <= m)
            e |= Qt::TopEdge;
        if (pos.y() >= height() - m)
            e |= Qt::BottomEdge;
        return e;
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        const auto e = edgesAt(event->position().toPoint());
        if (e == (Qt::LeftEdge | Qt::TopEdge) || e == (Qt::RightEdge | Qt::BottomEdge))
            setCursor(Qt::SizeFDiagCursor);
        else if (e == (Qt::RightEdge | Qt::TopEdge) || e == (Qt::LeftEdge | Qt::BottomEdge))
            setCursor(Qt::SizeBDiagCursor);
        else if (e & (Qt::LeftEdge | Qt::RightEdge))
            setCursor(Qt::SizeHorCursor);
        else if (e & (Qt::TopEdge | Qt::BottomEdge))
            setCursor(Qt::SizeVerCursor);
        else
            unsetCursor();
        QWidget::mouseMoveEvent(event);
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        const auto e = edgesAt(event->position().toPoint());
        if (event->button() == Qt::LeftButton && e && window()->windowHandle()) {
            window()->windowHandle()->startSystemResize(e);
            return;
        }
        QWidget::mousePressEvent(event);
    }
};

// Title bar: drag to move, double-click to maximize.
class DragBar : public QWidget {
   public:
    using QWidget::QWidget;

   protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && window()->windowHandle()) {
            window()->windowHandle()->startSystemMove();
            return;
        }
        QWidget::mousePressEvent(event);
    }
    void mouseDoubleClickEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            window()->isMaximized() ? window()->showNormal() : window()->showMaximized();
        }
    }
};

// Cover art with rounded top corners and a dark fade at the bottom for the overlay labels.
class CoverWidget : public QWidget {
   public:
    explicit CoverWidget(QWidget* parent) : QWidget(parent), m_pixmap(":/beyonder/cover.png") {}

   protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);

        const QRectF r = rect();
        const qreal radius = 10;
        QPainterPath clip;
        clip.moveTo(r.left(), r.bottom());
        clip.lineTo(r.left(), r.top() + radius);
        clip.quadTo(r.left(), r.top(), r.left() + radius, r.top());
        clip.lineTo(r.right() - radius, r.top());
        clip.quadTo(r.right(), r.top(), r.right(), r.top() + radius);
        clip.lineTo(r.right(), r.bottom());
        clip.closeSubpath();
        p.setClipPath(clip);

        p.fillRect(r, QColor("#050506"));
        if (!m_pixmap.isNull()) {
            const QSize scaled = m_pixmap.size().scaled(size(), Qt::KeepAspectRatioByExpanding);
            const QRect target((width() - scaled.width()) / 2, (height() - scaled.height()) / 2, scaled.width(), scaled.height());
            p.drawPixmap(target, m_pixmap);
        }
        QLinearGradient fade(r.topLeft(), r.bottomLeft());
        fade.setColorAt(0.0, QColor(0, 0, 0, 90));
        fade.setColorAt(0.18, QColor(0, 0, 0, 0));
        fade.setColorAt(0.72, QColor(0, 0, 0, 0));
        fade.setColorAt(1.0, QColor(10, 10, 11, 230));
        p.fillRect(r, fade);
    }

   private:
    QPixmap m_pixmap;
};

}  // namespace

// =====================================================================================================================
// Construction
// =====================================================================================================================

BeyonderWindow::BeyonderWindow(QWidget* parent) : QMainWindow(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setWindowTitle(BuildConfig.LAUNCHER_DISPLAYNAME);
    setWindowIcon(APPLICATION->logo());
    setMinimumSize(980, 620);

    m_root = new FrameRoot(this);
    m_root->setObjectName("bRoot");
    applyTheme();
    setCentralWidget(m_root);

    auto* rootLayout = new QVBoxLayout(m_root);
    rootLayout->setContentsMargins(FrameRoot::Margin, FrameRoot::Margin, FrameRoot::Margin, FrameRoot::Margin);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(buildTitleBar());

    auto* middle = new QWidget(m_root);
    auto* middleLayout = new QHBoxLayout(middle);
    middleLayout->setContentsMargins(0, 0, 0, 0);
    middleLayout->setSpacing(0);
    middleLayout->addWidget(buildSidebar());

    m_pages = new QStackedWidget(middle);
    m_pages->setObjectName("bContent");
    m_pages->setAttribute(Qt::WA_StyledBackground);
    m_pages->addWidget(buildLibraryPage());
    m_pages->addWidget(buildNewsPage());
    m_accountPage = new BeyonderAccountPage(this);
    m_settingsPage = new BeyonderSettingsPage(this);
    m_pages->addWidget(m_accountPage);
    m_pages->addWidget(m_settingsPage);
    middleLayout->addWidget(m_pages, 1);
    rootLayout->addWidget(middle, 1);

    rootLayout->addWidget(buildStatusBar());
    buildToast();

    const auto geometry = APPLICATION->settings()->get("BeyonderWindowGeometry").toString();
    if (geometry.isEmpty() || !restoreGeometry(QByteArray::fromBase64(geometry.toUtf8()))) {
        resize(1200, 760);
    }

    auto* accounts = APPLICATION->accounts();
    connect(accounts, &AccountList::listChanged, this, &BeyonderWindow::rebuildAccountMenu);
    connect(accounts, &AccountList::defaultAccountChanged, this, &BeyonderWindow::rebuildAccountMenu);
    rebuildAccountMenu();

    m_pingTimer = new QTimer(this);
    m_pingTimer->setInterval(60 * 1000);
    connect(m_pingTimer, &QTimer::timeout, this, &BeyonderWindow::pingServer);
    m_pingTimer->start();

    // Team-style role/whitelist changes show up within ~90 seconds.
    m_accessTimer = new QTimer(this);
    m_accessTimer->setInterval(90 * 1000);
    connect(m_accessTimer, &QTimer::timeout, this, &BeyonderWindow::checkAccess);
    m_accessTimer->start();
    connect(accounts, &AccountList::defaultAccountChanged, this, &BeyonderWindow::checkAccess);

    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(qBound(5, APPLICATION->settings()->get("BeyonderCheckIntervalMin").toInt(), 60) * 60 * 1000);
    connect(m_updateTimer, &QTimer::timeout, this, &BeyonderWindow::checkForUpdates);
    m_updateTimer->start();

    watchInstance(findInstance());
    updateNews();
    checkForUpdates();
}

BeyonderWindow::~BeyonderWindow() = default;

QWidget* BeyonderWindow::buildTitleBar()
{
    auto* bar = new DragBar(m_root);
    bar->setObjectName("bTitleBar");
    bar->setAttribute(Qt::WA_StyledBackground);
    bar->setFixedHeight(54);
    auto* l = new QHBoxLayout(bar);
    l->setContentsMargins(16, 0, 8, 0);
    l->setSpacing(10);

    auto* logo = new QLabel(bar);
    logo->setPixmap(APPLICATION->logo().pixmap(30, 30));
    logo->setAttribute(Qt::WA_TransparentForMouseEvents);
    l->addWidget(logo);
    auto* brand = makeLabel("BEYONDER", "bBrand", bar);
    brand->setAttribute(Qt::WA_TransparentForMouseEvents);
    l->addWidget(brand);
    auto* pill = makeLabel(tr("Launcher"), "bPill", bar);
    pill->setAttribute(Qt::WA_TransparentForMouseEvents);
    l->addWidget(pill, 0, Qt::AlignVCenter);
    l->addStretch(1);

    m_titleUpdateButton = new QPushButton(bar);
    m_titleUpdateButton->setObjectName("bGreenButton");
    m_titleUpdateButton->setIcon(lineIcon(Icon::Download, "#052e12", 16));
    m_titleUpdateButton->setCursor(Qt::PointingHandCursor);
    m_titleUpdateButton->hide();
    connect(m_titleUpdateButton, &QPushButton::clicked, this, [this] { startInstall(false); });
    l->addWidget(m_titleUpdateButton);

    m_accountButton = new QToolButton(bar);
    m_accountButton->setObjectName("bAccount");
    m_accountButton->setPopupMode(QToolButton::InstantPopup);
    m_accountButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_accountButton->setIconSize(QSize(24, 24));
    m_accountButton->setCursor(Qt::PointingHandCursor);
    m_accountMenu = new QMenu(m_accountButton);
    m_accountButton->setMenu(m_accountMenu);
    l->addWidget(m_accountButton);
    l->addSpacing(8);

    auto winButton = [bar](const char* icon, const char* name) {
        auto* b = new QToolButton(bar);
        b->setObjectName(name);
        b->setIcon(lineIcon(icon, "#b4b4bc", 16));
        b->setIconSize(QSize(16, 16));
        b->setFixedSize(40, 32);
        return b;
    };
    auto* minimize = winButton(Icon::Minimize, "bWinButton");
    connect(minimize, &QToolButton::clicked, this, &QWidget::showMinimized);
    m_maximizeButton = winButton(Icon::Maximize, "bWinButton");
    connect(m_maximizeButton, &QToolButton::clicked, this, [this] { isMaximized() ? showNormal() : showMaximized(); });
    auto* close = winButton(Icon::Close, "bCloseButton");
    connect(close, &QToolButton::clicked, this, &QWidget::close);
    l->addWidget(minimize);
    l->addWidget(m_maximizeButton);
    l->addWidget(close);
    return bar;
}

QWidget* BeyonderWindow::buildSidebar()
{
    auto* side = new QWidget(m_root);
    side->setObjectName("bSidebar");
    side->setAttribute(Qt::WA_StyledBackground);
    side->setFixedWidth(232);
    auto* l = new QVBoxLayout(side);
    l->setContentsMargins(12, 16, 12, 14);
    l->setSpacing(4);

    l->addWidget(makeLabel(tr("MENU"), "bSection", side));
    m_nav = new QButtonGroup(this);
    m_nav->setExclusive(true);
    auto navButton = [side, l](const char* icon, const QString& text) {
        auto* b = new QPushButton(lineIcon(icon, "#c9c9d0", 18), "  " + text, side);
        b->setObjectName("bNav");
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setIconSize(QSize(18, 18));
        l->addWidget(b);
        return b;
    };
    auto* library = navButton(Icon::Home, tr("Library"));
    auto* news = navButton(Icon::News, tr("News"));
    auto* account = navButton(Icon::User, tr("Account"));
    auto* settings = navButton(Icon::Settings, tr("Settings"));
    m_nav->addButton(library, 0);
    m_nav->addButton(news, 1);
    m_nav->addButton(account, 2);
    m_nav->addButton(settings, 3);
    library->setChecked(true);
    connect(m_nav, &QButtonGroup::idClicked, this, [this](int id) { m_pages->setCurrentIndex(id); });
    auto* advanced = navButton(Icon::Grid, tr("Advanced"));
    advanced->setCheckable(false);
    advanced->setToolTip(tr("Open the full launcher: instances, mods, logs"));
    connect(advanced, &QPushButton::clicked, this, &BeyonderWindow::classicViewRequested);

    auto* divider = new QFrame(side);
    divider->setFixedHeight(1);
    divider->setStyleSheet("background: #1d1d21;");
    l->addSpacing(10);
    l->addWidget(divider);
    l->addSpacing(10);

    // Quick launch
    l->addWidget(makeLabel(tr("QUICK LAUNCH"), "bSection", side));
    auto* quick = new QPushButton(side);
    quick->setObjectName("bQuick");
    quick->setCursor(Qt::PointingHandCursor);
    quick->setMinimumHeight(58);
    auto* ql = new QHBoxLayout(quick);
    ql->setContentsMargins(8, 8, 8, 8);
    ql->setSpacing(10);
    auto* qIcon = new QLabel(quick);
    qIcon->setPixmap(APPLICATION->logo().pixmap(34, 34));
    ql->addWidget(qIcon);
    auto* qText = new QVBoxLayout();
    qText->setSpacing(1);
    qText->addWidget(makeLabel(tr("Beyonder"), "bQuickTitle", quick));
    auto* statusRow = new QHBoxLayout();
    statusRow->setSpacing(5);
    m_quickDot = new QLabel("●", quick);
    m_quickDot->setStyleSheet(QString("color: %1; font-size: 9px;").arg(Color::Muted));
    m_quickStatus = makeLabel(tr("Checking…"), "bQuickStatus", quick);
    statusRow->addWidget(m_quickDot);
    statusRow->addWidget(m_quickStatus, 1);
    qText->addLayout(statusRow);
    ql->addLayout(qText, 1);
    for (auto* child : quick->findChildren<QLabel*>()) {
        child->setAttribute(Qt::WA_TransparentForMouseEvents);
    }
    connect(quick, &QPushButton::clicked, this, &BeyonderWindow::onPlayClicked);
    l->addWidget(quick);

    l->addStretch(1);

    // Community
    l->addWidget(makeLabel(tr("COMMUNITY"), "bSection", side));
    auto* community = new QHBoxLayout();
    community->setSpacing(8);
    m_discordButton = new QPushButton(lineIcon(Icon::Chat, "#c9c9d0", 16), tr(" Discord"), side);
    m_websiteButton = new QPushButton(lineIcon(Icon::Globe, "#c9c9d0", 16), tr(" Website"), side);
    for (auto* b : { m_discordButton, m_websiteButton }) {
        b->setObjectName("bCommunity");
        b->setCursor(Qt::PointingHandCursor);
        b->setEnabled(false);
        community->addWidget(b);
    }
    connect(m_discordButton, &QPushButton::clicked, this, [this] {
        if (m_manifest && m_manifest->discordUrl.isValid())
            QDesktopServices::openUrl(m_manifest->discordUrl);
    });
    connect(m_websiteButton, &QPushButton::clicked, this, [this] {
        if (m_manifest && m_manifest->websiteUrl.isValid())
            QDesktopServices::openUrl(m_manifest->websiteUrl);
    });
    l->addLayout(community);
    l->addSpacing(10);

    // Update box
    m_sideUpdateButton = new QPushButton(side);
    m_sideUpdateButton->setObjectName("bSideUpdate");
    m_sideUpdateButton->setCursor(Qt::PointingHandCursor);
    auto* ul = new QVBoxLayout(m_sideUpdateButton);
    ul->setContentsMargins(10, 9, 10, 9);
    ul->setSpacing(2);
    auto* uTitleRow = new QHBoxLayout();
    auto* uIcon = new QLabel(m_sideUpdateButton);
    uIcon->setPixmap(lineIcon(Icon::Refresh, Color::Green, 16).pixmap(16, 16));
    uTitleRow->addWidget(uIcon);
    uTitleRow->addWidget(makeLabel(tr("Update Available"), "bSideUpdateTitle", m_sideUpdateButton), 1);
    ul->addLayout(uTitleRow);
    m_sideUpdateText = makeLabel(QString(), "bSideUpdateText", m_sideUpdateButton);
    m_sideUpdateText->setWordWrap(true);
    ul->addWidget(m_sideUpdateText);
    for (auto* child : m_sideUpdateButton->findChildren<QLabel*>()) {
        child->setAttribute(Qt::WA_TransparentForMouseEvents);
    }
    m_sideUpdateButton->setMinimumHeight(62);
    m_sideUpdateButton->hide();
    connect(m_sideUpdateButton, &QPushButton::clicked, this, [this] { startInstall(false); });
    l->addWidget(m_sideUpdateButton);
    return side;
}

QWidget* BeyonderWindow::buildLibraryPage()
{
    auto* page = new QWidget();
    auto* l = new QVBoxLayout(page);
    l->setContentsMargins(28, 24, 28, 20);
    l->setSpacing(16);

    auto* headerRow = new QHBoxLayout();
    headerRow->setSpacing(10);
    headerRow->addWidget(makeLabel(tr("Library"), "bH1", page));
    headerRow->addWidget(makeLabel("1", "bCount", page), 0, Qt::AlignVCenter);
    headerRow->addStretch(1);
    l->addLayout(headerRow);
    auto* sub = makeLabel(tr("The official Beyonder release, ready to install, configure, and launch."), "bSubtle", page);
    l->addWidget(sub);
    l->addSpacing(4);

    // Server strip
    auto* strip = new QFrame(page);
    strip->setObjectName("bStrip");
    auto* sl = new QHBoxLayout(strip);
    sl->setContentsMargins(16, 12, 12, 12);
    sl->setSpacing(10);
    m_serverDot = new QLabel("●", strip);
    m_serverLabel = makeLabel(tr("Checking server…"), "bStripText", strip);
    sl->addWidget(m_serverDot);
    sl->addWidget(m_serverLabel, 1);
    m_accessLabel = new QLabel(strip);
    sl->addWidget(m_accessLabel);
    auto* refresh = new QPushButton(lineIcon(Icon::Refresh, "#d6d6dc", 14), tr(" Refresh"), strip);
    refresh->setObjectName("bGhost");
    refresh->setCursor(Qt::PointingHandCursor);
    connect(refresh, &QPushButton::clicked, this, [this] {
        checkForUpdates();
        pingServer();
        checkAccess();
    });
    sl->addWidget(refresh);
    l->addWidget(strip);

    // Cards
    auto* cards = new QHBoxLayout();
    cards->setSpacing(18);
    auto* gameCard = buildGameCard();
    cards->addWidget(gameCard, 0, Qt::AlignTop);

    m_communityCard = new QPushButton(page);
    m_communityCard->setObjectName("bDashed");
    m_communityCard->setCursor(Qt::PointingHandCursor);
    m_communityCard->setFixedSize(gameCard->width(), gameCard->sizeHint().height());
    auto* cl = new QVBoxLayout(m_communityCard);
    cl->addStretch(1);
    auto* cIcon = new QLabel(m_communityCard);
    cIcon->setPixmap(lineIcon(Icon::Chat, "#b4b4bc", 28).pixmap(28, 28));
    cIcon->setAlignment(Qt::AlignCenter);
    cl->addWidget(cIcon);
    auto* cTitle = makeLabel(tr("Join the Beyonder Discord"), "bCardTitle", m_communityCard);
    cTitle->setAlignment(Qt::AlignCenter);
    cTitle->setWordWrap(true);
    cTitle->setStyleSheet("font-size: 13px;");
    cl->addWidget(cTitle);
    auto* cText = makeLabel(tr("Announcements, patch notes, whitelist help and faction recruitment."), "bCardSub", m_communityCard);
    cText->setAlignment(Qt::AlignCenter);
    cText->setWordWrap(true);
    cl->addWidget(cText);
    cl->addStretch(1);
    for (auto* child : m_communityCard->findChildren<QLabel*>()) {
        child->setAttribute(Qt::WA_TransparentForMouseEvents);
    }
    connect(m_communityCard, &QPushButton::clicked, m_discordButton, &QPushButton::click);
    cards->addWidget(m_communityCard, 0, Qt::AlignTop);
    cards->addStretch(1);
    l->addLayout(cards);
    l->addStretch(1);

    auto* divider = new QFrame(page);
    divider->setFixedHeight(1);
    divider->setStyleSheet("background: #1d1d21;");
    l->addWidget(divider);
    auto* footer = new QHBoxLayout();
    footer->setSpacing(8);
    auto* fIcon = new QLabel(page);
    fIcon->setPixmap(lineIcon(Icon::Check, Color::Muted, 14).pixmap(14, 14));
    footer->addWidget(fIcon);
    footer->addWidget(makeLabel(tr("Beyonder is an unofficial fan project inspired by Lord of the Mysteries."), "bSubtle", page), 1);
    l->addLayout(footer);
    return page;
}

QWidget* BeyonderWindow::buildGameCard()
{
    auto* card = new QFrame();
    card->setObjectName("bCard");
    card->setFixedWidth(300);
    auto* l = new QVBoxLayout(card);
    l->setContentsMargins(1, 1, 1, 14);
    l->setSpacing(0);

    m_cover = new CoverWidget(card);
    m_cover->setFixedHeight(318);
    auto* coverLayout = new QVBoxLayout(m_cover);
    coverLayout->setContentsMargins(12, 12, 12, 12);
    auto* top = new QHBoxLayout();
    m_editionButton = new QToolButton(m_cover);
    m_editionButton->setObjectName("bEdition");
    m_editionButton->setPopupMode(QToolButton::InstantPopup);
    m_editionButton->setCursor(Qt::PointingHandCursor);
    m_editionButton->setToolTip(tr("Choose an edition"));
    m_editionMenu = new QMenu(m_editionButton);
    m_editionButton->setMenu(m_editionMenu);
    connect(m_editionMenu, &QMenu::aboutToShow, this, &BeyonderWindow::rebuildEditionMenu);
    m_stateBadge = makeLabel(QString(), "bBadge", m_cover);
    top->addWidget(m_editionButton);
    top->addStretch(1);
    top->addWidget(m_stateBadge);
    coverLayout->addLayout(top);
    coverLayout->addStretch(1);
    auto* bottom = new QHBoxLayout();
    m_installedLabel = makeLabel(QString(), "bCoverText", m_cover);
    m_playtimeBadge = makeLabel(QString(), "bBadge", m_cover);
    bottom->addWidget(m_installedLabel);
    bottom->addStretch(1);
    bottom->addWidget(m_playtimeBadge);
    coverLayout->addLayout(bottom);
    l->addWidget(m_cover);

    auto* body = new QVBoxLayout();
    body->setContentsMargins(14, 14, 14, 0);
    body->setSpacing(3);
    m_cardTitle = makeLabel(tr("Beyonder"), "bCardTitle", card);
    m_cardSubtitle = makeLabel(QString(), "bCardSub", card);
    body->addWidget(m_cardTitle);
    body->addWidget(m_cardSubtitle);
    body->addSpacing(12);

    auto* buttons = new QHBoxLayout();
    buttons->setSpacing(8);
    m_playButton = new QPushButton(tr("PLAY"), card);
    m_playButton->setObjectName("bPlay");
    m_playButton->setCursor(Qt::PointingHandCursor);
    m_playButton->setIconSize(QSize(16, 16));
    connect(m_playButton, &QPushButton::clicked, this, &BeyonderWindow::onPlayClicked);
    buttons->addWidget(m_playButton, 1);

    m_folderButton = new QToolButton(card);
    m_folderButton->setObjectName("bIconButton");
    m_folderButton->setIcon(lineIcon(Icon::Folder, "#d6d6dc", 18));
    m_folderButton->setToolTip(tr("Open the game folder"));
    m_folderButton->setCursor(Qt::PointingHandCursor);
    connect(m_folderButton, &QToolButton::clicked, this, [this] {
        if (auto* inst = findInstance())
            DesktopServices::openPath(inst->gameRoot(), true);
    });
    buttons->addWidget(m_folderButton);

    m_gameSettingsButton = new QToolButton(card);
    m_gameSettingsButton->setObjectName("bIconButton");
    m_gameSettingsButton->setIcon(lineIcon(Icon::Sliders, "#d6d6dc", 18));
    m_gameSettingsButton->setToolTip(tr("Game settings: Java, memory, mods, logs"));
    m_gameSettingsButton->setCursor(Qt::PointingHandCursor);
    connect(m_gameSettingsButton, &QToolButton::clicked, this, [this] {
        if (auto* inst = findInstance())
            APPLICATION->showInstanceWindow(inst);
    });
    buttons->addWidget(m_gameSettingsButton);
    body->addLayout(buttons);
    l->addLayout(body);
    return card;
}

QWidget* BeyonderWindow::buildNewsPage()
{
    auto* page = new QWidget();
    auto* l = new QVBoxLayout(page);
    l->setContentsMargins(28, 24, 28, 24);
    l->setSpacing(14);
    l->addWidget(makeLabel(tr("News"), "bH1", page));
    l->addWidget(makeLabel(tr("Announcements and patch notes from the Beyonder team."), "bSubtle", page));
    m_newsView = new QTextBrowser(page);
    m_newsView->setObjectName("bNewsView");
    m_newsView->setOpenExternalLinks(true);
    m_newsView->document()->setDefaultStyleSheet(
        "h2 { font-size: 18px; margin: 0 0 6px 0; } h3 { font-size: 15px; margin: 18px 0 2px 0; }"
        ".date { color: #8b8b93; font-size: 11px; } a { color: #f07a80; } p { line-height: 140%; }");
    l->addWidget(m_newsView, 1);
    return page;
}

QWidget* BeyonderWindow::buildStatusBar()
{
    auto* bar = new QWidget(m_root);
    bar->setObjectName("bStatusBar");
    bar->setAttribute(Qt::WA_StyledBackground);
    bar->setFixedHeight(36);
    auto* l = new QHBoxLayout(bar);
    l->setContentsMargins(16, 0, 4, 0);
    l->setSpacing(8);

    m_statusIcon = new QLabel(bar);
    l->addWidget(m_statusIcon);
    m_statusText = makeLabel(QString(), "bStatusText", bar);
    l->addWidget(m_statusText, 1);

    m_statusProgress = new QProgressBar(bar);
    m_statusProgress->setTextVisible(false);
    m_statusProgress->setFixedWidth(180);
    m_statusProgress->hide();
    l->addWidget(m_statusProgress);

    m_ramButton = new QToolButton(bar);
    m_ramButton->setObjectName("bFooterButton");
    m_ramButton->setIcon(lineIcon(Icon::Chip, Color::Muted, 14));
    m_ramButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_ramButton->setPopupMode(QToolButton::InstantPopup);
    m_ramButton->setCursor(Qt::PointingHandCursor);
    m_ramButton->setToolTip(tr("Memory given to Minecraft"));
    m_ramMenu = new QMenu(m_ramButton);
    m_ramButton->setMenu(m_ramMenu);
    connect(m_ramMenu, &QMenu::aboutToShow, this, &BeyonderWindow::rebuildRamMenu);
    l->addWidget(m_ramButton);

    auto* version = makeLabel(QString("v%1").arg(BuildConfig.printableVersionString()), "bFooterButton", bar);
    l->addWidget(version);

    auto* grip = new QSizeGrip(bar);
    grip->setFixedSize(14, 14);
    l->addWidget(grip, 0, Qt::AlignBottom | Qt::AlignRight);
    return bar;
}

void BeyonderWindow::buildToast()
{
    m_toast = new QFrame(m_root);
    m_toast->setObjectName("bToast");
    m_toast->setFixedWidth(330);
    auto* shadow = new QGraphicsDropShadowEffect(m_toast);
    shadow->setBlurRadius(32);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(0, 0, 0, 160));
    m_toast->setGraphicsEffect(shadow);

    auto* l = new QVBoxLayout(m_toast);
    l->setContentsMargins(16, 14, 12, 14);
    l->setSpacing(12);
    auto* top = new QHBoxLayout();
    top->setSpacing(12);
    auto* icon = new QLabel(m_toast);
    icon->setObjectName("bToastIcon");
    icon->setFixedSize(34, 34);
    icon->setAlignment(Qt::AlignCenter);
    icon->setPixmap(lineIcon(Icon::Download, Color::Green, 18).pixmap(18, 18));
    top->addWidget(icon, 0, Qt::AlignTop);
    auto* texts = new QVBoxLayout();
    texts->setSpacing(2);
    texts->addWidget(makeLabel(tr("Update Available"), "bToastTitle", m_toast));
    m_toastText = makeLabel(QString(), "bToastText", m_toast);
    m_toastText->setWordWrap(true);
    texts->addWidget(m_toastText);
    top->addLayout(texts, 1);
    auto* close = new QToolButton(m_toast);
    close->setObjectName("bWinButton");
    close->setIcon(lineIcon(Icon::Close, Color::Muted, 14));
    close->setCursor(Qt::PointingHandCursor);
    top->addWidget(close, 0, Qt::AlignTop);
    l->addLayout(top);

    auto* buttons = new QHBoxLayout();
    buttons->setSpacing(8);
    auto* now = new QPushButton(tr("Update Now"), m_toast);
    now->setObjectName("bGreenButton");
    now->setCursor(Qt::PointingHandCursor);
    auto* later = new QPushButton(tr("Later"), m_toast);
    later->setObjectName("bGhost");
    later->setCursor(Qt::PointingHandCursor);
    buttons->addWidget(now, 1);
    buttons->addWidget(later);
    l->addLayout(buttons);

    auto dismiss = [this] {
        if (m_manifest)
            m_toastDismissedFor = m_manifest->packVersion;
        showToast(false);
    };
    connect(close, &QToolButton::clicked, this, dismiss);
    connect(later, &QPushButton::clicked, this, dismiss);
    connect(now, &QPushButton::clicked, this, [this] {
        showToast(false);
        startInstall(false);
    });
    m_toast->hide();
}

// =====================================================================================================================
// Window events
// =====================================================================================================================

void BeyonderWindow::closeEvent(QCloseEvent* event)
{
    if (m_installTask && m_installTask->isRunning()) {
        auto answer = QMessageBox::question(this, tr("Update in progress"), tr("Beyonder is still being installed. Stop and close the launcher?"));
        if (answer != QMessageBox::Yes) {
            event->ignore();
            return;
        }
        m_installTask->abort();
    }
    APPLICATION->settings()->set("BeyonderWindowGeometry", QString::fromUtf8(saveGeometry().toBase64()));
    emit isClosing();
    event->accept();
}

void BeyonderWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::WindowStateChange && m_root) {
        const int m = static_cast<FrameRoot*>(m_root)->margin();
        m_root->layout()->setContentsMargins(m, m, m, m);
        m_maximizeButton->setIcon(lineIcon(isMaximized() ? Icon::Restore : Icon::Maximize, "#b4b4bc", 16));
        m_root->update();
    }
    QMainWindow::changeEvent(event);
}

void BeyonderWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    positionToast();
}

void BeyonderWindow::positionToast()
{
    if (!m_toast)
        return;
    m_toast->adjustSize();
    const int x = m_root->width() - m_toast->width() - 24;
    const int y = m_root->height() - m_toast->height() - 36 - 20;
    m_toast->move(x, y);
}

void BeyonderWindow::showToast(bool show)
{
    if (show) {
        positionToast();
        m_toast->show();
        m_toast->raise();
    } else {
        m_toast->hide();
    }
}

// =====================================================================================================================
// Manifest / update check
// =====================================================================================================================

void BeyonderWindow::checkForUpdates()
{
    if (m_state == State::Installing || m_state == State::Running) {
        return;
    }
    setState(State::Checking, tr("Checking for updates…"));

    m_manifestJob.reset(new NetJob("Beyonder manifest", APPLICATION->network()));
    // AcceptLocalFiles lets a test build point at a file:// manifest.
    auto [request, response] = Net::Request::makeByteArray(QUrl(BuildConfig.BEYONDER_MANIFEST_URL), Net::Request::Option::AcceptLocalFiles);
    m_manifestJob->addNetAction(request);
    connect(m_manifestJob.get(), &NetJob::succeeded, this, [this, response] {
        // Copy out before the job (which owns the buffer) goes away.
        const QByteArray manifestBytes = *response;
        m_manifestJob.reset();
        onManifestDownloaded(manifestBytes);
    });
    connect(m_manifestJob.get(), &NetJob::failed, this, [this](const QString& reason) {
        m_manifestJob.reset();
        onManifestFailed(reason);
    });
    m_manifestJob->start();
}

void BeyonderWindow::onManifestDownloaded(const QByteArray& manifestBytes)
{
    QString error;
    m_manifest = BeyonderManifest::parse(manifestBytes, error);
    m_manifestError = error;

    if (m_manifest && !m_manifest->minLauncherVersion.isEmpty() &&
        Version(BuildConfig.versionString()) < Version(m_manifest->minLauncherVersion)) {
        m_manifestError = tr("This launcher is too old for the current Beyonder release. Please download the latest launcher.");
        m_manifest.reset();
    }

    m_discordButton->setEnabled(m_manifest && m_manifest->discordUrl.isValid());
    m_websiteButton->setEnabled(m_manifest && m_manifest->websiteUrl.isValid());
    m_communityCard->setVisible(m_manifest && m_manifest->discordUrl.isValid());

    updateNews();
    refreshState();
    pingServer();
    checkAccess();
}

void BeyonderWindow::onManifestFailed(const QString& reason)
{
    qWarning() << "Beyonder manifest download failed:" << reason;
    m_manifest.reset();
    m_manifestError = tr("Couldn't reach the update server.");
    updateNews();
    refreshState();
    m_serverStatus = ServerStatus::Unknown;
    updateServerStrip();
}

void BeyonderWindow::updateNews()
{
    QString html;
    if (!m_manifest) {
        if (!m_manifestError.isEmpty()) {
            html = tr("<h2>The fog is thick tonight.</h2><p>%1 Check your internet connection, then press Refresh on the Library page.</p>")
                       .arg(m_manifestError.toHtmlEscaped());
        } else {
            html = tr("<p>Contacting the gray fog…</p>");
        }
    } else {
        html = m_manifest->motd.isEmpty() ? tr("<h2>Welcome, Beyonder.</h2>") : m_manifest->motd;
        for (const auto& item : m_manifest->news) {
            html += QString("<h3>%1</h3>").arg(item.url.isValid() ? QString("<a href=\"%1\">%2</a>").arg(item.url.toString(), item.title.toHtmlEscaped())
                                                                 : item.title.toHtmlEscaped());
            if (!item.date.isEmpty())
                html += QString("<div class=\"date\">%1</div>").arg(item.date.toHtmlEscaped());
            html += item.body;
        }
    }
    m_newsView->setHtml(html);
}

void BeyonderWindow::refreshState()
{
    auto* inst = findInstance();
    watchInstance(inst);

    if (m_state == State::Installing) {
        updateUi();
        return;
    }
    if (inst && inst->isRunning()) {
        setState(State::Running);
        return;
    }
    if (!m_manifest) {
        if (inst) {
            setState(State::Ready, m_manifestError + " " + tr("You can still play the installed version."));
        } else {
            setState(State::Error, m_manifestError);
        }
        return;
    }
    if (!inst) {
        setState(State::NeedsInstall);
    } else if (inst->getManagedPackVersionID() != m_manifest->packVersion || installedEditionId() != currentEditionId()) {
        setState(State::NeedsUpdate);
        // A new release (not an edition switch the player hasn't confirmed) installs by itself when auto-update is on.
        if (inst->getManagedPackVersionID() != m_manifest->packVersion && APPLICATION->settings()->get("BeyonderAutoUpdate").toBool()) {
            QTimer::singleShot(0, this, [this] { startInstall(false); });
        }
    } else {
        setState(State::Ready);
    }
}

void BeyonderWindow::setState(State state, const QString& message)
{
    m_state = state;
    m_statusMessage = message;
    if (state != State::Installing) {
        m_installPercent = -1;
    }
    updateUi();
}

// =====================================================================================================================
// UI state
// =====================================================================================================================

void BeyonderWindow::updateUi()
{
    auto* inst = findInstance();
    const QString installed = inst ? inst->getManagedPackVersionID() : QString();
    const QString latest = m_manifest ? m_manifest->packVersion : QString();
    const QString season = (m_manifest && !m_manifest->season.isEmpty()) ? m_manifest->season : tr("Season 0");
    const auto* edition = m_manifest ? m_manifest->edition(currentEditionId()) : nullptr;
    const QString editionName = edition ? edition->name : tr("Standard");
    const bool editionSwitch = inst && m_manifest && installed == latest && installedEditionId() != currentEditionId();

    QString playText = tr("PLAY");
    QString playIcon = Icon::Play;
    bool playEnabled = true;
    bool running = false;
    QString badgeText;
    const char* badgeStyle = "bBadge";
    QString subtitle;
    QString quick;
    QString dotColor = Color::Muted;
    QString status = m_statusMessage;
    bool busy = false;

    switch (m_state) {
        case State::Checking:
            playText = tr("CHECKING…");
            playEnabled = false;
            badgeText = tr("Checking");
            subtitle = tr("Checking for updates…");
            quick = tr("Checking…");
            busy = true;
            break;
        case State::NeedsInstall:
            playText = tr("INSTALL");
            playIcon = Icon::Download;
            badgeText = tr("Not installed");
            subtitle = tr("Not installed • %1").arg(latest);
            quick = tr("Not installed");
            if (status.isEmpty())
                status = tr("Beyonder isn't installed yet.");
            break;
        case State::NeedsUpdate:
            playText = editionSwitch ? tr("SWITCH") : tr("UPDATE");
            playIcon = Icon::Download;
            badgeText = editionSwitch ? tr("Switch") : tr("Update");
            badgeStyle = "bBadgeAmber";
            subtitle = editionSwitch ? tr("Switch to the %1 edition").arg(editionName) : tr("Update to %1 available").arg(latest);
            quick = editionSwitch ? tr("Edition change pending") : tr("Update available");
            dotColor = Color::Amber;
            if (status.isEmpty())
                status = editionSwitch ? tr("Press Switch to change to the %1 edition. Your worlds and settings are kept.").arg(editionName)
                                       : tr("Beyonder %1 is ready to install.").arg(latest);
            break;
        case State::Ready:
            badgeText = tr("Ready");
            badgeStyle = "bBadgeGreen";
            subtitle = tr("Ready to play • %1 edition").arg(editionName);
            quick = tr("Ready");
            dotColor = Color::Green;
            switch (m_access) {
                case Access::DiscordRequired:
                    playText = tr("LINK DISCORD");
                    playIcon = Icon::Chat;
                    badgeText = tr("Link Discord");
                    badgeStyle = "bBadgeAmber";
                    quick = tr("Discord not linked");
                    dotColor = Color::Amber;
                    break;
                case Access::NotWhitelisted:
                    playText = tr("NO ACCESS");
                    playIcon = Icon::Info;
                    badgeText = tr("No Access");
                    badgeStyle = "bBadgeAmber";
                    quick = tr("Not whitelisted");
                    dotColor = Color::Amber;
                    break;
                case Access::LostControl:
                    playText = tr("LOST CONTROL");
                    playIcon = Icon::Info;
                    badgeText = tr("Lost Control");
                    badgeStyle = "bBadgeRed";
                    subtitle = tr("Your Beyonder lost control");
                    quick = tr("Lost Control");
                    dotColor = Color::Red;
                    break;
                case Access::Banned:
                    playText = tr("BANNED");
                    playIcon = Icon::Info;
                    badgeText = tr("Banned");
                    badgeStyle = "bBadgeRed";
                    quick = tr("Banned");
                    dotColor = Color::Red;
                    break;
                default:
                    break;
            }
            if (accessBlocksPlay() && status.isEmpty() && !m_accessMessage.isEmpty())
                status = m_accessMessage;
            break;
        case State::Installing:
            playText = m_installPercent >= 0 ? tr("INSTALLING %1%").arg(m_installPercent) : tr("INSTALLING…");
            playEnabled = false;
            badgeText = tr("Installing");
            badgeStyle = "bBadgeAmber";
            subtitle = tr("Downloading the game and mods…");
            quick = tr("Installing…");
            dotColor = Color::Amber;
            busy = true;
            break;
        case State::Running:
            playText = tr("Running");
            playIcon = Icon::Check;
            playEnabled = false;
            running = true;
            badgeText = tr("Running");
            badgeStyle = "bBadgeGreen";
            subtitle = tr("In game • %1").arg(season);
            quick = tr("Running");
            dotColor = Color::Green;
            break;
        case State::Error:
            playText = tr("RETRY");
            playIcon = Icon::Refresh;
            badgeText = tr("Offline");
            badgeStyle = "bBadgeRed";
            subtitle = tr("Couldn't reach the update server");
            quick = tr("Offline");
            dotColor = Color::Red;
            break;
    }

    // Game card
    m_playButton->setText("  " + playText);
    m_playButton->setIcon(lineIcon(playIcon, running ? Color::Green : (!playEnabled ? "#6b6b74" : (m_state == State::Ready && accessBlocksPlay() ? "#f87171" : "#ffffff")),
                                   16, playIcon == Icon::Play));
    m_playButton->setEnabled(playEnabled);
    m_playButton->setProperty("running", running);
    const bool blocked = m_state == State::Ready && accessBlocksPlay();
    m_playButton->setProperty("blocked", blocked);
    restyle(m_playButton);
    m_stateBadge->setText(badgeText);
    m_stateBadge->setObjectName(badgeStyle);
    restyle(m_stateBadge);
    QString editionText = latest.isEmpty() ? editionName : QString("%1 • %2").arg(editionName, latest);
    if (m_manifest && m_manifest->editions.size() > 1)
        editionText += QString::fromUtf8("  ▾");
    m_editionButton->setText(editionText);
    m_editionButton->setEnabled(m_manifest && m_manifest->editions.size() > 1 && m_state != State::Installing);
    m_installedLabel->setText(installed.isEmpty() ? tr("Not installed") : tr("Installed %1").arg(installed));
    if (inst && inst->totalTimePlayed() > 0) {
        m_playtimeBadge->setText(tr("%1 played").arg(humanHours(inst->totalTimePlayed())));
        m_playtimeBadge->show();
    } else {
        m_playtimeBadge->hide();
    }
    m_cardSubtitle->setText(subtitle);
    m_folderButton->setEnabled(inst != nullptr);
    m_gameSettingsButton->setEnabled(inst != nullptr);

    // Sidebar
    m_quickStatus->setText(quick);
    m_quickDot->setStyleSheet(QString("color: %1; font-size: 9px;").arg(dotColor));
    const bool updateAvailable = m_state == State::NeedsUpdate && !editionSwitch;
    m_sideUpdateButton->setVisible(updateAvailable);
    m_sideUpdateText->setText(tr("Click to update to %1.").arg(latest));
    m_titleUpdateButton->setVisible(updateAvailable);
    m_titleUpdateButton->setText(tr(" Update %1").arg(latest));

    // Status bar
    QString statusLine = status;
    if (m_state == State::Running) {
        statusLine = tr("Running Beyonder • %1 MB RAM").arg(currentMaxMemoryMB());
    } else if (m_state == State::Ready && status.isEmpty()) {
        statusLine = tr("Ready • Beyonder %1").arg(installed);
    }
    m_statusText->setText(statusLine);
    const bool idle = !(m_state == State::Running || m_state == State::Installing);
    m_statusText->setProperty("idle", idle);
    restyle(m_statusText);
    m_statusIcon->setPixmap(lineIcon(Icon::Activity, idle ? Color::Muted : Color::Green, 14).pixmap(14, 14));
    m_statusProgress->setVisible(busy);
    if (busy && m_installPercent >= 0) {
        m_statusProgress->setRange(0, 100);
        m_statusProgress->setValue(m_installPercent);
    } else if (busy) {
        m_statusProgress->setRange(0, 0);
    }
    m_ramButton->setText(tr(" %1 MB").arg(currentMaxMemoryMB()));
    if (m_state == State::Ready && status.isEmpty() && launcherUpdateAvailable()) {
        m_statusText->setText(tr("Launcher %1 is available: see Settings › Updates.").arg(m_manifest->latestLauncherVersion));
    }

    // Toast: once per new version, until dismissed
    if (updateAvailable && m_toastDismissedFor != latest && APPLICATION->settings()->get("BeyonderShowToasts").toBool()) {
        m_toastText->setText(tr("Beyonder %1 is ready to install.").arg(latest));
        showToast(true);
    } else if (!updateAvailable) {
        showToast(false);
    }

    updateAccessLabel();
    if (m_accountPage)
        m_accountPage->refresh();
    if (m_settingsPage)
        m_settingsPage->refresh();
}

void BeyonderWindow::applyTheme()
{
    m_root->setStyleSheet(Beyonder::styleSheet(APPLICATION->settings()->get("BeyonderAccent").toString(),
                                     APPLICATION->settings()->get("BeyonderBackdrop").toBool()));
    if (m_newsView) {
        const QColor link = accent(APPLICATION->settings()->get("BeyonderAccent").toString()).color.lighter(150);
        m_newsView->document()->setDefaultStyleSheet(
            QString("h2 { font-size: 18px; margin: 0 0 6px 0; } h3 { font-size: 15px; margin: 18px 0 2px 0; }"
                    ".date { color: #8b8b93; font-size: 11px; } a { color: %1; } p { line-height: 140%; }")
                .arg(link.name()));
        updateNews();
    }
}

bool BeyonderWindow::launcherUpdateAvailable() const
{
    return m_manifest && !m_manifest->latestLauncherVersion.isEmpty() &&
           Version(BuildConfig.versionString()) < Version(m_manifest->latestLauncherVersion);
}

void BeyonderWindow::updateAccessLabel()
{
    auto account = APPLICATION->accounts()->defaultAccount();
    QString text;
    QString color;
    QString tip = m_accessMessage;
    if (account && m_access != Access::NotConfigured && m_access != Access::Unknown) {
        switch (m_access) {
            case Access::Checking:
                text = tr("Checking access…");
                color = Color::Muted;
                break;
            case Access::Allowed:
                text = tr("Access Verified");
                color = Color::Green;
                if (!m_accessDiscordName.isEmpty())
                    tip = tr("Discord: %1").arg(m_accessDiscordName);
                break;
            case Access::NotWhitelisted:
                text = tr("Not whitelisted");
                color = Color::Amber;
                break;
            case Access::DiscordRequired:
                text = tr("Discord not linked");
                color = Color::Amber;
                break;
            case Access::LostControl:
                text = tr("Lost Control");
                color = Color::Red;
                break;
            case Access::Banned:
                text = tr("Banned");
                color = Color::Red;
                break;
            default:
                text = tr("Access check unavailable");
                color = Color::Muted;
                break;
        }
        m_accessLabel->setText(text);
        m_accessLabel->setToolTip(tip);
        m_accessLabel->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: 600; padding-right: 6px;").arg(color));
        return;
    }
    m_accessLabel->setToolTip(QString());
    if (!account) {
        text = tr("Not signed in");
        color = Color::Muted;
    } else {
        switch (account->accountState()) {
            case AccountState::Online:
            case AccountState::Unchecked:
            case AccountState::Working:
                text = tr("Signed in");
                color = Color::Green;
                break;
            case AccountState::Offline:
                text = tr("Offline");
                color = Color::Amber;
                break;
            default:
                text = tr("Sign-in expired");
                color = Color::Amber;
                break;
        }
    }
    m_accessLabel->setText(text);
    m_accessLabel->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: 600; padding-right: 6px;").arg(color));
}

void BeyonderWindow::updateServerStrip()
{
    QString color = Color::Muted;
    QString text;
    const QString name = (m_manifest && !m_manifest->serverName.isEmpty()) ? m_manifest->serverName : tr("Beyonder");
    switch (m_serverStatus) {
        case ServerStatus::Unknown:
            text = tr("%1 Server • status unknown").arg(name);
            break;
        case ServerStatus::Pinging:
            text = tr("Checking %1 Server…").arg(name);
            break;
        case ServerStatus::NotConfigured:
            text = tr("No server announced yet");
            break;
        case ServerStatus::Offline:
            color = Color::Red;
            text = tr("%1 Server Offline").arg(name);
            break;
        case ServerStatus::Online: {
            color = Color::Green;
            text = tr("%1 Server Online").arg(name);
            if (m_serverOnline >= 0) {
                text += m_serverMax > 0 ? tr(" • %1 / %2 players").arg(m_serverOnline).arg(m_serverMax) : tr(" • %1 players").arg(m_serverOnline);
            }
            if (m_serverLatency >= 0) {
                text += tr(" (%1ms)").arg(m_serverLatency);
            }
            break;
        }
    }
    m_serverDot->setStyleSheet(QString("color: %1; font-size: 10px;").arg(color));
    m_serverLabel->setText(text);
}

void BeyonderWindow::pingServer()
{
    if (!m_manifest) {
        return;
    }
    if (m_manifest->server.isEmpty()) {
        m_serverStatus = ServerStatus::NotConfigured;
        updateServerStrip();
        return;
    }
    if (m_pingTask && m_pingTask->isRunning()) {
        return;
    }
    const auto target = MinecraftTarget::parse(m_manifest->server, false);
    if (m_serverStatus != ServerStatus::Online) {
        m_serverStatus = ServerStatus::Pinging;
        updateServerStrip();
    }
    m_pingTask.reset(new ServerPingTask(target.address, target.port));
    auto* task = m_pingTask.get();
    connect(task, &Task::succeeded, this, [this, task] {
        m_serverStatus = ServerStatus::Online;
        m_serverOnline = task->m_outputOnlinePlayers;
        m_serverMax = task->m_outputMaxPlayers;
        m_serverLatency = task->m_outputLatencyMs;
        updateServerStrip();
    });
    connect(task, &Task::failed, this, [this](const QString& reason) {
        qDebug() << "Beyonder server ping failed:" << reason;
        m_serverStatus = ServerStatus::Offline;
        updateServerStrip();
    });
    task->start();
}

// =====================================================================================================================
// Memory
// =====================================================================================================================

int BeyonderWindow::currentMaxMemoryMB() const
{
    if (auto* inst = findInstance()) {
        return inst->settings()->get("MaxMemAlloc").toInt();
    }
    return APPLICATION->settings()->get("MaxMemAlloc").toInt();
}

void BeyonderWindow::setMaxMemoryMB(int mb)
{
    if (auto* inst = findInstance()) {
        auto* settings = inst->settings();
        settings->set("OverrideMemory", true);
        settings->set("MaxMemAlloc", mb);
        settings->set("MinMemAlloc", std::min(1024, mb));
    } else {
        APPLICATION->settings()->set("MaxMemAlloc", mb);
    }
    updateUi();
}

void BeyonderWindow::rebuildRamMenu()
{
    m_ramMenu->clear();
    const int current = currentMaxMemoryMB();
    const auto* edition = m_manifest ? m_manifest->edition(currentEditionId()) : nullptr;
    const int recommended = edition ? edition->recommendedMemoryMB : 0;
    for (int gb : { 4, 6, 8, 10, 12, 16 }) {
        const int mb = gb * 1024;
        QString text = tr("%1 GB").arg(gb);
        if (mb == recommended)
            text += tr("  (recommended)");
        auto* action = m_ramMenu->addAction(text);
        action->setCheckable(true);
        action->setChecked(mb == current);
        connect(action, &QAction::triggered, this, [this, mb] { setMaxMemoryMB(mb); });
    }
    m_ramMenu->addSeparator();
    connect(m_ramMenu->addAction(tr("More Java settings…")), &QAction::triggered, this, [this] {
        if (auto* inst = findInstance())
            APPLICATION->showInstanceWindow(inst, "settings");
        else
            APPLICATION->ShowGlobalSettings(this, "java-settings");
    });
}

// =====================================================================================================================
// Instance management
// =====================================================================================================================

MinecraftInstance* BeyonderWindow::findInstance() const
{
    auto* list = APPLICATION->instances();
    for (int i = 0; i < list->count(); i++) {
        auto* inst = list->at(i);
        if (inst && inst->isManagedPack() && inst->getManagedPackID() == BuildConfig.BEYONDER_PACK_ID) {
            return inst;
        }
    }
    return nullptr;
}

void BeyonderWindow::watchInstance(MinecraftInstance* instance)
{
    if (m_watched == instance) {
        return;
    }
    if (m_watched) {
        disconnect(m_watched.data(), nullptr, this, nullptr);
    }
    m_watched = instance;
    if (instance) {
        connect(instance, &BaseInstance::runningStatusChanged, this, [this](bool) { refreshState(); });
    }
}

void BeyonderWindow::startInstall(bool launchAfter)
{
    if (m_state == State::Installing) {
        return;
    }
    if (!m_manifest) {
        checkForUpdates();
        return;
    }
    showToast(false);

    auto* existing = findInstance();
    const auto* edition = m_manifest->edition(currentEditionId());
    const QString editionId = edition->id;

    QMap<QString, QString> extraInfo;
    extraInfo.insert("pack_id", BuildConfig.BEYONDER_PACK_ID);
    extraInfo.insert("pack_version_id", m_manifest->packVersion);
    if (existing) {
        extraInfo.insert("original_instance_id", existing->id());
    }

    // Trusted: the manifest URL is baked into this build, so we accept downloads from the hosts it lists.
    auto* importTask = new InstanceImportTask(edition->mrpackUrl, true, this, std::move(extraInfo));
    if (existing) {
        importTask->setName(existing->name());
        importTask->setIcon(existing->iconKey());
        importTask->setGroup(APPLICATION->instances()->getInstanceGroup(existing->id()));
    } else {
        importTask->setName("Beyonder");
        importTask->setIcon("default");
    }
    importTask->setConfirmUpdate(false);

    m_installTask.reset(APPLICATION->instances()->wrapInstanceTask(importTask));
    setState(State::Installing, existing ? tr("Updating Beyonder…") : tr("Installing Beyonder…"));

    connect(m_installTask.get(), &Task::status, this, [this](const QString& status) {
        m_statusMessage = status;
        updateUi();
    });
    connect(m_installTask.get(), &Task::progress, this, [this](qint64 current, qint64 total) {
        if (total > 0) {
            const int percent = static_cast<int>(current * 100 / total);
            if (percent != m_installPercent) {
                m_installPercent = percent;
                updateUi();
            }
        }
    });
    connect(m_installTask.get(), &Task::succeeded, this, [this, launchAfter, editionId] {
        APPLICATION->settings()->set("BeyonderInstalledEdition", editionId);
        if (auto* inst = findInstance()) {
            applyRecommendedSettings(inst);
        }
        m_state = State::Ready;
        refreshState();
        if (launchAfter && m_state == State::Ready) {
            launchGame();
        }
    });
    connect(m_installTask.get(), &Task::failed, this, [this](const QString& reason) {
        m_state = State::Error;
        refreshState();
        QMessageBox::warning(this, tr("Install failed"), tr("Beyonder couldn't be installed:\n\n%1").arg(reason));
    });
    connect(m_installTask.get(), &Task::aborted, this, [this] {
        m_state = State::Error;
        refreshState();
    });

    m_installTask->start();
}

void BeyonderWindow::applyRecommendedSettings(MinecraftInstance* instance)
{
    const auto* edition = m_manifest ? m_manifest->edition(currentEditionId()) : nullptr;
    const int recommended = edition ? edition->recommendedMemoryMB : 0;
    if (recommended <= 0) {
        return;
    }
    auto* settings = instance->settings();
    // Only set memory the first time; after that the player's own choice wins.
    if (settings->get("OverrideMemory").toBool()) {
        return;
    }
    settings->set("OverrideMemory", true);
    settings->set("MaxMemAlloc", recommended);
    settings->set("MinMemAlloc", std::min(1024, recommended));
}

// =====================================================================================================================
// Play / accounts
// =====================================================================================================================

void BeyonderWindow::onPlayClicked()
{
    switch (m_state) {
        case State::Error:
            checkForUpdates();
            return;
        case State::NeedsInstall:
        case State::NeedsUpdate:
            if (!ensureAccount()) {
                return;
            }
            startInstall(true);
            return;
        case State::Ready:
            if (accessBlocksPlay()) {
                explainAccess();
                return;
            }
            launchGame();
            return;
        default:
            return;
    }
}

void BeyonderWindow::launchGame()
{
    auto* inst = findInstance();
    if (!inst) {
        refreshState();
        return;
    }
    auto account = ensureAccount();
    if (!account) {
        return;
    }

    MinecraftTarget::Ptr target = nullptr;
    if (m_manifest && !m_manifest->server.isEmpty()) {
        target.reset(new MinecraftTarget(MinecraftTarget::parse(m_manifest->server, false)));
    }
    if (APPLICATION->launch(inst, LaunchMode::Normal, target, account)) {
        APPLICATION->settings()->set("BeyonderSessions", APPLICATION->settings()->get("BeyonderSessions").toInt() + 1);
        setState(State::Running, tr("Starting Beyonder…"));
    }
}

MinecraftAccountPtr BeyonderWindow::ensureAccount()
{
    auto* accounts = APPLICATION->accounts();
    if (auto account = accounts->defaultAccount()) {
        return account;
    }
    if (accounts->count() > 0) {
        auto account = accounts->at(0);
        accounts->setDefaultAccount(account);
        return account;
    }
    addMicrosoftAccount();
    return accounts->defaultAccount();
}

void BeyonderWindow::addMicrosoftAccount()
{
    auto account = MSALoginDialog::newAccount(this);
    if (!account) {
        return;
    }
    auto* accounts = APPLICATION->accounts();
    accounts->addAccount(account);
    accounts->setDefaultAccount(account);
}

void BeyonderWindow::rebuildAccountMenu()
{
    auto* accounts = APPLICATION->accounts();
    auto current = accounts->defaultAccount();

    m_accountMenu->clear();
    for (int i = 0; i < accounts->count(); i++) {
        auto account = accounts->at(i);
        auto* action = m_accountMenu->addAction(QIcon(account->getFace()), account->profileName());
        action->setCheckable(true);
        action->setChecked(account == current);
        connect(action, &QAction::triggered, this, [account] { APPLICATION->accounts()->setDefaultAccount(account); });
    }
    if (accounts->count() > 0) {
        m_accountMenu->addSeparator();
    }
    if (m_accessLinkUrl.isValid()) {
        connect(m_accountMenu->addAction(lineIcon(Icon::Chat, "#c9c9d0", 16), m_accessDiscordName.isEmpty() ? tr("Link Discord account…") : tr("Discord: %1").arg(m_accessDiscordName)),
                &QAction::triggered, this, [this] { QDesktopServices::openUrl(m_accessLinkUrl); });
        m_accountMenu->addSeparator();
    }
    connect(m_accountMenu->addAction(tr("Add Microsoft account…")), &QAction::triggered, this, &BeyonderWindow::addMicrosoftAccount);
    connect(m_accountMenu->addAction(tr("Manage accounts…")), &QAction::triggered, this,
            [this] { APPLICATION->ShowGlobalSettings(this, "accounts"); });

    if (current) {
        m_accountButton->setIcon(QIcon(current->getFace(48, 48)));
        m_accountButton->setText(" " + current->profileName() + "  ");
    } else {
        m_accountButton->setIcon(lineIcon(R"(<circle cx="12" cy="8" r="4"/><path d="M4 21c1.5-4 4.5-6 8-6s6.5 2 8 6"/>)", "#c9c9d0", 20));
        m_accountButton->setText(tr(" Sign in  "));
    }
    if (m_accessLabel) {
        updateAccessLabel();
    }
}

// =====================================================================================================================
// Editions
// =====================================================================================================================

QString BeyonderWindow::currentEditionId() const
{
    const QString chosen = APPLICATION->settings()->get("BeyonderEdition").toString();
    if (!m_manifest) {
        return chosen.isEmpty() ? QStringLiteral("standard") : chosen;
    }
    return m_manifest->edition(chosen)->id;  // falls back to the first edition if the choice no longer exists
}

QString BeyonderWindow::installedEditionId() const
{
    const QString installed = APPLICATION->settings()->get("BeyonderInstalledEdition").toString();
    if (!installed.isEmpty()) {
        return installed;
    }
    // Installs from before editions existed used the only pack there was.
    return m_manifest && !m_manifest->editions.isEmpty() ? m_manifest->editions.first().id : QStringLiteral("standard");
}

void BeyonderWindow::setEdition(const QString& id)
{
    APPLICATION->settings()->set("BeyonderEdition", id);
    refreshState();
}

void BeyonderWindow::rebuildEditionMenu()
{
    m_editionMenu->clear();
    if (!m_manifest) {
        return;
    }
    const QString current = currentEditionId();
    for (const auto& e : m_manifest->editions) {
        QString text = e.name;
        if (!e.description.isEmpty())
            text += QString::fromUtf8(" — ") + e.description;
        auto* action = m_editionMenu->addAction(text);
        action->setCheckable(true);
        action->setChecked(e.id == current);
        const QString id = e.id;
        connect(action, &QAction::triggered, this, [this, id] { setEdition(id); });
    }
}

// =====================================================================================================================
// Access
// =====================================================================================================================

bool BeyonderWindow::accessBlocksPlay() const
{
    switch (m_access) {
        case Access::NotWhitelisted:
        case Access::DiscordRequired:
        case Access::LostControl:
        case Access::Banned:
            return true;
        default:
            // Unknown or unreachable: let the game server make the final call.
            return false;
    }
}

void BeyonderWindow::checkAccess()
{
    auto account = APPLICATION->accounts()->defaultAccount();
    if (!m_manifest || m_manifest->accessUrl.isEmpty()) {
        m_access = Access::NotConfigured;
        updateUi();
        return;
    }
    if (!account || !account->hasProfile()) {
        m_access = Access::Unknown;
        m_accessLinkUrl.clear();
        updateUi();
        return;
    }
    if (m_accessJob) {
        return;
    }
    QString url = m_manifest->accessUrl;
    url.replace("{uuid}", account->profileId());
    url.replace("{name}", QString::fromUtf8(QUrl::toPercentEncoding(account->profileName())));

    if (m_access == Access::NotConfigured || m_access == Access::Unknown) {
        m_access = Access::Checking;
        updateUi();
    }
    m_accessJob.reset(new NetJob("Beyonder access", APPLICATION->network()));
    auto [request, response] = Net::Request::makeByteArray(QUrl(url), Net::Request::Option::AcceptLocalFiles);
    m_accessJob->addNetAction(request);
    connect(m_accessJob.get(), &NetJob::succeeded, this, [this, response] {
        const QByteArray reply = *response;
        m_accessJob.reset();
        onAccessReply(reply);
    });
    connect(m_accessJob.get(), &NetJob::failed, this, [this](const QString& reason) {
        qWarning() << "Beyonder access check failed:" << reason;
        m_accessJob.reset();
        m_access = Access::Unreachable;
        m_accessMessage = tr("The access service didn't answer. You can still try to join; the server decides.");
        updateUi();
    });
    m_accessJob->start();
}

void BeyonderWindow::onAccessReply(const QByteArray& reply)
{
    const auto obj = QJsonDocument::fromJson(reply).object();
    const QString status = obj.value("status").toString();
    static const QHash<QString, Access> s_statuses = {
        { "allowed", Access::Allowed },           { "not_whitelisted", Access::NotWhitelisted },
        { "discord_required", Access::DiscordRequired }, { "lost_control", Access::LostControl },
        { "banned", Access::Banned },
    };
    m_access = s_statuses.value(status, Access::Unreachable);
    m_accessMessage = obj.value("message").toString();
    m_accessDiscordName = obj.value("discordName").toString();
    const auto character = obj.value("character").toObject();
    m_accessPathway = character.value("pathway").toString();
    m_accessSequence = character.value("sequence").toInt(-1);
    const QUrl link(obj.value("linkUrl").toString());
    m_accessLinkUrl = (link.isValid() && link.scheme() == "https") ? link : QUrl();

    if (m_accessMessage.isEmpty()) {
        switch (m_access) {
            case Access::NotWhitelisted:
                m_accessMessage = tr("You're not on the whitelist yet. Apply on the Beyonder Discord.");
                break;
            case Access::DiscordRequired:
                m_accessMessage = tr("Link your Discord account to play on Beyonder.");
                break;
            case Access::LostControl:
                m_accessMessage = tr("Your Beyonder lost control and became a monster. Wait for the next revival.");
                break;
            case Access::Banned:
                m_accessMessage = tr("This account is banned from Beyonder.");
                break;
            default:
                break;
        }
    }
    rebuildAccountMenu();
    updateUi();
}

void BeyonderWindow::explainAccess()
{
    if (m_access == Access::DiscordRequired && m_accessLinkUrl.isValid()) {
        QDesktopServices::openUrl(m_accessLinkUrl);
        return;
    }
    QString title;
    switch (m_access) {
        case Access::NotWhitelisted:
            title = tr("No access yet");
            break;
        case Access::DiscordRequired:
            title = tr("Link Discord");
            break;
        case Access::LostControl:
            title = tr("Lost Control");
            break;
        case Access::Banned:
            title = tr("Banned");
            break;
        default:
            return;
    }
    QMessageBox box(QMessageBox::Information, title, m_accessMessage, QMessageBox::Close, this);
    QPushButton* discord = nullptr;
    if (m_access == Access::NotWhitelisted && m_manifest && m_manifest->discordUrl.isValid()) {
        discord = box.addButton(tr("Open Discord"), QMessageBox::AcceptRole);
    } else if (m_accessLinkUrl.isValid()) {
        discord = box.addButton(tr("Open"), QMessageBox::AcceptRole);
    }
    box.exec();
    if (discord && box.clickedButton() == discord) {
        QDesktopServices::openUrl(m_access == Access::NotWhitelisted && m_manifest && m_manifest->discordUrl.isValid() ? m_manifest->discordUrl
                                                                                                                     : m_accessLinkUrl);
    }
}
