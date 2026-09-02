// Прив'язка акаунта Discord -- серверна половина.
//
// Переїхала сюди з КПК, і це не перестановка файлів: прив'язка належить
// ГРАВЦЕВІ. Вона переживає втрату пристрою, смерть і зміну апарата, від неї
// залежать фракція, стаж і посади, а їх питають рація, квести й ІІ -- яким
// екран ні до чого. Поки вона жила в моді інтерфейсу, сервер без КПК не міг
// прив'язати нікого.
//
// Порядок:
//
//     гравець зайшов, не прив'язаний
//       -> ядро відкриває ворота (окреме вікно, яке не відпускає)
//       -> гравець тисне «отримати код»
//       -> сервер просить у моста код (v1/link/begin)
//       -> код їде гравцеві, він набирає /link КОД у Discord
//       -> сервер ПОВІЛЬНО перепитує міст (v1/link/status)
//       -> DiscordId лягає у файл акаунта, ворота відчиняються
//
// Чому опитування, а не push: міст не може постукати в гру -- DayZ не приймає
// вхідних з'єднань. Єдиний канал усередину -- довге утримання опиту, і воно
// вже зайняте чатом.

class OZ_LinkBeginReply : OZ_BridgeReply
{
    protected string m_Uid;

    void OZ_LinkBeginReply(string uid)
    {
        m_Uid = uid;
    }

    override void OnBody(string json)
    {
        PlayerIdentity to = OZ_Link.Online(m_Uid);
        if (!to)
            return;

        OZ_LinkGrant g;
        string err;
        if (!JsonFileLoader<OZ_LinkGrant>.LoadData(json, g, err) || !g || g.Code == "")
        {
            OZ_Rpc.LinkRespond(to, OZ_LinkConst.OP_BEGIN, false, "", "STR_OZ_ERR_INTERNAL");
            return;
        }

        // Код доїхав -- отже з цієї миті є сенс питати статус.
        OZ_Link.Watch(m_Uid);

        OZ_Rpc.LinkRespond(to, OZ_LinkConst.OP_BEGIN, true, json, "");
    }

    override void OnFail(int code)
    {
        PlayerIdentity to = OZ_Link.Online(m_Uid);
        if (!to)
            return;

        OZ_Rpc.LinkRespond(to, OZ_LinkConst.OP_BEGIN, false, "", "STR_OZ_ERR_NO_BRIDGE");
    }
}

// Відповідь на «чи вже прив'язав». Нікому не відповідає сама: пише у файл, а
// клієнт побачить зміну наступним же власним запитом стану.
class OZ_LinkStatusReply : OZ_BridgeReply
{
    protected string m_Uid;

    void OZ_LinkStatusReply(string uid)
    {
        m_Uid = uid;
    }

    override void OnBody(string json)
    {
        OZ_LinkState st;
        string err;
        if (!JsonFileLoader<OZ_LinkState>.LoadData(json, st, err) || !st)
            return;

        if (!st.Linked)
            return;

        OZ_Link.Confirm(m_Uid, st.DiscordId, st.DiscordName);
    }

    override void OnFail(int code)
    {
        // Міст ліг посеред очікування. Нічого не робимо: наступний тік
        // спитає знову, а вийде час -- знімемось самі.
    }
}

class OZ_Link
{
    private static ref map<string, int> s_Waiting;

    private static const int POLL_MS   = 5000;
    private static const int GIVEUP_MS = 600000;

    private static ref Timer s_Timer;
    private static ref OZ_LinkTicker s_Ticker;
    private static int s_NextAt = 0;

    // Знайти живу особу за uid. Потрібно, бо відповідь моста приїжджає
    // ПІЗНІШЕ за запит, і особа, захоплена тоді, могла вже протухнути.
    static PlayerIdentity Online(string uid)
    {
        if (uid == "")
            return null;

        array<Man> players = new array<Man>();
        GetGame().GetPlayers(players);

        for (int i = 0; i < players.Count(); i++)
        {
            if (!players[i])
                continue;

            PlayerIdentity id = players[i].GetIdentity();
            if (!id)
                continue;
            if (id.GetPlainId() == uid)
                return id;
        }
        return null;
    }

    static bool IsLinked(string uid)
    {
        if (uid == "")
            return false;

        OZ_PlayerData d = OZ_PlayerStore.Load(uid);
        return d.DiscordId != "";
    }

    // Чи мусить цей гравець прив'язатись, перш ніж грати.
    //
    // Три причини сказати «ні», і кожна названа: вимкнено налаштуванням, уже
    // прив'язаний, або моста немає й код видати нікому -- останнє тому, що
    // жорсткі ворота при мертвому боті перетворили б збій бота на збій
    // сервера. Хто хоче саме жорстких -- ставить AllowPlayWhenBridgeDown у
    // false.
    static bool Gated(string uid)
    {
        OZ_Settings s = OZ_Settings.Get();
        if (!s)
            return false;
        if (!s.RequireDiscordLink)
            return false;
        if (IsLinked(uid))
            return false;

        // НУЛЬ ДЗЕРКАЛ -- ВОРІТ НЕМАЄ (ТЗ-2 R2.4).
        //
        // Вимагати прив'язки до сервісу, який нікуди не пише, означає
        // замикати вхід заради нічого: гравець іде по код, отримує роль у
        // гільдії, вертається -- і не бачить жодної різниці, бо в гільдії
        // тихо. Про сам стан кажуть уголос один раз на буті (OZ_Module),
        // а тут просто не тримаємо двері.
        //
        // Не плутати з «міст лежить» нижче: там сервіс є й мовчить тимчасово,
        // тут його свідомо вимкнули.
        if (OZ_BridgeClient.MirrorCount() == 0)
            return false;

        // Alive(), а не IsRunning(): друге означає «опит увімкнено», і при
        // мертвому боті лишається true назавжди -- через що цей вихід не
        // спрацьовував ЖОДНОГО разу саме тоді, коли був потрібен.
        if (!OZ_BridgeClient.Alive())
        {
            if (s.AllowPlayWhenBridgeDown)
                return false;
        }

        return true;
    }

    // Попросити в моста код. Відповідь піде гравцеві сама, відкладено.
    static void Begin(PlayerIdentity who)
    {
        if (!who)
            return;

        string uid = who.GetPlainId();

        if (IsLinked(uid))
        {
            OZ_Rpc.LinkRespond(who, OZ_LinkConst.OP_BEGIN, false, "", "STR_OZ_ERR_ALREADY_LINKED");
            return;
        }

        if (!OZ_BridgeClient.Alive())
        {
            OZ_Rpc.LinkRespond(who, OZ_LinkConst.OP_BEGIN, false, "", "STR_OZ_ERR_NO_BRIDGE");
            return;
        }

        OZ_LinkAsk a = new OZ_LinkAsk();
        a.Uid = uid;

        string letter;
        string err;
        if (!JsonFileLoader<OZ_LinkAsk>.MakeData(a, letter, err, false))
        {
            OZ_Log.Error("link: cannot build the letter: " + err);
            OZ_Rpc.LinkRespond(who, OZ_LinkConst.OP_BEGIN, false, "", "STR_OZ_ERR_INTERNAL");
            return;
        }

        OZ_BridgeClient.Call("v1/link/begin", letter, new OZ_LinkBeginReply(uid));
    }

    // Клієнт питає «ну що там». Відповідаємо з файла, нічого не питаючи в
    // моста: за міст тут відповідає повільний серверний тік.
    static void SendState(PlayerIdentity who)
    {
        if (!who)
            return;

        OZ_LinkState st = new OZ_LinkState();
        st.Linked = IsLinked(who.GetPlainId());

        // DiscordId клієнтові НЕ віддаємо: він йому ні для чого, а це чужий
        // ідентифікатор у чужій системі.
        string json;
        string err;
        if (!JsonFileLoader<OZ_LinkState>.MakeData(st, json, err, false))
            return;

        OZ_Rpc.LinkRespond(who, OZ_LinkConst.OP_STATE, true, json, "");
    }

    static void Watch(string uid)
    {
        if (uid == "")
            return;

        if (!s_Waiting)
            s_Waiting = new map<string, int>();

        s_Waiting.Set(uid, GetGame().GetTime() + GIVEUP_MS);
        EnsureTimer();
    }

    static void Confirm(string uid, string discordId, string discordName)
    {
        if (!GetGame().IsServer())
            return;

        // Порожній id -- це НЕ прив'язка. Поле під назвою DiscordId мусить
        // тримати ідентифікатор Discord, а не щось на його місці.
        if (discordId == "")
            return;

        OZ_PlayerData d = OZ_PlayerStore.Load(uid);

        if (d.DiscordId != discordId)
        {
            d.DiscordId = discordId;
            OZ_PlayerStore.MarkDirty(uid);

            string m = "link: " + uid;
            m += " is now linked as \"" + discordName + "\"";
            OZ_Log.Info(m);
        }

        Forget(uid);

        // Ворота чекають саме на це -- кажемо одразу, а не за секунду.
        PlayerIdentity to = Online(uid);
        if (to)
            SendState(to);
    }

    static void Forget(string uid)
    {
        if (!s_Waiting)
            return;
        if (!s_Waiting.Contains(uid))
            return;

        s_Waiting.Remove(uid);
    }

    private static void EnsureTimer()
    {
        if (s_Timer)
            return;

        // Носія тримаємо ЖИВИМ у статичному полі: таймер зберігає слабке
        // посилання, і локальний примірник прибрався б одразу після виходу з
        // методу, а таймер тікав би в порожнечу.
        s_Ticker = new OZ_LinkTicker();

        s_Timer = new Timer(CALL_CATEGORY_SYSTEM);
        s_Timer.Run(1.0, s_Ticker, "OZ_LinkTick", NULL, true);
    }

    static void Tick()
    {
        if (!s_Waiting)
            return;
        if (s_Waiting.Count() == 0)
            return;
        if (!OZ_BridgeClient.Alive())
            return;

        int now = GetGame().GetTime();
        if (now < s_NextAt)
            return;
        s_NextAt = now + POLL_MS;

        // Знімаємо прострочених ОКРЕМИМ проходом: правити мапу, по якій
        // ітеруєш, -- та помилка, яку потім ловлять місяцями.
        array<string> expired = new array<string>();

        for (int i = 0; i < s_Waiting.Count(); i++)
        {
            string uid = s_Waiting.GetKey(i);

            if (now > s_Waiting.GetElement(i))
            {
                expired.Insert(uid);
                continue;
            }

            OZ_LinkAsk a = new OZ_LinkAsk();
            a.Uid = uid;

            string letter;
            string err;
            if (!JsonFileLoader<OZ_LinkAsk>.MakeData(a, letter, err, false))
                continue;

            OZ_BridgeClient.Call("v1/link/status", letter, new OZ_LinkStatusReply(uid));
        }

        for (int j = 0; j < expired.Count(); j++)
        {
            OZ_Log.Dbg("link: gave up waiting for " + expired[j]);
            s_Waiting.Remove(expired[j]);
        }
    }
}

// Таймер Enforce кличе метод ЗА ІМЕНЕМ на об'єкті, а OZ_Link -- статичний
// клас без примірника. Тому один носій: він і є той об'єкт.
class OZ_LinkTicker
{
    void OZ_LinkTick()
    {
        OZ_Link.Tick();
    }
}
