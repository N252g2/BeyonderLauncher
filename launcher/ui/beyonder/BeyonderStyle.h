// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Beyonder Launcher - a fork of Prism Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

// Shared look and feel for the Beyonder windows and pages: colors, icons, stylesheet, small widgets.

#pragma once

#include <QAbstractButton>
#include <QColor>
#include <QIcon>
#include <QList>
#include <QString>
#include <cstdint>

class QBoxLayout;
class QFrame;
class QLabel;
class QPushButton;
class QSlider;
class QVBoxLayout;
class QWidget;

namespace Beyonder {

namespace Color {
inline constexpr const char* Green = "#4ade80";
inline constexpr const char* Amber = "#f5b84a";
inline constexpr const char* Red = "#ef4444";
inline constexpr const char* Muted = "#8b8b93";
}  // namespace Color

namespace Icon {
inline constexpr const char* Home = R"(<path d="M3 11 12 4l9 7"/><path d="M5 10v10h14V10"/><path d="M10 20v-6h4v6"/>)";
inline constexpr const char* News = R"(<rect x="3" y="5" width="18" height="15" rx="2"/><path d="M7 9h10M7 13h10M7 17h6"/>)";
inline constexpr const char* Settings =
    R"(<circle cx="12" cy="12" r="3"/><path d="M12 2v3M12 19v3M2 12h3M19 12h3M4.9 4.9l2.1 2.1M17 17l2.1 2.1M4.9 19.1 7 17M17 7l2.1-2.1"/>)";
inline constexpr const char* Grid = R"(<rect x="3" y="3" width="7" height="7" rx="1.5"/><rect x="14" y="3" width="7" height="7" rx="1.5"/><rect x="3" y="14" width="7" height="7" rx="1.5"/><rect x="14" y="14" width="7" height="7" rx="1.5"/>)";
inline constexpr const char* Folder = R"(<path d="M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v9a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/>)";
inline constexpr const char* Sliders = R"(<path d="M4 7h9M17 7h3M4 17h3M11 17h9"/><circle cx="15" cy="7" r="2"/><circle cx="9" cy="17" r="2"/>)";
inline constexpr const char* Refresh = R"(<path d="M20 11a8 8 0 1 0-2.3 5.7"/><path d="M20 4v7h-7"/>)";
inline constexpr const char* Download = R"(<path d="M12 4v11"/><path d="m7 10 5 5 5-5"/><path d="M5 20h14"/>)";
inline constexpr const char* Chat = R"(<path d="M4 5h16v11H9l-5 4z"/>)";
inline constexpr const char* Globe = R"(<circle cx="12" cy="12" r="9"/><path d="M3 12h18M12 3c3 3.5 3 14.5 0 18M12 3c-3 3.5-3 14.5 0 18"/>)";
inline constexpr const char* Activity = R"(<path d="M3 12h4l3-7 4 14 3-7h4"/>)";
inline constexpr const char* Info = R"(<circle cx="12" cy="12" r="9"/><path d="M12 11v6M12 7.5v.5"/>)";
inline constexpr const char* Check = R"(<circle cx="12" cy="12" r="9"/><path d="m8 12 3 3 5-6"/>)";
inline constexpr const char* Play = R"(<path d="M8 5v14l11-7z"/>)";
inline constexpr const char* Chip = R"(<rect x="6" y="6" width="12" height="12" rx="2"/><path d="M9 2v3M15 2v3M9 19v3M15 19v3M2 9h3M2 15h3M19 9h3M19 15h3"/>)";
inline constexpr const char* Close = R"(<path d="M6 6l12 12M18 6 6 18"/>)";
inline constexpr const char* Minimize = R"(<path d="M6 12h12"/>)";
inline constexpr const char* Maximize = R"(<rect x="6" y="6" width="12" height="12" rx="1"/>)";
inline constexpr const char* Restore = R"(<rect x="5" y="8" width="11" height="11" rx="1"/><path d="M8 8V5h11v11h-3"/>)";
inline constexpr const char* Bell = R"(<path d="M6 16V11a6 6 0 0 1 12 0v5l2 2H4z"/><path d="M10 20a2 2 0 0 0 4 0"/>)";
inline constexpr const char* Palette = R"(<path d="M12 3a9 9 0 1 0 0 18c1.2 0 2-.8 2-1.8 0-1.4-1.4-1.6-1.4-3 0-1 .8-1.7 1.8-1.7H17a4 4 0 0 0 4-4C21 6.4 17 3 12 3z"/><circle cx="7.5" cy="11" r="1"/><circle cx="10" cy="7" r="1"/><circle cx="15" cy="7.5" r="1"/>)";
inline constexpr const char* Game = R"(<rect x="2" y="7" width="20" height="11" rx="4"/><path d="M7 11v4M5 13h4"/><circle cx="16" cy="12" r="1"/><circle cx="18" cy="14" r="1"/>)";
inline constexpr const char* User = R"(<circle cx="12" cy="8" r="4"/><path d="M4 21c1.5-4 4.5-6 8-6s6.5 2 8 6"/>)";
inline constexpr const char* Link = R"(<path d="M10 14a4 4 0 0 0 5.7 0l3-3a4 4 0 0 0-5.7-5.7l-1 1"/><path d="M14 10a4 4 0 0 0-5.7 0l-3 3a4 4 0 0 0 5.7 5.7l1-1"/>)";
inline constexpr const char* Key = R"(<circle cx="8" cy="15" r="4"/><path d="m11 12 9-9M16 7l3 3M14 9l2 2"/>)";
inline constexpr const char* LogOut = R"(<path d="M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4"/><path d="m16 17 5-5-5-5M21 12H9"/>)";
inline constexpr const char* Clock = R"(<circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/>)";
inline constexpr const char* Sparkle = R"(<path d="M12 3v4M12 17v4M3 12h4M17 12h4M6 6l2.5 2.5M15.5 15.5 18 18M6 18l2.5-2.5M15.5 8.5 18 6"/>)";
inline constexpr const char* Copy = R"(<rect x="9" y="9" width="11" height="11" rx="2"/><path d="M5 15V5a2 2 0 0 1 2-2h8"/>)";
inline constexpr const char* Shield = R"(<path d="M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6z"/>)";
inline constexpr const char* Alert = R"(<path d="M12 3 2 20h20z"/><path d="M12 10v4M12 17v.5"/>)";
}  // namespace Icon

// Accent color presets for the Appearance settings.
struct Accent {
    QString id;
    QString name;
    QString description;
    QColor color;
};
const QList<Accent>& accents();
const Accent& accent(const QString& id);

// Full stylesheet for the Beyonder window, using the chosen accent; backdrop adds a soft accent glow behind the pages.
QString styleSheet(const QString& accentId, bool backdrop);

QIcon lineIcon(const QString& body, const QString& color = QStringLiteral("#ededef"), int size = 18, bool filled = false);
QLabel* makeLabel(const QString& text, const char* objectName, QWidget* parent);
void restyle(QWidget* w);
QString humanHours(int64_t seconds);

// --- small building blocks used by the Settings and Account pages ---

// Rounded card with a title, subtitle and an optional action on the right. Add content to the returned body layout.
struct Card {
    QFrame* frame;
    QVBoxLayout* body;
};
Card makeCard(const QString& title, const QString& subtitle, QWidget* action = nullptr, QWidget* parent = nullptr);

// Bordered row: title + description on the left, a control on the right.
QFrame* makeRow(const QString& title, const QString& description, QWidget* control, QWidget* parent = nullptr);

// Bordered row with a slider underneath, a value pill on the right and three tick labels.
QFrame* makeSliderRow(const QString& title,
                      const QString& description,
                      QSlider* slider,
                      QLabel* valuePill,
                      const QString& minText,
                      const QString& midText,
                      const QString& maxText,
                      QWidget* parent = nullptr);

// Small "label / value" tile (e.g. PLAYTIME / 2.7 hrs).
QFrame* makeStatTile(const QString& label, QLabel* value, const char* icon, QWidget* parent = nullptr);

// White primary button and dark secondary button.
QPushButton* makeWhiteButton(const QString& text, const char* icon = nullptr, QWidget* parent = nullptr);
QPushButton* makeGhostButton(const QString& text, const char* icon = nullptr, QWidget* parent = nullptr);
QPushButton* makeDangerButton(const QString& text, const char* icon = nullptr, QWidget* parent = nullptr);

// Checkable sub-navigation button used in the left column of Settings and Account.
QPushButton* makeSubNavButton(const char* icon, const QString& text, QWidget* parent = nullptr);

// On/off switch.
class ToggleSwitch : public QAbstractButton {
   public:
    explicit ToggleSwitch(QWidget* parent = nullptr);
    QSize sizeHint() const override;

   protected:
    void paintEvent(QPaintEvent* event) override;
};

}  // namespace Beyonder
