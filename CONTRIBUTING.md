# Contributing to OpenZone

Thanks for wanting to help. A few things to know before you open a pull request.

## Licence and rights

OpenZone is licensed under **CC BY-NC-SA 4.0** with an additional permission (see
`LICENSE` and `NOTICE`).

By submitting a contribution you agree that:

1. The contribution is your own work, or you have the right to submit it.
2. You assign copyright in the contribution to the project owner, or — where your
   jurisdiction does not permit assignment — you grant the owner an irrevocable,
   worldwide, royalty-free licence to use, modify, sublicense and relicense it,
   including under terms different from CC BY-NC-SA 4.0.

Point 2 exists so the project can be relicensed later without hunting down every
past contributor. Without it, a single unreachable contributor can freeze the
licence forever.

Contributions that carry code under a licence incompatible with CC BY-NC-SA 4.0
cannot be accepted. **GPL code in particular cannot go in.**

## Ground rules for the mod repositories

- **Do not invent DayZ API.** Every engine call must be checked against the
  unpacked game scripts. If you are not sure, unpack the PBO and look.
- **`OpenZone_VPP/gui/layouts/*.layout` files are generated, never hand-edited**
  -- with one named exception: `OpenZone_Core/gui/layouts/oz_link.layout`
  (hand-written on purpose, palette kept by hand). Every other layout comes
  from a description under `ui/OpenZone_VPP/`; regenerate with the MCP's
  `layout_build` or `python -m dayz_mcp.layoutgen <root>` from the generator
  repo. Run the gallery at two sizes and in two languages before calling a
  UI change done.
- **`ui/tokens.json` lives here and nowhere else.** `OpenZone_PDA` and
  `OpenZone_Factions` read it through their own `[build] tokens`; the
  `vpp` group exists only for this repo's own `OpenZone_VPP` window and is
  read straight, with no `[build] tokens` hop needed.
- **No text in code.** Every user-facing string goes through the stringtable.
  `original` is Ukrainian, `english` is English, the twelve remaining vanilla
  columns repeat the Ukrainian original (the engine needs the full column set);
  the capital Ukrainian I is stored as Latin I because no Metron font of the
  game draws U+0406. The one exception is the VPP admin console: it is English
  in place and stays that way.
- **No hard dependency beyond Community Framework.** Anything else is an optional
  provider behind an `#ifdef` plus a runtime probe, with a working fallback. This
  binds a mod's own pbo, not the family: a hard dependency is fine in a separate,
  optional glue pbo that depends on both sides it glues, and nothing else needs.
  `OpenZone_VPP` is Core's own example — a hard dependency on VPP Admin Tools,
  confined to a pbo a server can skip entirely.
- **Never identify an item by inheritance from our own class.** Item classnames come
  from JSON so that admins can point the mod at items from any mod.
- Every `.ps1` file must be saved as **UTF-8 with BOM**, or Windows PowerShell 5.1
  reads it in the system codepage and the parser dies on non-ASCII.

## Before you open a pull request

- The mod compiles: server boot and client compile check both clean.
- No new warnings in the server log.
- If you added a config field, it has a default, a migration, and a line in
  `Validate()`.
