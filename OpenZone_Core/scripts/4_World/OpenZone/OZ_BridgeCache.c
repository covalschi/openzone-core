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

    private static ref map<string, string> s_Body;
    private static ref map<string, int>    s_At;
    private static int s_Hits   = 0;
    private static int s_Misses = 0;

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
        return false;
    }

    private static void Ensure()
    {
        if (!s_Body)
            s_Body = new map<string, string>();
        if (!s_At)
            s_At = new map<string, int>();
    }

    private static string Key(string route, string letter)
    {
        return route + "|" + letter;
    }

    static bool Get(string route, string letter, out string json)
    {
        Ensure();

        string k = Key(route, letter);
        int at;
        if (!s_At.Find(k, at))
        {
            s_Misses++;
            return false;
        }

        if (GetGame().GetTime() - at > TTL_MS)
        {
            s_Body.Remove(k);
            s_At.Remove(k);
            s_Misses++;
            return false;
        }

        if (!s_Body.Find(k, json))
        {
            s_Misses++;
            return false;
        }

        s_Hits++;
        OZ_Log.Dbg("bridge cache: hit " + route + " (" + Stat() + ")");
        return true;
    }

    // Відмову не кешуємо: {"Error": ...} -- це відповідь про мить, а не про
    // світ, і наступний запит має право отримати іншу.
    static void Put(string route, string letter, string json)
    {
        if (json == "" || json.IndexOf("\"Error\"") != -1 || json.IndexOf("\"error\"") != -1)
            return;

        Ensure();

        if (s_Body.Count() >= MAX_KEYS)
            Clear("full");

        string k = Key(route, letter);
        s_Body.Set(k, json);
        s_At.Set(k, GetGame().GetTime());
    }

    static void Clear(string why)
    {
        if (!s_Body || s_Body.Count() == 0)
            return;

        OZ_Log.Dbg("bridge cache: cleared " + s_Body.Count().ToString() + " (" + why + ")");
        s_Body.Clear();
        s_At.Clear();
    }

    static string Stat()
    {
        return "hits=" + s_Hits.ToString() + " misses=" + s_Misses.ToString();
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
