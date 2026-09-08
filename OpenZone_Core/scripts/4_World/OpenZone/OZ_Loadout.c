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

// ЧОГО У ФАЙЛІ НЕ НАПИСАНО (ТЗ-3 R4.2).
//
// Health01 і QuickBar оголошені -1: «не чіпати» й «не призначати». Але ключа,
// якого у файлі немає, серіалізатор кладе НУЛЕМ (шапка OZ_ConfigBase), а нуль
// у цих двох -- законне значення, і обидва зміряні:
//
//   * Health01 = 0 -- ВБИТА річ. GetHealthLevelValue(level) повертає межу
//     рівня, «the max health as allowed by the given health level»
//     (3_game/entities/object.c:1177-1181), а межа STATE_RUINED
//     (3_game/constants.c:851, рівень 4) -- рівно 0.0. Ваніль додає +0.001
//     саме тоді, коли треба стати ВИЩЕ межі (magazine.c:386), тобто 0.001 --
//     це BADLY_DAMAGED, а не вбите.
//   * QuickBar = 0 -- ПЕРШИЙ слот панелі. Індекси з нуля: ваніль пише
//     SetQuickBarEntityShortcut(ent, 0) (presethandlers.c:46), панель
//     інвентаря передає сирий стовпчик (inventoryquickbar.c:191), а слотів
//     десять -- MAX_QUICKBAR_SLOTS_COUNT (quickbarbase.c:1), тобто 0..9.
//
// Отже після розбору «немає» й «нуль» -- один і той самий нуль, а означають
// протилежне. Розсудити їх може лише сирий текст файла, і читається він тим
// самим прийомом, що й ванільний лоадер
// (`handle == 0` і `FGets(...) >= 0`, jsonfileloader.c:114,118).
//
// ЯК ЗІСТАВЛЯЮТЬСЯ ЗАПИСИ. Файл читається ОДИН раз на завантаження й дає
// записи предметів у порядку файла: ім'я класу плюс два прапорці «ключ був».
// Copy() бере наступний незужитий запис із таким самим іменем класу --
// розбір масивів іде в порядку файла, тож звичайно це наступний-таки, а
// пошук уперед лишається запобіжником на файл, зведений рукою в іншому
// порядку. Не знайшлось зовсім -- нічого не домислюємо: у пам'яті лишається
// прочитане, тобто файлові вірять.
//
// ЧИТАЄМО ЛИШЕ ТОДІ, КОЛИ ПИТАЮТЬ. Шлях називає лоадер (OZ_ConfigFile), але
// сам текст читає це місце й лише на першому питанні -- інакше кожен конфіг
// серії, включно з сотнями файлів гравців, отримав би зайве читання з диска
// заради ключів, яких у ньому зроду не було.
class OZ_LoadoutKeys
{
    private static const string KEY_CLASS  = "\"ClassName\"";
    private static const string KEY_HEALTH = "\"Health01\"";
    private static const string KEY_QUICK  = "\"QuickBar\"";

    private static ref array<string> s_Class  = new array<string>();
    private static ref array<int>    s_Health = new array<int>();
    private static ref array<int>    s_Quick  = new array<int>();

    // Докуди дійшло зіставлення, і яке завантаження зараз читаємо.
    private static int s_Next = 0;
    private static int s_Gen  = -1;

    // Чи триває розбір файла взагалі. Поза вікном Copy() кличуть на вже
    // нормалізованих предметах, і питати нема про що.
    static bool Reading()
    {
        return OZ_ConfigFile.Path() != "";
    }

    // Чи ніс запис цього предмета обидва ключі. false -- зіставити не
    // вдалось; тоді обидві відповіді true, тобто «вір прочитаному».
    static bool Ask(string className, out bool has01, out bool hasQb)
    {
        has01 = true;
        hasQb = true;

        string path = OZ_ConfigFile.Path();
        if (path == "")
            return false;

        Scan(path);

        int at = Find(className);
        if (at == -1)
            return false;

        s_Next = at + 1;
        has01  = s_Health[at] != 0;
        hasQb  = s_Quick[at] != 0;
        return true;
    }

    private static int Find(string className)
    {
        for (int i = s_Next; i < s_Class.Count(); i++)
        {
            if (s_Class[i] == className)
                return i;
        }
        return -1;
    }

    // Один раз на завантаження: те саме покоління -- той самий текст.
    private static void Scan(string path)
    {
        if (s_Gen == OZ_ConfigFile.Gen())
            return;

        s_Gen = OZ_ConfigFile.Gen();
        s_Class.Clear();
        s_Health.Clear();
        s_Quick.Clear();
        s_Next = 0;

        if (!FileExist(path))
            return;

        // `handle == 0` -- ванільна перевірка (jsonfileloader.c:114): тип
        // FileHandle -- int[], і `!f` на ньому не те, що тут потрібно.
        FileHandle f = OpenFile(path, FileMode.READ);
        if (f == 0)
            return;

        string line;
        while (FGets(f, line) >= 0)
            Walk(line);

        CloseFile(f);
    }

    // Ключі беруться ПО ПОРЯДКУ, у якому стоять у тексті, а не «чи є вони в
    // рядку»: JsonFileLoader пише один ключ на рядок, але файл, зведений
    // адміном в один рядок, цим самим кодом читається так само.
    private static void Walk(string line)
    {
        int at = 0;
        int len = line.Length();

        while (at < len)
        {
            int c = line.IndexOfFrom(at, KEY_CLASS);
            int h = line.IndexOfFrom(at, KEY_HEALTH);
            int q = line.IndexOfFrom(at, KEY_QUICK);

            int next = Least(c, Least(h, q));
            if (next == -1)
                return;

            if (next == c)
            {
                s_Class.Insert(Value(line, c));
                s_Health.Insert(0);
                s_Quick.Insert(0);
                at = c + KEY_CLASS.Length();
            }
            else if (next == h)
            {
                Mark(s_Health);
                at = h + KEY_HEALTH.Length();
            }
            else
            {
                Mark(s_Quick);
                at = q + KEY_QUICK.Length();
            }
        }
    }

    // Менше з двох, де -1 означає «не знайдено», а не «найменше».
    private static int Least(int a, int b)
    {
        if (a == -1)
            return b;
        if (b == -1)
            return a;
        if (a < b)
            return a;
        return b;
    }

    private static void Mark(array<int> flags)
    {
        int n = flags.Count();
        if (n > 0)
            flags.Set(n - 1, 1);
    }

    // Значення рядкового ключа, що починається на `at`: від першої лапки
    // після його імені до наступної.
    private static string Value(string line, int at)
    {
        int open = line.IndexOfFrom(at + KEY_CLASS.Length(), "\"");
        if (open == -1)
            return "";

        int close = line.IndexOfFrom(open + 1, "\"");
        if (close == -1)
            return "";

        return line.Substring(open + 1, close - open - 1);
    }
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
    // КЛЮЧА НЕМАЄ -- «НЕ ЧІПАТИ»; НУЛЬ -- ЦЕ НУЛЬ (ТЗ-3 R4.2).
    //
    // Ключа, якого У ФАЙЛІ НЕМАЄ, копія переносить НУЛЕМ, а не значенням з
    // ініціалізатора, -- і саме тому тут стоїть питання до тексту файла
    // (OZ_LoadoutKeys). Нуль у цих двох ключах СПРАВЖНІЙ: 0 у Health01 --
    // вбита річ, 0 у QuickBar -- перший слот панелі, і обидва законно
    // просяться з файла. Відхиляти їх як «ключа немає» означало б відібрати
    // в адміна два значення, яких у нього нема чим замінити.
    OZ_LoadoutItem Copy(string where = "")
    {
        OZ_LoadoutItem c = new OZ_LoadoutItem();
        c.ClassName = ClassName;
        c.SlotName  = SlotName;
        c.Quantity  = Quantity;
        c.Health01  = Health01;
        c.QuickBar  = QuickBar;

        // Спершу свій запис, потім вкладені: у файлі Inside стоїть після
        // Health01 і QuickBar свого предмета, і читач іде тим самим порядком.
        bool has01;
        bool hasQb;
        if (OZ_LoadoutKeys.Ask(ClassName, has01, hasQb))
        {
            if (!has01)
                c.Health01 = -1;
            if (!hasQb)
                c.QuickBar = -1;
        }
        else if (OZ_LoadoutKeys.Reading() && Zero())
        {
            OZ_Log.Dbg(Unmatched(where));
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

    private bool Zero()
    {
        if (Health01 == 0)
            return true;
        return QuickBar == 0;
    }

    // Запис предмета не знайшовся в тексті файла, а нуль у ключі є. Беремо
    // як написано -- вбита річ, нульовий слот, -- і лишаємо рядок, з якого
    // адмін почне, якщо побачив не те. Ім'я пресета тут і потрібне: класів у
    // файлі десятки, і «Hoodie_Blue» без пресета не знаходиться.
    private string Unmatched(string where)
    {
        string m = "loadout";
        if (where != "")
            m += " " + where;
        m += ": " + ClassName + " has a zero Health01 or QuickBar and no entry of its own in the file text";
        m += " - taken as written (0 health is a ruined item, quick slot 0 is the first one)";
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
                // Id -- щоб єдиний рядок, який звідти може вийти (запис не
                // зіставився з текстом файла), називав ПРЕСЕТ, а не саму лише
                // назву класу: у файлі їх десятки.
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
