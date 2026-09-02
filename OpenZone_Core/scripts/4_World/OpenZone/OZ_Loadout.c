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
}

class OZ_LoadoutPreset
{
    string Id = "";
    ref array<ref OZ_LoadoutItem> Items;

    void OZ_LoadoutPreset()
    {
        Items = new array<ref OZ_LoadoutItem>();
    }
}

// Модифікатор -- за посадою чи міткою (R4.2). Replace -- за слотом,
// останній у порядку конфігу виграє; Add -- накопичується.
class OZ_LoadoutMod
{
    string Key = "";
    ref array<ref OZ_LoadoutItem> Replace;
    ref array<ref OZ_LoadoutItem> Add;

    void OZ_LoadoutMod()
    {
        Replace = new array<ref OZ_LoadoutItem>();
        Add     = new array<ref OZ_LoadoutItem>();
    }
}

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
    // Одноразова точка з цим словом замість id пресета -- «нічого не
    // надягати»: гравець з'являється голим (R2.1, п.1).
    static const string NONE = "-";

    private static ref OZ_LoadoutService s_Svc;

    static void Provide(OZ_LoadoutService svc)
    {
        if (!svc)
            return;

        s_Svc = svc;
        OZ_Log.Info("loadout service provided by another mod");
    }

    static bool Present()
    {
        return s_Svc != null;
    }

    static OZ_LoadoutService Get()
    {
        if (!s_Svc)
            s_Svc = new OZ_LoadoutService();
        return s_Svc;
    }

    // ШОВ. Кличе OZ_MissionServer одразу після super.OnClientNewEvent --
    // персонаж уже створений і одягнений місією. Порядок -- дзеркало
    // OZ_Spawns.Resolve (R2.1): одноразова точка вирішує першою, далі служба,
    // а без думки служби місія лишається як є.
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
