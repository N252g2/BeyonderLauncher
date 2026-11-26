// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Beyonder Launcher - a fork of Prism Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "BeyonderAccountPage.h"

#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "Application.h"
#include "DesktopServices.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/auth/AccountList.h"
#include "ui/beyonder/BeyonderStyle.h"
#include "ui/beyonder/BeyonderWindow.h"

using namespace Beyonder;

namespace {
// Top of the profile card: a soft accent-colored banner.
class Banner : public QWidget {
   public:
    using QWidget::QWidget;

   protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QColor c = accent(APPLICATION->settings()->get("BeyonderAccent").toString()).color;
        QLinearGradient g(0, 0, width(), height());
        g.setColorAt(0, QColor(c.red(), c.green(), c.blue(), 70));
        g.setColorAt(0.55, QColor(20, 20, 24));
        g.setColorAt(1, QColor(14, 14, 16));
        QPainterPath path;
        const qreal r = 11;
        const QRectF area = QRectF(this->rect()).adjusted(1, 1, -1, 0);
        path.moveTo(area.left(), area.bottom());
        path.lineTo(area.left(), area.top() + r);
        path.quadTo(area.left(), area.top(), area.left() + r, area.top());
        path.lineTo(area.right() - r, area.top());
        path.quadTo(area.right(), area.top(), area.right(), area.top() + r);
        path.lineTo(area.right(), area.bottom());
        path.closeSubpath();
        p.fillPath(path, g);
    }
};

QWidget* pageOf(QWidget* first, QWidget* second = nullptr)
{
    auto* w = new QWidget();
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(0, 0, 6, 0);
    l->setSpacing(16);
    l->addWidget(first);
    if (second)
        l->addWidget(second);
    l->addStretch(1);
    return w;
}

QLabel* statePill(QWidget* parent)
{
    auto* l = new QLabel(parent);
    l->setObjectName("bBadge");
    return l;
}

void setPill(QLabel* pill, const QString& text, const char* style)
{
    pill->setText(text);
    pill->setObjectName(style);
    restyle(pill);
}

void clearLayout(QVBoxLayout* l)
{
    while (auto* item = l->takeAt(0)) {
        delete item->widget();
        delete item;
    }
}
}  // namespace

BeyonderAccountPage::BeyonderAccountPage(BeyonderWindow* window) : QWidget(), m_window(window)
{
    auto* l = new QVBoxLayout(this);
    l->setContentsMargins(28, 24, 28, 20);
    l->setSpacing(4);
    auto* header = new QHBoxLayout();
    auto* titles = new QVBoxLayout();
    titles->setSpacing(4);
    titles->addWidget(makeLabel(tr("Account"), "bH1", this));
    titles->addWidget(makeLabel(tr("Your Minecraft identity, linked accounts and play stats."), "bSubtle", this));
    header->addLayout(titles, 1);
    m_signOut = makeGhostButton(tr("Sign Out"), Icon::LogOut, this);
    connect(m_signOut, &QPushButton::clicked, this, [this] {
        auto* accounts = APPLICATION->accounts();
        auto current = accounts->defaultAccount();
        if (!current)
            return;
        if (QMessageBox::question(m_window, tr("Sign out"), tr("Sign out %1 on this computer?").arg(current->profileName())) != QMessageBox::Yes)
            return;
        for (int i = 0; i < accounts->count(); i++) {
            if (accounts->at(i) == current) {
                accounts->removeAccount(accounts->index(i));
                break;
            }
        }
        refresh();
    });
    header->addWidget(m_signOut, 0, Qt::AlignTop);
    l->addLayout(header);
    l->addSpacing(14);

    auto* row = new QHBoxLayout();
    row->setSpacing(18);
    auto* nav = new QFrame(this);
    nav->setObjectName("sNavCard");
    nav->setFixedWidth(220);
    auto* nl = new QVBoxLayout(nav);
    nl->setContentsMargins(8, 8, 8, 8);
    nl->setSpacing(2);

    m_stack = new QStackedWidget();
    m_stack->addWidget(buildProfile());
    m_stack->addWidget(buildConnections());
    m_stack->addWidget(buildSecurity());
    const QList<std::pair<const char*, QString>> categories = {
        { Icon::User, tr("Profile") },
        { Icon::Link, tr("Connections") },
        { Icon::Key, tr("Security") },
    };
    for (int i = 0; i < categories.size(); i++) {
        auto* b = makeSubNavButton(categories[i].first, categories[i].second, nav);
        nl->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, i] {
            m_stack->setCurrentIndex(i);
            refresh();
        });
        if (i == 0)
            b->setChecked(true);
    }
    row->addWidget(nav, 0, Qt::AlignTop);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(m_stack);
    row->addWidget(scroll, 1);
    l->addLayout(row, 1);
}

void BeyonderAccountPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    refresh();
}

QWidget* BeyonderAccountPage::buildProfile()
{
    auto* card = new QFrame();
    card->setObjectName("sCard");
    auto* l = new QVBoxLayout(card);
    l->setContentsMargins(0, 0, 0, 20);
    l->setSpacing(0);

    m_banner = new Banner(card);
    m_banner->setFixedHeight(96);
    auto* bannerLayout = new QHBoxLayout(m_banner);
    bannerLayout->setContentsMargins(12, 12, 12, 12);
    bannerLayout->addStretch(1);
    auto* skin = makeGhostButton(tr("Manage Skin"), Icon::Palette, m_banner);
    skin->setToolTip(tr("Change your Minecraft skin and cape"));
    connect(skin, &QPushButton::clicked, this, [this] { APPLICATION->ShowGlobalSettings(m_window, "accounts"); });
    bannerLayout->addWidget(skin, 0, Qt::AlignTop);
    l->addWidget(m_banner);

    auto* body = new QVBoxLayout();
    body->setContentsMargins(20, 0, 20, 0);
    body->setSpacing(16);

    auto* identity = new QHBoxLayout();
    identity->setSpacing(16);
    m_face = new QLabel(card);
    m_face->setFixedSize(76, 76);
    m_face->setStyleSheet("border: 2px solid #34343a; border-radius: 10px; background: #121214;");
    m_face->setAlignment(Qt::AlignCenter);
    identity->addWidget(m_face, 0, Qt::AlignBottom);
    auto* texts = new QVBoxLayout();
    texts->setSpacing(4);
    auto* nameRow = new QHBoxLayout();
    nameRow->setSpacing(10);
    m_name = makeLabel(QString(), "sProfileName", card);
    m_accessBadge = statePill(card);
    nameRow->addWidget(m_name);
    nameRow->addWidget(m_accessBadge, 0, Qt::AlignVCenter);
    nameRow->addStretch(1);
    texts->addLayout(nameRow);
    m_subtitle = makeLabel(QString(), "sCardSub", card);
    texts->addWidget(m_subtitle);
    auto* uuidRow = new QHBoxLayout();
    uuidRow->setSpacing(8);
    m_uuid = makeLabel(QString(), "sMono", card);
    m_uuid->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* copy = makeGhostButton(tr("Copy"), Icon::Copy, card);
    connect(copy, &QPushButton::clicked, this, [this] { QGuiApplication::clipboard()->setText(m_uuid->text()); });
    uuidRow->addWidget(m_uuid);
    uuidRow->addWidget(copy);
    uuidRow->addStretch(1);
    texts->addLayout(uuidRow);
    identity->addLayout(texts, 1);
    body->addLayout(identity);

    auto* divider = new QFrame(card);
    divider->setFixedHeight(1);
    divider->setStyleSheet("background: #1d1d21;");
    body->addWidget(divider);

    auto* stats = new QGridLayout();
    stats->setSpacing(10);
    m_playtime = new QLabel();
    m_sessions = new QLabel();
    m_lastPlayed = new QLabel();
    m_sequence = new QLabel();
    stats->addWidget(makeStatTile(tr("Playtime"), m_playtime, Icon::Clock, card), 0, 0);
    stats->addWidget(makeStatTile(tr("Sessions"), m_sessions, Icon::Play, card), 0, 1);
    stats->addWidget(makeStatTile(tr("Last Played"), m_lastPlayed, Icon::Activity, card), 0, 2);
    stats->addWidget(makeStatTile(tr("Pathway"), m_sequence, Icon::Shield, card), 0, 3);
    body->addLayout(stats);
    l->addLayout(body);
    return pageOf(card);
}

QWidget* BeyonderAccountPage::buildConnections()
{
    auto* mcState = statePill(nullptr);
    auto minecraft = makeCard(tr("Minecraft Account"), tr("Microsoft sign-in. The launcher refreshes it automatically."), mcState);
    m_minecraftState = mcState;
    m_minecraftBody = minecraft.body;

    auto* dcState = statePill(nullptr);
    auto discord = makeCard(tr("Discord Account"), tr("Verifies whitelist access and syncs your guild roles."), dcState);
    m_discordState = dcState;
    m_discordBody = discord.body;
    return pageOf(minecraft.frame, discord.frame);
}

QWidget* BeyonderAccountPage::buildSecurity()
{
    auto signIn = makeCard(tr("Sign-in on this Computer"),
                           tr("Beyonder Launcher never sees your Microsoft password. Microsoft gives it a sign-in token, which is stored only on this "
                              "computer, in the launcher's data folder."));
    auto* folder = makeGhostButton(tr("Open data folder"), Icon::Folder);
    connect(folder, &QPushButton::clicked, this, [] { DesktopServices::openPath(QDir::currentPath(), true); });
    signIn.body->addWidget(folder, 0, Qt::AlignLeft);

    auto* remove = makeDangerButton(tr("Sign out everywhere"), Icon::LogOut);
    connect(remove, &QPushButton::clicked, this, &BeyonderAccountPage::signOutEverywhere);
    auto danger = makeCard(tr("Remove All Accounts"),
                           tr("Signs every Microsoft account out of this launcher and deletes the saved tokens. Your Beyonder worlds, settings and "
                              "game files stay on disk."),
                           remove);
    return pageOf(signIn.frame, danger.frame);
}

void BeyonderAccountPage::signOutEverywhere()
{
    auto* accounts = APPLICATION->accounts();
    if (accounts->count() == 0)
        return;
    if (QMessageBox::warning(m_window, tr("Sign out everywhere"),
                             tr("Remove all %n account(s) from this computer? You'll need to sign in with Microsoft again to play.", "", accounts->count()),
                             QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
        return;
    while (accounts->count() > 0)
        accounts->removeAccount(accounts->index(0));
    refresh();
}

void BeyonderAccountPage::refresh()
{
    if (!isVisible())
        return;
    auto* accounts = APPLICATION->accounts();
    auto account = accounts->defaultAccount();
    auto* inst = m_window->findInstance();
    m_signOut->setVisible(account != nullptr);

    // --- Profile
    if (account) {
        m_face->setPixmap(account->getFace(64, 64));
        m_name->setText(account->profileName());
        m_uuid->setText(account->profileId());
    } else {
        m_face->setPixmap(lineIcon(Icon::User, "#8b8b93", 32).pixmap(32, 32));
        m_name->setText(tr("Not signed in"));
        m_uuid->clear();
    }
    QString subtitle = account ? tr("Signed in with Microsoft") : tr("Press Play or Add Microsoft account to sign in.");
    if (!m_window->m_accessDiscordName.isEmpty())
        subtitle += " • " + tr("Discord: %1").arg(m_window->m_accessDiscordName);
    m_subtitle->setText(subtitle);

    using Access = BeyonderWindow::Access;
    switch (m_window->m_access) {
        case Access::Allowed:
            setPill(m_accessBadge, tr("Access Verified"), "bBadgeGreen");
            break;
        case Access::NotWhitelisted:
            setPill(m_accessBadge, tr("Not whitelisted"), "bBadgeAmber");
            break;
        case Access::DiscordRequired:
            setPill(m_accessBadge, tr("Discord not linked"), "bBadgeAmber");
            break;
        case Access::LostControl:
            setPill(m_accessBadge, tr("Lost Control"), "bBadgeRed");
            break;
        case Access::Banned:
            setPill(m_accessBadge, tr("Banned"), "bBadgeRed");
            break;
        default:
            setPill(m_accessBadge, account ? tr("Signed in") : tr("Guest"), "bBadge");
            break;
    }

    m_playtime->setText(inst ? humanHours(inst->totalTimePlayed()) : QStringLiteral("—"));
    m_sessions->setText(QString::number(APPLICATION->settings()->get("BeyonderSessions").toInt()));
    const qint64 last = inst ? inst->lastLaunch() : 0;
    m_lastPlayed->setText(last > 0 ? QLocale().toString(QDateTime::fromMSecsSinceEpoch(last).date(), QLocale::ShortFormat) : tr("Never"));
    if (!m_window->m_accessPathway.isEmpty()) {
        m_sequence->setText(m_window->m_accessSequence >= 0
                                ? tr("%1 • Seq %2").arg(m_window->m_accessPathway).arg(m_window->m_accessSequence)
                                : m_window->m_accessPathway);
        m_sequence->setStyleSheet(QString("color: %1;").arg(accent(APPLICATION->settings()->get("BeyonderAccent").toString()).color.lighter(140).name()));
    } else {
        m_sequence->setText(tr("Unknown"));
        m_sequence->setStyleSheet("color: #8b8b93;");
    }
    m_banner->update();

    // --- Connections: Minecraft
    clearLayout(m_minecraftBody);
    if (account) {
        const bool ok = account->accountState() == AccountState::Online || account->accountState() == AccountState::Unchecked ||
                        account->accountState() == AccountState::Working;
        setPill(m_minecraftState, ok ? tr("Connected") : tr("Needs sign-in"), ok ? "bBadgeGreen" : "bBadgeAmber");
    } else {
        setPill(m_minecraftState, tr("Not connected"), "bBadge");
    }
    for (int i = 0; i < accounts->count(); i++) {
        auto a = accounts->at(i);
        auto* rowFrame = new QFrame();
        rowFrame->setObjectName("sRow");
        auto* rl = new QHBoxLayout(rowFrame);
        rl->setContentsMargins(14, 10, 14, 10);
        rl->setSpacing(12);
        auto* face = new QLabel(rowFrame);
        face->setPixmap(a->getFace(32, 32));
        rl->addWidget(face);
        auto* t = new QVBoxLayout();
        t->setSpacing(1);
        t->addWidget(makeLabel(a->profileName() + (a == account ? tr("  (playing as)") : QString()), "sRowTitle", rowFrame));
        t->addWidget(makeLabel(a->profileId(), "sMono", rowFrame));
        rl->addLayout(t, 1);
        if (a != account) {
            auto* use = makeGhostButton(tr("Use"), nullptr, rowFrame);
            connect(use, &QPushButton::clicked, this, [this, a] {
                APPLICATION->accounts()->setDefaultAccount(a);
                refresh();
            });
            rl->addWidget(use);
        }
        auto* remove = makeDangerButton(tr("Disconnect"), Icon::Link, rowFrame);
        connect(remove, &QPushButton::clicked, this, [this, a] {
            auto* list = APPLICATION->accounts();
            for (int j = 0; j < list->count(); j++) {
                if (list->at(j) == a) {
                    list->removeAccount(list->index(j));
                    break;
                }
            }
            refresh();
        });
        rl->addWidget(remove);
        m_minecraftBody->addWidget(rowFrame);
    }
    auto* add = makeWhiteButton(tr("Add Microsoft account"), Icon::User);
    connect(add, &QPushButton::clicked, this, [this] {
        m_window->addMicrosoftAccount();
        refresh();
    });
    m_minecraftBody->addWidget(add, 0, Qt::AlignLeft);

    // --- Connections: Discord (from the access service)
    clearLayout(m_discordBody);
    const bool serviceConfigured = m_window->m_manifest && !m_window->m_manifest->accessUrl.isEmpty();
    if (!serviceConfigured) {
        setPill(m_discordState, tr("Coming soon"), "bBadge");
        auto* note = makeLabel(tr("Discord linking turns on once the Beyonder bot is online. You'll link your account here in one click."), "sRowDesc", nullptr);
        note->setWordWrap(true);
        m_discordBody->addWidget(note);
        return;
    }
    switch (m_window->m_access) {
        case Access::Allowed:
            setPill(m_discordState, tr("Access Verified"), "bBadgeGreen");
            break;
        case Access::DiscordRequired:
            setPill(m_discordState, tr("Not linked"), "bBadgeAmber");
            break;
        case Access::NotWhitelisted:
            setPill(m_discordState, tr("Not whitelisted"), "bBadgeAmber");
            break;
        case Access::Unreachable:
            setPill(m_discordState, tr("Service offline"), "bBadge");
            break;
        default:
            setPill(m_discordState, tr("Unknown"), "bBadge");
            break;
    }
    auto* rowFrame = new QFrame();
    rowFrame->setObjectName("sRow");
    auto* rl = new QHBoxLayout(rowFrame);
    rl->setContentsMargins(14, 10, 14, 10);
    rl->setSpacing(12);
    auto* icon = new QLabel(rowFrame);
    icon->setPixmap(lineIcon(Icon::Chat, "#c9c9d0", 20).pixmap(20, 20));
    rl->addWidget(icon);
    auto* t = new QVBoxLayout();
    t->setSpacing(1);
    t->addWidget(makeLabel(m_window->m_accessDiscordName.isEmpty() ? tr("No Discord account linked") : m_window->m_accessDiscordName, "sRowTitle", rowFrame));
    if (!m_window->m_accessMessage.isEmpty())
        t->addWidget(makeLabel(m_window->m_accessMessage, "sRowDesc", rowFrame));
    rl->addLayout(t, 1);
    auto* refreshButton = makeGhostButton(tr("Refresh"), Icon::Refresh, rowFrame);
    connect(refreshButton, &QPushButton::clicked, this, [this] { m_window->checkAccess(); });
    rl->addWidget(refreshButton);
    if (m_window->m_accessLinkUrl.isValid()) {
        auto* link = makeWhiteButton(m_window->m_accessDiscordName.isEmpty() ? tr("Link Discord") : tr("Manage"), Icon::Link, rowFrame);
        connect(link, &QPushButton::clicked, this, [this] { QDesktopServices::openUrl(m_window->m_accessLinkUrl); });
        rl->addWidget(link);
    }
    m_discordBody->addWidget(rowFrame);
}
