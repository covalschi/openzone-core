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
    //
    // ПИШЕМО ПОРЦІЯМИ, А НЕ ПАЧКОЮ. Тік раз на тридцять секунд писав УСЕ
    // брудне одним синхронним циклом, а брудними після ресинку ролей стають
    // майже всі онлайн одразу -- вісімдесят файлів в одному кадрі на сервері
    // з одним ядром. П'ять секунд і вісім файлів за тік прибирають хитч.
    //
    // СТЕЛЯ ВТРАТИ ЛИШАЄТЬСЯ ТРИДЦЯТЬМА СЕКУНДАМИ, і сама лише стала порція
    // її не тримала: вісімдесят брудних файлів по вісім за тік -- це десять
    // тіків, тобто п'ятдесят секунд. Тому FLUSH_PER_TICK -- дно, а не стеля;
    // FLUSH_TICKS каже, за скільки тіків черга мусить скінчитись у будь-якому
    // разі. 6 * 5 с = 30 с.
    private ref Timer m_FlushTimer;
    private static const int FLUSH_PER_TICK = 8;
    private static const int FLUSH_TICKS    = 6;

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
    private static const float FLUSH_INTERVAL = 5.0;

    // Хто вже отримав своє попередження про переповнення. Один рядок на
    // порушника за сеанс: без цього клієнт, що ллє частини без конверта,
    // сам собі малює тисячу WARNING і топить у них чужі.
    private ref array<string> m_ReqLoud = new array<string>();

    // Стелі проти зловмисних/обірваних частин. Легальний chunked-запит --
    // це список нотаток чи книжка чипа, десятки кілобайт щонайбільше, і
    // на сервер одночасно летить дай Боже одна-дві на гравця. Клієнт, що
    // шле частини без фінального конверта, інакше ріс би в пам'яті вічно.
    //
    // СТЕЛІ НА ВІДПРАВНИКА, А НЕ НА ВЕСЬ СЕРВЕР, і це не тонкощі обліку.
    // Спільна стеля на 64 ключі означала рівно те, що один підключений
    // клієнт, шлючи частини без конвертів, наповнював мапу до краю -- і з
    // тієї миті БУДЬ-ЯКИЙ довгий запит БУДЬ-ЯКОГО іншого гравця відхилявся
    // з STR_OZ_ERR_TOO_LONG, поки нападник не вийде. Стеля на uid лишає
    // йому можливість заморити голодом лише себе.
    //
    // Спільної стелі більше немає навмисно: відправник мусить бути
    // ПІДКЛЮЧЕНИМ гравцем (без sender ми виходимо першим рядком), тож
    // добуток «слоти сервера * REQPART_MAX_PER_UID» і є справжня межа, і
    // вона не залежить від того, скільки хтось один устиг захопити.
    private static const int REQPART_MAX_BYTES   = 262144;  // 256 KB на ключ
    private static const int REQPART_MAX_PER_UID = 4;       // потоків у польоті на гравця
    private static const int POISON_MAX_PER_UID  = 16;      // позначок на гравця

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

        // Probe() лінивий і його кличуть усі точки входу прав; тут -- щоб
        // рядок про джерело прав стояв у лозі старту, а не з'явився при
        // першому натисканні адміна.
        OZ_Perm.Probe();

        OZ_Spawns.ServerLoad();

        // Адмінська консоль: РОЗДІЛИ у власному реєстрі, не сторінки в
        // спільному (рішення власника 2026-09-01, ТЗ-5 §C2-C3). Ворота одні
        // й без винятків -- OZ_Perm.IsAdmin у OZ_AdminReq нижче.
        //
        // Factions.json у редакторі конфігів НЕМАЄ навмисно: фракції
        // народжуються й вмирають тільки через бота (рішення власника
        // 2026-08-30) -- інакше в фракції не було б ролі в Discord. Редактор,
        // який дозволяє дописати фракцію в файл, був би обхідною стежкою повз
        // це правило.
        OZ_AdminRegistry.Register(OZ_AdminSect.CONFIG, new OZ_ConfigSection());
        OZ_AdminRegistry.Register(OZ_AdminSect.SPAWNS, new OZ_SpawnSection());

        // NEWS -- у ядрі, а не в моді КПК, і це навмисно: писати новину не
        // потрібен ані прилад, ані фракції. Потрібен лише міст, а він ядровий.
        OZ_AdminRegistry.Register(OZ_AdminSect.NEWS, new OZ_NewsSection());

        // PLAYERS -- ТЕЖ У ЯДРІ (дизайн 2026-09-08). Пермадес чистить файл
        // гравця, а файл гравця -- ядровий; поки розділ жив у моді фракцій,
        // сервер core+PDA не мав вайпу зовсім.
        OZ_AdminRegistry.Register(OZ_AdminSect.PLAYERS, new OZ_PlayerSection());

        // Пермадес, запущений НЕ з гри: команда бота робить свою половину й
        // штовхає сюди, щоб гра зробила свою. Підписка мусить статись до
        // старту опиту -- міст стартує тіком пізніше саме заради цього
        // (нижче), тож власна підписка ядра встигає заведомо.
        //
        // ТУТ, А НЕ В OZF_Module: рід "wipe" возить ядрову службу, і сервер
        // без мода фракцій мусить її чути. Поки підписка стояла там, пуш від
        // `/openzone wipe` на core+PDA ішов у порожнечу.
        OZ_BridgeClient.Subscribe("wipe", new OZ_WipeSink());

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
        //
        // РЯДОК ГОТОВНОСТІ ЇДЕ ТУДИ Ж, І З ТІЄЇ САМОЇ ПРИЧИНИ (2026-09-08).
        // Він перелічує те, що приносять МОДИ -- сторінки, витирачі, -- а
        // друкувався тут, тобто до їхніх OnMissionStart. На стенді core+PDA
        // це було видно голим оком: `pages=0` при семи зареєстрованих і
        // `wipers=none` при живому OZ_PdaWiper. Рядок, який називає чуже
        // майно, мусить друкуватись тоді ж, коли ядро вирішує, що всі вже
        // сказали своє.
        m_BridgeTimer = new Timer(CALL_CATEGORY_SYSTEM);
        m_BridgeTimer.Run(BRIDGE_START_DELAY, this, "Ready", NULL, false);
    }

    // Усі OnMissionStart відпрацювали: сказати, з чим ядро піднялось, і аж
    // тоді пускати опит. Кличеться таймером на ім'я -- метод мусить бути
    // видимим (не private).
    void Ready()
    {
        OZ_Settings s = OZ_Settings.Get();

        string dbg = "off";
        if (s.DebugMode)
            dbg = "on";

        // РЯДОК ЗБИРАЄМО ПООПЕРАТОРНО, а не одним ланцюжком «+»: компілятор
        // Enforce має межу складності виразу й падає з «Formula too complex»
        // -- у сусідньому моді це знайшли емпірично на восьмому доданку.
        string summary = "core loaded: admins=" + s.AdminIds.Count();
        summary += " perms=" + OZ_Perm.Describe();
        summary += " pages=" + OZ_PageRegistry.Count().ToString();
        // Розділи консолі -- ІМЕНАМИ, а не числом: «три» не каже, чи серед них
        // той, якого адмін шукає, а «config,spawns,factions» каже.
        summary += " admin=" + OZ_AdminRegistry.Describe();
        // ВИТИРАЧІ -- ІМЕНАМИ Й У ПОРЯДКУ РЕЄСТРАЦІЇ (R-W1.9). Порядок ядро не
        // обіцяє (порядок модулів CF не гарантований), тож несподіванка мусить
        // бути ВИДНОЮ, а не виводитись; "none" -- законна відповідь, а не
        // порожнеча. Форма `ключ=значення` не випадкова: вердикт стенда читає
        // лічильники саме з цього рядка й саме в ній (зміряно 2026-09-04).
        summary += " wipers=" + OZ_Wipe.Describe();
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

        OZ_BridgeClient.Start();
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

    // Скільки потоків цього гравця зараз у польоті.
    private int KeysOf(string prefix)
    {
        int n = 0;
        for (int i = 0; i < m_ReqParts.Count(); i++)
        {
            if (m_ReqParts.GetKey(i).IndexOf(prefix) == 0)
                n++;
        }
        return n;
    }

    // Один рядок на порушника. Далі -- Dbg: сам факт уже сказано, а
    // повторення лише топить у собі чужі попередження.
    private void Loud(string uid, string what)
    {
        if (m_ReqLoud.Find(uid) != -1)
        {
            OZ_Log.Dbg("reqpart: " + what + " from " + uid);
            return;
        }

        m_ReqLoud.Insert(uid);
        OZ_Log.Warn("reqpart: " + what + " from " + uid);
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

        string uid    = sender.GetPlainId();
        string prefix = uid + "|";

        string sofar = "";
        bool known = m_ReqParts.Find(key, sofar);

        // Новий ключ понад стелю ЦЬОГО гравця не пускаємо: конверт, що склеює
        // й чистить, для такого потоку може не прийти взагалі.
        if (!known && KeysOf(prefix) >= REQPART_MAX_PER_UID)
        {
            Loud(uid, "too many in-flight streams, dropping");
            Poison(key, prefix);
            return;
        }

        // Тіло понад стелю -- або баг, або атака: викидаємо накопичене, щоб
        // не тримати чужий мегабайт до кінця сеансу.
        if (sofar.Length() + chunk.Length() > REQPART_MAX_BYTES)
        {
            m_ReqParts.Remove(key);
            Loud(uid, "body over cap, dropped a key");
            Poison(key, prefix);
            return;
        }

        m_ReqParts.Set(key, sofar + chunk);
    }

    // Позначка «цей потік викинуто» -- і стеля на неї, теж на відправника.
    //
    // Без стелі список ріс без краю: кожен наступний номер повідомлення
    // давав новий ключ, який одразу отруювався й лишався в масиві до
    // дисконекту. Найстаріша позначка цього гравця йде першою -- її конверт
    // або вже приїхав, або не приїде ніколи.
    //
    // RemoveOrdered, А НЕ Remove. array.Remove затикає дірку ОСТАННІМ
    // елементом і порядку не зберігає (1_Core/proto/enscript.c:463-470) --
    // тобто після першої ж евікції масив переставав бути списком у порядку
    // надходження, і «перший збіг = найстаріший» ставав неправдою. Тут це
    // коштувало не коректності, а самого змісту слова «найстаріша»:
    // викидалась довільна позначка гравця. Масив короткий (16 на гравця),
    // тож повільніше видалення тут нічого не важить.
    private void Poison(string key, string prefix)
    {
        if (m_ReqPoison.Find(key) != -1)
            return;

        int mine = 0;
        int oldest = -1;
        for (int i = 0; i < m_ReqPoison.Count(); i++)
        {
            if (m_ReqPoison[i].IndexOf(prefix) != 0)
                continue;
            mine++;
            if (oldest == -1)
                oldest = i;
        }

        if (mine >= POISON_MAX_PER_UID && oldest != -1)
            m_ReqPoison.RemoveOrdered(oldest);

        m_ReqPoison.Insert(key);
    }

    // Забрати накопичене під ключем. Повертає false, коли потік був отруєний:
    // тоді тіла немає й бути не може, і конверт мусить піти у відмову.
    private bool TakeBody(string key, inout string json)
    {
        int bad = m_ReqPoison.Find(key);
        if (bad != -1)
        {
            // Теж упорядковано: черга позначок має лишатись у порядку
            // надходження, інакше евікція в Poison() бере не найстарішу.
            m_ReqPoison.RemoveOrdered(bad);
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

    // ОКРЕМОГО ПРИЙМАЧА АДМІНСЬКИХ ЧАСТИН ТУТ БІЛЬШЕ НЕМАЄ.
    //
    // OZ_AdminReqPart мав побайтово те саме тіло, що й OZ_ReqPart вище,
    // складав у ту саму мапу під тим самим ключем і ключився номером від
    // того самого лічильника. Частини їдуть спільним RPC_REQ_PART; межа
    // прав перевіряється на КОНВЕРТІ (OZ_Perm.IsAdmin першим рядком
    // OZ_AdminReq), а не на шматку, тож нічого не відчиняється.

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
        OZ_AdminSection section = OZ_AdminRegistry.Get(sectionId);
        if (!section)
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

        string res = section.Handle(op, json, sender, ok, err);

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

        // Згори вниз і RemoveOrdered: індекси нижче за p від видалення не
        // рухаються, а порядок решти гравців зберігається -- на нього
        // спирається евікція в Poison().
        for (int p = m_ReqPoison.Count() - 1; p >= 0; p--)
        {
            if (m_ReqPoison[p].IndexOf(prefix) == 0)
                m_ReqPoison.RemoveOrdered(p);
        }

        int loud = m_ReqLoud.Find(uid);
        if (loud != -1)
            m_ReqLoud.Remove(loud);
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

        // 2. Сторінка мусить існувати -- І МАТИ ОБРОБНИКА. Одне звернення до
        //    реєстру замість Has()+Get(), і null-перевірка на тому ж місці:
        //    сторінка без обробника до реєстру більше не потрапляє (див.
        //    OZ_PageRegistry.Register), але диспетчер, який тримає межу
        //    безпеки, не має покладатись на це на слово.
        OZ_PageEntry page = OZ_PageRegistry.Get(pageId);
        if (!page || !page.Handler)
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

        string res = page.Handler.Handle(op, json, sender, ok, err);

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

        // ПЕРША ПОЯВА ШТАМПУЄТЬСЯ ТУТ, а не в OZ_PlayerStore.Load.
        //
        // Там штамп діставався кожному uid, якого хтось колись спитав, --
        // офлайновому контакту з чужого записника, адресатові старого
        // повідомлення, -- і заводив йому файл. Це єдине місце, де ми точно
        // знаємо, що людина справді зайшла.
        if (d.FirstSeen == "")
            d.FirstSeen = OZ_Time.NowUtc();

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

        // ВОРОТА ПРИВЯЗКИ -- СЕРВЕРНІ (R-F2.2/R-F2.4, H37). Свірка з мостом на
        // вході й строк, після якого неприв'язаного виводять; подробиці --
        // в OZ_Link.OnConnect.
        OZ_Link.OnConnect(pArgs.Identity);

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

        // ЗВІРКИ СХЕМИ ТУТ БІЛЬШЕ НЕМАЄ, і не тому, що вона зайва, а тому, що
        // вона не могла спрацювати ЖОДНОГО разу. Сервер і клієнт крутять один
        // і той самий pbo, а клієнт з іншою збіркою обов'язкового мода до
        // сервера не приєднається взагалі (те саме сказано в OZ_Rpc про
        // перейменування RPC). Число в конверті лишається як позначка форми:
        // ctx.Read усе одно мусить чимось перевірити, що приїхав саме привіт.
        string uid = sender.GetPlainId();

        // ПРИВІТ -- ОДИН НА ВХІД, і повторювати його безкоштовно не можна.
        //
        // Кожен привіт гонить повний OZ_SyncSender.Send: читання файла
        // гравця, обхід усіх сторінок, збірка JSON і гарантований RPC. Штатний
        // клієнт вітається рівно раз (OZ_MissionGameplay), тож усе, що частіше,
        // -- або баг, або безкоштовна для клієнта точка підсилення навантаження.
        int now = GetGame().GetTime();
        int servedAt;
        if (m_HelloAt.Find(uid, servedAt) && (now - servedAt) < HELLO_GAP_MS)
        {
            OZ_Log.Dbg("hello: repeated within " + HELLO_GAP_MS.ToString() + " ms, ignored for " + uid);
            return;
        }
        m_HelloAt.Set(uid, now);

        OZ_SyncSender.Send(sender, "on request");
    }

    // Коли цьому гравцеві востаннє відповіли на привіт.
    private ref map<string, int> m_HelloAt = new map<string, int>();
    private static const int HELLO_GAP_MS = 3000;

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

        // ПОРОЖНІЙ UID -- НЕ ГРАВЕЦЬ. PathOf клеїть uid просто в дорогу, тож
        // Load("") завів би під профілем сервера файл `players\.json` і носив
        // би в ньому чиюсь дату виходу. CF віддає UID окремим полем саме
        // тому, що особи на дисконекті може вже не бути, -- значить порожнє
        // тут можливе, і мовчки перетворювати його на файл не можна.
        if (dArgs.UID == "")
        {
            OZ_Log.Dbg("disconnect with no uid, nothing to write");
            return;
        }

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
        if (m_HelloAt.Contains(dArgs.UID))
            m_HelloAt.Remove(dArgs.UID);

        // Опитування моста про його код прив'язки теж припиняємо: воно жило
        // до десяти хвилин і не знало, що питати вже нема про кого. Разом із
        // ним іде й строк воріт.
        OZ_Link.Leave(dArgs.UID);

        OZ_Log.Dbg("disconnect " + dArgs.UID);
    }

    // StartBridge ТУТ БІЛЬШЕ НЕМАЄ: опит пускає Ready() вище, одразу після
    // рядка готовності. Причина в OnMissionStart -- обидві дії чекають на ту
    // саму мить, коли всі чужі OnMissionStart відпрацювали, і двох таймерів
    // на одну мить не треба.

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
        OZ_PlayerStore.FlushSome(FLUSH_PER_TICK, FLUSH_TICKS);
    }
}
