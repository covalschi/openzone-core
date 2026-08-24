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

    private static string Pad2(int v)
    {
        if (v < 10)
            return "0" + v.ToString();
        return v.ToString();
    }
}
