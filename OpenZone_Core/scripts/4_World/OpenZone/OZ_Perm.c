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
    private static void Probe()
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
            array<string> perms = new array<string>();
            perms.Insert(OZ_Settings.Get().VppPermission);
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

    static void ServerInit()
    {
        Probe();
    }

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

        Probe();

        string uid = identity.GetPlainId();

#ifdef AVPPAdminTools
        if (s_HasVpp)
        {
            // Чотири параметри: (id, permissionName, targetID, sendNotify).
            // targetID -- РЯДОК, не bool; sendNotify обов'язково false,
            // інакше кожна тиха перевірка плювала б гравцеві тост про відмову.
            if (GetPermissionManager().VerifyPermission(uid, OZ_Settings.Get().VppPermission, "", false))
                return true;
        }
#endif

        array<string> ids = OZ_Settings.Get().AdminIds;
        for (int i = 0; i < ids.Count(); i++)
        {
            if (ids[i] == uid)
                return true;
        }

        return false;
    }
}
