// Вкладка «Фракція» в КПК -- ОКРЕМИЙ pbo, як і вкладка рації.
//
// Жорстко вимагає обидва боки: без фракційної системи малювати нічого, без
// КПК немає де. Саме тому це склейка, а не частина котрогось із них: ані КПК,
// ані мод фракцій не мусять знати одне про одного, і кожен ставиться окремо.
//
// Сервер без цього pbo просто не має вкладки. Контакти, чат і карта при цьому
// працюють: вони питають ядро (OZ_Identity), а воно без мода фракцій чесно
// відповідає «фракції немає» -- і поділ на своїх і чужих зникає разом із нею,
// не ламаючи жодного екрана.

class CfgPatches
{
    class OpenZone_Factions_PDA
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
            "OpenZone_PDA"
        };
    };
};

class CfgMods
{
    class OpenZone_Factions_PDA
    {
        dir = "OpenZone_Factions_PDA";
        name = "OpenZone Factions PDA Page";
        author = "Zone Protocol";
        version = "0.1.0";
        type = "mod";

        dependencies[] = {"Game", "World", "Mission"};

        class defs
        {
            class gameScriptModule    { value = ""; files[] = {"OpenZone_Factions_PDA/scripts/3_Game"}; };
            class worldScriptModule   { value = ""; files[] = {"OpenZone_Factions_PDA/scripts/4_World"}; };
            class missionScriptModule { value = ""; files[] = {"OpenZone_Factions_PDA/scripts/5_Mission"}; };
        };
    };
};
