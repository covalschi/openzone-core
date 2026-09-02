// Реєстр АДМІНСЬКИХ РОЗДІЛІВ -- окремий від реєстру сторінок.
//
// Сторінка й адмінський розділ -- різні речі, і плутати їх коштувало нам усієї
// вкладки VPP. Сторінка живе на ПРИСТРОЇ: її роздає профіль КПК, і гейт
// OZ_PageAccess питає «чи є вона в того приладу, що в руках». Адмінський
// розділ не живе ніде: адміну не потрібен ані прилад, ані профіль, ані екран,
// щоб правити конфіг чи стерти персонажа.
//
// Поки розділи стояли в реєстрі сторінок, у цьому й був увесь клубок: щоб
// пропустити «admin», гейт КПК тримав виняток першим рядком -- тобто в межі
// безпеки була двері збоку, -- а розділ фракцій, який винятку не мав, не
// проходив узагалі й мовчав. Рішення власника 2026-09-01: «КПК взагалі не
// повинен мати адмінських функцій, усі адмінські функції у VPP».
//
// Тому в розділів свій реєстр, свій конверт (OZ_AdminReq/OZ_AdminRes) і одні
// ворота без винятків -- OZ_Perm.IsAdmin на сервері, першим рядком диспетчера.

class OZ_AdminSection
{
    // Повертає JSON відповіді. error -- КЛЮЧ стрінгтейбла, не готове речення:
    // текст складає клієнт, бо тільки він знає мову адміна.
    //
    // ПРАВА ТУТ НЕ ПЕРЕВІРЯЮТЬСЯ. Їх перевірив диспетчер, до розбору операції
    // й до будь-якого доступу до даних. Розділ, що перевіряє їх ще раз,
    // виглядає обережним, але створює друге місце, де це правило можна
    // забути -- а межа безпеки мусить бути одна.
    string Handle(string op, string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;
        error = "STR_OZ_ERR_UNKNOWN_OP";
        return "";
    }
}

class OZ_AdminRegistry
{
    private static ref map<string, ref OZ_AdminSection> s_Sections;

    private static void Ensure()
    {
        if (!s_Sections)
            s_Sections = new map<string, ref OZ_AdminSection>();
    }

    // ПЕРШИЙ ВИГРАЄ, і про другого гучно кажемо (правило серії, ТЗ-5 §C1 R1).
    // Чужий мод не може тихо підмінити наш розділ.
    static void Register(string sectionId, OZ_AdminSection section)
    {
        Ensure();

        if (s_Sections.Contains(sectionId))
        {
            OZ_Log.Warn("admin section \"" + sectionId + "\" registered twice, the second registration is ignored");
            return;
        }

        s_Sections.Insert(sectionId, section);
        OZ_Log.Dbg("admin section registered: " + sectionId);
    }

    static bool Has(string sectionId)
    {
        Ensure();
        return s_Sections.Contains(sectionId);
    }

    static int Count()
    {
        Ensure();
        return s_Sections.Count();
    }

    static OZ_AdminSection Get(string sectionId)
    {
        Ensure();
        return s_Sections.Get(sectionId);
    }

    // Перелік розділів РЯДКОМ -- для лога старту. Адмін мусить бачити, що
    // саме відповість йому консоль, не відкриваючи її.
    static string Describe()
    {
        Ensure();

        string line = "";
        for (int i = 0; i < s_Sections.Count(); i++)
        {
            if (line != "")
                line += ",";
            line += s_Sections.GetKey(i);
        }

        if (line == "")
            return "none";
        return line;
    }
}

// Імена розділів. Ядрові -- тут; свої оголошує той мод, який їх приносить.
class OZ_AdminSect
{
    static const string CONFIG = "config";
    static const string SPAWNS = "spawns";
    static const string NEWS   = "news";
}

// Операції розділу NEWS: одне написання для сервера (OZ_NewsSection) і для
// панелі VPP. Рядок, набраний двічі в двох pbo, розходиться мовчки.
class OZ_NewsOp
{
    static const string VOICES = "news_voices";
    static const string POST   = "news_post";
}
