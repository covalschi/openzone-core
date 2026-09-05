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
| **Player store** | Per-SteamID JSON that survives character death |
| **Bridge client** | Long-poll client for the OpenZone Discord bridge |
| **Affiliation contract** | `OZ_Identity`: which organisation, which stand towards another player, who leads. Declared here, answered with "none" here, filled in by the factions mod; read-only by design. The only faction-shaped thing in Core |
| **Spawn loadouts** | A service another mod fills in (`OZ_Loadout`, three-valued: no opinion / naked / preset) plus an applicator that strips what the mission gave a new character and dresses it from a preset, in the same frame after `OnClientNewEvent`; one-shot spawn points may carry a loadout word. [The factions mod](https://github.com/covalschi/openzone-factions) supplies the ladder (`OZ_Factions_Loadouts.json`) |

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

Optional, detected at runtime: VPP Admin Tools, DayZ Expansion.

## Mods built on it

- **[OpenZone PDA](https://github.com/covalschi/openzone-pda)** — S.T.A.L.K.E.R.-style PDA: map, factions, friends, shared markers, Discord-backed chat, configurable radio
- **[OpenZone Factions](https://github.com/covalschi/openzone-factions)** — factions, roles, ranks, roster, permadeath; supplies `OZ_Identity`

## Licence

CC BY-NC-SA 4.0 with an additional permission for server operators — see `LICENSE`
and `NOTICE`. Short version: use it, fork it, run it on a donation-funded server
freely; do not sell it.
