// ПЕРМАДЕС: служба ядра, реєстр витирачів і вхід у консолі.
//
// Уся ця машинерія жила в @OpenZone_Factions (OZF_AdminOps.c), хоч чистила
// поля, які пише сам КПК, -- і наслідок був не косметичний: сервер core+PDA
// БЕЗ мода фракцій лишався без вайпу цілком. Кнопки в консолі немає (панель
// фракцій не завантажена), операції немає (розділ не зареєстровано), а
// найгірше -- немає ПІДПИСКИ на рід "wipe": команда `/openzone wipe` стирала
// ролі й треди в Discord, а КПК небіжчика працювали далі. Перенесення --
// рішення власника 2026-09-08 (дизайн 2026-09-08-wipe-in-core-design.md).
//
// ЧОМУ OZ_Wipe, А НЕ OZ_PlayerWipe. Старе ім'я називало рівно одну половину
// -- «ІГРОВА ПОЛОВИНА ПЕРМАДЕСУ, окремо від того, хто її попросив». Тепер
// клас володіє і просьбою, і защіпкою, і приймачем поштовху, і реєстром.
// Метод Local ім'я зберігає: у нього два викликачі, і обидва мусять читатись
// як раніше.

// ХТО ЩЕ ВИТИРАЄ. Кожен мод, який щось пише у файл гравця, стирає СВОЄ.
//
// РЕЄСТР, А НЕ СЛУЖБА З ПОРОЖНЬОЮ РЕАЛІЗАЦІЄЮ. Обидві форми платформи описані
// в 2026-09-01-openzone-core-as-platform-design.md. OZ_Identity.Provide --
// служба в однині: питальнику потрібна одна відповідь. Витирачів за
// визначенням багато, тому Register, як у OZ_AdminRegistry та OZ_PageRegistry.
//
// ДОГОВІР ВИТИРАЧА, і всі три його пункти -- наслідки, а не смак:
//   * трогає ЛИШЕ своє й не залежить від сусіда (порядок модулів CF не
//     гарантований -- зміряно на цьому стенді, OZ_Module.c);
//   * ІДЕМПОТЕНТНИЙ: повтор на тому ж uid не міняє нічого;
//   * не має права провалити операцію -- див. Local нижче про те, чому.
class OZ_Wiper
{
    // Для лога: чиє це, коли рядок читає людина.
    string Name()
    {
        return "?";
    }

    // Стерти своє. Викликається ПІСЛЯ заморозки покоління -- усе, що тут
    // чиститься, вже лежить у файлі могили цілим.
    void Wipe(string uid)
    {
    }

    // ЧУЖІ РОДИ, які застаріває вайп ЦЬОГО мода. Ядро імен родів не знає й
    // знати не мусить (OZ_BridgeTypes.c: «роди оголошують самі моди»), тому
    // приймач поштовху збирає їх тут, а не вписує руками.
    void Stales(array<string> kinds)
    {
    }
}

class OZ_Wipe
{
    // Ключ -- id мода. ПЕРШИЙ ВИГРАЄ, і про другого гучно кажемо (правило
    // серії, ТЗ-5 §C1 R1), буква в букву як OZ_AdminRegistry.Register: чужий
    // мод не підмінює наш витирач мовчки.
    private static ref map<string, ref OZ_Wiper> s_Wipers = new map<string, ref OZ_Wiper>();

    static void Register(string modId, OZ_Wiper wiper)
    {
        if (!wiper || modId == "")
        {
            OZ_Log.Warn("wiper \"" + modId + "\" is not registrable: no id or no wiper");
            return;
        }

        if (s_Wipers.Contains(modId))
        {
            OZ_Log.Warn("wiper \"" + modId + "\" registered twice, the second registration is ignored");
            return;
        }

        s_Wipers.Insert(modId, wiper);
        OZ_Log.Dbg("wiper registered: " + modId + " (" + wiper.Name() + ")");
    }

    // Перелік витирачів РЯДКОМ -- для рядка готовності. «Два» не каже, чи є
    // серед них той, кого адмін шукає, а "pda,factions" каже. Заразом рядок
    // друкує ПОРЯДОК: ядро його не обіцяє, тож несподіванка мусить бути
    // видною, а не виводитись.
    static string Describe()
    {
        string line = "";
        for (int i = 0; i < s_Wipers.Count(); i++)
        {
            if (line != "")
                line += ",";
            line += s_Wipers.GetKey(i);
        }

        if (line == "")
            return "none";
        return line;
    }

    // Роди, які застаріває вайп: питаємо самих витирачів (R-W2.5).
    // Порожній список -- законна відповідь: Invalidate поверне false, а
    // викликач скине кеш ЦІЛКОМ, тобто грубо, але вірно.
    static void CollectStales(array<string> kinds)
    {
        if (!kinds)
            return;

        for (int i = 0; i < s_Wipers.Count(); i++)
        {
            OZ_Wiper w = s_Wipers.GetElement(i);
            if (w)
                w.Stales(kinds);
        }
    }

    // ------------------------------------------------- ЧИЙ ВАЙП У ДОРОЗІ
    //
    // Пермадес не ідемпотентний, і саме тому подвійне натискання коштує
    // дорого: обидва листи доїжджають до `ack.Ok`, обидві відповіді кличуть
    // Local, і покоління стрибає через два -- а Freeze між ними записує друге,
    // вже порожнє життя окремим файлом могили. Консоль тримає підтвердження
    // (WIPE двічі), але воно захищає від випадкового кліку, а не від другого
    // кліку по озброєній кнопці, поки міст думає над першим.
    //
    // Ключ -- ЦІЛЬ, а не адмін: два адміни на одного небіжчика -- та сама
    // шкода.
    //
    // ОСТИВАННЯ ПІСЛЯ УСПІХУ -- НОВЕ (R-W1.6, рішення власника 2026-09-08).
    // Досі защіпка стояла лише навколо КОНСОЛЬНОГО кругу: знялась на `ack` --
    // і одночасна команда бота, що приїхала поштовхом через секунду, робила
    // ДРУГИЙ пермадес по тому ж uid. Тепер запис живе ще WIPE_COOLDOWN_MS
    // після того, як Local відпрацював, і другий круг будь-якою дорогою
    // всередині вікна відхиляється.
    private static ref map<string, bool> s_InFlight = new map<string, bool>();
    private static ref map<string, int>  s_DoneAt   = new map<string, int>();

    private static const int WIPE_COOLDOWN_MS = 30000;

    static bool Busy(string uid)
    {
        if (s_InFlight.Contains(uid))
            return true;
        return Cooling(uid);
    }

    // Остигає ще? Прострочений запис прибираємо тут-таки: окремого прибиральника
    // мапа не варта -- ключів у ній стільки, скільки вайпів за запуск.
    private static bool Cooling(string uid)
    {
        int at;
        if (!s_DoneAt.Find(uid, at))
            return false;

        if ((GetGame().GetTime() - at) < WIPE_COOLDOWN_MS)
            return true;

        s_DoneAt.Remove(uid);
        return false;
    }

    static void Begin(string uid)
    {
        s_InFlight.Set(uid, true);
    }

    static void Done(string uid)
    {
        if (s_InFlight.Contains(uid))
            s_InFlight.Remove(uid);
    }

    // ------------------------------------------------- спитати міст
    //
    // МІСТ ПЕРШИМ, гру чіпаємо тільки під `ack.Ok` -- рішення власника
    // 2026-09-06 (D122, R-F4.1) переїжджає разом із обґрунтуванням, і воно
    // в OZ_WipeReply нижче.
    //
    // Перевірки (ціль, міст, защіпка) стоять у розділі: там живе `error`, а
    // тут -- лист і дзвінок.
    static bool Ask(string uid, string adminUid, string op)
    {
        OZ_AdminWipeAsk a = new OZ_AdminWipeAsk();
        a.Uid = uid;
        // Гру відпрацюємо самі, з відповіді, -- хай міст не шле поштовх назад.
        a.FromGame = true;

        string letter;
        string jerr;
        if (!JsonFileLoader<OZ_AdminWipeAsk>.MakeData(a, letter, jerr, false))
            return false;

        OZ_Log.Info("admin: player " + uid + " wipe asked by " + adminUid);
        Begin(uid);
        OZ_BridgeClient.Call("v1/player/wipe", letter, new OZ_WipeReply(adminUid, op, uid));
        return true;
    }

    // ------------------------------------------------- ІГРОВА ПОЛОВИНА
    //
    // Просять двоє: адмінська консоль у грі (і тоді вона ж кличе міст) і сам
    // МІСТ -- коли пермадес запустили командою бота, а не з гри. Без цієї
    // дороги друга робила лише половину справи: ролі в Discord скидались, а
    // КПК небіжчика лишались живими, бо гра про смерть не чула.
    static void Local(string uid)
    {
        if (!GetGame().IsServer())
            return;
        if (uid == "")
            return;

        // ОДНА ЗАЩІПКА НА ВСІ ДОРОГИ (R-W1.5): консольний круг знімає свою
        // позначку в OZ_WipeReply.OnBody ПЕРЕД цим викликом, тож сюди вона
        // доїжджає лише тоді, коли по цьому uid справді йде другий вайп --
        // або з другої консолі, або поштовхом моста поверх консольного.
        if (Busy(uid))
        {
            string busy = "wipe of " + uid + " refused: another one is in flight";
            busy += " or finished less than 30 s ago";
            busy += " - the console and a bot command race here, and the loser would freeze an empty life";
            OZ_Log.Warn(busy);
            return;
        }

        // СПЕРШУ ЗАМОРОЗКА, потім чистка, і це не питання смаку. Витирач, що
        // відпрацював до Freeze, стер би дані з МОГИЛИ -- тобто знищив би рівно
        // те, заради чого могила існує. Старе життя лягає в окремий файл цілим
        // -- з контактами, нотатками й усім, що в ньому було, -- і лише після
        // цього живий запис стає новим персонажем.
        OZ_PlayerStore.Freeze(uid);

        OZ_PlayerData d = OZ_PlayerStore.Load(uid);

        // ЕПОХУ ПІДНІМАЄ ЯДРО, хоч пише її лише КПК. Лічильник оголошений,
        // обнулений і версіонований тут (OZ_PlayerStore), а зміст «кожен
        // прилад, на якому цей акаунт колись відкривав сесію, запечатаний
        // назавжди» -- спільний для будь-якого мода з приладом. Якби +1 робив
        // витирач КПК, сервер без КПК не запечатував би нічого, а два моди з
        // приладами підняли б епоху на два.
        d.SessionEpoch = d.SessionEpoch + 1;

        // ВИТИРАЧІ -- в порядку реєстрації, і порядок ядро НЕ ОБІЦЯЄ.
        //
        // ВІДМОВА ОДНОГО НЕ ЗУПИНЯЄ РЕШТИ Й НЕ ПРОВАЛЮЄ ОПЕРАЦІЮ. Після
        // Freeze поняття «нічого не сталося» більше немає: покоління зсунуте,
        // ключ персонажа (<uid>#<gen>) мертвий, OZ_PlayerStore.IsLive відповідає
        // false кожному, хто спитає. Недотерте поле мертвого покоління -- сміття,
        // а не половина вайпу. Тому Error з іменем витирача -- і йдемо далі.
        //
        // bool Wipe() з відкотом тут не буває: відкочувати нічого -- могила вже
        // на диску, Gen уже зсунутий, файл уже записаний. Усе, що вправі
        // провалитись, мусить провалитись ДО заморозки, і воно провалюється:
        // живість моста, защіпка, ack.Ok.
        for (int i = 0; i < s_Wipers.Count(); i++)
        {
            OZ_Wiper w = s_Wipers.GetElement(i);
            if (!w)
            {
                OZ_Log.Error("wipe " + uid + ": wiper \"" + s_Wipers.GetKey(i) + "\" is gone, skipped");
                continue;
            }

            w.Wipe(uid);
            OZ_Log.Dbg("wipe " + uid + ": " + w.Name() + " done");
        }

        // ОДИН ЗАПИС НА ВСЕ, ЩО НАТЕРЛИ: епоха ядра й поля витирачів лягають
        // разом. MarkDirty тут не зайвий -- Freeze вище вже скинув чергу.
        OZ_PlayerStore.MarkDirty(uid);
        OZ_PlayerStore.Flush(uid);

        // Точка спавну ОСОБИСТА. ClearPersonal чесно скаже «не було» -- нам
        // однаково, головне, що після вайпу її немає.
        OZ_Spawns.ClearPersonal(uid);

        // І ОДНОРАЗОВА ТОЧКА -- ТЕЖ (ТЗ-5 R-A2.1, названий заказник --
        // дефібрилятор).
        //
        // Наслідок ловиться порядком подій, а не читанням: медик ставить
        // «підняти там, де впав», людину в ті ж хвилини стирають назавжди --
        // і нове життя, зовсім чужа людина з тим самим Steam-id, з'являється
        // на трупі попереднього.
        //
        // СТРОК ТУТ НЕ РЯТУЄ, і саме тому рядок потрібен. Строк у точки є
        // (OZ_Spawns.ONCE_TTL_MS -- п'ять хвилин), але все вікно помилки в
        // ньому й лежить: медик ставить точку, кличе адміна, адмін стирає --
        // це хвилини, а не години. Вайп -- єдина мить, коли ми ТОЧНО знаємо,
        // що обіцянка «повернути тебе сюди» більше ні до кого не стосується.
        //
        // Виклик безпечний, коли точки не було: ClearNextSpawn мовчки виходить
        // і в лог не пише -- на відміну від ClearPersonal вище, який каже.
        OZ_Spawns.ClearNextSpawn(uid);

        // ЧУЖІ ЗАПИСНИКИ НЕ ЧІПАЄМО, і це рішення власника 2026-08-30.
        //
        // Викреслити небіжчика з чужих контактів означало б РОЗПОВІСТИ про
        // його смерть: рядок, який зник, читається однозначно. КПК не
        // повідомляє про смерть -- ніколи. Запис лишається на місці
        // замороженим, з датою останньої появи в Зоні, і чи людина загинула,
        // чи просто не заходить, з нього не видно.
        //
        // Нове життя того самого акаунта -- ОКРЕМИЙ запис (uid#покоління),
        // якого ні в кого ще немає: знайомитись доведеться наново.

        // ОСТИВАННЯ ПОЧИНАЄТЬСЯ ТУТ, а не на `ack`: див. защіпку вище.
        s_DoneAt.Set(uid, GetGame().GetTime());

        OZ_Log.Info("player " + uid + " wiped: generation frozen, devices sealed, wipers=" + Describe());
    }
}

class OZ_AdminWipeAsk
{
    string Uid = "";

    // «Ігрову половину вже зроблено». Ставить ГРА, коли пермадес почався в
    // ній: тоді міст робить лише своє й не шле поштовх назад. Порожнє (з
    // команди бота) означає протилежне -- міст мусить розбудити гру.
    bool FromGame = false;
}

// Міст відповів на вайп -- ТУТ І ВІДБУВАЄТЬСЯ ІГРОВА ПОЛОВИНА.
//
// Порядок перевернуто 2026-09-06, і це не косметика. Гра робила своє
// ПЕРШОЮ -- заморожувала покоління, запечатувала КПК, стирала друзів, -- а
// потім питала міст. Відмова Discord (бот не може керувати роллю, тред не
// віддається, гільдія недоступна) лишала гравця розполовиненим: у Зоні мертвий,
// у Discord живий. Порада «повтори команду» не рятувала: повтор заморожував ЩЕ
// ОДНЕ, уже порожнє покоління, і кожна спроба додавала небіжчику життя.
//
// Тепер гру чіпає лише `ack.Ok`. Мовчання моста -- OnFail -- не чіпає нічого
// взагалі, а нерозбірлива відповідь так само нічого: половину вайпу неможливо
// відрізнити від бага, а «не сталося нічого» адмін бачить і повторює безпечно.
//
// FromGame у листі лишається true: бот не мусить штовхати нам "wipe" назад --
// свою половину ми зробимо самі, з цієї відповіді.
class OZ_WipeReply : OZ_BridgeReply
{
    protected string m_AdminUid;
    protected string m_Op;
    protected string m_Uid;

    void OZ_WipeReply(string adminUid, string op, string uid)
    {
        m_AdminUid = adminUid;
        m_Op       = op;
        m_Uid      = uid;
    }

    override void OnBody(string json)
    {
        // ПОЗНАЧКУ «В ДОРОЗІ» ЗНІМАЄМО ПЕРШИМ РЯДКОМ -- інакше Local нижче
        // побачив би власний круг як чужий і відмовив би сам собі.
        OZ_Wipe.Done(m_Uid);

        PlayerIdentity to = OZ_Link.Online(m_AdminUid);

        OZ_BridgeAck ack = new OZ_BridgeAck();
        string err;
        if (!JsonFileLoader<OZ_BridgeAck>.LoadData(json, ack, err) || !ack)
        {
            OZ_Log.Warn("admin: unreadable answer to the wipe of " + m_Uid + ": " + err + " - nothing was changed in the game");
            if (to)
                OZ_Rpc.AdminRespond(to, OZ_AdminSect.PLAYERS, m_Op, false, "", "STR_OZ_ERR_INTERNAL");
            return;
        }

        if (!ack.Ok)
        {
            OZ_Log.Warn("admin: bridge refused the wipe: " + ack.Why + " - nothing was changed in the game");
            if (to)
                OZ_Rpc.AdminRespond(to, OZ_AdminSect.PLAYERS, m_Op, false, "", ack.Why);
            return;
        }

        // Адмін міг вийти, поки міст думав, -- на долю персонажа це не
        // впливає. Спершу робимо, потім розповідаємо тому, хто ще слухає.
        OZ_Wipe.Local(m_Uid);

        // ДРУГИЙ СКИД КЕШУ, І НЕ ЗАЙВИЙ. OZ_BridgeClient.Call() вище вже
        // гасив кеш ЦІЛКОМ у мить відправлення листа -- "v1/player/wipe" це
        // дорога запису, -- а це РАНІШЕ, ніж міст встиг стерти гравця в
        // Discord. Читання чату, що влучило саме в цю щілину, кладе назад
        // склад розмов ЩЕ ДО ПЕРМАДЕСУ, і той живе в кеші до TTL_MS (60 с).
        // Тут ack.Ok каже, що вайп на мосту вже стався насправді, тож рід
        // "wipe" безпечно застарити ще раз -- рівно те, що для конверта
        // опиту робить Stales у OZ_WipeSink, тільки вручну: на цьому шляху
        // (FromGame=true) конверта роду "wipe" не буде зовсім.
        OZ_BridgeCache.Invalidate("wipe", "admin wipe ack");

        if (to)
            OZ_Rpc.AdminRespond(to, OZ_AdminSect.PLAYERS, m_Op, true, "{}", "");
    }

    override void OnFail(int code)
    {
        OZ_Wipe.Done(m_Uid);

        OZ_Log.Warn("admin: the wipe of " + m_Uid + " never reached the bridge - nothing was changed in the game");

        PlayerIdentity to = OZ_Link.Online(m_AdminUid);
        if (!to)
            return;
        OZ_Rpc.AdminRespond(to, OZ_AdminSect.PLAYERS, m_Op, false, "", "STR_OZ_ERR_NO_BRIDGE");
    }
}

// Поштовх «цього гравця стерли» -- від моста. Приходить, коли пермадес
// запустили командою бота: гра робить свою половину тут.
class OZ_WipeSink : OZ_BridgeSink
{
    override void Deliver(string json)
    {
        OZ_AdminWipeAsk a = new OZ_AdminWipeAsk();
        string err;
        if (!JsonFileLoader<OZ_AdminWipeAsk>.LoadData(json, a, err) || !a)
        {
            OZ_Log.Warn("wipe: unreadable push from the bridge: " + err);
            return;
        }

        OZ_Wipe.Local(a.Uid);
    }

    // ВАЙП ПЕРЕПИСУЄ СКЛАД РОЗМОВ -- отже застаріває кеш чату (платформа §4).
    //
    // Це той рідкісний рід, який застарює ЧУЖЕ: сторінка чату оголосила свої
    // v1/chat/list|open|older читальними, ядро тримає відповідь TTL_MS (60 с),
    // а міст на вайпі викреслює uid з members КОЖНОЇ бесіди (openzone-bridge
    // wipePlayer).
    //
    // АЛЕ ІМЕНІ РОДУ ТУТ НЕМАЄ, і це головна різниця з тим, як воно жило в
    // моді фракцій. Там стояло kinds.Insert("chat") -- ядро назвало б рід
    // ЧУЖОГО мода, чого контракт OZ_BridgeSink прямо забороняє
    // (OZ_BridgeTypes.c: «роди оголошують самі моди, ядро не знає їхніх
    // імен»). Тепер список збирають витирачі: "chat" називає витирач КПК,
    // бо чат -- його рід.
    //
    // РЯДОК ПРО ЦІНУ, А НЕ ПРО ПРАВИЛЬНІСТЬ. Порожній список законний: рід
    // лишається незареєстрованим, OZ_BridgeCache.Invalidate чесно поверне
    // false -- і викликач скине кеш ЦІЛКОМ. Чужого небіжчика в списку
    // учасників це не лишає; просто грубіше.
    //
    // І СПРАЦЬОВУЄ ВОНО ЛИШЕ КОЛИ КОНВЕРТ РОДУ "wipe" ДОЇХАВ ОПИТОМ -- вайп
    // командою бота. З VPP-консолі (OZ_AdminWipeAsk.FromGame=true) міст
    // конверт не шле (openzone-bridge/src/index.js: роут v1/player/wipe), і
    // цей Stales мовчить; той шлях застаріває кеш сам, в OZ_WipeReply.OnBody.
    override void Stales(array<string> kinds)
    {
        OZ_Wipe.CollectStales(kinds);
    }
}

// ------------------------------------------------------------ відповіді
//
// Корені створює СКРИПТ, а не серіалізатор (шапка OZ_ConfigBase): у панелі
// VPP ці об'єкти читаються після власних виділень, а поле, якого у відповіді
// не було, без ініціалізатора -- сира пам'ять.

// Хто зараз у Зоні: два паралельні списки, як їх малює панель. Моста не
// потребує -- це знання самого сервера.
class OZ_PlayersHere
{
    ref array<string> Names;
    ref array<string> Uids;

    void OZ_PlayersHere()
    {
        Names = new array<string>();
        Uids  = new array<string>();
    }
}

// Що ядро знає про цей файл гравця. Found=false означає «такого персонажа на
// цьому сервері немає» -- і саме цю відповідь консоль показує замість того,
// щоб зводити курок на сімнадцять цифр з одруківкою.
class OZ_PlayerPeeked
{
    bool   Found = false;
    string Name  = "";
    int    Gen   = 1;
}

// ------------------------------------------------------------ розділ консолі
//
// «Чистий аркуш»: персонаж помер назавжди, ГРАВЕЦЬ лишається. Прив'язка
// Discord ЛИШАЄТЬСЯ: гравець той самий, це персонаж новий.
//
// Половина моста (вихід із приватних тредів, скидання ролей до новачка) їде
// викликом v1/player/wipe, і відповідь клієнтові -- ТІЛЬКИ після неї: адмін
// мусить знати, що вайп пройшов ЦІЛКОМ, а не наполовину.
//
// ПРАВ ТУТ НЕ ПИТАЄМО: їх спитав диспетчер ядра, першим рядком, до розбору
// операції. Межа безпеки одна на всі розділи всіх модів (ТЗ-5 §C3).
class OZ_PlayerSection : OZ_AdminSection
{
    override string Handle(string op, string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        if (op == OZ_PlayerOp.HERE)
            return Here(ok, error);

        int peekCut = OZ_PlayerOp.PEEK.Length() + 1;
        if (op.IndexOf(OZ_PlayerOp.PEEK + ":") == 0)
            return Peek(op.Substring(peekCut, op.Length() - peekCut), ok, error);

        int wipeCut = OZ_PlayerOp.WIPE.Length() + 1;
        if (op.IndexOf(OZ_PlayerOp.WIPE + ":") == 0)
            return Wipe(op.Substring(wipeCut, op.Length() - wipeCut), op, sender, ok, error);

        error = "STR_OZ_ERR_UNKNOWN_OP";
        return "";
    }

    // Присутні -- з рушія, без моста. Це все, що ядрова панель уміє
    // перелічити САМА; відсутню ціль адмін набирає полем, і поле перемагає.
    private string Here(out bool ok, out string error)
    {
        OZ_PlayersHere h = new OZ_PlayersHere();

        array<Man> players = new array<Man>();
        GetGame().GetPlayers(players);

        for (int i = 0; i < players.Count(); i++)
        {
            if (!players[i])
                continue;
            PlayerIdentity id = players[i].GetIdentity();
            if (!id)
                continue;

            h.Names.Insert(id.GetName());
            h.Uids.Insert(id.GetPlainId());
        }

        string body;
        string err;
        if (!JsonFileLoader<OZ_PlayersHere>.MakeData(h, body, err, false))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        ok = true;
        return body;
    }

    // ПЕРШЕ НАТИСКАННЯ НАЗИВАЄ ЛЮДИНУ. Сімнадцять цифр -- це рівно та довжина,
    // на якій одруківка непомітна, і це єдине місце, де їх бачить людина.
    //
    // Через Peek, а не Load: інакше кожна перевірка заводила б файл акаунту,
    // якого на цьому сервері не було ніколи.
    private string Peek(string uid, out bool ok, out string error)
    {
        if (uid == "")
        {
            error = "STR_OZ_ERR_NO_TARGET";
            return "";
        }

        OZ_PlayerPeeked p = new OZ_PlayerPeeked();

        OZ_PlayerData d = OZ_PlayerStore.Peek(uid);
        if (d)
        {
            p.Found = true;
            p.Name  = d.Name;
            p.Gen   = d.Gen;
            if (p.Name == "")
                p.Name = uid;
        }

        string body;
        string err;
        if (!JsonFileLoader<OZ_PlayerPeeked>.MakeData(p, body, err, false))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        // «Немає такого» -- це ВІДПОВІДЬ, а не відмова: панель гасить курок і
        // каже про це своїми словами, не чіпаючи стрінгтейбл.
        ok = true;
        return body;
    }

    private string Wipe(string uid, string op, PlayerIdentity sender, out bool ok, out string error)
    {
        if (uid == "")
        {
            error = "STR_OZ_ERR_NO_TARGET";
            return "";
        }

        // ЦІЛІ БЕЗ ФАЙЛА ГРАВЦЯ НЕ БУВАЄ (R-W1.3), і перевірка стоїть ДО листа
        // мосту навмисно: у моста своя база, і він чесно стер би ролі акаунта,
        // якого на цьому сервері не було ніколи.
        if (!OZ_PlayerStore.Peek(uid))
        {
            error = "STR_OZ_ERR_NO_TARGET";
            return "";
        }

        // Міст питаємо ПЕРШИМ: якщо його немає, не робимо НІЧОГО. Половина
        // вайпу гірша за жодного -- замерзлі КПК при живих тредах виглядали
        // б як баг, а не як смерть.
        if (!OZ_BridgeClient.Alive())
        {
            error = "STR_OZ_ERR_NO_BRIDGE";
            return "";
        }

        // Другий натиск, поки перший у дорозі (або поки не минуло остивання),
        // -- відмова, а не другий пермадес. Причина довга й лежить у защіпці
        // OZ_Wipe вище.
        if (OZ_Wipe.Busy(uid))
        {
            error = "STR_OZ_ERR_SLOW_DOWN";
            return "";
        }

        if (!OZ_Wipe.Ask(uid, sender.GetPlainId(), op))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        // Відповідь піде з OZ_WipeReply, коли міст відпишеться.
        ok    = false;
        error = OZ_Const.DEFER;
        return "";
    }
}
