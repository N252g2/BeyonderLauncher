// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Beyonder Launcher - a fork of Prism Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "BeyonderStyle.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSlider>
#include <QStyle>
#include <QVBoxLayout>

namespace Beyonder {

namespace {
const char* const s_baseSheet = R"(
* {
    font-family: "Segoe UI Variable Text", "Segoe UI", "Inter", "SF Pro Text", "Helvetica Neue", "Noto Sans", sans-serif;
    font-size: 13px;
    color: #ededef;
}
#bRoot { background: #0e0e10; }
#bTitleBar, #bSidebar, #bStatusBar { background: #0e0e10; }
#bTitleBar { border-bottom: 1px solid #1d1d21; }
#bStatusBar { border-top: 1px solid #1d1d21; }
#bSidebar { border-right: 1px solid #1d1d21; }
#bContent { background: @CONTENT_BG@; }

#bBrand { font-size: 15px; font-weight: 700; letter-spacing: 1px; }
#bPill {
    color: #b4b4bc; font-size: 11px; padding: 2px 7px;
    border: 1px solid #2a2a2f; border-radius: 5px; background: #151518;
}

/* window controls */
#bWinButton { background: transparent; border: none; border-radius: 6px; padding: 6px; }
#bWinButton:hover { background: #1c1c20; }
#bCloseButton { background: transparent; border: none; border-radius: 6px; padding: 6px; }
#bCloseButton:hover { background: #c42b1c; }

/* green call-to-action (updates) */
#bGreenButton {
    background: #4ade80; color: #052e12; font-weight: 600;
    border: none; border-radius: 6px; padding: 7px 14px;
}
#bGreenButton:hover { background: #6ee7a0; }

#bAccount {
    background: #151518; border: 1px solid #26262b; border-radius: 8px; padding: 4px 10px 4px 6px;
    font-weight: 600;
}
#bAccount:hover { border-color: #34343a; }
#bAccount::menu-indicator { image: none; width: 0; }

/* sidebar */
#bSection { color: #6b6b74; font-size: 11px; font-weight: 600; letter-spacing: 1px; padding: 4px 10px; }
#bNav {
    text-align: left; padding: 9px 10px; border: 1px solid transparent; border-radius: 7px;
    background: transparent; color: #c9c9d0;
}
#bNav:hover { background: #151518; }
#bNav:checked { background: #1a1a1e; border-color: #26262b; color: #ffffff; font-weight: 600; }
#bQuick {
    text-align: left; background: #131316; border: 1px solid #232327; border-radius: 9px; padding: 8px;
}
#bQuick:hover { border-color: #34343a; }
#bQuickTitle { font-weight: 600; }
#bQuickStatus { color: #8b8b93; font-size: 11px; }
#bCommunity {
    background: transparent; border: 1px solid #26262b; border-radius: 7px; padding: 7px 8px; color: #c9c9d0;
}
#bCommunity:hover { background: #151518; }
#bSideUpdate {
    text-align: left; background: rgba(74, 222, 128, 0.07); border: 1px solid rgba(74, 222, 128, 0.45);
    border-radius: 9px; padding: 10px;
}
#bSideUpdate:hover { background: rgba(74, 222, 128, 0.12); }
#bSideUpdateTitle { color: #4ade80; font-weight: 600; }
#bSideUpdateText { color: #b4b4bc; font-size: 11px; }

/* content */
#bH1 { font-size: 26px; font-weight: 700; }
#bCount { font-size: 11px; color: #b4b4bc; border: 1px solid #2a2a2f; border-radius: 5px; padding: 1px 7px; background: #121214; }
#bSubtle { color: #8b8b93; }
#bStrip { background: #0e0e10; border: 1px solid #1d1d21; border-radius: 9px; }
#bStripText { font-weight: 600; }
#bGhost {
    background: #121214; border: 1px solid #26262b; border-radius: 7px; padding: 6px 12px; color: #d6d6dc;
}
#bGhost:hover { border-color: #3a3a40; }

#bCard { background: #0e0e10; border: 1px solid #1d1d21; border-radius: 11px; }
#bCardTitle { font-size: 15px; font-weight: 700; }
#bCardSub { color: #8b8b93; font-size: 12px; }
#bBadge {
    background: rgba(10, 10, 11, 0.82); border: 1px solid #34343a; border-radius: 5px;
    padding: 2px 7px; font-size: 11px; font-weight: 600;
}
#bBadgeGreen { background: rgba(20, 60, 35, 0.9); border: 1px solid rgba(74,222,128,0.5); color: #4ade80; border-radius: 5px; padding: 2px 7px; font-size: 11px; font-weight: 600; }
#bBadgeAmber { background: rgba(70, 50, 10, 0.9); border: 1px solid rgba(245,184,74,0.5); color: #f5b84a; border-radius: 5px; padding: 2px 7px; font-size: 11px; font-weight: 600; }
#bBadgeRed { background: rgba(70, 15, 15, 0.9); border: 1px solid rgba(239,68,68,0.5); color: #f87171; border-radius: 5px; padding: 2px 7px; font-size: 11px; font-weight: 600; }
#bCoverText { color: #d6d6dc; font-size: 11px; font-weight: 600; }

#bPlay {
    background: @ACCENT@; color: #ffffff; font-weight: 700; font-size: 13px; letter-spacing: 0.5px;
    border: 1px solid @ACCENT_BORDER@; border-radius: 8px; padding: 9px 14px;
}
#bPlay:hover { background: @ACCENT_HOVER@; }
#bPlay:disabled { background: #18181b; border-color: #26262b; color: #6b6b74; }
#bPlay[running="true"] { background: rgba(74, 222, 128, 0.1); border-color: rgba(74, 222, 128, 0.45); color: #4ade80; }
#bPlay[blocked="true"] { background: rgba(239, 68, 68, 0.1); border-color: rgba(239, 68, 68, 0.45); color: #f87171; }
#bPlay[blocked="true"]:hover { background: rgba(239, 68, 68, 0.16); }
#bEdition {
    background: rgba(10, 10, 11, 0.82); border: 1px solid #34343a; border-radius: 5px;
    padding: 2px 7px; font-size: 11px; font-weight: 600;
}
#bEdition:hover { border-color: #5a5a62; }
#bEdition::menu-indicator { image: none; width: 0; }
#bIconButton { background: #121214; border: 1px solid #26262b; border-radius: 8px; padding: 8px; }
#bIconButton:hover { border-color: #3a3a40; }

#bDashed {
    background: transparent; border: 1px dashed #2c2c32; border-radius: 11px; color: #8b8b93; padding: 16px;
}
#bDashed:hover { border-color: #45454c; background: #0d0d0f; }

#bNewsView { background: #0e0e10; border: 1px solid #1d1d21; border-radius: 11px; padding: 18px; }

/* status bar */
#bStatusText { color: #4ade80; font-size: 12px; }
#bStatusText[idle="true"] { color: #8b8b93; }
#bFooterButton { background: transparent; border: none; color: #8b8b93; font-size: 11px; font-weight: 600; letter-spacing: 0.5px; padding: 4px 8px; }
#bFooterButton:hover { color: #ededef; }
QProgressBar { background: #1a1a1e; border: none; border-radius: 3px; max-height: 6px; min-height: 6px; }
QProgressBar::chunk { background: #4ade80; border-radius: 3px; }

/* toast */
#bToast { background: #0e0e10; border: 1px solid #26262b; border-radius: 11px; }
#bToastIcon { background: rgba(74, 222, 128, 0.12); border: 1px solid rgba(74, 222, 128, 0.4); border-radius: 9px; }
#bToastTitle { font-weight: 700; }
#bToastText { color: #b4b4bc; font-size: 12px; }

QMenu { background: #121214; border: 1px solid #26262b; border-radius: 8px; padding: 5px; }
QMenu::item { padding: 7px 22px 7px 26px; border-radius: 5px; color: #ededef; }
QMenu::item:selected { background: #1f1f24; }
QMenu::separator { height: 1px; background: #26262b; margin: 5px 6px; }
QMenu::indicator { width: 8px; height: 8px; left: 10px; }
QMenu::indicator:checked { background: #4ade80; border-radius: 4px; }
QMenu::indicator:unchecked { background: transparent; }
QToolTip { background: #121214; color: #ededef; border: 1px solid #26262b; padding: 4px 6px; }
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: #2a2a2f; border-radius: 4px; min-height: 30px; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; }
)";

const char* const s_pagesSheet = R"(
/* settings & account pages */
#sNavCard, #sCard { background: #0e0e10; border: 1px solid #1d1d21; border-radius: 11px; }
#sSubNav {
    text-align: left; padding: 9px 10px; border: 1px solid transparent; border-radius: 7px;
    background: transparent; color: #c9c9d0;
}
#sSubNav:hover { background: #151518; }
#sSubNav:checked { background: #1a1a1e; border-color: #2c2c32; color: #ffffff; font-weight: 600; }
#sCardTitle { font-size: 15px; font-weight: 700; }
#sCardSub { color: #8b8b93; font-size: 12px; }
#sRow { background: #121214; border: 1px solid #232327; border-radius: 9px; }
#sRowTitle { font-weight: 600; }
#sRowDesc { color: #8b8b93; font-size: 12px; }
#sTick { color: #6b6b74; font-size: 11px; }
#sPill { background: #0a0a0b; border: 1px solid #2c2c32; border-radius: 6px; padding: 3px 9px; font-weight: 700; font-size: 12px; }
#sStat { background: #121214; border: 1px solid #232327; border-radius: 9px; }
#sStatLabel { color: #8b8b93; font-size: 10px; font-weight: 600; letter-spacing: 1px; }
#sStatValue { font-size: 15px; font-weight: 700; }
#sValue { font-weight: 600; }
#sKey { color: #8b8b93; }
#sWhite { background: #f3f3f5; color: #0a0a0b; font-weight: 600; border: none; border-radius: 7px; padding: 7px 14px; }
#sWhite:hover { background: #ffffff; }
#sWhite:disabled { background: #2a2a2f; color: #6b6b74; }
#sDanger { background: rgba(239, 68, 68, 0.08); color: #f87171; font-weight: 600; border: 1px solid rgba(239, 68, 68, 0.35); border-radius: 7px; padding: 7px 14px; }
#sDanger:hover { background: rgba(239, 68, 68, 0.15); }
#sAccentCard { background: #121214; border: 1px solid #232327; border-radius: 9px; text-align: left; padding: 12px; }
#sAccentCard:hover { border-color: #3a3a40; }
#sAccentCard:checked { border: 1px solid #ededef; }
#sSegment { background: #121214; border: 1px solid #232327; border-radius: 7px; padding: 7px 14px; color: #c9c9d0; }
#sSegment:checked { background: #f3f3f5; color: #0a0a0b; font-weight: 600; border-color: #f3f3f5; }
#sMono { font-family: "Cascadia Mono", "Consolas", "SF Mono", "DejaVu Sans Mono", monospace; font-size: 12px; color: #b4b4bc; }
#sProfileName { font-size: 20px; font-weight: 700; }
#sBanner { border-top-left-radius: 11px; border-top-right-radius: 11px; }
QScrollArea { background: transparent; border: none; }
QScrollArea > QWidget > QWidget { background: transparent; }
QSlider::groove:horizontal { height: 4px; background: #26262b; border-radius: 2px; }
QSlider::sub-page:horizontal { background: #ededef; border-radius: 2px; }
QSlider::handle:horizontal { background: #ffffff; width: 16px; height: 16px; margin: -6px 0; border-radius: 4px; }
QSlider::handle:horizontal:hover { background: #f3f3f5; }
QComboBox { background: #121214; border: 1px solid #2c2c32; border-radius: 7px; padding: 6px 10px; min-width: 120px; }
QComboBox QAbstractItemView { background: #121214; border: 1px solid #26262b; selection-background-color: #1f1f24; }
)";

}  // namespace

const QList<Accent>& accents()
{
    static const QList<Accent> s_accents = {
        { "crimson", QObject::tr("Crimson Moon"), QObject::tr("The red moon over Tingen"), QColor("#c0303a") },
        { "evernight", QObject::tr("Evernight"), QObject::tr("Deep violet of a starless night"), QColor("#7c4dcc") },
        { "storm", QObject::tr("Storm Blue"), QObject::tr("Cold blue of the open sea"), QColor("#2f6fd1") },
        { "sun", QObject::tr("Eternal Sun"), QObject::tr("Burnished gold and warm light"), QColor("#c99a2e") },
        { "steam", QObject::tr("Steam & Brass"), QObject::tr("Copper pipes and clockwork"), QColor("#b8692e") },
        { "fog", QObject::tr("Gray Fog"), QObject::tr("Silver mist above the fog"), QColor("#7d7a8c") },
    };
    return s_accents;
}

const Accent& accent(const QString& id)
{
    for (const auto& a : accents()) {
        if (a.id == id)
            return a;
    }
    return accents().first();
}

QString styleSheet(const QString& accentId, bool backdrop)
{
    const QColor c = accent(accentId).color;
    const QString content = backdrop
                                ? QString("qradialgradient(cx:0.85, cy:0.0, radius:1.1, fx:0.85, fy:0.0, stop:0 rgba(%1, %2, %3, 46), stop:0.6 #0a0a0b, stop:1 #0a0a0b)")
                                      .arg(c.red())
                                      .arg(c.green())
                                      .arg(c.blue())
                                : QStringLiteral("#0a0a0b");
    QString s = QString::fromUtf8(s_baseSheet) + QString::fromUtf8(s_pagesSheet);
    s.replace("@ACCENT_HOVER@", c.lighter(112).name());
    s.replace("@ACCENT_BORDER@", c.lighter(125).name());
    s.replace("@ACCENT@", c.name());
    s.replace("@CONTENT_BG@", content);
    return s;
}

QIcon lineIcon(const QString& body, const QString& color, int size, bool filled)
{
    const int px = size * 2;
    const QString svg =
        QString(R"(<svg xmlns="http://www.w3.org/2000/svg" width="%1" height="%1" viewBox="0 0 24 24" fill="%2" stroke="%3" )"
                R"(stroke-width="2" stroke-linecap="round" stroke-linejoin="round">%4</svg>)")
            .arg(px)
            .arg(filled ? color : QStringLiteral("none"), color, body);
    QPixmap pm;
    pm.loadFromData(svg.toUtf8(), "SVG");
    pm.setDevicePixelRatio(2.0);
    return QIcon(pm);
}

QString humanHours(int64_t seconds)
{
    const double hours = seconds / 3600.0;
    if (hours < 0.1) {
        return QObject::tr("%1 min").arg(qMax<int64_t>(seconds / 60, 0));
    }
    return QObject::tr("%1 hrs").arg(hours, 0, 'f', 1);
}

QLabel* makeLabel(const QString& text, const char* objectName, QWidget* parent)
{
    auto* l = new QLabel(text, parent);
    l->setObjectName(objectName);
    return l;
}

void restyle(QWidget* w)
{
    w->style()->unpolish(w);
    w->style()->polish(w);
}


Card makeCard(const QString& title, const QString& subtitle, QWidget* action, QWidget* parent)
{
    auto* frame = new QFrame(parent);
    frame->setObjectName("sCard");
    auto* outer = new QVBoxLayout(frame);
    outer->setContentsMargins(20, 18, 20, 20);
    outer->setSpacing(14);
    if (!title.isEmpty() || action) {
        auto* header = new QHBoxLayout();
        auto* texts = new QVBoxLayout();
        texts->setSpacing(2);
        if (!title.isEmpty()) {
            auto* t = makeLabel(title, "sCardTitle", frame);
            t->setWordWrap(true);
            texts->addWidget(t);
        }
        if (!subtitle.isEmpty()) {
            auto* sub = makeLabel(subtitle, "sCardSub", frame);
            sub->setWordWrap(true);
            texts->addWidget(sub);
        }
        header->addLayout(texts, 1);
        if (action) {
            action->setParent(frame);
            header->addWidget(action, 0, Qt::AlignTop);
        }
        outer->addLayout(header);
    }
    auto* body = new QVBoxLayout();
    body->setSpacing(10);
    outer->addLayout(body);
    return { frame, body };
}

QFrame* makeRow(const QString& title, const QString& description, QWidget* control, QWidget* parent)
{
    auto* row = new QFrame(parent);
    row->setObjectName("sRow");
    auto* l = new QHBoxLayout(row);
    l->setContentsMargins(16, 12, 16, 12);
    l->setSpacing(16);
    auto* texts = new QVBoxLayout();
    texts->setSpacing(2);
    auto* t = makeLabel(title, "sRowTitle", row);
    t->setWordWrap(true);
    texts->addWidget(t);
    if (!description.isEmpty()) {
        auto* d = makeLabel(description, "sRowDesc", row);
        d->setWordWrap(true);
        texts->addWidget(d);
    }
    l->addLayout(texts, 1);
    if (control) {
        control->setParent(row);
        l->addWidget(control, 0, Qt::AlignVCenter);
    }
    return row;
}

QFrame* makeSliderRow(const QString& title,
                      const QString& description,
                      QSlider* slider,
                      QLabel* valuePill,
                      const QString& minText,
                      const QString& midText,
                      const QString& maxText,
                      QWidget* parent)
{
    auto* row = new QFrame(parent);
    row->setObjectName("sRow");
    auto* l = new QVBoxLayout(row);
    l->setContentsMargins(16, 12, 16, 12);
    l->setSpacing(10);
    auto* top = new QHBoxLayout();
    auto* texts = new QVBoxLayout();
    texts->setSpacing(2);
    texts->addWidget(makeLabel(title, "sRowTitle", row));
    if (!description.isEmpty()) {
        auto* d = makeLabel(description, "sRowDesc", row);
        d->setWordWrap(true);
        texts->addWidget(d);
    }
    top->addLayout(texts, 1);
    valuePill->setParent(row);
    valuePill->setObjectName("sPill");
    top->addWidget(valuePill, 0, Qt::AlignTop);
    l->addLayout(top);
    slider->setParent(row);
    slider->setOrientation(Qt::Horizontal);
    slider->setCursor(Qt::PointingHandCursor);
    l->addWidget(slider);
    auto* ticks = new QHBoxLayout();
    ticks->addWidget(makeLabel(minText, "sTick", row));
    ticks->addStretch(1);
    ticks->addWidget(makeLabel(midText, "sTick", row));
    ticks->addStretch(1);
    ticks->addWidget(makeLabel(maxText, "sTick", row));
    l->addLayout(ticks);
    return row;
}

QFrame* makeStatTile(const QString& label, QLabel* value, const char* icon, QWidget* parent)
{
    auto* tile = new QFrame(parent);
    tile->setObjectName("sStat");
    auto* l = new QVBoxLayout(tile);
    l->setContentsMargins(14, 12, 14, 12);
    l->setSpacing(4);
    auto* top = new QHBoxLayout();
    top->setSpacing(6);
    if (icon) {
        auto* i = new QLabel(tile);
        i->setPixmap(lineIcon(icon, Color::Muted, 12).pixmap(12, 12));
        top->addWidget(i);
    }
    top->addWidget(makeLabel(label.toUpper(), "sStatLabel", tile), 1);
    l->addLayout(top);
    value->setParent(tile);
    value->setObjectName("sStatValue");
    l->addWidget(value);
    return tile;
}

namespace {
QPushButton* styledButton(const char* name, const QString& text, const char* icon, const QString& iconColor, QWidget* parent)
{
    auto* b = new QPushButton(text, parent);
    b->setObjectName(name);
    b->setCursor(Qt::PointingHandCursor);
    if (icon) {
        b->setIcon(lineIcon(icon, iconColor, 15));
        b->setIconSize(QSize(15, 15));
        b->setText(" " + text);
    }
    return b;
}
}  // namespace

QPushButton* makeWhiteButton(const QString& text, const char* icon, QWidget* parent)
{
    return styledButton("sWhite", text, icon, "#0a0a0b", parent);
}

QPushButton* makeGhostButton(const QString& text, const char* icon, QWidget* parent)
{
    return styledButton("bGhost", text, icon, "#d6d6dc", parent);
}

QPushButton* makeDangerButton(const QString& text, const char* icon, QWidget* parent)
{
    return styledButton("sDanger", text, icon, "#f87171", parent);
}

QPushButton* makeSubNavButton(const char* icon, const QString& text, QWidget* parent)
{
    auto* b = new QPushButton(lineIcon(icon, "#c9c9d0", 16), "  " + text, parent);
    b->setObjectName("sSubNav");
    b->setCheckable(true);
    b->setAutoExclusive(true);
    b->setCursor(Qt::PointingHandCursor);
    b->setIconSize(QSize(16, 16));
    return b;
}

ToggleSwitch::ToggleSwitch(QWidget* parent) : QAbstractButton(parent)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
}

QSize ToggleSwitch::sizeHint() const
{
    return { 40, 22 };
}

void ToggleSwitch::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF track = QRectF(rect()).adjusted(1, 1, -1, -1);
    const bool on = isChecked();
    const qreal alpha = isEnabled() ? 1.0 : 0.4;
    p.setOpacity(alpha);
    p.setPen(on ? Qt::NoPen : QPen(QColor("#34343a"), 1));
    p.setBrush(on ? QColor("#f3f3f5") : QColor("#1a1a1e"));
    p.drawRoundedRect(track, 6, 6);
    const qreal knob = track.height() - 6;
    const qreal x = on ? track.right() - knob - 3 : track.left() + 3;
    p.setPen(Qt::NoPen);
    p.setBrush(on ? QColor("#0a0a0b") : QColor("#6b6b74"));
    p.drawRoundedRect(QRectF(x, track.top() + 3, knob, knob), 4, 4);
}

}  // namespace Beyonder
