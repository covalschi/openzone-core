// Адмiнська консоль: серверна половина, з якою говорить будь-який
// адмiнський UI (першою -- вкладка VPP).
//
// РОЗДІЛИ, А НЕ СТОРІНКИ (рішення власника 2026-09-01, ТЗ-5 §C2-C3). Досi це
// була сторiнка "admin" у спiльному реєстрi, i щоб її пропустити, гейт КПК
// тримав виняток першим рядком -- тобто в межi безпеки були дверi збоку.
// Тепер розділи живуть у власному реєстрi (OZ_AdminRegistry), їдуть власним
// конвертом (OZ_AdminReq/OZ_AdminRes), а ворота однi й без винятків:
// OZ_Perm.IsAdmin, на сервері, першим рядком диспетчера в OZ_Module.
//
// Ядро приносить два роздiли: "config" (редактор зареєстрованих конфiгiв) i
// "spawns" (зони, стейджинґ, особистi точки). Мод фракцiй доклада свiй.

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

class OZ_ConfigSection : OZ_AdminSection
{
    override string Handle(string op, string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        // Прав тут БІЛЬШЕ НЕ ПЕРЕВІРЯЄМО: їх перевірив диспетчер, до розбору
        // операції. Друга перевірка в кожному розділі виглядала б обережною,
        // але робила б межу безпеки розсипаною по модах -- а її треба вміти
        // прочитати в одному місці.

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

    private string CfgGet(string name, out bool ok, out string error)
    {
        OZ_AdminCfgEntry e = OZ_AdminCfg.Find(name);
        if (!e)
        {
            error = "STR_OZ_ERR_NO_SUCH_CFG";
            return "";
        }

        // ФАЙЛА НЕМАЄ -- ЦЕ ВІДМОВА, а не порожній конфіг.
        //
        // Раніше сюди приїздив ok=true з порожнім тілом: редактор показував
        // чисте поле, і APPLY поверх нього записував порожній рядок як увесь
        // конфіг. Конфіг, зареєстрований, але ще не створений на диску, --
        // звичайний стан першого запуску, і мовчати про нього не можна.
        if (!FileExist(e.Path))
        {
            error = "STR_OZ_ERR_CFG_MISSING";
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

        OZ_ConfigLoader<OZ_SpawnsConfig>.Save(OZ_Const.PROFILE_DIR + "\\OZ_Core_Spawns.json", "spawns", tmp);
        OZ_Spawns.Reload();
        return true;
    }
}

// ---------------------------------------------------------------- спавни
//
// РОЗДІЛ ЯДРА, а не мода фракцій (рішення власника, ТЗ-5 §C1 R6). Досi цей
// обробник жив у @OpenZone_Factions i їздив конвертом ролей -- тобто на
// сервері без мода фракцій адмiн не мiг завести ЖОДНОЇ зони, хоч зони й
// файл зон лежать у ядрi, i хоч панель SPAWNS у вкладцi VPP -- теж ядрова.
// Правило серiї каже протилежне: будь-який мод працює, маючи одне лише ядро.
//
// Аргумент операцiї їде ТIЛОМ запиту рядком, а не JSON-об'єктом: він
// коротенький ("duty 30", "76561198... 25"), i конверт зi строковим полем
// різався б на 1023 байтах при серверному розборi -- та сама пастка, що й у
// cfg_get.
class OZ_SpawnSection : OZ_AdminSection
{
    override string Handle(string op, string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        if (op == OZ_SpawnOp.UID_HERE || op == OZ_SpawnOp.UID_CLEAR)
            return Personal(op, json, sender, ok, error);

        if (op == OZ_SpawnOp.HERE || op == OZ_SpawnOp.CLEAR)
            return Zone(op, json, sender, ok, error);

        error = "STR_OZ_ERR_UNKNOWN_OP";
        return "";
    }

    // arg -- "слаг" або "слаг радіус". Радіус необов'язковий: без нього
    // двадцять метрів, бо зона в одну точку -- це купа тіл, а не табір.
    private string Zone(string op, string arg, PlayerIdentity sender, out bool ok, out string error)
    {
        string role;
        float radius;
        if (!Split(arg, role, radius, error))
            return "";

        // ПРОБІЛ -- НЕ СЛАГ.
        //
        // Слаг із пробілом попереду (« duty») різався на порожній слаг і
        // хвіст, а порожній слаг означає ЗАПАСНУ зону -- ту, куди потрапляють
        // усі, в кого нічого не збіглося. Один зайвий пробіл у команді
        // переносив спавн усього сервера, і відповідь була «готово».
        //
        // Порожній слаг писати в поле незручно, тож домовляємось: "-" означає
        // порожній, "*" -- стейджинґ (його розбирає сам OZ_Spawns).
        if (role == "-")
            role = "";

        string err;

        if (op == OZ_SpawnOp.CLEAR)
        {
            err = OZ_Spawns.ClearZone(role);
        }
        else
        {
            // Позицію беремо з ЙОГО тіла на сервері, а не з чогось, що прислав
            // клієнт: інакше зону можна було б поставити куди завгодно, не
            // сходячи з місця.
            vector here = BodyOf(sender);
            if (here == vector.Zero)
            {
                error = "STR_OZ_ERR_INTERNAL";
                return "";
            }

            err = OZ_Spawns.SetZoneHere(role, here, radius);
        }

        if (err != "")
        {
            error = err;
            return "";
        }

        ok = true;
        return "{}";
    }

    // Особиста точка гравця. Позиція -- тіло АДМІНА на сервері, як і в
    // зоні: «стань там, де його дім, і натисни».
    private string Personal(string op, string arg, PlayerIdentity sender, out bool ok, out string error)
    {
        string uid;
        float radius;
        if (!Split(arg, uid, radius, error))
            return "";

        if (uid == "")
        {
            error = "STR_OZ_ERR_NO_TARGET";
            return "";
        }

        string err;

        if (op == OZ_SpawnOp.UID_CLEAR)
        {
            err = OZ_Spawns.ClearPersonal(uid);
        }
        else
        {
            vector here = BodyOf(sender);
            if (here == vector.Zero)
            {
                error = "STR_OZ_ERR_INTERNAL";
                return "";
            }

            err = OZ_Spawns.SetPersonalHere(uid, here, radius);
        }

        if (err != "")
        {
            error = err;
            return "";
        }

        ok = true;
        return "{}";
    }

    // «слово» або «слово число». Радіус за замовчуванням -- двадцять метрів.
    private bool Split(string arg, out string head, out float radius, out string error)
    {
        string rest = Trimmed(arg);
        head   = rest;
        radius = 20;

        int sp = rest.IndexOf(" ");
        if (sp == -1)
        {
            head = Trimmed(head);
            return true;
        }

        head = Trimmed(rest.Substring(0, sp));

        // РАДІУС МУСИТЬ БУТИ ЧИСЛОМ.
        //
        // ToFloat() на будь-якому смітті чесно повертає нуль, і зона ставала
        // точкою: усі спавняться в одному пікселі, один в одному. Помилку
        // набору не видно ніде -- команда відповідала «готово».
        string tail = Trimmed(rest.Substring(sp + 1, rest.Length() - sp - 1));
        if (!Number(tail))
        {
            error = "STR_OZ_ERR_BAD_RADIUS";
            return false;
        }

        radius = tail.ToFloat();

        // НУЛЬ -- ТЕЖ ВІДМОВА, і це та сама причина, від якої захищає перевірка
        // вище. Зона нульового радіуса -- точка, у якій усі спавняться один в
        // одному; саме її обіцяв не пустити коментар про «купу тіл, а не
        // табір», а число `0` крізь Number() проходило й давало рівно це.
        if (radius <= 0)
        {
            error = "STR_OZ_ERR_BAD_RADIUS";
            return false;
        }

        return true;
    }

    // Тіло гравця НА СЕРВЕРІ -- не координата, яку прислав клієнт.
    private vector BodyOf(PlayerIdentity who)
    {
        if (!who)
            return vector.Zero;

        array<Man> players = new array<Man>();
        GetGame().GetPlayers(players);
        for (int i = 0; i < players.Count(); i++)
        {
            if (!players[i])
                continue;
            PlayerIdentity id = players[i].GetIdentity();
            if (!id)
                continue;
            if (id.GetPlainId() != who.GetPlainId())
                continue;
            return players[i].GetPosition();
        }
        return vector.Zero;
    }

    // Пробіли з обох боків. У Enforce немає Trim(), а рядок приходить із
    // поля вводу -- там вони будуть.
    private string Trimmed(string s)
    {
        int from = 0;
        int to   = s.Length();

        while (from < to && s.Substring(from, 1) == " ")
            from++;
        while (to > from && s.Substring(to - 1, 1) == " ")
            to--;

        return s.Substring(from, to - from);
    }

    // Чи це взагалі число. ToFloat() не вміє сказати «ні», тож питаємо самі.
    //
    // ХОЧА Б ОДНА ЦИФРА обов'язкова. Без цієї умови рядок "." проходив як
    // число: крапка дозволена, інших символів немає, цикл закінчується
    // успіхом -- а ToFloat(".") дає нуль, тобто рівно ту точкову зону, яку ця
    // перевірка й мала не пустити.
    private bool Number(string s)
    {
        if (s == "")
            return false;

        bool dot = false;
        bool digit = false;

        for (int i = 0; i < s.Length(); i++)
        {
            string c = s.Substring(i, 1);

            if (c == ".")
            {
                if (dot)
                    return false;
                dot = true;
                continue;
            }

            // Через набір, а не через порівняння рядків: у Enforce «менше»
            // для string не визначене, і покластись на нього не можна.
            if ("0123456789".IndexOf(c) == -1)
                return false;

            digit = true;
        }

        return digit;
    }
}
