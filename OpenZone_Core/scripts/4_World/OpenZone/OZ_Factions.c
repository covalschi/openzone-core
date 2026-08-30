// Фракції -- служба ЯДРА, а не КПК.
//
// Живуть тут, бо фракціями користується не лише екран. Квестовий мод питає,
// чи свій перед ним; торговець -- чи продавати; ІІ -- чи стріляти; рація --
// кого пускати в канал. Якби таблиця лишалась у КПК, кожен із них тягнув би
// за собою мод ІНТЕРФЕЙСУ заради питання «хто в якій фракції». Тут її може
// спитати будь-хто, хто вже залежить від ядра.
//
// Ядро фракції НЕ ПРИДУМУЄ і нікого до них не приписує. Воно тримає таблицю
// (як звати, яким кольором малювати, хто кому ворог) і відповідає на питання
// «чия фракція». Хто до кого належить -- вирішує або чужий мод через
// постачальника, або ролі Discord, або адмін у файлі гравця.
//
// ПОРЯДОК СТАРШИНСТВА, і його треба знати, бо він визначає, хто кого
// перебиває:
//
//     1. постачальник (чужий мод)   -- знає найкраще, сказав слово -- воно й
//                                      буде
//     2. роль Discord               -- якщо акаунт прив'язаний і роль
//                                      відповідає фракції з таблиці
//     3. файл акаунта               -- те, що поставив адмін або квест
//
// Порожня фракція означає «одинак». Це не помилка й не збій -- це найчастіший
// стан у Зоні.

// Ставлення однієї фракції до іншої.
//
// Односторонньо в JSON, СИМЕТРИЧНО за замовчуванням у відповіді: якщо «Борг»
// оголосив ворожість «Волі», а «Воля» промовчала, вони все одно вороги.
// Односторонню ворожість можна задати явно -- просто напиши обидва рядки.
class OZ_FactionRelation
{
    string With;
    string Stand;
}

// Один запис таблиці.
//
// БЕЗ ІНІЦІАЛІЗАТОРІВ ПОЛІВ, навмисно: чужий мод має право успадкуватись від
// цього класу, а ініціалізатор поля в класі, від якого успадковуються, ламає
// нащадкам доступ до їхніх власних private-методів (виміряно 2026-08-26, див.
// скіл dayz-modding). Умовчання роздає Validate() -- він однаково мусить
// пройтись по записах, прочитаних із диска, де половини полів може не бути.
class OZ_Faction
{
    string Id;
    string DisplayName;

    // Коротка позначка для тісних місць: рядок контакту, підпис маячка.
    // Порожня -- малюй повну назву.
    string Short;

    // Колір рядком "R G B", 0..255. Не число ARGB: у JSON його читає ЛЮДИНА,
    // а 4278219546 не каже нікому нічого. ColorARGB() перетворює на те, що
    // розуміє віджет.
    string Color;

    // Чи можна вступити внутрішньоігровою дією. Ядро цього не enforce'ить --
    // воно лише возить прапорець для того, хто вступ і реалізує.
    bool Joinable;

    // Не показувати в переліках. Для службових фракцій: адміністрація,
    // сюжетні угруповання, які ще не мали з'явитись.
    bool Hidden;

    // БАЗОВА ІДЕНТИЧНІСТЬ, а не організація (рішення власника 2026-08-30).
    //
    // «Сталкери» -- це всі в Зоні: значок носить кожен, він не знімається при
    // вступі кудись іще, і в нього немає ні лідера, ні складу, ні внутрішніх
    // звань. Тому фракційний екран для такого запису НЕ ВІДКРИВАЄТЬСЯ: бачити
    // там поіменний список усіх сталкерів сервера і кнопку «вигнати» --
    // безглуздо й до того ж видало б людей, яких ніхто не питав.
    //
    // Для всього іншого це звичайна фракція: назва, колір, ставлення,
    // зона спавну працюють як завжди.
    bool BaseFaction;

    // Стеля складу САМЕ ЦІЄЇ фракції: скільки людей у ній може бути разом
    // (рахуються проекції ролей -- увесь відомий склад, не лише присутні).
    // 0 -- без межі. Рішення власника 2026-08-30: межа фракційна, не
    // серверна -- у «Долга» свій штат, у вчених свій.
    int MaxMembers;

    // Довільні мітки для групування: "military", "hostile-to-all", що
    // завгодно. Ядро в них не заглядає.
    ref array<string> Tags;

    ref array<ref OZ_FactionRelation> Relations;

    // Чуже добро. Рядок JSON, який ядро ВОЗИТЬ І НЕ ЧИТАЄ -- той самий
    // принцип, що з носіями даних і з конвертами моста. Чужий мод кладе сюди
    // свої поля й не мусить чекати, поки ми розширимо схему.
    string Extra;
}

class OZ_FactionsConfig : OZ_ConfigBase
{
    ref array<ref OZ_Faction> Factions;

    override int LatestVersion()
    {
        return 4;
    }

    override void LoadDefaults()
    {
        Version  = LatestVersion();
        Factions = new array<ref OZ_Faction>();

        // Id -- КЛЮЧ ЗВ'ЯЗКУ між ботом і грою, і єдиний рядок, який обидві
        // сторони мусять знати посимвольно. Розбіжність тут не косметична:
        // синхронізація ролей просто не знайде нічого, мовчки.
        //
        // Назви -- УКРАЇНСЬКІ й ЖИВИМ ТЕКСТОМ, а не ключами stringtable.
        // Причина виміряна: у наборі мов рушія немає колонки ukrainian
        // (заголовок таблиці -- original, english, czech, german, russian,
        // polish, hungarian, italian, spanish, french, chinese, japanese,
        // portuguese, chinesesimp). Отже #STR_-ключ українською не
        // намалюється НІКОЛИ, і текст мусить їхати даними.
        //
        // Порожня фракція -- це «одинак», відсутність приналежності. Тому
        // окремого id `loner` НЕМАЄ: він означав би те саме, але поводився
        // інакше -- Stand("", x) дає NEUTRAL, а Stand("loner","loner") дав би
        // ALLY, тобто всі неприкаяні стали б союзниками одне одному. А от
        // `neutral` -- справжня організація: у неї є лідер.
        Add("ecolog",    "Вчені",      "ВЧН", "230 200  90");
        Add("duty",      "Долг",       "ДЛГ", "196  64  40");
        Add("freedom",   "Воля",       "ВОЛ", " 96 176  72");
        Add("mercenary", "Найманці",   "НАЙ", " 80 130 190");
        Add("neutral",   "Нейтрали",   "НЕЙ", "200 200 200");
        // «Нейтрали» -- органiзацiя з лiдером; вiльнi сталкери -- БАЗОВА
        // фракцiя (рiшення власника 2026-08-30): її носять усi, вона не
        // знiмається вступом кудись iще й не має нi лiдера, нi складу.
        // Порожнiй Id як був, так i лишається «одинаком» без приналежностi
        // взагалi -- це не те саме, що бути сталкером.
        Add("stalker",   "Сталкери",   "СТК", "216 192 112", true);
        Add("bandit",    "Бандити",    "БАН", "150 120  70");
        Add("clearsky",  "Чисте небо", "ЧН",  "120 190 200");
        Add("monolith",  "Моноліт",    "МНЛ", "170 150 220");
        Add("military",  "Військові",  "ВІЙ", "110 130  90");

        // Роздано НЕ повністю: тільки те, що в першоджерелі не обговорюється.
        // Решту хай ставить сервер -- його сюжету ми не знаємо.
        Relate("duty",     "freedom",  "hostile");
        Relate("bandit",   "neutral",  "hostile");
        Relate("monolith", "neutral",  "hostile");
        Relate("monolith", "duty",     "hostile");
        Relate("monolith", "freedom",  "hostile");
        Relate("monolith", "clearsky", "hostile");
        Relate("military", "bandit",   "hostile");
        Relate("military", "monolith", "hostile");
        Relate("ecolog",   "neutral",  "friendly");
        Relate("ecolog",   "clearsky", "friendly");
    }

    private void Add(string id, string name, string tag, string colour, bool baseFaction = false)
    {
        OZ_Faction f = new OZ_Faction();
        f.Id            = id;
        f.DisplayName   = name;
        f.Short         = tag;
        f.Color         = colour;
        f.Joinable      = false;
        f.Hidden        = false;
        f.BaseFaction   = baseFaction;
        f.Tags          = new array<string>();
        f.Relations     = new array<ref OZ_FactionRelation>();
        f.Extra         = "";
        Factions.Insert(f);
    }

    private void Relate(string from, string to, string stand)
    {
        for (int i = 0; i < Factions.Count(); i++)
        {
            if (Factions[i].Id != from)
                continue;

            OZ_FactionRelation r = new OZ_FactionRelation();
            r.With  = to;
            r.Stand = stand;
            Factions[i].Relations.Insert(r);
            return;
        }
    }

    // Справжня міграція, а не штамп версії.
    //
    // v1 не знав про Short, Tags, Relations і Extra. Файл
    // адміна з версією 1 треба ДОПОВНИТИ умовчаннями, а не перезаписати
    // нашими фракціями: там уже може стояти його власний список.
    override bool Migrate(int from)
    {
        if (from > LatestVersion())
            return false;

        if (!Factions)
            Factions = new array<ref OZ_Faction>();

        // v2 -> v3: два id перейменовані під набір, узгоджений із ботом.
        //
        // Перейменування id -- НЕ косметика: на нього посилаються поле
        // Faction у файлах гравців і рядки Relations. Тому міняємо і сам Id,
        // і кожне посилання на нього. Записи адміна, яких у новому наборі
        // немає (скажімо, `military`), лишаються НЕДОТОРКАНИМИ: його таблиця
        // -- його справа, а видалити чуже під час міграції означало б
        // втратити дані мовчки.
        if (from < 3)
        {
            Rename("ecologist", "ecolog",  "Ecologists", "Вчені");
            Rename("loner",     "neutral", "Loners",     "Нейтрали");
        }

        // v3 -> v4: з'явилась базова фракцiя. Позначаємо ЛИШЕ «сталкерiв» i
        // лише якщо запис у файлi є: чужi фракцiї адмiна це не чiпає, а
        // прапорець на них ставити нема за що.
        if (from < 4)
        {
            for (int i = 0; i < Factions.Count(); i++)
            {
                if (Factions[i].Id == "stalker")
                    Factions[i].BaseFaction = true;
            }
        }

        // Порожні поля роздасть Validate() -- він біжить одразу після
        // міграції й робить рівно цю роботу для будь-якого запису з диска.
        Version = LatestVersion();
        OZ_Log.Dbg("factions migrated from v" + from.ToString());
        return true;
    }

    // Перейменування тягне за собою ПІДПИС, інакше запис виходить із
    // міграції суперечливим сам собі.
    //
    // Було: Id стає "neutral", DisplayName лишається "Loners" -- тобто фракція
    // з новим іменем і старим поняттям на екрані. Це видно й зараз у файлі
    // стенду. Реєстр із бота перекриває підпис на льоту, тож помітно стає саме
    // тоді, коли моста немає, -- у найгіршу мить.
    //
    // Міняємо ЛИШЕ якщо адмін підпису не чіпав, тобто там досі стоїть старе
    // умовчання. Свій підпис -- його справа, і затирати його міграцією
    // означало б красти роботу.
    private void Rename(string oldId, string newId, string wasLabel, string nowLabel)
    {
        bool touched = false;

        for (int i = 0; i < Factions.Count(); i++)
        {
            if (Factions[i].Id == oldId)
            {
                Factions[i].Id = newId;

                if (Factions[i].DisplayName == wasLabel)
                    Factions[i].DisplayName = nowLabel;

                touched = true;
            }

            if (!Factions[i].Relations)
                continue;

            for (int j = 0; j < Factions[i].Relations.Count(); j++)
            {
                if (Factions[i].Relations[j].With != oldId)
                    continue;
                Factions[i].Relations[j].With = newId;
                touched = true;
            }
        }

        if (touched)
            OZ_Log.Info("factions: renamed \"" + oldId + "\" to \"" + newId + "\"");
    }

    override void Validate(out int warnings)
    {
        warnings = 0;

        if (!Factions)
            Factions = new array<ref OZ_Faction>();

        for (int i = 0; i < Factions.Count(); i++)
        {
            OZ_Faction f = Factions[i];

            if (f.Id == "")
            {
                OZ_Log.Warn("faction #" + i.ToString() + " has no Id - it will never match anybody");
                warnings++;
            }

            if (f.DisplayName == "")
                f.DisplayName = f.Id;

            if (f.Color == "")
                f.Color = "200 200 200";

            if (!f.Tags)
                f.Tags = new array<string>();

            if (!f.Relations)
                f.Relations = new array<ref OZ_FactionRelation>();

            for (int j = 0; j < f.Relations.Count(); j++)
            {
                if (f.Relations[j].Stand == "")
                    f.Relations[j].Stand = OZ_FactionStand.NEUTRAL;

                if (f.Relations[j].With != "")
                    continue;

                string w = "faction \"" + f.Id;
                w += "\" declares a relation to nobody - it will never match";
                OZ_Log.Warn(w);
                warnings++;
            }
        }
    }
}

// Реєстр, як його присилає бот. НЕ те саме, що OZ_Faction: тут лише те, чим
// бот володіє, і жодного id ролі Discord -- гра його не потребує взагалі,
// бо бот перекладає ролі в слаги ще до того, як щось перетне провід.
class OZ_FactionRosterEntry
{
    string Id;
    string DisplayName;
    string Color;

    // Базова фракцiя: її носять усi й вона не є органiзацiєю. Див. довге
    // пояснення в полi OZ_Faction.BaseFaction.
    bool Base;
}

// Підпис однієї ролі, будь-якої з трьох осей. Той самий вигляд, що й у
// фракції, мінус колір: звання й мітки на екрані не фарбуються.
class OZ_RoleName
{
    string Id;
    string DisplayName;
}

class OZ_FactionRoster
{
    int Stamp;
    ref array<ref OZ_FactionRosterEntry> Factions;

    // Iншi осi -- лише пiдписи. Приїжджають тим самим конвертом, бо
    // змiнюються так само рiдко й з того самого джерела. FRanks --
    // внутрiфракцiйнi звання, id вигляду "duty:sergeant".
    ref array<ref OZ_RoleName> Ranks;
    ref array<ref OZ_RoleName> Traits;
    ref array<ref OZ_RoleName> Posts;
    ref array<ref OZ_RoleName> FRanks;
}

// Слова, якими описується ставлення. Рядками, бо їх читає адмін у JSON, і
// enum у файлі виглядав би числом.
class OZ_FactionStand
{
    static const string ALLY     = "ally";
    static const string FRIENDLY = "friendly";
    static const string NEUTRAL  = "neutral";
    static const string WARY     = "wary";
    static const string HOSTILE  = "hostile";
}

// Договір для чужого мода. Успадковуєш, перекриваєш FactionOf, прив'язуєш
// одним рядком зі свого OnMissionStart:
//
//     OZ_Factions.Bind(new MyFactionProvider());
//
// Повертати треба id із Factions.json. Незнайоме id ми покажемо ЯК Є -- краще
// чуже слово на екрані, ніж мовчазна підміна на «одинак».
class OZ_FactionProvider
{
    string FactionOf(PlayerBase player)
    {
        return "";
    }
}

class OZ_Factions
{
    private static ref OZ_FactionsConfig s_Cfg;
    private static ref OZ_FactionProvider s_Provider;

    // Ролі Discord: uid -> id фракції. Наповнює міст через прив'язку акаунта;
    // порожня мапа означає «синхронізації немає», а не «усі без фракції».
    private static ref map<string, string> s_ByRole;

    // Хто хоче знати про зміни, не опитуючи. Параметр -- Steam64 гравця,
    // чия фракція змінилась.
    static ref ScriptInvoker OnChanged = new ScriptInvoker();

    static void Bind(OZ_FactionProvider provider)
    {
        s_Provider = provider;
        OZ_Log.Dbg("faction provider bound");
    }

    static bool HasProvider()
    {
        return s_Provider != null;
    }

    // Ідемпотентна: хто перший покликав, той і завантажив.
    //
    // Порядок CF-модулів не гарантований, і на цьому стенді він УЖЕ підводив
    // -- рація відпрацювала раніше за КПК. Тому кличуть і ядро, і КПК, і будь
    // хто ще, кому таблиця потрібна раніше за нас.
    static void ServerLoad()
    {
        if (s_Cfg)
            return;
        Reload();
    }

    // Перечитати з диска примусово. Окремо від ServerLoad саме заради
    // майбутньої гарячої перезагрузки: тригером буде операція, а не таймер
    // (порівняння mtime неможливе -- рушій віддає лише FileAttr).
    static void Reload()
    {
        s_Cfg = new OZ_FactionsConfig();
        OZ_ConfigLoader<OZ_FactionsConfig>.Load(OZ_Const.PROFILE_DIR + "\\Factions.json", "factions", s_Cfg);
    }

    static int Count()
    {
        if (!s_Cfg)
            return 0;
        if (!s_Cfg.Factions)
            return 0;
        return s_Cfg.Factions.Count();
    }

    // ------------------------------------------------------------- таблиця

    static OZ_Faction Find(string id)
    {
        if (id == "")
            return null;
        if (!s_Cfg)
            return null;
        if (!s_Cfg.Factions)
            return null;

        for (int i = 0; i < s_Cfg.Factions.Count(); i++)
        {
            if (s_Cfg.Factions[i].Id == id)
                return s_Cfg.Factions[i];
        }
        return null;
    }

    // Усі id. `includeHidden` -- для того, хто справді хоче всі: адмінського
    // інструменту чи експорту. Перелік для гравця має ходити без нього.
    static void Ids(out array<string> outIds, bool includeHidden = false)
    {
        if (!outIds)
            outIds = new array<string>();
        outIds.Clear();

        if (!s_Cfg)
            return;
        if (!s_Cfg.Factions)
            return;

        for (int i = 0; i < s_Cfg.Factions.Count(); i++)
        {
            if (s_Cfg.Factions[i].Hidden)
            {
                if (!includeHidden)
                    continue;
            }
            outIds.Insert(s_Cfg.Factions[i].Id);
        }
    }

    // Чи це БАЗОВА фракцiя -- та, яку носять усi (сталкери). Питати треба
    // всюди, де йдеться про фракцiю ЯК ПРО ОРГАНIЗАЦIЮ: склад, лiдер,
    // внутрiшнi звання, запрошення. Там, де йдеться про назву й колiр,
    // питати не треба -- базова фракцiя малюється, як усi.
    //
    // Незнайоме id -- НЕ базове: мовчазне «так» зачинило б фракцiйний
    // екран тим, кого ми просто ще не знаємо.
    static bool IsBase(string id)
    {
        OZ_Faction f = Find(id);
        if (!f)
            return false;
        return f.BaseFaction;
    }

    // Фракцiя ЯК ОРГАНIЗАЦIЯ: те саме, що OfUid, але базова вважається
    // вiдсутнiстю. Один виклик замiсть пари «взяв i перевiрив», щоб
    // правило жило в одному мiсцi.
    static string OrgOfUid(string uid)
    {
        string slug = OfUid(uid);
        if (slug == "")
            return "";
        if (IsBase(slug))
            return "";
        return slug;
    }

    // Людська назва. Незнайоме id повертаємо ЯК Є: чуже слово на екрані
    // чесніше за мовчазну підміну на «одинак».
    static string NameOf(string id)
    {
        if (id == "")
            return "";

        OZ_Faction f = Find(id);
        if (f)
            return f.DisplayName;
        return id;
    }

    static string ShortOf(string id)
    {
        OZ_Faction f = Find(id);
        if (!f)
            return id;
        if (f.Short != "")
            return f.Short;
        return f.DisplayName;
    }

    static string ColorOf(string id)
    {
        OZ_Faction f = Find(id);
        if (f)
            return f.Color;
        return "200 200 200";
    }

    // "R G B" -> ARGB для віджета. Досі колір лежав у таблиці й не читався
    // ніким: на провід ішла тільки назва.
    static int ColorARGB(string id, int alpha = 255)
    {
        string raw = ColorOf(id);

        array<string> parts = new array<string>();
        raw.Split(" ", parts);

        // Три окремі змінні, а не масив сталого розміру: подвійні пробіли в
        // "196  64  40" дають порожні токени, тож рахувати треба ЗАПОВНЕНІ, а
        // не позиції.
        int r = -1;
        int g = -1;
        int b = -1;

        for (int i = 0; i < parts.Count(); i++)
        {
            if (parts[i] == "")
                continue;

            if (r < 0)
            {
                r = parts[i].ToInt();
                continue;
            }
            if (g < 0)
            {
                g = parts[i].ToInt();
                continue;
            }
            if (b < 0)
            {
                b = parts[i].ToInt();
                break;
            }
        }

        if (b < 0)
            return ARGB(alpha, 200, 200, 200);

        return ARGB(alpha, r, g, b);
    }

    // ---------------------------------------------------------- членство

    // Чия фракція. Старшинство описане в шапці файлу.
    static string Of(PlayerBase player, string uid)
    {
        if (s_Provider)
        {
            if (player)
            {
                string fromMod = s_Provider.FactionOf(player);
                if (fromMod != "")
                    return fromMod;
            }
        }

        if (uid == "")
            return "";

        if (s_ByRole)
        {
            string byRole;

            // НАЯВНІСТЬ запису -- це вже відповідь, і порожній рядок теж.
            //
            // Тут стояла перевірка `if (byRole != "")`, і вона тихо ламала
            // весь сенс розрізнення, яке боронить ForgetRole. Гравця вигнали
            // з Боргу в Discord, бот чесно сказав «ролі фракції не має», а гра
            // читала "duty" з його файла далі -- НАЗАВЖДИ. І це значення
            // годує AreHostile(), тобто рішення стріляти чи ні.
            //
            // Правило одне: запис Є -- Discord авторитетний, включно з
            // порожнім рядком. Запису НЕМАЄ -- ми не знаємо, і лише тоді
            // працює файл акаунта. Прибрати запис може тільки ForgetRole.
            if (s_ByRole.Find(uid, byRole))
                return byRole;
        }

        OZ_PlayerData d = OZ_PlayerStore.Load(uid);
        return d.Faction;
    }

    // Коли гравця під рукою немає -- офлайновий друг, запис у розмові.
    static string OfUid(string uid)
    {
        return Of(null, uid);
    }

    // Поставити фракцію в файл акаунта. Тільки сервер.
    //
    // Постачальник і роль Discord цим НЕ перебиваються -- вони старші, і
    // мовчазна незгода тут була б найгіршим виходом. Хто ставить фракцію
    // руками при живому постачальнику, той міняє запасний шлях, і це його
    // право.
    static void SetOf(string uid, string factionId)
    {
        if (!GetGame().IsServer())
            return;
        if (uid == "")
            return;

        OZ_PlayerData d = OZ_PlayerStore.Load(uid);
        if (d.Faction == factionId)
            return;

        d.Faction = factionId;
        OZ_PlayerStore.MarkDirty(uid);
        OnChanged.Invoke(uid);
    }

    // Міст приніс ролі. Порожній id фракції = «ролі, що відповідає фракції,
    // у нього немає» -- тоді працює запасний шлях через файл акаунта.
    static void SetFromRole(string uid, string factionId)
    {
        if (!GetGame().IsServer())
            return;
        if (uid == "")
            return;

        if (!s_ByRole)
            s_ByRole = new map<string, string>();

        string had;
        s_ByRole.Find(uid, had);
        if (had == factionId)
            return;

        s_ByRole.Set(uid, factionId);
        OnChanged.Invoke(uid);
    }

    // Синхронізація ролей зникла (міст ліг, гравець відв'язав акаунт).
    // Прибрати ЗАПИС, а не поставити порожній рядок: порожній рядок означав
    // би «ролі немає», а нам треба «ми не знаємо».
    static void ForgetRole(string uid)
    {
        if (!s_ByRole)
            return;
        if (!s_ByRole.Contains(uid))
            return;

        s_ByRole.Remove(uid);
        OnChanged.Invoke(uid);
    }

    // --------------------------------------------------------- ставлення

    // Як `a` ставиться до `b`.
    //
    // Симетрично за замовчуванням: якщо `a` мовчить, дивимось, що сказав `b`.
    // Односторонню ворожість задають ЯВНО -- двома рядками в JSON.
    static string Stand(string a, string b)
    {
        if (a == "")
            return OZ_FactionStand.NEUTRAL;
        if (b == "")
            return OZ_FactionStand.NEUTRAL;
        if (a == b)
            return OZ_FactionStand.ALLY;

        string direct = Declared(a, b);
        if (direct != "")
            return direct;

        string mirrored = Declared(b, a);
        if (mirrored != "")
            return mirrored;

        return OZ_FactionStand.NEUTRAL;
    }

    static bool AreHostile(string a, string b)
    {
        return Stand(a, b) == OZ_FactionStand.HOSTILE;
    }

    static bool AreFriendly(string a, string b)
    {
        string s = Stand(a, b);
        if (s == OZ_FactionStand.ALLY)
            return true;
        return s == OZ_FactionStand.FRIENDLY;
    }

    private static string Declared(string from, string to)
    {
        OZ_Faction f = Find(from);
        if (!f)
            return "";
        if (!f.Relations)
            return "";

        for (int i = 0; i < f.Relations.Count(); i++)
        {
            if (f.Relations[i].With == to)
                return f.Relations[i].Stand;
        }
        return "";
    }

    // ------------------------------------------------------------- реєстр

    // Реєстр від бота: як фракція ЗВЕТЬСЯ і якого вона кольору.
    //
    // ДВА ДОМИ, і поділ між ними -- за тим, ХТО МОЖЕ це налаштувати, а не за
    // тим, хто читає:
    //
    //   бот  -- слаг, назва, колір. Він створює ролі в Discord, отже дізнається
    //           про них першим, і власник вимагав налаштовувати звідти.
    //   гра  -- Relations, Joinable, Hidden. Правила симуляції цього сервера,
    //           яким у Discord немає де жити.
    //
    // Потік В ОДИН БІК. Бот може ДОДАТИ фракцію й переписати назву та колір;
    // видалити запис адміна або торкнутись його ставлень -- ніколи. Один
    // напрямок і один ключ зв'язку -- це і є весь механізм проти розходження.
    //
    // НЕ пишеться на диск. Мітка -- косметика, і при мертвому мості вона на
    // вісім секунд відкотиться до того, що в файлі адміна. Записувати чужу
    // назву в його файл означало б, що вона лишиться там і після того, як
    // бота приберуть.
    static void ApplyRoster(OZ_FactionRoster r)
    {
        if (!GetGame().IsServer())
            return;
        if (!r || !r.Factions)
            return;
        if (!s_Cfg)
            return;

        int added = 0;
        int renamed = 0;

        for (int i = 0; i < r.Factions.Count(); i++)
        {
            OZ_FactionRosterEntry e = r.Factions[i];
            if (e.Id == "")
                continue;

            OZ_Faction f = Find(e.Id);

            if (!f)
            {
                f = new OZ_Faction();
                f.Id            = e.Id;
                        f.Joinable      = false;
                f.Hidden        = false;
                f.Tags          = new array<string>();
                f.Relations     = new array<ref OZ_FactionRelation>();
                f.Extra         = "";
                s_Cfg.Factions.Insert(f);
                added++;
            }

            if (e.DisplayName != "")
            {
                if (f.DisplayName != e.DisplayName)
                    renamed++;
                f.DisplayName = e.DisplayName;
            }

            if (e.Color != "")
                f.Color = e.Color;

            // Базовiсть -- слово РЕЄСТРУ: саме бот вирiшує, яка фракцiя
            // всезагальна, бо саме вiн вдягає її значок на кожного при
            // прив'язцi. Ставимо лише true: власний прапорець адмiна в
            // файлi реєстр не знiмає, бо реєстр про нього не знає.
            if (e.Base)
                f.BaseFaction = true;
        }

        string m = "factions: roster from the bridge, stamp " + r.Stamp.ToString();
        m += " (" + added.ToString() + " added, ";
        m += renamed.ToString() + " renamed)";

        // БАЗОВУ НАЗИВАЄМО ВГОЛОС. Її прапорець приходить лише з реєстру й
        // на диск не лягає (записи бота живуть у пам'ятi), тож без цього
        // рядка перевiрити, чи вiн доїхав, можна було б тiльки з гри --
        // саме та дiагностика, яку шукають о другiй ночi.
        string bases = "";
        for (int b = 0; b < s_Cfg.Factions.Count(); b++)
        {
            if (!s_Cfg.Factions[b].BaseFaction)
                continue;
            if (bases != "")
                bases += ",";
            bases += s_Cfg.Factions[b].Id;
        }
        if (bases == "")
            bases = "none";
        m += " base=" + bases;

        OZ_Log.Info(m);
    }

    // ------------------------------------------------------------- експорт

    // Уся таблиця одним рядком JSON -- для чужого мода, який хоче її
    // перекинути кудись цілком: адмінському інструменту, вебредактору, боту.
    // Порожній рядок означає, що таблиця не завантажена або не серіалізується.
    static string ExportJson()
    {
        if (!s_Cfg)
            return "";

        string outJson;
        string err;
        if (!JsonFileLoader<OZ_FactionsConfig>.MakeData(s_Cfg, outJson, err, false))
        {
            OZ_Log.Error("factions export failed: " + err);
            return "";
        }
        return outJson;
    }
}
