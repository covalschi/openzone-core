// Спорядження на появі (ТЗ-3). Ядро оголошує СЛУЖБУ з порожньою реалізацією
// й АПЛІКАТОР; що саме надягти й кому -- вирішує чужий мод (фракцій), який
// підставляє свою реалізацію через OZ_Loadout.Provide, симетрично OZ_Identity.
// Ядро не знає слова «фракція».
//
// ЧОМУ «ЗНЯТИ ВСЕ Й НАДЯГТИ СВОЄ», а не «видати поверх». Файл місії
// перекриває StartingEquipSetup (init.c:229) і CreateCharacter без super, а
// компілюється останнім, тому modded MissionServer із перекритим
// StartingEquipSetup не доходить туди ніколи (ТЗ-3 F1-F3). Єдиний надійний
// шов -- OnClientNewEvent, і на момент повернення super місія ВЖЕ одягла
// персонажа обома своїми шляхами (missionserver.c:566 і :580). Отже пресет
// -- це роздягнути й одягти заново.
//
// ВІДПОВІДЬ СЛУЖБИ ТРИЗНАЧНА (R1.2): NAKED і NO_OPINION -- різні речі.
// Перше -- «роздягнути», друге -- «не втручатись»: місія одягла, лишаємо.

enum OZ_LoadoutVerdict
{
    NO_OPINION,
    NAKED,
    PRESET
}

// Один предмет пресета. SlotName -- ім'я слота з CfgSlots ("Body", "Legs",
// "Back", "Shoulder"...), порожньо -- «куди влізе»; спеціальне слово "hands"
// -- у руки. Inside -- вміст: що покласти В цей предмет.
class OZ_LoadoutItem
{
    string ClassName = "";
    string SlotName  = "";
    int    Quantity  = 0;    // 0 = як у класу; для магазина -- набоїв
    float  Health01  = -1;   // < 0 = не чіпати
    int    QuickBar  = -1;   // < 0 = не призначати
    ref array<ref OZ_LoadoutItem> Inside;

    void OZ_LoadoutItem()
    {
        Inside = new array<ref OZ_LoadoutItem>();
    }

    // Копія в об'єкт, який зробив скрипт (шапка OZ_ConfigBase). Пресети живуть
    // у конфігу мода фракцій увесь запуск сервера, а одягає з них ядро на
    // кожній появі -- тобто через години після розбору файла. Рекурсія по
    // Inside: вкладення теж виділив серіалізатор.
    //
    // ВІДСУТНІЙ КЛЮЧ КАЖЕ ПРО СЕБЕ ВГОЛОС (ТЗ-3 R4.2).
    //
    // Ключа, якого У ФАЙЛІ НЕМАЄ, копія переносить НУЛЕМ, а не значенням з
    // ініціалізатора. Для Health01 і QuickBar нуль означає не те саме, що
    // -1: «зіпсована річ» і «перший слот» замість «не чіпати». Тобто
    // обрізаний рукою запис видавав гравцеві вбиту куртку -- мовчки, і
    // причини в лозі не було жодної.
    //
    // Відрізнити «ключа немає» від «у ключі нуль» не може ніхто, крім
    // лоадера, тож ці два ключі стають ОБОВ'ЯЗКОВИМИ: нуль у них
    // відхиляється як «ключа немає», предмет отримує оголошене -1, і про
    // кожен такий запис у лозі стоїть рядок. Ціна названа в README: попросити
    // з файла вбиту річ або НУЛЬОВИЙ слот панелі більше не можна -- пишеться
    // 0.001 і слоти 1..9. Файл, який пише сам мод, несе всі шість ключів, тож
    // це стосується лише обрізаного вручну.
    OZ_LoadoutItem Copy(string where = "")
    {
        OZ_LoadoutItem c = new OZ_LoadoutItem();
        c.ClassName = ClassName;
        c.SlotName  = SlotName;
        c.Quantity  = Quantity;
        c.Health01  = Health01;
        c.QuickBar  = QuickBar;

        if (c.Health01 == 0)
        {
            OZ_Log.Warn(Missing(where, "Health01") + " - the item is left as the class makes it; write -1 for that, or 0.001 for a ruined one");
            c.Health01 = -1;
        }

        if (c.QuickBar == 0)
        {
            OZ_Log.Warn(Missing(where, "QuickBar") + " - no quick slot is assigned; write -1 for that, or 1..9 for a slot");
            c.QuickBar = -1;
        }

        if (Inside)
        {
            for (int i = 0; i < Inside.Count(); i++)
            {
                if (Inside[i])
                    c.Inside.Insert(Inside[i].Copy(where));
            }
        }

        return c;
    }

    private string Missing(string where, string field)
    {
        string m = "loadout";
        if (where != "")
            m += " " + where;
        m += ": " + ClassName + " has no " + field;
        return m;
    }
}

class OZ_LoadoutPreset
{
    string Id = "";
    ref array<ref OZ_LoadoutItem> Items;

    void OZ_LoadoutPreset()
    {
        Items = new array<ref OZ_LoadoutItem>();
    }

    // Тип оголошує ядро -- отже й Copy() до нього пише ядро, хоч користується
    // ним конфіг мода фракцій (OZF_LoadoutsConfig.Validate).
    OZ_LoadoutPreset Copy()
    {
        OZ_LoadoutPreset c = new OZ_LoadoutPreset();
        c.Id = Id;

        if (Items)
        {
            for (int i = 0; i < Items.Count(); i++)
            {
                // Id -- щоб рядок про відсутній ключ називав ПРЕСЕТ, а не
                // саму лише назву класу: у файлі їх десятки, і «немає
                // Health01 у Hoodie_Blue» без пресета не знаходиться.
                if (Items[i])
                    c.Items.Insert(Items[i].Copy(c.Id));
            }
        }

        return c;
    }
}

// МОДИФІКАТОРА ПРЕСЕТА (OZ_LoadoutMod) ТУТ БІЛЬШЕ НЕМАЄ (2026-09-06).
//
// Ключем у ньому був слаг ПОСАДИ або МІТКИ, тобто словник мода фракцій, а
// ядро слова «фракція» не знає -- шапка цього файла обіцяє рівно це. Клас
// переїхав до єдиного свого споживача як OZF_LoadoutMod
// (openzone-factions/OZF_Loadouts.c); ядро й далі возить готовий
// OZ_LoadoutPreset і в нього не заглядає.

// Порожня реалізація: думки не має. Мод, який знає, підставляє свою.
//
// ПРЕСЕТ, ЩО ПОВЕРТАЄТЬСЯ, МУСИТЬ КОМУСЬ НАЛЕЖАТИ (R1.3): поле власного
// конфігу або службового об'єкта, не свіжий локальний. Виміряно 2026-08-02:
// контейнер, повернутий із функції через не-ref out, знищується, і той, хто
// питав, читає звільнену пам'ять.
class OZ_LoadoutService
{
    OZ_LoadoutVerdict ForPlayer(string uid, out OZ_LoadoutPreset preset)
    {
        preset = null;
        return OZ_LoadoutVerdict.NO_OPINION;
    }

    // Пресет за id -- для одноразових точок чужих модів (R5.3): ядро возить
    // рядок і в нього не заглядає; розгортає його той, хто пресети тримає.
    bool Preset(string id, out OZ_LoadoutPreset preset)
    {
        preset = null;
        return false;
    }
}

class OZ_Loadout
{
    // Слово одноразової точки чужого мода «надягти нічого» (ТЗ-3 R5.1).
    // Рядок, а не окреме поле: ядро возить слово й у нього не заглядає.
    //
    // Present() тут немає: він не мав викликачів у жодному репозиторії серії
    // -- за «чи є мод» ходять до OZ_Identity.Present(), а тут відповідь дає
    // сама порожня реалізація.
    static const string NONE = "-";

    private static ref OZ_LoadoutService s_Svc;

    static void Provide(OZ_LoadoutService svc)
    {
        if (!svc)
            return;

        s_Svc = svc;
        OZ_Log.Info("loadout service provided by another mod");
    }

    static OZ_LoadoutService Get()
    {
        if (!s_Svc)
            s_Svc = new OZ_LoadoutService();
        return s_Svc;
    }

    // ШОВ. Кличе OZ_MissionServer одразу після super.OnClientNewEvent --
    // персонаж уже створений і одягнений місією. Без думки служби місія
    // лишається як є.
    //
    // ПОРЯДОК -- ДЗЕРКАЛО OZ_Spawns.Resolve (ТЗ-3 R2.1, ТЗ-5 R-A3.1): одна
    // подія не має права ходити двома різними драбинами. Одноразова точка
    // вирішує першою, далі служба, а без думки служби місія лишається як є.
    //
    // Рішення одноразової точки ВЖЕ з'їдене тим самим Resolve, що з'їв
    // позицію (R2.5); тут ми лише забираємо те, що він відклав.
    static void OnSpawn(PlayerBase player, PlayerIdentity identity)
    {
        if (!player || !identity)
            return;
        if (!GetGame().IsServer())
            return;

        string uid = identity.GetPlainId();

        string once;
        if (OZ_Spawns.TakeOnceLoadout(uid, once))
        {
            if (once == NONE)
            {
                OZ_LoadoutApply.To(player, null);
                OZ_Log.Info("loadout: " + uid + " -> naked (one-shot)");
                return;
            }

            if (once != "")
            {
                OZ_LoadoutPreset named;
                if (Get().Preset(once, named) && named)
                {
                    OZ_LoadoutApply.To(player, named);
                    OZ_Log.Info("loadout: " + uid + " -> " + named.Id + " (one-shot)");
                    return;
                }

                // Невідомий id -- не привід лишити людину голою через чужу
                // одруківку (дух R6.3): кажемо в лог і йдемо драбиною.
                OZ_Log.Warn("loadout: one-shot preset \"" + once + "\" is unknown, falling back to the service");
            }
        }

        OZ_LoadoutPreset preset;
        OZ_LoadoutVerdict verdict = Get().ForPlayer(uid, preset);

        if (verdict == OZ_LoadoutVerdict.PRESET && preset)
        {
            OZ_LoadoutApply.To(player, preset);
            OZ_Log.Info("loadout: " + uid + " -> " + preset.Id);
            return;
        }

        if (verdict == OZ_LoadoutVerdict.NAKED)
        {
            OZ_LoadoutApply.To(player, null);
            OZ_Log.Info("loadout: " + uid + " -> naked");
            return;
        }

        // NO_OPINION: місія одягла, лишаємо. Без рядка в лог -- на сервері
        // без мода фракцій це КОЖНА поява (приймання 9.1).
    }
}

// Аплікатор (R6): зняти все надіте й усе в карго, потім одягти за пресетом.
// preset == null -- лише зняти.
class OZ_LoadoutApply
{
    static void To(PlayerBase player, OZ_LoadoutPreset preset)
    {
        if (!player)
            return;

        Strip(player);

        if (!preset || !preset.Items)
            return;

        for (int i = 0; i < preset.Items.Count(); i++)
            Put(player, player, preset.Items[i], preset.Id);
    }

    // Догола -- буквально (R2.2): ні надітого, ні в контейнерах, ні в руках.
    //
    // Виміряно на стенді 2026-09-02 (ТЗ-3 R6.2): LocalDestroyEntity ставить
    // предмет у чергу ObjectDelete, але слот звільняється ОДРАЗУ -- нове
    // вбрання в тому ж кадрі сідає на місце без CallLater. Знищуємо лише
    // ВЕРХНІЙ рівень (те, що висить прямо на гравцеві): вміст іде разом із
    // контейнером, а ванільний RemoveAllItems, який проходить усе дерево,
    // на вкладеному предметі друкує "LocalDestroyEntity: No inventory
    // location" зі стеком -- теж виміряно.
    private static void Strip(PlayerBase player)
    {
        HumanInventory hi = player.GetHumanInventory();
        if (!hi)
            return;

        EntityAI inHands = hi.GetEntityInHands();
        if (inHands)
            hi.LocalDestroyEntity(inHands);

        array<EntityAI> items = new array<EntityAI>();
        hi.EnumerateInventory(InventoryTraversalType.PREORDER, items);

        for (int i = 0; i < items.Count(); i++)
        {
            EntityAI e = items[i];
            if (!e || e == player)
                continue;
            if (e.GetHierarchyParent() != player)
                continue;
            hi.LocalDestroyEntity(e);
        }
    }

    // Один предмет -- у гравця або всередину щойно створеного (`into`).
    // Неіснуючий клас -- WARNING з іменем пресета й класу, решта
    // надягається (R6.3): персонаж ніколи не лишається голим через одну
    // одруківку.
    private static EntityAI Put(PlayerBase player, EntityAI into, OZ_LoadoutItem it, string presetId)
    {
        if (!it || it.ClassName == "" || !into)
            return null;

        EntityAI e = null;

        if (it.SlotName == "hands")
        {
            // У руки -- лише самому гравцеві; вкладеному предмету рук немає.
            if (into == player)
            {
                HumanInventory hi = player.GetHumanInventory();
                if (hi)
                    e = hi.CreateInHands(it.ClassName);
            }
        }
        else if (it.SlotName != "")
        {
            int sid = InventorySlots.GetSlotIdFromString(it.SlotName);
            if (sid != InventorySlots.INVALID)
                e = into.GetInventory().CreateAttachmentEx(it.ClassName, sid);
            else
                OZ_Log.Warn("loadout " + presetId + ": no such slot \"" + it.SlotName + "\" for " + it.ClassName);
        }

        if (!e)
            e = into.GetInventory().CreateInInventory(it.ClassName);

        if (!e)
        {
            string where = "";
            if (it.SlotName != "")
                where = " (slot " + it.SlotName + ")";
            OZ_Log.Warn("loadout " + presetId + ": cannot create " + it.ClassName + where);
            return null;
        }

        if (it.Quantity > 0)
        {
            Magazine mag = Magazine.Cast(e);
            ItemBase ib  = ItemBase.Cast(e);
            if (mag)
                mag.ServerSetAmmoCount(it.Quantity);
            else if (ib)
                ib.SetQuantity(it.Quantity);
        }

        if (it.Health01 >= 0)
            e.SetHealth01("", "", it.Health01);

        if (it.QuickBar >= 0)
            player.SetQuickBarEntityShortcut(e, it.QuickBar);

        if (it.Inside)
        {
            for (int i = 0; i < it.Inside.Count(); i++)
                Put(player, e, it.Inside[i], presetId);
        }

        return e;
    }
}
