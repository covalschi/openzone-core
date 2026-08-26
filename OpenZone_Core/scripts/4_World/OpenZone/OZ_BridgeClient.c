// Клієнт моста OpenZone.
//
// СХЕМА, коротко:
//   гра -> міст   RestContext.POST -- вихідний запит, звичайний
//   міст -> гра   ДОВГИЙ опит: гра питає, міст ТРИМАЄ відповідь, поки не
//                 з'явиться що сказати (менше 10 с -- див. Start()), тоді
//                 віддає пачку; гра одразу перепитує.
//
// Довгий опит потрібен, бо в гри НЕМАЄ вхідних з'єднань: сервер DayZ нікого
// не слухає, і Discord не може постукати в гру. Затримка при цьому майже
// нульова, а не «раз на п'ять секунд»: коли Discord віддає рядок, міст
// відповідає тієї ж миті, не дочікуючись кінця тримання.
//
// ВСЕ асинхронне, і це не смак. Синхронний POST_now існує, але він зупиняє
// сервер на час подорожі до Discord і назад -- тобто на сотні мілісекунд у
// кращому випадку й на весь таймаут у гіршому. Заради однієї сторінки КПК
// морозити всіх, хто зараз у Зоні, не можна.
//
// Назовні ходить ТІЛЬКИ сервер. Ігровий клієнт секрета не бачить ніколи.

// Один політ туди й назад. Тримає свою відповідь живою, поки та не приїде.
class OZ_BridgeXfer : RestCallback
{
    // Дорога й тіло лежать ТУТ, а не в локальних змінних викликача: запит
    // асинхронний, і рядки мусять пережити повернення з Fly().
    string Route;
    string Body;

    private ref OZ_BridgeReply m_Reply;
    private bool m_Done;

    void OZ_BridgeXfer(string route, string body, OZ_BridgeReply reply)
    {
        Route   = route;
        Body    = body;
        m_Reply = reply;
        m_Done  = false;
    }

    bool IsDone()
    {
        return m_Done;
    }

    // OnError рушій кличе КІЛЬКА разів на один запит, якщо RetryCount > 1 --
    // так написано в його ж документації. Без цієї засувки один невдалий
    // опит породив би два наступні, потім чотири, і далі за прогресією.
    private bool Claim()
    {
        if (m_Done)
            return false;
        m_Done = true;
        return true;
    }

    override void OnSuccess(string data, int dataSize)
    {
        if (!Claim())
            return;

        // Print не виводить рядок довший за 1024 байти (сказано в самій
        // ваніль-документації RestCallback), тож сире тіло сюди не пишемо
        // ніколи -- лише розмір.
        OZ_Log.Dbg("bridge: " + Route + " ok, " + dataSize.ToString() + " b");

        if (m_Reply)
            m_Reply.OnBody(data);
    }

    override void OnError(int errorCode)
    {
        if (!Claim())
            return;

        // Вичерпаний таймаут читання приходить СЮДИ, кодом EREST_ERROR_TIMEOUT,
        // а не в OnTimeout -- зміряно на стенді. Для довгого опиту це звичайна
        // тиша, тож ні попередження, ні відкоту вона не варта: інакше кожен
        // спокійний проміжок виглядав би збоєм.
        if (errorCode == ERestResultState.EREST_ERROR_TIMEOUT)
        {
            OZ_Log.Dbg("bridge: " + Route + " went quiet");

            if (m_Reply)
                m_Reply.OnQuiet();
            return;
        }

        OZ_Log.Warn("bridge: " + Route + " failed, code " + errorCode.ToString());

        if (m_Reply)
            m_Reply.OnFail(errorCode);
    }

    override void OnTimeout()
    {
        if (!Claim())
            return;

        OZ_Log.Dbg("bridge: " + Route + " went quiet");

        if (m_Reply)
            m_Reply.OnQuiet();
    }
}

// Опит сам себе й перезапускає: пачка приїхала -- питаємо далі, тиша --
// питаємо далі, помилка -- питаємо далі, але не зразу.
class OZ_BridgePollReply : OZ_BridgeReply
{
    override void OnBody(string json)
    {
        // Міст відповів -- отже почув і про те, що ми свіжопіднялися.
        OZ_BridgeClient.Settled();
        OZ_BridgeClient.Absorb(json);
        OZ_BridgeClient.Again(0);
    }

    override void OnQuiet()
    {
        // Мостові не було чого сказати за цілий таймаут. Це і є звичайний хід
        // речей, а не збій: питаємо знову негайно.
        //
        // Тиша теж означає, що міст нас почув, зокрема й про свіжий запуск.
        OZ_BridgeClient.Settled();
        OZ_BridgeClient.Again(0);
    }

    override void OnFail(int code)
    {
        OZ_BridgeClient.Again(OZ_BridgeClient.BACKOFF_MS);
    }
}

// CallLaterByName хоче ЖИВИЙ об'єкт і назву методу. Статичний клас об'єктом
// не є, тож ось він -- уся його робота в одному рядку.
class OZ_BridgePump
{
    void Tick()
    {
        OZ_BridgeClient.Poll();
    }
}

class OZ_BridgeClient
{
    static const int BACKOFF_MS = 5000;

    private static bool s_Running = false;

    // Перший опит після Start() каже мостові, що ми нічого не пам'ятаємо.
    private static bool s_Fresh = true;

    // Коли пішов останній опит -- для підлоги в Again().
    private static int s_LastPollAt = 0;

    // Мінімальний проміжок між опитами. Не пауза, а запобіжник: див. Again().
    static const int MIN_GAP_MS = 250;
    private static int  s_Cursor  = 0;

    private static ref array<ref OZ_BridgeXfer> s_InFlight;
    private static ref map<string, ref OZ_BridgeSink> s_Sinks;
    private static ref OZ_BridgePollReply s_PollReply;
    private static ref OZ_BridgePump s_Pump;

    // Контекст беремо ОДИН раз і тримаємо, а не питаємо на кожен запит.
    //
    // Зміряно на стенді: інакше гра відкриває сокет і не надсилає в нього
    // нічого -- запит помирає разом із контекстом, а колбек отримує
    // EREST_ERROR_APPERROR (код 8) за мілісекунди. Приклад у
    // ваніль-документації тримає контекст у локальній змінній і тим ховає
    // вимогу.
    //
    // БЕЗ ref: у RestContext закритий деструктор, і оголошення з ref не
    // компілюється зовсім («Method '~RestContext' is private»). Володіє ним
    // сам RestApi -- він і віддає той самий контекст на ту саму адресу.
    private static RestContext s_Ctx;


    static bool IsRunning()
    {
        return s_Running;
    }

    static int Cursor()
    {
        return s_Cursor;
    }

    // Хто читатиме листи цього роду. Кличеться до Start(): підписка після
    // першої пачки означала б, що ту пачку ніхто не почув.
    static void Subscribe(string kind, OZ_BridgeSink sink)
    {
        if (!s_Sinks)
            s_Sinks = new map<string, ref OZ_BridgeSink>();

        s_Sinks.Set(kind, sink);
        OZ_Log.Dbg("bridge: sink for \"" + kind + "\"");
    }

    static void Start()
    {
        OZ_BridgeSettings b = OZ_Settings.Get().Bridge;

        if (!b.Enabled)
        {
            OZ_Log.Info("bridge: disabled");
            return;
        }

        // Ставимо обидва таймаути -- і одразу кажемо, що рушій на них не
        // зважає. Це найдорожча знахідка всього моста.
        //
        // Зміряно на 1.29, і на diag-, і на релізному сервері: АСИНХРОННИЙ
        // запит помирає рівно на десятій секунді з кодом 8, хоч би що тут
        // стояло, тимчасом як синхронний POST_now на тій самій адресі
        // спокійно дочікується двадцять п'ятої. Задокументований діапазон
        // 3..120 с описує, виходить, лише блокувальний виклик.
        //
        // Тому стеля не тут, а на тому боці: міст МУСИТЬ тримати відповідь
        // менше десяти секунд. Виглядало це інакше й дуже оманливо -- ніби
        // запит узагалі не надсилається, бо міст відповідав через 25 с у
        // сокет, якого вже ніхто не слухав.
        //
        // Виклики лишаємо: вони нічого не коштують, а на іншому рушії
        // можуть і спрацювати.
        GetRestApi().SetOption(ERestOption.ERESTOPTION_CONNECTION,    b.PollTimeoutSec);
        GetRestApi().SetOption(ERestOption.ERESTOPTION_READOPERATION, b.PollTimeoutSec);

        s_Ctx = GetRestApi().GetRestContext(Base());
        s_Ctx.SetHeader("application/json");

        s_InFlight  = new array<ref OZ_BridgeXfer>();
        s_PollReply = new OZ_BridgePollReply();
        s_Pump      = new OZ_BridgePump();
        s_Running   = true;
        s_Fresh     = true;

        string line = "bridge: polling " + b.Url;
        line += " as \"" + b.ServerId;
        line += "\", waiting up to " + b.PollTimeoutSec.ToString() + "s";
        OZ_Log.Info(line);

        Poll();
    }


    static void Stop()
    {
        s_Running = false;

        if (s_Pump)
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).RemoveByName(s_Pump, "Tick");

        s_InFlight  = NULL;
        s_PollReply = NULL;
        s_Pump      = NULL;
        s_Ctx       = NULL;
    }

    // Наступний опит через delay мілісекунд. Нуль -- у наступному кадрі, а не
    // тут-таки: перепитувати зсередини колбека того самого запиту означає
    // будувати стек із опитів.
    static void Again(int delay)
    {
        if (!s_Running || !s_Pump)
            return;

        // ПІДЛОГА НА ТЕМП, і вона тут не про ввічливість.
        //
        // Темп опиту задає МІСТ тим, що тримає відповідь. Поки він тримає,
        // Again(0) означає «раз на вісім секунд» і все гаразд. Але щойно міст
        // починає відповідати миттєво, Again(0) означає «щокадру» -- і опит
        // перетворюється на цикл без пауз.
        //
        // Виміряно на стенді: 5020 опитів за п'ять хвилин, тобто СІМНАДЦЯТЬ
        // на секунду, кожен із розбором JSON на ігровому потоці. І сервер
        // DayZ має рівно одне ядро.
        //
        // Причину усунуто на боці моста, але лишати темп цілком на розсуд
        // чужої сторони не можна: будь-який міст -- свій зламаний, чужий,
        // старої версії -- не мусить мати змоги розкрутити цей цикл. Чверть
        // секунди невидима для чату й обмежує найгірший випадок четвіркою
        // запитів на секунду замість сімнадцяти.
        if (delay < MIN_GAP_MS)
        {
            int since = GetGame().GetTime() - s_LastPollAt;
            if (since < MIN_GAP_MS)
                delay = MIN_GAP_MS - since;
        }

        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLaterByName(s_Pump, "Tick", delay, false);
    }

    // Міст відповів -- прапорець свіжого запуску більше не потрібен.
    //
    // Знімаємо ЛИШЕ після відповіді, а не одразу після надсилання: опит, що
    // не доїхав, лишає нас без проекції ролей назавжди, якщо прапорець уже
    // зняли.
    static void Settled()
    {
        s_Fresh = false;
    }

    static void Poll()
    {
        if (!s_Running)
            return;

        OZ_BridgeSettings b = OZ_Settings.Get().Bridge;

        OZ_BridgePoll p = new OZ_BridgePoll();
        p.Secret   = b.Secret;
        p.ServerId = b.ServerId;
        p.Cursor   = s_Cursor;
        p.Fresh    = s_Fresh;
        FillOnline(p.Uids);

        s_LastPollAt = GetGame().GetTime();

        string json;
        string err;
        if (!JsonFileLoader<OZ_BridgePoll>.MakeData(p, json, err, false))
        {
            OZ_Log.Error("bridge: cannot build the poll: " + err);
            Again(BACKOFF_MS);
            return;
        }

        Fly("v1/poll", json, s_PollReply);
    }

    // Питаємо лише про тих, хто зараз у Зоні. Міст тоді не тримає напоготові
    // розмови всіх, хто колись заходив, а сервер не отримує адресованого
    // нікому.
    private static void FillOnline(array<string> uids)
    {
        array<Man> players = new array<Man>();
        GetGame().GetPlayers(players);

        for (int i = 0; i < players.Count(); i++)
        {
            Man m = players[i];
            if (!m)
                continue;

            PlayerIdentity id = m.GetIdentity();
            if (!id)
                continue;

            uids.Insert(id.GetPlainId());
        }
    }

    // Вихідний лист сторінки. reply може бути порожнім -- тоді відповідь
    // просто нікому не потрібна.
    static void Call(string route, string letter, OZ_BridgeReply reply)
    {
        if (!s_Running)
        {
            if (reply)
                reply.OnFail(0);
            return;
        }

        OZ_BridgeSettings b = OZ_Settings.Get().Bridge;

        OZ_BridgeCall c = new OZ_BridgeCall();
        c.Secret   = b.Secret;
        c.ServerId = b.ServerId;
        c.Json     = letter;

        string json;
        string err;
        if (!JsonFileLoader<OZ_BridgeCall>.MakeData(c, json, err, false))
        {
            OZ_Log.Error("bridge: cannot build " + route + ": " + err);
            if (reply)
                reply.OnFail(0);
            return;
        }

        Fly(route, json, reply);
    }

    private static void Fly(string route, string json, OZ_BridgeReply reply)
    {
        Sweep();

        OZ_BridgeXfer x = new OZ_BridgeXfer(route, json, reply);
        s_InFlight.Insert(x);

        s_Ctx.POST(x, x.Route, x.Body);
    }

    // Прибирає завершені перельоти -- але НЕ зсередини їхнього ж колбека.
    // Скинути там останнє посилання означало б знищити об'єкт посеред його
    // власного методу.
    private static void Sweep()
    {
        for (int i = s_InFlight.Count() - 1; i >= 0; i--)
        {
            if (s_InFlight[i].IsDone())
                s_InFlight.Remove(i);
        }
    }

    // Рушій склеює адресу контексту з дорогою запиту без роздільника, тож
    // коса лишається за нами.
    private static string Base()
    {
        string u = OZ_Settings.Get().Bridge.Url;
        if (u.Length() > 0 && u.Substring(u.Length() - 1, 1) != "/")
            u += "/";
        return u;
    }

    // Пачка з моста. Ядро розкриває конверти й роздає їх за родом -- і на
    // цьому його знання про вміст закінчується.
    static void Absorb(string json)
    {
        OZ_BridgeBatch batch;
        string err;
        if (!JsonFileLoader<OZ_BridgeBatch>.LoadData(json, batch, err))
        {
            OZ_Log.Error("bridge: batch is not readable: " + err);
            return;
        }

        if (!batch)
            return;

        s_Cursor = batch.Cursor;

        for (int i = 0; batch.Items && i < batch.Items.Count(); i++)
        {
            OZ_BridgeEnvelope e = batch.Items[i];
            if (!e)
                continue;

            OZ_BridgeSink sink;
            if (!s_Sinks || !s_Sinks.Find(e.Kind, sink) || !sink)
            {
                OZ_Log.Dbg("bridge: nobody reads \"" + e.Kind + "\"");
                continue;
            }

            sink.Deliver(e.Json);
        }
    }
}
