// ЗАПИС ДЛЯ ЧУЖОГО МОДА -- і рівно стільки, скільки йому можна дати.
//
// Служба ядра OZ_Identity лишається ТІЛЬКИ на читання: змінити належність,
// призначити звання, зробити лідером -- це операції ВЛАСНИКА даних, і чужий
// мод не має отримати їх повз його правила. Хто хоче писати -- залежить від
// OpenZone_Factions явно, і потрапляє сюди.
//
// ТУТ ЛИШЕ ОСОБИСТІ ОСІ: сталкерське звання й мітки. Вони належать ЛЮДИНІ, а
// не організації: вступ до угруповання їх не дає, вихід не знімає (ТЗ-1 §2).
// Організації, внутрішнього звання, посад і лідерства тут НЕМАЄ навмисно
// (R7.3) -- це операції лідера й адміна, і квестовий мод до них не ходить.
//
// ЩО ЦЕ ЗА ФОРМА. Асинхронно: гра просить -> бот пише в Discord -> проекція
// повертається -> гра оновилась. Локально не застосовується НІЧОГО, і це не
// обережність, а те саме правило, про яке попереджає шапка цього мода: роль,
// знята при мертвому мості, інакше лишилась би знятою назавжди.
//
// МІСТ ЛЕЖИТЬ -- ВІДМОВА, ВИДИМА Й З ПРИЧИНОЮ (R7.8). Черги немає навмисно:
// квест, який сказав «вас підвищено» в порожнечу, гірший за помилку.
//
// ЗА ЗАМОВЧУВАННЯМ НЕ КЛИЧЕ НІХТО (R7.9). Звання й мітки не рухаються, поки
// не з'явиться мод, який їх рухає.

// Куди прийде відповідь. Успадковуєш, перекриваєш Done.
//
// `ok` -- чи змінилося щось насправді; `why` -- СЛОВА МОСТА при відмові, а не
// наш код помилки: він єдиний знає, чому саме Discord відмовив, і «бот не може
// керувати цією роллю -- підніми його роль вище» набагато корисніше за «не
// вдалося».
class OZ_GrantSink
{
    void Done(bool ok, string why)
    {
    }
}

// Внутрішнє: відповідь моста -> Done + рядок в аудит гри.
class OZ_GrantReply : OZ_BridgeReply
{
    protected string m_Uid;
    protected string m_Op;
    protected string m_Why;
    protected ref OZ_GrantSink m_Sink;

    void OZ_GrantReply(string uid, string op, string why, OZ_GrantSink sink)
    {
        m_Uid  = uid;
        m_Op   = op;
        m_Why  = why;
        m_Sink = sink;
    }

    private void Tell(bool ok, string why)
    {
        if (m_Sink)
            m_Sink.Done(ok, why);
    }

    override void OnBody(string json)
    {
        OZ_RoleAnswer a;
        string err;
        if (!JsonFileLoader<OZ_RoleAnswer>.LoadData(json, a, err) || !a)
        {
            OZ_Log.Error("grant: unreadable answer for " + m_Op + ": " + err);
            Tell(false, "STR_OZ_ERR_INTERNAL");
            return;
        }

        if (a.Ok)
        {
            // ПРИЧИНА ЙДЕ В ЛОГ, а не лише в аудит бота: коли завтра хтось
            // спитає, чому в гравця з'явилась мітка, відповідь має бути на
            // тому ж боці, де й решта історії сервера.
            OZ_Log.Info("grant: " + m_Uid + " " + m_Op + " -- accepted by Discord (" + m_Why + ")");
            Tell(true, "");
            return;
        }

        OZ_Log.Warn("grant: " + m_Uid + " " + m_Op + " refused: " + a.Why);
        Tell(false, a.Why);
    }

    override void OnFail(int code)
    {
        OZ_Log.Warn("grant: " + m_Uid + " " + m_Op + " could not reach the bridge");
        Tell(false, "STR_OZ_ERR_NO_BRIDGE");
    }
}

class OZ_RoleGrant
{
    // Сталкерське звання. Слаг ГОТОВИЙ -- сходинку рахує ВИКЛИКАЧ (R7.7):
    // драбину він бере з реєстру звань, а «на сходинку вище» -- рішення
    // квесту, а не фракцій. Порожній слаг знімає звання.
    static void SetRank(string uid, string rankSlug, string why, OZ_GrantSink done)
    {
        Ask(uid, OZ_RoleOp.RANK_SET, rankSlug, why, done);
    }

    static void AddTrait(string uid, string trait, string why, OZ_GrantSink done)
    {
        Ask(uid, OZ_RoleOp.TRAIT_ADD, trait, why, done);
    }

    static void RemoveTrait(string uid, string trait, string why, OZ_GrantSink done)
    {
        Ask(uid, OZ_RoleOp.TRAIT_REMOVE, trait, why, done);
    }

    // `why` ОБОВ'ЯЗКОВИЙ і непорожній (R7.6). Він іде в лог гри й в аудит
    // бота, і без нього запис «комусь дали звання» не відповідає на єдине
    // питання, заради якого його читатимуть.
    private static void Ask(string uid, string op, string arg, string why, OZ_GrantSink done)
    {
        if (!GetGame().IsServer())
            return;

        if (uid == "")
        {
            Refuse(done, "STR_OZ_ERR_NO_TARGET");
            return;
        }

        if (why == "")
        {
            OZ_Log.Error("grant: " + op + " for " + uid + " came without a reason and was refused");
            Refuse(done, "STR_OZ_ERR_NO_REASON");
            return;
        }

        // Міст лежить -- відмова ЗАРАЗ, а не намір на потім.
        if (!OZ_BridgeClient.Alive())
        {
            Refuse(done, "STR_OZ_ERR_NO_BRIDGE");
            return;
        }

        // Admin = true: це не дія гравця, і питати міст про лідерство
        // прохача нема сенсу -- прохача немає. Актора не називаємо саме тому.
        OZ_RoleAsk a = new OZ_RoleAsk();
        a.TargetUid = uid;
        a.Op        = op;
        a.Arg       = arg;
        a.Admin     = true;

        string letter;
        string err;
        if (!JsonFileLoader<OZ_RoleAsk>.MakeData(a, letter, err, false))
        {
            OZ_Log.Error("grant: cannot build the letter: " + err);
            Refuse(done, "STR_OZ_ERR_INTERNAL");
            return;
        }

        OZ_Log.Info("grant: " + uid + " " + op + " \"" + arg + "\" -- " + why);
        OZ_BridgeClient.Call("v1/roles/apply", letter, new OZ_GrantReply(uid, op, why, done));
    }

    // Відмова -- теж відповідь. Мовчазний вихід залишив би того, хто просив,
    // чекати назавжди, а це і є та сама черга, якої тут не має бути.
    private static void Refuse(OZ_GrantSink done, string why)
    {
        if (done)
            done.Done(false, why);
    }
}
