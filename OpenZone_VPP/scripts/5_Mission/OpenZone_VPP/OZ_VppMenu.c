// Вкладка «OpenZone» у VPP Admin Tools, друга редакцiя: форми, живий
// пошук, окремi роздiли. Субмод КПК доклада свою вкладку через modded
// class -- точки розширення позначенi словом protected.
//
// Клiєнтський UI i НIЧОГО бiльше: кожна дiя їде на сервер каналами ядра
// (OZ_Rpc.Request на сторiнку "admin", OZ_Rpc.RoleRequest на ролi та
// спавни), i кожну сервер перевiряє САМ (OZ_Perm.IsAdmin). Класи конфiгiв
// виднi клiєнтовi, тому форма працює цiлком тут: cfg_get -> LoadData ->
// поля -> MakeData -> cfg_set; жодних нових серверних операцiй.
//
// Пастки VPP (змiряно в zp-research, повторено i доповнено тут):
//  - обидва гарди обов'язковi (NO_GUI валить сервер, AVPPAdminTools -- то
//    iм'я класу CfgMods, не CfgPatches);
//  - super.DefineButtons(), iнакше зникають рiднi кнопки;
//  - перший аргумент InsertButton = iм'я права I класу пiдменю; право
//    реєструє сервер ядра (OZ_Perm);
//  - вiджет ЖИВЕ В БАЗОВОМУ M_SUB_WIDGET: базовi Show/Hide/OnUpdate
//    працюють з ним, власне поле лишає базi NULL i диаг-збiрка висне;
//  - вкладенi лапки в text-значеннi розмiтки вiшають парсер НАМЕРТВО
//    (бiсекцiя 2026-08-30);
//  - сорт свiй: HideBrokenWidgets приходить лише при ЗМIНI порядку.

#ifdef AVPPAdminTools
#ifndef NO_GUI

modded class VPPAdminHud
{
    override void DefineButtons()
    {
        super.DefineButtons();
        InsertButton("OZ_VppAdminMenu", "OpenZone", "set:dayz_gui_vpp image:vpp_icon_xml_editor", "OpenZone: factions, spawns, configs");
    }
}

class OZ_VppAdminMenu : AdminHudSubMenu
{
    static OZ_VppAdminMenu s_Inst;

    // ------------------------------------------------- точки розширення
    protected ref array<string> m_TabIds;
    protected ref array<Widget> m_TabBtns;
    protected ref map<string, Widget> m_Panes;

    // Перелiк редагованих конфiгiв з сервера: iм'я + власник (core/pda).
    protected ref array<string> m_CfgNames;
    protected ref array<string> m_CfgOwners;

    // ЧЕРГА cfg_get: рiвно один запит у польотi. Довгi вiдповiдi їдуть
    // частинами з ключем «сторiнка|операцiя», i два одночасних cfg_get
    // склеюють свої частини в одну кашу (змiряно 2026-08-30: Factions на
    // три чанки не розбирався, поки поруч летiв другий запит).
    protected ref array<string> m_CfgQ;
    protected bool m_CfgBusy = false;

    protected bool m_Ears = false;

    // ------------------------------------------------- фракцiї
    protected ref OZ_FactionsConfig m_FacCfg;
    protected ref array<int> m_FacRowIdx;   // рядок списку -> iндекс у конфiзi
    protected int  m_FacPickedIdx = -1;
    protected bool m_FacNewMode = false;
    protected bool m_FrmJoinable = true;
    protected bool m_FrmHidden = false;
    protected bool m_DelArmed = false;
    protected ref array<string> m_RosterNames;
    protected int m_RosterPicked = -1;

    // ------------------------------------------------- спавни
    protected ref OZ_SpawnsConfig m_SpawnsCfg;
    protected int m_SpFacAt = 0;

    // ------------------------------------------------- raw
    protected string m_RawPicked = "";
    protected ref array<string> m_RawRows;

    override void OnCreate(Widget RootW)
    {
        super.OnCreate(RootW);
        s_Inst = this;

        M_SUB_WIDGET = GetGame().GetWorkspace().CreateWidgets("OpenZone_VPP/gui/layouts/oz_vpp_admin.layout");
        if (!M_SUB_WIDGET)
        {
            OZ_Log.Error("vpp tab: layout failed to load");
            return;
        }

        M_SUB_WIDGET.SetHandler(this);
        M_SUB_WIDGET.SetSort(1000);
        M_SUB_WIDGET.Show(true);

        m_TitlePanel  = null;
        m_closeButton = ButtonWidget.Cast(M_SUB_WIDGET.FindAnyWidget("BtnVppClose"));

        m_TabIds  = new array<string>();
        m_TabBtns = new array<Widget>();
        m_Panes   = new map<string, Widget>();
        m_CfgNames  = new array<string>();
        m_CfgOwners = new array<string>();
        m_CfgQ      = new array<string>();
        m_FacRowIdx = new array<int>();
        m_RosterNames = new array<string>();
        m_RawRows = new array<string>();

        RegisterPane("factions", "FACTIONS", M_SUB_WIDGET.FindAnyWidget("PaneFactions"));
        RegisterPane("spawns",   "SPAWNS",   M_SUB_WIDGET.FindAnyWidget("PaneSpawns"));
        RegisterPane("raw",      "RAW JSON", M_SUB_WIDGET.FindAnyWidget("PaneRaw"));

        if (!m_Ears)
        {
            m_Ears = true;
            OZ_ClientState.ResponseWatch().Insert(this.OnAdminResponse);
            OZ_RoleNotice.OnAnswer.Insert(this.OnRoleAnswer);
        }

        // Панель покаже OnMenuShow: ShowSubMenu приходить одразу пiсля
        // OnCreate, а подвiйний показ подвоював би cfg_get.
    }

    void ~OZ_VppAdminMenu()
    {
        if (s_Inst == this)
            s_Inst = null;

        if (m_Ears)
        {
            OZ_ClientState.ResponseWatch().Remove(this.OnAdminResponse);
            OZ_RoleNotice.OnAnswer.Remove(this.OnRoleAnswer);
        }

        if (M_SUB_WIDGET)
            M_SUB_WIDGET.Unlink();
    }

    // Вкладка: кнопка з окремої розмiтки, панель -- вiд того, хто реєструє.
    // Субмод КПК кличе це саме з modded OnCreate.
    protected void RegisterPane(string id, string label, Widget pane)
    {
        if (!pane)
            return;

        Widget btn = GetGame().GetWorkspace().CreateWidgets("OpenZone_VPP/gui/layouts/oz_vpp_tab.layout", M_SUB_WIDGET);
        if (!btn)
            return;

        btn.SetPos(20 + m_TabIds.Count() * 160, 52);
        btn.SetName("tab:" + id);

        TextWidget t = TextWidget.Cast(btn.FindAnyWidget("OZ_VppTabText"));
        if (t)
            t.SetText(label);

        m_TabIds.Insert(id);
        m_TabBtns.Insert(btn);
        m_Panes.Set(id, pane);
    }

    protected void ShowPane(string id)
    {
        for (int i = 0; i < m_TabIds.Count(); i++)
        {
            Widget pane = m_Panes.Get(m_TabIds[i]);
            if (pane)
                pane.Show(m_TabIds[i] == id);

            TextWidget t = TextWidget.Cast(m_TabBtns[i].FindAnyWidget("OZ_VppTabText"));
            if (t)
            {
                if (m_TabIds[i] == id)
                    t.SetColor(ARGB(255, 255, 122, 26));
                else
                    t.SetColor(ARGB(255, 138, 143, 153));
            }
        }

        OnPaneShown(id);
    }

    // Панель показано -- свiжi данi. Субмод КПК довантажує своє тут.
    protected void OnPaneShown(string id)
    {
        if (id == "factions")
        {
            AskCfg("Factions");
            Ask("roster", "{}");
        }
        if (id == "spawns")
        {
            AskCfg("Spawns");
            if (!m_FacCfg)
                AskCfg("Factions");
        }
        if (id == "raw")
            Ask("cfg_list", "{}");
    }

    override void OnMenuShow()
    {
        super.OnMenuShow();
        if (M_SUB_WIDGET)
            M_SUB_WIDGET.SetSort(1000);
        Ask("cfg_list", "{}");
        ShowPane("factions");
    }

    override void HideBrokenWidgets(bool state)
    {
        super.HideBrokenWidgets(state);
        if (!M_SUB_WIDGET)
            return;
        if (state)
            M_SUB_WIDGET.SetSort(10);
        else
            M_SUB_WIDGET.SetSort(1000);
    }

    bool IsOpen()
    {
        return M_SUB_WIDGET && M_SUB_WIDGET.IsVisible();
    }

    void ForceHide()
    {
        if (!M_SUB_WIDGET || !M_SUB_WIDGET.IsVisible())
            return;
        M_SUB_WIDGET.Show(false);
        m_IsVisible = false;
    }

    // ---------------------------------------------------------- транспорт

    protected void Ask(string op, string json)
    {
        OZ_Rpc.Request(OZ_Const.PAGE_ADMIN, op, json);
    }

    protected void AskCfg(string name)
    {
        if (m_CfgQ.Find(name) != -1)
            return;
        m_CfgQ.Insert(name);
        PumpCfg();
    }

    protected void PumpCfg()
    {
        if (m_CfgBusy || m_CfgQ.Count() == 0)
            return;

        m_CfgBusy = true;

        OZ_AdminAsk a = new OZ_AdminAsk();
        a.Name = m_CfgQ[0];
        string json;
        string err;
        if (JsonFileLoader<OZ_AdminAsk>.MakeData(a, json, err, false))
            Ask("cfg_get", json);
        else
            m_CfgBusy = false;
    }

    protected void CfgDone(string name)
    {
        if (m_CfgQ.Count() > 0 && m_CfgQ[0] == name)
            m_CfgQ.Remove(0);
        m_CfgBusy = false;
        PumpCfg();
    }

    protected void SendCfg(string name, string body)
    {
        OZ_AdminAsk a = new OZ_AdminAsk();
        a.Name = name;
        a.Json = body;
        string json;
        string err;
        if (JsonFileLoader<OZ_AdminAsk>.MakeData(a, json, err, false))
            Ask("cfg_set", json);
    }

    protected void Hint(string t)
    {
        TextWidget h;
        Widget p = m_Panes.Get(CurrentPane());
        if (p)
        {
            h = TextWidget.Cast(p.FindAnyWidget("FacHint"));
            if (!h)
                h = TextWidget.Cast(p.FindAnyWidget("SpawnHint"));
            if (!h)
                h = TextWidget.Cast(p.FindAnyWidget("RawHint"));
            if (!h)
                h = TextWidget.Cast(p.FindAnyWidget("PdaHint"));
        }
        if (h)
            h.SetText(t);
    }

    protected string CurrentPane()
    {
        for (int i = 0; i < m_TabIds.Count(); i++)
        {
            Widget pane = m_Panes.Get(m_TabIds[i]);
            if (pane && pane.IsVisible())
                return m_TabIds[i];
        }
        return "";
    }

    // ---------------------------------------------------------- вiдповiдi

    void OnAdminResponse(string pageId, string op, bool ok, string json, string error)
    {
        if (pageId != OZ_Const.PAGE_ADMIN)
            return;

        if (!ok)
        {
            // Вiдмова на cfg_get мусить звiльнити чергу, iнакше вона стане.
            if (op == "cfg_get" && m_CfgQ.Count() > 0)
                CfgDone(m_CfgQ[0]);
            Hint("#" + error);
            return;
        }

        if (op == "cfg_list")
        {
            OZ_AdminCfgList l;
            string lerr;
            if (JsonFileLoader<OZ_AdminCfgList>.LoadData(json, l, lerr) && l)
            {
                m_CfgNames.Clear();
                m_CfgOwners.Clear();
                for (int i = 0; i < l.Names.Count(); i++)
                {
                    m_CfgNames.Insert(l.Names[i]);
                    if (l.Owners && i < l.Owners.Count())
                        m_CfgOwners.Insert(l.Owners[i]);
                    else
                        m_CfgOwners.Insert("core");
                }
                OnCfgListChanged();
            }
            return;
        }

        if (op == "cfg_get")
        {
            OZ_AdminAsk a;
            string gerr;
            if (JsonFileLoader<OZ_AdminAsk>.LoadData(json, a, gerr) && a)
            {
                CfgDone(a.Name);
                OnCfgText(a.Name, a.Json);
            }
            else if (m_CfgQ.Count() > 0)
            {
                CfgDone(m_CfgQ[0]);
            }
            return;
        }

        if (op == "cfg_set")
        {
            Hint("applied");
            OnCfgApplied();
            return;
        }

        if (op == "roster")
        {
            OZ_AdminRoster r;
            string rerr;
            if (JsonFileLoader<OZ_AdminRoster>.LoadData(json, r, rerr) && r)
                BuildRoster(r);
            return;
        }
    }

    // Текст конфiгу приїхав. Субмод КПК перехоплює свої iмена через super.
    protected void OnCfgText(string name, string body)
    {
        if (name == "Factions")
        {
            OZ_FactionsConfig fc;
            string err;
            if (JsonFileLoader<OZ_FactionsConfig>.LoadData(body, fc, err) && fc)
            {
                m_FacCfg = fc;
                RebuildFacList();
                PaintSpawnCycler();
            }
            else
                Hint("Factions.json does not parse: " + err);
            return;
        }

        if (name == "Spawns")
        {
            OZ_SpawnsConfig sc;
            string serr;
            if (JsonFileLoader<OZ_SpawnsConfig>.LoadData(body, sc, serr) && sc)
            {
                m_SpawnsCfg = sc;
                RebuildSpawnList();
            }
            return;
        }

        // Iнакше -- сирий редактор.
        if (name == m_RawPicked)
        {
            MultilineEditBoxWidget ed = MultilineEditBoxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("RawEdit"));
            if (ed)
                ed.SetText(body);
            Hint(name + " loaded");
        }
    }

    // Пiсля вдалого cfg_set перечитуємо те, що на екранi.
    protected void OnCfgApplied()
    {
        string cur = CurrentPane();
        if (cur == "factions")
            AskCfg("Factions");
        if (cur == "spawns")
            AskCfg("Spawns");
    }

    protected void OnCfgListChanged()
    {
        RebuildRawList();
    }

    void OnRoleAnswer(string op, bool ok, string why)
    {
        if (!IsOpen())
            return;

        string line = op;
        if (ok)
            line += ": done";
        else
            line += ": " + Widget.TranslateString("#" + why);
        Hint(line);

        if (op == OZ_RoleOp.SPAWN_HERE || op == OZ_RoleOp.SPAWN_CLEAR)
        {
            AskCfg("Spawns");
            return;
        }

        // Ролi їдуть через Discord: перепитуємо ростер трохи згодом.
        GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(this.AskRoster, 1500, false);
    }

    protected void AskRoster()
    {
        if (IsOpen())
            Ask("roster", "{}");
    }

    // ---------------------------------------------------------- фракцiї

    protected void RebuildFacList()
    {
        TextListboxWidget lb = TextListboxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("FacCfgList"));
        if (!lb || !m_FacCfg || !m_FacCfg.Factions)
            return;

        string filter = "";
        EditBoxWidget se = EditBoxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("FacSearch"));
        if (se)
        {
            filter = se.GetText();
            filter.ToLower();
        }

        lb.ClearItems();
        m_FacRowIdx.Clear();

        for (int i = 0; i < m_FacCfg.Factions.Count(); i++)
        {
            OZ_Faction f = m_FacCfg.Factions[i];

            string line = f.Id + "  --  " + f.DisplayName;
            if (f.Hidden)
                line += "  [h]";

            if (filter != "")
            {
                string probe = line;
                probe.ToLower();
                if (probe.IndexOf(filter) == -1)
                    continue;
            }

            int row = lb.AddItem(line, NULL, 0);
            m_FacRowIdx.Insert(i);

            if (i == m_FacPickedIdx)
                lb.SelectRow(row);
        }
    }

    protected void FillFacForm(int idx)
    {
        if (!m_FacCfg || idx < 0 || idx >= m_FacCfg.Factions.Count())
            return;

        m_FacPickedIdx = idx;
        m_FacNewMode = false;
        m_DelArmed = false;
        OZ_Faction f = m_FacCfg.Factions[idx];

        // Iснуючiй фракцiї слаг не редагується: за ним живуть ролi й
        // проекцiї, перейменування -- окрема пiсня.
        TextWidget idT = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("FrmIdText"));
        Widget idE = M_SUB_WIDGET.FindAnyWidget("FrmIdEdit");
        if (idT)
        {
            idT.Show(true);
            idT.SetText(f.Id);
        }
        if (idE)
            idE.Show(false);

        SetEdit("FrmName", f.DisplayName);
        SetEdit("FrmShort", f.Short);
        SetEdit("FrmColor", f.Color);
        SetEdit("FrmMax", f.MaxMembers.ToString());
        m_FrmJoinable = f.Joinable;
        m_FrmHidden   = f.Hidden;
        PaintToggles();
    }

    protected void NewFacForm()
    {
        m_FacPickedIdx = -1;
        m_FacNewMode = true;
        m_DelArmed = false;

        TextWidget idT = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("FrmIdText"));
        Widget idE = M_SUB_WIDGET.FindAnyWidget("FrmIdEdit");
        if (idT)
            idT.Show(false);
        if (idE)
            idE.Show(true);

        SetEdit("FrmIdEdit", "");
        SetEdit("FrmName", "");
        SetEdit("FrmShort", "");
        SetEdit("FrmColor", "200 200 200");
        SetEdit("FrmMax", "0");
        m_FrmJoinable = true;
        m_FrmHidden = false;
        PaintToggles();
        Hint("new faction: fill the form and press SAVE");
    }

    protected void PaintToggles()
    {
        TextWidget j = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("BtnFrmJoinText"));
        if (j)
        {
            if (m_FrmJoinable)
                j.SetText("joinable: yes");
            else
                j.SetText("joinable: no");
        }
        TextWidget h = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("BtnFrmHideText"));
        if (h)
        {
            if (m_FrmHidden)
                h.SetText("hidden: yes");
            else
                h.SetText("hidden: no");
        }
    }

    protected void SaveFaction()
    {
        if (!m_FacCfg)
            return;

        OZ_Faction f;
        if (m_FacNewMode)
        {
            string id = GetEdit("FrmIdEdit");
            id.ToLower();
            if (id == "" || id.IndexOf(" ") != -1)
            {
                Hint("id must be a single lowercase word");
                return;
            }
            for (int i = 0; i < m_FacCfg.Factions.Count(); i++)
            {
                if (m_FacCfg.Factions[i].Id == id)
                {
                    Hint("this id already exists");
                    return;
                }
            }
            f = new OZ_Faction();
            f.Id = id;
            m_FacCfg.Factions.Insert(f);
            m_FacPickedIdx = m_FacCfg.Factions.Count() - 1;
            m_FacNewMode = false;
        }
        else
        {
            if (m_FacPickedIdx < 0 || m_FacPickedIdx >= m_FacCfg.Factions.Count())
            {
                Hint("pick a faction first");
                return;
            }
            f = m_FacCfg.Factions[m_FacPickedIdx];
        }

        f.DisplayName = GetEdit("FrmName");
        f.Short       = GetEdit("FrmShort");
        f.Color       = GetEdit("FrmColor");
        f.MaxMembers  = GetEdit("FrmMax").ToInt();
        f.Joinable    = m_FrmJoinable;
        f.Hidden      = m_FrmHidden;

        PushFacCfg();
    }

    protected void DeleteFaction()
    {
        if (!m_FacCfg || m_FacPickedIdx < 0 || m_FacPickedIdx >= m_FacCfg.Factions.Count())
        {
            Hint("pick a faction first");
            return;
        }

        // Пiдтвердження другим натисканням: модальнi вiкна у VPP -- пастка
        // (лягають ПОЗАДУ повноекранного кореня й глушать ввiд).
        if (!m_DelArmed)
        {
            m_DelArmed = true;
            Hint("press DELETE again to remove " + m_FacCfg.Factions[m_FacPickedIdx].Id);
            return;
        }

        m_DelArmed = false;
        m_FacCfg.Factions.Remove(m_FacPickedIdx);
        m_FacPickedIdx = -1;
        PushFacCfg();
    }

    protected void PushFacCfg()
    {
        string body;
        string err;
        if (!JsonFileLoader<OZ_FactionsConfig>.MakeData(m_FacCfg, body, err, false))
        {
            Hint("cannot serialise");
            return;
        }
        SendCfg("Factions", body);
    }

    protected void BuildRoster(OZ_AdminRoster r)
    {
        TextListboxWidget lb = TextListboxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("FacRoster"));
        if (!lb)
            return;

        lb.ClearItems();
        m_RosterNames.Clear();
        m_RosterPicked = -1;

        for (int i = 0; i < r.Rows.Count(); i++)
        {
            OZ_AdminRosterRow row = r.Rows[i];
            string line = row.Name;
            if (row.Faction != "")
            {
                line += "  --  " + row.Faction;
                if (row.Leader)
                    line += " [L]";
            }
            lb.AddItem(line, NULL, 0);
            m_RosterNames.Insert(row.Name);
        }
    }

    // ---------------------------------------------------------- спавни

    protected void RebuildSpawnList()
    {
        TextListboxWidget lb = TextListboxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("SpawnList"));
        if (!lb || !m_SpawnsCfg)
            return;

        lb.ClearItems();

        if (m_SpawnsCfg.Zones)
        {
            for (int i = 0; i < m_SpawnsCfg.Zones.Count(); i++)
            {
                OZ_SpawnZone z = m_SpawnsCfg.Zones[i];
                string slug = z.Role;
                if (slug == "")
                    slug = "- (fallback)";
                string line = slug + "   " + z.Center + "   r=" + z.Radius.ToString();
                lb.AddItem(line, NULL, 0);
            }
        }

        if (lb.GetNumItems() == 0)
            lb.AddItem("no zones yet - stand somewhere and press SPAWN HERE", NULL, 0);
    }

    protected void PaintSpawnCycler()
    {
        TextWidget t = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("BtnSpFacText"));
        if (!t)
            return;
        t.SetText("faction: " + SpawnSlugAt(m_SpFacAt));
    }

    protected string SpawnSlugAt(int at)
    {
        // Останнiй пункт циклу -- запасна зона "-".
        int n = 0;
        if (m_FacCfg && m_FacCfg.Factions)
            n = m_FacCfg.Factions.Count();
        if (n == 0 || at >= n)
            return "-";
        return m_FacCfg.Factions[at].Id;
    }

    protected void CycleSpawnFaction()
    {
        int n = 0;
        if (m_FacCfg && m_FacCfg.Factions)
            n = m_FacCfg.Factions.Count();
        m_SpFacAt = (m_SpFacAt + 1) % (n + 1);
        PaintSpawnCycler();
    }

    // ---------------------------------------------------------- raw

    protected void RebuildRawList()
    {
        TextListboxWidget lb = TextListboxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("RawList"));
        if (!lb)
            return;

        lb.ClearItems();
        m_RawRows.Clear();

        for (int i = 0; i < m_CfgNames.Count(); i++)
        {
            if (m_CfgOwners[i] != "core")
                continue;
            lb.AddItem(m_CfgNames[i], NULL, 0);
            m_RawRows.Insert(m_CfgNames[i]);
        }
    }

    // ---------------------------------------------------------- ввiд

    override bool OnItemSelected(Widget w, int x, int y, int row, int column, int oldRow, int oldColumn)
    {
        if (!M_SUB_WIDGET)
            return super.OnItemSelected(w, x, y, row, column, oldRow, oldColumn);

        string nm = w.GetName();

        if (nm == "FacCfgList")
        {
            if (row >= 0 && row < m_FacRowIdx.Count())
                FillFacForm(m_FacRowIdx[row]);
            return true;
        }

        if (nm == "FacRoster")
        {
            if (row >= 0 && row < m_RosterNames.Count())
            {
                m_RosterPicked = row;
                Hint("player: " + m_RosterNames[row]);
            }
            return true;
        }

        if (nm == "RawList")
        {
            if (row >= 0 && row < m_RawRows.Count())
            {
                m_RawPicked = m_RawRows[row];
                AskCfg(m_RawPicked);
            }
            return true;
        }

        return super.OnItemSelected(w, x, y, row, column, oldRow, oldColumn);
    }

    override bool OnChange(Widget w, int x, int y, bool finished)
    {
        if (w && w.GetName() == "FacSearch")
        {
            RebuildFacList();
            return true;
        }
        return super.OnChange(w, x, y, finished);
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (!w || !M_SUB_WIDGET)
            return super.OnClick(w, x, y, button);

        if (w == m_closeButton)
            return super.OnClick(w, x, y, button);

        string nm = w.GetName();

        if (nm.IndexOf("tab:") == 0)
        {
            ShowPane(nm.Substring(4, nm.Length() - 4));
            return true;
        }

        if (nm == "BtnFacNew")
        {
            NewFacForm();
            return true;
        }

        if (nm == "BtnFacSave")
        {
            SaveFaction();
            return true;
        }

        if (nm == "BtnFacDel")
        {
            DeleteFaction();
            return true;
        }

        if (nm == "BtnFrmJoin")
        {
            m_FrmJoinable = !m_FrmJoinable;
            PaintToggles();
            return true;
        }

        if (nm == "BtnFrmHide")
        {
            m_FrmHidden = !m_FrmHidden;
            PaintToggles();
            return true;
        }

        if (nm == "BtnAssign" || nm == "BtnClearFac" || nm == "BtnLead")
        {
            if (m_RosterPicked < 0 || m_RosterPicked >= m_RosterNames.Count())
            {
                Hint("pick a player on the list first");
                return true;
            }
            string player = m_RosterNames[m_RosterPicked];

            if (nm == "BtnAssign")
            {
                if (m_FacPickedIdx < 0 || !m_FacCfg || m_FacPickedIdx >= m_FacCfg.Factions.Count())
                {
                    Hint("pick a faction on the left first");
                    return true;
                }
                OZ_Rpc.RoleRequest(OZ_RoleOp.FACTION_SET, player, m_FacCfg.Factions[m_FacPickedIdx].Id);
            }
            else if (nm == "BtnClearFac")
            {
                OZ_Rpc.RoleRequest(OZ_RoleOp.FACTION_CLEAR, player, "");
            }
            else
            {
                OZ_Rpc.RoleRequest(OZ_RoleOp.LEADER_TRANSFER, player, "");
            }
            return true;
        }

        if (nm == "BtnRoster")
        {
            Ask("roster", "{}");
            return true;
        }

        if (nm == "BtnSpFac")
        {
            CycleSpawnFaction();
            return true;
        }

        if (nm == "BtnSpHere" || nm == "BtnSpClear")
        {
            string slug = SpawnSlugAt(m_SpFacAt);
            string arg = slug;

            if (nm == "BtnSpHere")
            {
                string rad = GetEdit("SpRadius");
                if (rad != "")
                    arg += " " + rad;
                OZ_Rpc.RoleRequest(OZ_RoleOp.SPAWN_HERE, "", arg);
            }
            else
            {
                OZ_Rpc.RoleRequest(OZ_RoleOp.SPAWN_CLEAR, "", slug);
            }
            return true;
        }

        if (nm == "BtnRawReload")
        {
            if (m_RawPicked != "")
                AskCfg(m_RawPicked);
            return true;
        }

        if (nm == "BtnRawApply")
        {
            if (m_RawPicked == "")
            {
                Hint("pick a config first");
                return true;
            }
            MultilineEditBoxWidget ed = MultilineEditBoxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("RawEdit"));
            if (!ed)
                return true;
            string body;
            ed.GetText(body);
            SendCfg(m_RawPicked, body);
            return true;
        }

        return super.OnClick(w, x, y, button);
    }

    // ---------------------------------------------------------- дрiбне

    protected void SetEdit(string name, string val)
    {
        EditBoxWidget e = EditBoxWidget.Cast(M_SUB_WIDGET.FindAnyWidget(name));
        if (e)
            e.SetText(val);
    }

    protected string GetEdit(string name)
    {
        EditBoxWidget e = EditBoxWidget.Cast(M_SUB_WIDGET.FindAnyWidget(name));
        if (!e)
            return "";
        return e.GetText();
    }
}

// Вартовий: VPP ховає лише власний корiнь; сироту на коренi робочої
// областi прибирає мiсiя (OnUpdate пiдменю пiсля ховання не тiкає).
modded class MissionGameplay
{
    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);

        if (!OZ_VppAdminMenu.s_Inst || !OZ_VppAdminMenu.s_Inst.IsOpen())
            return;

        VPPAdminHud hud = VPPAdminHud.Cast(GetGame().GetUIManager().FindMenu(VPP_ADMIN_HUD));
        if (!hud || !hud.IsShowing())
            OZ_VppAdminMenu.s_Inst.ForceHide();
    }
}

#endif
#endif
