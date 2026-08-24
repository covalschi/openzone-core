// Клієнтська половина: те, що сервер сам вирішив надіслати.
//
// Тут НЕМАЄ жодного рішення -- лише пам'ять про відповідь сервера. IsAdmin()
// служить рівно для того, щоб не малювати кнопку, якої гравець усе одно не
// зможе натиснути; на сервері та сама перевірка робиться заново й по-справжньому.

class OZ_ClientState
{
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

        string line = "sync received: pages=" + p.Pages.Count().ToString();
        line += " admin=" + p.IsAdmin;
        line += " debug=" + p.DebugMode;
        OZ_Log.Info(line);
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

        // Роздачу відкритій сторінці додає Task 9 разом із самим меню.
    }
}
