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
class QButtonGroup;
class QLabel;
class QPushButton;
class QSlider;
class QStackedWidget;
class QVBoxLayout;

namespace Beyonder {
class ToggleSwitch;
}

/**
 * "Launcher Settings" page of the Beyonder window. Every control here reads and writes a real setting:
 * Beyonder's own (accent, auto-update, check interval, edition) or Prism's (downloads, Java, console, window).
 */
class BeyonderSettingsPage : public QWidget {
   public:
    explicit BeyonderSettingsPage(BeyonderWindow* window);

    // Re-read everything that can change outside this page (manifest, installed version, memory, hardware).
    void refresh();

   protected:
    void showEvent(QShowEvent* event) override;

   private:
    QWidget* buildGeneral();
    QWidget* buildAppearance();
    QWidget* buildUpdates();
    QWidget* buildDownloads();
    QWidget* buildGame();
    QWidget* buildHardware();

    Beyonder::ToggleSwitch* boundToggle(const QString& setting);
    QWidget* page(QWidget* first, QWidget* second = nullptr);
    void optimizeAutomatically();
    QString recommendedEditionId() const;

    BeyonderWindow* m_window;
    QStackedWidget* m_stack = nullptr;

    // Appearance
    QButtonGroup* m_accentGroup = nullptr;

    // Updates
    QLabel* m_updateSummary = nullptr;
    QPushButton* m_launcherDownload = nullptr;

    // Game
    QWidget* m_editionBox = nullptr;
    QVBoxLayout* m_editionLayout = nullptr;
    QSlider* m_memorySlider = nullptr;
    QLabel* m_memoryValue = nullptr;
    QLabel* m_memoryHint = nullptr;

    // Hardware
    QLabel* m_cpu = nullptr;
    QLabel* m_gpu = nullptr;
    QLabel* m_ram = nullptr;
    QLabel* m_disk = nullptr;
    QLabel* m_recommended = nullptr;
};
