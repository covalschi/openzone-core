// Права: VPP AdminTools, якщо він є, інакше список Steam64 у Settings.json.
//
// Обидва шляхи рівноправні. Вимагати від кожного сервера адмінський тулкіт
// заради одного мода -- зайве, а не мати прав узагалі -- небезпечно.
//
// ГОЛОВНЕ: перевірка ЗАВЖДИ серверна, у обробнику кожної операції. Клієнту
// їде лише підсумковий прапорець, і тільки щоб ховати кнопки. Клієнт, який
// сам вирішує, що він адмін, -- це не перевірка, а побажання.

class OZ_Perm
{
    private static bool s_Probed  = false;
    private static bool s_HasVpp  = false;

    // Дефайна VPPADMINTOOLS не існує. Рушій авто-дефайнить імена класів
    // CfgMods, а VPP оголошує себе як AVPPAdminTools -- перевірено в лозі
    // бута: у списку defines стоять саме OpenZone_Core і OPENZONE_CORE,
    // тобто ім'я класу CfgPatches і те, що в defines[].
    //
    // НЕ private: ліниву пробу кличуть усі точки входу прав, і старт ядра
    // теж -- щоб рядок про джерело прав стояв у лозі бута, а не з'явився
    // при першому натисканні адміна. Ідемпотентна.
    static void Probe()
    {
        if (s_Probed)
            return;
        s_Probed = true;

#ifdef AVPPAdminTools
        // Мод може бути в модпаку, але вимкнений -- тому ще й рантайм-проба,
        // а не сама лише умовна компіляція.
        s_HasVpp = (GetPermissionManager() != null);

        if (s_HasVpp)
        {
            // Незареєстроване право VPP відхиляє ЗАВЖДИ, хоч би хто його
            // питав (permissionmanager.c:715). Без цього рядка вся гілка VPP
            // тихо відповідала б «ні» кожному.
            //
            // "OZ_VppAdminMenu" -- право КНОПКИ вкладки OpenZone: перший
            // аргумент InsertButton у VPP -- одночасно iм'я права i класу
            // підменю. Реєструє СЕРВЕР, бо клієнтський pbo вкладки на
            // сервері може бути взагалі не завантажений, а кнопка без
            // серверного права мертва навіть для супер-адміна.
            array<string> perms = new array<string>();
            perms.Insert(OZ_Settings.Get().VppPermission);
            perms.Insert("OZ_VppAdminMenu");
            GetPermissionManager().AddPermissionType(perms);
        }
#endif

        if (s_HasVpp)
        {
            string on = "permissions: VPP present, using permission ";
            on += OZ_Settings.Get().VppPermission;
            OZ_Log.Info(on);
        }
        else
        {
            // += приймає лише рядок: string + int працює, а string += int --
            // ні. Тому явний ToString().
            string off = "permissions: VPP absent, falling back to AdminIds (";
            off += OZ_Settings.Get().AdminIds.Count().ToString();
            off += " entries)";
            OZ_Log.Info(off);
        }
    }

    // ServerInit() ТУТ БІЛЬШЕ НЕМАЄ: обгортка в один рядок навколо Probe(),
    // з одним викликачем, при тому що Probe лінивий і його кличе кожна точка
    // входу прав.

    static string Describe()
    {
        Probe();
        if (s_HasVpp)
            return "vpp";
        return "adminids";
    }

    static bool IsAdmin(PlayerIdentity identity)
    {
        if (!identity)
            return false;

        return IsAdminUid(identity.GetPlainId());
    }

    // ТЕ САМЕ ПИТАННЯ, АЛЕ БЕЗ ОСОБИ.
    //
    // Права й так рахуються по рядку: VerifyPermission бере uid рядком, а
    // AdminIds -- це список рядків. Особа потрібна була лише для того, щоб
    // дістати з неї той самий uid. Ворота прив'язки (OZ_Link.Gated) знають
    // саме uid і питають про людину, яка може бути зараз офлайн.
    static bool IsAdminUid(string uid)
    {
        if (uid == "")
            return false;

        Probe();

        OZ_Settings s = OZ_Settings.Get();
        if (!s)
            return false;

#ifdef AVPPAdminTools
        if (s_HasVpp)
        {
            // Чотири параметри: (id, permissionName, targetID, sendNotify).
            // targetID -- РЯДОК, не bool; sendNotify обов'язково false,
            // інакше кожна тиха перевірка плювала б гравцеві тост про відмову.
            if (GetPermissionManager().VerifyPermission(uid, s.VppPermission, "", false))
                return true;
        }
#endif

        array<string> ids = s.AdminIds;
        if (!ids)
            return false;

        for (int i = 0; i < ids.Count(); i++)
        {
            if (ids[i] == uid)
                return true;
        }

        return false;
    }
}
