// Settings.json -- СЕРВЕРНА поверхня конфігу.
//
// Тут лежить секрет моста, і саме тому цей об'єкт ніколи не серіалізується на
// клієнт цілком. Те, що їде проводом, збирає окремий OZ_SyncPayload -- і це не
// стильова забаганка, а межа безпеки: один недбалий SendRPC із цим об'єктом
// роздав би секрет кожному, хто зайшов на сервер.

// ЧИ ДЗЕРКАЛИТИ ЦЕЙ РІД У DISCORD. Пара, а не мапа.
//
// Масив записів, а не map<string,bool>: поведінка map у JsonFileLoader у
// цьому проєкті не перевірена, а масив записів -- форма, яка тут уже працює
// (OZ_SpawnZone у цьому файлі, OZ_Faction -- у моді фракцій). Платити
// невивченим ризиком за коротший файл немає за що.
class OZ_KindMirror
{
    string Kind   = "";
    bool   Mirror = false;

    // Копія у створений СКРИПТОМ об'єкт. Навіщо -- у шапці OZ_ConfigBase:
    // елемент масиву виділяє серіалізатор, тобто `Mirror = false` вище на
    // ньому не виконувалось, і рядок без ключа "Mirror" дав би дзеркало,
    // увімкнене сирою пам'яттю.
    OZ_KindMirror Copy()
    {
        OZ_KindMirror c = new OZ_KindMirror();
        c.Kind   = Kind;
        c.Mirror = Mirror;
        return c;
    }
}

class OZ_BridgeSettings
{
    bool   Enabled        = false;
    string Url            = "";

    // Хто питає. Один міст обслуговує кілька стендів, і саме за цим рядком
    // він пам'ятає, докуди кожен із них дочитав.
    string ServerId       = "dayz";
    string Secret         = "";

    // PollTimeoutSec ТУТ БІЛЬШЕ НЕМАЄ. Це була настройка, яку рушій зміряно
    // ігнорує: асинхронний запит помирає на десятій секунді, хоч би що в ній
    // стояло. Настройка, що нічого не робить, гірша за її відсутність --
    // адмін крутить її й пояснює собі наслідки, яких немає. Справжня стеля
    // живе на мості (POLL_HOLD_SECONDS < OZ_Const.REST_TIMEOUT_SEC), і поле,
    // яке лишилось у чиємусь файлі, JsonFileLoader просто пропустить.

    // KINDS[] ТУТ БІЛЬШЕ НЕМАЄ (ТЗ-5 R-C1.4).
    //
    // Це був другий рубильник із майже тим самим ім'ям, що й Mirrors нижче, і
    // з іншим змістом: один вирішував, ЩО возити мостом, другий -- що з
    // возимого показувати в гільдії. Саме з такої пари й береться помилка,
    // якою можна непомітно вимкнути пермадес. Підписки тепер виводяться з
    // того, які роди оголосили самі моди (OZ_BridgeClient.Subscribe), і
    // третього джерела правди для них немає.
    //
    // Ключ, що лишився в живому файлі, серіалізатор просто пропускає -- про
    // нього один раз каже OZ_Settings.WarnLegacyKinds.

    // ДЕ ДАНІ ЖИВУТЬ І ДЕ ВОНИ ВИДНІ -- РІЗНІ ПИТАННЯ (ТЗ-2 §3).
    //
    // Mirrors -- про ПОВЕРХНЮ: чи показувати цей рід у гільдії. Дім роду не
    // змінює нічого: чат живе в боті хоч із дзеркалом, хоч без.
    //
    // «Discord опціональний» означає рівно «дзеркала вимкнені»: бот працює,
    // база працює, гра працює, у гільдії тихо. Раніше вимкнути Discord
    // означало лишитись без чату зовсім -- перемикач керував тим, ЩО
    // синхронізувати, а не тим, ДЕ дані лежать.
    ref array<ref OZ_KindMirror> Mirrors;

    // Копія у створений СКРИПТОМ об'єкт -- разом з усім, що всередині.
    // Шапка OZ_ConfigBase каже, навіщо; тут варто назвати ціну помилки:
    // Enabled і Secret -- члени цього класу, і файл із розділом "Bridge" без
    // ключа "Enabled" вмикав би міст сирою пам'яттю.
    //
    // Порожні елементи масиву дзеркал НЕ ВИКИДАЄМО: про них лає Validate, а
    // виправити їх ядро не береться. Тихо прибраний null усе одно повертався
    // б із файла кожного старту, а прибраний ІЗ ФАЙЛА забрав би з собою те,
    // що адмін там писав і зіпсував одним символом. Тому скарга лунає щоразу
    // й файла не чіпає (шапка Validate).
    OZ_BridgeSettings Copy()
    {
        OZ_BridgeSettings c = new OZ_BridgeSettings();
        c.Enabled  = Enabled;
        c.Url      = Url;
        c.ServerId = ServerId;
        c.Secret   = Secret;

        c.Mirrors = new array<ref OZ_KindMirror>();
        if (Mirrors)
        {
            for (int j = 0; j < Mirrors.Count(); j++)
            {
                if (Mirrors[j])
                    c.Mirrors.Insert(Mirrors[j].Copy());
                else
                    c.Mirrors.Insert(null);
            }
        }

        return c;
    }
}

// ФРАКЦІЙНИХ МЕЖ ТУТ БІЛЬШЕ НЕМАЄ (рішення власника 2026-09-04).
//
// Розділ "Faction" разом із класом меж поїхав у мод фракцій, у власний файл
// $profile:OpenZone\OZ_Factions_Settings.json. Причина та сама, що й в усього
// іншого виносу: у ядрі служби, а не гра, і сервер, якому потрібна сама лише
// рація, не мусить мати в налаштуваннях розділ про запрошення до угруповань.
//
// СТАРІ ФАЙЛИ ВІД ЦЬОГО НЕ ЛАМАЮТЬСЯ. Розділ, який лишився на диску,
// JsonFileLoader просто не має куди покласти і мовчки пропускає; дефолт і
// власне число файла мода фракцій -- єдині джерела правди відтепер.

class OZ_Settings : OZ_ConfigBase
{
    bool                  DebugMode = true;
    ref array<string>     AdminIds;
    string                VppPermission = "OpenZone:Admin";
    ref OZ_BridgeSettings Bridge;

    // Вимагати прив'язку Discord при вході. Поки не прив'язався -- вікно не
    // відпускає.
    //
    // Це рішення СЕРВЕРА. Ролі Discord дають фракцію, стаж і посади, тобто
    // все, що вирішує, хто кому ворог; непов'язаний гравець для цієї
    // машинерії просто не існує.
    bool RequireDiscordLink = true;

    // Що робити, коли МОСТА немає, а прив'язка вимагається.
    //
    // true (умовчання) -- пускати: код видає міст, і без нього прив'язатись
    // фізично неможливо, тож жорсткі ворота перетворили б збій бота на збій
    // СЕРВЕРА, і кожен гравець опинився б замкненим у вікні, з якого немає
    // виходу. false -- не пускати, для того, хто свідомо хоче саме цього.
    bool AllowPlayWhenBridgeDown = true;

    private static ref OZ_Settings s_Inst;

    // ЧИ МОЖНА ЦЕЙ ФАЙЛ ПИСАТИ ВЗАГАЛІ.
    //
    // false означає «на диску лежить єдиний примірник, якого лоадер не
    // зрозумів і не зміг винести в карантин, або файл із новішої схеми»: у
    // пам'яті тоді дефолти, і будь-який запис поверх стер би AdminIds, адресу
    // й секрет моста. Один тумблер дзеркала в панелі VPP робив рівно це --
    // і .bak при ньому теж не оновлюється (backup=false), тож відновлюватись
    // не було б звідки.
    //
    // OZ_Spawns тримає такий самий прапорець; тут він з'явився на файлі з
    // найширшим радіусом ураження останнім.
    private static bool s_Writable = true;

    static OZ_Settings Get()
    {
        return s_Inst;
    }

    static bool Writable()
    {
        return s_Writable;
    }

    override int LatestVersion()
    {
        return OZ_Const.SCHEMA_SETTINGS;
    }

    // Виставляє КОЖНЕ поле: кличеться і на порожньому об'єкті, і поверх
    // напівпрочитаного після невдалого розбору.
    override void LoadDefaults()
    {
        Version       = LatestVersion();
        DebugMode     = true;
        AdminIds      = new array<string>();
        VppPermission = "OpenZone:Admin";
        Bridge        = new OZ_BridgeSettings();

        RequireDiscordLink      = true;
        AllowPlayWhenBridgeDown = true;

        if (Bridge)
        {
            // ПОРОЖНЬО -- ЦЕ «ВСІ ДЗЕРКАЛА ВИМКНЕНІ», а не «всі ввімкнені»
            // (ТЗ-2 R3.2, правило 2). Тут стояла зворотна сумісність із
            // живими серверами; власник зняв її 2026-09-01 -- мод живе лише
            // на дев-стенді. Умовчання «тихо» правильніше й саме собою:
            // сервер, який ще нічого не налаштував, не має починати з того,
            // що виливає переписку гравців у гільдію.
            Bridge.Mirrors = new array<ref OZ_KindMirror>();
        }
    }

    // ВЛАСНОГО Migrate() ТУТ БІЛЬШЕ НЕМАЄ.
    //
    // Він робив рівно одне: заводив порожній Bridge.Mirrors для файлів
    // старших за v3. Те саме -- і без жодної умови на версію -- робить
    // Validate() нижче, а лоадер кличе його ОДРАЗУ після Migrate, завжди.
    // Крок міграції, який повторює наступний крок конвеєра, -- це друге
    // місце, де ту саму починку треба не забути.
    //
    // Історія версій лишається тут словами: v2 заводив розділ "Faction" (він
    // поїхав у мод фракцій), v3 -- дзеркала по родах, і порожній список у
    // ньому означає «всі вимкнені», що є свідомою зміною поведінки
    // (ТЗ-2 R3.2, зворотну сумісність власник зняв 2026-09-01).

    // Кожне зауваження -- окремий Warning. Завантаження НЕ валиться.
    //
    // WARNINGS -- ЦЕ «Я ЩОСЬ ПОЛАГОДИВ», А НЕ «Я ЩОСЬ ПОМІТИВ».
    //
    // Лоадер пише файл назад саме за цим числом, і сенс того запису -- покласти
    // на диск те, що Validate полагодив у пам'яті. Скарга, після якої об'єкт не
    // змінився, повторюється КОЖЕН бут, тобто переписувала OZ_Core_Settings.json
    // разом із бекапом щоразу, як сервер піднімався: на цьому стенді -- вічно,
    // бо міст ходить по http і рядок про «не https» стоїть завжди
    // (task-62-report §2, та сама починка, що й у КПК: ff3dccc).
    //
    // Тому нижче рахуються лише ті зауваження, після яких у пам'яті справді
    // інше значення: підставлений ServerId і вимкнений міст. Решта -- сказати
    // адміну й далі.
    override void Validate(out int warnings)
    {
        warnings = 0;

        if (!AdminIds)
            AdminIds = new array<string>();

        // ВКЛАДЕНЕ -- У СТВОРЕНЕ СКРИПТОМ, і саме тут: лоадер кличе Validate
        // одразу після розбору, поки читання ще чесне (шапка OZ_ConfigBase).
        if (!Bridge)
            Bridge = new OZ_BridgeSettings();
        else
            Bridge = Bridge.Copy();

        if (!Bridge.Mirrors)
            Bridge.Mirrors = new array<ref OZ_KindMirror>();

        // ДЗЕРКАЛА: чистимо запис, а не завантаження (ТЗ-2 R3.4).
        //
        // Ядро НЕ знає переліку родів -- їх оголошують моди, і сьогодні
        // встановлених модів може не бути жодного. Тому «незнайомий рід» тут
        // не перевіряється зовсім: єдине, про що ядро може судити само, --
        // порожнє ім'я й другий запис про той самий рід.
        //
        // Дублікат небезпечний саме мовчанням: два рядки про "chat" з різними
        // Mirror дають відповідь, яка залежить від порядку у файлі. Перший
        // виграє -- те саме правило, що й скрізь у цій серії (ТЗ-5 R1), -- і
        // про це кажуть уголос.
        for (int mi = 0; mi < Bridge.Mirrors.Count(); mi++)
        {
            // СКАРГА БЕЗ ПОЧИНКИ НЕ РАХУЄТЬСЯ (див. шапку Validate): порожній
            // запис лишається на місці навмисно -- тихо прибраний, він
            // повертався б із файла кожен старт, а прибраний із файла забирав
            // би з собою те, що адмін там писав.
            OZ_KindMirror m = Bridge.Mirrors[mi];
            if (!m || m.Kind == "")
            {
                OZ_Log.Warn("Bridge.Mirrors[" + mi.ToString() + "] has no Kind and is ignored");
                continue;
            }

            for (int mj = 0; mj < mi; mj++)
            {
                OZ_KindMirror earlier = Bridge.Mirrors[mj];
                if (earlier && earlier.Kind == m.Kind)
                {
                    string dup = "Bridge.Mirrors lists \"" + m.Kind;
                    dup += "\" twice - the first entry wins, the second is ignored";
                    OZ_Log.Warn(dup);
                    break;
                }
            }
        }

        // Так само: чужий id ми не виправляємо й не викидаємо -- лише кажемо.
        for (int i = 0; i < AdminIds.Count(); i++)
        {
            if (AdminIds[i].Length() != 17)
            {
                string bad = "AdminIds[" + i;
                bad += "] is not a 17-digit Steam64 id: " + AdminIds[i];
                OZ_Log.Warn(bad);
            }
        }

        // А оці дві -- ПОЧИНКИ: у пам'яті після них інше значення, і саме
        // заради них лоадер переписує файл.
        if (Bridge.Enabled && Bridge.ServerId == "")
        {
            OZ_Log.Warn("Bridge.ServerId is empty - falling back to \"dayz\"");
            Bridge.ServerId = "dayz";
            warnings++;
        }

        if (Bridge.Enabled && Bridge.Url == "")
        {
            OZ_Log.Warn("Bridge.Enabled is true but Bridge.Url is empty - bridge stays off");
            Bridge.Enabled = false;
            warnings++;
        }

        // DayZ не дає задати заголовки запиту: RestContext.SetHeader керує лише
        // Content-Type. Секрет тому їде в ТІЛІ, і відкритий http роздав би його
        // всім, хто дивиться канал.
        //
        // НЕ РАХУЄТЬСЯ: адресу за адміна ми не переписуємо -- це його рішення,
        // а на дев-стенді ще й свідоме. Саме цей рядок і переписував файл
        // кожен бут, бо він правдивий завжди.
        if (Bridge.Enabled && Bridge.Url.IndexOf("https://") != 0)
            OZ_Log.Warn("Bridge.Url is not https - the shared secret travels in the request body");

    }

    static void ServerLoad()
    {
        OZ_Json.EnsureTree();

        // ПРОБА -- ДО ЗАВАНТАЖЕННЯ, і саме тут її й забули з першого разу
        // (зміряно на стенді 2026-09-07). Load сам переписує файл, коли
        // Validate щось полагодив, -- а на цьому стенді він лагодить завжди
        // (Bridge.Url не https). Проба після Load читала вже переписаний
        // файл, знятого ключа в ньому не бачила й мовчала на кожному буті.
        bool legacyKinds = HasLegacyKinds();

        s_Inst = new OZ_Settings();
        s_Writable = OZ_ConfigLoader<OZ_Settings>.Load(OZ_Const.SETTINGS, OZ_Const.SETTINGS_TAG, s_Inst);

        OZ_Log.SetDebug(s_Inst.DebugMode);

        if (legacyKinds)
            SayKindsAreGone();
    }

    // ПРО ЗНЯТИЙ КЛЮЧ КАЖУТЬ УГОЛОС, А НЕ МОВЧКИ ПРОПУСКАЮТЬ.
    //
    // Bridge.Kinds пішов із класу (ТЗ-5 R-C1.4), і серіалізатор тепер просто
    // не має куди покласти цей ключ -- файл читається як раніше, тиша повна.
    // Але адмін, який колись вимкнув ним рід, побачив би лише те, що рід
    // раптом їде мостом, і причини в лозі не знайшов би. Тому читаємо файл
    // текстом РІВНО ЗАРАДИ ЦЬОГО ОДНОГО СЛОВА.
    //
    // Рядок файла в лог не потрапляє НІКОЛИ: тут лежить секрет моста, і
    // звідси виходить сама лише відповідь «так/ні».
    private static bool HasLegacyKinds()
    {
        if (!FileExist(OZ_Const.SETTINGS))
            return false;

        // `handle == 0` -- ванільна перевірка (jsonfileloader.c:114): тип
        // FileHandle -- int[], і `!f` на ньому не те, що тут потрібно.
        FileHandle f = OpenFile(OZ_Const.SETTINGS, FileMode.READ);
        if (f == 0)
            return false;

        bool seen = false;
        string line;
        while (FGets(f, line) >= 0)
        {
            if (line.IndexOf("\"Kinds\"") != -1)
            {
                seen = true;
                break;
            }
        }
        CloseFile(f);

        return seen;
    }

    private static void SayKindsAreGone()
    {
        string w = "Bridge.Kinds is no longer read and the key is ignored";
        w += " - subscriptions follow the kinds mods register; use Bridge.Mirrors to decide what the guild sees";
        OZ_Log.Warn(w);

        // ОДИН РАЗ, А НЕ ЩОБУТУ. Файл переписуємо тут-таки, і ключ із нього
        // зникає разом із рештою знятих полів; наступний старт мовчить.
        // Не переписуємо лише те, чого лоадер не зрозумів: у пам'яті тоді
        // дефолти, і запис коштував би адмінові адресу з секретом.
        if (s_Writable)
            OZ_ConfigLoader<OZ_Settings>.Save(OZ_Const.SETTINGS, OZ_Const.SETTINGS_TAG, s_Inst);
    }
}
