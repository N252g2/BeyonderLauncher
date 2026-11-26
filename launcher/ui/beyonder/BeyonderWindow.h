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

class QLabel;
class QProgressBar;
class QPushButton;
class QTextBrowser;
class QToolButton;
class QMenu;
class MinecraftInstance;

/**
 * Single-game home screen: one managed Beyonder instance, installed and kept up to date
 * from the pack manifest, launched straight into the Beyonder server.
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

   private slots:
    void checkForUpdates();
    void onPlayClicked();
    void rebuildAccountMenu();

   private:
    enum class State : std::uint8_t { Checking, NeedsInstall, NeedsUpdate, Ready, Installing, Running, Error };

    void buildUi();
    void setState(State state, const QString& message = QString());
    void onManifestDownloaded(const QByteArray& data);
    void onManifestFailed(const QString& reason);
    void refreshState();

    MinecraftInstance* findInstance() const;
    void watchInstance(MinecraftInstance* instance);
    void startInstall(bool launchAfter);
    void applyRecommendedSettings(MinecraftInstance* instance);
    void launchGame();
    MinecraftAccountPtr ensureAccount();
    void addMicrosoftAccount();

    State m_state = State::Checking;
    std::optional<BeyonderManifest> m_manifest;
    QString m_manifestError;

    NetJob::Ptr m_manifestJob;
    unique_qobject_ptr<Task> m_installTask;
    QPointer<MinecraftInstance> m_watched;

    QLabel* m_logo = nullptr;
    QTextBrowser* m_motd = nullptr;
    QToolButton* m_accountButton = nullptr;
    QMenu* m_accountMenu = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_versionLabel = nullptr;
    QProgressBar* m_progress = nullptr;
    QPushButton* m_playButton = nullptr;
};
