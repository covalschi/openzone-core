// Адмiнська консоль: серверна половина, з якою говорить будь-який
// адмiнський UI (першою -- вкладка VPP).
//
// Одна сторiнка ("admin"), жменька операцiй, i ворота -- OZ_Perm.IsAdmin
// на КОЖНIЙ. Пристрiй тут нi до чого: адмiну не потрiбен КПК у руках, щоб
// правити конфiг, тому гейт доступу КПК пропускає цю сторiнку наскрiзь, а
// справжня межа безпеки стоїть тут.

// Один редагований конфiг: як його звати, де лежить файл i хто вмiє
// ПЕРЕВIРИТИ й ЗАСТОСУВАТИ новий текст. Реєструє власник конфiгу -- ядро
// свої, КПК свої; ядро не знає чужих типiв i знати не мусить.
class OZ_AdminCfgApplier
{
    // Розiбрати текст, зберегти файл i перечитати живий конфiг. Повертає
    // false, коли текст не розбирається, -- ФАЙЛ ТОДI НЕ ЧIПАЄТЬСЯ: смiття
    // не досягає диска, i чинна версiя лишається чинною.
    bool Apply(string json)
    {
        return false;
    }
}

class OZ_AdminCfgEntry
{
    string Name;
    string Path;
    // Чий конфіг: "core" чи "pda" -- вкладки адмінки діляться за власником.
    string Owner;
    ref OZ_AdminCfgApplier Applier;
}

class OZ_AdminCfg
{
    private static ref array<ref OZ_AdminCfgEntry> s_All;

    static void Register(string name, string path, OZ_AdminCfgApplier applier, string owner = "core")
    {
        if (!s_All)
            s_All = new array<ref OZ_AdminCfgEntry>();

        OZ_AdminCfgEntry e = new OZ_AdminCfgEntry();
        e.Name    = name;
        e.Path    = path;
        e.Owner   = owner;
        e.Applier = applier;
        s_All.Insert(e);
    }

    static OZ_AdminCfgEntry Find(string name)
    {
        if (!s_All)
            return null;
        for (int i = 0; i < s_All.Count(); i++)
        {
            if (s_All[i].Name == name)
                return s_All[i];
        }
        return null;
    }

    static void Names(array<string> outNames, array<string> outOwners)
    {
        if (!s_All)
            return;
        for (int i = 0; i < s_All.Count(); i++)
        {
            outNames.Insert(s_All[i].Name);
            outOwners.Insert(s_All[i].Owner);
        }
    }
}

// Конверти операцiй.
class OZ_AdminAsk
{
    string Name = "";
    string Json = "";
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
            OZ_Rpc.Respond(to, OZ_Const.PAGE_ADMIN, m_Op, false, "", "STR_OZ_ERR_INTERNAL");
            return;
        }

        if (!ack.Ok)
        {
            OZ_Log.Warn("admin: bridge refused the wipe: " + ack.Why);
            OZ_Rpc.Respond(to, OZ_Const.PAGE_ADMIN, m_Op, false, "", ack.Why);
            return;
        }

        OZ_Rpc.Respond(to, OZ_Const.PAGE_ADMIN, m_Op, true, "{}", "");
    }

    override void OnFail(int code)
    {
        PlayerIdentity to = OZ_Link.Online(m_AdminUid);
        if (!to)
            return;
        OZ_Rpc.Respond(to, OZ_Const.PAGE_ADMIN, m_Op, false, "", "STR_OZ_ERR_NO_BRIDGE");
    }
}

class OZ_AdminCfgList
{
    ref array<string> Names;
    ref array<string> Owners;

    void OZ_AdminCfgList()
    {
        Names  = new array<string>();
        Owners = new array<string>();
    }
}

class OZ_AdminRosterRow
{
    string Name    = "";
    string Uid     = "";
    string Faction = "";
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

class OZ_AdminPage : OZ_PageHandler
{
    override string Handle(string op, string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        // МЕЖА БЕЗПЕКИ. Сторiнка зареєстрована в загальному реєстрi, i будь-який
        // клiєнт може назвати її iм'я -- вiдповiдає ця перевiрка, i тiльки вона.
        if (!sender || !OZ_Perm.IsAdmin(sender))
        {
            error = "STR_OZ_ERR_ADMIN_ONLY";
            return "";
        }

        if (op == "cfg_list")
            return CfgList(ok, error);

        // Iм'я конфiгу живе В САМIЙ операцiї, а тiло їде СИРИМ тiлом
        // запиту/вiдповiдi. Конверт зi строковим полем тут заборонений:
        // JsonFileLoader рiже строкове ЗНАЧЕННЯ на 1023 байтах при розборi
        // (змiряно 2026-08-30: envelope=2925, body=1023), i будь-який конфiг
        // довший за кiлобайт приїздив обрубком.
        if (op.IndexOf("cfg_get:") == 0)
            return CfgGet(op.Substring(8, op.Length() - 8), ok, error);
        if (op.IndexOf("cfg_set:") == 0)
            return CfgSet(op.Substring(8, op.Length() - 8), json, sender, ok, error);

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

    private string CfgList(out bool ok, out string error)
    {
        OZ_AdminCfgList l = new OZ_AdminCfgList();
        OZ_AdminCfg.Names(l.Names, l.Owners);

        string outJson;
        string err;
        if (!JsonFileLoader<OZ_AdminCfgList>.MakeData(l, outJson, err, false))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        ok = true;
        return outJson;
    }

    private string CfgGet(string name, out bool ok, out string error)
    {
        OZ_AdminCfgEntry e = OZ_AdminCfg.Find(name);
        if (!e)
        {
            error = "STR_OZ_ERR_NO_SUCH_CFG";
            return "";
        }

        // Сирий текст файлу, а не пересерiалiзований об'єкт: адмiн править
        // САМЕ ТЕ, що лежить на диску, разом iз вiдсутнiми полями й усiм.
        ok = true;
        return ReadFileText(e.Path);
    }

    private string CfgSet(string name, string body, PlayerIdentity sender, out bool ok, out string error)
    {
        OZ_AdminCfgEntry e = OZ_AdminCfg.Find(name);
        if (!e || !e.Applier)
        {
            error = "STR_OZ_ERR_NO_SUCH_CFG";
            return "";
        }

        // Розбiр -- ДО диска: смiття не досягає файлу, чинна версiя чинна.
        // Хто саме зламався -- скаже лог; клiєнтовi досить «не розiбралось».
        if (!e.Applier.Apply(body))
        {
            error = "STR_OZ_ERR_CFG_REJECTED";
            return "";
        }

        OZ_Log.Info("admin: config " + name + " applied by " + sender.GetPlainId());

        ok = true;
        return "{}";
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
            row.Faction = OZ_Factions.OfUid(row.Uid);
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

    // Прочитати файл як текст. FGets рiже переноси -- склеюємо назад.
    private string ReadFileText(string path)
    {
        if (!FileExist(path))
            return "";

        FileHandle f = OpenFile(path, FileMode.READ);
        if (!f)
            return "";

        string text = "";
        string line;
        while (FGets(f, line) >= 0)
        {
            if (text != "")
                text += "\n";
            text += line;
        }
        CloseFile(f);
        return text;
    }
}

// Аплаєр ядра: спавни. Розбiр у ТИМЧАСОВИЙ об'єкт, збереження через той
// самий лоадер (вiн робить .bak), потiм Reload -- живий конфiг
// перечитується з уже перевiреного диска повним конвеєром валiдацiї.
//
// Аплаєра фракцiй тут БIЛЬШЕ НЕМАЄ: фракцiї народжуються й вмирають
// тiльки через бота (рiшення власника 2026-08-30), i редактор файла був
// би обхiдною стежкою повз це правило.
class OZ_SpawnsCfgApplier : OZ_AdminCfgApplier
{
    override bool Apply(string json)
    {
        OZ_SpawnsConfig tmp;
        string err;
        if (!JsonFileLoader<OZ_SpawnsConfig>.LoadData(json, tmp, err) || !tmp)
        {
            OZ_Log.Warn("admin: Spawns.json rejected: " + err);
            return false;
        }

        OZ_ConfigLoader<OZ_SpawnsConfig>.Save(OZ_Const.PROFILE_DIR + "\\Spawns.json", "spawns", tmp);
        OZ_Spawns.Reload();
        return true;
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
        d.Faction         = "";
        d.SeenFaction     = "";
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
