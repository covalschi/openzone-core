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
// Ядро саме пристроїв не має й вирішити це не може. Тому тут -- база, яку
// заміщає той, хто пристрої приносить: КПК кладе сюди свого нащадка, і той
// звіряє сторінку з профілем пристрою в руках.
//
// Чому нащадок, а не modded class зі static: перевизначати статичний метод
// через modded class -- хисткий трюк, а тут через нього проходить КОЖЕН
// запит клієнта. Механізм, що тримає межу безпеки, має бути нудним.
//
// Без провайдера пускає все: ядро без КПК не має чого забороняти.
class OZ_PageAccess
{
    private static ref OZ_PageAccess s_Provider;

    static void Bind(OZ_PageAccess provider)
    {
        s_Provider = provider;
    }

    // ОПЕРАЦІЯ входить у питання, а не лише сторінка.
    //
    // Інакше виходить зачароване коло: замкнений пристрій не пускає на
    // сторінку, а відімкнути його можна ЛИШЕ операцією на тій самій
    // сторінці -- і код нема куди ввести. Саме це й було на живому клієнті:
    // правильний пін відповідав «wrong code», бо до перевірки піна справа
    // не доходила зовсім.
    static bool Allowed(PlayerIdentity who, string pageId, string op)
    {
        string ignored;
        return Allowed(who, pageId, op, ignored);
    }

    // Те саме, але з ПРИЧИНОЮ.
    //
    // Гейт відмовляє з різних міркувань -- сторінки немає в профілі, модуль не
    // вставлений, пристрій замкнений, пристрій вимкнений, -- а гравець бачив
    // одне й те саме «на цьому пристрої такого екрана немає». Для вимкненого
    // приладу це просто неправда: екран є, живлення немає, і людина шукає
    // модуль замість того, щоб натиснути «увімкнути».
    static bool Allowed(PlayerIdentity who, string pageId, string op, out string why)
    {
        why = "STR_OZ_ERR_NO_ACCESS";

        if (!s_Provider)
            return true;
        return s_Provider.Check(who, pageId, op, why);
    }

    // Перевизначає нащадок. `why` уже містить загальну причину -- міняти її
    // треба лише там, де є конкретніша.
    bool Check(PlayerIdentity who, string pageId, string op, out string why)
    {
        return true;
    }
}
