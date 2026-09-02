// Панель «FACTIONS» адмiнського вiкна OpenZone -- ОКРЕМИЙ pbo.
//
// requiredAddons нижче -- ЖОРСТКА залежнiсть, тобто блокуюче вiкно ще до
// завантаження, а не тихий пропуск. Тут це саме те, що треба: панель без
// фракцiй нiчого не малює, а без ядрового вiкна їй нема до чого чiплятись.
//
// Сервер без цього pbo бачить у вкладцi OpenZone тiльки те, що належить
// ядру, -- спавни й редактор конфiгiв. Це не збiднена версiя, а чесна: усе
// решта й справдi не встановлене.
//
// DZM_VPPAdminToolsScripts -- клас CfgPatches скриптового pbo самого VPP.
// #ifdef у кодi гардить на AVPPAdminTools: рушiй авто-дефайнить iмена класiв
// CfgMods, а не CfgPatches (змiряно 2026-07-31 на 1.29 diag).

class CfgPatches
{
    class OpenZone_Factions_VPP
    {
        units[] = {};
        weapons[] = {};
        requiredVersion = 0.1;
        requiredAddons[] =
        {
            "DZ_Data",
            "DZ_Scripts",
            "OpenZone_Core",
            "OpenZone_Factions",
            "OpenZone_VPP",
            "DZM_VPPAdminToolsScripts"
        };
    };
};

class CfgMods
{
    class OpenZone_Factions_VPP
    {
        dir = "OpenZone_Factions_VPP";
        name = "OpenZone Factions VPP Pane";
        author = "Zone Protocol";
        version = "0.1.0";
        type = "mod";

        dependencies[] = {"Mission"};

        class defs
        {
            class missionScriptModule { value = ""; files[] = {"OpenZone_Factions_VPP/scripts/5_Mission"}; };
        };
    };
};
