// Серверний модуль фракційної системи.
//
// Усе, що ядро колись робило для фракцій, робиться тепер тут: читання
// конфігу, підписки на роди моста, реєстрація адмінської сторінки, RPC змін
// ролей і прибирання за гравцем на виході.
//
// ЯДРО ПРО ЦЕЙ МОД НЕ ЗНАЄ. Зв'язок односторонній: ми підставляємо свою
// реалізацію в OZ_Identity, і всі, кому треба знати, чия людина перед ними,
// питають ЯДРО, а не нас. Тому КПК, рація й будь-який майбутній мод
// компілюються й працюють без цього pbo.
//
// РОДИ МОСТА ПІДПИСУЄМО САМІ. Раніше це робило ядро -- за нас і за КПК, --
// і через це список родів у ядрі мусив знати про кожен мод серії. Тепер
// кожен просить своє, а сервер вирішує, що дозволити (OZ_BridgeSettings.Kinds).

[CF_RegisterModule(OZF_Module)]
class OZF_Module : CF_ModuleWorld
{
    override void OnInit()
    {
        super.OnInit();

        EnableMissionStart();
        EnableInvokeDisconnect();
    }

    override void OnMissionStart(Class sender, CF_EventArgs args)
    {
        super.OnMissionStart(sender, args);

        if (!GetGame().IsServer())
            return;

        OZ_Factions.ServerLoad();
        OZF_Loadouts.ServerLoad();

        // Спорядження на появі (ТЗ-3): драбина фракцій, звань, посад і міток
        // живе тут; ядро лише роздягає й одягає те, що ми назвемо.
        OZ_Loadout.Provide(new OZF_LoadoutService());

        // Ось той самий односторонній зв'язок: ядро отримує реалізацію й
        // роздає її всім охочим, не знаючи, звідки вона.
        OZ_Identity.Provide(new OZF_Identity());

        // Адмінський РОЗДІЛ -- НАШ, окремо від ядрових. Ядро лишає собі
        // спавни й редактор конфігів; ролі, ранги, звання й призначення
        // фракцій живуть тут, разом із кодом, який їх виконує.
        //
        // РОЗДІЛ, А НЕ СТОРІНКА (ТЗ-5 §C2). Саме через це ця половина вкладки
        // VPP не працювала жодного разу: сторінку "factions" не просив ніхто
        // в усьому дереві -- клієнт слав усе на "admin", де знали лише
        // cfg_*, -- а якби й просив, гейт КПК вимагав би від адміна
        // розімкнений увімкнений прилад із цією сторінкою в профілі.
        OZ_AdminRegistry.Register(OZF_Const.SECTION, new OZF_AdminSection());

        // Ролі й ростер -- НАШІ роди. Підписка з OnMissionStart: міст
        // стартує на тік пізніше саме для того, щоб ця встигла.
        OZ_BridgeClient.Subscribe("roles", new OZ_RolesSink());
        OZ_BridgeClient.Subscribe("roster", new OZ_RosterSink());

        // Пермадес, запущений НЕ з гри: команда бота робить свою половину й
        // штовхає сюди, щоб гра зробила свою.
        OZ_BridgeClient.Subscribe("wipe", new OZ_WipeSink());

        OZ_Rpc.RegisterRoles(this);

        OZ_Log.Info("factions loaded: " + OZ_Factions.Count().ToString() + " faction(s)");

        // МІСТ ДЛЯ ФРАКЦІЙ ОБОВ'ЯЗКОВИЙ, і мовчати про це не можна (ТЗ-2 R2.1).
        //
        // Базову фракцію призначає гра й тримає у файлі гравця, тож вона є й
        // при мертвому мості (ТЗ-1 R5.4). Усе інше -- угруповання, звання,
        // трейти, склад, лідерство і зняття ролей на виході -- живе в боті, і
        // без нього цей мод не працює, а ЗОБРАЖУЄ роботу: екрани малюються,
        // склад порожній, лідерські кнопки відмовляють по одній.
        //
        // Конфігурації «фракції без моста» не існує. Кажемо це рядком на
        // буті, а не залишаємо адмінові з'ясовувати по симптомах.
        OZ_BridgeSettings b = OZ_Settings.Get().Bridge;
        if (!b || !b.Enabled)
            OZ_Log.Warn("factions need the bridge: the bot owns organisations, ranks, traits and leadership. With Bridge.Enabled false only the base faction works");
    }

    // Роль -- відповідь моста на «зараз», а не властивість гравця. Поки він
    // у Зоні, міст присилає її щоразу, як вона змінюється; щойно вийшов --
    // присилати перестає, і те, що лишилось у пам'яті, з кожною хвилиною все
    // менше схоже на правду.
    //
    // Прибираємо ЗАПИС, а не ставимо порожній: «ролі немає» і «ми не знаємо»
    // -- різні відповіді, і саме на цій різниці тримається запасний шлях
    // через файл акаунта.
    override void OnInvokeDisconnect(Class sender, CF_EventArgs args)
    {
        super.OnInvokeDisconnect(sender, args);

        if (!GetGame().IsServer())
            return;

        auto dArgs = CF_EventPlayerDisconnectedArgs.Cast(args);
        if (!dArgs)
            return;

        OZ_Roles.Forget(dArgs.UID);

        // Запрошення до того, хто вийшов, показувати більше нікому. Стояло в
        // ядрі й пережило винесення -- через що набір без цього мода не
        // компілювався зовсім; місце йому тут, поруч із рештою нашого.
        OZ_FactionInvites.Forget(dArgs.UID);
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
        //
        // АДМІНСЬКИЙ ВИНЯТОК: адреса "uid:<steam64>" називає особу точно.
        // Приймається ЛИШЕ від адміна -- консоль і так бачить uid-и в
        // ростері, а лідерські операції лишаються іменними: межа «клієнт не
        // оперує чужими Steam64» стоїть для гравців, не для адмінки.
        string targetUid = "";
        if (targetName.IndexOf("key:") == 0)
        {
            // КЛЮЧ ПЕРСОНАЖА (ТЗ-4 R-C4.1) у тій самій непрозорій формі, що
            // й у контактах: клієнт бачить хеш, не Steam64. Розгортається
            // лише серед тих, кого відправник і так може назвати -- його
            // друзі, його угруповання, присутні, -- і називає ОДНЕ живе
            // життя: ключ вайпнутого персонажа не називає нікого. Адресація
            // іменем лишається нижче як застаріла: вона мовчки не працювала
            // ні на відсутніх, ні на тезках.
            string tag = targetName.Substring(4, targetName.Length() - 4);
            targetUid = OZ_RoleOps.UidByTag(tag, sender.GetPlainId());
            if (targetUid == "")
            {
                OZ_Rpc.RoleRespond(sender, op, false, "STR_OZ_ERR_NO_TARGET");
                return;
            }
        }
        else if (targetName.IndexOf("uid:") == 0)
        {
            if (!OZ_Perm.IsAdmin(sender))
            {
                OZ_Rpc.RoleRespond(sender, op, false, "STR_OZ_ERR_ADMIN_ONLY");
                return;
            }
            targetUid = targetName.Substring(4, targetName.Length() - 4);
        }
        else if (targetName != "")
        {
            targetUid = OZ_RoleOps.UidByName(targetName, sender.GetPlainId());

            // СЕБЕ ЗА ІМЕНЕМ ТЕЖ МОЖНА. UidByName виключає відправника,
            // щоб тезка не пiдставився пiд чужу операцiю, -- але той, хто
            // називає ВЛАСНЕ iм'я, не двозначний (змiряно 2026-08-30:
            // єдиний гравець на стендi не мiг призначити фракцiю нiкому).
            if (targetUid == "" && sender.GetName() == targetName)
                targetUid = sender.GetPlainId();
        }

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

        // ПІТИ САМОМУ. Ціль -- завжди сам відправник, і саме тому це окрема
        // операція, а не faction.clear з власним ім'ям у полі: ім'я треба
        // спершу знайти серед присутніх, а піти з фракції людина має право
        // незалежно від того, чи є в Зоні хтось із таким самим ім'ям.
        if (op == "leave")
        {
            OZ_RoleOps.Request(sender, sender.GetPlainId(), OZ_RoleOp.FACTION_CLEAR, "");
            return;
        }

        // ЗОН СПАВНА ТУТ БІЛЬШЕ НЕМАЄ. Вони повернулись у ядро власним
        // адмінським розділом (ТЗ-5 §C1 R6, §C4 R-C4.6): зони, файл зон і
        // панель SPAWNS у вкладці VPP -- ядрові, і поки їхній обробник жив
        // тут, сервер без мода фракцій не міг завести жодної зони.
        OZ_RoleOps.Request(sender, targetUid, op, arg);
    }
}

// Реалізація служби ядра. Тонка обгортка й нічого більше: правила живуть у
// OZ_Factions та OZ_Roles, а тут лише переклад із мови ядра на нашу.
class OZF_Identity : OZ_IdentityService
{
    override string BaseOf(string uid)
    {
        return OZ_Factions.BaseOfUid(uid);
    }

    override string OrgOf(string uid)
    {
        return OZ_Factions.OrgOfUid(uid);
    }

    override string FactionName(string id)
    {
        return OZ_Factions.NameOf(id);
    }

    override string OrgOfPlayer(PlayerBase player, string uid)
    {
        return OZ_Factions.OrgOf(player, uid);
    }

    // Перший вхід: базова фракція з'являється тут і більше ніде.
    //
    // «Яка саме» -- перша з BaseFaction: true в порядку OZ_Core_Factions.json
    // (ТЗ-1 R5.2). Порядок файлу і є відповіддю: перевпорядкувати його адмін
    // уміє, а окреме поле «головна базова» було б другим джерелом правди про
    // одне й те саме.
    override void EnsureBase(string uid)
    {
        if (!GetGame().IsServer())
            return;
        if (uid == "")
            return;
        if (OZ_Factions.BaseOfUid(uid) != "")
            return;

        string slug = OZ_Factions.FirstBaseId();
        if (slug == "")
        {
            // РАЗ НА ЗАПУСК, а не на кожен вхід. Це стан файла налаштувань:
            // він не зміниться від того, що зайшов ще один гравець, а рядок
            // на кожного перетворив би лог на шум рівно там, де адмін і мав
            // би прочитати цю єдину фразу.
            if (!s_WarnedNoBase)
            {
                s_WarnedNoBase = true;
                OZ_Log.Warn("factions: no faction is marked BaseFaction in OZ_Core_Factions.json - nobody gets a base faction, and spawn zones fall back to staging");
            }
            return;
        }

        OZ_Factions.SetBaseOf(uid, slug);
        OZ_Log.Info("factions: " + uid + " joins the Zone as \"" + slug + "\"");
    }

    private static bool s_WarnedNoBase = false;

    override string FactionShort(string id)
    {
        return OZ_Factions.ShortOf(id);
    }

    override int FactionColor(string id, int alpha)
    {
        return OZ_Factions.ColorARGB(id, alpha);
    }

    override int FactionCount()
    {
        return OZ_Factions.Count();
    }

    override void FactionIds(out array<string> outIds)
    {
        OZ_Factions.Ids(outIds);
    }

    override string Stand(string a, string b)
    {
        return OZ_Factions.Stand(a, b);
    }

    override bool AreHostile(string a, string b)
    {
        return OZ_Factions.AreHostile(a, b);
    }

    override bool AreFriendly(string a, string b)
    {
        return OZ_Factions.AreFriendly(a, b);
    }

    override string RankOf(string uid)
    {
        return OZ_Roles.RankOf(uid);
    }

    override string FRankOf(string uid)
    {
        return OZ_Roles.FRankOf(uid);
    }

    override bool IsLeader(string uid)
    {
        return OZ_Roles.IsLeader(uid);
    }

    override bool HasPost(string uid, string post)
    {
        return OZ_Roles.HasPost(uid, post);
    }

    override bool Stale()
    {
        return OZ_Roles.Stale();
    }

    override bool PendingInvite(string uid, out string factionId, out string fromName)
    {
        factionId = "";
        fromName  = "";

        OZ_FactionInvite inv = OZ_FactionInvites.Pending(uid);
        if (!inv)
            return false;

        factionId = inv.Faction;
        fromName  = inv.FromName;
        return true;
    }

    override string SeenRankName(string uid)
    {
        OZ_RoleView v = OZ_Roles.Seen(uid);
        if (!v)
            return "";
        return OZ_RoleNames.Of(v.Rank);
    }

    override void SeenTraitNames(string uid, out array<string> outNames)
    {
        if (!outNames)
            return;

        OZ_RoleView v = OZ_Roles.Seen(uid);
        if (!v)
            return;

        for (int i = 0; i < v.Traits.Count(); i++)
            outNames.Insert(OZ_RoleNames.Of(v.Traits[i]));
    }

    override string RankName(string uid)
    {
        return OZ_RoleNames.Of(OZ_Roles.RankOf(uid));
    }

    override void TraitNames(string uid, out array<string> outNames)
    {
        if (!outNames)
            return;

        OZ_RoleView v = OZ_Roles.Of(uid);
        if (!v)
            return;

        for (int i = 0; i < v.Traits.Count(); i++)
            outNames.Insert(OZ_RoleNames.Of(v.Traits[i]));
    }
}
