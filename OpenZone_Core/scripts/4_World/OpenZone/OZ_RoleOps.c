// Зміна ролей З ГРИ.
//
// ГРА НІЧОГО НЕ ЗАПИСУЄ. Вона просить міст, міст міняє роль у Discord, і
// зміна повертається сюди звичайною проекцією наступним опитом. Discord
// лишається єдиним домом факту, а гра стає пультом до нього.
//
// Це не ускладнення заради краси -- це те, що робить розходження НЕМОЖЛИВИМ.
// Якби гра писала фракцію в себе, з'явився б другий хазяїн даних, а з ним
// класичний набір: хто виграє при одночасній правці, що робити з відмовою
// Discord, і як відрізнити «лідер вигнав» від «міст не доїхав». Жодного з цих
// питань тут не виникає: відмова означає, що не змінилось НІЧОГО й НІДЕ, тому
// про неї можна чесно сказати, а не замазувати.
//
// Рішення власника 2026-08-27: Discord головний завжди; при мовчазному мості
// -- відмовити й назвати причину, а не складати в чергу.

class OZ_RoleAsk
{
    // Порожній -- дія адміністратора. Міст тоді не питає, чи актор лідер:
    // хто на цьому сервері адмін, знає лише гра, і це твердження береться
    // під спільний секрет.
    string ActorUid  = "";
    string TargetUid = "";
    string Op        = "";
    string Arg       = "";
    bool   Admin     = false;
}

class OZ_RoleAnswer
{
    bool   Ok  = false;
    string Why = "";
}

// Відповідь моста. Каже ТОМУ, ХТО ПРОСИВ, а не тому, кого змінили: для
// другого зміна приїде проекцією й виглядатиме як звичайна правка ролі.
class OZ_RoleReply : OZ_BridgeReply
{
    protected string m_Who;
    protected string m_Op;

    void OZ_RoleReply(string who, string op)
    {
        m_Who = who;
        m_Op  = op;
    }

    override void OnBody(string json)
    {
        PlayerIdentity to = OZ_Link.Online(m_Who);

        OZ_RoleAnswer a;
        string err;
        if (!JsonFileLoader<OZ_RoleAnswer>.LoadData(json, a, err) || !a)
        {
            if (to)
                OZ_Rpc.RoleRespond(to, m_Op, false, "STR_OZ_ERR_INTERNAL");
            return;
        }

        if (a.Ok)
        {
            OZ_Log.Info("roles: " + m_Who + " did " + m_Op + " -- accepted by Discord");
            if (to)
                OZ_Rpc.RoleRespond(to, m_Op, true, "");
            return;
        }

        // Причину віддаємо СЛОВАМИ моста, не своїм кодом помилки. Він єдиний
        // знає, чому саме Discord відмовив, і «бот не може керувати цією
        // роллю -- підніми його роль вище» набагато корисніше за «не вдалося».
        OZ_Log.Warn("roles: " + m_Who + " " + m_Op + " refused: " + a.Why);
        if (to)
            OZ_Rpc.RoleRespond(to, m_Op, false, a.Why);
    }

    override void OnFail(int code)
    {
        PlayerIdentity to = OZ_Link.Online(m_Who);
        if (to)
            OZ_Rpc.RoleRespond(to, m_Op, false, "STR_OZ_ERR_NO_BRIDGE");
    }
}

class OZ_RoleOps
{
    // Кому належить це ім'я, серед тих, хто зараз у Зоні.
    //
    // Однакові імена можливі, і тоді ми НЕ ВГАДУЄМО: порожнє означає «не
    // знайшли», і дія чесно не відбувається. Вибрати одного з двох Сидорових
    // навмання гірше за відмову -- другий не зрозуміє, за що його вигнали.
    static string UidByName(string name, string exceptUid)
    {
        if (name == "")
            return "";

        array<Man> players = new array<Man>();
        GetGame().GetPlayers(players);

        string found = "";

        for (int i = 0; i < players.Count(); i++)
        {
            if (!players[i])
                continue;

            PlayerIdentity id = players[i].GetIdentity();
            if (!id)
                continue;
            if (id.GetPlainId() == exceptUid)
                continue;
            if (id.GetName() != name)
                continue;

            if (found != "")
                return "";

            found = id.GetPlainId();
        }

        return found;
    }

    // Попросити міст змінити ролі. Особа актора -- ЗАВЖДИ з sender.
    static void Request(PlayerIdentity actor, string targetUid, string op, string arg)
    {
        if (!actor)
            return;
        RequestAs(actor, actor.GetPlainId(), targetUid, op, arg);
    }

    // Те саме, але від ЧУЖОГО імені, і це не лазівка.
    //
    // Потрібне рівно для прийнятого запрошення: право дає ЛІДЕР, який
    // запросив, а відповідь треба показати тому, хто натиснув «прийняти».
    // Два різні гравці в одній дії, тож дві різні ролі в підписі: `tell` --
    // кому відповідати, `actorUid` -- чиїм правом користуємось.
    //
    // Викликати це можна лише зсередини: назвати чуже ім'я клієнт не може, бо
    // в конверті RPC такого поля немає.
    static void RequestAs(PlayerIdentity tell, string actorUid, string targetUid, string op, string arg)
    {
        RequestAs(tell, actorUid, targetUid, op, arg, false);
    }

    // consented -- «за цим стоїть згода людини, яку міняють».
    //
    // Ставить його ТІЛЬКИ прийняте запрошення, і саме він відмикає
    // faction.set. Без нього лідер надсилав би "faction.set" просто по
    // проводу й записував у свою фракцію будь-кого з присутніх, не питаючи --
    // тобто вся згода, заради якої запрошення й існує, обходилась одним RPC.
    //
    // Прапорець НЕ приходить від клієнта: у конверті RPC такого поля немає, і
    // виставити його може лише код на сервері.
    static void RequestAs(PlayerIdentity tell, string actorUid, string targetUid, string op, string arg, bool consented)
    {
        if (!GetGame().IsServer())
            return;
        if (!tell)
            return;

        if (targetUid == "")
        {
            OZ_Rpc.RoleRespond(tell, op, false, "STR_OZ_ERR_NO_TARGET");
            return;
        }

        // Мовчазний міст -- відмова з причиною, і НІЧОГО не змінюється. Черги
        // тут немає навмисно: намір, який виконається через півгодини сам по
        // собі, гірший за чесне «зараз не вийшло». Рішення власника.
        if (!OZ_BridgeClient.Alive())
        {
            OZ_Rpc.RoleRespond(tell, op, false, "STR_OZ_ERR_NO_BRIDGE");
            return;
        }

        // Адміном може бути ЛИШЕ той, від чийого імені просять, і лише коли
        // він же й тисне. Прийняте запрошення адмінським не буває.
        bool admin = false;
        if (tell.GetPlainId() == actorUid)
            admin = OZ_Perm.IsAdmin(tell);

        // ВСТУП У ФРАКЦІЮ -- ТІЛЬКИ ЗІ ЗГОДИ. Ні лідер, ні хто завгодно інший
        // не може надіслати цю операцію з проводу: єдиний шлях -- запрошення,
        // яке людина прийняла сама.
        if (op == OZ_RoleOp.FACTION_SET)
        {
            if (!consented && !admin)
            {
                OZ_Rpc.RoleRespond(tell, op, false, "STR_OZ_ERR_NEEDS_INVITE");
                return;
            }
        }

        // Не адмін -- значить лідер, і це перевіряється ТУТ ТЕЖ, а не лише на
        // мості. Не заради безпеки -- міст перевірить сам і лишається
        // головним, -- а заради відповіді: «ти не лідер» мусить прийти
        // миттєво, а не за півсекунди з мережі.
        //
        // Прийняте запрошення сюди НЕ заходить, і це не послаблення. Місцева
        // перевірка питає OZ_Roles.IsLeader, а проекція ролей живе лише поки
        // гравець у Зоні: щойно лідер вийшов -- вона стерта, і його власне
        // запрошення перестало б працювати через те, що він відійшов від
        // комп'ютера. Лідерство перевірить МІСТ, який дивиться в Discord і
        // про присутність нічого не знає.
        if (!admin && !consented)
        {
            if (!Allowed(tell, actorUid, op, arg, targetUid))
                return;
        }

        OZ_RoleAsk a = new OZ_RoleAsk();
        a.TargetUid = targetUid;
        a.Op        = op;
        a.Arg       = arg;
        a.Admin     = admin;

        // Актора адміністратор не називає: міст тоді не питає про лідерство.
        if (!admin)
            a.ActorUid = actorUid;

        string letter;
        string err;
        if (!JsonFileLoader<OZ_RoleAsk>.MakeData(a, letter, err, false))
        {
            OZ_Log.Error("roles: cannot build the letter: " + err);
            OZ_Rpc.RoleRespond(tell, op, false, "STR_OZ_ERR_INTERNAL");
            return;
        }

        OZ_BridgeClient.Call("v1/roles/apply", letter, new OZ_RoleReply(tell.GetPlainId(), op));
    }

    // Чи можна цьому гравцеві просити саме це. Дзеркало leaderMay() на мості
    // -- і воно там лишається головним. Тут -- щоб відмова була швидкою.
    //
    // Відповідає САМА, бо причина відмови в кожному випадку своя, а «не можна»
    // без причини -- найгірше, що інтерфейс може сказати.
    private static bool Allowed(PlayerIdentity tell, string actorUid, string op, string arg, string targetUid)
    {
        string mine = OZ_Factions.OfUid(actorUid);

        if (mine == "")
        {
            OZ_Rpc.RoleRespond(tell, op, false, "STR_OZ_ERR_NOT_LEADER");
            return false;
        }

        if (!OZ_Roles.IsLeader(actorUid))
        {
            OZ_Rpc.RoleRespond(tell, op, false, "STR_OZ_ERR_NOT_LEADER");
            return false;
        }

        // Прийняти можна ТІЛЬКИ до себе.
        if (op == OZ_RoleOp.FACTION_SET)
        {
            if (arg == mine)
                return true;

            OZ_Rpc.RoleRespond(tell, op, false, "STR_OZ_ERR_OTHER_FACTION");
            return false;
        }

        // Решта -- тільки над своїми.
        if (OZ_Factions.OfUid(targetUid) != mine)
        {
            OZ_Rpc.RoleRespond(tell, op, false, "STR_OZ_ERR_NOT_YOURS");
            return false;
        }

        if (op == OZ_RoleOp.FACTION_CLEAR)
            return true;
        if (op == OZ_RoleOp.LEADER_TRANSFER)
            return true;

        if (op == OZ_RoleOp.POST_ADD || op == OZ_RoleOp.POST_REMOVE)
        {
            // Посада мусить належати ЙОГО фракції -- слаг має вигляд
            // "duty:guard", і префікс перевіряється тут, щоб лідер Долгу не
            // роздавав посад Волі.
            if (arg.IndexOf(mine + ":") != 0)
            {
                OZ_Rpc.RoleRespond(tell, op, false, "STR_OZ_ERR_OTHER_FACTION");
                return false;
            }

            // Лідерство ПЕРЕДАЮТЬ, а не роздають: лідер, який може видати
            // посаду лідера, здатен зробити другого -- і тоді жоден із них не
            // лідер.
            if (arg == mine + ":leader")
            {
                OZ_Rpc.RoleRespond(tell, op, false, "STR_OZ_ERR_USE_TRANSFER");
                return false;
            }

            return true;
        }

        // Звання й мітки -- не лідерська справа.
        OZ_Rpc.RoleRespond(tell, op, false, "STR_OZ_ERR_ADMIN_ONLY");
        return false;
    }
}

// Зони спавна -- адмінські операції.
//
// Живуть поруч із рольовими, бо приходять тим самим RPC і тією ж перевіркою
// прав. Але на МІСТ вони не йдуть узагалі: координати належать карті, а не
// Discord, і бот про них нічого не знає й знати не мусить.
class OZ_SpawnOps
{
    static void Handle(PlayerIdentity who, string op, string arg)
    {
        if (!GetGame().IsServer())
            return;
        if (!who)
            return;

        if (!OZ_Perm.IsAdmin(who))
        {
            OZ_Rpc.RoleRespond(who, op, false, "STR_OZ_ERR_ADMIN_ONLY");
            return;
        }

        // arg -- "слаг" або "слаг радіус". Радіус необов'язковий: без нього
        // двадцять метрів, бо зона в одну точку -- це купа тіл, а не табір.
        string rest = Trimmed(arg);
        string role = rest;
        float radius = 20;

        int sp = rest.IndexOf(" ");
        if (sp != -1)
        {
            role = rest.Substring(0, sp);

            // РАДІУС МУСИТЬ БУТИ ЧИСЛОМ.
            //
            // ToFloat() на будь-якому смітті чесно повертає нуль, і зона
            // ставала точкою: усі спавняться в одному пікселі, один в одному.
            // Помилку набору не видно ніде -- команда відповідала «готово».
            string tail = Trimmed(rest.Substring(sp + 1, rest.Length() - sp - 1));
            if (!Number(tail))
            {
                OZ_Rpc.RoleRespond(who, op, false, "STR_OZ_ERR_BAD_RADIUS");
                return;
            }

            radius = tail.ToFloat();
        }

        // ПРОБІЛ -- НЕ СЛАГ.
        //
        // Слаг із пробілом попереду (« duty») різався на порожній слаг і
        // хвіст, а порожній слаг означає ЗАПАСНУ зону -- ту, куди потрапляють
        // усі, в кого нічого не збіглося. Один зайвий пробіл у команді
        // переносив спавн усього сервера, і відповідь була «готово».
        role = Trimmed(role);

        // Порожній слаг -- це ЗАПАСНА зона, і писати його як "" у команді
        // незручно. Домовляємось: "-" означає порожній.
        if (role == "-")
            role = "";

        string err;

        if (op == OZ_RoleOp.SPAWN_CLEAR)
        {
            err = OZ_Spawns.ClearZone(role);
        }
        else
        {
            PlayerBase p = PlayerBase.Cast(GetGame().GetPlayer());
            vector here = vector.Zero;

            // Позицію беремо з ЙОГО тіла на сервері, а не з чогось, що прислав
            // клієнт: інакше зону можна було б поставити куди завгодно, не
            // сходячи з місця.
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
                here = players[i].GetPosition();
                break;
            }

            if (here == vector.Zero)
            {
                OZ_Rpc.RoleRespond(who, op, false, "STR_OZ_ERR_INTERNAL");
                return;
            }

            err = OZ_Spawns.SetZoneHere(role, here, radius);
        }

        if (err != "")
        {
            OZ_Rpc.RoleRespond(who, op, false, err);
            return;
        }

        OZ_Rpc.RoleRespond(who, op, true, "");
    }

    // Пробіли з обох боків. У Enforce немає Trim(), а рядок приходить із
    // чату -- там вони будуть.
    private static string Trimmed(string s)
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
    private static bool Number(string s)
    {
        if (s == "")
            return false;

        bool dot = false;

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
        }

        return true;
    }
}

// Назви операцій. Рядки збігаються з тими, що читає міст, ПОСИМВОЛЬНО.
class OZ_RoleOp
{
    static const string FACTION_SET     = "faction.set";
    static const string FACTION_CLEAR   = "faction.clear";
    static const string POST_ADD        = "post.add";
    static const string POST_REMOVE     = "post.remove";
    static const string TRAIT_ADD       = "trait.add";
    static const string TRAIT_REMOVE    = "trait.remove";
    static const string RANK_SET        = "rank.set";
    static const string LEADER_TRANSFER = "leader.transfer";

    // Ці двоє до моста не доходять: карта -- не його справа.
    static const string SPAWN_HERE  = "spawn.here";
    static const string SPAWN_CLEAR = "spawn.clear";
}

// Запрошення до фракції.
//
// ЖИВЕ В ГРІ, і тільки до згоди. Це не роль і не факт про гравця -- це намір
// лідера, на який ще ніхто не відповів. Записати його в Discord означало б
// зарахувати людину у фракцію, поки вона думає.
//
// Згода ОБОВ'ЯЗКОВА: у фракцію не можна записати нікого без його відома, і
// саме тому запрошення взагалі існує замість прямого faction.set.
class OZ_FactionInvite
{
    string Faction = "";
    string FromUid = "";
    string FromName = "";
    int    ExpiresAt = 0;
}

class OZ_FactionInvites
{
    private static ref map<string, ref OZ_FactionInvite> s_By;

    // Строк життя береться з Settings.json (Faction.InviteTtlSeconds).

    static void Offer(PlayerIdentity from, string targetUid)
    {
        if (!from)
            return;

        string me = from.GetPlainId();

        string mine = OZ_Factions.OfUid(me);
        if (mine == "" || !OZ_Roles.IsLeader(me))
        {
            OZ_Rpc.RoleRespond(from, "invite", false, "STR_OZ_ERR_NOT_LEADER");
            return;
        }

        // ПОРОЖНЯ ЦІЛЬ -- ВІДМОВА, а не мовчазний успіх.
        //
        // UidByName повертає порожній рядок, коли не знайшов або знайшов
        // двох. Раніше це проходило далі: запрошення лягало під ключ "" й
        // лідерові казали «готово». Ніхто нікого не запросив, і дізнатись
        // про це не було як.
        if (targetUid == "")
        {
            OZ_Rpc.RoleRespond(from, "invite", false, "STR_OZ_ERR_NO_TARGET");
            return;
        }

        if (targetUid == me)
        {
            OZ_Rpc.RoleRespond(from, "invite", false, "STR_OZ_ERR_SELF");
            return;
        }

        if (OZ_Factions.OfUid(targetUid) == mine)
        {
            OZ_Rpc.RoleRespond(from, "invite", false, "STR_OZ_ERR_ALREADY_IN");
            return;
        }

        // МIСЦЯ МАЄ ВИСТАЧАТИ ВЖЕ ЗАРАЗ. Межа з Settings.json; нуль -- без
        // межi. Рахуються проекцiї ролей -- увесь вiдомий склад, не лише
        // присутнi. Перевiрка повторюється на прийняттi: за двi хвилини
        // строку запрошення фракцiя могла заповнитись iншими.
        if (!FactionHasRoom(mine))
        {
            OZ_Rpc.RoleRespond(from, "invite", false, "STR_OZ_ERR_FACTION_FULL");
            return;
        }

        // ЧУЖЕ ЗАПРОШЕННЯ НЕ ПЕРЕБИВАЄТЬСЯ.
        //
        // Ключ -- людина, і другий лідер, що встиг натиснути, просто підміняв
        // перше запрошення своїм. На екрані в цей момент могло стояти «Долг»,
        // а «Прийняти» відправляло б у Бандити: людина погодилась на одне, а
        // отримала інше. Згода на те й згода, щоб стосуватись саме того, що
        // показали.
        //
        // Перший встиг -- його й черга, поки не сплине строк або людина не
        // відмовиться. Другому чесно кажемо, що зайнято.
        OZ_FactionInvite already = Pending(targetUid);
        if (already)
        {
            OZ_Rpc.RoleRespond(from, "invite", false, "STR_OZ_ERR_INVITE_BUSY");
            return;
        }

        if (!s_By)
            s_By = new map<string, ref OZ_FactionInvite>();

        OZ_FactionInvite inv = new OZ_FactionInvite();
        inv.Faction   = mine;
        inv.FromUid   = me;
        inv.FromName  = from.GetName();
        inv.ExpiresAt = GetGame().GetTime() + OZ_Settings.Get().Faction.InviteTtlSeconds * 1000;

        s_By.Set(targetUid, inv);

        OZ_Rpc.RoleRespond(from, "invite", true, "");

        // Кажемо запрошеному одразу, якщо він у Зоні. Не в Зоні -- побачить,
        // коли зайде, якщо встигне до строку.
        PlayerIdentity to = OZ_Link.Online(targetUid);
        if (to)
            OZ_Rpc.RoleRespond(to, "invited", true, mine);
    }

    // Чинне запрошення, або null. Прострочене прибирає за собою.
    static OZ_FactionInvite Pending(string targetUid)
    {
        if (!s_By)
            return null;

        OZ_FactionInvite inv;
        if (!s_By.Find(targetUid, inv))
            return null;
        if (!inv)
            return null;

        if (GetGame().GetTime() > inv.ExpiresAt)
        {
            s_By.Remove(targetUid);
            return null;
        }

        return inv;
    }

    static void Accept(PlayerIdentity who)
    {
        if (!who)
            return;

        string me = who.GetPlainId();

        OZ_FactionInvite inv = Pending(me);
        if (!inv)
        {
            OZ_Rpc.RoleRespond(who, "accept", false, "STR_OZ_ERR_NO_INVITE");
            return;
        }

        // Мiсце могло скiнчитись, поки запрошення лежало.
        if (!FactionHasRoom(inv.Faction))
        {
            s_By.Remove(me);
            OZ_Rpc.RoleRespond(who, "accept", false, "STR_OZ_ERR_FACTION_FULL");
            return;
        }

        // ЗНІМАЄМО ДО виклику моста. Запрошення, яке не спрацювало через
        // мовчазний міст, усе одно використане: інакше воно лишалось би
        // висіти й спрацювало б від наступного натискання, коли лідер уже
        // передумав.
        s_By.Remove(me);

        // Актор -- ЛІДЕР, а не той, хто приймає: саме його лідерство
        // перевіряє міст. Він може бути офлайн, і це не заважає -- міст
        // дивиться на його ролі в Discord, а не на присутність у Зоні.
        // Право дає ЛІДЕР, відповідь бачить той, хто прийняв. Лідер може
        // бути офлайн -- міст дивиться на його ролі в Discord, а не на
        // присутність у Зоні.
        // consented=true -- ЄДИНЕ місце, де це ставиться. Людина щойно
        // натиснула «прийняти» на запрошенні, яке бачила своїми очима.
        OZ_RoleOps.RequestAs(who, inv.FromUid, me, OZ_RoleOp.FACTION_SET, inv.Faction, true);
    }

    // Чи є мiсце. Межа -- поле САМОЇ фракцiї у Factions.json; нуль або
    // вiдсутнiсть поля -- межi немає.
    private static bool FactionHasRoom(string faction)
    {
        OZ_Faction f = OZ_Factions.Find(faction);
        if (!f || f.MaxMembers <= 0)
            return true;

        array<string> members = new array<string>();
        OZ_Roles.FactionMembers(faction, members);
        return members.Count() < f.MaxMembers;
    }

    static void Decline(PlayerIdentity who)
    {
        if (!who)
            return;
        if (!s_By)
            return;

        s_By.Remove(who.GetPlainId());
        OZ_Rpc.RoleRespond(who, "decline", true, "");
    }

    // Гравець вийшов -- запрошення до нього більше нікому показувати.
    static void Forget(string uid)
    {
        if (!s_By)
            return;
        if (!s_By.Contains(uid))
            return;
        s_By.Remove(uid);
    }
}
