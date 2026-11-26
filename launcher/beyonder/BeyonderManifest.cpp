// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Beyonder Launcher - a fork of Prism Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "BeyonderManifest.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

std::optional<BeyonderManifest> BeyonderManifest::parse(const QByteArray& data, QString& errorOut)
{
    QJsonParseError parseError{};
    const auto doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        errorOut = QCoreApplication::translate("BeyonderManifest", "The pack manifest is not valid JSON: %1").arg(parseError.errorString());
        return std::nullopt;
    }
    const auto obj = doc.object();

    BeyonderManifest m;
    m.formatVersion = obj.value("formatVersion").toInt(0);
    if (m.formatVersion < 1 || m.formatVersion > SupportedFormatVersion) {
        errorOut = QCoreApplication::translate("BeyonderManifest",
                                               "The pack manifest uses format version %1, which this launcher doesn't understand. "
                                               "Please update the launcher.")
                       .arg(m.formatVersion);
        return std::nullopt;
    }

    m.packVersion = obj.value("packVersion").toString().trimmed();
    m.mrpackUrl = QUrl(obj.value("mrpackUrl").toString().trimmed());
    m.server = obj.value("server").toString().trimmed();
    m.recommendedMemoryMB = obj.value("recommendedMemoryMB").toInt(0);
    m.motd = obj.value("motd").toString();
    m.minLauncherVersion = obj.value("minLauncherVersion").toString().trimmed();
    m.season = obj.value("season").toString().trimmed();
    m.serverName = obj.value("serverName").toString().trimmed();

    const auto links = obj.value("links").toObject();
    auto httpsOrEmpty = [](const QString& s) {
        QUrl u(s.trimmed());
        return (u.isValid() && (u.scheme() == "https" || u.scheme() == "http")) ? u : QUrl();
    };
    m.discordUrl = httpsOrEmpty(links.value("discord").toString());
    m.websiteUrl = httpsOrEmpty(links.value("website").toString());

    for (const auto& v : obj.value("news").toArray()) {
        const auto n = v.toObject();
        NewsItem item{ n.value("title").toString(), n.value("date").toString(), n.value("body").toString(),
                       httpsOrEmpty(n.value("url").toString()) };
        if (!item.title.isEmpty()) {
            m.news.append(item);
        }
    }

    if (m.packVersion.isEmpty()) {
        errorOut = QCoreApplication::translate("BeyonderManifest", "The pack manifest has no \"packVersion\".");
        return std::nullopt;
    }
    auto isHttps = [](const QUrl& u) { return u.isValid() && u.scheme() == "https"; };

    for (const auto& v : obj.value("editions").toArray()) {
        const auto e = v.toObject();
        Edition ed;
        ed.id = e.value("id").toString().trimmed().toLower();
        ed.name = e.value("name").toString().trimmed();
        ed.description = e.value("description").toString().trimmed();
        ed.mrpackUrl = QUrl(e.value("mrpackUrl").toString().trimmed());
        ed.recommendedMemoryMB = e.value("recommendedMemoryMB").toInt(m.recommendedMemoryMB);
        if (ed.id.isEmpty() || !isHttps(ed.mrpackUrl)) {
            errorOut = QCoreApplication::translate("BeyonderManifest", "An edition in the pack manifest needs an \"id\" and an https:// \"mrpackUrl\".");
            return std::nullopt;
        }
        if (ed.name.isEmpty())
            ed.name = ed.id.left(1).toUpper() + ed.id.mid(1);
        m.editions.append(ed);
    }
    if (m.editions.isEmpty()) {
        if (!isHttps(m.mrpackUrl)) {
            errorOut = QCoreApplication::translate("BeyonderManifest", "The pack manifest's \"mrpackUrl\" must be an https:// link.");
            return std::nullopt;
        }
        m.editions.append(Edition{ "standard", QCoreApplication::translate("BeyonderManifest", "Standard"), QString(), m.mrpackUrl,
                                   m.recommendedMemoryMB });
    }

    m.accessUrl = obj.value("accessUrl").toString().trimmed();
    if (!m.accessUrl.isEmpty() && !m.accessUrl.startsWith("https://") && !m.accessUrl.startsWith("file://")) {
        errorOut = QCoreApplication::translate("BeyonderManifest", "The pack manifest's \"accessUrl\" must be an https:// link.");
        return std::nullopt;
    }
    return m;
}

const BeyonderManifest::Edition* BeyonderManifest::edition(const QString& id) const
{
    for (const auto& e : editions) {
        if (e.id == id)
            return &e;
    }
    return editions.isEmpty() ? nullptr : &editions.first();
}
