// OpenZone Core -- the shared foundation for the OpenZone family of DayZ mods.
//
// Ships no gameplay. Provides the config service, permissions, the RPC envelope,
// the page registry and the per-player store that every OpenZone mod builds on.
//
// The CfgPatches class name below is this addon's identity: it is the exact string
// other mods put in their own requiredAddons[]. That dependency is HARD -- a mod
// declaring it and loading without this one gets a blocking "Addon X requires addon
// Y" dialog before the game starts, not a silently skipped pbo.

class CfgPatches
{
    class OpenZone_Core
    {
        units[] = {};
        weapons[] = {};
        requiredVersion = 0.1;
        requiredAddons[] =
        {
            "DZ_Data",
            "DZ_Scripts",
            "JM_CF_Scripts"
        };
    };
};

class CfgMods
{
    class OpenZone_Core
    {
        dir = "OpenZone_Core";
        name = "OpenZone Core";
        author = "Zone Protocol";
        version = "0.1.0";
        type = "mod";

        // Must be > 0 or CF_ModStorage silently does nothing. Every future bump
        // appends to the END of the stream and is read back behind a
        // ctx.GetVersion() gate -- CF writes are positional, so inserting a field
        // anywhere else shifts and eats every later mod's data.
        storageVersion = 1;

        dependencies[] = {"Game", "World", "Mission"};
        defines[] = {"OPENZONE_CORE"};

        class defs
        {
            // files[] entries are DIRECTORIES: every .c beneath them compiles into
            // that VM, recursively.
            class gameScriptModule    { value = ""; files[] = {"OpenZone_Core/scripts/3_Game"}; };
            class worldScriptModule   { value = ""; files[] = {"OpenZone_Core/scripts/4_World"}; };
            class missionScriptModule { value = ""; files[] = {"OpenZone_Core/scripts/5_Mission"}; };
        };
    };
};
