// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Beyonder Launcher - a fork of Prism Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <optional>

/**
 * The pack manifest the launcher downloads on start-up (BuildConfig.BEYONDER_MANIFEST_URL).
 *
 * Example:
 * {
 *   "formatVersion": 1,
 *   "packVersion": "0.1.0",
 *   "mrpackUrl": "https://github.com/you/BeyonderPack/releases/download/v0.1.0/Beyonder-0.1.0.mrpack",
 *   "server": "play.example.net:25565",
 *   "recommendedMemoryMB": 6144,
 *   "motd": "<h2>Season 1: Tingen</h2><p>The Nighthawks are recruiting.</p>",
 *   "minLauncherVersion": "12.0.0"
 * }
 */
struct BeyonderManifest {
    static constexpr int SupportedFormatVersion = 1;

    int formatVersion = 0;
    QString packVersion;
    QUrl mrpackUrl;
    QString server;           // "host" or "host:port"; empty = don't auto-join
    int recommendedMemoryMB = 0;  // 0 = leave the user's memory settings alone
    QString motd;             // small HTML/markdown snippet shown on the home screen
    QString minLauncherVersion;

    // Returns the manifest, or nothing and an error message in errorOut.
    static std::optional<BeyonderManifest> parse(const QByteArray& data, QString& errorOut);
};
