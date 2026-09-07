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
| **Spawn loadouts** | A service another mod fills in (`OZ_Loadout`, three-valued: no opinion / naked / preset) plus an applicator that strips what the mission gave a new character and dresses it from a preset, in the same frame after `OnClientNewEvent`. [The factions mod](https://github.com/covalschi/openzone-factions) supplies the ladder (`OZ_Factions_Loadouts.json`) |
| **Design tokens** | `ui/tokens.json`: the series' single source of colour, font, spacing, device-geometry and VPP admin-window (`vpp`) tokens; the PDA and the factions read this same file through their own `[build] tokens` |

## Writing a config on Core

A config is a class extending `OZ_ConfigBase`, read through
`OZ_ConfigLoader<T>.Load(path, tag, cfg)`. A broken file never stops the server:
it is quarantined, the defaults load, the log says so. Three rules for the class
itself, and the third is the one that bites.

**1. Construct it yourself.** `Load` refuses a null `cfg`. The serializer would
happily allocate the root — and then nothing in the object would have run its
field initialisers.

**2. `LoadDefaults()` assigns every field.** It runs on a missing file, on a
failed parse and on a failed migration, on top of whatever was half-read.

**3. `Validate()` reseats every nested object.** `JsonFileLoader` is a wrapper
over the native `JsonSerializer.ReadFromString`, declared
`proto bool ReadFromString(void variable_in, ...)`: it fills the ROOT object you
passed in, but it allocates every nested `ref` and every array element itself —
even for a section the file does not contain, which comes back allocated and
zeroed rather than null. Those allocations never run the script constructor, so
`string Kind = ""` and `int MaxMembers = 50` did not happen. A member the file
did not carry is raw memory: it reads as zero immediately after the parse and as
somebody else's bytes minutes later, which is how a rank once printed as
`mercenary:$`. The usual way in is not a corrupt file but a **new field** —
live files do not carry it, and `LoadDefaults()` does not run on a successful
parse.

The idiom is a `Copy()` on every nested class (a `new` of its own type, member by
member, nested ones through their own `Copy()`), called from `Validate()` — which
the loader runs immediately after the parse, while the read is still honest:

```c
if (!Bridge)
    Bridge = new OZ_BridgeSettings();
else
    Bridge = Bridge.Copy();
```

After the copy a member absent from the file holds zero, not the value in its
initialiser, so `Validate()` supplies the real defaults it cares about
(`Bridge.ServerId`, `Staging.Radius`) exactly as it always did. Core's own six
copies — `OZ_BridgeSettings`, `OZ_KindMirror`, `OZ_SpawnPlace`, `OZ_SpawnZone`,
`OZ_SpawnPersonal`, `OZ_FriendReq` — are the worked example.

**Where zero is a real value, the key becomes required.** Nothing below the
loader can tell "the file had no `Health01`" from "the file said `0`", and for a
loadout item those mean opposite things — untouched versus ruined on spawn. So
`OZ_LoadoutItem` (the type any mod's loadout presets are written in) refuses the
ambiguous zero out loud: a zero `Health01` or `QuickBar` is reported as a missing
key, naming the preset and the class, and the declared `-1` ("do not touch", "no
quick slot") is used instead. The price is that a hand-written file can no longer
ask for a ruined item with a bare `0` — write `0.001` — nor for quick slot `0`;
slots are `1..9`. A file the mod itself writes carries all six keys, so this only
ever fires on one trimmed by hand.

The same holds outside `OZ_ConfigLoader` for any `JsonFileLoader.LoadData` whose
result outlives the call: a bridge envelope kept in a cache, a config a screen
paints from between refreshes. Copy it in the sink. A value read in the same call
as the parse needs no copy.

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
