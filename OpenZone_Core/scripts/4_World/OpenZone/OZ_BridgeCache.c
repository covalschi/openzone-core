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

    // ЖОДНОГО ЧУЖОГО ІМЕНІ В ЦЬОМУ ФАЙЛІ (дизайн платформи §4).
    //
    // Тут стояли три списки літералів: читальні дороги ("v1/news/list",
    // "v1/chat/open"...), нейтральні ("v1/roles/roster") і роди, які кеш
    // «розуміє» ("chat", "news", "roles", "roster", "wipe") -- тобто словник
    // КПК і мода фракцій усередині ядра. Третій мод зі своїм родом не мав
    // способу туди потрапити: кожен його конверт скидав кеш цілком, а кожна
    // його дорога вважалась записом.
    //
    // Тепер усе це каже сам мод -- OZ_BridgeSink.Reads/Neutral/Stales, --
    // а ядро лише питає (OZ_BridgeClient.RouteRole/StaleRoutes). Мовчазний
    // мод нічого не ламає: його дороги вважаються записувальними, тобто
    // консервативно, як і будь-яка незнайома дорога.

    // Лише те, що читає й нічого не змінює на мосту.
    static bool Readable(string route)
    {
        return OZ_BridgeClient.RouteRole(route) == OZ_BridgeClient.ROUTE_READ;
    }

    // Ні читання листування, ні його зміна: питання про права, про стан
    // прив'язки, про голоси новин. Такі не кешуються (відповідь -- про мить),
    // але й кеш не скидають: сторінка новин просить список і голоси разом, і
    // без цього список у кеші не жив довше одного запиту.
    static bool Neutral(string route)
    {
        return OZ_BridgeClient.RouteRole(route) == OZ_BridgeClient.ROUTE_NEUTRAL;
    }

    // Дороги, які застарює конверт цього роду. Буфер один на весь клас:
    // Absorb питає його по конверту на кожну пачку опиту.
    private static ref array<string> s_Doomed = new array<string>();
    private static ref array<string> s_Stale  = new array<string>();

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
    // йшло по HTTP. Ключ несе дорогу, а дороги роду називає сам мод, тож рід
    // із конверта прямо каже, що саме застаріло.
    //
    // ДОРОГА ЦІЛКОМ, А НЕ ПРЕФІКС: список читальних доріг у нас тепер точний,
    // і збіг по префіксу "v1/<рід>/" був лише здогадом про те, як мод назвав
    // свої дороги.
    //
    // Повертає false для роду, про застарювання якого ніхто нічого не сказав:
    // тоді викликач скидає все.
    //
    // ПОРОЖНІЙ СПИСОК -- ЦЕ ТЕЖ «НІХТО НЕ СКАЗАВ», а не «нічого не
    // застаріло». Рід, чий sink не оголосив ані читальних доріг, ані чужого
    // роду, який він переписує (вайп -> чат), не знає про свої наслідки
    // нічого -- і тиха відповідь «так, я все прибрав» лишала б у кеші склад
    // розмов, переписаний тим самим вайпом. Консервативна половина тут
    // коштує один зайвий похід до моста, а неконсервативна -- відповідь про
    // світ, якого вже немає.
    static bool Invalidate(string kind, string why)
    {
        if (kind == "")
            return false;

        if (!OZ_BridgeClient.StaleRoutes(kind, s_Stale))
            return false;

        if (s_Stale.Count() == 0)
        {
            OZ_Log.Dbg("bridge cache: \"" + kind + "\" declares no stale routes (" + why + ")");
            return false;
        }

        if (s_Body.Count() == 0)
            return true;

        s_Doomed.Clear();
        for (int i = 0; i < s_Body.Count(); i++)
        {
            string k = s_Body.GetKey(i);
            for (int r = 0; r < s_Stale.Count(); r++)
            {
                // Ключ -- "дорога|лист", тож порівнюємо з дорогою плюс риска:
                // інакше "v1/chat/list" збігся б і з "v1/chat/listen".
                if (k.IndexOf(s_Stale[r] + "|") == 0)
                {
                    s_Doomed.Insert(k);
                    break;
                }
            }
        }

        if (s_Doomed.Count() == 0)
            return true;

        for (int j = 0; j < s_Doomed.Count(); j++)
        {
            s_Body.Remove(s_Doomed[j]);
            s_At.Remove(s_Doomed[j]);
        }

        OZ_Log.Dbg("bridge cache: dropped " + s_Doomed.Count().ToString() + " of " + kind + " (" + why + ")");
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
