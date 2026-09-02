// Серверний синглтон ядра на модульній системі CF.
//
// Enable* викликаються ТІЛЬКИ всередині OnInit і ТІЛЬКИ після super.OnInit():
// модулі конструюються на CF_LifecycleEvents.OnGameCreate, і до цього моменту
// CF_Modules<T>.Get() повертає null. Це не стиль, це порядок ініціалізації.

[CF_RegisterModule(OZ_Module)]
class OZ_Module : CF_ModuleWorld
{
    // Періодичний скид відкладених записів.
    //
    // Покладатись лише на дисконект і вимкнення -- крихко: обидва не
    // спрацьовують при падінні сервера, а саме тоді дані й потрібні. Тридцять
    // секунд -- стеля втрати, і вона нічого не коштує: скидається лише те,
    // що позначене брудним.
    private ref Timer m_FlushTimer;

    // Відкладений старт моста -- див. OnMissionStart. Секунда: підписки
    // трапляються в OnMissionStart чужих модулів, а не пізніше.
    private ref Timer m_BridgeTimer;
    private static const float BRIDGE_START_DELAY = 1.0;

    // Частини довгих запитів, що ще їдуть: ключ -- ГРАВЕЦЬ|НОМЕР ПОВІДОМЛЕННЯ.
    // Фінальний конверт забирає й чистить. Гарантований канал зберігає
    // порядок, тому «частини, потім конверт» -- інваріант, а не сподівання.
    //
    // Ключем була пара «сторінка + операція», і два одночасні запити на ту
    // саму пару склеювали свої шматки в одну кашу -- див. OZ_Rpc про номер.
    private ref map<string, string> m_ReqParts = new map<string, string>();

    // Ключі, чий потік ми ВИКИНУЛИ, не дочекавшись конверта: тіло переросло
    // стелю або в польоті стало забагато ключів. Конверт по такому ключу
    // мусить бути відхилений ЦІЛКОМ.
    //
    // Без цього списку виходило гірше за втрату: накопичене викидалось, а
    // конверт приїздив і оброблявся зі своїм власним хвостом -- тобто обрізок
    // чужого тіла проходив як цілий документ.
    private ref array<string> m_ReqPoison = new array<string>();
    private static const float FLUSH_INTERVAL = 30.0;

    // Стелі проти зловмисних/обірваних частин. Легальний chunked-запит --
    // це список нотаток чи книжка чипа, десятки кілобайт щонайбільше, і
    // на сервер одночасно летить дай Боже одна-дві на гравця. Клієнт, що
    // шле частини без фінального конверта, інакше ріс би в пам'яті вічно.
    private static const int REQPART_MAX_BYTES = 262144;  // 256 KB на ключ
    private static const int REQPART_MAX_KEYS  = 64;       // усього в польоті

    override void OnInit()
    {
        super.OnInit();

        EnableMissionStart();
        EnableMissionFinish();
        EnableInvokeConnect();
        EnableInvokeDisconnect();
    }

    override void OnMissionStart(Class sender, CF_EventArgs args)
    {
        super.OnMissionStart(sender, args);

        // Конфіги, права й сховище -- серверні. Клієнт отримує лише те, що
        // сервер сам йому вирішив надіслати.
        if (!GetGame().IsServer())
            return;

        OZ_Settings.ServerLoad();

        OZ_Settings s = OZ_Settings.Get();

        string dbg = "off";
        if (s.DebugMode)
            dbg = "on";

        // Два оператори, а не один довгий ланцюжок «+»: компілятор Enforce має
        // межу складності виразу й падає з «Formula too complex» -- у сусідньому
        // моді це знайшли емпірично на восьмому доданку.
        OZ_Perm.ServerInit();

        OZ_Spawns.ServerLoad();

        // Адмiнська консоль: РОЗДIЛИ у власному реєстрi, не сторiнки в
        // спiльному (рiшення власника 2026-09-01, ТЗ-5 §C2-C3). Ворота однi
        // й без винятків -- OZ_Perm.IsAdmin у OZ_AdminReq нижче.
        //
        // Factions.json у редакторi конфiгiв НЕМАЄ навмисно: фракцiї
        // народжуються й вмирають тiльки через бота (рiшення власника
        // 2026-08-30) -- iнакше в фракцiї не було б ролi в Discord. Редактор,
        // який дозволяє дописати фракцiю в файл, був би обхiдною стежкою повз
        // це правило.
        OZ_AdminRegistry.Register(OZ_AdminSect.CONFIG, new OZ_ConfigSection());
        OZ_AdminRegistry.Register(OZ_AdminSect.SPAWNS, new OZ_SpawnSection());

        // NEWS -- у ядрі, а не в моді КПК, і це навмисно: писати новину не
        // потрібен ані прилад, ані фракції. Потрібен лише міст, а він ядровий.
        OZ_AdminRegistry.Register(OZ_AdminSect.NEWS, new OZ_NewsSection());
        OZ_AdminCfg.Register("Spawns", OZ_Const.PROFILE_DIR + "\\OZ_Core_Spawns.json", new OZ_SpawnsCfgApplier());

        OZ_Rpc.RegisterServer(this);

        m_FlushTimer = new Timer(CALL_CATEGORY_SYSTEM);
        m_FlushTimer.Run(FLUSH_INTERVAL, this, "FlushTick", NULL, true);

        // МІСТ СТАРТУЄ НЕ ТУТ, А ТІКОМ ПІЗНІШЕ, і це не косметика.
        //
        // Роди оголошують МОДИ, кожен свій: чат і новини -- КПК, ролі й
        // ростер -- фракції. Порядок CF-модулів не гарантований (перевірено
        // на цьому стенді), тож частина підписок неминуче трапляється ПІСЛЯ
        // OnMissionStart ядра. Стартувати опит тут означало б програти гонку
        // тим, хто підписався пізніше, -- і втратити їхню першу пачку.
        //
        // Один тік затримки гарантує, що OnMissionStart відпрацював у всіх.
        m_BridgeTimer = new Timer(CALL_CATEGORY_SYSTEM);
        m_BridgeTimer.Run(BRIDGE_START_DELAY, this, "StartBridge", NULL, false);

        string summary = "core loaded: admins=" + s.AdminIds.Count();
        summary += " perms=" + OZ_Perm.Describe();
        summary += " pages=" + OZ_PageRegistry.Count().ToString();
        // Розділи консолі -- ІМЕНАМИ, а не числом: «три» не каже, чи серед них
        // той, якого адмін шукає, а «config,spawns,factions» каже.
        summary += " admin=" + OZ_AdminRegistry.Describe();
        summary += " spawnzones=" + OZ_Spawns.Count().ToString();

        // Стейджинґ окремим словом, а не в лічильнику зон: він або є, або
        // його немає, і адмін мусить бачити відповідь, не рахуючи рядки.
        if (OZ_Spawns.HasStaging())
            summary += " staging=on";
        else
            summary += " staging=off";

        // ДЗЕРКАЛА -- ЧИСЛОМ У РЯДКУ ГОТОВНОСТІ. Нуль тут означає «у гільдії
        // тихо», і це найчастіше питання адміна після «чому нічого немає».
        summary += " mirrors=" + OZ_BridgeClient.MirrorCount().ToString();

        summary += " debug=" + dbg;
        OZ_Log.Info(summary);
    }

    // Прив'язка -- ВЛАСНА пара RPC, а не сторінка. Причина в OZ_Rpc: сторінки
    // проходять крізь OZ_PageAccess, який КПК підміняє перевіркою «чи є ця
    // сторінка на цьому пристрої», і прив'язатись зміг би лише власник КПК.
    //
    // Особа -- ЗАВЖДИ з sender, те саме правило, що й у OZ_Req. Клієнт не
    // називає, за кого просить, і не може: у конверті немає такого поля.
    void OZ_LinkReq(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server)
            return;

        Param1<string> data;
        if (!ctx.Read(data))
            return;

        if (!sender)
            return;

        string op = data.param1;

        if (op == OZ_LinkConst.OP_BEGIN)
        {
            OZ_Link.Begin(sender);
            return;
        }

        if (op == OZ_LinkConst.OP_STATE)
        {
            OZ_Link.SendState(sender);
            return;
        }

        OZ_Rpc.LinkRespond(sender, op, false, "", "STR_OZ_ERR_UNKNOWN_OP");
    }

    // Частина довгого запиту. Тільки накопичити: перевірки доступу зроблені
    // ОДИН раз у фінальному OZ_Req -- частини без конверта нікуди не ведуть.
    void OZ_ReqPart(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server || !sender)
            return;

        Param2<int, string> data;
        if (!ctx.Read(data))
            return;

        TakePart(PartKey(sender, data.param1), data.param2, sender);
    }

    // Ключ потоку: гравець і номер повідомлення. Сторінки й операції в ньому
    // більше немає -- див. OZ_Rpc про те, чому пара «сторінка + операція»
    // ключем бути не може.
    private string PartKey(PlayerIdentity who, int msgId)
    {
        return who.GetPlainId() + "|" + msgId.ToString();
    }

    // Накопичити один шматок під ключем. Спільне для сторінок і для консолі:
    // стелі проти обірваного потоку мусять бути ОДНІ, інакше другий канал
    // тихо лишається без них.
    private void TakePart(string key, string chunk, PlayerIdentity sender)
    {
        // Потік уже отруєний -- більше нічого не накопичуємо. Чекаємо конверт,
        // щоб відповісти відмовою й прибрати позначку.
        if (m_ReqPoison.Find(key) != -1)
            return;

        string sofar = "";
        bool known = m_ReqParts.Find(key, sofar);

        // Новий ключ у переповнену мапу не пускаємо: конверт, що склеює й
        // чистить, для такого потоку може не прийти взагалі.
        if (!known && m_ReqParts.Count() >= REQPART_MAX_KEYS)
        {
            OZ_Log.Warn("reqpart: too many in-flight keys, dropping from " + sender.GetPlainId());
            Poison(key);
            return;
        }

        // Тіло понад стелю -- або баг, або атака: викидаємо накопичене, щоб
        // не тримати чужий мегабайт до кінця сеансу.
        if (sofar.Length() + chunk.Length() > REQPART_MAX_BYTES)
        {
            m_ReqParts.Remove(key);
            OZ_Log.Warn("reqpart: body over cap, dropped key from " + sender.GetPlainId());
            Poison(key);
            return;
        }

        m_ReqParts.Set(key, sofar + chunk);
    }

    private void Poison(string key)
    {
        if (m_ReqPoison.Find(key) == -1)
            m_ReqPoison.Insert(key);
    }

    // Забрати накопичене під ключем. Повертає false, коли потік був отруєний:
    // тоді тіла немає й бути не може, і конверт мусить піти у відмову.
    private bool TakeBody(string key, inout string json)
    {
        int bad = m_ReqPoison.Find(key);
        if (bad != -1)
        {
            m_ReqPoison.Remove(bad);
            m_ReqParts.Remove(key);
            return false;
        }

        string parts = "";
        if (m_ReqParts.Find(key, parts))
        {
            json = parts + json;
            m_ReqParts.Remove(key);
        }
        return true;
    }

    // Частина довгого АДМІНСЬКОГО запиту. Накопичується в тій самій мапі, що
    // й частини сторінок: ключ несе слово "admin" між особою й розділом, тож
    // розділ і сторінка з однаковим іменем не склеять свої тіла в одне.
    //
    // Прав тут не питаємо -- рівно як і в OZ_ReqPart: частини без конверта
    // нікуди не ведуть, а стеля на об'єм і на кількість ключів нижче захищає
    // від того, хто шле їх і не шле конверта.
    void OZ_AdminReqPart(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server || !sender)
            return;

        Param2<int, string> data;
        if (!ctx.Read(data))
            return;

        TakePart(PartKey(sender, data.param1), data.param2, sender);
    }

    // Адмінська консоль. Порядок нижче -- і є вся межа безпеки, і він
    // коротший за сторінковий рівно тому, що тут немає пристрою: ані профілю,
    // ані сторінки, ані замка. Одні ворота, першим рядком.
    void OZ_AdminReq(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server)
            return;

        Param4<int, string, string, string> data;
        if (!ctx.Read(data))
            return;

        // 1. Особа -- ЗАВЖДИ з sender. Ніколи з корисного навантаження.
        if (!sender)
            return;

        string sectionId = data.param2;
        string op        = data.param3;
        string json      = data.param4;

        // Довге тіло приїхало частинами поперед конверта -- приклеїти. До
        // перевірки прав, бо накопичене треба прибрати НАВІТЬ у відмові:
        // інакше чужі частини лежали б у мапі до кінця сеансу.
        if (!TakeBody(PartKey(sender, data.param1), json))
        {
            OZ_Log.Warn("admin: dropped an over-cap body from " + sender.GetPlainId());
            OZ_Rpc.AdminRespond(sender, sectionId, op, false, "", "STR_OZ_ERR_TOO_LONG");
            return;
        }

        // 2. МЕЖА БЕЗПЕКИ, і вона одна. Розділи не мають власних перевірок:
        //    друга перевірка в кожному моді -- це друге місце, де правило
        //    можна забути.
        if (!OZ_Perm.IsAdmin(sender))
        {
            string w0 = "rejected admin section \"" + sectionId;
            w0 += "\" from " + sender.GetPlainId();
            w0 += ": not an admin";
            OZ_Log.Warn(w0);
            OZ_Rpc.AdminRespond(sender, sectionId, op, false, "", "STR_OZ_ERR_ADMIN_ONLY");
            return;
        }

        // 3. Розділ мусить існувати. Warn, а не Dbg: на відміну від сторінок,
        //    які клієнт питає раз на секунду, сюди приходять лише за
        //    натисканням, і невідоме ім'я означає розсинхрон збірок.
        if (!OZ_AdminRegistry.Has(sectionId))
        {
            string w1 = "rejected admin section \"" + sectionId;
            w1 += "\" from " + sender.GetPlainId();
            w1 += ": no such section, have " + OZ_AdminRegistry.Describe();
            OZ_Log.Warn(w1);
            OZ_Rpc.AdminRespond(sender, sectionId, op, false, "", "STR_OZ_ERR_NO_SECTION");
            return;
        }

        bool ok;
        string err;

        string res = OZ_AdminRegistry.Get(sectionId).Handle(op, json, sender, ok, err);

        // Розділ міг піти по відповідь за межі сервера -- у міст, у Discord.
        // Тоді він відповість сам, коли та приїде.
        if (!ok && err == OZ_Const.DEFER)
            return;

        OZ_Rpc.AdminRespond(sender, sectionId, op, ok, res, err);
    }

    // Викинути всі недособрані частини гравця. Кличеться на дисконекті:
    // без конверта вони нікуди не ведуть, а тримати їх нема кому. Разом із
    // ними йдуть і отруєні ключі: конверта, який мав би їх забрати, вже
    // не буде.
    private void ForgetReqParts(string uid)
    {
        string prefix = uid + "|";
        array<string> doomed = new array<string>();
        for (int i = 0; i < m_ReqParts.Count(); i++)
        {
            string k = m_ReqParts.GetKey(i);
            if (k.IndexOf(prefix) == 0)
                doomed.Insert(k);
        }
        for (int j = 0; j < doomed.Count(); j++)
            m_ReqParts.Remove(doomed[j]);

        for (int p = m_ReqPoison.Count() - 1; p >= 0; p--)
        {
            if (m_ReqPoison[p].IndexOf(prefix) == 0)
                m_ReqPoison.Remove(p);
        }
    }

    // Порядок перевірок нижче -- і є межа безпеки. Міняти його не можна.
    void OZ_Req(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server)
            return;

        Param4<int, string, string, string> data;
        if (!ctx.Read(data))
            return;

        // 1. Особа -- ЗАВЖДИ з sender. Ніколи з корисного навантаження:
        //    туди клієнт напише що завгодно. Перевірка стоїть ПЕРЕД склейкою
        //    тіла, бо без особи немає й ключа, під яким те тіло лежить.
        if (!sender)
            return;

        string pageId = data.param2;
        string op     = data.param3;
        string json   = data.param4;

        // Довге тіло приїхало частинами поперед конверта -- приклеїти. Потік,
        // який ми викинули по стелі, сюди не доходить: конверт по такому
        // ключу відхиляється цілком, а не обробляється зі своїм хвостом.
        if (!TakeBody(PartKey(sender, data.param1), json))
        {
            OZ_Log.Warn("page: dropped an over-cap body from " + sender.GetPlainId());
            OZ_Rpc.Respond(sender, pageId, op, false, "", "STR_OZ_ERR_TOO_LONG");
            return;
        }

        // 2. Сторінка мусить існувати.
        if (!OZ_PageRegistry.Has(pageId))
        {
            string w1 = "rejected page \"" + pageId;
            w1 += "\" from " + sender.GetPlainId();
            w1 += ": no such page";
            OZ_Log.Warn(w1);
            OZ_Rpc.Respond(sender, pageId, op, false, "", "STR_OZ_ERR_NO_PAGE");
            return;
        }

        // 3. І входити в набір сторінок ПРИСТРОЮ цього гравця. Саме цей крок
        //    не дає смикнути сторінку, якої в його КПК немає, навіть якщо
        //    запит підроблено.
        string why;
        if (!OZ_PageAccess.Allowed(sender, pageId, op, why))
        {
            // Dbg, не Warn -- на відміну від кроку 2 вище.
            //
            // «Такої сторінки немає» -- це або баг, або підробка, і за всю
            // історію стенду трапилось один раз. А «сторінки немає на ЦЬОМУ
            // пристрої» -- це звичайний стан будь-якого запечатаного чи
            // замкненого КПК: клієнт питає раз на секунду, гейт щоразу чесно
            // відмовляє. Один сеанс дав 1423 такі рядки при max_warnings = 0
            // у профілі стенду, тобто нормальна робота мода читалась як
            // аварія і топила в собі справжні попередження.
            // ПРИЧИНУ ПИШЕМО ТУ, ЯКУ НАЗВАВ ГЕЙТ.
            //
            // Тут стояло глухе «not on this device» на ВСІ його відмови --
            // а їх п'ять: сторінки немає в профілі, прилад вимкнено, прилад
            // замкнено, прилад не ініційовано, прилад -- капсула. Рядок брехав
            // у чотирьох випадках із п'яти, і 2026-09-01 на цьому згаяли
            // півгодини: у лозі стояло «не на цьому пристрої», а насправді в
            // приладу сіла батарея.
            string w2 = "rejected page \"" + pageId;
            w2 += "\" from " + sender.GetPlainId();
            w2 += ": " + why;
            OZ_Log.Dbg(w2);
            OZ_Rpc.Respond(sender, pageId, op, false, "", why);
            return;
        }

        bool ok;
        string err;

        string res = OZ_PageRegistry.Get(pageId).Handler.Handle(op, json, sender, ok, err);

        // Сторінка могла піти по відповідь за межі сервера -- у міст, у
        // Discord. Тоді вона відповість сама, коли та приїде, а тут треба
        // саме промовчати: інакше клієнт побачив би «не вдалося» за секунду
        // до справжньої відповіді.
        if (!ok && err == OZ_Const.DEFER)
            return;

        OZ_Rpc.Respond(sender, pageId, op, ok, res, err);
    }

    override void OnInvokeConnect(Class sender, CF_EventArgs args)
    {
        super.OnInvokeConnect(sender, args);

        if (!GetGame().IsServer())
            return;

        CF_EventPlayerArgs pArgs = CF_EventPlayerArgs.Cast(args);
        if (!pArgs || !pArgs.Identity)
            return;

        bool admin = OZ_Perm.IsAdmin(pArgs.Identity);

        OZ_PlayerData d = OZ_PlayerStore.Load(pArgs.Identity.GetPlainId());
        d.LastSeen = OZ_Time.NowUtc();
        // Ім'я оновлюємо щовходу: гравець міг його змінити, а показувати
        // старе там, де інший гравець вирішує, кого приймати в друзі, гірше
        // за будь-яку іншу неточність.
        d.Name = pArgs.Identity.GetName();
        OZ_PlayerStore.MarkDirty(pArgs.Identity.GetPlainId());

        // Базова фракція -- на вході, і саме тут, а не окремим хуком (ТЗ-1
        // R5.5): місце, де вже прочитано файл гравця, одне, і другий хук
        // означав би друге читання й друге місце, де про це можна забути.
        OZ_Identity.Get().EnsureBase(pArgs.Identity.GetPlainId());

        string line = "connect " + pArgs.Identity.GetName();
        line += " (" + pArgs.Identity.GetPlainId();
        line += ") admin=" + admin;
        OZ_Log.Dbg(line);
    }

    // Клієнт привітався -- значить він уже готовий приймати. Тільки тут і
    // надсилаємо: штовхати на конекті не можна, перевірено на стенді.
    void OZ_Hello(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server)
            return;

        Param1<int> data;
        if (!ctx.Read(data))
            return;

        if (!sender)
            return;

        if (data.param1 != OZ_Const.SCHEMA_SETTINGS)
        {
            string mism = "client schema " + data.param1.ToString();
            mism += " != server " + OZ_Const.SCHEMA_SETTINGS.ToString();
            mism += " for " + sender.GetPlainId();
            OZ_Log.Warn(mism);
        }

        SendSync(sender);
    }

    private void SendSync(PlayerIdentity to)
    {
        OZ_SyncPayload p = new OZ_SyncPayload();
        p.Schema    = OZ_Const.SCHEMA_SETTINGS;
        p.DebugMode = OZ_Settings.Get().DebugMode;

        // Прив'язка їде тим самим конвертом. Клієнт тягне його рівно один раз,
        // щойно з'явився у світі -- тобто це найраніша мить, коли є кому
        // показати ворота, і вона не коштує жодного нового пакета.
        p.Linked       = OZ_Link.IsLinked(to.GetPlainId());
        p.LinkRequired = OZ_Link.Gated(to.GetPlainId());
        OZ_PageRegistry.FillPayload(p);

        string json;
        string err;
        // prettyPrint=false: у провід не треба ані відступів, ані переносів.
        if (!JsonFileLoader<OZ_SyncPayload>.MakeData(p, json, err, false))
        {
            OZ_Log.Error("cannot serialise sync payload: " + err);
            return;
        }

        OZ_Rpc.SendSync(to, json);
    }

    override void OnInvokeDisconnect(Class sender, CF_EventArgs args)
    {
        super.OnInvokeDisconnect(sender, args);

        if (!GetGame().IsServer())
            return;

        // На дисконекті особи може вже не бути, тому CF окремо несе UID у
        // CF_EventPlayerDisconnectedArgs. Спираємось на нього, а не на Identity.
        CF_EventPlayerDisconnectedArgs dArgs = CF_EventPlayerDisconnectedArgs.Cast(args);
        if (!dArgs)
            return;

        OZ_PlayerData d = OZ_PlayerStore.Load(dArgs.UID);
        d.LastSeen = OZ_Time.NowUtc();
        OZ_PlayerStore.MarkDirty(dArgs.UID);
        OZ_PlayerStore.Unload(dArgs.UID);

        // ЗАПРОШЕННЯ Й ПРОЕКЦІЮ РОЛЕЙ ЧИСТИТЬ МОД ФРАКЦІЙ, а не ядро.
        //
        // Тут стояв виклик OZ_FactionInvites.Forget, і він пережив винесення
        // фракцій. Ціна виявилась не косметичною: імені з незавантаженого
        // мода в Enforce не існує навіть у мертвій гілці, тож набір без
        // OpenZone_Factions не компілював УЗАГАЛІ -- «Can't compile "World"
        // script module! ... Can't find variable 'OZ_FactionInvites'». Тобто
        // правило серії «будь-який мод запускається, маючи одне лише ядро»
        // ламав один рядок у самому ядрі.
        //
        // OZF_Module уже має свій OnInvokeDisconnect і чистить там OZ_Roles;
        // запрошення прибираються поруч, бо належать тому ж модові.

        // Недособрані частини довгих запитів цього гравця -- геть.
        ForgetReqParts(dArgs.UID);

        OZ_Log.Dbg("disconnect " + dArgs.UID);
    }

    // Кличеться таймером, а не напряму: див. OnMissionStart про гонку
    // підписок. Метод мусить бути НЕ приватним -- Timer шукає його по імені.
    void StartBridge()
    {
        OZ_BridgeClient.Start();
    }

    override void OnMissionFinish(Class sender, CF_EventArgs args)
    {
        super.OnMissionFinish(sender, args);

        if (!GetGame().IsServer())
            return;

        if (m_FlushTimer)
            m_FlushTimer.Stop();

        OZ_BridgeClient.Stop();

        // Останній шанс дописати відкладене: після цього процес зникає.
        OZ_PlayerStore.FlushAll();
        OZ_Log.Dbg("core shutting down");
    }

    // Кличеться таймером на ім'я -- метод мусить бути видимим (не private).
    void FlushTick()
    {
        OZ_PlayerStore.FlushAll();
    }
}
