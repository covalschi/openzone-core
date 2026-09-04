// Клієнтська половина: те, що сервер сам вирішив надіслати.
//
// Тут НЕМАЄ жодного рішення -- лише пам'ять про відповідь сервера. Прапорця
// прав тут теж немає: клієнт КПК більше не знає, адмін гравець чи ні (ТЗ-5
// §C2), бо не лишилось жодного екрана, який би це питав.

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
    // без цього наступний сервер успадковував би ЧУЖИЙ знімок: перелік
    // сторінок, стан прив'язки, рівень налагодження -- усе з минулого
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
        s_AdminWatch = null;
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

    // Відповіді АДМІНСЬКОЇ консолі -- окремий інвокер, а не гілка в
    // сторінковому. Розділи більше не сторінки (ТЗ-5 §C2), і слухачі в них
    // інші: сторінки слухає екран КПК, розділи -- вікно VPP. Спільний інвокер
    // означав би, що кожен слухач фільтрує чужі конверти на око -- рівно та
    // помилка, через яку панель фракцій адресувала свої запити на "admin".
    private static ref ScriptInvoker s_AdminWatch;

    static ScriptInvoker AdminWatch()
    {
        if (!s_AdminWatch)
            s_AdminWatch = new ScriptInvoker();
        return s_AdminWatch;
    }

    private static ref OZ_SyncPayload  s_Payload;
    private static ref OZ_ClientState  s_Inst;

    // Частини довгих відповідей, що ще їдуть: ключ -- НОМЕР ПОВІДОМЛЕННЯ,
    // який роздає відправник (див. OZ_Rpc). Фінальний конверт забирає й
    // чистить; порядок гарантує канал.
    //
    // Ключем була пара «сторінка + операція»: два пуші однієї операції --
    // скажімо, дві довгі новини підряд -- перепліталися шматками, і гинули
    // обидва. Номер робить кожну посилку окремою незалежно від того, що в ній.
    private ref map<int, string> m_ResParts = new map<int, string>();

    static OZ_ClientState Instance()
    {
        if (!s_Inst)
            s_Inst = new OZ_ClientState();
        return s_Inst;
    }

    static OZ_SyncPayload Get()    { return s_Payload; }
    static bool           Ready()  { return s_Payload != null; }

    // Що доклав мод у пакет (OZ_SyncExtras). До першого пакета -- запасне.
    static string Extra(string key, string fallback)
    {
        return OZ_SyncExtras.Of(s_Payload, key, fallback);
    }

    // Пакет приїхав -- перший чи повторний (ТЗ-5 R-C1.3). Хто тримає в себе
    // значення з нього, перечитує тут, а не гадає, чи змінилось.
    private static ref ScriptInvoker s_SyncWatch;

    static ScriptInvoker SyncWatch()
    {
        if (!s_SyncWatch)
            s_SyncWatch = new ScriptInvoker();
        return s_SyncWatch;
    }

    // IsAdmin() ТУТ БІЛЬШЕ НЕМАЄ, разом із полем, яке його живило.
    //
    // Він існував заради двох кнопок на карті КПК, які клієнт ховав від
    // неадміна; кнопки поїхали у вкладку VPP (ТЗ-5 §C4), і викликачів не
    // лишилось жодного. Права адміна тепер живуть лише на сервері, де вони
    // й вирішуються.

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
        line += " debug=" + p.DebugMode;
        line += " linked=" + p.Linked;
        line += " gate=" + p.LinkRequired;
        line += " extras=" + p.Extras.Count().ToString();
        OZ_Log.Info(line);

        SyncWatch().Invoke(p);
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

    // Звістка від сервера. Далі її розбирає той, хто малює.
    //
    // Ядровим цим каналом сьогодні говорить КПК (обмін контактами); фракції
    // мають власний, під власним іменем мода, і зводиться все одно в
    // OZ_Notice.
    //
    // ІМ'Я МЕТОДА -- ЦЕ РЯДОК НА ПРОВОДІ (OZ_Rpc.RPC_NOTICE), збіг посимвольний;
    // збіг із іменем класу-збірника нижче не заважає, як не заважає в OZ_Show.
    void OZ_Notice(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Client)
            return;

        Param3<string, bool, string> data;
        if (!ctx.Read(data))
            return;

        OZ_Notice.Take(data.param1, data.param2, data.param3);
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

        Param2<int, string> part;
        if (!ctx.Read(part))
            return;

        string sofar = "";
        m_ResParts.Find(part.param1, sofar);
        m_ResParts.Set(part.param1, sofar + part.param2);
    }

    void OZ_Res(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Client)
            return;

        Param6<int, string, string, bool, string, string> data;
        if (!ctx.Read(data))
            return;

        // Довге тіло приїхало частинами поперед конверта -- приклеїти.
        string body = data.param5;
        string parts = "";
        if (m_ResParts.Find(data.param1, parts))
        {
            body = parts + body;
            m_ResParts.Remove(data.param1);
        }

        string line = "response page=" + data.param2;
        line += " op=" + data.param3;
        line += " ok=" + data.param4;
        if (!data.param4)
            line += " error=" + data.param6;
        OZ_Log.Dbg(line);

        if (s_Listener)
            s_Listener.OnResponse(data.param2, data.param3, data.param4, body, data.param6);

        if (s_Watch)
            s_Watch.Invoke(data.param2, data.param3, data.param4, body, data.param6);
    }

    // Частина довгої АДМІНСЬКОЇ відповіді. Ключ той самий, що й у сторінок --
    // номер повідомлення, роздає відправник, -- тому окремої мапи не треба:
    // номери не перетинаються, бо роздає їх один лічильник.
    void OZ_AdminResPart(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Client)
            return;

        Param2<int, string> part;
        if (!ctx.Read(part))
            return;

        string sofar = "";
        m_ResParts.Find(part.param1, sofar);
        m_ResParts.Set(part.param1, sofar + part.param2);
    }

    void OZ_AdminRes(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Client)
            return;

        Param6<int, string, string, bool, string, string> data;
        if (!ctx.Read(data))
            return;

        string body = data.param5;
        string parts = "";
        if (m_ResParts.Find(data.param1, parts))
        {
            body = parts + body;
            m_ResParts.Remove(data.param1);
        }

        string line = "admin response section=" + data.param2;
        line += " op=" + data.param3;
        line += " ok=" + data.param4;
        if (!data.param4)
            line += " error=" + data.param6;
        OZ_Log.Dbg(line);

        if (s_AdminWatch)
            s_AdminWatch.Invoke(data.param2, data.param3, data.param4, body, data.param6);
    }
}
