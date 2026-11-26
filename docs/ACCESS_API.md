# Beyonder access API

The launcher can show each player's access to the Beyonder server before they press Play: whitelist, Discord link, Lost Control and bans. It is optional: without `accessUrl` in the pack manifest, Play always tries to join and the game server decides.

## Manifest

```json
"accessUrl": "https://api.example.net/v1/access/{uuid}"
```

`{uuid}` is replaced with the player's Minecraft UUID (32 hex characters, no dashes) and `{name}` with their Minecraft name (URL-encoded). The launcher calls it with `GET` when it starts, when the player changes account, when they press Refresh, and every 90 seconds.

## Response

`200 OK` with a JSON object:

```json
{
  "status": "allowed",
  "message": "Shown to the player when Play is blocked.",
  "linkUrl": "https://api.example.net/link?state=...",
  "discordName": "klein.moretti"
}
```

| `status` | Launcher shows | Play button |
|---|---|---|
| `allowed` | Access Verified | PLAY |
| `discord_required` | Discord not linked | LINK DISCORD: opens `linkUrl` |
| `not_whitelisted` | Not whitelisted | NO ACCESS: explains `message`, offers the Discord invite from `links.discord` |
| `lost_control` | Lost Control | LOST CONTROL: explains `message` (e.g. when the next revival is) |
| `banned` | Banned | BANNED: explains `message` |

All fields except `status` are optional. `linkUrl` must be `https://`. When it's present, the account menu also gets a "Link Discord account" entry.

Any other status, an error, or a timeout counts as "unavailable": the launcher doesn't block Play, and the server stays in charge. The launcher check is a convenience for players. It is not security: the Minecraft server (a whitelist plugin or mod) must enforce the same rules.

## Building the service

A small service is enough:

1. A Discord bot that knows which members have the whitelist roles.
2. A Discord OAuth2 link flow that ties a Discord account to a Minecraft UUID. Confirm ownership in game, for example with a `/link <code>` command on the server. Don't ask players to hand over Minecraft tokens.
3. A table of Minecraft UUID → Discord ID, whitelist state, Lost Control state and revival time, and bans, which the server mod writes to when a character loses control.
4. The `GET /v1/access/{uuid}` endpoint above, reading that table.
