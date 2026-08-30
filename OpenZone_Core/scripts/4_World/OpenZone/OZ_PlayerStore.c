// Дані гравця: один файл на Steam64 у $profile:OpenZone\players\.
//
// Чому НЕ CF_ModStorage: він прив'язаний до сутності й помирає разом із
// персонажем. Друзі, мітки й прив'язка до Discord помирати не повинні --
// це власність акаунта, а не тіла.
//
// Запис відкладений: пачкою й на дисконекті, а не на кожну зміну. Гравців
// може бути вісімдесят, а змін -- десятки на хвилину в кожного.

class OZ_PlayerData : OZ_ConfigBase
{
    string SteamId   = "";
    string DiscordId = "";
    string FirstSeen = "";
    string LastSeen  = "";

    // Ім'я з останнього входу. Кеш, а не джерело правди: воно потрібне, щоб
    // показати запит у друзі від того, кого зараз немає на сервері. Без
    // кешу довелось би або показувати Steam64 замість імені, або мовчати про
    // офлайнових зовсім.
    string Name = "";

    // --- останнє, що ми про нього знали ---
    //
    // Знімок ролей із останнього разу, коли гравець був у мережі. Це кеш
    // ПОКАЗУ, того ж роду, що й Name вище: щоб офлайновий контакт у списку
    // не перетворювався на голе ім'я без фракції й звання.
    //
    // Окремі поля, а НЕ Faction нижче -- і це головне в усьому записі.
    // Faction годує запасний шлях OZ_Factions.Of(), а той годує AreHostile(),
    // тобто рішення стріляти чи ні. Записати туди проекцію Discord означало б
    // рівно те, від чого застерігає шапка OZ_Roles: роль, зняту при мертвому
    // мості, вже ніщо не зніме, і гра стрілятиме по своїх ще довго після того,
    // як у гільдії все виправили.
    //
    // Ці ж поля не читає ніхто, крім списку контактів, і той показує їх під
    // позначкою «застаріле».
    string SeenFaction = "";
    string SeenRank    = "";
    // Внутрiфракцiйне звання -- окрема вiсь вiд SeenRank (сталкерського).
    string SeenFRank   = "";
    ref array<string> SeenPosts;
    ref array<string> SeenTraits;

    // Епоха сесій. Пристрій пам'ятає, з якою епохою на ньому відкрилась
    // сесія, і вважається ОНЛАЙН, поки вона збігається з цією.
    //
    // «Скинути інші сесії» -- це просто += 1. Усі решта пристроїв мовчки
    // переходять в офлайн: вони не ламаються й не втрачають даних, вони
    // перестають оновлюватись, і їхній знімок замерзає на цій миті.
    //
    // Ніякого обліку чужих предметів, переживає рестарт, коштує одну
    // операцію -- а альтернатива вимагала б знати, де зараз кожен пристрій,
    // який гравець колись тримав у руках.
    int SessionEpoch = 1;

    // ПОКОЛІННЯ ПЕРСОНАЖА. Один Steam-акаунт -- багато життів у Зоні, і це
    // різні люди: після пермадесу старий запис заморожується у власний файл
    // (players\<uid>.g<N>.json), а тут починається новий -- з тим самим
    // Steam64 і чистим усім.
    //
    // Нуль у старому файлі означає перше покоління: до пермадесу все, що
    // існувало, було першим життям.
    int Gen = 1;

    // --- транспондер ---
    //
    // Кому видно твою позицію на чужій карті. Рядком, бо це ж слово лежить у
    // JSON і його читає адмін:
    //
    //   "off"      -- нікому. Значення за замовчуванням: маячок мовчить, доки
    //                 гравець сам його не увімкнув. Іншого безпечного
    //                 замовчування тут немає.
    //   "contacts" -- лише тим, кого перелічено в TransponderTo
    //   "friends"  -- усім друзям
    //   "public"   -- усім, хто в радіусі прийому
    //
    // Приймати чужі маячки й вести свій однаково потребує АНТЕНИ: радіус дає
    // саме вона. Без модуля антени транспондер не працює в обидва боки.
    string TransponderMode = "off";
    ref array<string> TransponderTo;

    // --- присутність ---
    //
    // Чи видно тебе в списку «хто зараз у Зоні». Це ІНША річ, ніж транспондер,
    // і плутати їх не можна:
    //
    //   транспондер -- твоя точка на чужій КАРТІ, у радіусі антени;
    //   присутність -- твоє ім'я в СПИСКУ онлайну, на весь сервер.
    //
    // Вимикачі незалежні: можна числитись у мережі й не світити позицію, і
    // навпаки -- вести маячок для двох контактів, лишаючись невидимим для
    // решти. Замовчування тут протилежне транспондеру: у списку видно, бо
    // список і є те, заради чого КПК носять.
    bool PresenceHidden = false;

    // --- друзі ---
    //
    // Дружба ВЗАЄМНА і потребує згоди обох. Тому списків два: прийняті друзі
    // й вхідні запити. Односторонній «друг» був би способом стежити за тим,
    // хто про це не знає.
    //
    // Просити в друзі можна лише ЗБЛИЗЬКА -- це перевіряє сервер. У Зоні
    // знайомляться в очі, а не за списком онлайну; заразом це знімає питання
    // «звідки клієнт узяв чужий Steam64»: нізвідки, він його не бачить.
    ref array<string> Friends;
    // NPC у контактах -- псевдо-uid-и "npc:<id>". СВІДОМО окремий простір
    // від SteamID: NPC не буває другом, членом групи чи ціллю обміну.
    ref array<string> NpcContacts;
    ref array<string> FriendReq;

    // --- групові розмови ---
    //
    // Id груп, у яких гравець складається. Реєстр тут потрібен, бо id групи
    // НЕ обчислюється з її учасників -- вони міняються. Особисту розмову
    // шукати не треба: її id -- це два Steam64 по порядку.
    ref array<string> Chats;

    // --- фракція ---
    //
    // Запасний шлях: коли на сервері немає мода фракцій, приналежність ставить
    // адмін або квестовий мод -- сюди. Живий постачальник це поле перекриває,
    // і воно лишається просто останнім відомим значенням.
    //
    // Порожня -- одинак. Це не помилка й не «не задано», а найчастіший стан у
    // Зоні, і окремого слова для нього не треба.
    string Faction = "";

    override int LatestVersion()
    {
        return 2;
    }

    override bool Migrate(int from)
    {
        // 1 -> 2: слаги фракцій перейменовано (v2 -> v3 таблиці фракцій).
        //
        // Коментар тієї міграції обіцяв, що перейменування тягне за собою
        // «поле Faction у файлах гравців», і не робив цього: таблиця
        // переїжджала, а файли лишались із мертвим id. Гравець із
        // Faction: "loner" ставав безфракційним назавжди -- NameOf нічого не
        // знаходив, AreHostile нічого не вирішував, і жодного рядка в лог.
        //
        // Тут, а не одним проходом по всій теці: файли вантажаться ліниво, і
        // проходити по тих, хто не заходив рік, немає навіщо.
        if (from < 2)
        {
            if (Faction == "loner")
                Faction = "neutral";
            else if (Faction == "ecologist")
                Faction = "ecolog";
        }

        Version = LatestVersion();
        return true;
    }

    override void LoadDefaults()
    {
        Version      = LatestVersion();
        SteamId      = "";
        DiscordId    = "";
        FirstSeen    = "";
        LastSeen     = "";
        SessionEpoch = 1;

        TransponderMode = "off";
        TransponderTo   = new array<string>();
        PresenceHidden  = false;

        Name      = "";
        Friends   = new array<string>();
        NpcContacts = new array<string>();
        FriendReq = new array<string>();
        Chats     = new array<string>();
        Faction   = "";
    }

    override void Validate(out int warnings)
    {
        warnings = 0;

        // Масиви, яких немає у старому файлі, мусять з'явитись ТУТ, а не при
        // першому зверненні: інакше кожен, хто їх читає, зобов'язаний
        // перевіряти на null, і рано чи пізно хтось забуде.
        if (!TransponderTo)
            TransponderTo = new array<string>();
        if (!Friends)
            Friends = new array<string>();
        if (!FriendReq)
            FriendReq = new array<string>();
        if (!Chats)
            Chats = new array<string>();
        if (!NpcContacts)
            NpcContacts = new array<string>();

        // Файл, написаний до пермадесу, покоління не знає -- воно перше.
        if (Gen < 1)
            Gen = 1;

        // КОНТАКТИ СТАРОГО ЗРАЗКА -- голі Steam64. Дописуємо їм перше
        // покоління тут, один раз при читанні файла: інакше кожне місце, яке
        // порівнює ключі, мусило б знати обидва написання, і рано чи пізно
        // хтось порівняв би "76561..." з "76561...#1" і не знайшов друга.
        Generational(Friends);
        Generational(FriendReq);
        Generational(TransponderTo);
    }

    // Голий uid -> "uid#1". NPC ("npc:...") і вже позначені ключі не чіпаємо.
    private void Generational(array<string> list)
    {
        if (!list)
            return;

        for (int i = 0; i < list.Count(); i++)
        {
            if (list[i] == "")
                continue;
            if (list[i].IndexOf("#") != -1)
                continue;
            if (list[i].IndexOf("npc:") == 0)
                continue;

            list[i] = list[i] + "#1";
        }
    }
}

class OZ_PlayerStore
{
    private static ref map<string, ref OZ_PlayerData> s_Cache;
    private static ref array<string>                  s_Dirty;

    private static void Ensure()
    {
        if (!s_Cache)
            s_Cache = new map<string, ref OZ_PlayerData>();
        if (!s_Dirty)
            s_Dirty = new array<string>();
    }

    private static string PathOf(string uid)
    {
        return OZ_Const.PLAYERS_DIR + "\\" + uid + ".json";
    }

    static OZ_PlayerData Load(string uid)
    {
        Ensure();

        if (s_Cache.Contains(uid))
            return s_Cache.Get(uid);

        OZ_PlayerData d = new OZ_PlayerData();
        // backup=false: файлів гравців сотні, і копія кожного перед кожним
        // записом засмітила б Backup так, що знайти в ньому щось стало б
        // неможливо. Резервні копії -- для конфігів адміна.
        OZ_ConfigLoader<OZ_PlayerData>.Load(PathOf(uid), "player_" + uid, d, false);

        d.SteamId = uid;
        if (d.FirstSeen == "")
        {
            d.FirstSeen = OZ_Time.NowUtc();
            MarkDirty(uid);
        }

        s_Cache.Insert(uid, d);
        return d;
    }

    static void MarkDirty(string uid)
    {
        Ensure();
        if (s_Dirty.Find(uid) == -1)
            s_Dirty.Insert(uid);
    }

    static void Flush(string uid)
    {
        Ensure();

        if (!s_Cache.Contains(uid))
            return;

        OZ_ConfigLoader<OZ_PlayerData>.Save(PathOf(uid), "player_" + uid, s_Cache.Get(uid), false);

        int i = s_Dirty.Find(uid);
        if (i != -1)
            s_Dirty.Remove(i);
    }

    static void FlushAll()
    {
        Ensure();
        while (s_Dirty.Count() > 0)
            Flush(s_Dirty[0]);
    }

    // Вивантажує з пам'яті, дописавши на диск. Кличеться на дисконекті:
    // тримати в кеші того, хто пішов, немає сенсу.
    static void Unload(string uid)
    {
        Ensure();
        Flush(uid);
        s_Cache.Remove(uid);
    }

    // ------------------------------------------- ключ персонажа
    //
    // Один Steam-акаунт -- багато життів, і в записнику це РІЗНІ ЛЮДИ. Тому
    // контакти, запити й транспондер зберігають не Steam64, а КЛЮЧ
    // ПЕРСОНАЖА: "<uid>#<покоління>".
    //
    // Голий uid у старому файлі означає перше покоління -- до пермадесу
    // інших і не було.
    static string KeyOf(string uid)
    {
        OZ_PlayerData d = Load(uid);
        if (!d)
            return uid + "#1";
        return uid + "#" + d.Gen.ToString();
    }

    static string UidOfKey(string key)
    {
        int at = key.IndexOf("#");
        if (at == -1)
            return key;
        return key.Substring(0, at);
    }

    static int GenOfKey(string key)
    {
        int at = key.IndexOf("#");
        if (at == -1)
            return 1;
        return key.Substring(at + 1, key.Length() - at - 1).ToInt();
    }

    // Чи це ЖИВЕ покоління -- той самий персонаж, а не той, ким цей акаунт
    // був колись. Заморожений контакт не буває онлайн, не оновлює фракцію й
    // не отримує повідомлень.
    static bool IsLive(string key)
    {
        string uid = UidOfKey(key);
        if (uid == "")
            return false;

        OZ_PlayerData d = Load(uid);
        if (!d)
            return false;

        return GenOfKey(key) == d.Gen;
    }

    // Заморожений запис минулого покоління -- з диска, ОДИН раз.
    //
    // Читається рідко (лише коли в чиємусь записнику лишився старий
    // персонаж), тож кеш маленький і живе до кінця сеансу.
    private static ref map<string, ref OZ_PlayerData> s_Frozen;

    static OZ_PlayerData FrozenOf(string key)
    {
        if (!s_Frozen)
            s_Frozen = new map<string, ref OZ_PlayerData>();

        if (s_Frozen.Contains(key))
            return s_Frozen.Get(key);

        string uid = UidOfKey(key);
        string path = OZ_Const.PLAYERS_DIR + "\\" + uid + ".g" + GenOfKey(key).ToString() + ".json";

        OZ_PlayerData d;
        if (FileExist(path))
        {
            d = new OZ_PlayerData();
            OZ_ConfigLoader<OZ_PlayerData>.Load(path, "grave_" + key, d, false);
        }

        s_Frozen.Set(key, d);
        return d;
    }

    // Запис ЗА КЛЮЧЕМ: живий або заморожений. Може бути null -- покоління
    // є в чиємусь записнику, а файла вже немає (адмін прибрав руками).
    static OZ_PlayerData ByKey(string key)
    {
        if (IsLive(key))
            return Load(UidOfKey(key));
        return FrozenOf(key);
    }

    // ---------------------------------------------------- пермадес
    //
    // ЗАМОРОЗИТИ запис і почати наступний. Старе життя лишається на диску
    // цілим -- окремим файлом із номером покоління, -- а живий запис іде
    // далі вже як новий персонаж.
    //
    // Копія, а не перейменування: живий файл мусить лишатись на місці, бо
    // Steam64 не змінився й наступний Load піде саме за ним.
    static void Freeze(string uid)
    {
        Ensure();

        OZ_PlayerData d = Load(uid);
        if (!d)
            return;

        // На диск -- ПЕРЕД копіюванням: у пам'яті може бути свіжіше.
        MarkDirty(uid);
        Flush(uid);

        string grave = OZ_Const.PLAYERS_DIR + "\\" + uid + ".g" + d.Gen.ToString() + ".json";
        if (FileExist(PathOf(uid)))
        {
            if (!CopyFile(PathOf(uid), grave))
                OZ_Log.Warn("player " + uid + ": could not freeze generation " + d.Gen.ToString());
            else
                OZ_Log.Info("player " + uid + ": generation " + d.Gen.ToString() + " frozen as " + grave);
        }

        d.Gen = d.Gen + 1;
        MarkDirty(uid);
        Flush(uid);
    }

    static int CachedCount()
    {
        Ensure();
        return s_Cache.Count();
    }
}
