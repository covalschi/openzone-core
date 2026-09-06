// Вкладка «OpenZone» у VPP Admin Tools: цей pbo реєструє рівно три
// панелі -- SPAWNS, RAW JSON, NEWS. Панель іншого мода додає СКЛЕЙКА --
// окремий pbo, що чіпляється сюди через modded class, де точки
// розширення позначені словом protected. Панель FACTIONS, наприклад,
// приїжджає з OpenZone_Factions_VPP, з репозиторію фракцій.
//
// Клієнтський UI i НІЧОГО більше: кожна дія їде на сервер АДМІНСЬКИМ
// конвертом ядра (OZ_Rpc.AdminRequest у розділ "config" чи "spawns"), i
// кожну сервер перевіряє САМ -- OZ_Perm.IsAdmin першим рядком диспетчера.
//
// Досі це був конверт СТОРІНОК на сторінку "admin", i звідти ж росли три з
// чотирьох поломок вкладки: панель фракцій адресувала свій ростер на
// "admin", де про нього не чули; за цим стояв гейт КПК, який пропускав саме
// й тільки "admin"; а роль-конверт, яким їхали спавни, обробляв мод фракцій,
// тобто без нього панель SPAWNS говорила в порожнечу.
//
// Пастки VPP (зміряно в zp-research, повторено i доповнено тут):
//  - обидва гарди обов'язкові (NO_GUI валить сервер, AVPPAdminTools -- то
//    ім'я класу CfgMods, не CfgPatches);
//  - super.DefineButtons(), інакше зникають рідні кнопки;
//  - перший аргумент InsertButton = ім'я права I класу підменю; право
//    реєструє сервер ядра (OZ_Perm);
//  - віджет ЖИВЕ В БАЗОВОМУ M_SUB_WIDGET: базові Show/Hide/OnUpdate
//    працюють з ним, власне поле лишає базі NULL i диаг-збірка висне;
//  - вкладені лапки в text-значенні розмітки вішають парсер НАМЕРТВО
//    (бісекція 2026-08-30);
//  - сорт свій: HideBrokenWidgets приходить лише при ЗМІНІ порядку.

#ifdef AVPPAdminTools
#ifndef NO_GUI

// Слаги фракцій ДЛЯ ПАНЕЛІ СПАВНіВ -- маленький шов, а не знання.
//
// Зона спавну може належати фракції, тож циклер мусить показати перелік. Але
// звідки той перелік узявся -- справа мода фракцій, якого може й не бути:
// тоді список порожній, циклер показує саму лише запасну зону "-", i панель
// працює далі -- слаг набирається в полі поруч.
//
// Заповнює його склейка @OpenZone_Factions_VPP, коли отримує ростер. До
// 2026-09-01 Set() не кликав НІХТО в усьому дереві, тож циклер вічно стояв
// на "-", i завести фракційну зону з вкладки не можна було взагалі.
class OZ_VppFactionSlugs
{
    // ПУБЛІЧНИЙ статик: панель фракцій кладе сюди свої слаги зі свого pbo,
    // а ядро про фракції не знає й знати не мусить.
    static ref array<string> Slugs = new array<string>();

    // Копію робимо СВОЮ: масив приходить із чужого pbo, і жити він може
    // рівно доти, доки живе його панель.
    static void Set(array<string> slugs)
    {
        Slugs.Clear();
        if (!slugs)
            return;

        for (int i = 0; i < slugs.Count(); i++)
            Slugs.Insert(slugs[i]);
    }

    // Count() І At() ТУТ БІЛЬШЕ НЕМАЄ: лічильник і захищений індекс навколо
    // масиву, який сам має і Count(), і межі. Читач у цього списку один, і він
    // за два рядки нижче.
}

modded class VPPAdminHud
{
    override void DefineButtons()
    {
        super.DefineButtons();
        InsertButton("OZ_VppAdminMenu", "OpenZone", "set:dayz_gui_vpp image:vpp_icon_xml_editor", "OpenZone: spawns, configs, news");
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

    // Перелік редагованих конфігів з сервера: ім'я + власник (core/pda).
    protected ref array<string> m_CfgNames;
    protected ref array<string> m_CfgOwners;

    // ЧЕРГА cfg_get: рівно один запит у польоті.
    //
    // Причина, з якої вона з'явилась, ЗНЯТА 2026-09-01: частини склеювались
    // за ключем «сторінка|операція», i два одночасних cfg_get змішували свої
    // шматки в одну кашу (зміряно 2026-08-30: Factions на три чанки не
    // розбирався, поки поруч летів другий запит). Тепер ключ -- номер
    // повідомлення, i змішатись вони не можуть.
    //
    // Черга ЛИШАЄТЬСЯ, i це не забутий код: конфіги -- десятки кілобайт, а
    // черга рівняє навантаження й тримає відповіді в передбачуваному порядку.
    // Тепер це вибір, а не обхід дефекту.
    protected ref array<string> m_CfgQ;
    protected bool m_CfgBusy = false;

    protected bool m_Ears = false;

    // Перемальовування списку кличе SelectRow, а якщо рушій відповість на
    // нього OnItemSelected -- вийде рекурсія. Прапорець рве це коло.
    protected bool m_Repaint = false;

    // ------------------------------------------------- спавни
    protected ref OZ_SpawnsConfig m_SpawnsCfg;
    protected int m_SpFacAt = 0;

    // Рядок списку -> що це таке i чим воно зветься. Без цього рядки були
    // інертні: показану особисту точку з цього екрана не було чим торкнути.
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

        AddOwnPane("spawns", "SPAWNS",   "SpawnHint");
        AddOwnPane("raw",    "RAW JSON", "RawHint");
        AddOwnPane("news",   "NEWS",     "NewsHint");

        if (!m_Ears)
        {
            m_Ears = true;
            OZ_ClientState.AdminWatch().Insert(this.OnAdminResponse);
        }

        // Панель покаже OnMenuShow: ShowSubMenu приходить одразу після
        // OnCreate, а подвійний показ подвоював би cfg_get.
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

    // Вкладка: кнопка з окремої розмітки, панель -- від того, хто реєструє.
    // Субмод КПК кличе це саме з modded OnCreate.
    // ------------------------------------------------------------ NEWS
    //
    // Розділ адміна (ТЗ-6 R2.2). Список імен, якими він може підписати, дає
    // МІСТ (news_voices); панель його лише малює й по колу перебирає. Вона
    // не вирішує прав і не вигадує імен: список -- підказка, грант -- факт,
    // і маршрут запису перевіряє це ще раз (R3.2).
    // Дзеркало чату (ТЗ-2 §8): стан приходить mirror_list, перемикання --
    // mirror_set у два натискання, бо вмикання заливає в Discord усю
    // історію, а це дія назовні.
    protected bool m_MirrorKnown = false;
    protected bool m_MirrorOn    = false;
    protected bool m_MirrorArmed = false;

    // Дзеркало ролей (ТЗ-2 §15): той самий перемикач у два натискання для
    // роду "roles". Увімкнення змушує бота переписати ролі Discord зі своїх
    // таблиць -- дія назовні, як i заливка чату.
    protected bool m_RolesKnown = false;
    protected bool m_RolesOn    = false;
    protected bool m_RolesArmed = false;

    protected ref array<string> m_NwVoices;
    protected int               m_NwPick;
    protected string            m_NwSelf;

    // Зазор між вкладками в одиницях розмітки -- дзеркало vpp.tabGap із
    // ui/tokens.json. Рушій того файлу не читає, тому число живе двічі:
    // змінюєш токен -- зміни й цю сталу.
    static const float TAB_GAP = 6;

    // Колір напису вкладки бере ПАЛІТРА ЯДРА (OZ_Palette), а не літерал тут.
    // Та сама пара стояла й у OZ_PdaMenu.c, і в панелі фракцій: одне число в
    // трьох репозиторіях розходиться мовчки, бо ніщо не звіряє його з
    // ui/tokens.json.

    // Розмір шаблону вкладки в ЕКРАННИХ ПІКСЕЛЯХ (не в одиницях: GetSize на
    // віджеті точного розміру відповідає пікселями, зміряно 2026-09-06 --
    // 222.222 px на 3840x1600 і 150 px на 1920x1080 з тих самих оголошених
    // 150 одиниць; довший розбір -- у коментарі LayoutTabs). Знятий ОДИН РАЗ
    // із першої ж створеної кнопки -- до того, як до неї хоч раз доторкнувся
    // ShrinkTab (RegisterPane нижче). GetSize кнопки ПІСЛЯ SetSize, яким
    // ShrinkTab її стискає, -- поведінка, якої рушій не документує і ніхто не
    // міряв, тому LayoutTabs більше не читає розмір шаблону з m_TabBtns[0]
    // живцем: лише з цих двох полів, які раз записані вже не міняються.
    protected float m_TabW = 0;

    // Перший вдалий прохід LayoutTabs ще не стався (сторож нижче): до нього
    // GetScreenSize повертає 0 і для смуги, і для щойно створеної кнопки, а
    // ShowSubMenu приходить одразу після OnCreate -- раніше першого кадру
    // рушія. LayoutTabs виставляє прапорець замість того, щоб здатися
    // назавжди; MissionGameplay.OnUpdate внизу файлу пробує ще раз щокадру,
    // поки вікно відкрите, аж доки прохід не вдасться і сам його не згасить.
    protected bool m_TabsPending = false;

    // Своя панель -- окремою розміткою, точно як у склейок: створити під
    // коренем і зареєструвати. Ім'я файлу росте з того самого id, тож
    // розійтися їм нема де.
    protected void AddOwnPane(string id, string label, string hintName)
    {
        string layout = "OpenZone_VPP/gui/layouts/oz_vpp_pane_" + id + ".layout";

        Widget pane = GetGame().GetWorkspace().CreateWidgets(layout, M_SUB_WIDGET);
        if (!pane)
        {
            OZ_Log.Error("vpp pane: layout failed to load: " + layout);
            return;
        }

        RegisterPane(id, label, pane, hintName);
    }

    // Смуга вкладок -- порожня рамка з розмітки вікна. Якщо її раптом немає
    // (старий .layout поруч із новим скриптом), вкладки лишаються дітьми
    // кореня, як було до 2026-09-05: гірше на вигляд, але не порожньо.
    protected Widget TabStrip()
    {
        if (!M_SUB_WIDGET)
            return null;

        Widget strip = M_SUB_WIDGET.FindAnyWidget("TabStrip");
        if (strip)
            return strip;

        return M_SUB_WIDGET;
    }

    // Обидві ранні здачі тепер КАЖУТЬ ПРО СЕБЕ (лог) і не лишають сироти:
    // до 2026-09-05 обидві мовчали, а підказку панелі записували ще ДО
    // перевірки кнопки -- отже запис у m_PaneHints без пари в m_TabIds/
    // m_TabBtns/m_Panes лишався, якщо шаблон вкладки не завантажився.
    protected void RegisterPane(string id, string label, Widget pane, string hintName = "")
    {
        if (!pane)
        {
            OZ_Log.Error("vpp pane: no widget to register: " + id);
            return;
        }

        Widget btn = GetGame().GetWorkspace().CreateWidgets("OpenZone_VPP/gui/layouts/oz_vpp_tab.layout", TabStrip());
        if (!btn)
        {
            OZ_Log.Error("vpp tab: layout failed to load: " + id);
            return;
        }

        // Розмір шаблону -- з ПЕРШОЇ кнопки, яка тут коли-небудь з'явилась,
        // до того, як LayoutTabs нижче встигне її стиснути (коментар біля
        // m_TabW вище).
        // Висота нам не потрібна -- смуга її задає сама, -- але GetSize
        // хоче обидва out-параметри, тож приймаємо її в локальну.
        float tabH;
        if (m_TabW <= 0)
            btn.GetSize(m_TabW, tabH);

        m_PaneHints.Set(id, hintName);

        btn.SetName("tab:" + id);

        TextWidget t = TextWidget.Cast(btn.FindAnyWidget("OZ_VppTabText"));
        if (t)
            t.SetText(label);

        m_TabIds.Insert(id);
        m_TabBtns.Insert(btn);
        m_Panes.Set(id, pane);

        LayoutTabs();
    }

    // Вкладки ділять смугу. Кімната -- ЕКРАННА ширина самої смуги, ширина
    // вкладки -- зі знятого один раз шаблону (m_TabW вище), зазор --
    // TAB_GAP; коли всі не влазять, ширина ділиться на всіх, і кнопка з
    // дітьми вужчає (ShrinkTab).
    //
    // УСЕ, ЩО ЧИТАЄТЬСЯ З ВІДЖЕТА ТОЧНОГО РОЗМІРУ, -- ВЖЕ ПІКСЕЛІ, І GetSize
    // ТЕЖ (зміряно 2026-09-06 на стенді). Смуга оголошена як 1000x28 одиниць,
    // а GetSize і GetScreenSize повертають про неї ОДНЕ Й ТЕ САМЕ число --
    // 1481.48 px; шаблон вкладки оголошений як 150 одиниць, а GetSize дає
    // 222.222 px. SetPos теж бере пікселі: GetPos повертає рівно те, що
    // подали, а GetScreenPos = екранний x батька плюс воно.
    //
    // Через це масштаб room/stripLw, який стояв тут до 2026-09-06, ЗАВЖДИ
    // дорівнював одиниці, і TAB_GAP -- єдине число тут, яке справді в
    // ОДИНИЦЯХ розмітки, бо дзеркалить vpp.tabGap, -- їхав на екран
    // непомасштабованим: 6 px замість 6·s = 8.9 px, тобто крок вкладок
    // виходив 228.2 замість 231.1 px на 3840x1600 (замір task-48a §5,
    // пояснений тут). Ширину вкладки та помилка не чіпала: m_TabW уже в
    // пікселях, множення на одиницю його не змінювало.
    //
    // Тому масштаб одиниць береться ІДІОМОЮ САМОГО РУШІЯ -- висота екрана
    // на 1080 (tabberui.c:48 `m_ResolutionMultiplier = y / 1080`,
    // inventorymenu.c:72, tutorialsmenu.c:15) -- і множить РІВНО TAB_GAP.
    // Ділення пишеться через float явно: у Enforce `int / int` -- цілочисельне,
    // і 1600/1080 дало б 1.
    //
    // Доти тут стояли п'ять піксельних літералів (160/8/20/52/40): на
    // 3840x1600 вони стискали вкладку зі 150 одиниць до 103 і клали смугу на
    // 35-й одиниці замість 52-ї, тобто вікно виглядало по-різному на різних
    // екранах.
    //
    // РАННІЙ ВИХІД НА НУЛІ, А НЕ НА ВІД'ЄМНИХ, І НЕ НАЗАВЖДИ: до першого
    // проходу розмітки GetScreenSize повертає 0 і для смуги, і для щойно
    // створеної кнопки (зміряно, gui-layouts.md §24, "widget measured before
    // its first layout pass"), а ShowSubMenu приходить одразу після OnCreate
    // (коментар в OnCreate) -- тобто й виклик з OnMenuShow часто встигає
    // раніше першого кадру рушія. Без цієї сторожі room == 0 робить want
    // від'ємним, SetPos тягне вкладки вліво від нуля, а ShrinkTab кличеться
    // з від'ємною шириною. Замість остаточної здачі гарда виставляє
    // m_TabsPending, і MissionGameplay.OnUpdate (сторож у кінці файлу)
    // пробує ще раз щокадру, поки вікно відкрите, аж доки прохід не вдасться;
    // виклик з OnMenuShow лишається -- він просто рідко буває першим, що
    // спрацював.
    protected void LayoutTabs()
    {
        int n = m_TabBtns.Count();
        Widget strip = TabStrip();
        if (n == 0 || !strip)
            return;

        float room, striph;
        strip.GetScreenSize(room, striph);

        // Ту саму смугу читаємо ДРУГИМ способом -- це доказ, на якому стоїть
        // уся арифметика нижче, і він мусить лишатись видимим у лозі.
        float stripLw, stripLh;
        strip.GetSize(stripLw, stripLh);

        int scrW, scrH;
        GetScreenSize(scrW, scrH);
        float scrHf = scrH;

        if (room <= 0 || m_TabW <= 0 || scrHf <= 0)
        {
            m_TabsPending = true;
            return;
        }

        float s    = scrHf / 1080.0;
        float gap  = TAB_GAP * s;
        float want = m_TabW;
        if (want * n + gap * (n - 1) > room)
            want = (room - gap * (n - 1)) / n;

        // ОДИН РЯДОК НА ПРОХІД -- арифметика смуги і те, що з неї вийшло.
        //
        // Він тут не для налагодження «поки що», а тому, що 2026-09-06 живий
        // замір спіймав розбіжність, якої з самого коду не видно: крок вкладок
        // мав бути 150·s + 6·s = 231.1 px при s = 1.4815 (3840x1600), а на
        // екрані вийшов рівно 228 px. Розрізнити «SetPos бере пікселі» від
        // «SetPos бере одиниці» здатні тільки два числа поруч -- ПОДАНЕ в
        // SetPos і ПРОЧИТАНЕ назад із GetPos/GetScreenPos, -- а вгадувати
        // рушій нам не можна. Саме цей рядок і показав, що room і stripLw
        // рівні, тобто старий scale завжди був одиницею.
        //
        // КОРОТКО НЕ ЗАРАДИ КРАСИ: рядок скриптового лога рушій ріже на 255
        // символах разом із префіксом (зміряно тут-таки -- перший варіант
        // цього рядка обривався на третій вкладці).
        string dbg = "vpp strip: room=" + room.ToString() + " lw=" + stripLw.ToString();
        dbg += " s=" + s.ToString() + " tabW=" + m_TabW.ToString();
        dbg += " want=" + want.ToString() + " gap=" + gap.ToString();

        float spx, spy;
        strip.GetScreenPos(spx, spy);
        dbg += " x0=" + spx.ToString();

        for (int i = 0; i < n; i++)
        {
            Widget b = m_TabBtns[i];
            float at = i * (want + gap);
            b.SetPos(at, 0);
            float bw, bh;
            b.GetScreenSize(bw, bh);
            if (bw > want)
                ShrinkTab(b, want);

            float lx, ly, sx, sy;
            b.GetPos(lx, ly);
            b.GetScreenPos(sx, sy);
            dbg += " |" + i.ToString() + " " + at.ToString() + "=" + lx.ToString() + ">" + sx.ToString();
        }

        OZ_Log.Dbg(dbg);

        m_TabsPending = false;
    }

    // Кнопка і всі її діти (рамка, тло, напис) вужчають на одне число.
    //
    // ПІКСЕЛІ, НЕ ОДИНИЦІ: SetSize на віджеті точного розміру пише ЕКРАННІ
    // пікселі (зміряно 2026-09-03, gui-layouts.md §24), тому ширина-ціль тут
    // приходить уже в пікселях (LayoutTabs більше не ділить на scale), і
    // дельта рахується від GetScreenSize, а не GetSize. Шлях спрацьовує лише
    // від сьомої вкладки (див. коментар LayoutTabs, три вкладки в ядрі) і на
    // живому стенді не перевірений -- жива перевірка ще належна.
    private void ShrinkTab(Widget b, float width)
    {
        float w, h;
        b.GetScreenSize(w, h);
        float d = w - width;
        b.SetSize(width, h);
        Widget c = b.GetChildren();
        while (c)
        {
            c.GetScreenSize(w, h);
            c.SetSize(w - d, h);
            c = c.GetSibling();
        }
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
                    t.SetColor(OZ_Palette.ACCENT);
                else
                    t.SetColor(OZ_Palette.MUTED);
            }
        }

        OnPaneShown(id);
    }

    // Панель показано -- свіжі дані. Субмод КПК довантажує своє тут.
    //
    // Гілки "factions" тут БІЛЬШЕ НЕМАЄ: ростер просить та панель, яка його
    // малює, i ядро про неї не знає. Поки цей рядок стояв тут, ядро питало
    // ростер у розділу, якого без мода фракцій не існує.
    protected void OnPaneShown(string id)
    {
        if (id == "spawns")
            AskCfg("Spawns");
        if (id == "raw")
        {
            Ask(OZ_AdminSect.CONFIG, "cfg_list", "{}");
            m_MirrorArmed = false;
            m_RolesArmed  = false;
            PaintMirror();
            Ask(OZ_AdminSect.CONFIG, "mirror_list", "{}");
        }
        if (id == "news")
            Ask(OZ_AdminSect.NEWS, OZ_NewsOp.VOICES, "{}");
    }

    override void OnMenuShow()
    {
        super.OnMenuShow();

        // Прохід розмітки смуги ЗВІДСИ: не єдиний і не завжди вдалий --
        // ShowSubMenu (тобто цей виклик) приходить одразу після OnCreate,
        // часто раніше першого кадру рушія, тож GetScreenSize може й тут
        // повернути 0 (коментар у LayoutTabs). Лишається як найраніша
        // спроба; коли вона падає на нулях, m_TabsPending передає естафету
        // MissionGameplay.OnUpdate.
        LayoutTabs();

        if (M_SUB_WIDGET)
            M_SUB_WIDGET.SetSort(1000);
        Ask(OZ_AdminSect.CONFIG, "cfg_list", "{}");

        // ПЕРША ЗАРЕЄСТРОВАНА, а не названа рядком.
        //
        // Тут стояло ShowPane("factions") -- панель, яку ядро не реєструє. Без
        // @OpenZone_Factions_VPP вікно відкривалось ПОРОЖНІМ: жодна панель не
        // показана, CurrentPane() повертає порожній рядок, i всі
        // повідомлення -- включно з рядком помилки -- летіли в нікуди.
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

    // Кличе MissionGameplay.OnUpdate (сторож у кінці файлу), поки перший
    // вдалий прохід LayoutTabs не стався. LayoutTabs сама гасить прапорець
    // по вдалому проходу, тож виклик після успіху -- порожній no-op.
    void RetryTabsLayoutIfPending()
    {
        if (m_TabsPending)
            LayoutTabs();
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
        // Тіло їде СИРИМ: конверт зі строковим полем різався б на 1023
        // байтах при серверному розборі (та сама пастка, що й у cfg_get).
        Ask(OZ_AdminSect.CONFIG, "cfg_set:" + name, body);
    }

    // Підказка -- у рядок ТІЄЇ панелі, що на екрані.
    //
    // Ім'я віджета називає САМА панель у RegisterPane. Перебір відомих імен
    // лишився запасним ходом для панелей, зареєстрованих старим викликом, i
    // саме через нього панель рації (RadHint) викидала всі свої підказки: її
    // ім'я в переліку ядра ніколи не значилось, а ядро й не мусить знати
    // імена віджетів чужих модів.
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

    // ---------------------------------------------------------- відповіді

    void OnAdminResponse(string sectionId, string op, bool ok, string json, string error)
    {
        // Спавни -- ТЕЖ НАШІ, i це нове.
        //
        // Раніше відповідь на SPAWN HERE приходила рольовим конвертом, i
        // підписаний на неї був лише pbo фракцій. Без нього натискання не
        // давало ні підказки, ні помилки, ні оновлення списку: успіх був
        // невідрізненний від мовчазної відмови.
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
            // Відмова на cfg_get мусить звільнити чергу, інакше вона стане.
            if (op.IndexOf("cfg_get:") == 0)
                CfgDone(op.Substring(8, op.Length() - 8));
            if (op.IndexOf("mirror_set:") == 0)
            {
                m_MirrorArmed = false;
                m_RolesArmed  = false;
                PaintMirror();
            }
            Hint("#" + error);
            return;
        }

        if (op == "mirror_list")
        {
            OZ_MirrorState mst;
            string merr;
            if (JsonFileLoader<OZ_MirrorState>.LoadData(json, mst, merr) && mst && mst.Mirrors)
            {
                m_MirrorKnown = true;
                m_MirrorOn    = false;
                m_RolesKnown  = true;
                m_RolesOn     = false;
                for (int mi = 0; mi < mst.Mirrors.Count(); mi++)
                {
                    if (!mst.Mirrors[mi])
                        continue;
                    if (mst.Mirrors[mi].Kind == "chat")
                        m_MirrorOn = mst.Mirrors[mi].Mirror;
                    if (mst.Mirrors[mi].Kind == "roles")
                        m_RolesOn = mst.Mirrors[mi].Mirror;
                }
                PaintMirror();
            }
            return;
        }

        if (op.IndexOf("mirror_set:") == 0)
        {
            OZ_MirrorReport mrep;
            string rerr;
            m_MirrorArmed = false;
            m_RolesArmed  = false;
            if (JsonFileLoader<OZ_MirrorReport>.LoadData(json, mrep, rerr) && mrep)
            {
                if (mrep.Kind == "roles")
                {
                    m_RolesKnown = true;
                    m_RolesOn    = mrep.On;
                }
                else
                {
                    m_MirrorKnown = true;
                    m_MirrorOn    = mrep.On;
                }
                string line = mrep.Kind + " mirror ";
                if (mrep.On)
                    line += "ON";
                else
                    line += "OFF";
                if (mrep.Skipped == 1 && mrep.Pushed == 0 && mrep.Failed == 0 && mrep.Note.IndexOf("already") == 0)
                    line += " (unchanged)";
                else
                    line += ": pushed " + mrep.Pushed.ToString() + ", skipped " + mrep.Skipped.ToString() + ", failed " + mrep.Failed.ToString();
                if (mrep.Note != "")
                    line += " - " + mrep.Note;
                Hint(line);
            }
            PaintMirror();
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
                RebuildRawList();
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

    // Відповідь на спавнову операцію: сказати, що вийшло, i перечитати список.
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
            // «Spawns loaded», а рядок один. Зміряно на стенді: слаг "zzz"
            // мовчки не додався, i єдиним слідом операції було слово
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

    // Текст конфігу приїхав. Субмод КПК перехоплює свої імена через super.
    protected void OnCfgText(string name, string body)
    {
        // НЕ return: той самий Spawns може бути потрібен ОБОМ панелям --
        // списку зон i сирому редактору, якщо в ньому вибрано саме його.
        // Раніше тут стояв ранній вихід, i через нього єдиний рядок списку
        // RAW JSON (а «Spawns» -- єдиний ядровий конфіг у ньому) не міг
        // наповнити поле редактора НІКОЛИ: клік перемальовував список зон,
        // поле лишалось порожнім, а APPLY слав порожній рядок як cfg_set.
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

        // Сирий редактор -- окремим питанням, а не «інакше».
        if (name == m_RawPicked)
        {
            MultilineEditBoxWidget ed = MultilineEditBoxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("RawEdit"));
            if (ed)
                ed.SetText(body);

            // Підказку -- ЛИШЕ коли редактор на екрані. Рядок підказки в
            // панелі один, i «Spawns loaded», написане поверх повідомлення
            // сусідньої панелі, стирає саме те, заради чого воно писалось.
            if (CurrentPane() == "raw")
                Hint(name + " loaded");
        }
    }

    // Після вдалого cfg_set перечитуємо те, що на екрані.
    protected void OnCfgApplied()
    {
        if (CurrentPane() == "spawns")
            AskCfg("Spawns");
    }

    // OnCfgListChanged ТУТ БІЛЬШЕ НЕМАЄ: обгортка в один рядок навколо
    // RebuildRawList, з одним викликачем і без жодного override у КПК,
    // фракціях чи рації.

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

        // СТЕЙДЖИНҐ -- ПЕРШИМ РЯДКОМ, i навіть коли його немає.
        //
        // Це точка найпершої появи персонажа, i досі її не було видно у
        // вкладці зовсім: задати чи зняти її можна було лише з карти КПК.
        // Рядок «staging   (not set)» відповідає на питання, яке інакше
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
                // Порожній слаг у полі -- це «нічого не набрано», тобто
                // циклер. Пишемо "-", який сервер розуміє як порожній.
                if (slug == "")
                    slug = "-";
                m_SpawnRowKey.Insert(slug);
            }
        }

        // Особисті точки -- у тому ж списку, з міткою гравця: адмін бачить
        // УСЕ, що впливає на спавни, на одному екрані.
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

    // ЩО САМЕ поїде в операцію: поле сильніше за циклер.
    //
    // Циклер перелічує відоме -- організації й базові фракції з ростера. Поле
    // приймає те, чого в переліку немає й бути не може: "*" (стейджинґ), "-"
    // (запасна зона) i будь-який слаг, про який ця збірка ще не чула. Рівно
    // той самий договір, що був на карті КПК, де полем слага служило поле
    // імені мітки, -- лише тепер він живе там, де решта спавнів.
    protected string PickedSlug()
    {
        string typed = GetEdit("SpSlug");

        // Пробіли з країв: людина набирає в полі, i « duty» різалось би на
        // порожній слаг -- тобто мовчки переносило б ЗАПАСНУ зону.
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
        // Останній пункт циклу -- запасна зона "-". Без мода фракцій перелік
        // порожній, i вона лишається єдиним варіантом.
        int n = OZ_VppFactionSlugs.Slugs.Count();
        if (n == 0 || at >= n)
            return "-";
        return OZ_VppFactionSlugs.Slugs[at];
    }

    protected void CycleSpawnFaction()
    {
        int n = OZ_VppFactionSlugs.Slugs.Count();
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

    // ---------------------------------------------------------- ввід

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

        // Рядок списку спавнів КЛАДЕ СЕБЕ В ПОЛЕ, а не робить нічого.
        //
        // Обробника в цього списку не було жодного, тож єдиний спосіб зняти
        // показану особисту точку був -- набрати Steam64 з екрана руками.
        // Тепер клік заповнює те поле, якого стосується рядок, i друга дія
        // (CLEAR) б'є саме туди, куди дивиться адмін.
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

    // OnChange ТУТ БІЛЬШЕ НЕМАЄ: override, який лише кликав super, тобто
    // рівно те, що станеться й без нього.

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

        // ОСОБИСТА ТОЧКА -- звідси, а не з панелі фракцій.
        //
        // Вона про спавн, а не про фракцію, i мусить бути там, де решта
        // спавнів: на сервері без мода фракцій її не було де поставити
        // взагалі, хоч сам механізм -- ядровий.
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

            // Довге тіло відхиляємо ТУТ, з числом: сервер розбирає зібраний
            // JSON через JsonFileLoader, а той ріже строкове значення на
            // 1023 байтах мовчки -- тобто без цієї перевірки адмін бачив би
            // «готово» на новину, яка поїхала в гільдію обрубком.
            if (text.Length() >= OZ_NewsAdminAsk.BODY_MAX)
            {
                Hint("the body is " + text.Length().ToString() + " b, the limit is " + OZ_NewsAdminAsk.BODY_MAX.ToString());
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

        if (nm == "BtnMirror")
        {
            MirrorClick("chat");
            return true;
        }

        if (nm == "BtnMirrorRoles")
        {
            MirrorClick("roles");
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

    protected void PaintMirror()
    {
        if (!M_SUB_WIDGET)
            return;

        TextWidget lbl = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("MirrorLabel"));
        if (lbl)
        {
            if (!m_MirrorKnown)
                lbl.SetText("chat mirror: ?");
            else if (m_MirrorOn)
                lbl.SetText("chat mirror: ON");
            else
                lbl.SetText("chat mirror: OFF");
        }

        TextWidget bt = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("BtnMirrorText"));
        if (bt)
        {
            if (m_MirrorArmed)
                bt.SetText("PRESS AGAIN");
            else if (m_MirrorKnown && m_MirrorOn)
                bt.SetText("MIRROR OFF");
            else
                bt.SetText("MIRROR ON");
        }

        TextWidget rl = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("MirrorRolesLabel"));
        if (rl)
        {
            if (!m_RolesKnown)
                rl.SetText("roles mirror: ?");
            else if (m_RolesOn)
                rl.SetText("roles mirror: ON");
            else
                rl.SetText("roles mirror: OFF");
        }

        TextWidget rb = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("BtnMirrorRolesText"));
        if (rb)
        {
            if (m_RolesArmed)
                rb.SetText("PRESS AGAIN");
            else if (m_RolesKnown && m_RolesOn)
                rb.SetText("ROLES OFF");
            else
                rb.SetText("ROLES ON");
        }
    }

    // Один перемикач, два роди. Перше натискання каже, що саме станеться,
    // друге робить (R5.2).
    protected void MirrorClick(string kind)
    {
        bool known = m_MirrorKnown;
        bool on    = m_MirrorOn;
        bool armed = m_MirrorArmed;
        if (kind == "roles")
        {
            known = m_RolesKnown;
            on    = m_RolesOn;
            armed = m_RolesArmed;
        }

        if (!known)
        {
            Hint("the mirror state has not arrived yet");
            Ask(OZ_AdminSect.CONFIG, "mirror_list", "{}");
            return;
        }

        if (!armed)
        {
            SetMirrorArmed(kind, true);
            PaintMirror();
            if (kind == "roles")
            {
                if (on)
                    Hint("turning the roles mirror OFF: the bot stops touching Discord roles, its tables stay the home - press again to confirm");
                else
                    Hint("turning the roles mirror ON: the bot creates the roles and rewrites every linked member's roles from its tables - press again to confirm");
            }
            else
            {
                if (on)
                    Hint("turning the chat mirror OFF: the bot stops writing, the Discord threads stay as an archive - press again to confirm");
                else
                    Hint("turning the chat mirror ON: the whole chat history goes into Discord threads first - press again to confirm");
            }
            return;
        }

        SetMirrorArmed(kind, false);
        PaintMirror();
        Hint("switching the " + kind + " mirror...");
        if (on)
            Ask(OZ_AdminSect.CONFIG, "mirror_set:" + kind + ":off", "{}");
        else
            Ask(OZ_AdminSect.CONFIG, "mirror_set:" + kind + ":on", "{}");
    }

    protected void SetMirrorArmed(string kind, bool armed)
    {
        if (kind == "roles")
            m_RolesArmed = armed;
        else
            m_MirrorArmed = armed;
    }

    // ---------------------------------------------------------- дрібне

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

// Вартовий: дві роботи, поки вікно відкрите. VPP ховає лише власний
// корінь, сироту на корені робочої області прибирає місія (OnUpdate
// підменю після ховання не тікає). Друга -- добиває розмітку смуги
// вкладок, поки перший вдалий прохід ще не стався (m_TabsPending,
// коментар у LayoutTabs).
modded class MissionGameplay
{
    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);

        if (!OZ_VppAdminMenu.s_Inst || !OZ_VppAdminMenu.s_Inst.IsOpen())
            return;

        OZ_VppAdminMenu.s_Inst.RetryTabsLayoutIfPending();

        VPPAdminHud hud = VPPAdminHud.Cast(GetGame().GetUIManager().FindMenu(VPP_ADMIN_HUD));
        if (!hud || !hud.IsShowing())
            OZ_VppAdminMenu.s_Inst.ForceHide();
    }
}

#endif
#endif
