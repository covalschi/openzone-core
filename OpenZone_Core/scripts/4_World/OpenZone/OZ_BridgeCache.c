// Кеш відповідей моста -- лише в пам'яті, лише читання (ТЗ-2 R4.3, R4.4).
//
// Сторінки, чий дім -- бот (новини, розмови), ходили до моста на КОЖНЕ
// відкриття: той самий список новин, та сама історія розмови, той самий
// гравець -- і щоразу HTTP. Тут лежить остання відповідь на той самий лист,
// і поки світ не змінився, вона й віддається.
//
// «СВІТ ЗМІНИВСЯ» ВИЗНАЧАЄ ОПИТ, А НЕ ГОДИННИК (R4.4). Усе, що міняє листування,
// приходить у пачці опиту -- чужий рядок, нова новина, зміна ролі -- і разом
// із нею їде курсор. Пачка з конвертами або з новим курсором скидає кеш
// цілком. Окремого запиту «скинь кеш» немає й не треба.
//
// Другий привід скинути -- власний ЗАПИС через міст: відправлений рядок,
// створена група, пост. Відповідь моста на такий лист ми не знаємо наперед,
// а те, що після нього лежить у кеші, вже не про цей світ.
//
// НА ДИСК СЕРВЕРА DAYZ НІЧОГО НЕ ПИШЕТЬСЯ (R4.3): це map у статиці, і він
// зникає разом із процесом. Листування живе в базі бота, і лише там.
//
// Ключ -- дорога плюс сам лист: лист містить Uid, тож відповідь одного гравця
// ніколи не дістанеться іншому.
//
// Строк життя -- запобіжник, а не механізм: за хвилину без опиту (міст
// мовчав, а потім заговорив) стара відповідь не мусить пережити те, чого
// опит не привіз.

class OZ_BridgeCache
{
    static const int TTL_MS  = 60000;
    static const int MAX_KEYS = 512;

    private static ref map<string, string> s_Body = new map<string, string>();
    private static ref map<string, int>    s_At   = new map<string, int>();

    // Лише те, що читає й нічого не змінює на мосту. Перелік короткий і
    // явний: дорога, якої тут немає, кеш не торкається -- і скидає його.
    static bool Readable(string route)
    {
        if (route == "v1/news/list")  return true;
        if (route == "v1/news/open")  return true;
        if (route == "v1/chat/list")  return true;
        if (route == "v1/chat/open")  return true;
        if (route == "v1/chat/older") return true;
        return false;
    }

    // Ні читання листування, ні його зміна: питання про права й про стан
    // привязки. Такі не кешуються (відповідь -- про мить), але й кеш не
    // скидають: зміряно, що сторінка новин просить список і голоси разом, і
    // без цього списку в кеші не жив довше одного запиту.
    static bool Neutral(string route)
    {
        if (route == "v1/news/voices") return true;
        if (route == "v1/link/status") return true;

        // ПРИВ'ЯЗКА НІЧОГО НЕ МІНЯЄ В ЛИСТУВАННІ. Запит коду -- питання про
        // одну людину й одну мить; він не читальний (відповідь щоразу інша)
        // і не пише нічого, що лежить у кеші. Без цього рядка кожне
        // натискання «отримати код» гасило чат і новини всьому серверу.
        if (route == "v1/link/begin") return true;

        // РОСТЕР ролей теж: він відповідає про фракції, а кешуємо ми лише
        // новини й розмови.
        if (route == "v1/roles/roster") return true;

        return false;
    }

    // Роди, які кеш РОЗУМІЄ. Конверт незнайомого роду скидає кеш цілком: ми
    // не знаємо, чого він торкнувся, і вгадувати тут не можна.
    private static bool KnownKind(string kind)
    {
        if (kind == "chat")   return true;
        if (kind == "news")   return true;
        if (kind == "roles")  return true;
        if (kind == "roster") return true;

        // ВАЙП -- ЖИВИЙ РІД, і його тут бракувало: міст шле його
        // (openzone-bridge/src/index.js:881,1498), мод фракцій на нього
        // підписаний (OZF_Module.c:68), а кеш читав у лозі «poll item of
        // unknown kind "wipe"» і скидався ЦІЛКОМ. Скидання цілком було
        // випадково правильним, і саме тому небезпечним: додати рід у цей
        // список і нічого більше означало б тихо перетворити його на
        // порожню дію -- доріг "v1/wipe/" не існує. Що він застарює
        // насправді -- у PrefixOf нижче.
        if (kind == "wipe")   return true;

        // "link" ТУТ БІЛЬШЕ НЕМАЄ: такого конверта не шле ніхто (у мості
        // жодного kind: 'link'), і не читає теж ніхто.
        return false;
    }

    // Що саме застаріває від конверта цього роду.
    //
    // Зазвичай -- дорога того самого імені: "news" застарює "v1/news/".
    // Виняток один, і він мовчазний: вайп гравця переписує СКЛАД РОЗМОВ
    // (openzone-bridge/src/index.js, wipePlayer чистить c.members і архівує
    // приватний тред), тобто застарює чат -- при тому, що дороги "v1/wipe/"
    // не існує зовсім.
    private static string PrefixOf(string kind)
    {
        if (kind == "wipe")
            return "v1/chat/";

        return "v1/" + kind + "/";
    }

    private static string Key(string route, string letter)
    {
        return route + "|" + letter;
    }

    static bool Get(string route, string letter, out string json)
    {
        string k = Key(route, letter);
        int at;
        if (!s_At.Find(k, at))
            return false;

        if (GetGame().GetTime() - at > TTL_MS)
        {
            s_Body.Remove(k);
            s_At.Remove(k);
            return false;
        }

        if (!s_Body.Find(k, json))
            return false;

        OZ_Log.Dbg("bridge cache: hit " + route);
        return true;
    }

    // Відмову не кешуємо: {"Error": ...} -- це відповідь про мить, а не про
    // світ, і наступний запит має право отримати іншу.
    //
    // ПО ФОРМІ, А НЕ ПО ПІДРЯДКУ. Пошук лапкового "error" будь-де в тілі
    // означав, що звичайне повідомлення чи новина зі словом error у тексті
    // гасила кеш цілої сторінки. Міст ставить відмову ПЕРШИМ ключем
    // ({"Error":"no_chat"}), і саме це ми й питаємо.
    static void Put(string route, string letter, string json)
    {
        if (json == "")
            return;
        if (json.IndexOf("{\"Error\"") == 0)
            return;

        if (s_Body.Count() >= MAX_KEYS)
            Clear("full");

        string k = Key(route, letter);
        s_Body.Set(k, json);
        s_At.Set(k, GetGame().GetTime());
    }

    // Скинути ЛИШЕ те, що належить цьому родові.
    //
    // Кеш скидався ЦІЛКОМ на будь-яку непорожню пачку опиту -- тобто одне
    // чуже повідомлення в чаті викидало з кеша новини й розмови всіх
    // вісімдесяти гравців, і наступне відкриття будь-якої сторінки знову
    // йшло по HTTP. Ключ несе дорогу, дорога починається з "v1/<рід>/", тож
    // рід із конверта прямо називає, що саме застаріло.
    //
    // Повертає false для роду, якого ми не знаємо: тоді викликач скидає все.
    static bool Invalidate(string kind, string why)
    {
        if (kind == "" || !KnownKind(kind))
            return false;

        if (s_Body.Count() == 0)
            return true;

        string prefix = PrefixOf(kind);

        array<string> doomed = new array<string>();
        for (int i = 0; i < s_Body.Count(); i++)
        {
            string k = s_Body.GetKey(i);
            if (k.IndexOf(prefix) == 0)
                doomed.Insert(k);
        }

        if (doomed.Count() == 0)
            return true;

        for (int j = 0; j < doomed.Count(); j++)
        {
            s_Body.Remove(doomed[j]);
            s_At.Remove(doomed[j]);
        }

        OZ_Log.Dbg("bridge cache: dropped " + doomed.Count().ToString() + " of " + prefix + " (" + why + ")");
        return true;
    }

    static void Clear(string why)
    {
        if (s_Body.Count() == 0)
            return;

        OZ_Log.Dbg("bridge cache: cleared " + s_Body.Count().ToString() + " (" + why + ")");
        s_Body.Clear();
        s_At.Clear();
    }
}

// Обгортка відповіді: кладе тіло в кеш і передає далі тому, хто питав.
// Відмови й тиша йдуть далі як є -- їх не кешують.
class OZ_BridgeCacheFill : OZ_BridgeReply
{
    private string m_Route;
    private string m_Letter;
    private ref OZ_BridgeReply m_Inner;

    void OZ_BridgeCacheFill(string route, string letter, OZ_BridgeReply inner)
    {
        m_Route  = route;
        m_Letter = letter;
        m_Inner  = inner;
    }

    override void OnBody(string json)
    {
        OZ_BridgeCache.Put(m_Route, m_Letter, json);
        if (m_Inner)
            m_Inner.OnBody(json);
    }

    override void OnFail(int code)
    {
        if (m_Inner)
            m_Inner.OnFail(code);
    }

    override void OnQuiet()
    {
        if (m_Inner)
            m_Inner.OnQuiet();
    }
}
