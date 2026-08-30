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
    bool   Leader  = false;
}

class OZ_AdminRoster
{
    ref array<ref OZ_AdminRosterRow> Rows;
    ref array<string> Factions;

    void OZ_AdminRoster()
    {
        Rows     = new array<ref OZ_AdminRosterRow>();
        Factions = new array<string>();
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
        if (op == "cfg_get")
            return CfgGet(json, ok, error);
        if (op == "cfg_set")
            return CfgSet(json, sender, ok, error);
        if (op == "roster")
            return Roster(ok, error);

        error = "STR_OZ_ERR_UNKNOWN_OP";
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

    private string CfgGet(string json, out bool ok, out string error)
    {
        OZ_AdminAsk a;
        string err;
        if (!JsonFileLoader<OZ_AdminAsk>.LoadData(json, a, err) || !a)
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_AdminCfgEntry e = OZ_AdminCfg.Find(a.Name);
        if (!e)
        {
            error = "STR_OZ_ERR_NO_SUCH_CFG";
            return "";
        }

        // Сирий текст файлу, а не пересерiалiзований об'єкт: адмiн править
        // САМЕ ТЕ, що лежить на диску, разом iз вiдсутнiми полями й усiм.
        string text = ReadFileText(e.Path);

        OZ_AdminAsk res = new OZ_AdminAsk();
        res.Name = a.Name;
        res.Json = text;

        string outJson;
        if (!JsonFileLoader<OZ_AdminAsk>.MakeData(res, outJson, err, false))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        ok = true;
        return outJson;
    }

    private string CfgSet(string json, PlayerIdentity sender, out bool ok, out string error)
    {
        OZ_AdminAsk a;
        string err;
        if (!JsonFileLoader<OZ_AdminAsk>.LoadData(json, a, err) || !a)
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_AdminCfgEntry e = OZ_AdminCfg.Find(a.Name);
        if (!e || !e.Applier)
        {
            error = "STR_OZ_ERR_NO_SUCH_CFG";
            return "";
        }

        // Розбiр -- ДО диска: смiття не досягає файлу, чинна версiя чинна.
        // Хто саме зламався -- скаже лог; клiєнтовi досить «не розiбралось».
        if (!e.Applier.Apply(a.Json))
        {
            error = "STR_OZ_ERR_CFG_REJECTED";
            return "";
        }

        OZ_Log.Info("admin: config \"" + a.Name + "\" applied by " + sender.GetPlainId());

        ok = true;
        return "{}";
    }

    private string Roster(out bool ok, out string error)
    {
        OZ_AdminRoster r = new OZ_AdminRoster();
        OZ_Factions.Ids(r.Factions);

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

// Аплаєри ядра: фракцiї i спавни. Розбiр у ТИМЧАСОВИЙ об'єкт, збереження
// через той самий лоадер (вiн робить .bak), потiм Reload -- живий конфiг
// перечитується з уже перевiреного диска повним конвеєром валiдацiї.
class OZ_FactionsCfgApplier : OZ_AdminCfgApplier
{
    override bool Apply(string json)
    {
        OZ_FactionsConfig tmp;
        string err;
        if (!JsonFileLoader<OZ_FactionsConfig>.LoadData(json, tmp, err) || !tmp)
        {
            OZ_Log.Warn("admin: Factions.json rejected: " + err);
            return false;
        }

        OZ_ConfigLoader<OZ_FactionsConfig>.Save(OZ_Const.PROFILE_DIR + "\\Factions.json", "factions", tmp);
        OZ_Factions.Reload();
        return true;
    }
}

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
