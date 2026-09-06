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
    private static ref map<string, int> s_Waiting = new map<string, int>();

    private static const int GIVEUP_MS = 600000;

    // Як часто перепитувати міст про кожного, хто чекає код.
    private static const float POLL_SECONDS = 5.0;

    private static ref Timer s_Timer;
    private static ref OZ_LinkTicker s_Ticker;

    // Коли цей гравець востаннє просив код. Стеля -- нижче.
    private static ref map<string, int> s_BeganAt = new map<string, int>();
    private static const int BEGIN_GAP_MS = 10000;

    // ВОРОТА ТЕПЕР СЕРВЕРНІ (рішення власника R-F2.2/H37, ТЗ-5 §F2).
    //
    // RequireDiscordLink досі виконував ОДИН лише клієнт: вікно, яке не
    // відпускає. Сервер не питав OZ_Link.Gated() ніде, крім прапорця в
    // пакеті синхронізації, тож неприв'язаний гравець із підміненим чи просто
    // без нашого клієнта ходив, лутався й користувався всіма сторінками як
    // прив'язаний. Тобто настройка не обмежувала нічого (defect D32).
    //
    // Рішення власника -- КІК, а не гейт на кожну операцію: пускати в Зону
    // того, кого машинерія ролей не знає, немає сенсу ні в чому, а перевірка
    // в кожній точці входу -- це двадцять місць, де її колись забудуть.
    //
    // Кік відкладений, і це не м'якість: код видає МІСТ, гравець мусить піти в
    // Discord і набрати шість символів. П'ять хвилин -- це «пішов і повернувся»
    // з великим запасом; хто за цей час не прив'язався, той відмовився.
    private static ref map<string, int> s_KickAt = new map<string, int>();
    private static const int GATE_GRACE_MS = 300000;

    // Кому вже сказали, за що. Кік іде НАСТУПНИМ тіком, щоб гарантований
    // RPC із причиною встиг доїхати до вікна воріт (R-F2.3: кик із внятною
    // причиною, а не мовчазний розрив).
    private static ref array<string> s_KickTold = new array<string>();

    // Знайти живу особу за uid. Потрібно, бо відповідь моста приїжджає
    // ПІЗНІШЕ за запит, і особа, захоплена тоді, могла вже протухнути.
    static PlayerIdentity Online(string uid)
    {
        Man m = OZ_Players.ManOf(uid);
        if (!m)
            return null;
        return m.GetIdentity();
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

        // ОДИН КОД НА ДЕСЯТЬ СЕКУНД, і це не ввічливість до моста.
        //
        // OP_BEGIN не троттлився ніяк, а кожен виклик означав HTTP до моста
        // ПЛЮС повне скидання OZ_BridgeCache (дорога v1/link/begin не
        // читальна й не нейтральна). Тобто будь-який клієнт міг натисканням
        // однієї кнопки в циклі і вантажити бота, і тримати кеш новин та
        // розмов усього сервера порожнім. Людина, яка справді йде в Discord,
        // десяти секунд не помічає.
        //
        // Той, хто вже чекає, теж отримує відмову: код йому видано, і другий
        // не додає нічого, крім другого рядка в базі бота.
        int now = GetGame().GetTime();
        int began;
        if (s_Waiting.Contains(uid) || (s_BeganAt.Find(uid, began) && (now - began) < BEGIN_GAP_MS))
        {
            OZ_Log.Dbg("link: " + uid + " asked for a code again too soon");
            OZ_Rpc.LinkRespond(who, OZ_LinkConst.OP_BEGIN, false, "", "STR_OZ_ERR_SLOW_DOWN");
            return;
        }
        s_BeganAt.Set(uid, now);

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

    // ГРАВЕЦЬ ЗАЙШОВ. Кличе OZ_Module.OnInvokeConnect.
    //
    // Два різні обов'язки в одному місці:
    //
    //   D33 (R-F2.4) -- ЗВІРКА НА ВХОДІ. Прив'язка, що сталась поза
    //   десятихвилинним вікном (гравець набрав /link через годину після
    //   того, як вийшов), не записувалась НІКОЛИ: у пам'яті сервера вже не
    //   було кого чекати. Watch() ставить його в чергу опиту стану, і
    //   найближчий тік запитає міст -- тобто вхід і є та сверка.
    //
    //   R-F2.2 -- СТРОК. Хто не прив'язався за GATE_GRACE_MS, того кикаємо.
    static void OnConnect(PlayerIdentity who)
    {
        if (!who)
            return;

        string uid = who.GetPlainId();
        if (uid == "")
            return;

        OZ_Settings s = OZ_Settings.Get();
        if (!s || !s.RequireDiscordLink)
            return;
        if (IsLinked(uid))
            return;

        // Звірка з мостом: він міг записати прив'язку, поки нас тут не було.
        // ОДИН запит, а не десятихвилинне чекання: чекають того, хто щойно
        // взяв код, а тут ми лише питаємо, чи вже не прив'язаний.
        AskStatus(uid);

        int grace = GATE_GRACE_MS / 1000;
        s_KickAt.Set(uid, GetGame().GetTime() + GATE_GRACE_MS);
        EnsureTimer();
        OZ_Log.Dbg("link: " + uid + " has " + grace.ToString() + "s to link or leave");
    }

    // Спитати міст, чи цей uid уже прив'язаний. Відповідь нікому не йде: вона
    // лягає у файл (OZ_LinkStatusReply -> Confirm), а клієнт побачить зміну
    // своїм же запитом стану.
    private static void AskStatus(string uid)
    {
        if (!OZ_BridgeClient.Alive())
            return;

        OZ_LinkAsk a = new OZ_LinkAsk();
        a.Uid = uid;

        string letter;
        string err;
        if (!JsonFileLoader<OZ_LinkAsk>.MakeData(a, letter, err, false))
            return;

        OZ_BridgeClient.Call("v1/link/status", letter, new OZ_LinkStatusReply(uid));
    }

    // Гравець вийшов сам -- чи ми його вивели. Прибираємо все, що про нього
    // пам'ятали.
    static void Leave(string uid)
    {
        Forget(uid);

        if (s_KickAt.Contains(uid))
            s_KickAt.Remove(uid);

        int told = s_KickTold.Find(uid);
        if (told != -1)
            s_KickTold.Remove(told);

        StopIfIdle();
    }

    static void Watch(string uid)
    {
        if (uid == "")
            return;

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
        {
            SendState(to);

            // І ПАКЕТ СИНХРОНІЗАЦІЇ ЗНОВУ (ТЗ-5 R-C1.3). Стан прив'язки в
            // ньому інакше лишався б таким, яким був на вході: усе, що читає
            // OZ_ClientState.Get().Linked, а не відповідь вікна прив'язки,
            // жило б у минулому до перезаходу.
            OZ_SyncSender.Send(to, "after link");
        }
    }

    // Кличеться і з Confirm, і з дисконекту в OZ_Module: гравець, що вийшов,
    // ще десять хвилин генерував HTTP v1/link/status раз на п'ять секунд про
    // нікого.
    static void Forget(string uid)
    {
        if (!s_Waiting.Contains(uid))
            return;

        s_Waiting.Remove(uid);
        StopIfIdle();
    }

    // Чи є кому тікати. Дві черги: хто чекає код і хто чекає рішення.
    private static int Pending()
    {
        return s_Waiting.Count() + s_KickAt.Count();
    }

    private static void EnsureTimer()
    {
        if (s_Timer)
        {
            // ТАЙМЕР, ЩО ЙДЕ, НЕ ПЕРЕЗАПУСКАЄМО.
            //
            // Timer.Run кличе OnStart, а той ставить m_time = 0
            // (3_Game/tools/tools.c:340-346) -- тобто кожен Run скидає відлік
            // до нуля. EnsureTimer кличеться з OnConnect, тобто з КОЖНОГО
            // входу неприв'язаного гравця; двоє таких, що перезаходять
            // частіше ніж раз на п'ять секунд, тримали відлік на нулі вічно,
            // і Tick() не спрацьовував ЖОДНОГО разу. А в Tick() і ворота
            // (Gate), і звірка з мостом -- тобто кік, заради якого все це
            // писалось, переставав працювати саме від того трафіку, який сам
            // і породжує: кикнутий гравець одразу заходить назад.
            //
            // Другий Run нічого й не давав: SetRunning (:351-375) відмовляє
            // повторному вставлянню в чергу.
            if (!s_Timer.IsRunning())
                s_Timer.Run(POLL_SECONDS, s_Ticker, "OZ_LinkTick", NULL, true);
            return;
        }

        // Носія тримаємо ЖИВИМ у статичному полі: таймер зберігає слабке
        // посилання, і локальний примірник прибрався б одразу після виходу з
        // методу, а таймер тікав би в порожнечу.
        s_Ticker = new OZ_LinkTicker();

        // П'ЯТЬ СЕКУНД САМИМ ТАЙМЕРОМ. Тікало раз на секунду, і перший рядок
        // тіла троттлив себе назад до п'яти вручну через s_NextAt -- тобто
        // чотири з п'яти викликів існували, щоб одразу вийти.
        s_Timer = new Timer(CALL_CATEGORY_SYSTEM);
        s_Timer.Run(POLL_SECONDS, s_Ticker, "OZ_LinkTick", NULL, true);
    }

    // Нема кого чекати -- нема чого тікати. Таймер інакше жив до кінця місії
    // після першого ж запиту коду за весь сеанс.
    private static void StopIfIdle()
    {
        if (Pending() > 0)
            return;
        if (!s_Timer)
            return;

        s_Timer.Stop();
    }

    static void Tick()
    {
        // Ворота дивимось ЗАВЖДИ, навіть коли міст мовчить: рішення про кік
        // спирається на Gated(), а той сам ураховує і мертвий міст, і
        // AllowPlayWhenBridgeDown.
        Gate();

        if (s_Waiting.Count() == 0)
        {
            StopIfIdle();
            return;
        }
        if (!OZ_BridgeClient.Alive())
            return;

        int now = GetGame().GetTime();

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

            // Вийшов -- більше не питаємо. Дисконект зве Forget сам, але
            // сюди можна прийти й між подіями.
            if (!Online(uid))
            {
                expired.Insert(uid);
                continue;
            }

            AskStatus(uid);
        }

        for (int j = 0; j < expired.Count(); j++)
        {
            OZ_Log.Dbg("link: stopped waiting for " + expired[j]);
            s_Waiting.Remove(expired[j]);
        }

        StopIfIdle();
    }

    // Строк воріт. Один прохід: кому вже нема чого чекати -- зняти, кому
    // вийшов час -- сказати, кому вже сказали -- вивести.
    private static void Gate()
    {
        if (s_KickAt.Count() == 0)
            return;

        int now = GetGame().GetTime();

        array<string> done = new array<string>();
        array<string> kick = new array<string>();

        for (int i = 0; i < s_KickAt.Count(); i++)
        {
            string uid = s_KickAt.GetKey(i);

            // Вийшов сам, прив'язався, або ворота перестали триматись
            // (дзеркал нема, міст ліг при AllowPlayWhenBridgeDown) -- усе це
            // однаково означає «більше не наша справа».
            if (!Online(uid) || !Gated(uid))
            {
                done.Insert(uid);
                continue;
            }

            if (now < s_KickAt.GetElement(i))
                continue;

            kick.Insert(uid);
        }

        for (int d = 0; d < done.Count(); d++)
            Leave(done[d]);

        for (int k = 0; k < kick.Count(); k++)
        {
            string ku = kick[k];
            PlayerIdentity to = Online(ku);
            if (!to)
            {
                Leave(ku);
                continue;
            }

            if (s_KickTold.Find(ku) == -1)
            {
                // Причина -- у вікно воріт, яке зараз перед ним: рушій свого
                // тексту при розриві не показує, і це найближче до «внятної
                // причини», що взагалі є. Кік -- наступним тіком.
                s_KickTold.Insert(ku);
                OZ_Rpc.LinkRespond(to, OZ_LinkConst.OP_BEGIN, false, "", "STR_OZ_KICK_NO_LINK");
                OZ_Log.Info("link: " + ku + " did not link in time - telling him, kicking on the next tick");
                continue;
            }

            OZ_Log.Info("link: kicking " + ku + " - RequireDiscordLink is on and he is not linked");
            Leave(ku);
            GetGame().DisconnectPlayer(to, ku);
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
