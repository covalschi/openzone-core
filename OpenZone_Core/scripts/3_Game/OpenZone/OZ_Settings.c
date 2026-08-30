// Settings.json -- СЕРВЕРНА поверхня конфігу.
//
// Тут лежить секрет моста, і саме тому цей об'єкт ніколи не серіалізується на
// клієнт цілком. Те, що їде проводом, збирає окремий OZ_SyncPayload -- і це не
// стильова забаганка, а межа безпеки: один недбалий SendRPC із цим об'єктом
// роздав би секрет кожному, хто зайшов на сервер.

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
}

// Фракцiйнi межi -- рiшення сервера, не Discord. Стеля складу живе НЕ тут,
// а в кожнiй фракцiї окремо (Factions.json, поле MaxMembers): у кожного
// угруповання свiй штат, i редагується вiн через адмiнську консоль.
class OZ_FactionLimits
{
    // Скiльки живе запрошення у фракцiю. Довше -- i гравець приймає
    // запрошення вiд лiдера, який давно передумав.
    int InviteTtlSeconds = 120;
}

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

    ref OZ_FactionLimits Faction;

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
        Faction       = new OZ_FactionLimits();

        RequireDiscordLink      = true;
        AllowPlayWhenBridgeDown = true;
    }

    override bool Migrate(int from)
    {
        // Ланцюжок покрокових мiграцiй: кожен крок пiднiмає на одну версiю.
        if (from < 2)
        {
            // v2: фракцiйнi межi. Новий блок з умовчаннями -- стара поведiнка
            // (без лiмiту, двi хвилини на запрошення) зберiгається дослiвно.
            Faction = new OZ_FactionLimits();
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
        if (!Faction)
            Faction = new OZ_FactionLimits();

        if (Faction.InviteTtlSeconds < 10)
        {
            OZ_Log.Warn("Faction.InviteTtlSeconds under 10 s cannot be read in time, clamped to 10");
            Faction.InviteTtlSeconds = 10;
            warnings++;
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
