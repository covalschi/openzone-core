// Реєстр сторінок -- єдина точка розширення фреймворку.
//
// Наші власні підсистеми (карта, фракції, чат, рація) стають у нього так само,
// як став би чужий мод. Окремої машинерії «для сторонніх» тут немає навмисно:
// точка розширення, якою не користується сам автор, гниє непоміченою.

class OZ_PageHandler
{
    // Повертає JSON відповіді. error -- КЛЮЧ стрінгтейбла, не готове речення:
    // текст складає клієнт, бо тільки він знає мову гравця.
    string Handle(string op, string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;
        error = "STR_OZ_ERR_UNKNOWN_OP";
        return "";
    }
}

class OZ_PageEntry
{
    string PageId;
    string TitleKey;
    string Icon;
    ref OZ_PageHandler Handler;
}

class OZ_PageRegistry
{
    private static ref map<string, ref OZ_PageEntry> s_Pages;

    private static void Ensure()
    {
        if (!s_Pages)
            s_Pages = new map<string, ref OZ_PageEntry>();
    }

    // Кличеться один раз, з OnMissionStart модуля-власника сторінки.
    // handler створює викликач: спроба зробити це з typename тут зайва --
    // власник і так знає свій конкретний тип.
    static void Register(string pageId, string titleKey, string icon, OZ_PageHandler handler)
    {
        Ensure();

        if (s_Pages.Contains(pageId))
        {
            OZ_Log.Warn("page \"" + pageId + "\" registered twice, the second registration is ignored");
            return;
        }

        OZ_PageEntry e = new OZ_PageEntry();
        e.PageId   = pageId;
        e.TitleKey = titleKey;
        e.Icon     = icon;
        e.Handler  = handler;

        s_Pages.Insert(pageId, e);
        OZ_Log.Dbg("page registered: " + pageId);
    }

    static bool Has(string pageId)
    {
        Ensure();
        return s_Pages.Contains(pageId);
    }

    static int Count()
    {
        Ensure();
        return s_Pages.Count();
    }

    static OZ_PageEntry Get(string pageId)
    {
        Ensure();
        return s_Pages.Get(pageId);
    }

    static void FillPayload(OZ_SyncPayload p)
    {
        Ensure();

        for (int i = 0; i < s_Pages.Count(); i++)
        {
            OZ_PageEntry e = s_Pages.GetElement(i);

            OZ_SyncPageInfo info = new OZ_SyncPageInfo();
            info.PageId   = e.PageId;
            info.TitleKey = e.TitleKey;
            info.Icon     = e.Icon;

            p.Pages.Insert(info);
        }
    }
}

// Чи дозволена сторінка ЦЬОМУ гравцеві.
//
// Ядро саме пристроїв не має, тому тут пускає все. КПК підмінює це через
// modded class перевіркою профілю свого пристрою -- так ядро лишається
// нічого не знати про КПК, а перевірка все одно є.
class OZ_PageAccess
{
    static bool Allowed(PlayerIdentity who, string pageId)
    {
        return true;
    }
}
