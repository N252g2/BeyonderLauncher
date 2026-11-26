// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Beyonder Launcher - a fork of Prism Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include <QMainWindow>
#include <QPointer>
#include <cstdint>
#include <optional>

#include "QObjectPtr.h"
#include "beyonder/BeyonderManifest.h"
#include "minecraft/auth/MinecraftAccount.h"
#include "net/NetJob.h"
#include "tasks/Task.h"
#include "ui/pages/instance/ServerPingTask.h"

class QButtonGroup;
class QFrame;
class QLabel;
class QMenu;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QTextBrowser;
class QTimer;
class QToolButton;
class MinecraftInstance;

/**
 * Single-game home screen: one managed Beyonder instance, installed and kept up to date
 * from the pack manifest, launched straight into the Beyonder server.
 *
 * Frameless window: custom title bar (drag / double-click to maximize), sidebar navigation,
 * library page with a live server strip and the game card, news page, status bar and update toast.
 */
class BeyonderWindow : public QMainWindow {
    Q_OBJECT

   public:
    explicit BeyonderWindow(QWidget* parent = nullptr);
    ~BeyonderWindow() override;

   signals:
    void isClosing();
    void classicViewRequested();

   protected:
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

   private slots:
    void checkForUpdates();
    void onPlayClicked();
    void rebuildAccountMenu();
    void pingServer();

   private:
    enum class State : std::uint8_t { Checking, NeedsInstall, NeedsUpdate, Ready, Installing, Running, Error };

    // UI construction
    QWidget* buildTitleBar();
    QWidget* buildSidebar();
    QWidget* buildLibraryPage();
    QWidget* buildNewsPage();
    QWidget* buildStatusBar();
    QWidget* buildGameCard();
    void buildToast();

    // State
    void setState(State state, const QString& message = QString());
    void updateUi();
    void updateNews();
    void updateServerStrip();
    void updateAccessLabel();
    void rebuildRamMenu();
    void showToast(bool show);
    void positionToast();
    void onManifestDownloaded(const QByteArray& data);
    void onManifestFailed(const QString& reason);
    void refreshState();

    // Instance management
    MinecraftInstance* findInstance() const;
    void watchInstance(MinecraftInstance* instance);
    void startInstall(bool launchAfter);
    void applyRecommendedSettings(MinecraftInstance* instance);
    void launchGame();
    MinecraftAccountPtr ensureAccount();
    void addMicrosoftAccount();
    int currentMaxMemoryMB() const;
    void setMaxMemoryMB(int mb);

    State m_state = State::Checking;
    QString m_statusMessage;
    int m_installPercent = -1;
    std::optional<BeyonderManifest> m_manifest;
    QString m_manifestError;
    QString m_toastDismissedFor;

    NetJob::Ptr m_manifestJob;
    unique_qobject_ptr<Task> m_installTask;
    shared_qobject_ptr<ServerPingTask> m_pingTask;
    QTimer* m_pingTimer = nullptr;
    enum class ServerStatus : std::uint8_t { Unknown, Pinging, Online, Offline, NotConfigured } m_serverStatus = ServerStatus::Unknown;
    int m_serverOnline = -1;
    int m_serverMax = -1;
    qint64 m_serverLatency = -1;
    QPointer<MinecraftInstance> m_watched;

    // Chrome
    QWidget* m_root = nullptr;
    QToolButton* m_maximizeButton = nullptr;
    QPushButton* m_titleUpdateButton = nullptr;
    QToolButton* m_accountButton = nullptr;
    QMenu* m_accountMenu = nullptr;

    // Sidebar
    QStackedWidget* m_pages = nullptr;
    QButtonGroup* m_nav = nullptr;
    QLabel* m_quickStatus = nullptr;
    QLabel* m_quickDot = nullptr;
    QPushButton* m_discordButton = nullptr;
    QPushButton* m_websiteButton = nullptr;
    QPushButton* m_sideUpdateButton = nullptr;
    QLabel* m_sideUpdateText = nullptr;

    // Library page
    QLabel* m_serverDot = nullptr;
    QLabel* m_serverLabel = nullptr;
    QLabel* m_accessLabel = nullptr;
    QWidget* m_cover = nullptr;
    QLabel* m_editionBadge = nullptr;
    QLabel* m_stateBadge = nullptr;
    QLabel* m_installedLabel = nullptr;
    QLabel* m_playtimeBadge = nullptr;
    QLabel* m_cardTitle = nullptr;
    QLabel* m_cardSubtitle = nullptr;
    QPushButton* m_playButton = nullptr;
    QToolButton* m_folderButton = nullptr;
    QToolButton* m_gameSettingsButton = nullptr;
    QPushButton* m_communityCard = nullptr;

    // News page
    QTextBrowser* m_newsView = nullptr;

    // Status bar
    QLabel* m_statusIcon = nullptr;
    QLabel* m_statusText = nullptr;
    QProgressBar* m_statusProgress = nullptr;
    QToolButton* m_ramButton = nullptr;
    QMenu* m_ramMenu = nullptr;

    // Toast
    QFrame* m_toast = nullptr;
    QLabel* m_toastText = nullptr;
};
