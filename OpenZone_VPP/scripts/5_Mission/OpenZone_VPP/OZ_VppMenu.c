// Вкладка «OpenZone» у VPP Admin Tools, третя редакцiя: фракцiї -- лише
// призначення (народжує й ховає їх бот у Discord), гравцевi -- мiтки,
// особистий спавн i пермадес. Субмод КПК доклада свою вкладку через
// modded class -- точки розширення позначенi словом protected.
//
// Клiєнтський UI i НIЧОГО бiльше: кожна дiя їде на сервер АДМIНСЬКИМ
// конвертом ядра (OZ_Rpc.AdminRequest у роздiл "config" чи "spawns"), i
// кожну сервер перевiряє САМ -- OZ_Perm.IsAdmin першим рядком диспетчера.
//
// Досi це був конверт СТОРIНОК на сторiнку "admin", i звiдти ж росли три з
// чотирьох поломок вкладки: панель фракцiй адресувала свiй ростер на
// "admin", де про нього не чули; за цим стояв гейт КПК, який пропускав саме
// й тiльки "admin"; а роль-конверт, яким їхали спавни, обробляв мод фракцiй,
// тобто без нього панель SPAWNS говорила в порожнечу.
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

// Слаги фракцiй ДЛЯ ПАНЕЛI СПАВНiВ -- маленький шов, а не знання.
//
// Зона спавну може належати фракцiї, тож циклер мусить показати перелiк. Але
// звiдки той перелiк узявся -- справа мода фракцiй, якого може й не бути:
// тодi список порожнiй, циклер показує саму лише запасну зону "-", i панель
// працює далi -- слаг набирається в полi поруч.
//
// Заповнює його склейка @OpenZone_Factions_VPP, коли отримує ростер. До
// 2026-09-01 Set() не кликав НIХТО в усьому деревi, тож циклер вiчно стояв
// на "-", i завести фракцiйну зону з вкладки не можна було взагалi.
class OZ_VppFactionSlugs
{
    private static ref array<string> s_Slugs;

    static void Set(array<string> slugs)
    {
        s_Slugs = new array<string>();
        if (!slugs)
            return;

        for (int i = 0; i < slugs.Count(); i++)
            s_Slugs.Insert(slugs[i]);
    }

    static int Count()
    {
        if (!s_Slugs)
            return 0;
        return s_Slugs.Count();
    }

    static string At(int i)
    {
        if (!s_Slugs || i < 0 || i >= s_Slugs.Count())
            return "";
        return s_Slugs[i];
    }
}

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

    // Ім'я віджета підказки для кожної панелі -- називає сама панель.
    protected ref map<string, string> m_PaneHints;

    // Перелiк редагованих конфiгiв з сервера: iм'я + власник (core/pda).
    protected ref array<string> m_CfgNames;
    protected ref array<string> m_CfgOwners;

    // ЧЕРГА cfg_get: рiвно один запит у польотi.
    //
    // Причина, з якої вона з'явилась, ЗНЯТА 2026-09-01: частини склеювались
    // за ключем «сторiнка|операцiя», i два одночасних cfg_get змiшували свої
    // шматки в одну кашу (змiряно 2026-08-30: Factions на три чанки не
    // розбирався, поки поруч летiв другий запит). Тепер ключ -- номер
    // повiдомлення, i змiшатись вони не можуть.
    //
    // Черга ЛИШАЄТЬСЯ, i це не забутий код: конфiги -- десятки кiлобайт, а
    // черга рiвняє навантаження й тримає вiдповiдi в передбачуваному порядку.
    // Тепер це вибiр, а не обхiд дефекту.
    protected ref array<string> m_CfgQ;
    protected bool m_CfgBusy = false;

    protected bool m_Ears = false;

    // Перемальовування списку кличе SelectRow, а якщо рушiй вiдповiсть на
    // нього OnItemSelected -- вийде рекурсiя. Прапорець рве це коло.
    protected bool m_Repaint = false;

    // ------------------------------------------------- спавни
    protected ref OZ_SpawnsConfig m_SpawnsCfg;
    protected int m_SpFacAt = 0;

    // Рядок списку -> що це таке i чим воно зветься. Без цього рядки були
    // iнертнi: показану особисту точку з цього екрана не було чим торкнути.
    protected ref array<string> m_SpawnRowKind;
    protected ref array<string> m_SpawnRowKey;

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
        m_PaneHints = new map<string, string>();
        m_CfgNames  = new array<string>();
        m_CfgOwners = new array<string>();
        m_CfgQ      = new array<string>();
        m_RawRows = new array<string>();
        m_SpawnRowKind = new array<string>();
        m_SpawnRowKey  = new array<string>();
        m_NwVoices = new array<string>();
        m_NwPick   = 0;
        m_NwSelf   = "";

        RegisterPane("spawns",   "SPAWNS",   M_SUB_WIDGET.FindAnyWidget("PaneSpawns"), "SpawnHint");
        RegisterPane("raw",      "RAW JSON", M_SUB_WIDGET.FindAnyWidget("PaneRaw"),    "RawHint");
        RegisterPane("news",     "NEWS",     M_SUB_WIDGET.FindAnyWidget("PaneNews"),   "NewsHint");

        if (!m_Ears)
        {
            m_Ears = true;
            OZ_ClientState.AdminWatch().Insert(this.OnAdminResponse);
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
            OZ_ClientState.AdminWatch().Remove(this.OnAdminResponse);
        }

        if (M_SUB_WIDGET)
            M_SUB_WIDGET.Unlink();
    }

    // Вкладка: кнопка з окремої розмiтки, панель -- вiд того, хто реєструє.
    // Субмод КПК кличе це саме з modded OnCreate.
    // ------------------------------------------------------------ NEWS
    //
    // Розділ адміна (ТЗ-6 R2.2). Список імен, якими він може підписати, дає
    // МІСТ (news_voices); панель його лише малює й по колу перебирає. Вона
    // не вирішує прав і не вигадує імен: список -- підказка, грант -- факт,
    // і маршрут запису перевіряє це ще раз (R3.2).
    protected ref array<string> m_NwVoices;
    protected int               m_NwPick;
    protected string            m_NwSelf;

    protected void RegisterPane(string id, string label, Widget pane, string hintName = "")
    {
        if (!pane)
            return;

        m_PaneHints.Set(id, hintName);

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
    //
    // Гiлки "factions" тут БIЛЬШЕ НЕМАЄ: ростер просить та панель, яка його
    // малює, i ядро про неї не знає. Поки цей рядок стояв тут, ядро питало
    // ростер у розділу, якого без мода фракцiй не iснує.
    protected void OnPaneShown(string id)
    {
        if (id == "spawns")
            AskCfg("Spawns");
        if (id == "raw")
            Ask(OZ_AdminSect.CONFIG, "cfg_list", "{}");
        if (id == "news")
            Ask(OZ_AdminSect.NEWS, OZ_NewsOp.VOICES, "{}");
    }

    override void OnMenuShow()
    {
        super.OnMenuShow();
        if (M_SUB_WIDGET)
            M_SUB_WIDGET.SetSort(1000);
        Ask(OZ_AdminSect.CONFIG, "cfg_list", "{}");

        // ПЕРША ЗАРЕЄСТРОВАНА, а не названа рядком.
        //
        // Тут стояло ShowPane("factions") -- панель, яку ядро не реєструє. Без
        // @OpenZone_Factions_VPP вiкно вiдкривалось ПОРОЖНIМ: жодна панель не
        // показана, CurrentPane() повертає порожнiй рядок, i всi
        // повiдомлення -- включно з рядком помилки -- летiли в нiкуди.
        if (m_TabIds.Count() > 0)
            ShowPane(m_TabIds[0]);
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

    // Один розділ на виклик, і розділ називає ВИКЛИКАЧ. Панель фракцій просить
    // свій "factions", ядрова половина -- "config" і "spawns". Поки адреса
    // була захована тут і завжди дорівнювала "admin", склейка фракцій слала
    // ростер туди, де про нього не чули, і мовчала про це.
    protected void Ask(string sectionId, string op, string json)
    {
        OZ_Rpc.AdminRequest(sectionId, op, json);
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
        Ask(OZ_AdminSect.CONFIG, "cfg_get:" + m_CfgQ[0], "{}");
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
        Ask(OZ_AdminSect.CONFIG, "cfg_set:" + name, body);
    }

    // Пiдказка -- у рядок ТIЄЇ панелi, що на екранi.
    //
    // Iм'я вiджета називає САМА панель у RegisterPane. Перебiр вiдомих iмен
    // лишився запасним ходом для панелей, зареєстрованих старим викликом, i
    // саме через нього панель рацiї (RadHint) викидала всi свої пiдказки: її
    // iм'я в перелiку ядра нiколи не значилось, а ядро й не мусить знати
    // iмена вiджетiв чужих модiв.
    protected void Hint(string t)
    {
        string id = CurrentPane();
        Widget p = m_Panes.Get(id);
        if (!p)
            return;

        TextWidget h;

        string named = "";
        if (m_PaneHints.Find(id, named) && named != "")
            h = TextWidget.Cast(p.FindAnyWidget(named));

        if (!h)
            h = TextWidget.Cast(p.FindAnyWidget("SpawnHint"));
        if (!h)
            h = TextWidget.Cast(p.FindAnyWidget("RawHint"));
        if (!h)
            h = TextWidget.Cast(p.FindAnyWidget("FacHint"));
        if (!h)
            h = TextWidget.Cast(p.FindAnyWidget("PdaHint"));
        if (!h)
            h = TextWidget.Cast(p.FindAnyWidget("RadHint"));

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

    void OnAdminResponse(string sectionId, string op, bool ok, string json, string error)
    {
        // Спавни -- ТЕЖ НАШI, i це нове.
        //
        // Ранiше вiдповiдь на SPAWN HERE приходила рольовим конвертом, i
        // пiдписаний на неї був лише pbo фракцiй. Без нього натискання не
        // давало нi пiдказки, нi помилки, нi оновлення списку: успiх був
        // невiдрiзненний вiд мовчазної вiдмови.
        if (sectionId == OZ_AdminSect.SPAWNS)
        {
            OnSpawnAnswer(op, ok, error);
            return;
        }

        if (sectionId == OZ_AdminSect.NEWS)
        {
            OnNewsAnswer(op, ok, json, error);
            return;
        }

        if (sectionId != OZ_AdminSect.CONFIG)
            return;

        if (!ok)
        {
            // Вiдмова на cfg_get мусить звiльнити чергу, iнакше вона стане.
            if (op.IndexOf("cfg_get:") == 0)
                CfgDone(op.Substring(8, op.Length() - 8));
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

    }

    // Вiдповiдь на спавнову операцiю: сказати, що вийшло, i перечитати список.
    protected void OnSpawnAnswer(string op, bool ok, string error)
    {
        if (!IsOpen())
            return;

        if (!ok)
        {
            // ВІДМОВУ ЛИШАЄМО НА ЕКРАНІ й списку не чіпаємо.
            //
            // Тут стояло перечитування «в обох випадках, щоб адмін бачив, що
            // нічого не змінилось». Ціною була сама причина: відповідь на той
            // cfg_get приходила за мить і писала в той самий рядок своє
            // «Spawns loaded», а рядок один. Змiряно на стендi: слаг "zzz"
            // мовчки не додався, i єдиним слiдом операцiї було слово
            // «loaded». Відмова нічого не змінила -- отже й перечитувати
            // нічого.
            Hint(op + ": " + Widget.TranslateString("#" + error));
            return;
        }

        Hint(op + ": done");
        AskCfg("Spawns");
    }

    // Відповідь розділу NEWS: список імен або результат публікації.
    protected void OnNewsAnswer(string op, bool ok, string json, string error)
    {
        if (!IsOpen())
            return;

        if (!ok)
        {
            Hint(op + ": " + Words(error));
            return;
        }

        if (op == OZ_NewsOp.VOICES)
        {
            OZ_NewsAdminVoices v;
            string verr;
            if (!JsonFileLoader<OZ_NewsAdminVoices>.LoadData(json, v, verr) || !v)
            {
                Hint("voices: unreadable answer");
                return;
            }

            m_NwSelf = v.Self;
            m_NwVoices.Clear();
            if (v.Voices)
            {
                for (int i = 0; i < v.Voices.Count(); i++)
                    m_NwVoices.Insert(v.Voices[i]);
            }

            // Вибір не зберігаємо між відповідями: список міг змінитись, і
            // старий індекс показував би одне ім'я, а підписував інше.
            m_NwPick = 0;
            PaintNewsWho();

            if (m_NwVoices.Count() == 0)
                Hint("no personas granted to you; posts go under your own name");
            else
                Hint(m_NwVoices.Count().ToString() + " persona(s) available");
            return;
        }

        if (op == OZ_NewsOp.POST)
        {
            OZ_NewsAdminAnswer a;
            string aerr;
            string who = "";
            if (JsonFileLoader<OZ_NewsAdminAnswer>.LoadData(json, a, aerr) && a)
                who = a.Who;

            SetEdit("NwTitle", "");
            MultilineEditBoxWidget body = MultilineEditBoxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("NwBody"));
            if (body)
                body.SetText("");

            Hint("posted as \"" + who + "\"");
            return;
        }
    }

    // Помилка -- або ключ таблиці рядків, або слова моста. Міст відмовляє
    // словами (not_your_voice, no_title), і перекладати їх нема куди: показуємо
    // як є, а ключі -- через таблицю.
    protected string Words(string error)
    {
        if (error.IndexOf("STR_") == 0)
            return Widget.TranslateString("#" + error);
        return error;
    }

    // Кнопка підпису показує поточний вибір: нуль -- своє ім'я.
    protected void PaintNewsWho()
    {
        TextWidget t = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("BtnNwWhoText"));
        if (!t)
            return;

        if (m_NwPick <= 0 || m_NwPick > m_NwVoices.Count())
        {
            m_NwPick = 0;
            if (m_NwSelf != "")
                t.SetText(m_NwSelf + "  (myself)");
            else
                t.SetText("myself");
            return;
        }

        t.SetText(m_NwVoices[m_NwPick - 1] + "  (persona)");
    }

    protected string PickedVoice()
    {
        if (m_NwPick <= 0 || m_NwPick > m_NwVoices.Count())
            return "";
        return m_NwVoices[m_NwPick - 1];
    }

    // Текст конфiгу приїхав. Субмод КПК перехоплює свої iмена через super.
    protected void OnCfgText(string name, string body)
    {
        // НЕ return: той самий Spawns може бути потрiбен ОБОМ панелям --
        // списку зон i сирому редактору, якщо в ньому вибрано саме його.
        // Ранiше тут стояв ранній вихiд, i через нього єдиний рядок списку
        // RAW JSON (а «Spawns» -- єдиний ядровий конфiг у ньому) не мiг
        // наповнити поле редактора НIКОЛИ: клiк перемальовував список зон,
        // поле лишалось порожнiм, а APPLY слав порожнiй рядок як cfg_set.
        if (name == "Spawns")
        {
            OZ_SpawnsConfig sc;
            string serr;
            if (JsonFileLoader<OZ_SpawnsConfig>.LoadData(body, sc, serr) && sc)
            {
                m_SpawnsCfg = sc;
                RebuildSpawnList();
            }
        }

        // Сирий редактор -- окремим питанням, а не «iнакше».
        if (name == m_RawPicked)
        {
            MultilineEditBoxWidget ed = MultilineEditBoxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("RawEdit"));
            if (ed)
                ed.SetText(body);

            // Пiдказку -- ЛИШЕ коли редактор на екранi. Рядок пiдказки в
            // панелi один, i «Spawns loaded», написане поверх повiдомлення
            // сусiдньої панелi, стирає саме те, заради чого воно писалось.
            if (CurrentPane() == "raw")
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

    // ---------------------------------------------------------- спавни

    protected void RebuildSpawnList()
    {
        TextListboxWidget lb = TextListboxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("SpawnList"));
        if (!lb || !m_SpawnsCfg)
            return;

        m_Repaint = true;

        lb.ClearItems();
        m_SpawnRowKind.Clear();
        m_SpawnRowKey.Clear();

        // СТЕЙДЖИНҐ -- ПЕРШИМ РЯДКОМ, i навiть коли його немає.
        //
        // Це точка найпершої появи персонажа, i досi її не було видно у
        // вкладцi зовсiм: задати чи зняти її можна було лише з карти КПК.
        // Рядок «staging   (not set)» вiдповiдає на питання, яке iнакше
        // вимагало б читати файл руками.
        string stg = "staging   (not set)";
        if (m_SpawnsCfg.Staging && m_SpawnsCfg.Staging.Center != "")
            stg = "staging   " + m_SpawnsCfg.Staging.Center + "   r=" + m_SpawnsCfg.Staging.Radius.ToString();
        lb.AddItem(stg, NULL, 0);
        m_SpawnRowKind.Insert("zone");
        m_SpawnRowKey.Insert("*");

        if (m_SpawnsCfg.Zones)
        {
            for (int i = 0; i < m_SpawnsCfg.Zones.Count(); i++)
            {
                OZ_SpawnZone z = m_SpawnsCfg.Zones[i];
                string slug = z.Role;
                string shown = slug;
                if (shown == "")
                    shown = "- (fallback)";
                string line = shown + "   " + z.Center + "   r=" + z.Radius.ToString();
                lb.AddItem(line, NULL, 0);
                m_SpawnRowKind.Insert("zone");
                // Порожнiй слаг у полi -- це «нiчого не набрано», тобто
                // циклер. Пишемо "-", який сервер розумiє як порожнiй.
                if (slug == "")
                    slug = "-";
                m_SpawnRowKey.Insert(slug);
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
                m_SpawnRowKind.Insert("uid");
                m_SpawnRowKey.Insert(p.Uid);
            }
        }

        m_Repaint = false;
    }

    protected void PaintSpawnCycler()
    {
        TextWidget t = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("BtnSpFacText"));
        if (!t)
            return;
        t.SetText("cycler: " + SpawnSlugAt(m_SpFacAt));
    }

    // ЩО САМЕ поїде в операцiю: поле сильнiше за циклер.
    //
    // Циклер перелiчує вiдоме -- органiзацiї й базовi фракцiї з ростера. Поле
    // приймає те, чого в перелiку немає й бути не може: "*" (стейджинґ), "-"
    // (запасна зона) i будь-який слаг, про який ця збiрка ще не чула. Рiвно
    // той самий договiр, що був на картi КПК, де полем слага служило поле
    // iменi мiтки, -- лише тепер вiн живе там, де решта спавнiв.
    protected string PickedSlug()
    {
        string typed = GetEdit("SpSlug");

        // Пробiли з країв: людина набирає в полi, i « duty» рiзалось би на
        // порожнiй слаг -- тобто мовчки переносило б ЗАПАСНУ зону.
        while (typed.Length() > 0 && typed.Substring(0, 1) == " ")
            typed = typed.Substring(1, typed.Length() - 1);
        while (typed.Length() > 0 && typed.Substring(typed.Length() - 1, 1) == " ")
            typed = typed.Substring(0, typed.Length() - 1);

        if (typed != "")
            return typed;

        return SpawnSlugAt(m_SpFacAt);
    }

    protected string SpawnSlugAt(int at)
    {
        // Останнiй пункт циклу -- запасна зона "-". Без мода фракцiй перелiк
        // порожнiй, i вона лишається єдиним варiантом.
        int n = OZ_VppFactionSlugs.Count();
        if (n == 0 || at >= n)
            return "-";
        return OZ_VppFactionSlugs.At(at);
    }

    protected void CycleSpawnFaction()
    {
        int n = OZ_VppFactionSlugs.Count();
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

        if (nm == "RawList")
        {
            if (row >= 0 && row < m_RawRows.Count())
            {
                m_RawPicked = m_RawRows[row];
                AskCfg(m_RawPicked);
            }
            return true;
        }

        // Рядок списку спавнiв КЛАДЕ СЕБЕ В ПОЛЕ, а не робить нiчого.
        //
        // Обробника в цього списку не було жодного, тож єдиний спосiб зняти
        // показану особисту точку був -- набрати Steam64 з екрана руками.
        // Тепер клiк заповнює те поле, якого стосується рядок, i друга дiя
        // (CLEAR) б'є саме туди, куди дивиться адмiн.
        if (nm == "SpawnList")
        {
            if (row >= 0 && row < m_SpawnRowKind.Count())
            {
                if (m_SpawnRowKind[row] == "uid")
                {
                    SetEdit("SpUid", m_SpawnRowKey[row]);
                    Hint("player " + m_SpawnRowKey[row] + " picked");
                }
                else
                {
                    SetEdit("SpSlug", m_SpawnRowKey[row]);
                    Hint("zone \"" + m_SpawnRowKey[row] + "\" picked");
                }
            }
            return true;
        }

        return super.OnItemSelected(w, x, y, row, column, oldRow, oldColumn);
    }

    override bool OnChange(Widget w, int x, int y, bool finished)
    {
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

        if (nm == "BtnSpFac")
        {
            CycleSpawnFaction();
            return true;
        }

        if (nm == "BtnSpHere" || nm == "BtnSpClear")
        {
            string slug = PickedSlug();
            string zarg = slug;

            if (nm == "BtnSpHere")
            {
                string zrad = GetEdit("SpRadius");
                if (zrad != "")
                    zarg += " " + zrad;
                Ask(OZ_AdminSect.SPAWNS, OZ_SpawnOp.HERE, zarg);
            }
            else
            {
                Ask(OZ_AdminSect.SPAWNS, OZ_SpawnOp.CLEAR, slug);
            }
            return true;
        }

        // ОСОБИСТА ТОЧКА -- звiдси, а не з панелi фракцiй.
        //
        // Вона про спавн, а не про фракцiю, i мусить бути там, де решта
        // спавнiв: на серверi без мода фракцiй її не було де поставити
        // взагалi, хоч сам механiзм -- ядровий.
        if (nm == "BtnSpUidHere" || nm == "BtnSpUidClear")
        {
            string uid = GetEdit("SpUid");
            if (uid == "")
            {
                Hint("type a Steam64 first");
                return true;
            }

            if (nm == "BtnSpUidHere")
            {
                string urad = GetEdit("SpRadius");
                string uarg = uid;
                if (urad != "")
                    uarg += " " + urad;
                Ask(OZ_AdminSect.SPAWNS, OZ_SpawnOp.UID_HERE, uarg);
            }
            else
            {
                Ask(OZ_AdminSect.SPAWNS, OZ_SpawnOp.UID_CLEAR, uid);
            }
            return true;
        }

        if (nm == "BtnNwWho")
        {
            m_NwPick++;
            if (m_NwPick > m_NwVoices.Count())
                m_NwPick = 0;
            PaintNewsWho();
            return true;
        }

        if (nm == "BtnNwPost")
        {
            string title = GetEdit("NwTitle");
            string text  = "";
            MultilineEditBoxWidget nb = MultilineEditBoxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("NwBody"));
            if (nb)
                nb.GetText(text);

            // Порожнє відхиляємо ТУТ, до мосту: він відмовив би тими ж
            // словами, але за круг через сервер, і адмін чекав би на відповідь
            // про те, що бачить сам.
            if (title.Trim() == "")
            {
                Hint("a title first");
                return true;
            }
            if (text.Trim() == "")
            {
                Hint("the body is empty");
                return true;
            }

            OZ_NewsAdminAsk ask = new OZ_NewsAdminAsk();
            ask.Who   = PickedVoice();
            ask.Title = title;
            ask.Body  = text;

            string letter;
            string lerr;
            if (!JsonFileLoader<OZ_NewsAdminAsk>.MakeData(ask, letter, lerr, false))
            {
                Hint("cannot build the post: " + lerr);
                return true;
            }

            Hint("posting...");
            Ask(OZ_AdminSect.NEWS, OZ_NewsOp.POST, letter);
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
