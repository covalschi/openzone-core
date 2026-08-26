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

    // Коли востаннє щось приїжджало від моста, у мілісекундах.
    private static int s_LastAt = 0;

    // Скільки проекція вважається свіжою. Опит моста тримається до восьми
    // секунд, тож тридцять -- це «пропустили три поспіль», а не «затримка».
    private static const int STALE_MS = 30000;

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
        s_LastAt = GetGame().GetTime();

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

        if (v.Conflict.Count() > 1)
        {
            string w = "roles: " + v.Uid;
            w += " holds more than one faction role (";
            w += v.Conflict[0] + ", " + v.Conflict[1];
            w += ") - showing none until the guild is fixed";
            OZ_Log.Warn(w);
        }
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
        if (s_LastAt == 0)
            return true;
        return (GetGame().GetTime() - s_LastAt) > STALE_MS;
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
}
