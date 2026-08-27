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
    private static const float FLUSH_INTERVAL = 30.0;

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

        // Фракції -- служба ядра, бо їх питає не лише екран: квести,
        // торгівля, ІІ, рація. Ідемпотентна, тож КПК і далі кличе її в себе
        // -- порядок CF-модулів не гарантований, і на цьому стенді він уже
        // підводив.
        OZ_Factions.ServerLoad();
        OZ_Spawns.ServerLoad();

        OZ_Rpc.RegisterServer(this);

        m_FlushTimer = new Timer(CALL_CATEGORY_SYSTEM);
        m_FlushTimer.Run(FLUSH_INTERVAL, this, "FlushTick", NULL, true);

        // Рід "roles" -- другий у проєкті після "chat", і механізм родів
        // нарешті везе більше за один. Підписка ДО Start(): перша ж пачка
        // може приїхати раніше, ніж наступний рядок виконається.
        OZ_BridgeClient.Subscribe("roles", new OZ_RolesSink());
        OZ_BridgeClient.Subscribe("roster", new OZ_RosterSink());

        OZ_BridgeClient.Start();

        string summary = "core loaded: admins=" + s.AdminIds.Count();
        summary += " perms=" + OZ_Perm.Describe();
        summary += " pages=" + OZ_PageRegistry.Count().ToString();
        summary += " factions=" + OZ_Factions.Count().ToString();
        summary += " spawnzones=" + OZ_Spawns.Count().ToString();

        // Стейджинґ окремим словом, а не в лічильнику зон: він або є, або
        // його немає, і адмін мусить бачити відповідь, не рахуючи рядки.
        if (OZ_Spawns.HasStaging())
            summary += " staging=on";
        else
            summary += " staging=off";

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

    // Зміна ролей із гри. Особа -- ЗАВЖДИ з sender: клієнт не називає, від
    // чийого імені просить, і не може -- у конверті немає такого поля.
    void OZ_RoleReq(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server)
            return;

        Param3<string, string, string> data;
        if (!ctx.Read(data))
            return;

        if (!sender)
            return;

        string op         = data.param1;
        string targetName = data.param2;
        string arg        = data.param3;

        // Ім'я -> особа, і тільки серед тих, хто В ЗОНІ. Клієнт назвав рядок;
        // кому він належить, вирішуємо ми.
        string targetUid = "";
        if (targetName != "")
            targetUid = OZ_RoleOps.UidByName(targetName, sender.GetPlainId());

        // Запрошення -- НЕ операція над ролями, тому й не йде в OZ_RoleOps:
        // до згоди воно взагалі нічого не міняє в Discord.
        if (op == "invite")
        {
            OZ_FactionInvites.Offer(sender, targetUid);
            return;
        }

        if (op == "accept")
        {
            OZ_FactionInvites.Accept(sender);
            return;
        }

        if (op == "decline")
        {
            OZ_FactionInvites.Decline(sender);
            return;
        }

        // Зони спавна -- не про гравця й не про Discord, тому окремою гілкою
        // й без цілі: адмін ставить зону ТАМ, ДЕ СТОЇТЬ САМ.
        if (op == OZ_RoleOp.SPAWN_HERE || op == OZ_RoleOp.SPAWN_CLEAR)
        {
            OZ_SpawnOps.Handle(sender, op, arg);
            return;
        }

        OZ_RoleOps.Request(sender, targetUid, op, arg);
    }

    // Порядок перевірок нижче -- і є межа безпеки. Міняти його не можна.
    void OZ_Req(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server)
            return;

        Param3<string, string, string> data;
        if (!ctx.Read(data))
            return;

        string pageId = data.param1;
        string op     = data.param2;
        string json   = data.param3;

        // 1. Особа -- ЗАВЖДИ з sender. Ніколи з корисного навантаження:
        //    туди клієнт напише що завгодно.
        if (!sender)
            return;

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
            string w2 = "rejected page \"" + pageId;
            w2 += "\" from " + sender.GetPlainId();
            w2 += ": not on this device";
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

        SendSync(sender, OZ_Perm.IsAdmin(sender));
    }

    private void SendSync(PlayerIdentity to, bool admin)
    {
        OZ_SyncPayload p = new OZ_SyncPayload();
        p.Schema    = OZ_Const.SCHEMA_SETTINGS;
        p.IsAdmin   = admin;
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

        // Запрошення до того, хто вийшов, показувати більше нікому.
        OZ_FactionInvites.Forget(dArgs.UID);

        // І ПРОЕКЦІЮ РОЛЕЙ теж забуваємо.
        //
        // Вона -- відповідь моста на «зараз», а не властивість гравця. Поки
        // він у Зоні, міст присилає її щоразу, як вона змінюється; щойно
        // вийшов -- присилати перестає, і те, що лишилось у пам'яті, з
        // кожною хвилиною все менше схоже на правду. Гравець міг відв'язати
        // акаунт, вийти з гільдії, втратити фракцію -- ми про це вже не
        // почуємо.
        //
        // Прибираємо ЗАПИС, а не ставимо порожній: «ролі немає» і «ми не
        // знаємо» -- різні відповіді, і саме на цій різниці тримається
        // запасний шлях через файл акаунта.
        //
        // До цієї правки OZ_Roles.Forget не кликав ніхто, а разом із ним був
        // недосяжний і OZ_Factions.ForgetRole -- тобто розрізнення, яке вони
        // боронять, не працювало ЖОДНОГО разу.
        OZ_Roles.Forget(dArgs.UID);

        OZ_Log.Dbg("disconnect " + dArgs.UID);
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
