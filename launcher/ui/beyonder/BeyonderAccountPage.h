// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Beyonder Launcher - a fork of Prism Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include <QWidget>

class BeyonderWindow;
class QLabel;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;

/**
 * "Account" page: Minecraft profile and play stats, Minecraft + Discord connections, and sign-out.
 * Discord and character data come from the access service (docs/ACCESS_API.md) when it's configured.
 */
class BeyonderAccountPage : public QWidget {
   public:
    explicit BeyonderAccountPage(BeyonderWindow* window);
    void refresh();

   protected:
    void showEvent(QShowEvent* event) override;

   private:
    QWidget* buildProfile();
    QWidget* buildConnections();
    QWidget* buildSecurity();
    void signOutEverywhere();

    BeyonderWindow* m_window;
    QStackedWidget* m_stack = nullptr;
    QPushButton* m_signOut = nullptr;

    // Profile
    QWidget* m_banner = nullptr;
    QLabel* m_face = nullptr;
    QLabel* m_name = nullptr;
    QLabel* m_subtitle = nullptr;
    QLabel* m_uuid = nullptr;
    QLabel* m_accessBadge = nullptr;
    QLabel* m_playtime = nullptr;
    QLabel* m_sessions = nullptr;
    QLabel* m_lastPlayed = nullptr;
    QLabel* m_sequence = nullptr;

    // Connections
    QVBoxLayout* m_minecraftBody = nullptr;
    QLabel* m_minecraftState = nullptr;
    QVBoxLayout* m_discordBody = nullptr;
    QLabel* m_discordState = nullptr;
};
