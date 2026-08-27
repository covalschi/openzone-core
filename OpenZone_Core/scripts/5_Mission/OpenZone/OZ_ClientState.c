// Клієнтська половина: те, що сервер сам вирішив надіслати.
//
// Тут НЕМАЄ жодного рішення -- лише пам'ять про відповідь сервера. IsAdmin()
// служить рівно для того, щоб не малювати кнопку, якої гравець усе одно не
// зможе натиснути; на сервері та сама перевірка робиться заново й по-справжньому.

// Слухач відповідей. Ядро не знає, хто саме відкритий на екрані, і знати не
// повинне: воно доставляє конверт і кличе того, хто підписався.
class OZ_ResponseListener
{
    void OnResponse(string pageId, string op, bool ok, string json, string error)
    {
    }
}

class OZ_ClientState
{
    private static ref OZ_ResponseListener s_Listener;

    static void BindListener(OZ_ResponseListener l)
    {
        s_Listener = l;
    }

    private static ref OZ_SyncPayload  s_Payload;
    private static ref OZ_ClientState  s_Inst;

    static OZ_ClientState Instance()
    {
        if (!s_Inst)
            s_Inst = new OZ_ClientState();
        return s_Inst;
    }

    static OZ_SyncPayload Get()    { return s_Payload; }
    static bool           Ready()  { return s_Payload != null; }

    static bool IsAdmin()
    {
        if (!s_Payload)
            return false;
        return s_Payload.IsAdmin;
    }

    static OZ_SyncPageInfo PageInfo(string pageId)
    {
        if (!s_Payload)
            return null;

        for (int i = 0; i < s_Payload.Pages.Count(); i++)
        {
            if (s_Payload.Pages[i].PageId == pageId)
                return s_Payload.Pages[i];
        }
        return null;
    }

    // --- обробники CF: ім'я = рядок з AddRPC, чотири параметри, не static ---

    void OZ_Sync(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Client)
            return;

        Param1<string> data;
        if (!ctx.Read(data))
            return;

        string err;
        OZ_SyncPayload p = new OZ_SyncPayload();
        if (!JsonFileLoader<OZ_SyncPayload>.LoadData(data.param1, p, err))
        {
            OZ_Log.Error("sync payload unreadable: " + err);
            return;
        }

        s_Payload = p;
        OZ_Log.SetDebug(p.DebugMode);

        // Єдине місце, де вмикаються ворота прив'язки. Рішення серверне,
        // клієнт лише виконує.
        OZ_LinkGate.FromSync(p.Linked, p.LinkRequired);

        string line = "sync received: pages=" + p.Pages.Count().ToString();
        line += " admin=" + p.IsAdmin;
        line += " debug=" + p.DebugMode;
        line += " linked=" + p.Linked;
        line += " gate=" + p.LinkRequired;
        OZ_Log.Info(line);
    }

    // Відповідь на запит прив'язки. Веде її ВІКНО, а не цей клас: тут лише
    // розбір конверта й передача тому, хто малює.
    void OZ_LinkRes(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Client)
            return;

        Param4<string, bool, string, string> data;
        if (!ctx.Read(data))
            return;

        OZ_LinkMenu m = OZ_LinkGate.Menu();
        if (!m)
            return;

        m.OnLinkResponse(data.param1, data.param2, data.param3, data.param4);
    }

    // Відповідь на прохання змінити ролі. Далі її розбирає той, хто малює.
    void OZ_RoleRes(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Client)
            return;

        Param3<string, bool, string> data;
        if (!ctx.Read(data))
            return;

        OZ_RoleNotice.Take(data.param1, data.param2, data.param3);
    }

    // «Покажи це». Ядро не знає, що саме -- лише розносить команду тим, хто
    // підписався. КПК підписується на "pda" й відкриває себе.
    void OZ_Show(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Client)
            return;

        Param1<string> data;
        if (!ctx.Read(data))
            return;

        OZ_Show.Take(data.param1);
    }

    void OZ_Res(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Client)
            return;

        Param5<string, string, bool, string, string> data;
        if (!ctx.Read(data))
            return;

        string line = "response page=" + data.param1;
        line += " op=" + data.param2;
        line += " ok=" + data.param3;
        if (!data.param3)
            line += " error=" + data.param5;
        OZ_Log.Dbg(line);

        if (s_Listener)
            s_Listener.OnResponse(data.param1, data.param2, data.param3, data.param4, data.param5);
    }
}
