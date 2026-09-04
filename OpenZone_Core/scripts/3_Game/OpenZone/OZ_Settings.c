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
// (OZ_SpawnZone, OZ_Faction). Платити невивченим ризиком за коротший файл
// немає за що.
class OZ_KindMirror
{
    string Kind   = "";
    bool   Mirror = false;
}

class OZ_BridgeSettings
{
    bool   Enabled        = false;
    string Url            = "";

    // Хто питає. Один міст обслуговує кілька стендів, і саме за цим рядком
    // він пам'ятає, докуди кожен із них дочитав.
    string ServerId       = "dayz";
    string Secret         = "";
    // Скільки гра просить чекати на відповідь довгого опиту. ПРОСИТЬ --
    // рушій асинхронні запити однаково обриває на десятій секунді, хоч би
    // що тут стояло (зміряно; див. OZ_BridgeClient.Start). Справжня стеля
    // задається на мості: POLL_HOLD_SECONDS має бути менший за 10.
    int    PollTimeoutSec = 30;

    // ЩО САМЕ синхронізувати з Discord, по родах. Порожній список означає
    // «все, що моди попросять» -- саме так поводився міст до появи цього
    // поля, і мовчки міняти поведінку наявних серверів не можна.
    //
    // НАВІЩО ПО РОДАХ, А НЕ ОДНИМ ПЕРЕМИКАЧЕМ. Enabled -- це «є міст чи
    // немає», і він або забирає інтеграцію цілком, або дає всю. Адмінові ж
    // потрібне середнє: чат у Discord хочу, а фракціями керую в грі; або
    // навпаки -- фракції з бота, а чат хай лишається ігровим. Рішення
    // власника 2026-08-31.
    //
    // Роди оголошують САМІ МОДИ, а не ядро: воно не знає й не мусить знати,
    // що чат зветься "chat", а ролі -- "roles". Тому перевірка -- по рядку,
    // а список у файлі веде адмін.
    ref array<string> Kinds;

    // ДЕ ДАНІ ЖИВУТЬ І ДЕ ВОНИ ВИДНІ -- РІЗНІ ПИТАННЯ (ТЗ-2 §3).
    //
    // Kinds вище -- про підписки: що взагалі возити мостом. Mirrors -- про
    // ПОВЕРХНЮ: чи показувати цей рід у гільдії. Дім роду не змінює ні те,
    // ні інше: чат живе в боті хоч із дзеркалом, хоч без.
    //
    // «Discord опціональний» означає рівно «дзеркала вимкнені»: бот працює,
    // база працює, гра працює, у гільдії тихо. Раніше вимкнути Discord
    // означало лишитись без чату зовсім -- перемикач керував тим, ЩО
    // синхронізувати, а не тим, ДЕ дані лежать.
    ref array<ref OZ_KindMirror> Mirrors;
}

// ФРАКЦІЙНИХ МЕЖ ТУТ БІЛЬШЕ НЕМАЄ (рішення власника 2026-09-04).
//
// Розділ "Faction" разом із класом меж поїхав у мод фракцій, у власний файл
// $profile:OpenZone\OZ_Factions_Settings.json. Причина та сама, що й в усього
// іншого виносу: у ядрі служби, а не гра, і сервер, якому потрібна сама лише
// рація, не мусить мати в налаштуваннях розділ про запрошення до угруповань.
//
// СТАРІ ФАЙЛИ ВІД ЦЬОГО НЕ ЛАМАЮТЬСЯ. Розділ, який лишився на диску,
// JsonFileLoader просто не має куди покласти і мовчки пропускає; мод фракцій
// на першому старті читає з нього своє число сам, тож адмін не втрачає межу,
// яку колись виставив (див. OZF_Settings.Inherit).

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

    static OZ_Settings Get()
    {
        return s_Inst;
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

        // Порожньо -- «все, що попросять». Див. OZ_BridgeSettings.Kinds.
        if (Bridge)
        {
            Bridge.Kinds = new array<string>();

            // ПОРОЖНЬО -- ЦЕ «ВСІ ДЗЕРКАЛА ВИМКНЕНІ», а не «всі ввімкнені»
            // (ТЗ-2 R3.2, правило 2). Тут стояла зворотна сумісність із
            // живими серверами; власник зняв її 2026-09-01 -- мод живе лише
            // на дев-стенді. Умовчання «тихо» правильніше й саме собою:
            // сервер, який ще нічого не налаштував, не має починати з того,
            // що виливає переписку гравців у гільдію.
            Bridge.Mirrors = new array<ref OZ_KindMirror>();
        }
    }

    override bool Migrate(int from)
    {
        // Ланцюжок покрокових мiграцiй: кожен крок пiднiмає на одну версiю.
        //
        // КРОКУ v2 ТУТ БІЛЬШЕ НЕМАЄ: він заводив розділ "Faction", а той поїхав
        // у мод фракцій. Номер не перевикористовується й крок не зсувається --
        // версії у файлах адмінів означають те саме, що й означали, а зайвий
        // розділ на диску нікому не заважає.

        // v3: дзеркала. Порожнiй список -- усi вимкненi, i це НЕ збереження
        // старої поведiнки, а свiдома її змiна (ТЗ-2 R3.2): ранiше вiдсутнiсть
        // списку означала «дзеркалити все». Власник зняв зворотну сумiснiсть
        // 2026-09-01. Адмiн, який хоче гiльдiю, вмикає роди поiменно.
        if (from < 3)
        {
            if (Bridge && !Bridge.Mirrors)
                Bridge.Mirrors = new array<ref OZ_KindMirror>();
        }

        Version = LatestVersion();
        return true;
    }

    // Кожне зауваження -- окремий Warning. Завантаження НЕ валиться.
    override void Validate(out int warnings)
    {
        warnings = 0;

        if (!AdminIds)
            AdminIds = new array<string>();
        if (!Bridge)
            Bridge = new OZ_BridgeSettings();
        if (!Bridge.Kinds)
            Bridge.Kinds = new array<string>();
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
            OZ_KindMirror m = Bridge.Mirrors[mi];
            if (!m || m.Kind == "")
            {
                OZ_Log.Warn("Bridge.Mirrors[" + mi.ToString() + "] has no Kind and is ignored");
                warnings++;
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
                    warnings++;
                    break;
                }
            }
        }

        for (int i = 0; i < AdminIds.Count(); i++)
        {
            if (AdminIds[i].Length() != 17)
            {
                string bad = "AdminIds[" + i;
                bad += "] is not a 17-digit Steam64 id: " + AdminIds[i];
                OZ_Log.Warn(bad);
                warnings++;
            }
        }

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
        if (Bridge.Enabled && Bridge.Url.IndexOf("https://") != 0)
        {
            OZ_Log.Warn("Bridge.Url is not https - the shared secret travels in the request body");
            warnings++;
        }

        if (Bridge.PollTimeoutSec < OZ_Const.REST_TIMEOUT_MIN || Bridge.PollTimeoutSec > OZ_Const.REST_TIMEOUT_MAX)
        {
            OZ_Log.Warn("Bridge.PollTimeoutSec outside the engine range 3..120, clamped");
            Bridge.PollTimeoutSec = Math.Clamp(Bridge.PollTimeoutSec, OZ_Const.REST_TIMEOUT_MIN, OZ_Const.REST_TIMEOUT_MAX);
            warnings++;
        }
    }

    static void ServerLoad()
    {
        OZ_Json.EnsureTree();

        s_Inst = new OZ_Settings();
        OZ_ConfigLoader<OZ_Settings>.Load(OZ_Const.SETTINGS, "Settings", s_Inst);

        OZ_Log.SetDebug(s_Inst.DebugMode);
    }
}
