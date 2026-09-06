// Куди з'являється гравець.
//
// Чотири незалежні речі під одним дахом, і плутати їх не можна:
//
//   1. ЗОНА РОЛІ -- постійна. «Борг з'являється біля Ростока». Живе в
//      Spawns.json, ключ -- той самий слаг, що й у фракції.
//   2. ОСОБИСТА ТОЧКА -- постійна прив'язка ОДНОГО гравця до місця, поверх
//      його фракції та будь-чого. Вигнанець, відлюдник, персонаж квесту.
//      Ставить адмін; діє, доки адмін не зняв.
//   3. СТЕЙДЖИНҐ -- куди тих, у кого ролі немає ЗОВСІМ. Це не «запасна зона»:
//      запасна ловить того, чия фракція є, але зони їй не завели. Стейджинґ
//      ловить того, кого нікуди не взяли, і головне -- новачка, який ще не
//      прив'язався. Ворота прив'язки тримають його на місці, тож місце має
//      бути таким, де стояти не соромно.
//
// Порядок: особиста -> зона ролі -> стейджинґ -> запасна -> те, що дав
// рушій. Конкретне перебиває загальне. Нічого не налаштовано -- нічого й не
// змінюється: ванільна поведінка лишається недоторканою, і це умова того,
// щоб мод можна було просто поставити.
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

// Особистий спавн ОДНОГО гравця -- поверх фракцій і будь-чого. Живе у файлі,
// переживає рестарти й діє, доки адмін його не зніме. «Цей у вигнанні за периметром», «той живе на
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

    // ВЛАСНОГО Migrate() ТУТ БІЛЬШЕ НЕМАЄ: обидва його кроки (порожній
    // Staging для файлів v1, порожній Personal для v2) робить Validate()
    // нижче -- без умови на версію й на кожному завантаженні, бо лоадер
    // кличе його ОДРАЗУ після Migrate. Історія лишається словами: обидва
    // поля заводяться порожніми, тобто вимкненими, щоб файл, який працював
    // учора, поводився сьогодні так само.

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

        // РОЗБІРНІСТЬ КООРДИНАТИ ПЕРЕВІРЯЄМО В УСІХ ТРЬОХ МІСЦЯХ.
        //
        // Тут перевірявся лише стейджинґ, хоч комусь із трьох ця перевірка
        // потрібна найменше: стейджинґ адмін ставить одного разу й одразу
        // бачить. Зони й особисті точки, навпаки, живуть роками, правляться
        // руками в файлі, і нерозбірна координата в них дає рівно те, чого
        // комент вище обіцяв не допустити: "0 0 0", тобто ріг карти, і жодної
        // підказки, чому людина з'явилась у морі.
        for (int i = 0; i < Zones.Count(); i++)
        {
            if (Zones[i].Radius < 0)
                Zones[i].Radius = 0;

            if (Zones[i].Center == "")
            {
                string w = "spawn zone #" + i.ToString();
                w += " has no Center - it will never be used";
                OZ_Log.Warn(w);
                warnings++;
                continue;
            }

            if (Zones[i].Center.ToVector() == vector.Zero)
            {
                string wv = "spawn zone \"" + Zones[i].Role;
                wv += "\": Center is not a readable position - the zone is dropped";
                OZ_Log.Warn(wv);
                Zones[i].Center = "";
                warnings++;
            }
        }

        for (int k = 0; k < Personal.Count(); k++)
        {
            if (Personal[k].Radius < 0)
                Personal[k].Radius = 0;

            if (Personal[k].Uid == "" || Personal[k].Center == "")
            {
                string pw = "personal spawn #" + k.ToString();
                pw += " misses Uid or Center - it will never be used";
                OZ_Log.Warn(pw);
                warnings++;
                continue;
            }

            if (Personal[k].Center.ToVector() == vector.Zero)
            {
                string pv = "personal spawn for " + Personal[k].Uid;
                pv += ": Center is not a readable position - the point is dropped";
                OZ_Log.Warn(pv);
                Personal[k].Center = "";
                warnings++;
            }
        }
    }
}

class OZ_Spawns
{
    private static ref OZ_SpawnsConfig s_Cfg;

    // Чи можна взагалі писати цей файл. false означає «на диску лежить
    // єдиний примірник, якого лоадер не зрозумів і не зміг винести в
    // карантин»: у пам'яті тоді дефолти, і Save() поверх них стер би зони
    // адміна назавжди.
    private static bool s_Writable = true;

    // Скільки метрів розкиду ще має сенс, і скільки разів шукати сушу.
    private static const float MAX_RADIUS = 1000;
    private static const int   TRIES      = 10;

    // ОДНОРАЗОВОЇ ТОЧКИ ТУТ БІЛЬШЕ НЕМАЄ (2026-09-06).
    //
    // Механізм жив на випадок чужого мода -- дефібрилятора, медика, квесту:
    // «підняти там, де впав», разом із рішенням про набір. Продюсера в нього
    // не з'явилось за весь час існування серії: grep по всіх шести
    // репозиторіях знаходив рівно один зовнішній виклик -- ClearNextSpawn на
    // вайпі у фракціях, тобто скасування точки, якої ніхто не ставив.
    // Півтори сотні рядків підтримували гілку, у яку не заходили ніколи.
    //
    // Повернути його дешево: порядок «одноразова -> особиста -> зона ролі»
    // описаний у шапці цього файла, а OZ_LoadoutService.Preset -- частина
    // контракту, яку мод фракцій уже реалізує, -- лишилась на місці.

    static void ServerLoad()
    {
        if (s_Cfg)
            return;
        Reload(false);
    }

    // Перечитати файл. Викликається на старті -- і ПЕРЕД КОЖНИМ ЗАПИСОМ.
    //
    // Save() пише ВЕСЬ документ із пам'яті, а пам'ять -- знімок останнього
    // читання, тобто зазвичай момент старту сервера. Адмін правив Spawns.json
    // руками при живому сервері (звична річ: файл для того й текстовий), потім
    // ставив одну зону з гри -- і всі правки зникали під знімком тритижневої
    // давнини. Повідомлення при цьому не було жодного: запис удався.
    //
    // quiet=true -- для читання ПЕРЕД власним записом: без бекапа й без
    // перезапису, спричиненого самим лише Validate. Файл із будь-яким
    // постійним зауваженням (порожній Center, немає Uid) інакше писався й
    // бекапився на кожному такому читанні -- тобто .bak затирався ЩЕ ДО
    // правки, заради якої адмін його й відкрив.
    static void Reload(bool quiet = false)
    {
        s_Cfg = new OZ_SpawnsConfig();
        s_Writable = OZ_ConfigLoader<OZ_SpawnsConfig>.Load(OZ_Const.PROFILE_DIR + "\\OZ_Core_Spawns.json", "spawns", s_Cfg, !quiet, !quiet);
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

        Reload(true);

        // Файл, якого лоадер не зрозумів, не перезаписуємо своїм знімком:
        // у пам'яті зараз дефолти, і запис стер би зони адміна назавжди.
        if (!s_Writable)
            return "STR_OZ_ERR_INTERNAL";

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
        //
        // ПЕРЕВІРЯЄМО, ЛИШЕ КОЛИ Є КОМУ. Без мода фракцій ядро не знає жодного
        // id і відхилило б будь-яку зону -- тобто зробило б спавни
        // непридатними на сервері, який фракцій і не просив.
        if (role != "" && OZ_Identity.Present())
        {
            array<string> known;
            OZ_Identity.Get().FactionIds(known);
            if (known.Find(role) == -1)
                return "STR_OZ_ERR_NO_FACTION";
        }

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

        Reload(true);

        // Файл, якого лоадер не зрозумів, не перезаписуємо своїм знімком:
        // у пам'яті зараз дефолти, і запис стер би зони адміна назавжди.
        if (!s_Writable)
            return "STR_OZ_ERR_INTERNAL";

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

        Reload(true);

        // Файл, якого лоадер не зрозумів, не перезаписуємо своїм знімком:
        // у пам'яті зараз дефолти, і запис стер би зони адміна назавжди.
        if (!s_Writable)
            return "STR_OZ_ERR_INTERNAL";

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

        Reload(true);

        // Файл, якого лоадер не зрозумів, не перезаписуємо своїм знімком:
        // у пам'яті зараз дефолти, і запис стер би зони адміна назавжди.
        if (!s_Writable)
            return "STR_OZ_ERR_INTERNAL";

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

    private static void Save()
    {
        if (!s_Writable)
        {
            OZ_Log.Error("spawns: the file could not be read and could not be quarantined - refusing to overwrite it");
            return;
        }

        OZ_ConfigLoader<OZ_SpawnsConfig>.Save(OZ_Const.PROFILE_DIR + "\\OZ_Core_Spawns.json", "spawns", s_Cfg);
        OZ_Log.Info("spawns: written, " + Count().ToString() + " zone(s)");
    }

    // ЗОВНІШНЬОГО API ТУТ БІЛЬШЕ НЕМАЄ. SetNextSpawn/ClearNextSpawn/
    // HasNextSpawn/TakeOnceLoadout возили одноразову точку чужого мода, і
    // жоден мод серії її не ставив; див. шапку класу.
    //
    // ClearNextSpawn лишається порожньою заглушкою рівно доти, доки
    // openzone-factions не прибере свій виклик на вайпі (OZF_AdminOps.c):
    // імені, якого немає, Enforce не пробачає навіть у мертвій гілці.
    static void ClearNextSpawn(string uid)
    {
    }

    // ------------------------------------------------------- розв'язання

    // Куди насправді класти. `fallback` -- те, що дав рушій; повертаємо його
    // без змін, якщо сказати нам нічого.
    static vector Resolve(PlayerIdentity who, vector fallback)
    {
        if (!who)
            return fallback;

        string uid = who.GetPlainId();

        // 1. Особиста точка -- найконкретніше, що ми знаємо про цю людину.
        vector personal = PersonalFor(uid);
        if (personal != vector.Zero)
        {
            Told(uid, personal, "personal");
            return personal;
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

    // Ролі за старшинством: належність, потім стаж, потім мітки, потім зона
    // «для всіх». Належність перша тому, що вона -- єдина вісь, яка щось
    // означає на карті: стаж і мітки кажуть, ХТО ти, а не де твої.
    //
    // ДВІ ОСІ, І ПОРЯДОК МІЖ НИМИ ВАЖИТЬ (ТЗ-1 §5). Спершу УГРУПОВАННЯ: у
    // борговця є база Боргу, і саме туди він має виходити. Якщо угруповання
    // немає -- БАЗОВА: одинак-сталкер теж людина з місцем на карті, і без
    // цього кроку весь сервер, крім членів ГП, ходив би через стейджинґ.
    private static vector ZoneFor(string uid, out string why)
    {
        why = "";

        if (!s_Cfg)
            return vector.Zero;

        string faction = OZ_Identity.Get().OrgOf(uid);
        if (faction == "")
            faction = OZ_Identity.Get().BaseOf(uid);

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
            // Немає ЖОДНОЇ з двох осей -- ні угруповання, ні базової, ні з
            // Discord, ні з файла акаунта. Після ТЗ-1 §7 це означає рівно
            // одне: гравець не заходив жодного разу, бо перший вхід базову
            // призначає. Місце такому одне.
            //
            // Саме `else`, а не окремий крок: той, чия належність Є, але зони
            // їй не завели, у стейджинґ потрапити не повинен. Його ловить
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

        // ЦЕНТР, а не остання спроба -- рівно те, що обіцяє коментар вище.
        //
        // Тут поверталось `p`, тобто ОСТАННЯ випадкова точка, яка щойно
        // виявилась водою. Тобто після десяти невдач мод робив саме те, від
        // чого захищався весь цикл: кидав людину в море. Центр теж може бути
        // водою -- зону поставили у воду, це вибір адміна, -- але тоді всі
        // потраплять в одне й те саме місце, і причину буде видно з першого
        // погляду замість того, щоб гадати над розкидом.
        return InWorld(center);
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
