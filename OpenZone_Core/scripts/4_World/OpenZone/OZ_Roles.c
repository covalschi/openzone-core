// Що Discord каже про гравця: звання, посади, мітки.
//
// ТРИ ОСІ, і вони НАВМИСНО живуть у різних місцях, бо ламаються по-різному.
// Фракцію тримає OZ_Factions -- у неї є ставлення, запасний шлях через файл
// акаунта й контракт для чужого постачальника. Звання й мітки не мають нічого
// з цього: вони приходять ТІЛЬКИ з Discord або не приходять узагалі. Злити їх
// в одну службу означало б дати званню постачальника, якого в нього немає, і
// мітці -- єдине значення, якого вона не може мати.
//
// Питати все одразу все ж треба в одному місці, тому є OZ_Identity нижче.
//
// ЩО ЦЕЙ КЛАС НЕ РОБИТЬ: не пише нічого у файл акаунта. Проекція ролей --
// відповідь мосту на «зараз», а не стан гравця. Записати її в OZ_PlayerData
// означало б, що роль, знята в Discord при мертвому мості, лишиться назавжди.

class OZ_RoleView
{
    string Uid          = "";
    string Faction      = "";
    string Rank         = "";
    ref array<string> Posts;
    ref array<string> Traits;

    // Дві фракції разом -- це помилка налаштування гільдії, а не стан
    // гравця. Міст відмовляється вгадувати й перелічує обидві; ми показуємо
    // «без фракції» і даємо адмінові побачити, що він накоїв.
    ref array<string> Conflict;

    void OZ_RoleView()
    {
        Posts    = new array<string>();
        Traits   = new array<string>();
        Conflict = new array<string>();
    }
}

class OZ_Roles
{
    private static ref map<string, ref OZ_RoleView> s_By;

    static void Apply(OZ_RoleView v)
    {
        if (!GetGame().IsServer())
            return;
        if (!v)
            return;
        if (v.Uid == "")
            return;

        if (!s_By)
            s_By = new map<string, ref OZ_RoleView>();

        // Пишемо лише ЗМІНУ. Проекція приїжджає кожен опит -- сім разів на
        // хвилину на гравця, -- і рядок на кожну перетворив би лог на шум,
        // у якому не видно того єдиного разу, коли щось справді сталося.
        OZ_RoleView had;
        bool changed = true;
        if (s_By.Find(v.Uid, had) && had)
            changed = !Same(had, v);

        s_By.Set(v.Uid, v);

        if (changed)
        {
            string m = "roles: " + v.Uid;
            m += " faction=\"" + v.Faction;
            m += "\" rank=\"" + v.Rank;
            m += "\" posts=" + v.Posts.Count().ToString();
            m += " traits=" + v.Traits.Count().ToString();
            OZ_Log.Info(m);
        }

        // Фракцію веде ЇЇ служба -- одна правда, одне місце. Порожній рядок
        // тут значущий: він означає «Discord каже, що фракції немає», і
        // OZ_Factions саме так його й читає.
        OZ_Factions.SetFromRole(v.Uid, v.Faction);

        if (changed)
            Remember(v);

        if (v.Conflict.Count() > 1)
        {
            string w = "roles: " + v.Uid;
            w += " holds more than one faction role (";
            w += v.Conflict[0] + ", " + v.Conflict[1];
            w += ") - showing none until the guild is fixed";
            OZ_Log.Warn(w);
        }
    }

    // Знімок для показу офлайнових. Пишемо ЛИШЕ у власні поля Seen*, ніколи
    // не в Faction: див. довгу причину в OZ_PlayerData.
    private static void Remember(OZ_RoleView v)
    {
        OZ_PlayerData d = OZ_PlayerStore.Load(v.Uid);
        if (!d)
            return;

        d.SeenFaction = v.Faction;
        d.SeenRank    = v.Rank;

        if (!d.SeenPosts)
            d.SeenPosts = new array<string>();
        if (!d.SeenTraits)
            d.SeenTraits = new array<string>();

        d.SeenPosts.Clear();
        d.SeenTraits.Clear();

        for (int i = 0; i < v.Posts.Count(); i++)
            d.SeenPosts.Insert(v.Posts[i]);
        for (int j = 0; j < v.Traits.Count(); j++)
            d.SeenTraits.Insert(v.Traits[j]);

        OZ_PlayerStore.MarkDirty(v.Uid);
    }

    // Останнє відоме -- ЯВНО, окремим викликом. Тим, хто питає Of(), знімок
    // не дістається: «не знаємо» мусить лишатись відповіддю.
    static OZ_RoleView Seen(string uid)
    {
        OZ_RoleView live = Of(uid);
        if (live)
            return live;

        OZ_PlayerData d = OZ_PlayerStore.Load(uid);
        if (!d)
            return null;
        if (d.SeenFaction == "" && d.SeenRank == "")
            return null;

        OZ_RoleView v = new OZ_RoleView();
        v.Uid     = uid;
        v.Faction = d.SeenFaction;
        v.Rank    = d.SeenRank;

        if (d.SeenPosts)
        {
            for (int i = 0; i < d.SeenPosts.Count(); i++)
                v.Posts.Insert(d.SeenPosts[i]);
        }
        if (d.SeenTraits)
        {
            for (int j = 0; j < d.SeenTraits.Count(); j++)
                v.Traits.Insert(d.SeenTraits[j]);
        }

        return v;
    }

    // Про цього гравця нічого не відомо: не прив'язаний, або міст його не
    // бачить. Прибираємо ЗАПИС, а не ставимо порожній -- «немає ролі» й «ми
    // не знаємо» різні відповіді.
    static void Forget(string uid)
    {
        if (s_By && s_By.Contains(uid))
            s_By.Remove(uid);

        OZ_Factions.ForgetRole(uid);
    }

    // Порівняння за ЗМІСТОМ, не за посиланням: об'єкт щоразу новий, бо його
    // щойно розібрали з JSON.
    private static bool Same(OZ_RoleView a, OZ_RoleView b)
    {
        if (a.Faction != b.Faction)
            return false;
        if (a.Rank != b.Rank)
            return false;
        if (a.Posts.Count() != b.Posts.Count())
            return false;
        if (a.Traits.Count() != b.Traits.Count())
            return false;

        for (int i = 0; i < a.Posts.Count(); i++)
        {
            if (b.Posts.Find(a.Posts[i]) == -1)
                return false;
        }

        for (int j = 0; j < a.Traits.Count(); j++)
        {
            if (b.Traits.Find(a.Traits[j]) == -1)
                return false;
        }

        return true;
    }

    static OZ_RoleView Of(string uid)
    {
        if (!s_By)
            return null;

        OZ_RoleView v;
        if (!s_By.Find(uid, v))
            return null;
        return v;
    }

    static string RankOf(string uid)
    {
        OZ_RoleView v = Of(uid);
        if (!v)
            return "";
        return v.Rank;
    }

    static bool HasPost(string uid, string post)
    {
        OZ_RoleView v = Of(uid);
        if (!v)
            return false;
        return v.Posts.Find(post) != -1;
    }

    static bool IsLeader(string uid)
    {
        return HasPost(uid, "leader");
    }

    static bool HasTrait(string uid, string trait)
    {
        OZ_RoleView v = Of(uid);
        if (!v)
            return false;
        return v.Traits.Find(trait) != -1;
    }

    // Чи протухла проекція. Не «моста немає» -- саме «давно нічого не
    // приїжджало», бо для того, хто дивиться на екран, це те саме, а для
    // діагностики різне.
    //
    // Той, хто малює, мусить показати ОСТАННЄ ВІДОМЕ приглушеним, а не
    // порожнє: порожнє читається як «одинак», і робити такий висновок гра
    // права не має.
    static bool Stale()
    {
        // Питаємо МІСТ, а не власний лічильник приїздів.
        //
        // Тут стояло «коли востаннє приїжджала проекція», і це працювало рівно
        // доти, доки проекція приїжджала щоопиту. Щойно її стали слати ЛИШЕ
        // ПРИ ЗМІНІ -- а це зробили, щоб не крутити опит на сімнадцять
        // запитів на секунду, -- лічильник перестав означати те, що означав:
        // тридцять секунд без змін у Discord, і цілком здоровий міст читався
        // як мертвий. Видно на екрані: підказка «мережа мовчить» під свіжим
        // списком.
        //
        // Живий міст -- це «дзвінок дійшов недавно», і ця відповідь уже є в
        // одному місці. Друга власна міра того самого була б третім домом для
        // факту, і розійшлася б так само.
        return !OZ_BridgeClient.Alive();
    }
}

// Конверт роду "roles" з моста.
class OZ_RolesSink : OZ_BridgeSink
{
    override void Deliver(string json)
    {
        OZ_RoleView v;
        string err;
        if (!JsonFileLoader<OZ_RoleView>.LoadData(json, v, err) || !v)
        {
            OZ_Log.Warn("roles: unreadable projection from the bridge: " + err);
            return;
        }

        OZ_Roles.Apply(v);
    }
}

// Конверт роду "roster": як звуться фракції. Приходить лише коли реєстр
// змінився, а не щоопиту.
class OZ_RosterSink : OZ_BridgeSink
{
    override void Deliver(string json)
    {
        OZ_FactionRoster r;
        string err;
        if (!JsonFileLoader<OZ_FactionRoster>.LoadData(json, r, err) || !r)
        {
            OZ_Log.Warn("factions: unreadable roster from the bridge: " + err);
            return;
        }

        OZ_Factions.ApplyRoster(r);

        // Підписи інших осей -- у словник. Фракції веде OZ_Factions, бо в них
        // є ще й ставлення; решта -- самі лише слова.
        OZ_RoleNames.Absorb(r.Ranks);
        OZ_RoleNames.Absorb(r.Traits);
        OZ_RoleNames.Absorb(r.Posts);
    }
}

// Як звуться звання, посади й мітки.
//
// Живе окремо від OZ_Factions, бо це не служба, а ПРОСТО СЛОВНИК: у нього
// немає ні ставлень, ні запасного шляху через файл акаунта, ні контракту для
// чужого постачальника. Єдина його робота -- перекласти слаг у те, що можна
// показати людині.
//
// Приїжджає з реєстру й НЕ ЗБЕРІГАЄТЬСЯ на диск, з тієї ж причини, що й
// підписи фракцій: підпис -- косметика, і записувати чуже слово у файл
// адміна означало б, що воно там лишиться, коли бота вже не буде.
//
// Слаг без підпису показуємо ЯК Є. Порожній рядок був би гірший: гравець
// побачив би нічого там, де в нього насправді є звання.
class OZ_RoleNames
{
    private static ref map<string, string> s_By;

    static void Absorb(array<ref OZ_RoleName> list)
    {
        if (!list)
            return;

        if (!s_By)
            s_By = new map<string, string>();

        for (int i = 0; i < list.Count(); i++)
        {
            if (!list[i])
                continue;
            if (list[i].Id == "")
                continue;
            if (list[i].DisplayName == "")
                continue;

            s_By.Set(list[i].Id, list[i].DisplayName);
        }
    }

    static string Of(string slug)
    {
        if (slug == "")
            return "";

        if (s_By)
        {
            string name;
            if (s_By.Find(slug, name))
                return name;
        }

        return slug;
    }

    static int Count()
    {
        if (!s_By)
            return 0;
        return s_By.Count();
    }
}

// Одне місце, щоб спитати все. Фасад, а не служба: він нічим не володіє й
// нічого не вирішує -- лише збирає відповідь із трьох, які володіють.
//
// Існує з двох причин, і обидві практичні: сторінка контактів хоче ОДИН
// виклик на гравця замість трьох, а Stale -- це один факт про міст, не три.
class OZ_Identity
{
    static string Faction(string uid)  { return OZ_Factions.OfUid(uid); }
    static string Rank(string uid)     { return OZ_Roles.RankOf(uid); }
    static bool   Leader(string uid)   { return OZ_Roles.IsLeader(uid); }
    static bool   Stale()              { return OZ_Roles.Stale(); }

    static bool HasTrait(string uid, string trait)
    {
        return OZ_Roles.HasTrait(uid, trait);
    }

    // --- останнє відоме, для офлайнових ---
    //
    // Ті самі три осі, але з знімка: гравця немає на сервері, питати міст про
    // нього нема сенсу, а показати «Іванов» без фракції й звання -- це збрехати
    // рівно там, де лідер вирішує, свій це чи ні.
    //
    // Окремі назви, а не прапорець у тих самих функціях: хто питає Seen*, той
    // СВІДОМО бере застаріле й мусить це показати.
    static string SeenFactionId(string uid)
    {
        // Файл акаунта старший: його ставили руками, і воно не застаріле.
        string byFile = OZ_Factions.OfUid(uid);
        if (byFile != "")
            return byFile;

        OZ_RoleView v = OZ_Roles.Seen(uid);
        if (!v)
            return "";
        return v.Faction;
    }

    static string SeenRankName(string uid)
    {
        OZ_RoleView v = OZ_Roles.Seen(uid);
        if (!v)
            return "";
        return OZ_RoleNames.Of(v.Rank);
    }

    static void SeenPostNames(string uid, out array<string> into)
    {
        if (!into)
            return;

        OZ_RoleView v = OZ_Roles.Seen(uid);
        if (!v)
            return;

        string faction = SeenFactionId(uid);
        if (faction == "")
            return;

        for (int i = 0; i < v.Posts.Count(); i++)
            into.Insert(OZ_RoleNames.Of(faction + ":" + v.Posts[i]));
    }

    static void SeenTraitNames(string uid, out array<string> into)
    {
        if (!into)
            return;

        OZ_RoleView v = OZ_Roles.Seen(uid);
        if (!v)
            return;

        for (int i = 0; i < v.Traits.Count(); i++)
            into.Insert(OZ_RoleNames.Of(v.Traits[i]));
    }

    // Звання людською назвою, готове до показу.
    static string RankName(string uid)
    {
        return OZ_RoleNames.Of(OZ_Roles.RankOf(uid));
    }

    // Посади людськими назвами. Слаг посади в реєстрі має вигляд
    // "duty:leader", а в проекції лежить сам "leader" -- тому фракцію
    // додаємо тут, а не змушуємо кожного, хто малює, пам'ятати про це.
    static void PostNames(string uid, out array<string> into)
    {
        if (!into)
            return;

        OZ_RoleView v = OZ_Roles.Of(uid);
        if (!v)
            return;

        string faction = OZ_Factions.OfUid(uid);
        if (faction == "")
            return;

        for (int i = 0; i < v.Posts.Count(); i++)
            into.Insert(OZ_RoleNames.Of(faction + ":" + v.Posts[i]));
    }

    static void TraitNames(string uid, out array<string> into)
    {
        if (!into)
            return;

        OZ_RoleView v = OZ_Roles.Of(uid);
        if (!v)
            return;

        for (int i = 0; i < v.Traits.Count(); i++)
            into.Insert(OZ_RoleNames.Of(v.Traits[i]));
    }
}
