// Куди з'являється гравець.
//
// Чотири незалежні речі під одним дахом, і плутати їх не можна:
//
//   1. ЗОНА РОЛІ -- постійна. «Борг з'являється біля Ростока». Живе в
//      Spawns.json, ключ -- той самий слаг, що й у фракції.
//   2. ОДНОРАЗОВА ТОЧКА -- від чужого мода. «Дефібрилятор підняв його там,
//      де він упав». Ставиться викликом, з'їдається НА НАЙБЛИЖЧОМУ спавні й
//      зникає. Саме одноразовість робить її придатною: мод, який лікує, не
//      мусить пам'ятати, коли прибрати за собою.
//   3. ОСОБИСТА ТОЧКА -- постійна прив'язка ОДНОГО гравця до місця, поверх
//      його фракції та будь-чого. Вигнанець, відлюдник, персонаж квесту.
//      Ставить адмін; діє, доки адмін не зняв.
//   4. СТЕЙДЖИНҐ -- куди тих, у кого ролі немає ЗОВСІМ. Це не «запасна зона»:
//      запасна ловить того, чия фракція є, але зони їй не завели. Стейджинґ
//      ловить того, кого нікуди не взяли, і головне -- новачка, який ще не
//      прив'язався. Ворота прив'язки тримають його на місці, тож місце має
//      бути таким, де стояти не соромно.
//
// Порядок: одноразова -> особиста -> зона ролі -> стейджинґ -> запасна ->
// те, що дав рушій. Одноразова ЗАВЖДИ нагорі: вона каже про цю конкретну
// смерть, і жодне постійне правило не сміє її перебити. Нічого не
// налаштовано -- нічого й не змінюється: ванільна поведінка лишається
// недоторканою, і це умова того, щоб мод можна було просто поставити.
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

// Особистий спавн ОДНОГО гравця -- поверх фракцій і будь-чого. Постійний,
// на відміну від одноразової точки: живе у файлі, переживає рестарти й діє,
// доки адмін його не зніме. «Цей у вигнанні за периметром», «той живе на
// маяку» -- прив'язка до долі людини, а не до слага її фракції.
class OZ_SpawnPersonal
{
    string Uid;
    string Center;
    float  Radius;
}

class OZ_SpawnsConfig : OZ_ConfigBase
{
    ref array<ref OZ_SpawnZone> Zones;

    // Особисті точки: гравець -> місце. Дивляться ПЕРЕД зонами ролей.
    ref array<ref OZ_SpawnPersonal> Personal;

    // Куди тих, у кого ролі немає. Порожній Center -- вимкнено, і тоді вони
    // йдуть тим самим шляхом, що й раніше.
    ref OZ_SpawnPlace Staging;

    override int LatestVersion()
    {
        return 3;
    }

    override void LoadDefaults()
    {
        Version = LatestVersion();

        // ПОРОЖНЬО навмисно. Координати належать карті, а карт багато, і
        // вигадана точка на Чернарусі означала б, що на Сахаліні всі
        // з'являються в морі. Порожній список -- це «нічого не чіпаємо».
        Zones    = new array<ref OZ_SpawnZone>();
        Personal = new array<ref OZ_SpawnPersonal>();
        Staging  = new OZ_SpawnPlace();
    }

    override bool Migrate(int from)
    {
        // 1 -> 2: з'явився стейджинґ. Порожнім, тобто вимкненим: файл, який
        // працював учора, мусить поводитись сьогодні так само.
        if (!Staging)
            Staging = new OZ_SpawnPlace();

        // 2 -> 3: особисті точки. Теж порожніми з тієї ж причини.
        if (!Personal)
            Personal = new array<ref OZ_SpawnPersonal>();

        Version = LatestVersion();
        return true;
    }

    override void Validate(out int warnings)
    {
        warnings = 0;

        if (!Zones)
            Zones = new array<ref OZ_SpawnZone>();

        if (!Personal)
            Personal = new array<ref OZ_SpawnPersonal>();

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

        for (int k = 0; k < Personal.Count(); k++)
        {
            if (Personal[k].Radius < 0)
                Personal[k].Radius = 0;

            if (Personal[k].Uid != "" && Personal[k].Center != "")
                continue;

            string pw = "personal spawn #" + k.ToString();
            pw += " misses Uid or Center - it will never be used";
            OZ_Log.Warn(pw);
            warnings++;
        }
    }
}

class OZ_Spawns
{
    private static ref OZ_SpawnsConfig s_Cfg;

    // Одноразові точки від чужих модів: uid -> позиція рядком.
    private static ref map<string, string> s_Once;

    // Коли одноразова точка перестає бути правдою. Ключ той самий, що й у
    // s_Once, значення -- час рушія в мілісекундах.
    private static ref map<string, int> s_OnceUntil;

    // Скільки метрів розкиду ще має сенс, і скільки разів шукати сушу.
    private static const float MAX_RADIUS = 1000;
    private static const int   TRIES      = 10;

    // Скільки живе одноразова точка. П'ять хвилин: рівно стільки триває
    // ситуація, заради якої її ставлять.
    private static const int ONCE_TTL_MS = 300000;

    static void ServerLoad()
    {
        if (s_Cfg)
            return;
        Reload();
    }

    // Перечитати файл. Викликається на старті -- і ПЕРЕД КОЖНИМ ЗАПИСОМ.
    //
    // Save() пише ВЕСЬ документ із пам'яті, а пам'ять -- знімок останнього
    // читання, тобто зазвичай момент старту сервера. Адмін правив Spawns.json
    // руками при живому сервері (звична річ: файл для того й текстовий), потім
    // ставив одну зону з гри -- і всі правки зникали під знімком тритижневої
    // давнини. Повідомлення при цьому не було жодного: запис удався.
    static void Reload()
    {
        s_Cfg = new OZ_SpawnsConfig();
        OZ_ConfigLoader<OZ_SpawnsConfig>.Load(OZ_Const.PROFILE_DIR + "\\OZ_Core_Spawns.json", "spawns", s_Cfg);
    }

    static int Count()
    {
        if (!s_Cfg)
            return 0;
        if (!s_Cfg.Zones)
            return 0;
        return s_Cfg.Zones.Count();
    }

    // ------------------------------------------------------ для адміна

    // Зона ролі -- ТУТ, де я стою. Радіус у метрах.
    //
    // Координати набирали руками у файлі, і це найгірший спосіб задати місце
    // на карті: щоб дізнатись, куди ставити, треба спершу туди прийти й
    // подивитись координати -- а тоді вже переписати їх у файл без помилки.
    // Прийшов, став, відмітив.
    //
    // Порожня роль -- ЗАПАСНА зона (для всіх, у кого нічого не збіглося).
    // "*" -- стейджинґ. Не слаг фракції, бо стейджинґ і не роль: це відповідь
    // на відсутність ролі.
    static string SetZoneHere(string role, vector where, float radius)
    {
        if (!GetGame().IsServer())
            return "STR_OZ_ERR_INTERNAL";
        if (!s_Cfg)
            return "STR_OZ_ERR_INTERNAL";

        Reload();

        if (radius < 0)
            radius = 0;

        string pos = where.ToString(false);

        if (role == "*")
        {
            if (!s_Cfg.Staging)
                s_Cfg.Staging = new OZ_SpawnPlace();

            s_Cfg.Staging.Center = pos;
            s_Cfg.Staging.Radius = radius;
            Save();
            return "";
        }

        // Роль мусить існувати -- інакше зона нікому не дістанеться, а адмін
        // про це не дізнається до першої чужої смерті. Порожня ("запасна")
        // -- виняток: вона навмисно нічия.
        if (role != "" && !OZ_Factions.Find(role))
            return "STR_OZ_ERR_NO_FACTION";

        for (int i = 0; i < s_Cfg.Zones.Count(); i++)
        {
            if (s_Cfg.Zones[i].Role != role)
                continue;

            s_Cfg.Zones[i].Center = pos;
            s_Cfg.Zones[i].Radius = radius;
            Save();
            return "";
        }

        OZ_SpawnZone z = new OZ_SpawnZone();
        z.Role   = role;
        z.Center = pos;
        z.Radius = radius;
        s_Cfg.Zones.Insert(z);

        Save();
        return "";
    }

    // Прибрати зону. Стейджинґ вимикається порожнім центром, зона ролі --
    // зникає зі списку: порожній запис у файлі означав би «є, але зламана».
    static string ClearZone(string role)
    {
        if (!GetGame().IsServer())
            return "STR_OZ_ERR_INTERNAL";
        if (!s_Cfg)
            return "STR_OZ_ERR_INTERNAL";

        Reload();

        if (role == "*")
        {
            if (s_Cfg.Staging)
            {
                s_Cfg.Staging.Center = "";
                s_Cfg.Staging.Radius = 0;
            }
            Save();
            return "";
        }

        for (int i = 0; i < s_Cfg.Zones.Count(); i++)
        {
            if (s_Cfg.Zones[i].Role != role)
                continue;

            s_Cfg.Zones.Remove(i);
            Save();
            return "";
        }

        return "STR_OZ_ERR_NO_ZONE";
    }

    // Особиста точка ЦЬОГО гравця -- ТУТ, де стоїть адмін. Постійна.
    static string SetPersonalHere(string uid, vector where, float radius)
    {
        if (!GetGame().IsServer())
            return "STR_OZ_ERR_INTERNAL";
        if (!s_Cfg)
            return "STR_OZ_ERR_INTERNAL";
        if (uid == "")
            return "STR_OZ_ERR_NO_TARGET";

        Reload();

        if (radius < 0)
            radius = 0;

        string pos = where.ToString(false);

        for (int i = 0; i < s_Cfg.Personal.Count(); i++)
        {
            if (s_Cfg.Personal[i].Uid != uid)
                continue;

            s_Cfg.Personal[i].Center = pos;
            s_Cfg.Personal[i].Radius = radius;
            Save();
            return "";
        }

        OZ_SpawnPersonal p = new OZ_SpawnPersonal();
        p.Uid    = uid;
        p.Center = pos;
        p.Radius = radius;
        s_Cfg.Personal.Insert(p);

        Save();
        return "";
    }

    static string ClearPersonal(string uid)
    {
        if (!GetGame().IsServer())
            return "STR_OZ_ERR_INTERNAL";
        if (!s_Cfg)
            return "STR_OZ_ERR_INTERNAL";

        Reload();

        for (int i = 0; i < s_Cfg.Personal.Count(); i++)
        {
            if (s_Cfg.Personal[i].Uid != uid)
                continue;

            s_Cfg.Personal.Remove(i);
            Save();
            return "";
        }

        return "STR_OZ_ERR_NO_ZONE";
    }

    static bool HasPersonal(string uid)
    {
        if (!s_Cfg || !s_Cfg.Personal)
            return false;
        for (int i = 0; i < s_Cfg.Personal.Count(); i++)
        {
            if (s_Cfg.Personal[i].Uid == uid && s_Cfg.Personal[i].Center != "")
                return true;
        }
        return false;
    }

    private static void Save()
    {
        OZ_ConfigLoader<OZ_SpawnsConfig>.Save(OZ_Const.PROFILE_DIR + "\\OZ_Core_Spawns.json", "spawns", s_Cfg);
        OZ_Log.Info("spawns: written, " + Count().ToString() + " zone(s)");
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

        // СТРОК ПРИДАТНОСТІ.
        //
        // Точка «підняти там, де впав» правдива хвилини дві, поки медик
        // стоїть над тілом. Але зникала вона тільки коли спрацьовувала -- а
        // якщо гравець вирішив не відроджуватись і вийшов, вона лишалась у
        // пам'яті сервера НАЗАВЖДИ. Через тиждень він гине на іншому кінці
        // карти й прокидається там, де його колись намагався підняти медик,
        // і зрозуміти це неможливо ні йому, ні адмінові.
        if (!s_OnceUntil)
            s_OnceUntil = new map<string, int>();

        s_OnceUntil.Set(uid, GetGame().GetTime() + ONCE_TTL_MS);

        string m = "spawn: next spawn for " + uid;
        m += " set to " + pos.ToString(false);
        if (reason != "")
            m += " (" + reason + ")";
        OZ_Log.Dbg(m);
    }

    static void ClearNextSpawn(string uid)
    {
        if (s_OnceUntil && s_OnceUntil.Contains(uid))
            s_OnceUntil.Remove(uid);

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
        if (!s_Once.Contains(uid))
            return false;

        return !Expired(uid);
    }

    // Чи вийшов строк. Прострочену прибираємо ТУТ САМІ: питання «чи є точка»
    // й «чи вона ще правдива» -- одне питання, і два різних відповіді на нього
    // розійшлись би першої ж миті.
    private static bool Expired(string uid)
    {
        if (!s_OnceUntil)
            return false;

        int until;
        if (!s_OnceUntil.Find(uid, until))
            return false;

        if (GetGame().GetTime() < until)
            return false;

        s_OnceUntil.Remove(uid);
        if (s_Once && s_Once.Contains(uid))
            s_Once.Remove(uid);

        OZ_Log.Dbg("spawn: one-shot for " + uid + " expired unused");
        return true;
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
        if (s_Once && !Expired(uid))
        {
            string once;
            if (s_Once.Find(uid, once))
            {
                s_Once.Remove(uid);
                if (s_OnceUntil && s_OnceUntil.Contains(uid))
                    s_OnceUntil.Remove(uid);

                vector p = once.ToVector();
                if (p != vector.Zero)
                {
                    Told(uid, p, "one-shot");
                    return p;
                }
            }
        }

        // 2. Особиста точка. Нижче одноразової НАВМИСНО: дефібрилятор каже
        //    про ЦЮ смерть, а особиста -- про долю взагалі, і конкретне
        //    мусить перебивати загальне.
        vector personal = PersonalFor(uid);
        if (personal != vector.Zero)
        {
            Told(uid, personal, "personal");
            return personal;
        }

        // 3. Зона ролі або стейджинґ.
        string why;
        vector zoned = ZoneFor(uid, why);
        if (zoned != vector.Zero)
        {
            Told(uid, zoned, why);
            return zoned;
        }

        // 4. Рушій.
        Told(uid, fallback, "engine");
        return fallback;
    }

    private static vector PersonalFor(string uid)
    {
        if (!s_Cfg || !s_Cfg.Personal)
            return vector.Zero;

        for (int i = 0; i < s_Cfg.Personal.Count(); i++)
        {
            if (s_Cfg.Personal[i].Uid != uid)
                continue;
            if (s_Cfg.Personal[i].Center == "")
                continue;

            vector c = s_Cfg.Personal[i].Center.ToVector();
            if (c == vector.Zero)
                continue;

            return Scatter(c, s_Cfg.Personal[i].Radius);
        }
        return vector.Zero;
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
        // ВЕРХНЯ МЕЖА РАДІУСА.
        //
        // Знизу нуль стерегли з першого дня, згори -- ніщо. Помилка набору
        // («2000» замість «200») перетворювала зону фракції на пів карти, і
        // гравці одного табору прокидались за десять кілометрів один від
        // одного. Це виглядає як зламаний спавн, а не як зайвий нуль.
        if (radius > MAX_RADIUS)
        {
            OZ_Log.Warn("spawns: radius " + radius.ToString() + " capped at " + MAX_RADIUS.ToString());
            radius = MAX_RADIUS;
        }

        vector p = center;

        // Пробуємо кілька разів і беремо перше НЕ У ВОДІ.
        //
        // Коло розкиду не знає, що під ним: зона біля берега справно кидала
        // людей у море -- заплив, обморожений, без речей, за кілометр від
        // своїх. SurfaceIsSea коштує дешево, а спроб треба небагато: якщо
        // десять поспіль дали воду, то зону поставили в воду, і чесніше
        // віддати центр, ніж шукати далі.
        for (int attempt = 0; attempt < TRIES; attempt++)
        {
            p = center;

            if (radius > 0)
            {
                float a = Math.RandomFloat(0, Math.PI2);
                float r = Math.RandomFloat(0, radius);
                p[0] = center[0] + Math.Cos(a) * r;
                p[2] = center[2] + Math.Sin(a) * r;
            }

            p = InWorld(p);

            if (!GetGame().SurfaceIsSea(p[0], p[2]) && !GetGame().SurfaceIsPond(p[0], p[2]))
                return p;

            if (radius <= 0)
                break;
        }

        return p;
    }

    private static vector InWorld(vector p)
    {
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
