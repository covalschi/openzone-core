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
    // Забути все при кінці місії. Статики переживають перепідключення, і
    // без цього наступний сервер успадковував би ЧУЖИЙ знімок: прапорець
    // адміна, перелік сторінок, рівень налагодження -- усе з минулого
    // сервера, поки новий не пришле свій OZ_Sync. А сервер без OpenZone не
    // пришле його ніколи.
    static void Forget()
    {
        s_Payload = null;

        // Слухач -- це меню МИНУЛОЇ місії. Не скинути його -- значить і
        // тримати мертве меню живим, і віддавати йому відповіді наступного
        // сервера. Вотчер скидаємо теж: підписники самі знімаються у своїх
        // OnMissionFinish (вони йдуть ПЕРЕД цим Forget), а хто не зняться --
        // тому й потрібен чистий інвокер. Недоскладені частини відповідей
        // минулої місії не мають права приклеїтись до відповіді наступної.
        s_Listener = null;
        s_Watch = null;
        if (s_Inst)
            s_Inst.m_ResParts.Clear();

        OZ_Log.SetDebug(false);
    }

    private static ref OZ_ResponseListener s_Listener;

    // Другий, НЕадресний слухач: усі відповіді й пуші, що приїхали клієнтові,
    // байдуже, відкрите меню чи ні. Потрібен речам поза меню -- тост чату
    // на HUD слухає саме тут. Лінивий: нікому не треба -- нічого й немає.
    private static ref ScriptInvoker s_Watch;

    static ScriptInvoker ResponseWatch()
    {
        if (!s_Watch)
            s_Watch = new ScriptInvoker();
        return s_Watch;
    }

    static void BindListener(OZ_ResponseListener l)
    {
        s_Listener = l;
    }

    private static ref OZ_SyncPayload  s_Payload;
    private static ref OZ_ClientState  s_Inst;

    // Частини довгих відповідей, що ще їдуть: ключ -- сторінка|операція.
    // Фінальний OZ_Res забирає й чистить; порядок гарантує канал.
    private ref map<string, string> m_ResParts = new map<string, string>();

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

    // Частина довгої відповіді: рушійний RPC псує рядки понад ~1024 байти,
    // тому тіло їде шматками поперед свого конверта (OZ_Rpc.Respond).
    void OZ_ResPart(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Client)
            return;

        Param3<string, string, string> part;
        if (!ctx.Read(part))
            return;

        string key = part.param1 + "|" + part.param2;
        string sofar = "";
        m_ResParts.Find(key, sofar);
        m_ResParts.Set(key, sofar + part.param3);
    }

    void OZ_Res(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Client)
            return;

        Param5<string, string, bool, string, string> data;
        if (!ctx.Read(data))
            return;

        // Довге тіло приїхало частинами поперед конверта -- приклеїти.
        string pkey = data.param1 + "|" + data.param2;
        string parts = "";
        if (m_ResParts.Find(pkey, parts))
        {
            data.param4 = parts + data.param4;
            m_ResParts.Remove(pkey);
        }

        string line = "response page=" + data.param1;
        line += " op=" + data.param2;
        line += " ok=" + data.param3;
        if (!data.param3)
            line += " error=" + data.param5;
        OZ_Log.Dbg(line);

        if (s_Listener)
            s_Listener.OnResponse(data.param1, data.param2, data.param3, data.param4, data.param5);

        if (s_Watch)
            s_Watch.Invoke(data.param1, data.param2, data.param3, data.param4, data.param5);
    }
}
