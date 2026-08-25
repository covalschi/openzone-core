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
        return 1;
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

    static int CachedCount()
    {
        Ensure();
        return s_Cache.Count();
    }
}
