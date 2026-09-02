// Розширення пакета синхронізації для модів (ТЗ-5 R-C1.3, D87).
//
// СЛУЖБА ЯДРА, а не знання про моди: ядро не має жодного уявлення, що таке
// «тост» чи «маршрут». Воно тримає список тих, хто хоче щось докласти до
// пакета, кличе їх перед відправкою й везе результат. Той самий візерунок,
// що й у OZ_PageRegistry: мод реєструє себе, ядро лише роздає.
//
// Сервер: OZ_SyncExtras.OnFill().Insert(MyFill) у OnMissionStart, а MyFill
// кладе пари через Put(). Клієнт: OZ_ClientState.Extra(key, fallback) --
// і OZ_ClientState.SyncWatch(), щоб дізнатись, коли пакет приїхав знову.
//
// Ключі -- з префіксом мода ("pda.toast_s"): простір один на всіх, і два моди
// з однаковим ключем перезаписали б одне одного мовчки.

class OZ_SyncExtras
{
    private static ref ScriptInvoker s_OnFill;

    // Хто докладає. Викликається з OZ_SyncSender.Send з одним аргументом --
    // самим OZ_SyncPayload.
    static ScriptInvoker OnFill()
    {
        if (!s_OnFill)
            s_OnFill = new ScriptInvoker();
        return s_OnFill;
    }

    // Покласти або перезаписати. Порожній ключ не кладемо: він не знайдеться
    // ніколи, а місце займе.
    static void Put(OZ_SyncPayload p, string key, string value)
    {
        if (!p || !p.Extras || key == "")
            return;

        for (int i = 0; i < p.Extras.Count(); i++)
        {
            if (p.Extras[i].Key == key)
            {
                p.Extras[i].Value = value;
                return;
            }
        }

        OZ_SyncExtra e = new OZ_SyncExtra();
        e.Key   = key;
        e.Value = value;
        p.Extras.Insert(e);
    }

    // Прочитати; за відсутності -- запасне значення, а не порожній рядок:
    // "0" і «немає» -- різні відповіді, і читач мусить їх розрізняти сам.
    static string Of(OZ_SyncPayload p, string key, string fallback)
    {
        if (!p || !p.Extras)
            return fallback;

        for (int i = 0; i < p.Extras.Count(); i++)
        {
            if (p.Extras[i].Key == key)
                return p.Extras[i].Value;
        }
        return fallback;
    }
}
