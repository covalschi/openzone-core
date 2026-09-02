// Адмінська сторінка фракційної системи: ростер і пермадес.
//
// ОКРЕМИЙ РОЗДІЛ, а не операції ядрового. Ядро лишає собі редактор конфігів
// і спавни -- те, що є в нього завжди; ростер із рангами, званнями, рисами
// й призначенням фракцій має сенс рівно там, де стоїть цей мод. Сервер без
// нього не мусить бачити в консолі половину кнопок, які нічого не роблять.
//
// Межа безпеки НЕ ТУТ, і це зміна 2026-09-01 (ТЗ-5 §C3): права перевіряє
// диспетчер ядра в OZ_AdminReq, першим рядком, один раз на всі розділи всіх
// модів. Друга перевірка тут виглядала б обережною, а насправді робила б
// правило розсипаним по модах -- і мовчазний розділ без неї виглядав би так
// само, як розділ, що чесно відмовив.

class OZF_Const
{
    // Ім'я АДМІНСЬКОГО РОЗДІЛУ, не сторінки. Рядок той самий, місце інше:
    // розділи живуть у OZ_AdminRegistry і їдуть конвертом OZ_AdminReq.
    static const string SECTION = "factions";
}

class OZ_AdminRosterRow
{
    string Name    = "";
    string Uid     = "";
    // ОБИДВІ осі, і саме для адміна це важливо найбільше: він єдиний, хто
    // бачить різницю між «одинак-легенда» і «не заходив жодного разу», а
    // одне поле їх не розрізняє.
    string Base    = "";
    string Org     = "";
    string DName   = "";
    string Traits  = "";
    string Rank    = "";
    string FRank   = "";
    bool   Leader  = false;
}

class OZ_AdminRoster
{
    ref array<ref OZ_AdminRosterRow> Rows;
    ref array<string> Factions;

    // Каталоги з реєстру бота: адмiну треба з чого вибирати. FRanks --
    // внутрiфракцiйнi звання, id вигляду "duty:sergeant".
    ref array<string> Traits;
    ref array<string> Ranks;
    ref array<string> FRanks;

    void OZ_AdminRoster()
    {
        Rows     = new array<ref OZ_AdminRosterRow>();
        Factions = new array<string>();
        Traits   = new array<string>();
        Ranks    = new array<string>();
        FRanks   = new array<string>();
    }
}

class OZ_AdminWipeAsk
{
    string Uid = "";

    // «Ігрову половину вже зроблено». Ставить ГРА, коли пермадес почався в
    // нiй: тодi мiст робить лише своє й не шле поштовх назад. Порожнє (з
    // команди бота) означає протилежне -- мiст мусить розбудити гру.
    bool FromGame = false;
}

// Вiдповiдь моста на команду: вдалося чи нi, i чому.
class OZ_BridgeAck
{
    bool   Ok  = false;
    string Why = "";
}

// Мiст вiдповiв на вайп -- доносимо вiдповiдь адмiновi. Iгрова половина
// на цю мить УЖЕ зроблена: якщо мiст вiдмовив, адмiн бачить причину й
// повторює команду -- iгрова половина iдемпотентна (епоха просто пiде ще
// на крок уперед, порожнi списки лишаться порожнiми).
class OZ_AdminWipeReply : OZ_BridgeReply
{
    protected string m_AdminUid;
    protected string m_Op;

    void OZ_AdminWipeReply(string adminUid, string op)
    {
        m_AdminUid = adminUid;
        m_Op       = op;
    }

    override void OnBody(string json)
    {
        PlayerIdentity to = OZ_Link.Online(m_AdminUid);
        if (!to)
            return;

        OZ_BridgeAck ack;
        string err;
        if (!JsonFileLoader<OZ_BridgeAck>.LoadData(json, ack, err) || !ack)
        {
            OZ_Rpc.AdminRespond(to, OZF_Const.SECTION, m_Op, false, "", "STR_OZ_ERR_INTERNAL");
            return;
        }

        if (!ack.Ok)
        {
            OZ_Log.Warn("admin: bridge refused the wipe: " + ack.Why);
            OZ_Rpc.AdminRespond(to, OZF_Const.SECTION, m_Op, false, "", ack.Why);
            return;
        }

        OZ_Rpc.AdminRespond(to, OZF_Const.SECTION, m_Op, true, "{}", "");
    }

    override void OnFail(int code)
    {
        PlayerIdentity to = OZ_Link.Online(m_AdminUid);
        if (!to)
            return;
        OZ_Rpc.AdminRespond(to, OZF_Const.SECTION, m_Op, false, "", "STR_OZ_ERR_NO_BRIDGE");
    }
}

// ІГРОВА ПОЛОВИНА ПЕРМАДЕСУ, окремо від того, хто її попросив.
//
// Просять двоє: адмінська консоль у грі (і тоді вона ж кличе міст) і сам
// МІСТ -- коли пермадес запустили командою бота, а не з гри. Без цього
// класу друга дорога робила лише половину справи: ролі в Discord
// скидались, а КПК небіжчика лишались живими, бо гра про смерть не чула.
class OZ_PlayerWipe
{
    static void Local(string uid)
    {
        if (!GetGame().IsServer())
            return;
        if (uid == "")
            return;

        // СПЕРШУ ЗАМОРОЗКА, потiм чистка. Старе життя лягає в окремий файл
        // цiлим -- з контактами, нотатками й усiм, що в ньому було, -- i
        // лише пiсля цього живий запис стає новим персонажем.
        OZ_PlayerStore.Freeze(uid);

        OZ_PlayerData d = OZ_PlayerStore.Load(uid);
        d.SessionEpoch = d.SessionEpoch + 1;

        if (d.Friends)
            d.Friends.Clear();
        if (d.FriendReq)
            d.FriendReq.Clear();
        if (d.NpcContacts)
            d.NpcContacts.Clear();
        if (d.Chats)
            d.Chats.Clear();
        if (d.TransponderTo)
            d.TransponderTo.Clear();

        d.TransponderMode = "off";
        d.PresenceHidden  = false;
        d.BaseFaction     = "";
        d.OrgFaction      = "";
        d.SeenBase        = "";
        d.SeenOrg         = "";
        d.SeenRank        = "";
        d.SeenFRank       = "";
        if (d.SeenPosts)
            d.SeenPosts.Clear();
        if (d.SeenTraits)
            d.SeenTraits.Clear();

        OZ_PlayerStore.Flush(uid);

        // Точки спавну: одноразова й особиста. ClearPersonal чесно скаже
        // «не було» -- нам однаково, головне, що пiсля вайпу її немає.
        OZ_Spawns.ClearNextSpawn(uid);
        OZ_Spawns.ClearPersonal(uid);

        // ЧУЖІ ЗАПИСНИКИ НЕ ЧІПАЄМО, і це рішення власника 2026-08-30.
        //
        // Викреслити небiжчика з чужих контактiв означало б РОЗПОВIСТИ про
        // його смерть: рядок, який зник, читається однозначно. КПК не
        // повiдомляє про смерть -- нiколи. Запис лишається на мiсцi
        // замороженим, з датою останньої появи в Зонi, i чи людина загинула,
        // чи просто не заходить, з нього не видно.
        //
        // Нове життя того самого акаунта -- ОКРЕМИЙ запис (uid#покоління),
        // якого нi в кого ще немає: знайомитись доведеться наново.

        // Проекцiю ролей забуваємо: мiст пришле нову, вже новачкову.
        OZ_Roles.Forget(uid);

        OZ_Log.Info("player " + uid + " wiped: generation frozen, devices sealed");
    }
}

// Поштовх «цього гравця стерли» -- від моста. Приходить, коли пермадес
// запустили командою бота: гра робить свою половину тут.
class OZ_WipeSink : OZ_BridgeSink
{
    override void Deliver(string json)
    {
        OZ_AdminWipeAsk a;
        string err;
        if (!JsonFileLoader<OZ_AdminWipeAsk>.LoadData(json, a, err) || !a)
        {
            OZ_Log.Warn("wipe: unreadable push from the bridge: " + err);
            return;
        }

        OZ_PlayerWipe.Local(a.Uid);
    }
}

class OZF_AdminSection : OZ_AdminSection
{
    override string Handle(string op, string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        // Прав тут не питаємо: їх спитав диспетчер ядра, першим рядком, до
        // розбору операції. Межа безпеки одна на всі розділи всіх модів.

        if (op == "roster")
            return Roster(ok, error);

        // Пермадес. Iм'я живе в операцiї з тiєї ж причини, що й у cfg_*:
        // uid короткий, але правило одне на всi адмiнськi операцiї.
        if (op.IndexOf("player_wipe:") == 0)
            return PlayerWipe(op.Substring(12, op.Length() - 12), op, sender, ok, error);

        error = "STR_OZ_ERR_UNKNOWN_OP";
        return "";
    }

    // «Чистий аркуш»: персонаж помер назавжди, ГРАВЕЦЬ лишається.
        //
        // Ігрова половина -- тут i одразу: епоха сесiй +1 (всi його КПК
        // замерзають назавжди -- сесiю вiдкриває лише безхазяйний пристрiй,
        // а цi назавжди лишаються зайнятими мертвою сесiєю), друзi, запити,
        // групи, транспондер, особиста точка спавну -- геть. Прив'язка Discord
        // ЛИШАЄТЬСЯ: гравець той самий, це персонаж новий.
        //
        // Половина моста (вихiд iз приватних тредiв, скидання ролей до
        // новачка) їде викликом v1/player/wipe, i вiдповiдь клiєнтовi -- ТIЛЬКИ
        // пiсля неї: адмiн мусить знати, що вайп пройшов ЦIЛКОМ, а не наполовину.
        private string PlayerWipe(string uid, string op, PlayerIdentity sender, out bool ok, out string error)
        {
            if (uid == "")
            {
                error = "STR_OZ_ERR_NO_TARGET";
                return "";
            }

            // Мiст питаємо ПЕРШИМ: якщо його немає, не робимо НIЧОГО. Половина
            // вайпу гiрша за жодного -- замерзлi КПК при живих тредах виглядали
            // б як баг, а не як смерть.
            if (!OZ_BridgeClient.Alive())
            {
                error = "STR_OZ_ERR_NO_BRIDGE";
                return "";
            }

            OZ_PlayerWipe.Local(uid);
            OZ_Log.Info("admin: player " + uid + " wiped by " + sender.GetPlainId());

            OZ_AdminWipeAsk a = new OZ_AdminWipeAsk();
            a.Uid = uid;
            // Гру ми вже вiдпрацювали самi -- хай мiст не шле нам поштовх назад.
            a.FromGame = true;

            string letter;
            string jerr;
            if (!JsonFileLoader<OZ_AdminWipeAsk>.MakeData(a, letter, jerr, false))
            {
                error = "STR_OZ_ERR_INTERNAL";
                return "";
            }

            OZ_BridgeClient.Call("v1/player/wipe", letter, new OZ_AdminWipeReply(sender.GetPlainId(), op));

            // Вiдповiдь пiде з OZ_AdminWipeReply, коли мiст вiдпишеться.
            ok    = false;
            error = OZ_Const.DEFER;
            return "";
        }

    private string Roster(out bool ok, out string error)
        {
            OZ_AdminRoster r = new OZ_AdminRoster();
            OZ_Factions.Ids(r.Factions);
            OZ_Roles.TraitIds(r.Traits);
            OZ_Roles.RankIds(r.Ranks);
            OZ_Roles.FRankIds(r.FRanks);

            array<Man> players = new array<Man>();
            GetGame().GetPlayers(players);

            for (int i = 0; i < players.Count(); i++)
            {
                if (!players[i])
                    continue;
                PlayerIdentity id = players[i].GetIdentity();
                if (!id)
                    continue;

                OZ_AdminRosterRow row = new OZ_AdminRosterRow();
                row.Name    = id.GetName();
                row.Uid     = id.GetPlainId();
                row.Base    = OZ_Factions.BaseOfUid(row.Uid);
                row.Org     = OZ_Factions.OrgOfUid(row.Uid);
                row.DName   = OZ_Roles.DiscordNameOf(row.Uid);
                row.Traits  = OZ_Roles.TraitsLineOf(row.Uid);
                row.Rank    = OZ_Roles.RankOf(row.Uid);
                row.FRank   = OZ_Roles.FRankOf(row.Uid);
                row.Leader  = OZ_Roles.IsLeader(row.Uid);
                r.Rows.Insert(row);
            }

            string outJson;
            string err;
            if (!JsonFileLoader<OZ_AdminRoster>.MakeData(r, outJson, err, false))
            {
                error = "STR_OZ_ERR_INTERNAL";
                return "";
            }

            ok = true;
            return outJson;
        }
}
