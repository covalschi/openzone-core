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
    // ДВІ ОСІ НАЛЕЖНОСТІ, і вони незалежні (ТЗ-1 §2). Base -- «сталкер», є в
    // кожного; Org -- угруповання, не більше одного, порожньо в одинака.
    // Вступ до Org не чіпає Base, вихід із Org не чіпає ані Base, ані Rank.
    string Base         = "";
    string Org          = "";
    string Rank         = "";
    // ВНУТРIФРАКЦIЙНЕ звання -- окрема вiсь вiд сталкерського Rank
    // (рiшення власника 2026-08-30): живе всерединi угруповання, вмирає
    // разом iз членством, роздає лiдер. Слаг ГОЛИЙ, без префiкса фракцiї --
    // чиє це звання, каже Org поруч.
    string FRank        = "";
    // Видиме iм'я Discord: тiльки для адмiнських екранiв, гравцям не їде.
    string DName        = "";
    ref array<string> Posts;
    ref array<string> Traits;

    // Два УГРУПОВАННЯ разом -- це помилка налаштування гільдії, а не стан
    // гравця. Міст відмовляється вгадувати й перелічує обидва; ми лишаємо
    // Org порожнім і даємо адмінові побачити, що він накоїв. Base при цьому
    // заповнена як звичайно: конфлікт угруповань не робить людину не
    // сталкером (ТЗ-1 R1.5).
    ref array<string> Conflict;

    void OZ_RoleView()
    {
        Posts    = new array<string>();
        Traits   = new array<string>();
        Conflict = new array<string>();
    }
}

// Дзвінок «ролі змінились»: сторінки фракції і контактів оновлюються
// самі, а не чекають, поки гравець перевідкриє вкладку. Лінива побудова
// навмисно: статичний new у декларації НЕ виконується (зміряно).
class OZ_RoleNotify
{
    private static ref ScriptInvoker s_On;

    static ScriptInvoker On()
    {
        if (!s_On)
            s_On = new ScriptInvoker();
        return s_On;
    }
}

class OZ_Roles
{
    private static ref map<string, ref OZ_RoleView> s_By;

    private static ref map<string, string> s_DName;

    // Видиме iм'я Discord з останньої проекцiї; порожньо, коли не знаємо.
    static string DiscordNameOf(string uid)
    {
        if (!s_DName)
            return "";
        string n;
        if (!s_DName.Find(uid, n))
            return "";
        return n;
    }

    static void Apply(OZ_RoleView v)
    {
        if (!GetGame().IsServer())
            return;
        if (!v)
            return;
        if (v.Uid == "")
            return;

        if (v.DName != "")
        {
            if (!s_DName)
                s_DName = new map<string, string>();
            s_DName.Set(v.Uid, v.DName);
        }

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
            m += " base=\"" + v.Base;
            m += "\" org=\"" + v.Org;
            m += "\" rank=\"" + v.Rank;
            m += "\" frank=\"" + v.FRank;
            m += "\" posts=" + v.Posts.Count().ToString();
            m += " traits=" + v.Traits.Count().ToString();
            OZ_Log.Info(m);
        }

        // Належність веде ЇЇ служба -- одна правда, одне місце. Для
        // УГРУПОВАННЯ порожній рядок значущий: він означає «Discord каже, що
        // угруповання немає», і OZ_Factions саме так його й читає.
        //
        // Для БАЗОВОЇ -- ні: її призначила гра, і порожнє поле в проекції
        // означає лише «міст про неї не сказав» (R5.4). Правило живе в
        // OZ_Factions.BaseOfUid, тут ми просто чесно кладемо, що приїхало.
        OZ_Factions.SetFromRole(v.Uid, v.Org);
        OZ_Factions.SetBaseFromRole(v.Uid, v.Base);

        if (changed)
        {
            Remember(v);
            OZ_RoleNotify.On().Invoke(v.Uid);
        }

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
    // не в BaseFaction/OrgFaction: див. довгу причину в OZ_PlayerData.
    private static void Remember(OZ_RoleView v)
    {
        OZ_PlayerData d = OZ_PlayerStore.Load(v.Uid);
        if (!d)
            return;

        d.SeenBase    = v.Base;
        d.SeenOrg     = v.Org;
        d.SeenRank    = v.Rank;
        d.SeenFRank   = v.FRank;

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
        if (d.SeenBase == "" && d.SeenOrg == "" && d.SeenRank == "")
            return null;

        OZ_RoleView v = new OZ_RoleView();
        v.Uid   = uid;
        v.Base  = d.SeenBase;
        v.Org   = d.SeenOrg;
        v.Rank  = d.SeenRank;
        v.FRank = d.SeenFRank;

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
    // Усі, про кого сервер ЗНАЄ, що вони в цьому УГРУПОВАННІ. Це кеш
    // проекцій за цей запуск -- офлайн, про якого ніхто не питав, сюди не
    // потрапить, і чесніше показати менше, ніж вигадати повний список.
    //
    // Саме угруповання, і це не дрібниця: по базовій осі «склад» -- це весь
    // сервер, і екран, який колись питав тут «сталкерів», віддавав справжні
    // імена всіх гравців кожному новачкові (ТЗ-1 §6).
    static void OrgMembers(string org, array<string> outUids)
    {
        if (!s_By || org == "")
            return;

        for (int i = 0; i < s_By.Count(); i++)
        {
            OZ_RoleView v = s_By.GetElement(i);
            if (v && v.Org == org && outUids.Find(v.Uid) == -1)
                outUids.Insert(v.Uid);
        }
    }

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
        if (a.Base != b.Base)
            return false;
        if (a.Org != b.Org)
            return false;
        if (a.Rank != b.Rank)
            return false;
        if (a.FRank != b.FRank)
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

    // Внутрiфракцiйне звання. Порожньо -- без звання, або поза фракцiєю,
    // або проекцiя ще не приїхала. Iнтерфейс для ЧУЖИХ модiв теж: квест
    // може питати FRankOf так само, як HasTrait.
    static string FRankOf(string uid)
    {
        OZ_RoleView v = Of(uid);
        if (!v)
            return "";
        return v.FRank;
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

    // Мiтки гравця одним рядком ("medic,mechanic") -- для адмiнського
    // ростера. Порожньо -- мiток немає або проекцiя ще не приїхала.
    static string TraitsLineOf(string uid)
    {
        OZ_RoleView v = Of(uid);
        if (!v)
            return "";
        return TraitsLine(v);
    }

    // Те саме для проекції, якої в кеші НЕМАЄ: адмінський ростер везе з
    // бази бота й офлайнових (ТЗ-4 R-C4.2), а класти їх у кеш не можна --
    // кеш означає «в Зоні».
    static string TraitsLine(OZ_RoleView v)
    {
        if (!v)
            return "";

        string line = "";
        for (int i = 0; i < v.Traits.Count(); i++)
        {
            if (line != "")
                line += ",";
            line += v.Traits[i];
        }
        return line;
    }

    static bool ViewIsLeader(OZ_RoleView v)
    {
        if (!v)
            return false;
        return v.Posts.Find("leader") != -1;
    }

    // ------------------------------------------- каталоги мiток i звань
    //
    // КАТАЛОГИ, а не чиїсь ролi: перелiки слагiв, якi реєстр бота взагалi
    // знає. Потрiбнi адмiнському UI, щоб було з чого вибирати. Живуть тут,
    // бо приїжджають тим самим конвертом роду "roster", що й проекцiї.
    private static ref array<string> s_TraitIds;
    private static ref array<string> s_RankIds;
    // Внутрiфракцiйнi звання, id-шники вигляду "duty:sergeant".
    private static ref array<string> s_FRankIds;

    static void RememberTraitIds(array<ref OZ_RoleName> list)
    {
        if (!s_TraitIds)
            s_TraitIds = new array<string>();
        Absorb(list, s_TraitIds);
    }

    static void RememberRankIds(array<ref OZ_RoleName> list)
    {
        if (!s_RankIds)
            s_RankIds = new array<string>();
        Absorb(list, s_RankIds);
    }

    // Внутрiфракцiйнi звання зберiгаємо З ПОРЯДКОМ: без нього «пiдвищити»
    // не має змiсту -- слаги гра знає, а хто з них вищий, каже реєстр.
    private static ref array<int> s_FRankOrder;

    static void RememberFRankIds(array<ref OZ_RoleName> list)
    {
        if (!s_FRankIds)
            s_FRankIds = new array<string>();
        if (!s_FRankOrder)
            s_FRankOrder = new array<int>();

        if (!list)
            return;

        for (int i = 0; i < list.Count(); i++)
        {
            if (!list[i] || list[i].Id == "")
                continue;

            int at = s_FRankIds.Find(list[i].Id);
            if (at == -1)
            {
                s_FRankIds.Insert(list[i].Id);
                s_FRankOrder.Insert(list[i].Order);
                continue;
            }

            // Порядок могли переставити -- беремо свiжий.
            s_FRankOrder[at] = list[i].Order;
        }
    }

    // Драбина ОДНIЄЇ фракцiї, знизу вгору: слаги без префiкса й пiдписи
    // поруч. Порожньо -- у фракцiї звань не завели.
    static void FRankLadder(string faction, out array<string> outIds, out array<string> outNames)
    {
        if (!outIds)
            outIds = new array<string>();
        if (!outNames)
            outNames = new array<string>();
        outIds.Clear();
        outNames.Clear();

        if (faction == "" || !s_FRankIds || !s_FRankOrder)
            return;

        string prefix = faction + ":";
        array<int> orders = new array<int>();

        for (int i = 0; i < s_FRankIds.Count(); i++)
        {
            if (s_FRankIds[i].IndexOf(prefix) != 0)
                continue;

            string bare = s_FRankIds[i].Substring(prefix.Length(), s_FRankIds[i].Length() - prefix.Length());
            int ord = s_FRankOrder[i];

            // Вставляємо ЗА ПОРЯДКОМ одразу: драбина мусить приїхати
            // клiєнтовi шикованою, iнакше «наступне вище» рахуватиме
            // кожен екран сам i по-своєму.
            int at = 0;
            while (at < orders.Count() && orders[at] <= ord)
                at++;

            orders.InsertAt(ord, at);
            outIds.InsertAt(bare, at);
            outNames.InsertAt(OZ_RoleNames.Of(s_FRankIds[i]), at);
        }
    }

    private static void Absorb(array<ref OZ_RoleName> list, array<string> into)
    {
        if (!list)
            return;

        for (int i = 0; i < list.Count(); i++)
        {
            if (!list[i] || list[i].Id == "")
                continue;
            if (into.Find(list[i].Id) == -1)
                into.Insert(list[i].Id);
        }
    }

    static void TraitIds(array<string> into)
    {
        if (!into || !s_TraitIds)
            return;
        for (int i = 0; i < s_TraitIds.Count(); i++)
            into.Insert(s_TraitIds[i]);
    }

    static void RankIds(array<string> into)
    {
        if (!into || !s_RankIds)
            return;
        for (int i = 0; i < s_RankIds.Count(); i++)
            into.Insert(s_RankIds[i]);
    }

    static void FRankIds(array<string> into)
    {
        if (!into || !s_FRankIds)
            return;
        for (int i = 0; i < s_FRankIds.Count(); i++)
            into.Insert(s_FRankIds[i]);
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
        OZ_RoleNames.Absorb(r.FRanks);

        // А СЛАГИ мiток i звань -- ще й списками: адмiнському UI треба з
        // чого вибирати, а словник вибору не вiддає.
        OZ_Roles.RememberTraitIds(r.Traits);
        OZ_Roles.RememberRankIds(r.Ranks);
        OZ_Roles.RememberFRankIds(r.FRanks);
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

