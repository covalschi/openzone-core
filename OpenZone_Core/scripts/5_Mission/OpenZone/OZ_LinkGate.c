// Клієнтські ворота: тримають вікно прив'язки відкритим, поки сервер вимагає.
//
// Меню тут НЕ намагається зловити Esc. Рушій закриває меню повз скрипт, і
// перехоплювати це зсередини -- гра в кота-мишку, яку видно по миготінню.
// Замість цього ворота просто відкривають його знову на наступному тіку.
// Виглядає як вікно, яке не закривається, а поводиться передбачувано.
//
// Вимикається рівно двома шляхами: сервер сказав «прив'язаний», або сервер
// сказав, що більше не вимагає. Клієнт сам не вирішує нічого.

class OZ_LinkGate
{
    private static bool s_Required;
    private static bool s_Done;
    private static OZ_LinkMenu s_Menu;

    // Приїхав конверт синхронізації. Єдине місце, де ворота вмикаються.
    static void FromSync(bool linked, bool required)
    {
        s_Required = required;
        s_Done     = linked;
    }

    // Прив'язались. Більше не відкриваємо.
    static void Satisfied()
    {
        s_Done = true;
    }

    static void BindMenu(OZ_LinkMenu m)
    {
        s_Menu = m;
    }

    static OZ_LinkMenu Menu()
    {
        return s_Menu;
    }

    // Кличеться з MissionGameplay.OnUpdate. Дешевий: два порівняння, поки
    // ворота не потрібні, і FindMenu лише коли потрібні.
    static void Tick()
    {
        if (!s_Required)
            return;
        if (s_Done)
            return;

        UIManager ui = GetGame().GetUIManager();
        if (!ui)
            return;

        // МЕРТВОГО ворота відпускають, і вже відкрите вікно знімають самі.
        //
        // Це не ввічливість, це вихід з падіння: клієнт помирав з
        // ACCESS_VIOLATION рівно тоді, коли смерть заставала це вікно
        // відкритим. Рушій на смерті розбирає місію разом з HUD, а меню в
        // OnHide тягнеться до GetHud() -- і тягнеться вже в порожнечу.
        // Перевірено на стенді: та сама смерть при вимкнених воротах клієнт
        // переживає без жодного рядка в логу.
        //
        // Прив'язку це не послаблює: гравець мертвий, грати йому нічим, а
        // щойно він відродиться -- вікно повернеться наступним же тіком.
        if (!Playing())
        {
            if (ui.FindMenu(OZ_LinkConst.MENU_LINK))
                ui.CloseMenu(OZ_LinkConst.MENU_LINK);
            return;
        }

        if (ui.FindMenu(OZ_LinkConst.MENU_LINK))
            return;

        // Чуже меню поверх нашого id -- реєстру id у рушії немає, і зіткнення
        // непереборне. Відкривати поверх чужого вікна не будемо.
        if (ui.GetMenu())
            return;

        ui.EnterScriptedMenu(OZ_LinkConst.MENU_LINK, null);
    }

    // Чи є зараз кому й коли показувати вікно. Три різні «ні», і жодне з них
    // не зводиться до інших: гравця ще немає, гравець є але мертвий, гравець
    // живий але рушій саме його перестворює.
    private static bool Playing()
    {
        PlayerBase p = PlayerBase.Cast(GetGame().GetPlayer());
        if (!p)
            return false;
        if (!p.IsAlive())
            return false;

        // Той самий вартовий, яким рушій боронить власне меню паузи
        // (missiongameplay.c:1276): персонаж уже існує, але ще не готовий.
        if (!p.IsPlayerLoaded())
            return false;

        Mission m = GetGame().GetMission();
        if (!m)
            return false;
        if (m.IsPlayerRespawning())
            return false;

        return true;
    }
}
