// Сторінка «Фракція»: свої люди і фракційні дії. Все, що робить лідер,
// їде через OZ_Rpc.RoleRequest -- тим самим каналом, яким це робили
// контакти, поки фракційні кнопки жили там.
//
// ДІЛИТЬ ВКЛАДКУ З КОНТАКТАМИ (рішення власника 2026-08-30): ліворуч люди,
// праворуч свої. Сторінки лишились дві -- два обробники, два конверти, два
// незалежні оновлення; спільна в них тільки кнопка вкладки, і зшиває їх
// меню (див. m_Companion в OZ_PdaMenu), а не цей файл.
//
// БАЗОВА ФРАКЦІЯ сюди не доходить: сервер віддає «сталкерам» порожню
// фракцію, і половина екрана чесно каже NO FACTION. Сталкери -- це всі в
// Зоні, а не організація: складу в неї немає, лідера немає, і поіменний
// перелік усіх сталкерів сервера тут був би і безглуздий, і зайве
// розголошення.

class OZ_PdaPageFaction : OZ_PdaPage
{
    private int m_Beat = 0;
    private ref OZ_FactionState m_St;
    private Widget m_Rows;
    private ref array<Widget> m_RowWgts;
    private int m_RowsY = 0;
    private string m_Picked = "";     // ім'я обраного члена

    // ДВА КРОКИ на кожну незворотну дію (ТЗ-4 R-C5.1): перший клік озброює
    // й називає ціль і дію словами (R-C5.2), другий -- той самий, поки не
    // вийшов час, -- виконує. Інший вибір знімає озброєння.
    private string m_Armed      = "";
    private float  m_ArmedUntil = 0;
    private static const float ARM_MS = 8000;

    private ButtonWidget m_BtnKick;
    private ButtonWidget m_BtnLead;
    private ButtonWidget m_BtnInvite;
    private ButtonWidget m_BtnJoin;
    private ButtonWidget m_BtnRefuse;
    private ButtonWidget m_BtnPromote;
    private ButtonWidget m_BtnDemote;
    private ButtonWidget m_BtnLeave;

    override string LayoutPath()
    {
        return "OpenZone_Factions_PDA/gui/layouts/oz_pda_page_faction.layout";
    }

    override void OnBuilt()
    {
        m_Rows    = Wgt("FactionRows");
        m_RowWgts = new array<Widget>();

        m_BtnKick   = ButtonWidget.Cast(Wgt("BtnFKick"));
        SetText("BtnFKickText", "#STR_OZ_FACTION_KICK");
        m_BtnLead   = ButtonWidget.Cast(Wgt("BtnFLead"));
        SetText("BtnFLeadText", "#STR_OZ_FACTION_LEAD");
        m_BtnInvite = ButtonWidget.Cast(Wgt("BtnFInvite"));
        SetText("BtnFInviteText", "#STR_OZ_FACTION_INVITE");
        m_BtnJoin   = ButtonWidget.Cast(Wgt("BtnFJoin"));
        SetText("BtnFJoinText", "#STR_OZ_FACTION_JOIN");
        m_BtnRefuse = ButtonWidget.Cast(Wgt("BtnFRefuse"));
        SetText("BtnFRefuseText", "#STR_OZ_FACTION_REFUSE");

        m_BtnPromote = ButtonWidget.Cast(Wgt("BtnFPromote"));
        SetText("BtnFPromoteText", "#STR_OZ_FACTION_PROMOTE");
        m_BtnDemote  = ButtonWidget.Cast(Wgt("BtnFDemote"));
        SetText("BtnFDemoteText", "#STR_OZ_FACTION_DEMOTE");
        m_BtnLeave   = ButtonWidget.Cast(Wgt("BtnFLeave"));
        SetText("BtnFLeaveText", "#STR_OZ_FACTION_LEAVE");
    }

    override void OnSelected()
    {
        Request();
    }

    override void OnRefresh()
    {
        // Ролі їдуть через Discord і повертаються не миттєво: рідкий
        // самооновлювач страхує пуш, а не замінює його. Раз на 5 секунд
        // -- зміни й так приходять пушем одразу.
        m_Beat++;
        if (m_Beat % 5 != 0)
            return;
        Request();
    }

    private void Request()
    {
        OZ_Rpc.Request(OZFP_Const.PAGE_FACTION, "state", "{}");
    }

    override void OnResponse(string op, bool ok, string json, string error)
    {
        if (op == "push")
        {
            Request();
            return;
        }

        if (op != "state")
            return;

        if (!ok)
        {
            SetHintSticky("FactionHint", "#" + error);
            return;
        }

        string err;
        OZ_FactionState st;
        if (!JsonFileLoader<OZ_FactionState>.LoadData(json, st, err) || !st)
            return;

        m_St = st;
        Paint();
    }

    private void Paint()
    {
        if (!m_St)
            return;

        // Шапка: чия це сторінка. Одинак бачить чесне «фракції немає».
        if (m_St.Org == "")
        {
            SetText("FactionTitle", "#STR_OZ_FACTION_NONE");
            SetText("FactionMine", "");
        }
        else
        {
            TextWidget t = TextWidget.Cast(Wgt("FactionTitle"));
            if (t)
            {
                t.SetText(m_St.FactionName);
                if (m_St.Color != 0)
                    t.SetColor(m_St.Color);
            }

            string mine = m_St.MyRank;
            if (m_St.MeLeader)
            {
                if (mine != "")
                    mine += "  ";
                mine += Widget.TranslateString("#STR_OZ_FACTION_LEADER");
            }
            SetText("FactionMine", mine);
        }

        // Запрошення: банер з двома кнопками.
        bool invited = m_St.InviteFaction != "";
        Widget banner = Wgt("InvitePane");
        if (banner)
            banner.Show(invited);
        if (invited)
            SetText("InviteText", m_St.InviteFaction + "  --  " + m_St.InviteFrom);

        // Члени.
        for (int r = 0; r < m_RowWgts.Count(); r++)
        {
            if (m_RowWgts[r])
                m_RowWgts[r].Unlink();
        }
        m_RowWgts.Clear();
        m_RowsY = 0;

        if (m_Rows && m_St.Members)
        {
            for (int i = 0; i < m_St.Members.Count(); i++)
                MemberRow(m_St.Members[i]);

            // Висота канви чесна: коли всі влазять, повзунок не потрібен.
            // Ширина -- рядка фракції в правій половині спільної вкладки.
            m_Rows.SetSize(616, m_RowsY);
        }

        PaintButtons();
    }

    private void MemberRow(OZ_FactionMember m)
    {
        Widget w = GetGame().GetWorkspace().CreateWidgets("OpenZone_PDA/gui/layouts/oz_pda_faction_row.layout", m_Rows);
        if (!w)
            return;

        w.SetPos(0, m_RowsY);
        m_RowsY += 34;
        w.SetName(RowKey(m));
        w.SetUserID(8);
        m_RowWgts.Insert(w);

        TextWidget n = TextWidget.Cast(w.FindAnyWidget("FRowName"));
        if (n)
        {
            string label = m.Name;
            if (m.Leader)
                label = "* " + label;
            n.SetText(label);
            if (m.Me)
                n.SetColor(ARGB(255, 255, 122, 26));
            else if (m.Online)
                n.SetColor(ARGB(255, 126, 200, 160));
        }

        // У списку СВОЇХ головне звання -- фракційне: воно й вирішує, хто
        // кому старший усередині. Сталкерське показуємо, коли фракційного
        // немає, -- порожній стовпчик не сказав би нічого.
        TextWidget rk = TextWidget.Cast(w.FindAnyWidget("FRowRank"));
        if (rk)
        {
            if (m.FRank != "")
                rk.SetText(m.FRank);
            else
                rk.SetText(m.Rank);
        }

        // Зебра: парні рядки трохи світліші, оку легше вести рядок.
        Widget bg = w.FindAnyWidget("FRowBg");
        if (bg && (m_RowWgts.Count() % 2) == 0)
            bg.SetColor(ARGB(255, 20, 20, 23));

        Widget pick = w.FindAnyWidget("FRowPick");
        if (pick)
            pick.Show(RowKey(m) == m_Picked);
    }

    // Кого вибрано в СУСІДНІЙ половині вкладки. Саме його кличуть у
    // фракцію: у списку своїх його ще немає й бути не може.
    private string ContactPick()
    {
        OZ_PdaMenu menu = OZ_PdaMenu.Cast(GetGame().GetUIManager().FindMenu(OZ_PdaConst.MENU_PDA));
        if (!menu)
            return "";

        OZ_PdaPageContacts c = OZ_PdaPageContacts.Cast(menu.PageOf(OZ_PdaConst.PAGE_CONTACTS));
        if (!c)
            return "";
        return c.PickedKey();
    }

    private void PaintButtons()
    {
        bool lead = m_St && m_St.MeLeader;
        bool mine = m_St && m_St.Org != "";
        bool pickedOther = m_Picked != "" && m_St && !PickedIsMe();

        // Лідерські дії над обраним СВОЇМ.
        if (m_BtnKick)
            m_BtnKick.Show(lead && pickedOther);
        if (m_BtnLead)
            m_BtnLead.Show(lead && pickedOther);

        // Підвищення й зниження -- лише коли у фракції взагалі є драбина:
        // кнопка, якій нема куди рухати, гірша за її відсутність.
        bool ladder = m_St && m_St.RankIds && m_St.RankIds.Count() > 0;
        if (m_BtnPromote)
            m_BtnPromote.Show(lead && pickedOther && ladder);
        if (m_BtnDemote)
            m_BtnDemote.Show(lead && pickedOther && ladder);

        // Покликати -- того, кого видно ЛІВОРУЧ, у списку людей.
        if (m_BtnInvite)
            m_BtnInvite.Show(lead && ContactPick() != "");

        // Піти можна завжди, поки є звідки. Лідер теж: посада перейде
        // наступному сама.
        if (m_BtnLeave)
            m_BtnLeave.Show(mine);
    }

    // Сусідня сходинка драбини для обраного. `up` -- вгору, інакше вниз.
    // Порожній рядок означає «зняти звання зовсім», і це законна
    // відповідь: зниження з найнижчої сходинки саме цим і є.
    private string NextRank(bool up)
    {
        if (!m_St || !m_St.RankIds || m_St.RankIds.Count() == 0)
            return "";

        OZ_FactionMember m = PickedMember();
        if (!m)
            return "";

        int at = m_St.RankIds.Find(m.FRankId);   // -1, коли звання немає

        if (up)
        {
            if (at + 1 >= m_St.RankIds.Count())
                return m.FRankId;   // вище нікуди -- лишаємо як є
            return m_St.RankIds[at + 1];
        }

        if (at <= 0)
            return "";   // нижче найнижчої -- без звання
        return m_St.RankIds[at - 1];
    }

    // Ім'я віджета рядка й m_Picked -- ключ персонажа; ім'я лише тоді, коли
    // сервер старий і ключа не дав.
    private string RowKey(OZ_FactionMember m)
    {
        if (m.Key != "")
            return m.Key;
        return m.Name;
    }

    // Адреса обраного для сервера: ключем (ТЗ-4 R-C4.1), а без ключа --
    // іменем, як раніше.
    private string Target()
    {
        OZ_FactionMember m = PickedMember();
        if (m && m.Key != "")
            return "key:" + m.Key;
        return m_Picked;
    }

    private string PickedLabel()
    {
        OZ_FactionMember m = PickedMember();
        if (m)
            return m.Name;
        return m_Picked;
    }

    private string T(string key)
    {
        return Widget.TranslateString("#" + key);
    }

    // Перший клік -- озброїти й сказати словами, що саме станеться; другий
    // такий самий у межах ARM_MS -- виконати. Підтверджується САМЕ те, що
    // написано: інша дія чи інша ціль починають спочатку.
    private bool Confirm(string what, string text)
    {
        float now = GetGame().GetTime();
        if (m_Armed == what && now < m_ArmedUntil)
        {
            m_Armed = "";
            SetText("FactionHint", "");
            return true;
        }

        m_Armed      = what;
        m_ArmedUntil = now + ARM_MS;
        SetHintSticky("FactionHint", text);
        return false;
    }

    private string Ask(string key, string who)
    {
        return T(key) + " " + who + " - " + T("STR_OZ_F_AGAIN");
    }

    private string RankLabel(string id)
    {
        if (id == "")
            return T("STR_OZ_F_NO_RANK");
        if (m_St && m_St.RankIds && m_St.RankNames)
        {
            int at = m_St.RankIds.Find(id);
            if (at >= 0 && at < m_St.RankNames.Count())
                return m_St.RankNames[at];
        }
        return id;
    }

    private string MyFRank()
    {
        if (!m_St || !m_St.Members)
            return "";
        for (int i = 0; i < m_St.Members.Count(); i++)
        {
            if (m_St.Members[i].Me)
                return m_St.Members[i].FRank;
        }
        return "";
    }

    // Що саме буде втрачено (ТЗ-4 R-C2.2): угруповання, звання, посада.
    // Із посад клієнт знає лише лідерство -- решта живе в Discord.
    private string LossTail()
    {
        string s = "";
        string fr = MyFRank();
        if (fr != "")
            s += ", " + T("STR_OZ_F_LOSE_RANK") + " " + fr;
        if (m_St && m_St.MeLeader)
            s += " " + T("STR_OZ_F_LOSE_LEAD");
        return s;
    }

    private string JoinText()
    {
        return T("STR_OZ_F_ASK_JOIN") + " " + m_St.InviteFaction + ": " + T("STR_OZ_F_LOSE") + " " + m_St.FactionName + LossTail() + " - " + T("STR_OZ_F_AGAIN");
    }

    private string LeaveText()
    {
        return T("STR_OZ_F_ASK_LEAVE") + " " + m_St.FactionName + LossTail() + " - " + T("STR_OZ_F_AGAIN");
    }

    private OZ_FactionMember PickedMember()
    {
        if (!m_St || !m_St.Members)
            return null;
        for (int i = 0; i < m_St.Members.Count(); i++)
        {
            if (RowKey(m_St.Members[i]) == m_Picked)
                return m_St.Members[i];
        }
        return null;
    }

    private bool PickedIsMe()
    {
        if (!m_St || !m_St.Members)
            return false;
        for (int i = 0; i < m_St.Members.Count(); i++)
        {
            if (RowKey(m_St.Members[i]) == m_Picked)
                return m_St.Members[i].Me;
        }
        return false;
    }

    override bool OnPageClick(Widget w, int x, int y)
    {
        if (!w)
            return false;

        if (w.GetUserID() == 8)
        {
            m_Picked = w.GetName();
            m_Armed  = "";
            Paint();
            return true;
        }

        if (w == m_BtnJoin)
        {
            // Ця вкладка є лише в того, хто ВЖЕ в угрупованні, тож прийняти
            // тут -- завжди покинути своє: кажемо, що саме (ТЗ-4 R-C2.2).
            if (m_St && Confirm("join", JoinText()))
                OZ_Rpc.RoleRequest("accept", "", "");
            return true;
        }

        if (w == m_BtnRefuse)
        {
            OZ_Rpc.RoleRequest("decline", "", "");
            return true;
        }

        if (w == m_BtnKick)
        {
            if (m_Picked != "" && Confirm("kick:" + m_Picked, Ask("STR_OZ_F_ASK_KICK", PickedLabel())))
                OZ_Rpc.RoleRequest(OZ_RoleOp.FACTION_CLEAR, Target(), "");
            return true;
        }

        if (w == m_BtnLead)
        {
            if (m_Picked != "" && Confirm("lead:" + m_Picked, Ask("STR_OZ_F_ASK_LEAD", PickedLabel())))
                OZ_Rpc.RoleRequest(OZ_RoleOp.LEADER_TRANSFER, Target(), "");
            return true;
        }

        if (w == m_BtnInvite)
        {
            // Кличемо того, кого вибрано в лівій половині вкладки.
            string who = ContactPick();
            if (who == "")
            {
                SetHintSticky("FactionHint", "#STR_OZ_FRIEND_PICK");
                return true;
            }
            // Запрошення підтвердження не потребує: воно нічого не міняє,
            // поки той не погодиться сам. Адреса -- ключ персонажа.
            OZ_Rpc.RoleRequest("invite", "key:" + who, "");
            return true;
        }

        if (w == m_BtnPromote || w == m_BtnDemote)
        {
            if (m_Picked == "")
                return true;

            // Сходинку рахує клієнт -- він має драбину, -- але право на
            // саму зміну перевіряють сервер і міст, як і завжди.
            string next = NextRank(w == m_BtnPromote);
            string ask  = T("STR_OZ_F_ASK_RANK") + " " + PickedLabel() + ": " + RankLabel(next) + " - " + T("STR_OZ_F_AGAIN");
            if (Confirm("rank:" + m_Picked + ":" + next, ask))
                OZ_Rpc.RoleRequest(OZ_RoleOp.FRANK_SET, Target(), next);
            return true;
        }

        if (w == m_BtnLeave)
        {
            // Ціль не називаємо: піти можна тільки самому.
            if (m_St && Confirm("leave", LeaveText()))
                OZ_Rpc.RoleRequest("leave", "", "");
            return true;
        }

        return false;
    }
}
