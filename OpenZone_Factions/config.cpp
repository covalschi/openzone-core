// Фракційна система OpenZone -- ОКРЕМИЙ МОД, а не частина ядра.
//
// Рішення власника 2026-08-31: у ядрі лишаються служби, а не гра. Фракції
// разом із ролями та їхніми операціями важили близько чверті ядра (865 + 706
// + 794 рядки), і сервер, якому потрібна сама лише рація, платив за них
// блокуючою залежністю та синхронізацією з Discord, якої не просив.
//
// Тепер правило просте: БУДЬ-ЯКИЙ мод серії запускається, маючи саме ядро.
// Фракції -- один із таких модів, не привілейований.
//
// Ядро про цей мод не знає нічого. Зв'язок односторонній: мод підставляє
// свою реалізацію в OZ_Identity, і всі, кому треба знати, чия людина перед
// ними, питають ядро. Без цього мода відповідь порожня -- фракції немає,
// ставлення нейтральне, лідерів нема, -- і жодна сторінка від цього не
// ламається.

class CfgPatches
{
    class OpenZone_Factions
    {
        units[] = {};
        weapons[] = {};
        requiredVersion = 0.1;
        requiredAddons[] =
        {
            "DZ_Data",
            "DZ_Scripts",
            "OpenZone_Core"
        };
    };
};

class CfgMods
{
    class OpenZone_Factions
    {
        dir = "OpenZone_Factions";
        name = "OpenZone Factions";
        author = "Zone Protocol";
        version = "0.1.0";
        type = "mod";

        dependencies[] = {"Game", "World", "Mission"};

        class defs
        {
            class gameScriptModule    { value = ""; files[] = {"OpenZone_Factions/scripts/3_Game"}; };
            class worldScriptModule   { value = ""; files[] = {"OpenZone_Factions/scripts/4_World"}; };
        };
    };
};
