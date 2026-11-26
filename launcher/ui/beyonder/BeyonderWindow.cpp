// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Beyonder Launcher - a fork of Prism Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "BeyonderWindow.h"

#include <QCloseEvent>
#include <QDebug>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QTextBrowser>
#include <QToolButton>
#include <QVBoxLayout>

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

namespace {
const char* const s_styleSheet = R"(
#beyonderRoot {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1d1830, stop:0.55 #120f1d, stop:1 #0b0a12);
}
#beyonderRoot QLabel { color: #e9e4d8; }
#beyonderTitle { font-size: 26px; font-weight: 600; letter-spacing: 6px; color: #f3efe6; }
#beyonderSubtitle { color: #a8a2b8; }
#beyonderStatus { color: #cfc9db; }
#beyonderVersion { color: #8d889c; font-size: 11px; }
#beyonderMotd {
    background: rgba(255, 255, 255, 0.04);
    border: 1px solid rgba(207, 203, 216, 0.18);
    border-radius: 8px;
    color: #e9e4d8;
    padding: 12px;
}
#beyonderBottom {
    background: rgba(0, 0, 0, 0.35);
    border-top: 1px solid rgba(207, 203, 216, 0.15);
}
#beyonderPlay {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #c0303a, stop:1 #7b1020);
    color: #fff7ef;
    border: 1px solid #e2574c;
    border-radius: 6px;
    font-size: 18px;
    font-weight: 700;
    letter-spacing: 3px;
    padding: 10px 36px;
    min-width: 180px;
}
#beyonderPlay:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #d23c46, stop:1 #8c1525); }
#beyonderPlay:disabled { background: #3a3446; border-color: #4a4458; color: #8d889c; }
#beyonderRoot QToolButton {
    color: #e9e4d8;
    background: transparent;
    border: 1px solid transparent;
    border-radius: 6px;
    padding: 6px 10px;
}
#beyonderRoot QToolButton:hover { border-color: rgba(207, 203, 216, 0.35); }
#beyonderRoot QProgressBar {
    background: rgba(255, 255, 255, 0.08);
    border: none;
    border-radius: 3px;
    max-height: 6px;
}
#beyonderRoot QProgressBar::chunk { background: #c0303a; border-radius: 3px; }
)";

QToolButton* makeToolButton(const QString& text, const QString& tip, QWidget* parent)
{
    auto* b = new QToolButton(parent);
    b->setText(text);
    b->setToolTip(tip);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}
}  // namespace

BeyonderWindow::BeyonderWindow(QWidget* parent) : QMainWindow(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(BuildConfig.LAUNCHER_DISPLAYNAME);
    setWindowIcon(APPLICATION->logo());
    buildUi();

    const auto geometry = APPLICATION->settings()->get("BeyonderWindowGeometry").toString();
    if (geometry.isEmpty() || !restoreGeometry(QByteArray::fromBase64(geometry.toUtf8()))) {
        resize(960, 600);
    }

    auto* accounts = APPLICATION->accounts();
    connect(accounts, &AccountList::listChanged, this, &BeyonderWindow::rebuildAccountMenu);
    connect(accounts, &AccountList::defaultAccountChanged, this, &BeyonderWindow::rebuildAccountMenu);
    rebuildAccountMenu();

    watchInstance(findInstance());
    checkForUpdates();
}

BeyonderWindow::~BeyonderWindow() = default;

void BeyonderWindow::buildUi()
{
    auto* root = new QWidget(this);
    root->setObjectName("beyonderRoot");
    root->setStyleSheet(s_styleSheet);
    setCentralWidget(root);

    auto* rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Header
    auto* header = new QWidget(root);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(24, 18, 18, 12);
    headerLayout->setSpacing(14);

    m_logo = new QLabel(header);
    m_logo->setPixmap(APPLICATION->logo().pixmap(56, 56));
    headerLayout->addWidget(m_logo);

    auto* titleBox = new QVBoxLayout();
    titleBox->setSpacing(2);
    auto* title = new QLabel(tr("BEYONDER"), header);
    title->setObjectName("beyonderTitle");
    auto* subtitle = new QLabel(tr("Above the gray fog, the path awaits."), header);
    subtitle->setObjectName("beyonderSubtitle");
    titleBox->addWidget(title);
    titleBox->addWidget(subtitle);
    headerLayout->addLayout(titleBox);
    headerLayout->addStretch(1);

    auto* folderButton = makeToolButton(tr("Game folder"), tr("Open the Beyonder game folder"), header);
    connect(folderButton, &QToolButton::clicked, this, [this] {
        if (auto* inst = findInstance()) {
            DesktopServices::openPath(inst->gameRoot(), true);
        }
    });
    auto* settingsButton = makeToolButton(tr("Settings"), tr("Launcher, Java and memory settings"), header);
    connect(settingsButton, &QToolButton::clicked, this, [this] { APPLICATION->ShowGlobalSettings(this); });
    auto* classicButton = makeToolButton(tr("Advanced"), tr("Open the full launcher (instances, mods, logs)"), header);
    connect(classicButton, &QToolButton::clicked, this, &BeyonderWindow::classicViewRequested);

    headerLayout->addWidget(folderButton);
    headerLayout->addWidget(settingsButton);
    headerLayout->addWidget(classicButton);
    rootLayout->addWidget(header);

    // Message of the day / news from the manifest
    auto* body = new QWidget(root);
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(24, 6, 24, 18);
    m_motd = new QTextBrowser(body);
    m_motd->setObjectName("beyonderMotd");
    m_motd->setOpenExternalLinks(true);
    m_motd->setHtml(tr("<p>Contacting the gray fog…</p>"));
    bodyLayout->addWidget(m_motd);
    rootLayout->addWidget(body, 1);

    // Bottom bar: account, status, play
    auto* bottom = new QWidget(root);
    bottom->setObjectName("beyonderBottom");
    bottom->setAttribute(Qt::WA_StyledBackground);
    auto* bottomLayout = new QHBoxLayout(bottom);
    bottomLayout->setContentsMargins(24, 14, 24, 14);
    bottomLayout->setSpacing(18);

    m_accountButton = new QToolButton(bottom);
    m_accountButton->setPopupMode(QToolButton::InstantPopup);
    m_accountButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_accountButton->setIconSize(QSize(28, 28));
    m_accountButton->setCursor(Qt::PointingHandCursor);
    m_accountMenu = new QMenu(m_accountButton);
    m_accountButton->setMenu(m_accountMenu);
    bottomLayout->addWidget(m_accountButton);

    auto* statusBox = new QVBoxLayout();
    statusBox->setSpacing(6);
    m_statusLabel = new QLabel(bottom);
    m_statusLabel->setObjectName("beyonderStatus");
    m_statusLabel->setWordWrap(true);
    m_progress = new QProgressBar(bottom);
    m_progress->setTextVisible(false);
    m_progress->setVisible(false);
    m_versionLabel = new QLabel(bottom);
    m_versionLabel->setObjectName("beyonderVersion");
    statusBox->addWidget(m_statusLabel);
    statusBox->addWidget(m_progress);
    statusBox->addWidget(m_versionLabel);
    bottomLayout->addLayout(statusBox, 1);

    m_playButton = new QPushButton(tr("PLAY"), bottom);
    m_playButton->setObjectName("beyonderPlay");
    m_playButton->setCursor(Qt::PointingHandCursor);
    m_playButton->setDefault(true);
    connect(m_playButton, &QPushButton::clicked, this, &BeyonderWindow::onPlayClicked);
    bottomLayout->addWidget(m_playButton);

    rootLayout->addWidget(bottom);
}

void BeyonderWindow::closeEvent(QCloseEvent* event)
{
    if (m_installTask && m_installTask->isRunning()) {
        auto answer = QMessageBox::question(this, tr("Update in progress"),
                                            tr("Beyonder is still being installed. Stop and close the launcher?"));
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

// ---------------------------------------------------------------------------------------------------------------------
// Manifest / update check
// ---------------------------------------------------------------------------------------------------------------------

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
        const QByteArray data = *response;
        m_manifestJob.reset();
        onManifestDownloaded(data);
    });
    connect(m_manifestJob.get(), &NetJob::failed, this, [this](const QString& reason) {
        m_manifestJob.reset();
        onManifestFailed(reason);
    });
    m_manifestJob->start();
}

void BeyonderWindow::onManifestDownloaded(const QByteArray& data)
{
    QString error;
    m_manifest = BeyonderManifest::parse(data, error);
    m_manifestError = error;
    if (!m_manifest) {
        m_motd->setHtml(tr("<h2>Something is wrong with the update server.</h2><p>%1</p>").arg(error.toHtmlEscaped()));
    }

    if (m_manifest) {
        if (!m_manifest->motd.isEmpty()) {
            m_motd->setHtml(m_manifest->motd);
        } else {
            m_motd->setHtml(tr("<h2>Welcome, Beyonder.</h2><p>Press Play to enter the world.</p>"));
        }
        if (!m_manifest->minLauncherVersion.isEmpty() &&
            Version(BuildConfig.versionString()) < Version(m_manifest->minLauncherVersion)) {
            m_manifestError = tr("This version of the launcher is too old for the current Beyonder release. "
                                 "Please download the latest launcher.");
            m_manifest.reset();
        }
    }
    refreshState();
}

void BeyonderWindow::onManifestFailed(const QString& reason)
{
    qWarning() << "Beyonder manifest download failed:" << reason;
    m_manifest.reset();
    m_manifestError = tr("Couldn't reach the update server (%1).").arg(reason);
    m_motd->setHtml(tr("<h2>The fog is thick tonight.</h2>"
                       "<p>The launcher couldn't reach the Beyonder update server. Check your internet connection, "
                       "then press Retry.</p>"));
    refreshState();
}

void BeyonderWindow::refreshState()
{
    auto* inst = findInstance();
    watchInstance(inst);

    if (inst && inst->isRunning()) {
        setState(State::Running);
        return;
    }

    if (!m_manifest) {
        if (inst) {
            // Offline or broken manifest: still let people play what they have.
            setState(State::Ready, m_manifestError + " " + tr("You can still play the installed version."));
        } else {
            setState(State::Error, m_manifestError);
        }
        return;
    }

    if (!inst) {
        setState(State::NeedsInstall);
    } else if (inst->getManagedPackVersionID() != m_manifest->packVersion) {
        setState(State::NeedsUpdate);
    } else {
        setState(State::Ready);
    }
}

void BeyonderWindow::setState(State state, const QString& message)
{
    m_state = state;

    auto* inst = findInstance();
    const QString installed = inst ? inst->getManagedPackVersionID() : QString();
    QString versionText = tr("Launcher %1").arg(BuildConfig.printableVersionString());
    if (!installed.isEmpty()) {
        versionText += " · " + tr("Beyonder %1").arg(installed);
    }
    if (m_manifest && m_manifest->packVersion != installed) {
        versionText += " · " + tr("latest %1").arg(m_manifest->packVersion);
    }
    m_versionLabel->setText(versionText);

    bool playEnabled = true;
    bool showProgress = false;
    QString buttonText = tr("PLAY");
    QString status = message;

    switch (state) {
        case State::Checking:
            playEnabled = false;
            showProgress = true;
            m_progress->setRange(0, 0);
            break;
        case State::NeedsInstall:
            buttonText = tr("INSTALL");
            if (status.isEmpty())
                status = tr("Beyonder isn't installed yet. It will download the game and mods.");
            break;
        case State::NeedsUpdate:
            buttonText = tr("UPDATE");
            if (status.isEmpty())
                status = tr("A new version of Beyonder is available (%1).").arg(m_manifest->packVersion);
            break;
        case State::Ready:
            if (status.isEmpty())
                status = tr("Ready.");
            break;
        case State::Installing:
            playEnabled = false;
            showProgress = true;
            buttonText = tr("INSTALLING");
            break;
        case State::Running:
            playEnabled = false;
            buttonText = tr("PLAYING");
            if (status.isEmpty())
                status = tr("Beyonder is running.");
            break;
        case State::Error:
            buttonText = tr("RETRY");
            break;
    }

    m_playButton->setText(buttonText);
    m_playButton->setEnabled(playEnabled);
    m_progress->setVisible(showProgress);
    m_statusLabel->setText(status);
}

// ---------------------------------------------------------------------------------------------------------------------
// Instance management
// ---------------------------------------------------------------------------------------------------------------------

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
    if (!m_manifest) {
        checkForUpdates();
        return;
    }

    auto* existing = findInstance();

    QMap<QString, QString> extraInfo;
    extraInfo.insert("pack_id", BuildConfig.BEYONDER_PACK_ID);
    extraInfo.insert("pack_version_id", m_manifest->packVersion);
    if (existing) {
        extraInfo.insert("original_instance_id", existing->id());
    }

    // Trusted: the manifest URL is baked into this build, so we accept downloads from the hosts it lists.
    auto* importTask = new InstanceImportTask(m_manifest->mrpackUrl, true, this, std::move(extraInfo));
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
    m_progress->setRange(0, 0);
    setState(State::Installing, existing ? tr("Updating Beyonder…") : tr("Installing Beyonder…"));

    connect(m_installTask.get(), &Task::status, this, [this](const QString& status) { m_statusLabel->setText(status); });
    connect(m_installTask.get(), &Task::progress, this, [this](qint64 current, qint64 total) {
        if (total > 0) {
            m_progress->setRange(0, 1000);
            m_progress->setValue(static_cast<int>(current * 1000 / total));
        }
    });
    connect(m_installTask.get(), &Task::succeeded, this, [this, launchAfter] {
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
    if (!m_manifest || m_manifest->recommendedMemoryMB <= 0) {
        return;
    }
    auto settings = instance->settings();
    // Only set memory the first time; after that the player's own choice wins.
    if (settings->get("OverrideMemory").toBool()) {
        return;
    }
    settings->set("OverrideMemory", true);
    settings->set("MaxMemAlloc", m_manifest->recommendedMemoryMB);
    settings->set("MinMemAlloc", std::min(1024, m_manifest->recommendedMemoryMB));
}

// ---------------------------------------------------------------------------------------------------------------------
// Play / accounts
// ---------------------------------------------------------------------------------------------------------------------

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
    connect(m_accountMenu->addAction(tr("Add Microsoft account…")), &QAction::triggered, this, &BeyonderWindow::addMicrosoftAccount);
    connect(m_accountMenu->addAction(tr("Manage accounts…")), &QAction::triggered, this,
            [this] { APPLICATION->ShowGlobalSettings(this, "accounts"); });

    if (current) {
        m_accountButton->setIcon(QIcon(current->getFace()));
        m_accountButton->setText(current->profileName());
    } else {
        m_accountButton->setIcon(QIcon::fromTheme("noaccount"));
        m_accountButton->setText(tr("Sign in"));
    }
}
