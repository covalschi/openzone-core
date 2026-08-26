// Ворота прив'язки: вікно, яке не відпускає, поки акаунт не прив'язаний.
//
// Живе в ЯДРІ й не знає нічого про КПК. Прив'язатись мусить кожен, а КПК є не
// в кожного -- і рації, квестам та ІІ фракція з Discord потрібна так само.
//
// Як воно тримає. Меню, яке рушій закрив, повертається на наступному кадрі
// місії: OZ_LinkGate.Tick відкриває його знову, поки сервер каже «не
// прив'язаний». Тому тут НЕМАЄ кнопки закриття й не перекривається OnClick
// для неї -- ловити Esc усередині меню марно, рушій закриває його повз нас.
// Єдиний спосіб вийти -- прив'язатись, або щоб сервер сказав, що більше не
// вимагає.

class OZ_LinkMenu : UIScriptedMenu
{
    private ButtonWidget m_BtnGet;
    private string m_Code;
    private bool m_Asking;

    override Widget Init()
    {
        layoutRoot = GetGame().GetWorkspace().CreateWidgets("OpenZone_Core/gui/layouts/oz_link.layout");
        if (!layoutRoot)
        {
            OZ_Log.Error("link gate layout failed to load");
            return null;
        }

        m_BtnGet = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnGet"));
        return layoutRoot;
    }

    override void OnShow()
    {
        super.OnShow();

        if (!GetLayoutRoot())
        {
            GetGame().GetUIManager().CloseMenu(OZ_LinkConst.MENU_LINK);
            return;
        }

        SetFocus(layoutRoot);

        // Місію й HUD БЕРЕМО ЧЕРЕЗ ЗМІННУ й перевіряємо обидва.
        //
        // Ланцюжок GetMission().GetHud().Show() без перевірок падає з
        // ACCESS_VIOLATION, і падає не тут, а на смерті: рушій розбирає HUD
        // раніше, ніж закриває наші меню. Один прогін стенду пішов на те, щоб
        // це побачити, і другий -- щоб довести, що з вимкненими воротами та
        // сама смерть проходить чисто.
        Mission m = GetGame().GetMission();
        if (m)
        {
            array<string> excludes = new array<string>();
            excludes.Insert("menu");
            m.AddActiveInputExcludes(excludes);

            Hud hud = m.GetHud();
            if (hud)
                hud.Show(false);
        }

        OZ_LinkGate.BindMenu(this);

        Text("Title", "#STR_OZ_GATE_TITLE");
        Text("Why",   "#STR_OZ_GATE_WHY");
        Paint();

        // Питаємо стан одразу: гравець міг прив'язатись з іншого пристрою,
        // поки це вікно вантажилось.
        OZ_Rpc.LinkRequest(OZ_LinkConst.OP_STATE);
    }

    override void OnHide()
    {
        super.OnHide();

        OZ_LinkGate.BindMenu(null);

        Mission m = GetGame().GetMission();
        if (!m)
            return;

        // Знімаємо блокування ввода ТИМ САМИМ викликом, яким його знімає сам
        // рушій у Continue() (missiongameplay.c:1307). ResetGUI цього не
        // робить, тож без цього рядка гравець після воріт лишався б без
        // керування -- вікна вже немає, а рухатись не можна.
        array<string> excludes = new array<string>();
        excludes.Insert("menu");
        m.RemoveActiveInputExcludes(excludes, true);

        Hud hud = m.GetHud();
        if (hud)
            hud.Show(true);

        m.ResetGUI();
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (w && w == m_BtnGet)
        {
            m_Asking = true;
            Paint();
            OZ_Rpc.LinkRequest(OZ_LinkConst.OP_BEGIN);
            return true;
        }
        return super.OnClick(w, x, y, button);
    }

    // Сервер відповів. Малюємо те, що сказали, і нічого не вигадуємо самі.
    void OnLinkResponse(string op, bool ok, string json, string error)
    {
        if (op == OZ_LinkConst.OP_BEGIN)
        {
            m_Asking = false;

            if (!ok)
            {
                Text("Hint", "#" + error);
                return;
            }

            OZ_LinkGrant g;
            string err;
            if (JsonFileLoader<OZ_LinkGrant>.LoadData(json, g, err) && g && g.Code != "")
                m_Code = g.Code;

            Paint();
            return;
        }

        if (op != OZ_LinkConst.OP_STATE)
            return;

        OZ_LinkState st;
        string serr;
        if (!JsonFileLoader<OZ_LinkState>.LoadData(json, st, serr) || !st)
            return;

        if (st.Linked)
        {
            // Єдиний вихід звідси.
            OZ_LinkGate.Satisfied();
            GetGame().GetUIManager().CloseMenu(OZ_LinkConst.MENU_LINK);
        }
    }

    private void Paint()
    {
        if (m_Code != "")
        {
            string line = "/link " + m_Code;
            Text("Code", line);
            Text("Hint", "#STR_OZ_GATE_WAITING");
            if (m_BtnGet)
                m_BtnGet.Show(false);
            return;
        }

        if (m_Asking)
        {
            Text("Code", "");
            Text("Hint", "#STR_OZ_LINK_ASKING");
            return;
        }

        Text("Code", "");
        Text("Hint", "");
        if (m_BtnGet)
            m_BtnGet.Show(true);
        Text("BtnGetText", "#STR_OZ_GATE_GET");
    }

    private void Text(string name, string value)
    {
        if (!layoutRoot)
            return;
        TextWidget w = TextWidget.Cast(layoutRoot.FindAnyWidget(name));
        if (w)
            w.SetText(value);
    }
}
