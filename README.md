# OpenZone Core

The shared foundation for the **OpenZone** family of DayZ mods.

Core ships no gameplay of its own. It provides the plumbing every OpenZone mod needs,
so that each mod is content and rules rather than another copy of the same
infrastructure.

## What it provides

| | |
|---|---|
| **Config service** | Versioned JSON in `$profile:OpenZone\`, stepwise migrations, `Validate()` that warns instead of crashing, automatic backups, hot reload without a restart |
| **Two config surfaces** | Server-only settings (secrets) and client-synced settings, with a hard rule that the first never crosses the wire |
| **Permissions** | VPP Admin Tools when present, a SteamID list when not — decided at runtime, never at compile time |
| **Transport** | One generic request/response envelope over Community Framework's string-keyed RPC, so the contract does not grow with every new screen |
| **Page registry** | Screens register themselves once; any mod can add one |
| **Player store** | Per-SteamID JSON that survives character death. `Load()` keeps a uid resident and CREATES its file if there is none, so a lookup about somebody offline belongs in `Peek()` — it reads through a short-lived cache, marks nothing dirty and writes nothing |
| **Bridge client** | Long-poll client for the OpenZone Discord bridge |
| **Affiliation contract** | `OZ_Identity`: which organisation, which stand towards another player, who leads. Declared here, answered with "none" here, filled in by the factions mod; read-only by design. The only faction-shaped thing in Core |
| **Spawn loadouts** | A service another mod fills in (`OZ_Loadout`, three-valued: no opinion / naked / preset) plus an applicator that strips what the mission gave a new character and dresses it from a preset, in the same frame after `OnClientNewEvent`; one-shot spawn points may carry a loadout word. [The factions mod](https://github.com/covalschi/openzone-factions) supplies the ladder (`OZ_Factions_Loadouts.json`) |
| **Design tokens** | `ui/tokens.json`: the series' single source of colour, font, spacing, device-geometry and VPP admin-window (`vpp`) tokens; the PDA and the factions read this same file through their own `[build] tokens` |

## The Discord link gate

`RequireDiscordLink` (in `$profile:OpenZone\OZ_Core_Settings.json`, **on** by default)
is enforced by the server, not only by a client window. Roles in the guild decide
faction, standing and posts, so a player the bot has never heard of does not exist for
any of it. What the server actually does:

| | |
|---|---|
| **On connect** | An unlinked player is asked about once at the bridge (he may have typed `/link` while away), the gate window opens on his screen, and a five-minute clock starts (`OZ_Link.GATE_GRACE_MS`, a constant, not a setting) |
| **Within the grace** | He presses "get a code", types `/link CODE` in Discord, and the moment the bridge confirms it the window closes and the clock is dropped. One code per player per ten seconds |
| **After the grace** | He is told the reason (`STR_OZ_KICK_NO_LINK`) into the gate window, and disconnected on the next tick — the engine's own disconnect screen carries no text of ours. With the window broken or gone the reason goes to a system notification instead |
| **Admins never** | Anyone `OZ_Perm` calls an admin — `AdminIds` in the same file, or the VPP permission — passes the gate unlinked. Admin rights do not come from guild roles, and the one person who must get in when everything else is broken is him |
| **No mirrors, no gate** | With every `Bridge.Mirrors` entry off, linking would change nothing a player can see, so the gate does not hold at all |
| **Bridge not answering** | **Nobody is kicked.** The code comes from the bridge, so a bot outage would otherwise empty the server. This holds whatever `AllowPlayWhenBridgeDown` says: that setting decides whether an unlinked player may *play* while the bot is down (`true`, the default, closes the window and lets him in; `false` leaves the window in his face), never whether the server throws him out for an answer nobody could give. The clock restarts when the bridge answers again |

"Not answering" means no HTTP answer for sixty seconds (`OZ_BridgeClient.DEAD_MS`) — a
dead process and a hung one that accepts the connection and never replies count the
same.

Settings involved: `RequireDiscordLink`, `AllowPlayWhenBridgeDown`, `AdminIds`,
`VppPermission`, `Bridge.Enabled`, `Bridge.Url`, `Bridge.Mirrors`.

## OpenZone_VPP

A second, optional pbo shipped from this same repository. It adds an "OpenZone" tab to
VPP Admin Tools with three panes — SPAWNS, RAW JSON, NEWS — and nothing else. Unlike
Core proper, it carries a hard dependency on VPP Admin Tools (`DZM_VPPAdminToolsScripts`);
a server that leaves `@OpenZone_VPP` out of its mod list keeps every other service Core
provides. The FACTIONS pane in that tab is not registered here — it comes from
`OpenZone_Factions_VPP`, in the [OpenZone Factions](https://github.com/covalschi/openzone-factions)
repository.

## Requirements

- [Community Framework](https://steamcommunity.com/sharedfiles/filedetails/?id=1559212036) (`JM_CF_Scripts`)

Optional, detected at runtime: VPP Admin Tools.

## Mods built on it

- **[OpenZone PDA](https://github.com/covalschi/openzone-pda)** — S.T.A.L.K.E.R.-style PDA: map, factions, friends, shared markers, Discord-backed chat, configurable radio
- **[OpenZone Factions](https://github.com/covalschi/openzone-factions)** — factions, roles, ranks, roster, permadeath; supplies `OZ_Identity`

## Licence

CC BY-NC-SA 4.0 with an additional permission for server operators — see `LICENSE`
and `NOTICE`. Short version: use it, fork it, run it on a donation-funded server
freely; do not sell it.
