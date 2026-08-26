// Куди з'являється гравець.
//
// Три незалежні речі під одним дахом, і плутати їх не можна:
//
//   1. ЗОНА РОЛІ -- постійна. «Борг з'являється біля Ростока». Живе в
//      Spawns.json, ключ -- той самий слаг, що й у фракції.
//   2. ОДНОРАЗОВА ТОЧКА -- від чужого мода. «Дефібрилятор підняв його там,
//      де він упав». Ставиться викликом, з'їдається НА НАЙБЛИЖЧОМУ спавні й
//      зникає. Саме одноразовість робить її придатною: мод, який лікує, не
//      мусить пам'ятати, коли прибрати за собою.
//   3. СТЕЙДЖИНҐ -- куди тих, у кого ролі немає ЗОВСІМ. Це не «запасна зона»:
//      запасна ловить того, чия фракція є, але зони їй не завели. Стейджинґ
//      ловить того, кого нікуди не взяли, і головне -- новачка, який ще не
//      прив'язався. Ворота прив'язки тримають його на місці, тож місце має
//      бути таким, де стояти не соромно.
//
// Порядок: одноразова -> зона ролі -> стейджинґ -> запасна -> те, що дав
// рушій. Нічого не налаштовано -- нічого й не змінюється: ванільна поведінка
// лишається недоторканою, і це умова того, щоб мод можна було просто
// поставити.
//
// ДЕ ЦЕ ЧІПЛЯЄТЬСЯ, і чому саме там. MissionServer.CreateCharacter -- єдиний
// шов, крізь який проходить створення НОВОГО персонажа: два виклики в усьому
// корпусі рушія (missionserver.c:560 і :576), обидва з OnClientNewEvent.
// Переприєднання туди НЕ заходить -- OnClientReconnectEvent отримує вже
// готового PlayerBase і CreateCharacter не кличе. Тому гравець, який просто
// повернувся, лишається там, де вийшов, і телепортувати його ми не можемо
// навіть помилково.

// Місце на карті: центр і розкид. Окремий тип, бо стейджинґ -- це МІСЦЕ, а
// не зона ролі, і поле Role у ньому не мало б жодного значення. Успадкування
// свідомо не беремо: два поля дешевше продублювати, ніж покладатись на те, як
// серіалізатор Enforce поводиться з базовим класом.
class OZ_SpawnPlace
{
    string Center;
    float  Radius;
}

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

    // Куди тих, у кого ролі немає. Порожній Center -- вимкнено, і тоді вони
    // йдуть тим самим шляхом, що й раніше.
    ref OZ_SpawnPlace Staging;

    override int LatestVersion()
    {
        return 2;
    }

    override void LoadDefaults()
    {
        Version = LatestVersion();

        // ПОРОЖНЬО навмисно. Координати належать карті, а карт багато, і
        // вигадана точка на Чернарусі означала б, що на Сахаліні всі
        // з'являються в морі. Порожній список -- це «нічого не чіпаємо».
        Zones   = new array<ref OZ_SpawnZone>();
        Staging = new OZ_SpawnPlace();
    }

    override bool Migrate(int from)
    {
        // 1 -> 2: з'явився стейджинґ. Порожнім, тобто вимкненим: файл, який
        // працював учора, мусить поводитись сьогодні так само.
        if (!Staging)
            Staging = new OZ_SpawnPlace();

        Version = LatestVersion();
        return true;
    }

    override void Validate(out int warnings)
    {
        warnings = 0;

        if (!Zones)
            Zones = new array<ref OZ_SpawnZone>();

        // Може не бути в файлі зовсім -- тоді серіалізатор лишає null, а не
        // порожній об'єкт.
        if (!Staging)
            Staging = new OZ_SpawnPlace();

        if (Staging.Radius < 0)
            Staging.Radius = 0;

        // Координату, яку рушій не розбере, ловимо ТУТ, а не на спавні:
        // помилку в файлі адмін мусить побачити при завантаженні, а не
        // дізнатись про неї з того, що новачки з'являються не там.
        if (Staging.Center != "")
        {
            if (Staging.Center.ToVector() == vector.Zero)
            {
                OZ_Log.Warn("staging spawn: Center is not a readable position - staging is off");
                Staging.Center = "";
                warnings++;
            }
        }

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
                    Told(uid, p, "one-shot");
                    return p;
                }
            }
        }

        // 2. Зона ролі або стейджинґ.
        string why;
        vector zoned = ZoneFor(uid, why);
        if (zoned != vector.Zero)
        {
            Told(uid, zoned, why);
            return zoned;
        }

        // 3. Рушій.
        Told(uid, fallback, "engine");
        return fallback;
    }

    // Один рядок на спавн, ЗАВЖДИ, і в ньому названа гілка.
    //
    // Без нього «гравець з'явився не там» -- це здогад: зона не збіглась,
    // фракція виявилась не тою, координата не розібралась, чи наш шов узагалі
    // не покликали. Чотири різні причини з однаковим виглядом, і рівно на
    // цьому згаяно один прогін стенду.
    private static void Told(string uid, vector where, string why)
    {
        string m = "spawn: " + uid;
        m += " -> " + where.ToString(false);
        m += " (" + why + ")";
        OZ_Log.Dbg(m);
    }

    // Ролі за старшинством: фракція, потім стаж, потім мітки, потім зона «для
    // всіх». Фракція перша тому, що вона -- єдина вісь, яка щось означає на
    // карті: стаж і мітки кажуть, ХТО ти, а не де твої.
    private static vector ZoneFor(string uid, out string why)
    {
        why = "";

        if (!s_Cfg)
            return vector.Zero;

        string faction = OZ_Factions.OfUid(uid);
        if (faction != "")
        {
            vector byFaction = PickIn(faction);
            if (byFaction != vector.Zero)
            {
                why = "zone " + faction;
                return byFaction;
            }
        }
        else
        {
            // Фракції немає -- ні з Discord, ні з файла акаунта. Це або
            // новачок, який ще не прив'язався, або той, кого нікуди не взяли.
            // Обом місце одне.
            //
            // Саме `else`, а не окремий крок: той, чия фракція Є, але зони їй
            // не завели, у стейджинґ потрапити не повинен. Його ловить
            // запасна зона нижче -- інакше повноправного борговця відносило б
            // до новачків через недогляд адміна в іншому рядку файла.
            vector staged = Staging();
            if (staged != vector.Zero)
            {
                why = "staging";
                return staged;
            }
        }

        // Зона без ролі -- спільна. Стоїть останньою, щоб не перебивати
        // жодну іменовану.
        vector common = PickIn("");
        if (common != vector.Zero)
            why = "fallback zone";
        return common;
    }

    private static vector Staging()
    {
        if (!s_Cfg.Staging)
            return vector.Zero;
        if (s_Cfg.Staging.Center == "")
            return vector.Zero;

        vector c = s_Cfg.Staging.Center.ToVector();
        if (c == vector.Zero)
            return vector.Zero;

        return Scatter(c, s_Cfg.Staging.Radius);
    }

    // Чи налаштований стейджинґ. Для звіту на буті: адмін мусить бачити з
    // лога, що його координата доїхала, а не з'ясовувати це вбивством себе.
    static bool HasStaging()
    {
        if (!s_Cfg)
            return false;
        if (!s_Cfg.Staging)
            return false;
        return s_Cfg.Staging.Center != "";
    }

    private static vector PickIn(string role)
    {
        if (!s_Cfg.Zones)
            return vector.Zero;

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

        // ЗАГАНЯЄМО В КАРТУ, і це не перестраховка.
        //
        // Розкид -- це коло, а коло біля краю виходить за нього. Радіус у
        // півтори тисячі метрів навколо узбережжя цілком розумний для
        // стейджинґу й дає точки в морі або поза межами світу, де SurfaceY
        // повертає що завгодно. Адмін бачить гравців, які падають крізь
        // ніщо, і жодної підказки, що винен радіус.
        float edge = GetGame().GetWorld().GetWorldSize();
        p[0] = Math.Clamp(p[0], 0, edge);
        p[2] = Math.Clamp(p[2], 0, edge);

        p[1] = GetGame().SurfaceY(p[0], p[2]);
        return p;
    }
}
