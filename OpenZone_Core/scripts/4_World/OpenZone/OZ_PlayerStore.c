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
