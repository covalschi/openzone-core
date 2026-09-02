// Розділ консолі NEWS: адмін пише новину від імені будь-якої персони.
//
// ТЗ-6 R2.2. Персони чеканить і роздає адмін у Discord
// (/openzone persona grant); тут він ними КОРИСТУЄТЬСЯ.
//
// ЧОМУ ЦЕ РОЗДІЛ КОНСОЛІ, А НЕ СТОРІНКА КПК. Сторінка КПК належить приладу:
// її видно, коли прилад у руках, і вона питає гейт про доступ до сторінки.
// Інструмент адміна не має до приладу стосунку взагалі -- рішення власника
// 2026-09-01 називає дві різні поверхні для двох різних людей: лідер пише з
// КПК, адмін -- із VPP.
//
// ПРАВ ТУТ НЕ ПЕРЕВІРЯЄМО: їх перевірив диспетчер (OZ_Perm.IsAdmin), до
// розбору операції. Друга перевірка створила б друге місце, де межу безпеки
// можна забути.
//
// ВІДПОВІДАЄМО ВІДКЛАДЕНО. Пост їде в міст, а міст -- у Discord; поки він
// летить, сервер не тримає адміна в невіданні й не бреше йому «готово».
// Тому Handle віддає OZ_Const.DEFER, а справжню відповідь надсилає
// OZ_NewsAdminReply, коли вона приїде.

// Імена з префіксом Admin, бо мод КПК уже володіє OZ_NewsReply, OZ_NewsAsk
// і сусідніми: Enforce має один простір імен на всі моди, і збіг ловиться
// лише компіляцією сервера -- як цей і зловився.
class OZ_NewsAdminAsk
{
    string Uid   = "";
    string Who   = "";
    string Title = "";
    string Body  = "";
}

class OZ_NewsAdminVoices
{
    string Self   = "";
    bool   Admin  = false;
    bool   Leader = false;
    string Org    = "";
    ref array<string> Voices;

    void OZ_NewsAdminVoices()
    {
        Voices = new array<string>();
    }
}

class OZ_NewsAdminAnswer
{
    bool   ok    = false;
    string Who   = "";
    string Error = "";
}

// Відповідь моста -> адмінові, тим самим конвертом, що й решта розділу.
class OZ_NewsAdminReply : OZ_BridgeReply
{
    protected string m_Who;
    protected string m_Op;

    void OZ_NewsAdminReply(string who, string op)
    {
        m_Who = who;
        m_Op  = op;
    }

    // Кому відповідати. Особу беремо ЗАНОВО за uid: поки лист летів, гравець
    // міг вийти, а тримати протухлу PlayerIdentity й діяти за нею -- те саме,
    // від чого застерігає OZ_PdaLookup.
    private PlayerIdentity To()
    {
        return OZ_Link.Online(m_Who);
    }

    override void OnBody(string json)
    {
        PlayerIdentity to = To();
        if (!to)
            return;

        OZ_NewsAdminAnswer a;
        string err;
        if (!JsonFileLoader<OZ_NewsAdminAnswer>.LoadData(json, a, err) || !a)
        {
            OZ_Rpc.AdminRespond(to, OZ_AdminSect.NEWS, m_Op, false, "", "STR_OZ_ERR_INTERNAL");
            return;
        }

        // Відмову віддаємо СЛОВАМИ МОСТА. Він єдиний знає, чому саме: чужа
        // персона, порожній заголовок, немає права писати. Свій код помилки
        // тут означав би перекладати те, чого ми не бачили.
        if (a.Error != "")
        {
            OZ_Log.Warn("news: " + m_Op + " refused by the bridge: " + a.Error);
            OZ_Rpc.AdminRespond(to, OZ_AdminSect.NEWS, m_Op, false, "", a.Error);
            return;
        }

        if (m_Op == "news_post")
            OZ_Log.Info("news: posted as \"" + a.Who + "\"");

        OZ_Rpc.AdminRespond(to, OZ_AdminSect.NEWS, m_Op, true, json, "");
    }

    override void OnFail(int code)
    {
        PlayerIdentity to = To();
        if (to)
            OZ_Rpc.AdminRespond(to, OZ_AdminSect.NEWS, m_Op, false, "", "STR_OZ_ERR_NO_BRIDGE");
    }
}

class OZ_NewsSection : OZ_AdminSection
{
    override string Handle(string op, string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok    = false;
        error = "STR_OZ_ERR_UNKNOWN_OP";

        if (!sender)
            return "";

        // Міст лежить -- кажемо це ЗАРАЗ, а не мовчимо до таймауту. Черги
        // немає навмисно: новина, яку адмін вважає надісланою, не має
        // з'явитися через півгодини сама (те саме правило, що в ТЗ-2 R4.2).
        if (!OZ_BridgeClient.Alive())
        {
            error = "STR_OZ_ERR_NO_BRIDGE";
            return "";
        }

        if (op == "news_voices")
            return Ask("v1/news/voices", sender, op, "", error);

        if (op == "news_post")
            return Ask("v1/news/post", sender, op, json, error);

        return "";
    }

    // Спитати міст. Uid підставляємо МИ, з особи відправника: клієнт не
    // називає, за кого просить, і не може -- у конверті немає такого поля.
    // Те саме правило, що в OZ_Req і в ролевих операціях.
    private string Ask(string route, PlayerIdentity sender, string op, string json, out string error)
    {
        OZ_NewsAdminAsk a = new OZ_NewsAdminAsk();
        a.Uid = sender.GetPlainId();

        if (json != "")
        {
            OZ_NewsAdminAsk from;
            string perr;
            if (!JsonFileLoader<OZ_NewsAdminAsk>.LoadData(json, from, perr) || !from)
            {
                error = "STR_OZ_ERR_INTERNAL";
                return "";
            }
            a.Who   = from.Who;
            a.Title = from.Title;
            a.Body  = from.Body;
        }

        string letter;
        string err;
        if (!JsonFileLoader<OZ_NewsAdminAsk>.MakeData(a, letter, err, false))
        {
            OZ_Log.Error("news: cannot build the letter: " + err);
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_BridgeClient.Call(route, letter, new OZ_NewsAdminReply(a.Uid, op));

        // Відповідь прийде сама. Диспетчер це впізнає й нічого не надішле.
        error = OZ_Const.DEFER;
        return "";
    }
}
