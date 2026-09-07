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

        // ОДИН РЯДОК НА ПЕРЕХІД, а не на кожну спробу.
        //
        // Мертвий міст писав WARNING кожні п'ять секунд -- до семисот рядків
        // за годину, -- і вердикт стенду (він рахує WARNING) залипав на
        // «негідно» через сам лише вимкнений бот, ховаючи справжні
        // попередження. Про те, що міст ліг, треба сказати ОДИН раз, і ще
        // раз -- коли він повернувся.
        OZ_BridgeClient.Fell(Route, errorCode);

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

        // Пачка, яка нічого не привезла й не зрушила курсор, означає, що міст
        // відповів МИТТЄВО й ні про що. Перепитувати таке в наступному кадрі
        // -- це цикл без пауз; чекаємо секунду. Пачка з вмістом означає, що
        // розмова триває, і там пауза не потрібна взагалі.
        if (OZ_BridgeClient.Absorb(json))
        {
            OZ_BridgeClient.Again(OZ_BridgeClient.IDLE_GAP_MS);
            return;
        }

        OZ_BridgeClient.Again(0);
    }

    override void OnQuiet()
    {
        // ТИША -- НЕ ВІДПОВІДЬ, і саме тому вона більше не кличе Settled().
        //
        // Утримання моста -- вісім секунд, рушій кидає запит на десятій, тож
        // при живому мості сюди не заходять узагалі: він відповідає раніше.
        // Тиша означає, що з'єднання прийняли й не відповіли, -- зависле
        // node, DROP на порту, чужий міст із утриманням понад десять секунд.
        // Settled() освіжав s_LastOkAt, тобто Alive() тримався на самих лише
        // НЕВДАЧАХ і не ставав false ніколи. Наслідок був не косметичний:
        // AllowPlayWhenBridgeDown не вмикався, а ворота, отримавши кік
        // (99ea53a), виводили кожного неприв'язаного за GATE_GRACE_MS --
        // тимчасом як v1/link/begin не міг видати йому код, за відсутність
        // якого його й виводять.
        //
        // Питаємо знову негайно: сам таймаут уже й був паузою, а темп при
        // цьому задає рушій -- один запит на десять секунд.
        OZ_BridgeClient.Quiet();
        OZ_BridgeClient.Again(0);
    }

    override void OnFail(int code)
    {
        OZ_BridgeClient.Again(OZ_BridgeClient.Backoff());
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

// Кого ще вважати «онлайном» для моста, крім самих гравців. Наповнює мод
// пристроїв: захоплений живий термінал говорить за свого власника й тоді,
// коли самого власника в Зоні немає, -- і його акаунт мусить дренуватись,
// інакше тримач не побачить ехо власних відправлень. Ядро пристроїв не
// знає, воно лише тримає гачок.
class OZ_BridgeUidProvider
{
    void Fill(array<string> uids) {}
}

// ЧИ Є В МОСТА БОТ -- питання окремим запитом (ТЗ-2 R2.6).
//
// /v1/ping -- єдина дорога моста без секрета й без стану: вона існує рівно
// щоб довести, що гра до нього дістає. Відтепер вона несе ще й `discord`, і
// цим одним полем гра розрізняє «міст лежить» (жодної відповіді) і «міст
// живий, але Discord у нього не налаштований». Друге -- не збій: без бота
// міст обслуговує чат, новини, фракції і вайп зі своєї бази, просто в
// гільдії нікого немає.
//
// Це GET, а не POST, тому й колбек свій: OZ_BridgeXfer тримає тіло запиту,
// якого тут немає зовсім.
class OZ_BridgePong
{
    bool ok      = false;
    bool discord = false;
}

class OZ_BridgePing : RestCallback
{
    override void OnSuccess(string data, int dataSize)
    {
        OZ_BridgeClient.Pinged(data);
    }

    override void OnError(int errorCode)
    {
        // Мовчки: про мертвий міст скаже опит, а гадати про бота по
        // невдалому пінгу -- це вигадати стан, якого ми не бачили.
        OZ_Log.Dbg("bridge: ping failed, code " + errorCode.ToString());
    }

    override void OnTimeout()
    {
        OZ_Log.Dbg("bridge: ping went quiet");
    }
}

class OZ_BridgeClient
{
    static const int BACKOFF_MS = 5000;

    // Скільки чекати після НЕВДАЛОГО опиту. Росте вдвічі на кожній наступній
    // невдачі до хвилини й падає назад, щойно міст відповів: п'ять секунд
    // назавжди означали і сталий потік невдалих запитів, і сталий потік
    // рядків у лозі про сервіс, якого зараз просто немає.
    private static int s_Backoff = BACKOFF_MS;
    private static const int BACKOFF_MAX_MS = 60000;

    // Чи вважали ми міст живим, коли говорили про це востаннє.
    private static bool s_SaidAlive = true;

    private static bool s_Running = false;

    // Перший опит після Start() каже мостові, що ми нічого не пам'ятаємо.
    private static bool s_Fresh = true;

    // Коли пішов останній опит -- для підлоги в Again().
    private static int s_LastPollAt = 0;

    // Мінімальний проміжок між опитами. Не пауза, а запобіжник: див. Again().
    static const int MIN_GAP_MS = 250;

    // Коли міст востаннє ВІДПОВІВ. Не «коли ми востаннє питали».
    private static int s_LastOkAt = 0;

    // Скільки мовчання вважати смертю. Утримання -- вісім секунд, таймаут
    // клієнта -- сорок, тож хвилина означає «не відповів жодного разу за
    // кілька спроб поспіль», а не «затримався».
    private static const int DEAD_MS = 60000;
    private static int  s_Cursor  = 0;

    private static ref array<ref OZ_BridgeXfer> s_InFlight;
    private static ref map<string, ref OZ_BridgeSink> s_Sinks = new map<string, ref OZ_BridgeSink>();
    private static ref OZ_BridgePollReply s_PollReply;
    private static ref OZ_BridgePump s_Pump;

    // Відповідь /v1/ping. Тримаємо в сильному посиланні: рушій кличе колбек
    // пізніше, ніж повертається GET, і зібраний колбек нікуди не приїде.
    private static ref OZ_BridgePing s_Ping;

    // ЩО МИ ЗНАЄМО ПРО БОТА НА ТОМУ БОЦІ (ТЗ-2 R2.6).
    //
    // Три стани, а не два: «не питали», «є бот», «бота немає». Поки не
    // питали, поводимось як раніше -- інакше кожен бут між Start() і
    // відповіддю читався б як «Discord вимкнено», а це рівно та мовчазна
    // різниця, яку R2.6 і прибирає.
    private static bool s_DiscordKnown = false;
    private static bool s_Discord      = true;

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


    // IsRunning() І Cursor() ТУТ БІЛЬШЕ НЕМАЄ. Обидва не мали викликачів у
    // жодному репозиторії серії, а кожна згадка IsRunning у сусідніх файлах
    // була коментарем «не питай це, питай Alive()»: s_Running означає лише
    // «Start() відпрацював», тобто дублює Bridge.Enabled. Курсор -- приватний
    // стан опиту, який опит і возить.

    // Хто читатиме листи цього роду. Кличеться до Start(): підписка після
    // першої пачки означала б, що ту пачку ніхто не почув.
    //
    // ПІДПИСКУ БІЛЬШЕ НІЩО НЕ ФІЛЬТРУЄ (ТЗ-5 R-C1.4). Тут стояла перевірка по
    // Bridge.Kinds -- другий рубильник із майже тим самим ім'ям, що й
    // Bridge.Mirrors, і з іншим змістом. Рід возиться тому, що його оголосив
    // мод; чи видно його в гільдії, вирішує Mirrors, і рубильник на це один.
    static void Subscribe(string kind, OZ_BridgeSink sink)
    {
        if (kind == "" || !sink)
            return;

        s_Sinks.Set(kind, sink);

        // Таблиця доріг збирається з підписок, тож нова підписка робить її
        // застарілою. Скидаємо, а не доповнюємо: доповнення мусило б знати
        // про другий прохід (застарювання чужих родів) і повторювати його.
        s_RouteRole = NULL;
        s_Stale     = NULL;

        OZ_Log.Dbg("bridge: sink for \"" + kind + "\"");
    }

    // ---------------------------------------------------- ДОРОГИ РОДІВ
    //
    // «Читальна / нейтральна / записувальна» і «що застаріває від конверта
    // цього роду» -- дві таблиці, зібрані з ТОГО, ЩО ОГОЛОСИЛИ САМІ МОДИ
    // (OZ_BridgeSink.Reads/Neutral/Stales). У ядрі тут стояли їхні імена
    // списком -- "chat", "news", "roles", "roster", "wipe" -- усупереч
    // дизайну платформи §4; третій мод зі своїм родом не мав жодного способу
    // потрапити в цей список, і кожен його конверт скидав чужий кеш цілком.
    //
    // Ліниво й зі скиданням на підписці: підписки приходять з OnInit різних
    // модів, і моменту «всі вже підписались» ядро не знає.
    static const int ROUTE_WRITE   = 0;
    static const int ROUTE_READ    = 1;
    static const int ROUTE_NEUTRAL = 2;

    private static ref map<string, int> s_RouteRole;
    private static ref map<string, ref array<string>> s_Stale;

    private static void BuildRoutes()
    {
        if (s_RouteRole)
            return;

        s_RouteRole = new map<string, int>();
        s_Stale     = new map<string, ref array<string>>();

        // ВЛАСНІ ДОРОГИ ЯДРА -- єдині імена, які воно має право знати.
        // Прив'язка нічого не міняє в листуванні: і статус, і запит коду --
        // питання про одну людину й одну мить, а не про світ. Без цього
        // кожне натискання «отримати код» гасило чат і новини всьому серверу.
        s_RouteRole.Set("v1/link/status", ROUTE_NEUTRAL);
        s_RouteRole.Set("v1/link/begin",  ROUTE_NEUTRAL);

        // Голоси новин -- ТЕЖ ВЛАСНА дорога ядра, і саме тому вона тут.
        // Питання про ПРАВА на мить («від чийого імені я можу писати»), не
        // про стрічку: кешувати нема чого, але й гасити кешовану стрічку
        // воно не мусить -- сторінка просить список і голоси разом.
        // Кличе її адмінська новинна форма самого ядра (OZ_NewsAdmin.Ask),
        // тобто й на сервері, де КПК не встановлено; без цього рядка кожне
        // «хто говорить» у панелі VPP такого сервера рахувалося б записом і
        // гасило кеш цілком.
        s_RouteRole.Set("v1/news/voices", ROUTE_NEUTRAL);

        // РЕШТА ВЛАСНИХ ДОРІГ ЯДРА -- ЗАПИС, і це свідомо, а не забуто:
        //   v1/news/post   -- ядро пише новину (OZ_NewsAdmin); світ після неї
        //                     інший, і скидання кеша цілком застарює й
        //                     стрічку роду "news" разом з усім іншим;
        //   v1/mirror/fill -- заливка історії роду в гільдію (OZ_MirrorOps);
        //   v1/poll        -- сам опит, і він єдиний тут не ходить через
        //                     Call() зовсім: Fly() кличеться прямо, тож кеша
        //                     не питає й не гасить із жодного боку.
        // Незнайома дорога і так вважається записом, тож рядків для них
        // немає -- є ця табличка, щоб наступний читач не виводив це наново.

        int i;
        int j;

        for (i = 0; i < s_Sinks.Count(); i++)
        {
            OZ_BridgeSink sink = s_Sinks.GetElement(i);
            if (!sink)
                continue;

            array<string> reads = new array<string>();
            sink.Reads(reads);

            // ЧИТАЛЬНА ДОРОГА КЕШУЄТЬСЯ ЛИШЕ ВІД РОДУ, ЯКИЙ СКАЗАВ, ЯК ВІН
            // ЗАСТАРІВАЄ (шапка OZ_BridgeSink).
            //
            // Reads() без FollowsCursor() і без Stales() -- половина
            // контракту: кеш тримав би відповідь, якої не гасить ані зсув
            // курсора, ані чужий рід, і сторінка бачила б учорашній список
            // до кінця TTL_MS. Нейтральна роль -- це «не кешуємо, але й
            // чужого не гасимо»: повільніше й ніколи не хибно, тобто той
            // самий вибір, який ядро робить для будь-якого мовчазного роду.
            array<string> stales = new array<string>();
            sink.Stales(stales);

            bool ages = sink.FollowsCursor();
            if (stales.Count() > 0)
                ages = true;

            int role = ROUTE_NEUTRAL;
            if (ages)
                role = ROUTE_READ;

            for (j = 0; j < reads.Count(); j++)
                s_RouteRole.Set(reads[j], role);

            if (!ages && reads.Count() > 0)
                OZ_Log.Dbg("bridge: \"" + s_Sinks.GetKey(i) + "\" reads are not cached - the sink declares Reads() but neither FollowsCursor() nor Stales(), so nothing says when its answers go stale");

            // Рід застарює ВЛАСНІ читальні дороги -- це і є звичайний
            // випадок, який раніше вгадувався префіксом "v1/<рід>/".
            s_Stale.Set(s_Sinks.GetKey(i), reads);

            array<string> quiet = new array<string>();
            sink.Neutral(quiet);
            for (j = 0; j < quiet.Count(); j++)
            {
                // Читальна дорога сильніша за нейтральну: якщо мод назвав
                // її двічі, кеш має право її тримати.
                if (!s_RouteRole.Contains(quiet[j]))
                    s_RouteRole.Set(quiet[j], ROUTE_NEUTRAL);
            }
        }

        // ДРУГИМ ПРОХОДОМ -- винятки. Рід, який переписує чуже (вайп чистить
        // склад розмов), забирає читальні дороги того роду собі. Саме другим:
        // на першому списки інших родів ще не були відомі.
        //
        // Stales() тут питається вдруге, і це дешевше за таблицю заради
        // одного числа: метод порожній у кожного, хто мовчить, а весь цей
        // збір відбувається раз на підписку, а не на запит.
        for (i = 0; i < s_Sinks.Count(); i++)
        {
            OZ_BridgeSink other = s_Sinks.GetElement(i);
            if (!other)
                continue;

            array<string> named = new array<string>();
            other.Stales(named);
            if (named.Count() == 0)
                continue;

            array<string> mine = s_Stale.Get(s_Sinks.GetKey(i));
            if (!mine)
                continue;

            for (j = 0; j < named.Count(); j++)
            {
                array<string> theirs = s_Stale.Get(named[j]);
                if (!theirs)
                    continue;

                for (int k = 0; k < theirs.Count(); k++)
                {
                    if (mine.Find(theirs[k]) == -1)
                        mine.Insert(theirs[k]);
                }
            }
        }
    }

    // Чим ця дорога є для кеша. Незнайома -- запис: не знаємо, чого вона
    // торкнеться, і вгадувати тут не можна.
    static int RouteRole(string route)
    {
        BuildRoutes();

        int role;
        if (!s_RouteRole.Find(route, role))
            return ROUTE_WRITE;
        return role;
    }

    // Дороги, які застарює конверт цього роду -- у БУФЕР ВИКЛИКАЧА.
    //
    // Не через out-контейнер: виміряно 2026-08-02, що контейнер, повернутий
    // із функції через не-ref out, знищується, і той, хто питав, читає
    // звільнену пам'ять.
    //
    // false -- роду ніхто не оголошував: тоді викликач не вгадує й скидає все.
    static bool StaleRoutes(string kind, notnull array<string> found)
    {
        found.Clear();

        BuildRoutes();

        if (!s_Sinks.Contains(kind))
            return false;

        array<string> mine = s_Stale.Get(kind);
        if (!mine)
            return true;

        for (int i = 0; i < mine.Count(); i++)
            found.Insert(mine[i]);
        return true;
    }

    // Курсор опиту зрушив. Застаріває рід, якому той потік належить -- і
    // каже про це він сам (OZ_BridgeSink.FollowsCursor). Ядро тут писало
    // "chat" літералом: воно возить курсор і не читає його змісту.
    private static void CursorMoved(int cursor)
    {
        for (int i = 0; i < s_Sinks.Count(); i++)
        {
            OZ_BridgeSink sink = s_Sinks.GetElement(i);
            if (!sink)
                continue;
            if (!sink.FollowsCursor())
                continue;

            OZ_BridgeCache.Invalidate(s_Sinks.GetKey(i), "poll cursor " + cursor.ToString());
        }
    }

    // Речення про перемикання дзеркала цього роду -- від самого мода.
    // Рід без підписки (адмін увімкнув його колись, а мод зараз знято)
    // отримує загальне речення порожньої реалізації.
    private static ref OZ_BridgeSink s_PlainSink = new OZ_BridgeSink();

    static string MirrorNote(string kind, bool on)
    {
        OZ_BridgeSink sink;
        if (s_Sinks.Find(kind, sink) && sink)
            return sink.MirrorNote(kind, on);

        return s_PlainSink.MirrorNote(kind, on);
    }

    // Роди, які на цьому сервері хтось справді читає. Ядро їх не знає
    // наперед -- їх оголошують МОДИ підпискою, -- і саме тому список дзеркал
    // для панелі будується звідси, а не з двох літералів у ядрі.
    static void FillKinds(array<string> outKinds)
    {
        if (!outKinds)
            return;

        for (int i = 0; i < s_Sinks.Count(); i++)
            outKinds.Insert(s_Sinks.GetKey(i));
    }

    // ЧИ ВИДНО ЦЕЙ РІД У ГІЛЬДІЇ. Перше спрацьоване правило вирішує
    // (ТЗ-2 R3.2).
    //
    // Питання НЕ про те, де дані живуть: чат живе в боті хоч із дзеркалом,
    // хоч без. Це питання про поверхню -- чи виносити рід у треди Discord.
    //
    //   1. міст вимкнений цілком          -> off усім
    //   2. списку немає або він порожній  -> off усім
    //   3. є запис про цей рід            -> його Mirror
    //   4. інакше                         -> off
    //
    // Правило 2 раніше означало протилежне -- «дзеркалити все», -- і тримало
    // зворотну сумісність із живими серверами. Знято рішенням власника
    // 2026-09-01: мод тільки на дев-стенді. Умовчання «тихо» правильніше й
    // саме собою: сервер, який ще нічого не налаштував, не має починати з
    // того, що виливає переписку гравців у чужу гільдію.
    static bool Mirrored(string kind)
    {
        OZ_Settings s = OZ_Settings.Get();
        if (!s || !s.Bridge)
            return false;
        if (!s.Bridge.Enabled)
            return false;

        array<ref OZ_KindMirror> list = s.Bridge.Mirrors;
        if (!list || list.Count() == 0)
            return false;

        for (int i = 0; i < list.Count(); i++)
        {
            OZ_KindMirror m = list[i];
            if (m && m.Kind == kind)
                return m.Mirror;
        }
        return false;
    }

    // Імена ввімкнених родів -- для мосту. Той сам вирішує, що з ними
    // робити; наша справа -- чесно сказати, що дозволено показувати.
    static void FillMirrors(array<string> outKinds)
    {
        if (!outKinds)
            return;
        outKinds.Clear();

        OZ_Settings s = OZ_Settings.Get();
        if (!s || !s.Bridge || !s.Bridge.Enabled)
            return;

        array<ref OZ_KindMirror> list = s.Bridge.Mirrors;
        if (!list)
            return;

        for (int i = 0; i < list.Count(); i++)
        {
            OZ_KindMirror m = list[i];
            if (m && m.Mirror && m.Kind != "")
                outKinds.Insert(m.Kind);
        }
    }

    // Скільки родів справді дзеркаляться. Потрібно рівно там, де різниця
    // видима: «у гільдії тихо» -- це стан, про який треба сказати вголос.
    //
    // Через FillMirrors, а не власним циклом: два перебори того самого списку
    // з тими самими null-перевірками розходяться мовчки, і сказати «нуль»,
    // коли мосту їде три роди, було б найгіршою з можливих розбіжностей.
    static int MirrorCount()
    {
        array<string> t = new array<string>();
        FillMirrors(t);
        return t.Count();
    }

    // «DISCORD НЕ НАЛАШТОВАНИЙ» -- ТОЙ САМИЙ ВИПАДОК, ЩО «НУЛЬ ДЗЕРКАЛ»
    // (ТЗ-2 R2.6, останній абзац; R2.4).
    //
    // Не окрема гілка й не окреме правило: сервіс, у який ми пишемо, у
    // гільдії не з'являється ні тоді, ні тоді, а решта -- чат, новини,
    // фракції, вайп -- працює однаково. Тому питання одне: «чи є сенс
    // тримати ворота прив'язки», і відповідь на нього дає Silent().
    static bool DiscordOff()
    {
        return s_DiscordKnown && !s_Discord;
    }

    // Чи в гільдії тихо -- з будь-якої з двох причин.
    static bool Silent()
    {
        return DiscordOff() || MirrorCount() == 0;
    }

    // Відповідь на /v1/ping. Один рядок у лог на перехід, і тільки коли бота
    // немає: «бот є» -- це звичайний стан, про який казати нема чого.
    static void Pinged(string json)
    {
        // Корінь створює скрипт, а не серіалізатор (шапка OZ_ConfigBase).
        OZ_BridgePong p = new OZ_BridgePong();
        string err;
        if (!JsonFileLoader<OZ_BridgePong>.LoadData(json, p, err) || !p)
            return;

        bool was      = s_Discord;
        bool wasKnown = s_DiscordKnown;

        s_Discord      = p.discord;
        s_DiscordKnown = true;

        if (!s_Discord && (!wasKnown || was))
            OZ_Log.Info("bridge: Discord is not configured on the bridge - the bot works, the guild stays quiet");
        else if (s_Discord && wasKnown && !was)
            OZ_Log.Info("bridge: the bridge has a Discord bot again");
    }

    // Спитати міст, чи є в нього бот. Кличеться на буті й ще раз щоразу, як
    // міст повертається до життя: міст могли перезапустити з токеном.
    static void Ping()
    {
        if (!s_Running || !s_Ctx)
            return;

        s_Ctx.GET(s_Ping, "v1/ping");
    }

    static void Start()
    {
        OZ_BridgeSettings b = OZ_Settings.Get().Bridge;

        if (!b.Enabled)
        {
            OZ_Log.Info("bridge: disabled");
            return;
        }

        // НІКОМУ ВОЗИТИ -- НЕ ЇДЕМО ВЗАГАЛІ (ТЗ-2 R2.3).
        //
        // Роди з домом «бот» оголошують МОДИ, підписуючись сюди: чат і новини
        // приносить КПК, ролі й ростер -- фракції. Жодної підписки означає
        // сервер, на якому нічого з бота не живе, -- і тоді опит раз на вісім
        // секунд возить порожнечу вічно, а в лозі копичаться v1/poll failed
        // про сервіс, якого нікому й не треба.
        //
        // Питаємо ПІСЛЯ Enabled, а не замість: адмін, який лишив Enabled: true
        // на сервері без відповідних модів, має прочитати саме це, а не
        // «disabled», якого він не писав.
        if (s_Sinks.Count() == 0)
        {
            OZ_Log.Info("bridge: nothing on this server lives in the bot - not polling at all");
            return;
        }

        // «У гільдії тихо» -- це СТАН, і його кажуть уголос один раз.
        //
        // Міст працює, база працює, гра працює, а Discord мовчить -- рівно те,
        // що означає «Discord опціональний» (ТЗ-2 §3). Мовчазна конфігурація
        // тут гірша за будь-яку іншу: адмін, який чекає тредів і не бачить їх,
        // піде шукати поламане замість того, щоб увімкнути дзеркало.
        if (MirrorCount() == 0)
            OZ_Log.Info("bridge: every mirror is off - the bot works, the guild stays quiet");

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
        // Число тут -- ЛІТЕРАЛ, і настройки під нього більше немає.
        //
        // Bridge.PollTimeoutSec був параметром, який рушій ЗМІРЯНО ігнорує:
        // асинхронний запит помирає рівно на десятій секунді, хоч би що тут
        // стояло. Настройка, яка нічого не робить, гірша за її відсутність --
        // адмін крутить її й пояснює собі наслідки, яких немає.
        GetRestApi().SetOption(ERestOption.ERESTOPTION_CONNECTION,    OZ_Const.REST_TIMEOUT_SEC);
        GetRestApi().SetOption(ERestOption.ERESTOPTION_READOPERATION, OZ_Const.REST_TIMEOUT_SEC);

        s_Ctx = GetRestApi().GetRestContext(Base());
        s_Ctx.SetHeader("application/json");

        s_InFlight  = new array<ref OZ_BridgeXfer>();
        s_PollReply = new OZ_BridgePollReply();
        s_Pump      = new OZ_BridgePump();
        s_Ping      = new OZ_BridgePing();
        s_Running   = true;
        s_Fresh     = true;
        s_Backoff   = BACKOFF_MS;
        s_SaidAlive = true;

        // Вважаємо міст живим, поки не доведено протилежне. Інакше в перші
        // секунди після старту, коли відповіді ще не було, він читався б як
        // мертвий -- а «ще не питали» і «не відповідає» це різні речі.
        s_LastOkAt  = GetGame().GetTime();

        string line = "bridge: polling " + b.Url;
        line += " as \"" + b.ServerId;
        line += "\", the engine drops an async request at " + OZ_Const.REST_TIMEOUT_SEC.ToString() + "s";
        OZ_Log.Info(line);

        // Питаємо про бота ДО першого опиту: рядок «Discord не налаштований»
        // мусить стояти поруч із рядком про дзеркала, а не через хвилину
        // після нього (ТЗ-2 R2.6).
        Ping();
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
        s_Ping      = NULL;
        s_Ctx       = NULL;

        // Про чужу конфігурацію після зупинки ми знову не знаємо нічого:
        // наступний Start() може дивитись уже на інший міст.
        s_DiscordKnown = false;
        s_Discord      = true;
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
    // зняли. Тиша (див. Quiet нижче) відповіддю не є й сюди не заходить.
    static void Settled()
    {
        s_Fresh    = false;
        s_LastOkAt = GetGame().GetTime();
        s_Backoff  = BACKOFF_MS;

        if (!s_SaidAlive)
        {
            s_SaidAlive = true;
            OZ_Log.Info("bridge: answering again");

            // Міст повернувся -- можливо, вже з токеном або вже без нього.
            // Перепитуємо саме на переході, а не за таймером: конфігурація
            // моста змінюється тільки разом із його перезапуском.
            Ping();
        }
    }

    // Опит вичерпав таймаут читання й не привіз нічого.
    //
    // Нічого не штампуємо: Alive() міряє час від ОСТАННЬОЇ ВІДПОВІДІ, і мовчазний
    // міст мусить дійти до DEAD_MS сам. Єдина робота тут -- сказати про перехід
    // один раз, бо тихий міст не заходить у Fell() і без цього рядка падав би
    // мовчки: у лозі не було б нічого, а ворота відпустили б усіх.
    static void Quiet()
    {
        if (!s_SaidAlive)
            return;
        if (Alive())
            return;

        s_SaidAlive = false;

        int dead = DEAD_MS / 1000;
        OZ_Log.Warn("bridge: no answer for " + dead.ToString() + "s - the bridge counts as down");
    }

    // Невдалий переліт. Один рядок на перехід «живий -> мертвий», далі Dbg.
    static void Fell(string route, int errorCode)
    {
        string line = "bridge: " + route + " failed, code " + errorCode.ToString();

        if (s_SaidAlive)
        {
            s_SaidAlive = false;
            OZ_Log.Warn(line);
            return;
        }

        OZ_Log.Dbg(line);
    }

    // Наступна пауза після невдачі -- і подвоєння для тієї, що буде далі.
    static int Backoff()
    {
        int now = s_Backoff;
        s_Backoff = s_Backoff * 2;
        if (s_Backoff > BACKOFF_MAX_MS)
            s_Backoff = BACKOFF_MAX_MS;
        return now;
    }

    // Чи міст ЖИВИЙ, а не «чи ми ввімкнули опит».
    //
    // IsRunning() каже лише те, що Start() відпрацював: s_Running стає true
    // один раз і падає тільки в Stop() на кінці місії. Мертвий процес моста
    // його не чіпає -- OnFail просто планує наступну спробу. Тобто IsRunning()
    // був синонімом Bridge.Enabled, і кожна перевірка «а чи є міст» насправді
    // питала «а чи ввімкнений він у налаштуваннях».
    //
    // Це робило AllowPlayWhenBridgeDown недосяжним рівно тоді, коли він
    // потрібен: бот падає -- і кожен непривязаний гравець опиняється у
    // вікні, яке не закривається, на сервері, що працює нормально.
    static bool Alive()
    {
        if (!s_Running)
            return false;
        return (GetGame().GetTime() - s_LastOkAt) < DEAD_MS;
    }

    // ПАУЗА ПІСЛЯ ПОРОЖНЬОЇ ПАЧКИ.
    //
    // Темп опиту задає міст тим, що ТРИМАЄ відповідь, і поки він тримає,
    // Again(0) означає «раз на вісім секунд». Але міст, який відповідає
    // порожнечею миттєво -- свій зламаний, чужий, старої версії, -- крутив
    // цикл на підлозі в чверть секунди, тобто чотири GetPlayers + JSON + HTTP
    // на секунду на сервері з одним ядром, вічно й ні за що. Пачка, що
    // НІЧОГО не привезла й не зрушила курсор, -- це і є «мостові нема чого
    // сказати», і чекати після неї можна секунду.
    static const int IDLE_GAP_MS = 1000;

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
        FillMirrors(p.Mirrors);
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

        for (int pi = 0; pi < s_UidProviders.Count(); pi++)
            s_UidProviders[pi].Fill(uids);
    }

    private static ref array<ref OZ_BridgeUidProvider> s_UidProviders = new array<ref OZ_BridgeUidProvider>();

    static void RegisterUidProvider(OZ_BridgeUidProvider p)
    {
        s_UidProviders.Insert(p);
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

        // Читання -- з кешу, коли він має відповідь на цей самий лист; запис
        // -- скидає кеш, бо світ за ним інший (ТЗ-2 R4.3, R4.4). Хто не
        // питає Alive() перед викликом, того сюди не пускають сторінки, тож
        // застаріла відповідь при мертвому мості звідси не піде (R4.1).
        // Обгортка -- у сильному посиланні: параметр 'reply' сильним не є, і
        // компілятор відмовляється класти в нього новий об'єкт (зміряно:
        // "Variable 'reply' is not strong ref"). Переліт тримає її сам.
        ref OZ_BridgeCacheFill wrap = null;
        if (OZ_BridgeCache.Readable(route))
        {
            string hit;
            if (OZ_BridgeCache.Get(route, letter, hit))
            {
                if (reply)
                    reply.OnBody(hit);
                return;
            }
            wrap = new OZ_BridgeCacheFill(route, letter, reply);
        }
        else if (!OZ_BridgeCache.Neutral(route))
        {
            OZ_BridgeCache.Clear("write " + route);
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

        if (wrap)
            Fly(route, json, wrap);
        else
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
    // Повертає true, коли пачка НІЧОГО не привезла й не зрушила курсор.
    static bool Absorb(string json)
    {
        // Корінь створює скрипт, а не серіалізатор (шапка OZ_ConfigBase).
        OZ_BridgeBatch parsed = new OZ_BridgeBatch();
        string err;
        if (!JsonFileLoader<OZ_BridgeBatch>.LoadData(json, parsed, err))
        {
            OZ_Log.Error("bridge: batch is not readable: " + err);
            return false;
        }

        // І одразу копія: нижче кожен sink.Deliver() розбирає власний
        // документ, а цикл після нього повертається по наступний конверт
        // ЦІЄЇ пачки. Копія робиться до першого такого розбору -- поки
        // читання ще чесне.
        OZ_BridgeBatch batch = parsed.Copy();

        // СКИДАЄМО КЕШ ПО РОДАХ, А НЕ ЦІЛКОМ (ТЗ-2 R4.4 у формі, яку вона
        // мала на увазі).
        //
        // Було: будь-яка непорожня пачка або будь-який зсув курсора чистили
        // ВЕСЬ кеш читань. Один чужий рядок у чаті викидав з нього новини й
        // розмови всіх, хто зараз у Зоні, -- тобто на живому сервері кеш
        // стояв порожній рівно тоді, коли він найпотрібніший.
        //
        // Курсор рухає ЛИШЕ один потік моста (він рахує його по
        // messagesSince), тож його зсув застарює саме той рід -- зокрема й
        // тоді, коли рядок був не для нас і в пачку не потрапив. Який це рід,
        // каже сам мод (FollowsCursor); решту називають самі конверти своїм
        // Kind.
        bool moved = batch.Cursor != s_Cursor;
        bool carried = batch.Items && batch.Items.Count() > 0;

        if (moved)
            CursorMoved(batch.Cursor);

        for (int c = 0; batch.Items && c < batch.Items.Count(); c++)
        {
            OZ_BridgeEnvelope ce = batch.Items[c];
            if (!ce)
                continue;

            // Рід, про наслідки якого ніхто не сказав -- ані підпискою, ані
            // оголошенням доріг, -- міг зачепити що завгодно; тоді скидаємо
            // все, як і раніше.
            if (!OZ_BridgeCache.Invalidate(ce.Kind, "poll item"))
                OZ_BridgeCache.Clear("poll item of \"" + ce.Kind + "\", which says nothing about what it stales");
        }

        s_Cursor = batch.Cursor;

        for (int i = 0; batch.Items && i < batch.Items.Count(); i++)
        {
            OZ_BridgeEnvelope e = batch.Items[i];
            if (!e)
                continue;

            OZ_BridgeSink sink;
            if (!s_Sinks.Find(e.Kind, sink) || !sink)
            {
                OZ_Log.Dbg("bridge: nobody reads \"" + e.Kind + "\"");
                continue;
            }

            sink.Deliver(e.Json);
        }

        return !moved && !carried;
    }
}
