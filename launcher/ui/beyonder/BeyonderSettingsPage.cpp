// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Beyonder Launcher - a fork of Prism Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "BeyonderSettingsPage.h"

#include <QButtonGroup>
#include <QDesktopServices>
#include <QDir>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QStackedWidget>
#include <QStorageInfo>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include "Application.h"
#include "BuildConfig.h"
#include "HardwareInfo.h"
#include "Version.h"
#include "minecraft/MinecraftInstance.h"
#include "ui/beyonder/BeyonderStyle.h"
#include "ui/beyonder/BeyonderWindow.h"

using namespace Beyonder;

BeyonderSettingsPage::BeyonderSettingsPage(BeyonderWindow* window) : QWidget(), m_window(window)
{
    auto* l = new QVBoxLayout(this);
    l->setContentsMargins(28, 24, 28, 20);
    l->setSpacing(4);
    l->addWidget(makeLabel(tr("Launcher Settings"), "bH1", this));
    l->addWidget(makeLabel(tr("Choose a category to customize Beyonder Launcher."), "bSubtle", this));
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
    const QList<std::pair<const char*, QString>> categories = {
        { Icon::Sliders, tr("General") },   { Icon::Palette, tr("Appearance") }, { Icon::Refresh, tr("Updates") },
        { Icon::Download, tr("Downloads") }, { Icon::Game, tr("Game") },          { Icon::Chip, tr("Hardware") },
    };
    m_stack->addWidget(buildGeneral());
    m_stack->addWidget(buildAppearance());
    m_stack->addWidget(buildUpdates());
    m_stack->addWidget(buildDownloads());
    m_stack->addWidget(buildGame());
    m_stack->addWidget(buildHardware());

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

void BeyonderSettingsPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    refresh();
}

QWidget* BeyonderSettingsPage::page(QWidget* first, QWidget* second)
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

ToggleSwitch* BeyonderSettingsPage::boundToggle(const QString& setting)
{
    auto* t = new ToggleSwitch();
    t->setChecked(APPLICATION->settings()->get(setting).toBool());
    connect(t, &ToggleSwitch::toggled, this, [setting](bool on) { APPLICATION->settings()->set(setting, on); });
    return t;
}

// ---------------------------------------------------------------------------------------------------------------------

QWidget* BeyonderSettingsPage::buildGeneral()
{
    auto* all = makeGhostButton(tr("All launcher settings"), Icon::Settings);
    connect(all, &QPushButton::clicked, this, [this] { APPLICATION->ShowGlobalSettings(m_window); });
    auto card = makeCard(tr("Interface and Window Behavior"), tr("How the launcher behaves around the game."), all);

    auto* language = makeGhostButton(tr("Change…"));
    connect(language, &QPushButton::clicked, this, [this] { APPLICATION->ShowGlobalSettings(m_window, "language-settings"); });
    card.body->addWidget(makeRow(tr("Display Language"), tr("Matches your system language unless you pick one."), language));
    card.body->addWidget(makeRow(tr("Close launcher when the game starts"), tr("Frees memory for Minecraft. The launcher reopens when you start it again."),
                                 boundToggle("CloseAfterLaunch")));
    card.body->addWidget(makeRow(tr("Quit launcher when the game closes"), tr("Exits completely after you leave Beyonder."),
                                 boundToggle("QuitAfterGameStop")));
    card.body->addWidget(makeRow(tr("Show game log window"), tr("Opens a log window next to Minecraft. Useful for reporting bugs."),
                                 boundToggle("ShowConsole")));
    card.body->addWidget(makeRow(tr("Show game log after a crash"), tr("Opens the log automatically if Minecraft crashes."),
                                 boundToggle("ShowConsoleOnError")));
    card.body->addWidget(makeRow(tr("Start in Advanced view"), tr("Opens the full multi-instance launcher instead of this home screen. Takes effect on restart."),
                                 boundToggle("BeyonderClassicView")));
    return page(card.frame);
}

QWidget* BeyonderSettingsPage::buildAppearance()
{
    auto card = makeCard(tr("Accent Color"), tr("Colors the Play button and the glow behind the launcher."));
    auto* grid = new QGridLayout();
    grid->setSpacing(10);
    m_accentGroup = new QButtonGroup(this);
    m_accentGroup->setExclusive(true);
    const QString current = APPLICATION->settings()->get("BeyonderAccent").toString();
    int i = 0;
    for (const auto& a : accents()) {
        auto* b = new QPushButton();
        b->setObjectName("sAccentCard");
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setMinimumHeight(70);
        auto* bl = new QHBoxLayout(b);
        bl->setContentsMargins(12, 10, 12, 10);
        auto* texts = new QVBoxLayout();
        texts->setSpacing(3);
        texts->addWidget(makeLabel(a.name, "sRowTitle", b));
        auto* d = makeLabel(a.description, "sRowDesc", b);
        d->setWordWrap(true);
        texts->addWidget(d);
        bl->addLayout(texts, 1);
        auto* swatch = new QLabel(b);
        swatch->setFixedSize(20, 20);
        swatch->setStyleSheet(QString("background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 %1, stop:1 %2); border-radius: 5px; border: 1px solid rgba(255,255,255,0.25);")
                                  .arg(a.color.lighter(130).name(), a.color.darker(130).name()));
        bl->addWidget(swatch, 0, Qt::AlignTop);
        for (auto* child : b->findChildren<QLabel*>())
            child->setAttribute(Qt::WA_TransparentForMouseEvents);
        b->setChecked(a.id == current || (i == 0 && current.isEmpty()));
        const QString id = a.id;
        connect(b, &QPushButton::clicked, this, [this, id] {
            APPLICATION->settings()->set("BeyonderAccent", id);
            m_window->applyTheme();
        });
        m_accentGroup->addButton(b);
        grid->addWidget(b, i / 3, i % 3);
        i++;
    }
    card.body->addLayout(grid);

    auto backdrop = makeCard(tr("Backdrop"), tr("Extra touches for the launcher window."));
    auto* glow = boundToggle("BeyonderBackdrop");
    connect(glow, &ToggleSwitch::toggled, this, [this] { m_window->applyTheme(); });
    backdrop.body->addWidget(makeRow(tr("Accent glow"), tr("A soft glow of your accent color behind the pages."), glow));
    auto* prismThemes = makeGhostButton(tr("Open"));
    connect(prismThemes, &QPushButton::clicked, this, [this] { APPLICATION->ShowGlobalSettings(m_window, "appearance-settings"); });
    backdrop.body->addWidget(makeRow(tr("Advanced view theme"), tr("Theme and icons for the full launcher, logs and dialogs."), prismThemes));
    return page(card.frame, backdrop.frame);
}

QWidget* BeyonderSettingsPage::buildUpdates()
{
    auto* check = makeGhostButton(tr("Check Now"), Icon::Refresh);
    connect(check, &QPushButton::clicked, this, [this] { m_window->checkForUpdates(); });
    auto card = makeCard(tr("Launcher and Game Updates"), QString(), check);
    m_updateSummary = makeLabel(QString(), "sCardSub", card.frame);
    m_updateSummary->setWordWrap(true);
    card.body->insertWidget(0, m_updateSummary);

    m_launcherDownload = makeWhiteButton(tr("Download new launcher"), Icon::Download);
    connect(m_launcherDownload, &QPushButton::clicked, this, [this] {
        if (m_window->m_manifest && m_window->m_manifest->launcherUrl.isValid())
            QDesktopServices::openUrl(m_window->m_manifest->launcherUrl);
    });
    card.body->addWidget(m_launcherDownload, 0, Qt::AlignLeft);

    card.body->addWidget(makeRow(tr("Auto update Beyonder"), tr("Downloads and installs new game releases as soon as the launcher sees them."),
                                 boundToggle("BeyonderAutoUpdate")));
    card.body->addWidget(makeRow(tr("Update pop-ups"), tr("Shows a pop-up in the corner when a new Beyonder version is out."),
                                 boundToggle("BeyonderShowToasts")));

    auto* slider = new QSlider();
    slider->setRange(1, 12);  // x5 minutes
    auto* value = new QLabel();
    auto setText = [value](int v) { value->setText(QObject::tr("Every %1 min").arg(v * 5)); };
    slider->setValue(qBound(1, APPLICATION->settings()->get("BeyonderCheckIntervalMin").toInt() / 5, 12));
    setText(slider->value());
    connect(slider, &QSlider::valueChanged, this, [this, setText](int v) {
        setText(v);
        APPLICATION->settings()->set("BeyonderCheckIntervalMin", v * 5);
        if (m_window->m_updateTimer)
            m_window->m_updateTimer->setInterval(v * 5 * 60 * 1000);
    });
    card.body->addWidget(makeSliderRow(tr("Background Update Check"), tr("How often the launcher looks for new Beyonder releases while it's open."),
                                       slider, value, tr("5 min"), tr("30 min"), tr("60 min")));
    return page(card.frame);
}

QWidget* BeyonderSettingsPage::buildDownloads()
{
    auto card = makeCard(tr("Download Speed and Reliability"), tr("Tune how the launcher downloads the game, mods and updates."));

    auto addSlider = [this, &card](const QString& setting, int min, int max, const QString& title, const QString& desc,
                                   std::function<QString(int)> format, const QString& minT, const QString& midT, const QString& maxT) {
        auto* slider = new QSlider();
        slider->setRange(min, max);
        slider->setValue(qBound(min, APPLICATION->settings()->get(setting).toInt(), max));
        auto* value = new QLabel(format(slider->value()));
        connect(slider, &QSlider::valueChanged, this, [setting, value, format](int v) {
            value->setText(format(v));
            APPLICATION->settings()->set(setting, v);
        });
        card.body->addWidget(makeSliderRow(title, desc, slider, value, minT, midT, maxT));
    };
    addSlider("NumberOfConcurrentDownloads", 1, 16, tr("Parallel Downloads"), tr("Higher values download many small mod files faster."),
              [](int v) { return QObject::tr("%n stream(s)", "", v); }, tr("1 (Gentle)"), tr("6 (Default)"), tr("16 (Max)"));
    addSlider("NetworkRetryLimit", 0, 5, tr("Retries"), tr("How many times a failed download is retried before giving up."),
              [](int v) { return QObject::tr("%n retry(s)", "", v); }, tr("0"), tr("2 (Default)"), tr("5"));
    addSlider("RequestTimeout", 5, 60, tr("Connection Timeout"), tr("Raise this on slow or unstable connections."),
              [](int v) { return QObject::tr("%1 s").arg(v); }, tr("5 s"), tr("15 s (Default)"), tr("60 s"));
    return page(card.frame);
}

QWidget* BeyonderSettingsPage::buildGame()
{
    auto editions = makeCard(tr("Edition"), tr("Standard has the full visuals. Lite trades them for higher frame rates on weaker PCs. Switching keeps your worlds and settings."));
    m_editionBox = new QWidget(editions.frame);
    m_editionLayout = new QVBoxLayout(m_editionBox);
    m_editionLayout->setContentsMargins(0, 0, 0, 0);
    editions.body->addWidget(m_editionBox);

    auto* java = makeGhostButton(tr("Java settings…"));
    connect(java, &QPushButton::clicked, this, [this] {
        if (auto* inst = m_window->findInstance())
            APPLICATION->showInstanceWindow(inst, "settings");
        else
            APPLICATION->ShowGlobalSettings(m_window, "java-settings");
    });
    auto card = makeCard(tr("Memory and Java"), tr("How much of your PC Minecraft may use."), java);
    m_memorySlider = new QSlider();
    m_memorySlider->setRange(4, 32);  // x512 MB = 2..16 GB
    m_memoryValue = new QLabel();
    connect(m_memorySlider, &QSlider::valueChanged, this, [this](int v) { m_memoryValue->setText(QString("%1 GB").arg(v / 2.0, 0, 'f', v % 2 ? 1 : 0)); });
    connect(m_memorySlider, &QSlider::sliderReleased, this, [this] { m_window->setMaxMemoryMB(m_memorySlider->value() * 512); });
    connect(m_memorySlider, &QSlider::actionTriggered, this, [this](int action) {
        if (action != QAbstractSlider::SliderMove)
            QTimer::singleShot(0, this, [this] { m_window->setMaxMemoryMB(m_memorySlider->value() * 512); });
    });
    card.body->addWidget(makeSliderRow(tr("Maximum Memory (RAM)"), QString(), m_memorySlider, m_memoryValue, tr("2 GB"), tr("8 GB"), tr("16 GB")));
    m_memoryHint = makeLabel(QString(), "sRowDesc", card.frame);
    m_memoryHint->setWordWrap(true);
    card.body->addWidget(m_memoryHint);

    auto* autoJava = new ToggleSwitch();
    autoJava->setChecked(APPLICATION->settings()->get("AutomaticJavaSwitch").toBool() && APPLICATION->settings()->get("AutomaticJavaDownload").toBool());
    connect(autoJava, &ToggleSwitch::toggled, this, [](bool on) {
        APPLICATION->settings()->set("AutomaticJavaSwitch", on);
        APPLICATION->settings()->set("AutomaticJavaDownload", on);
    });
    card.body->addWidget(makeRow(tr("Manage Java automatically"), tr("Downloads and picks the right Java version for Beyonder."), autoJava));
    card.body->addWidget(makeRow(tr("Start Minecraft maximized"), tr("Opens the game window maximized instead of at its default size."),
                                 boundToggle("LaunchMaximized")));
    return page(editions.frame, card.frame);
}

QWidget* BeyonderSettingsPage::buildHardware()
{
    auto* optimize = makeWhiteButton(tr("Optimize Automatically"), Icon::Sparkle);
    connect(optimize, &QPushButton::clicked, this, &BeyonderSettingsPage::optimizeAutomatically);
    auto card = makeCard(tr("This PC"), tr("Detected specs, used to recommend an edition and memory."), optimize);
    auto addRow = [&card](const QString& key, QLabel*& value) {
        value = makeLabel(QString(), "sValue", card.frame);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        auto* row = new QFrame(card.frame);
        row->setObjectName("sRow");
        auto* l = new QHBoxLayout(row);
        l->setContentsMargins(16, 12, 16, 12);
        l->addWidget(makeLabel(key, "sKey", row));
        l->addStretch(1);
        value->setParent(row);
        l->addWidget(value);
        card.body->addWidget(row);
    };
    addRow(tr("Processor"), m_cpu);
    addRow(tr("Graphics"), m_gpu);
    addRow(tr("System RAM"), m_ram);
    addRow(tr("Free Disk Space"), m_disk);
    addRow(tr("Recommended Edition"), m_recommended);
    return page(card.frame);
}

// ---------------------------------------------------------------------------------------------------------------------

QString BeyonderSettingsPage::recommendedEditionId() const
{
    const auto& m = m_window->m_manifest;
    if (!m || m->editions.isEmpty())
        return QStringLiteral("standard");
    const uint64_t ram = HardwareInfo::totalRamMiB();
    bool hasLite = false;
    for (const auto& e : m->editions)
        hasLite |= e.id == "lite";
    // 12 GB+ handles shaders and Distant Horizons comfortably; below that, prefer Lite if it exists.
    if (ram > 0 && ram < 12 * 1024 && hasLite)
        return QStringLiteral("lite");
    return m->editions.first().id;
}

void BeyonderSettingsPage::optimizeAutomatically()
{
    const QString id = recommendedEditionId();
    const auto& m = m_window->m_manifest;
    int memory = 4096;
    if (m) {
        if (const auto* e = m->edition(id); e && e->recommendedMemoryMB > 0)
            memory = e->recommendedMemoryMB;
        m_window->setEdition(id);
    }
    const uint64_t ram = HardwareInfo::totalRamMiB();
    if (ram > 0)
        memory = std::min<int>(memory, static_cast<int>(ram / 2));  // never more than half the PC's RAM
    memory = std::max(2048, memory / 512 * 512);
    m_window->setMaxMemoryMB(memory);
    refresh();
}

void BeyonderSettingsPage::refresh()
{
    if (!isVisible())
        return;
    const auto& m = m_window->m_manifest;
    auto* inst = m_window->findInstance();

    // Updates
    QString summary = tr("Launcher %1").arg(BuildConfig.printableVersionString());
    const bool launcherUpdate = m_window->launcherUpdateAvailable();
    summary += launcherUpdate ? tr(" • version %1 is available").arg(m->latestLauncherVersion) : tr(" is up to date");
    if (inst) {
        summary += tr(" • Beyonder %1 installed").arg(inst->getManagedPackVersionID());
    }
    if (m && inst && inst->getManagedPackVersionID() != m->packVersion) {
        summary += tr(" • %1 available").arg(m->packVersion);
    }
    m_updateSummary->setText(summary);
    m_launcherDownload->setVisible(launcherUpdate && m->launcherUrl.isValid());

    // Editions
    while (auto* item = m_editionLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    if (m && !m->editions.isEmpty()) {
        const QString current = m_window->currentEditionId();
        const QString installed = inst ? m_window->installedEditionId() : QString();
        const QString recommended = recommendedEditionId();
        for (const auto& e : m->editions) {
            QString tags;
            if (e.id == installed)
                tags += tr("Installed");
            if (e.id == recommended)
                tags += (tags.isEmpty() ? "" : " • ") + tr("Recommended for this PC");
            QString desc = e.description;
            if (e.recommendedMemoryMB > 0)
                desc += (desc.isEmpty() ? "" : " • ") + tr("%1 GB RAM").arg(e.recommendedMemoryMB / 1024.0, 0, 'f', e.recommendedMemoryMB % 1024 ? 1 : 0);
            if (!tags.isEmpty())
                desc += " — " + tags;
            auto* choose = e.id == current ? makeGhostButton(tr("Selected"), Icon::Check) : makeWhiteButton(tr("Choose"));
            choose->setEnabled(e.id != current);
            const QString id = e.id;
            connect(choose, &QPushButton::clicked, this, [this, id] {
                m_window->setEdition(id);
                refresh();
            });
            m_editionLayout->addWidget(makeRow(e.name, desc, choose));
        }
    } else {
        m_editionLayout->addWidget(makeLabel(tr("Editions appear once the update server is reachable."), "sRowDesc", m_editionBox));
    }

    // Memory
    const int mb = m_window->currentMaxMemoryMB();
    m_memorySlider->blockSignals(true);
    m_memorySlider->setValue(qBound(4, mb / 512, 32));
    m_memorySlider->blockSignals(false);
    m_memoryValue->setText(QString("%1 GB").arg(m_memorySlider->value() / 2.0, 0, 'f', m_memorySlider->value() % 2 ? 1 : 0));
    const auto* edition = m ? m->edition(m_window->currentEditionId()) : nullptr;
    const uint64_t ram = HardwareInfo::totalRamMiB();
    QString hint;
    if (edition && edition->recommendedMemoryMB > 0)
        hint = tr("Recommended for the %1 edition: %2 GB.").arg(edition->name).arg(edition->recommendedMemoryMB / 1024.0, 0, 'f', 1);
    if (ram > 0 && static_cast<uint64_t>(mb) > ram * 3 / 4)
        hint += (hint.isEmpty() ? "" : " ") + tr("That's more than ¾ of your PC's memory, which can make Windows slow while playing.");
    m_memoryHint->setText(hint);

    // Hardware (only when that page is open: GPU detection is not free)
    if (m_stack->currentIndex() == 5) {
        if (m_cpu->text().isEmpty()) {
            m_cpu->setText(tr("%1 (%n threads)", "", QThread::idealThreadCount()).arg(HardwareInfo::cpuInfo()));
            QStringList gpus = HardwareInfo::gpuInfo();
            gpus.removeIf([](const QString& g) { return g.contains("failed", Qt::CaseInsensitive) || g.trimmed().isEmpty(); });
            m_gpu->setText(gpus.isEmpty() ? tr("Unknown") : gpus.join(", "));
        }
        m_ram->setText(ram > 0 ? tr("%1 GB").arg(ram / 1024.0, 0, 'f', 1) : tr("Unknown"));
        const QString gameDir = inst ? inst->gameRoot() : QDir::currentPath();
        QStorageInfo storage(gameDir);
        m_disk->setText(storage.isValid() ? tr("%1 GB").arg(storage.bytesAvailable() / 1073741824.0, 0, 'f', 1) : tr("Unknown"));
        const QString rec = recommendedEditionId();
        QString recText = rec;
        int recMem = 0;
        if (m) {
            if (const auto* e = m->edition(rec)) {
                recText = e->name;
                recMem = e->recommendedMemoryMB;
            }
        }
        if (recMem > 0) {
            if (ram > 0)
                recMem = std::min<int>(recMem, static_cast<int>(ram / 2));
            recText += tr(" (%1 GB RAM)").arg(recMem / 1024.0, 0, 'f', recMem % 1024 ? 1 : 0);
        }
        m_recommended->setText(recText);
    }
}
