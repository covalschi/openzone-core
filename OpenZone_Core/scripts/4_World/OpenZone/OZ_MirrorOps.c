// Перемикання дзеркала Discord з адмінської консолі (ТЗ-2 §8).
//
// ЧОМУ ОПЕРАЦІЯ, А НЕ ПРАВКА ФАЙЛА (R5.1). Правка Bridge.Mirrors у
// OZ_Core_Settings.json змінила б, куди мод пише, але не перенесла б нічого:
// рядки, надіслані при вимкненому дзеркалі, живуть лише в базі бота, і
// гільдія їх не бачила. Тому вмикання -- це спершу ЗАЛИВКА історії ботом і
// лише потім Mirror: true (R5.2). Вимкнення -- лише прапорець: бот перестає
// писати з наступного опиту, треди лишаються архівом (R5.3). Дім даних не
// змінюється в жодну сторону (R5.4). Той самий стан двічі -- Skipped (R5.5).

class OZ_MirrorReport
{
    int    Pushed  = 0;
    int    Skipped = 0;
    int    Failed  = 0;
    string Note    = "";
    // Що стало після операції -- панелі не треба питати ще раз.
    string Kind    = "";
    bool   On      = false;
}

// Стан усіх дзеркал -- для панелі: ті самі записи, що в Settings.
class OZ_MirrorState
{
    ref array<ref OZ_KindMirror> Mirrors;

    void OZ_MirrorState()
    {
        Mirrors = new array<ref OZ_KindMirror>();
    }
}

class OZ_MirrorFillAsk
{
    string Kind = "";
}

class OZ_MirrorFillAck
{
    bool   Ok      = false;
    string Why     = "";
    int    Pushed  = 0;
    int    Skipped = 0;
    int    Failed  = 0;
    string Note    = "";
}

// Мiст залив (або не залив) iсторiю. Лише пiсля його «так» прапорець
// пишеться -- вiдмова на серединi лишає Mirror як був, а залите не
// продублюється при повторi: у кожного рядка стабiльний власний id.
class OZ_MirrorFillReply : OZ_BridgeReply
{
    protected string m_AdminUid;
    protected string m_Op;
    protected string m_Kind;

    void OZ_MirrorFillReply(string adminUid, string op, string kind)
    {
        m_AdminUid = adminUid;
        m_Op       = op;
        m_Kind     = kind;
    }

    override void OnBody(string json)
    {
        PlayerIdentity to = OZ_Link.Online(m_AdminUid);

        OZ_MirrorFillAck ack;
        string err;
        if (!JsonFileLoader<OZ_MirrorFillAck>.LoadData(json, ack, err) || !ack)
        {
            OZ_Log.Warn("mirror: unreadable answer from the bridge: " + err);
            if (to)
                OZ_Rpc.AdminRespond(to, OZ_AdminSect.CONFIG, m_Op, false, "", "STR_OZ_ERR_INTERNAL");
            return;
        }

        if (!ack.Ok)
        {
            OZ_Log.Warn("mirror: the bridge did not fill " + m_Kind + ": " + ack.Why);
            if (to)
                OZ_Rpc.AdminRespond(to, OZ_AdminSect.CONFIG, m_Op, false, "", ack.Why);
            return;
        }

        // Залито -- тепер прапорець. Адмін міг вийти, поки міст працював:
        // прапорець пишеться однаково, бо операцію він уже замовив.
        OZ_MirrorOps.Write(m_Kind, true);

        OZ_MirrorReport rep = new OZ_MirrorReport();
        rep.Pushed  = ack.Pushed;
        rep.Skipped = ack.Skipped;
        rep.Failed  = ack.Failed;
        rep.Note    = ack.Note;
        rep.Kind    = m_Kind;
        rep.On      = true;

        if (to)
            OZ_Rpc.AdminRespond(to, OZ_AdminSect.CONFIG, m_Op, true, OZ_MirrorOps.Json(rep), "");
    }

    override void OnFail(int code)
    {
        PlayerIdentity to = OZ_Link.Online(m_AdminUid);
        if (!to)
            return;
        OZ_Rpc.AdminRespond(to, OZ_AdminSect.CONFIG, m_Op, false, "", "STR_OZ_ERR_NO_BRIDGE");
    }
}

class OZ_MirrorOps
{
    // Стан дзеркал для панелі. "chat" i "roles" є в списку завжди: це два
    // роди, у яких є дзеркало (ТЗ-2 §8 i §15), i панель мусить мати що
    // показати на порожньому Settings.
    static string List(out bool ok, out string error)
    {
        ok = false;

        OZ_MirrorState st = new OZ_MirrorState();
        OZ_Settings s = OZ_Settings.Get();
        bool sawChat = false;
        bool sawRoles = false;
        if (s && s.Bridge && s.Bridge.Mirrors)
        {
            for (int i = 0; i < s.Bridge.Mirrors.Count(); i++)
            {
                OZ_KindMirror m = s.Bridge.Mirrors[i];
                if (!m || m.Kind == "")
                    continue;
                OZ_KindMirror copy = new OZ_KindMirror();
                copy.Kind   = m.Kind;
                copy.Mirror = OZ_BridgeClient.Mirrored(m.Kind);
                st.Mirrors.Insert(copy);
                if (m.Kind == "chat")
                    sawChat = true;
                if (m.Kind == "roles")
                    sawRoles = true;
            }
        }
        if (!sawChat)
        {
            OZ_KindMirror chat = new OZ_KindMirror();
            chat.Kind   = "chat";
            chat.Mirror = false;
            st.Mirrors.Insert(chat);
        }
        if (!sawRoles)
        {
            OZ_KindMirror roles = new OZ_KindMirror();
            roles.Kind   = "roles";
            roles.Mirror = false;
            st.Mirrors.Insert(roles);
        }

        string outJson;
        string err;
        if (!JsonFileLoader<OZ_MirrorState>.MakeData(st, outJson, err, false))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        ok = true;
        return outJson;
    }

    // "<kind>:on" / "<kind>:off".
    static string Set(string arg, string op, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        int cut = arg.IndexOf(":");
        if (cut <= 0)
        {
            error = "STR_OZ_ERR_UNKNOWN_OP";
            return "";
        }

        string kind = arg.Substring(0, cut);
        string word = arg.Substring(cut + 1, arg.Length() - cut - 1);
        bool on = word == "on";
        if (!on && word != "off")
        {
            error = "STR_OZ_ERR_UNKNOWN_OP";
            return "";
        }

        OZ_Settings s = OZ_Settings.Get();
        if (!s || !s.Bridge || !s.Bridge.Enabled)
        {
            // Правило 1 (R3.2): без моста дзеркал немає, і вмикати нема куди.
            error = "STR_OZ_ERR_NO_BRIDGE";
            return "";
        }

        OZ_MirrorReport rep = new OZ_MirrorReport();
        rep.Kind = kind;

        bool now = OZ_BridgeClient.Mirrored(kind);
        if (now == on)
        {
            rep.Skipped = 1;
            rep.On      = now;
            if (on)
                rep.Note = "the " + kind + " mirror is already on";
            else
                rep.Note = "the " + kind + " mirror is already off";
            ok = true;
            return Json(rep);
        }

        if (!on)
        {
            Write(kind, false);
            rep.On   = false;
            rep.Note = "the bot stops writing " + kind + " to Discord from the next poll; the threads stay as an archive";
            OZ_Log.Info("mirror: " + kind + " switched off by " + sender.GetPlainId());
            ok = true;
            return Json(rep);
        }

        if (!OZ_BridgeClient.Alive())
        {
            error = "STR_OZ_ERR_NO_BRIDGE";
            return "";
        }

        // R3.3: записки при увімкненому дзеркалі бачить персонал гільдії з
        // MANAGE_THREADS. Сьогодні міст такого дзеркала не має й відмовить,
        // але попередження мусить стояти тут, де його прочитають, а не там,
        // де його забудуть.
        if (kind == "notes")
            OZ_Log.Warn("mirror: turning notes on would show every player's notebook to guild staff with MANAGE_THREADS");

        OZ_MirrorFillAsk a = new OZ_MirrorFillAsk();
        a.Kind = kind;

        string letter;
        string jerr;
        if (!JsonFileLoader<OZ_MirrorFillAsk>.MakeData(a, letter, jerr, false))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_Log.Info("mirror: " + kind + " switching on by " + sender.GetPlainId() + " - asking the bridge to fill the history first");
        OZ_BridgeClient.Call("v1/mirror/fill", letter, new OZ_MirrorFillReply(sender.GetPlainId(), op, kind));

        ok    = false;
        error = OZ_Const.DEFER;
        return "";
    }

    // Прапорець у Settings і на диск. Список Mirrors їде мосту з кожним
    // опитом, тож окремого повідомлення не треба.
    static void Write(string kind, bool on)
    {
        OZ_Settings s = OZ_Settings.Get();
        if (!s || !s.Bridge)
            return;
        if (!s.Bridge.Mirrors)
            s.Bridge.Mirrors = new array<ref OZ_KindMirror>();

        bool found = false;
        for (int i = 0; i < s.Bridge.Mirrors.Count(); i++)
        {
            OZ_KindMirror m = s.Bridge.Mirrors[i];
            if (m && m.Kind == kind)
            {
                m.Mirror = on;
                found = true;
                break;
            }
        }
        if (!found)
        {
            OZ_KindMirror add = new OZ_KindMirror();
            add.Kind   = kind;
            add.Mirror = on;
            s.Bridge.Mirrors.Insert(add);
        }

        OZ_ConfigLoader<OZ_Settings>.Save(OZ_Const.SETTINGS, "settings", s);

        string state = "off";
        if (on)
            state = "on";
        OZ_Log.Info("mirror: " + kind + " is now " + state + " (Settings written)");
    }

    static string Json(OZ_MirrorReport rep)
    {
        string outJson;
        string err;
        if (!JsonFileLoader<OZ_MirrorReport>.MakeData(rep, outJson, err, false))
            return "{}";
        return outJson;
    }
}
