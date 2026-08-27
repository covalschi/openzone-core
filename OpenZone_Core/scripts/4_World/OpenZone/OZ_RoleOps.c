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

        // Не адмін -- значить лідер, і це перевіряється ТУТ ТЕЖ, а не лише на
        // мості. Не заради безпеки -- міст перевірить сам і лишається
        // головним, -- а заради відповіді: «ти не лідер» мусить прийти
        // миттєво, а не за півсекунди з мережі.
        if (!admin)
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

    // Дві хвилини. Довше -- і гравець приймає запрошення від лідера, який
    // давно передумав; коротше -- не встигає прочитати.
    private static const int TTL_MS = 120000;

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

        if (!s_By)
            s_By = new map<string, ref OZ_FactionInvite>();

        OZ_FactionInvite inv = new OZ_FactionInvite();
        inv.Faction   = mine;
        inv.FromUid   = me;
        inv.FromName  = from.GetName();
        inv.ExpiresAt = GetGame().GetTime() + TTL_MS;

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
        OZ_RoleOps.RequestAs(who, inv.FromUid, me, OZ_RoleOp.FACTION_SET, inv.Faction);
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
