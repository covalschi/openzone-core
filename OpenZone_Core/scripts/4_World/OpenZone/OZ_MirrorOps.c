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

// Міст залив (або не залив) історію. Лише після його «так» прапорець
// пишеться -- відмова на середині лишає Mirror як був, а залите не
// продублюється при повторі: у кожного рядка стабільний власний id.
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

        // Знімаємо обидва поля до першої склейки рядка: конверт виділив
        // серіалізатор, і читати з нього після виділення пам'яті вже не
        // можна (шапка OZ_ConfigBase).
        bool   ok  = ack.Ok;
        string why = ack.Why;

        if (!ok)
        {
            OZ_Log.Warn("mirror: the bridge did not fill " + m_Kind + ": " + why);
            if (to)
                OZ_Rpc.AdminRespond(to, OZ_AdminSect.CONFIG, m_Op, false, "", why);
            return;
        }

        // Залито -- тепер прапорець. Адмін міг вийти, поки міст працював:
        // прапорець пишеться однаково, бо операцію він уже замовив.
        //
        // Історія при відмові запису лишається залитою, і це не біда: у
        // гільдії з'явився архів, якого там не було, а дзеркало як було
        // вимкнене, так і лишилось. Повторний виклик нічого не подвоїть --
        // у кожного рядка стабільний власний id.
        if (!OZ_MirrorOps.Write(m_Kind, true))
        {
            if (to)
                OZ_Rpc.AdminRespond(to, OZ_AdminSect.CONFIG, m_Op, false, "", "STR_OZ_ERR_INTERNAL");
            return;
        }

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
    // Стан дзеркал для панелі.
    //
    // СПИСОК БУДУЄМО З ТОГО, ЩО СПРАВДІ Є, а не з двох літералів.
    //
    // Тут стояли жорстко "chat" і "roles" -- «два роди, у яких є дзеркало».
    // Обидва належать ЧУЖИМ модам: chat реєструє КПК, roles -- фракції. Тобто
    // ядро наодинці пропонувало адмінові тумблери для дзеркал, яких на його
    // сервері не існує, а третій мод зі своїм родом не з'явився б у панелі
    // ніколи -- при тому, що підписка на рід і так проходить через ядро.
    //
    // Джерел два, і обидва потрібні: підписки кажуть, що на сервері живе
    // (OZ_BridgeClient.Subscribe), а Settings -- що адмін уже вмикав, зокрема
    // й для мода, який зараз знято.
    static string List(out bool ok, out string error)
    {
        ok = false;

        OZ_MirrorState st = new OZ_MirrorState();
        array<string> kinds = new array<string>();
        OZ_BridgeClient.FillKinds(kinds);

        OZ_Settings s = OZ_Settings.Get();
        if (s && s.Bridge && s.Bridge.Mirrors)
        {
            for (int i = 0; i < s.Bridge.Mirrors.Count(); i++)
            {
                OZ_KindMirror m = s.Bridge.Mirrors[i];
                if (!m || m.Kind == "")
                    continue;
                if (kinds.Find(m.Kind) == -1)
                    kinds.Insert(m.Kind);
            }
        }

        for (int k = 0; k < kinds.Count(); k++)
        {
            OZ_KindMirror copy = new OZ_KindMirror();
            copy.Kind   = kinds[k];
            copy.Mirror = OZ_BridgeClient.Mirrored(kinds[k]);
            st.Mirrors.Insert(copy);
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
            if (!Write(kind, false))
            {
                error = "STR_OZ_ERR_INTERNAL";
                return "";
            }
            rep.On   = false;
            if (kind == "roles")
                rep.Note = "the bot stops touching Discord roles from the next poll; they stay as they are until the mirror is on again";
            else
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

        // Питаємо ПЕРЕД заливкою: результат однаково не буде куди записати,
        // а заливка історії -- найдорожча операція, яка тут узагалі є.
        if (!OZ_Settings.Writable())
        {
            OZ_Log.Error("mirror: " + kind + " not switched on - Settings could not be read");
            error = "STR_OZ_ERR_INTERNAL";
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
    //
    // Повертає false, коли писати не можна: тоді в пам'яті теж нічого не
    // міняється, бо стан панелі й стан файла мусять збігатись.
    static bool Write(string kind, bool on)
    {
        OZ_Settings s = OZ_Settings.Get();
        if (!s || !s.Bridge)
            return false;

        // ФАЙЛ, ЯКОГО ЛОАДЕР НЕ ЗРОЗУМІВ, НЕ ПЕРЕЗАПИСУЄМО ОДНІЄЮ БУЛЕВОЮ.
        //
        // У пам'яті зараз дефолти, а на диску -- єдиний примірник із
        // AdminIds, адресою й секретом моста. Запис поверх нього стер би все
        // це заради тумблера в панелі, і .bak не оновлюється (нижче), тож
        // відновлюватись було б нізвідки. Error, а не Warn: адмін натиснув
        // кнопку й мусить дізнатись, що вона не спрацювала.
        if (!OZ_Settings.Writable())
        {
            string no = "mirror: " + kind;
            no += " not written - Settings could not be read and must not be overwritten with defaults";
            OZ_Log.Error(no);
            return false;
        }

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

        // БЕЗ РЕЗЕРВНОЇ КОПІЇ, і це не економія.
        //
        // Слот Settings.bak.json один, і в ньому лежить те, що адмін правив
        // руками, -- зокрема застарілий розділ "Faction", який мод фракцій
        // читає звідти при своїй міграції. Перемикач дзеркала, тобто одна
        // булева, затирав цю копію ЩОРАЗУ, і після двох натискань відновити
        // з неї було вже нічого.
        OZ_ConfigLoader<OZ_Settings>.Save(OZ_Const.SETTINGS, OZ_Const.SETTINGS_TAG, s, false);

        string state = "off";
        if (on)
            state = "on";
        OZ_Log.Info("mirror: " + kind + " is now " + state + " (Settings written)");
        return true;
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
