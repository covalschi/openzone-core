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

        // Поки гравця немає, показувати нема кому: вікно поверх екрана
        // завантаження рушій однаково зніме.
        if (!GetGame().GetPlayer())
            return;

        UIManager ui = GetGame().GetUIManager();
        if (!ui)
            return;

        if (ui.FindMenu(OZ_LinkConst.MENU_LINK))
            return;

        // Чуже меню поверх нашого id -- реєстру id у рушії немає, і зіткнення
        // непереборне. Відкривати поверх чужого вікна не будемо.
        if (ui.GetMenu())
            return;

        ui.EnterScriptedMenu(OZ_LinkConst.MENU_LINK, null);
    }
}
