// Реальний час, а не час від старту сервера.
//
// GetGame().GetTime() рахує мілісекунди від запуску місії: після рестарту він
// починається з нуля, і збережений у файлі «FirstSeen» став би меншим за
// пізніший «LastSeen». Для того, що переживає рестарт, потрібен календар.

class OZ_Time
{
    // Рядок ISO-подібного вигляду YYYY-MM-DD HH:MM:SS у UTC.
    // Зберігаємо саме рядком: він читається людиною в JSON без розшифровки,
    // а сортується лексикографічно так само, як хронологічно.
    static string NowUtc()
    {
        int y, mo, d, h, mi, s;
        GetYearMonthDayUTC(y, mo, d);
        GetHourMinuteSecondUTC(h, mi, s);

        // out -- зарезервоване слово Enforce (out-параметри), змінною так не назвати.
        string res = y.ToString();
        res += "-" + Pad2(mo);
        res += "-" + Pad2(d);
        res += " " + Pad2(h);
        res += ":" + Pad2(mi);
        res += ":" + Pad2(s);
        return res;
    }

    // Той самий календар, зсунутий уперед на стільки секунд. Для строків, які
    // мусять пережити рестарт: час рушія для них не годиться, бо після
    // рестарту він починається з нуля, і все, що на нього спиралось,
    // виглядало б або протухлим, або вічним.
    static string InUtc(int seconds)
    {
        int y, mo, d, h, mi, s;
        GetYearMonthDayUTC(y, mo, d);
        GetHourMinuteSecondUTC(h, mi, s);

        // Носимо додавання вгору по розрядах. Днів у місяці рівно стільки,
        // скільки їх є: строк у кілька хвилин через межу місяця інакше
        // з'їхав би на добу.
        s += seconds;

        mi += s / 60;
        s   = s % 60;

        h  += mi / 60;
        mi  = mi % 60;

        d  += h / 24;
        h   = h % 24;

        while (d > DaysIn(y, mo))
        {
            d -= DaysIn(y, mo);
            mo++;
            if (mo > 12)
            {
                mo = 1;
                y++;
            }
        }

        string res = y.ToString();
        res += "-" + Pad2(mo);
        res += "-" + Pad2(d);
        res += " " + Pad2(h);
        res += ":" + Pad2(mi);
        res += ":" + Pad2(s);
        return res;
    }

    // Чи момент `a` настав раніше за `b`. Обидва -- рядки цього ж класу.
    //
    // Порівнюємо ЦИФРИ, а не рядки: у Enforce «менше» для string не
    // визначене взагалі. Формат фіксованої ширини з нулями попереду, тож
    // після викидання роздільників лишаються рівно чотирнадцять цифр, і
    // посимвольне порівняння збігається з хронологічним.
    static bool Before(string a, string b)
    {
        string da = Digits(a);
        string db = Digits(b);

        if (da.Length() != db.Length())
            return da.Length() < db.Length();

        for (int i = 0; i < da.Length(); i++)
        {
            int va = "0123456789".IndexOf(da.Get(i));
            int vb = "0123456789".IndexOf(db.Get(i));
            if (va != vb)
                return va < vb;
        }

        return false;
    }

    private static string Digits(string s)
    {
        string res = "";
        for (int i = 0; i < s.Length(); i++)
        {
            string c = s.Get(i);
            if ("0123456789".IndexOf(c) != -1)
                res += c;
        }
        return res;
    }

    private static int DaysIn(int year, int month)
    {
        if (month == 2)
        {
            if (year % 400 == 0)
                return 29;
            if (year % 100 == 0)
                return 28;
            if (year % 4 == 0)
                return 29;
            return 28;
        }

        if (month == 4 || month == 6 || month == 9 || month == 11)
            return 30;

        return 31;
    }

    private static string Pad2(int v)
    {
        if (v < 10)
            return "0" + v.ToString();
        return v.ToString();
    }
}
