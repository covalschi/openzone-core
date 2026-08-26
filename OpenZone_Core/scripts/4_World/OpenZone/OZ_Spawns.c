// Куди з'являється гравець.
//
// Дві незалежні речі під одним дахом, і плутати їх не можна:
//
//   1. ЗОНА РОЛІ -- постійна. «Борг з'являється біля Ростока». Живе в
//      Spawns.json, ключ -- той самий слаг, що й у фракції.
//   2. ОДНОРАЗОВА ТОЧКА -- від чужого мода. «Дефібрилятор підняв його там,
//      де він упав». Ставиться викликом, з'їдається НА НАЙБЛИЖЧОМУ спавні й
//      зникає. Саме одноразовість робить її придатною: мод, який лікує, не
//      мусить пам'ятати, коли прибрати за собою.
//
// Порядок: одноразова -> зона ролі -> те, що дав рушій. Нічого не налаштовано
// -- нічого й не змінюється: ванільна поведінка лишається недоторканою, і це
// умова того, щоб мод можна було просто поставити.
//
// ДЕ ЦЕ ЧІПЛЯЄТЬСЯ, і чому саме там. MissionServer.CreateCharacter -- єдиний
// шов, крізь який проходить створення НОВОГО персонажа: два виклики в усьому
// корпусі рушія (missionserver.c:560 і :576), обидва з OnClientNewEvent.
// Переприєднання туди НЕ заходить -- OnClientReconnectEvent отримує вже
// готового PlayerBase і CreateCharacter не кличе. Тому гравець, який просто
// повернувся, лишається там, де вийшов, і телепортувати його ми не можемо
// навіть помилково.

class OZ_SpawnZone
{
    // Слаг ролі: фракція, стаж або мітка. Порожній -- зона за замовчуванням
    // для всіх, у кого нічого не збіглося.
    string Role;

    // Центр і радіус у метрах. Точка береться випадково в колі: десять
    // сталкерів, що з'явились в одній координаті, -- це купа тіл, а не табір.
    string Center;
    float  Radius;
}

class OZ_SpawnsConfig : OZ_ConfigBase
{
    ref array<ref OZ_SpawnZone> Zones;

    override int LatestVersion()
    {
        return 1;
    }

    override void LoadDefaults()
    {
        Version = LatestVersion();

        // ПОРОЖНЬО навмисно. Координати належать карті, а карт багато, і
        // вигадана точка на Чернарусі означала б, що на Сахаліні всі
        // з'являються в морі. Порожній список -- це «нічого не чіпаємо».
        Zones = new array<ref OZ_SpawnZone>();
    }

    override bool Migrate(int from)
    {
        Version = LatestVersion();
        return true;
    }

    override void Validate(out int warnings)
    {
        warnings = 0;

        if (!Zones)
            Zones = new array<ref OZ_SpawnZone>();

        for (int i = 0; i < Zones.Count(); i++)
        {
            if (Zones[i].Radius < 0)
                Zones[i].Radius = 0;

            if (Zones[i].Center != "")
                continue;

            string w = "spawn zone #" + i.ToString();
            w += " has no Center - it will never be used";
            OZ_Log.Warn(w);
            warnings++;
        }
    }
}

class OZ_Spawns
{
    private static ref OZ_SpawnsConfig s_Cfg;

    // Одноразові точки від чужих модів: uid -> позиція рядком.
    private static ref map<string, string> s_Once;

    static void ServerLoad()
    {
        if (s_Cfg)
            return;
        Reload();
    }

    static void Reload()
    {
        s_Cfg = new OZ_SpawnsConfig();
        OZ_ConfigLoader<OZ_SpawnsConfig>.Load(OZ_Const.PROFILE_DIR + "\\Spawns.json", "spawns", s_Cfg);
    }

    static int Count()
    {
        if (!s_Cfg)
            return 0;
        if (!s_Cfg.Zones)
            return 0;
        return s_Cfg.Zones.Count();
    }

    // ------------------------------------------------- для чужих модів

    // Наступний спавн цього гравця -- ТУТ. Одноразово.
    //
    // Для дефібрилятора, медика, квесту: «підняти там, де впав», «у табору».
    // Мод не мусить пам'ятати, коли скасувати -- точка зникає, щойно
    // спрацювала. Другий виклик до спавну просто замінює першу.
    //
    // Порожня позиція СКАСОВУЄ раніше поставлену: так у мода є чим передумати.
    static void SetNextSpawn(string uid, vector pos, string reason)
    {
        if (!GetGame().IsServer())
            return;
        if (uid == "")
            return;

        if (!s_Once)
            s_Once = new map<string, string>();

        if (pos == vector.Zero)
        {
            ClearNextSpawn(uid);
            return;
        }

        s_Once.Set(uid, pos.ToString(false));

        string m = "spawn: next spawn for " + uid;
        m += " set to " + pos.ToString(false);
        if (reason != "")
            m += " (" + reason + ")";
        OZ_Log.Dbg(m);
    }

    static void ClearNextSpawn(string uid)
    {
        if (!s_Once)
            return;
        if (!s_Once.Contains(uid))
            return;
        s_Once.Remove(uid);
    }

    static bool HasNextSpawn(string uid)
    {
        if (!s_Once)
            return false;
        return s_Once.Contains(uid);
    }

    // ------------------------------------------------------- розв'язання

    // Куди насправді класти. `fallback` -- те, що дав рушій; повертаємо його
    // без змін, якщо сказати нам нічого.
    static vector Resolve(PlayerIdentity who, vector fallback)
    {
        if (!who)
            return fallback;

        string uid = who.GetPlainId();

        // 1. Одноразова -- З'ЇДАЄТЬСЯ. Знімаємо ДО перевірок нижче: точка,
        //    яка не спрацювала через криву координату, все одно мусить
        //    зникнути, інакше вона чекала б наступної смерті.
        if (s_Once)
        {
            string once;
            if (s_Once.Find(uid, once))
            {
                s_Once.Remove(uid);

                vector p = once.ToVector();
                if (p != vector.Zero)
                {
                    OZ_Log.Dbg("spawn: used the one-shot point for " + uid);
                    return p;
                }
            }
        }

        // 2. Зона ролі.
        vector zoned = ZoneFor(uid);
        if (zoned != vector.Zero)
            return zoned;

        // 3. Рушій.
        return fallback;
    }

    // Ролі за старшинством: фракція, потім стаж, потім мітки, потім зона «для
    // всіх». Фракція перша тому, що вона -- єдина вісь, яка щось означає на
    // карті: стаж і мітки кажуть, ХТО ти, а не де твої.
    private static vector ZoneFor(string uid)
    {
        if (!s_Cfg)
            return vector.Zero;
        if (!s_Cfg.Zones)
            return vector.Zero;

        string faction = OZ_Factions.OfUid(uid);
        if (faction != "")
        {
            vector byFaction = PickIn(faction);
            if (byFaction != vector.Zero)
                return byFaction;
        }

        // Зона без ролі -- спільна. Стоїть останньою, щоб не перебивати
        // жодну іменовану.
        return PickIn("");
    }

    private static vector PickIn(string role)
    {
        for (int i = 0; i < s_Cfg.Zones.Count(); i++)
        {
            if (s_Cfg.Zones[i].Role != role)
                continue;
            if (s_Cfg.Zones[i].Center == "")
                continue;

            vector c = s_Cfg.Zones[i].Center.ToVector();
            if (c == vector.Zero)
                continue;

            return Scatter(c, s_Cfg.Zones[i].Radius);
        }
        return vector.Zero;
    }

    // Випадкова точка в колі. Десять сталкерів в одній координаті -- це купа
    // тіл, а не табір.
    //
    // Висоту бере рельєф: координата з файла описує МІСЦЕ, і змушувати адміна
    // вгадувати Y означало б, що будь-яка правка карти ховає людей під землю.
    private static vector Scatter(vector center, float radius)
    {
        vector p = center;

        if (radius > 0)
        {
            float a = Math.RandomFloat(0, Math.PI2);
            float r = Math.RandomFloat(0, radius);
            p[0] = center[0] + Math.Cos(a) * r;
            p[2] = center[2] + Math.Sin(a) * r;
        }

        p[1] = GetGame().SurfaceY(p[0], p[2]);
        return p;
    }
}
