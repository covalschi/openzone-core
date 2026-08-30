// Вкладка «OpenZone» у VPP Admin Tools, третя редакцiя: фракцiї -- лише
// призначення (народжує й ховає їх бот у Discord), гравцевi -- мiтки,
// особистий спавн i пермадес. Субмод КПК доклада свою вкладку через
// modded class -- точки розширення позначенi словом protected.
//
// Клiєнтський UI i НIЧОГО бiльше: кожна дiя їде на сервер каналами ядра
// (OZ_Rpc.Request на сторiнку "admin", OZ_Rpc.RoleRequest на ролi та
// спавни), i кожну сервер перевiряє САМ (OZ_Perm.IsAdmin).
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

    // ------------------------------------------------- фракцiї i гравцi
    //
    // Джерело фракцiй -- РОСТЕР, а не Factions.json: файла в реєстрi
    // конфiгiв бiльше немає (фракцiї народжує лише бот), а для призначення
    // досить слагiв.
    protected ref array<string> m_Factions;
    protected ref array<int> m_FacRowIdx;   // рядок списку -> iндекс у m_Factions
    protected int m_FacPicked = -1;

    // Ростер цiлком: картцi гравця треба все, а не лише iм'я з uid-ом.
    protected ref array<string> m_RosterNames;
    protected ref array<string> m_RosterUids;
    protected ref array<string> m_RosterDNames;
    protected ref array<string> m_RosterFactions;
    protected ref array<string> m_RosterRanks;
    protected ref array<string> m_RosterFRanks;
    protected ref array<string> m_RosterTraits;
    protected ref array<bool>   m_RosterLeads;
    protected int m_RosterPicked = -1;

    // Каталоги з реєстру бота, i що зараз пiд курсором циклерiв. FRanks --
    // повнi id "duty:sergeant"; циклер показує лише драбину фракцiї
    // ВИБРАНОГО гравця, тому iндекс живе окремо й скидається з вибором.
    protected ref array<string> m_Traits;
    protected int m_TraitAt = 0;
    protected ref array<string> m_Ranks;
    protected int m_RankAt = 0;
    protected ref array<string> m_FRanks;
    protected int m_FRankAt = 0;

    // Пермадес: пiдтвердження другим натисканням, як i всюди у вкладцi
    // (модальнi вiкна у VPP -- пастка).
    protected bool m_WipeArmed = false;
    protected string m_WipeUid = "";

    // Перемальовування списку кличе SelectRow, а якщо рушiй вiдповiсть на
    // нього OnItemSelected -- вийде рекурсiя. Прапорець рве це коло.
    protected bool m_Repaint = false;

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
        m_Factions  = new array<string>();
        m_FacRowIdx = new array<int>();
        m_RosterNames    = new array<string>();
        m_RosterUids     = new array<string>();
        m_RosterDNames   = new array<string>();
        m_RosterFactions = new array<string>();
        m_RosterRanks    = new array<string>();
        m_RosterFRanks   = new array<string>();
        m_RosterTraits   = new array<string>();
        m_RosterLeads    = new array<bool>();
        m_Traits  = new array<string>();
        m_Ranks   = new array<string>();
        m_FRanks  = new array<string>();
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
            Ask("roster", "{}");
        if (id == "spawns")
        {
            AskCfg("Spawns");
            // Циклеру фракцiй потрiбен ростер -- вiн i джерело слагiв.
            if (m_Factions.Count() == 0)
                Ask("roster", "{}");
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
        Ask("cfg_get:" + m_CfgQ[0], "{}");
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
        // Тiло їде СИРИМ: конверт зi строковим полем рiзався б на 1023
        // байтах при серверному розборi (та сама пастка, що й у cfg_get).
        Ask("cfg_set:" + name, body);
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
            if (op.IndexOf("cfg_get:") == 0)
                CfgDone(op.Substring(8, op.Length() - 8));
            if (op.IndexOf("player_wipe:") == 0)
                m_WipeArmed = false;
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

        if (op.IndexOf("cfg_get:") == 0)
        {
            string gname = op.Substring(8, op.Length() - 8);
            CfgDone(gname);
            OnCfgText(gname, json);
            return;
        }

        if (op.IndexOf("cfg_set:") == 0)
        {
            Hint(op.Substring(8, op.Length() - 8) + " applied");
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

        if (op.IndexOf("player_wipe:") == 0)
        {
            m_WipeArmed = false;
            Hint("wiped: the character starts over as a novice stalker");
            // Ролi їдуть через Discord -- ростер оновлюємо з запасом.
            GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(this.AskRoster, 2500, false);
            return;
        }
    }

    // Текст конфiгу приїхав. Субмод КПК перехоплює свої iмена через super.
    protected void OnCfgText(string name, string body)
    {
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
        if (CurrentPane() == "spawns")
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

        // Умова не переноситься: парсер Enforce падає на багаторядковому
        // if iз хвостовим || (змiряно 2026-08-30).
        bool spawnOp = op == OZ_RoleOp.SPAWN_HERE || op == OZ_RoleOp.SPAWN_CLEAR;
        if (!spawnOp)
            spawnOp = op == OZ_RoleOp.SPAWN_UID_HERE || op == OZ_RoleOp.SPAWN_UID_CLEAR;
        if (spawnOp)
        {
            AskCfg("Spawns");
            return;
        }

        // Ролi їдуть через Discord: перепитуємо ростер трохи згодом, i ще
        // раз пiзнiше -- проекцiя вертається не миттєво.
        GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(this.AskRoster, 1500, false);
        GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(this.AskRoster, 6000, false);
    }

    protected void AskRoster()
    {
        if (IsOpen())
            Ask("roster", "{}");
    }

    // ---------------------------------------------------------- фракцiї

    protected void BuildRoster(OZ_AdminRoster r)
    {
        m_Repaint = true;

        // Слаги фракцiй i каталоги -- з того самого конверта.
        m_Factions.Clear();
        if (r.Factions)
        {
            for (int fi = 0; fi < r.Factions.Count(); fi++)
                m_Factions.Insert(r.Factions[fi]);
        }

        m_Traits.Clear();
        if (r.Traits)
        {
            for (int ti = 0; ti < r.Traits.Count(); ti++)
                m_Traits.Insert(r.Traits[ti]);
        }
        if (m_TraitAt >= m_Traits.Count())
            m_TraitAt = 0;

        m_Ranks.Clear();
        if (r.Ranks)
        {
            for (int ri = 0; ri < r.Ranks.Count(); ri++)
                m_Ranks.Insert(r.Ranks[ri]);
        }
        if (m_RankAt >= m_Ranks.Count())
            m_RankAt = 0;

        m_FRanks.Clear();
        if (r.FRanks)
        {
            for (int qi = 0; qi < r.FRanks.Count(); qi++)
                m_FRanks.Insert(r.FRanks[qi]);
        }

        RebuildFacList();
        PaintSpawnCycler();
        PaintTraitCycler();
        PaintRankCycler();

        // Вибiр переживає оновлення: ростер перечитується сам пiсля кожної
        // операцiї, i губити вiд цього видiлення -- значить клацати гравця
        // заново пiсля кожної кнопки.
        string keepUid = "";
        if (m_RosterPicked >= 0 && m_RosterPicked < m_RosterUids.Count())
            keepUid = m_RosterUids[m_RosterPicked];

        m_RosterNames.Clear();
        m_RosterUids.Clear();
        m_RosterDNames.Clear();
        m_RosterFactions.Clear();
        m_RosterRanks.Clear();
        m_RosterFRanks.Clear();
        m_RosterTraits.Clear();
        m_RosterLeads.Clear();
        m_RosterPicked = -1;
        m_WipeArmed = false;

        for (int i = 0; i < r.Rows.Count(); i++)
        {
            OZ_AdminRosterRow row = r.Rows[i];

            m_RosterNames.Insert(row.Name);
            m_RosterUids.Insert(row.Uid);
            m_RosterDNames.Insert(row.DName);
            m_RosterFactions.Insert(row.Faction);
            m_RosterRanks.Insert(row.Rank);
            m_RosterFRanks.Insert(row.FRank);
            m_RosterTraits.Insert(row.Traits);
            m_RosterLeads.Insert(row.Leader);

            if (keepUid != "" && row.Uid == keepUid)
                m_RosterPicked = i;
        }

        RepaintRoster();
        m_Repaint = false;
        FillPlayerCard();
        PaintFRankCycler();
    }

    // Список гравцiв: компактний рядок, видiлений позначено стрiлкою --
    // пiдсвiтка рядка листбокса непомiтна, i це вже коштувало плутанини.
    protected void RepaintRoster()
    {
        TextListboxWidget lb = TextListboxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("FacRoster"));
        if (!lb)
            return;

        lb.ClearItems();

        for (int i = 0; i < m_RosterNames.Count(); i++)
        {
            string line = "";
            if (i == m_RosterPicked)
                line = "> ";

            line += m_RosterNames[i];
            if (m_RosterFactions[i] != "")
            {
                line += "  --  " + m_RosterFactions[i];
                if (m_RosterLeads[i])
                    line += " [L]";
            }

            lb.AddItem(line, NULL, 0);

            if (i == m_RosterPicked)
                lb.SelectRow(i);
        }
    }

    // Картка гравця: кожен факт своїм рядком, бо в один вони не влазять.
    protected void FillPlayerCard()
    {
        TextWidget nameT    = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("InfoName"));
        TextWidget discordT = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("InfoDiscord"));
        TextWidget steamT   = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("InfoSteam"));
        TextWidget facT     = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("InfoFaction"));
        TextWidget rankT    = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("InfoRank"));
        TextWidget traitsT  = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("InfoTraits"));

        if (m_RosterPicked < 0 || m_RosterPicked >= m_RosterNames.Count())
        {
            if (nameT)
                nameT.SetText("pick a player on the right");
            if (discordT)
                discordT.SetText("");
            if (steamT)
                steamT.SetText("");
            if (facT)
                facT.SetText("");
            if (rankT)
                rankT.SetText("");
            if (traitsT)
                traitsT.SetText("");
            return;
        }

        int i = m_RosterPicked;

        if (nameT)
            nameT.SetText(m_RosterNames[i]);

        if (discordT)
        {
            string dn = m_RosterDNames[i];
            if (dn == "")
                dn = "-";
            discordT.SetText("discord: " + dn);
        }

        if (steamT)
            steamT.SetText("steam: " + m_RosterUids[i]);

        if (facT)
        {
            string fac = m_RosterFactions[i];
            if (fac == "")
                fac = "-";
            if (m_RosterFRanks[i] != "")
                fac += "  ^" + m_RosterFRanks[i];
            if (m_RosterLeads[i])
                fac += "  [leader]";
            facT.SetText("faction: " + fac);
        }

        if (rankT)
        {
            string rk = m_RosterRanks[i];
            if (rk == "")
                rk = "-";
            rankT.SetText("stalker rank: " + rk);
        }

        if (traitsT)
        {
            string tr = m_RosterTraits[i];
            if (tr == "")
                tr = "-";
            traitsT.SetText("traits: " + tr);
        }
    }

    protected void RebuildFacList()
    {
        TextListboxWidget lb = TextListboxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("FacCfgList"));
        if (!lb)
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

        for (int i = 0; i < m_Factions.Count(); i++)
        {
            string line = m_Factions[i];

            if (filter != "")
            {
                string probe = line;
                probe.ToLower();
                if (probe.IndexOf(filter) == -1)
                    continue;
            }

            // Видiлення позначаємо стрiлкою: пiдсвiтка рядка листбокса
            // непомiтна, i хто видiлений -- було не зрозумiло.
            if (i == m_FacPicked)
                line = "> " + line;

            int row = lb.AddItem(line, NULL, 0);
            m_FacRowIdx.Insert(i);

            if (i == m_FacPicked)
                lb.SelectRow(row);
        }
    }

    // Цiль дiй з гравцем: точна адреса uid з ростера. Сервер приймає таку
    // форму лише вiд адмiна -- тезки й самопризначення перестають бути
    // проблемою.
    protected string PickedPlayer()
    {
        if (m_RosterPicked < 0 || m_RosterPicked >= m_RosterUids.Count())
            return "";
        return m_RosterUids[m_RosterPicked];
    }

    protected void PaintTraitCycler()
    {
        TextWidget t = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("BtnTraitText"));
        if (!t)
            return;

        if (m_Traits.Count() == 0)
        {
            t.SetText("trait: none known yet");
            return;
        }
        t.SetText("trait: " + m_Traits[m_TraitAt]);
    }

    protected void PaintRankCycler()
    {
        TextWidget t = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("BtnRankText"));
        if (!t)
            return;

        if (m_Ranks.Count() == 0)
        {
            t.SetText("rank: none known yet");
            return;
        }
        t.SetText("rank: " + m_Ranks[m_RankAt]);
    }

    // Драбина фракцiї ВИБРАНОГО гравця, голими слагами. Порожньо -- гравець
    // не вибраний, поза фракцiєю, або в його фракцiї звань не завели.
    protected void FRankOptions(array<string> outBare)
    {
        if (m_RosterPicked < 0 || m_RosterPicked >= m_RosterFactions.Count())
            return;

        string prefix = m_RosterFactions[m_RosterPicked] + ":";
        if (prefix == ":")
            return;

        for (int i = 0; i < m_FRanks.Count(); i++)
        {
            if (m_FRanks[i].IndexOf(prefix) == 0)
                outBare.Insert(m_FRanks[i].Substring(prefix.Length(), m_FRanks[i].Length() - prefix.Length()));
        }
    }

    protected void PaintFRankCycler()
    {
        TextWidget t = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("BtnFRankText"));
        if (!t)
            return;

        array<string> opts = new array<string>();
        FRankOptions(opts);

        if (opts.Count() == 0)
        {
            t.SetText("faction rank: none here");
            return;
        }

        if (m_FRankAt >= opts.Count())
            m_FRankAt = 0;
        t.SetText("faction rank: " + opts[m_FRankAt]);
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

        // Особистi точки -- у тому ж списку, з мiткою гравця: адмiн бачить
        // УСЕ, що впливає на спавни, на одному екранi.
        if (m_SpawnsCfg.Personal)
        {
            for (int k = 0; k < m_SpawnsCfg.Personal.Count(); k++)
            {
                OZ_SpawnPersonal p = m_SpawnsCfg.Personal[k];
                string pline = "player " + p.Uid + "   " + p.Center + "   r=" + p.Radius.ToString();
                lb.AddItem(pline, NULL, 0);
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
        int n = m_Factions.Count();
        if (n == 0 || at >= n)
            return "-";
        return m_Factions[at];
    }

    protected void CycleSpawnFaction()
    {
        int n = m_Factions.Count();
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

        if (m_Repaint)
            return true;

        string nm = w.GetName();

        if (nm == "FacCfgList")
        {
            if (row >= 0 && row < m_FacRowIdx.Count())
            {
                m_FacPicked = m_FacRowIdx[row];
                m_Repaint = true;
                RebuildFacList();
                m_Repaint = false;
                Hint("faction: " + m_Factions[m_FacPicked]);
            }
            return true;
        }

        if (nm == "FacRoster")
        {
            if (row >= 0 && row < m_RosterNames.Count())
            {
                m_RosterPicked = row;
                m_WipeArmed = false;
                m_FRankAt = 0;
                m_Repaint = true;
                RepaintRoster();
                m_Repaint = false;
                FillPlayerCard();
                PaintFRankCycler();
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

        if (nm == "BtnAssign" || nm == "BtnClearFac" || nm == "BtnLead")
        {
            string uid = PickedPlayer();
            if (uid == "")
            {
                Hint("pick a player on the list first");
                return true;
            }
            string player = "uid:" + uid;

            if (nm == "BtnAssign")
            {
                if (m_FacPicked < 0 || m_FacPicked >= m_Factions.Count())
                {
                    Hint("pick a faction on the left first");
                    return true;
                }
                OZ_Rpc.RoleRequest(OZ_RoleOp.FACTION_SET, player, m_Factions[m_FacPicked]);
            }
            else if (nm == "BtnClearFac")
            {
                OZ_Rpc.RoleRequest(OZ_RoleOp.FACTION_CLEAR, player, "");
            }
            else
            {
                // Консоль ставить лiдера НАПРЯМУ. leader.transfer тут не
                // годиться: вiн -- акт лiдера й вимагає, щоб актор сам
                // тримав пост (змiряно: nobody to hand it over from).
                OZ_Rpc.RoleRequest(OZ_RoleOp.LEADER_SET, player, "");
            }
            return true;
        }

        if (nm == "BtnRank")
        {
            if (m_Ranks.Count() > 0)
                m_RankAt = (m_RankAt + 1) % m_Ranks.Count();
            PaintRankCycler();
            return true;
        }

        if (nm == "BtnFRank")
        {
            array<string> copts = new array<string>();
            FRankOptions(copts);
            if (copts.Count() > 0)
                m_FRankAt = (m_FRankAt + 1) % copts.Count();
            PaintFRankCycler();
            return true;
        }

        if (nm == "BtnFRankSet" || nm == "BtnFRankClear")
        {
            string fuid = PickedPlayer();
            if (fuid == "")
            {
                Hint("pick a player on the list first");
                return true;
            }

            if (nm == "BtnFRankSet")
            {
                array<string> fopts = new array<string>();
                FRankOptions(fopts);
                if (fopts.Count() == 0)
                {
                    Hint("his faction has no ranks - add them via the bot");
                    return true;
                }
                if (m_FRankAt >= fopts.Count())
                    m_FRankAt = 0;
                OZ_Rpc.RoleRequest(OZ_RoleOp.FRANK_SET, "uid:" + fuid, fopts[m_FRankAt]);
            }
            else
            {
                OZ_Rpc.RoleRequest(OZ_RoleOp.FRANK_SET, "uid:" + fuid, "");
            }
            return true;
        }

        if (nm == "BtnRankSet" || nm == "BtnRankClear")
        {
            string ruid = PickedPlayer();
            if (ruid == "")
            {
                Hint("pick a player on the list first");
                return true;
            }

            if (nm == "BtnRankSet")
            {
                if (m_Ranks.Count() == 0)
                {
                    Hint("no ranks in the bot registry yet");
                    return true;
                }
                OZ_Rpc.RoleRequest(OZ_RoleOp.RANK_SET, "uid:" + ruid, m_Ranks[m_RankAt]);
            }
            else
            {
                // Порожнiй аргумент -- зняти звання зовсiм: так читає його
                // мiст (rank.set без arg знiмає всi).
                OZ_Rpc.RoleRequest(OZ_RoleOp.RANK_SET, "uid:" + ruid, "");
            }
            return true;
        }

        if (nm == "BtnTrait")
        {
            if (m_Traits.Count() > 0)
                m_TraitAt = (m_TraitAt + 1) % m_Traits.Count();
            PaintTraitCycler();
            return true;
        }

        if (nm == "BtnTraitAdd" || nm == "BtnTraitDel")
        {
            string tuid = PickedPlayer();
            if (tuid == "")
            {
                Hint("pick a player on the list first");
                return true;
            }
            if (m_Traits.Count() == 0)
            {
                Hint("no traits in the bot registry yet");
                return true;
            }

            if (nm == "BtnTraitAdd")
                OZ_Rpc.RoleRequest(OZ_RoleOp.TRAIT_ADD, "uid:" + tuid, m_Traits[m_TraitAt]);
            else
                OZ_Rpc.RoleRequest(OZ_RoleOp.TRAIT_REMOVE, "uid:" + tuid, m_Traits[m_TraitAt]);
            return true;
        }

        if (nm == "BtnPSpawn" || nm == "BtnPSpawnClear")
        {
            string suid = PickedPlayer();
            if (suid == "")
            {
                Hint("pick a player on the list first");
                return true;
            }

            if (nm == "BtnPSpawn")
            {
                string arg = suid;
                string rad = GetEdit("PSpRadius");
                if (rad != "")
                    arg += " " + rad;
                OZ_Rpc.RoleRequest(OZ_RoleOp.SPAWN_UID_HERE, "", arg);
            }
            else
            {
                OZ_Rpc.RoleRequest(OZ_RoleOp.SPAWN_UID_CLEAR, "", suid);
            }
            return true;
        }

        if (nm == "BtnWipe")
        {
            string wuid = PickedPlayer();
            if (wuid == "")
            {
                Hint("pick a player on the list first");
                return true;
            }

            // Пiдтвердження другим натисканням ПО ТОМУ Ж гравцевi: змiна
            // вибору скидає зброю, iнакше пiдтвердженням для одного стало
            // б натискання, зроблене для iншого.
            if (!m_WipeArmed || m_WipeUid != wuid)
            {
                m_WipeArmed = true;
                m_WipeUid = wuid;
                Hint("press WIPE again to erase " + m_RosterNames[m_RosterPicked] + " forever");
                return true;
            }

            m_WipeArmed = false;
            Ask("player_wipe:" + wuid, "{}");
            Hint("wiping...");
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
            string zarg = slug;

            if (nm == "BtnSpHere")
            {
                string zrad = GetEdit("SpRadius");
                if (zrad != "")
                    zarg += " " + zrad;
                OZ_Rpc.RoleRequest(OZ_RoleOp.SPAWN_HERE, "", zarg);
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
