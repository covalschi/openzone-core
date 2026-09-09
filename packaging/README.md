# packaging

What goes into a `@Mod` folder besides the packed pbo.

The `@Mod` folders are **build output and are not in git**. `mod_build` writes
`addons/` into them and signs it with the series key (`keys/ZoneProtocol.biprivatekey`,
never committed); everything else a published mod needs lives here and is copied in by
[`package.ps1`](../package.ps1).

```
packaging/<Mod>/mod.cpp     the name, author, version and description DayZ shows in
                            the launcher and the mod list
packaging/<Mod>/meta.cpp    the Workshop item id, for mods that have been published
```

`meta.cpp` ties a local folder to a Workshop item; without it the next upload creates a
**second** item instead of updating the one that exists. Publisher writes it into the
`@Mod` folder on the first publish -- **copy it back here and commit it**, otherwise the
next `package.ps1` cannot restore it after a deleted build folder. `timestamp` is left
to Publisher: a number invented to fill it would be false.

Publishing, in order: `mod_build` (packs and signs) -> `.\package.ps1` (puts the rest in
place; `-Check` only reports) -> Publisher, pointed at the `@Mod` folder itself. The
procedure and its reasons are written out in `openzone-radio/docs/publishing.md`.
