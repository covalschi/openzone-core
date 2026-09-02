// Транспорт: ОДИН універсальний конверт замість RPC на кожну функцію.
//
// Причина проста: сторінок буде багато, і контракт, що росте з кожною новою,
// доведеться узгоджувати між ядром і кожним модом серії. Конверт із трьох
// рядків (сторінка, операція, JSON) не росте зовсім.
//
// CF мультиплексує все через один рушійний id, а ключем служить пара
// (ім'я мода, ім'я функції) -- зіткнення з чужим модом можливе лише при збігу
// ОБОХ рядків, чого не буває.

class OZ_Rpc
{
    static const string RPC_HELLO = "OZ_Hello";
    static const string RPC_SYNC  = "OZ_Sync";
    static const string RPC_REQ   = "OZ_Req";
    static const string RPC_RES   = "OZ_Res";

    // Частини довгого JSON: рушійний RPC псує рядки понад ~1024 байти
    // (див. OZ_Const.RPC_STR_CHUNK). Частини йдуть попереду свого конверта.
    // Порядок доставки гарантований каналом: guaranteed-RPC приїздять у
    // порядку відправки.
    static const string RPC_REQ_PART = "OZ_ReqPart";
    static const string RPC_RES_PART = "OZ_ResPart";

    // НОМЕР ПОВІДОМЛЕННЯ -- те, чим склеюються частини.
    //
    // Раніше ключем була пара «сторінка + операція», і це ламалось рівно
    // тоді, коли два запити на ту саму пару летіли одночасно: їхні шматки
    // складались в один буфер, і обидва тіла гинули. Виміряно 2026-08-30 --
    // конфіг на три шматки не розбирався, поки поруч летів другий cfg_get, --
    // і вкладка VPP досі носить чергу «рівно один запит у польоті» саме
    // через це. Черга лікувала свій екран; правило ж стосується КОЖНОЇ
    // сторінки й будь-якого чужого мода.
    //
    // Номер НЕ пов'язує запит із відповіддю. Він означає лише «оці шматки й
    // отой конверт -- одне повідомлення», тому його роздає САМ ВІДПРАВНИК
    // кожній довгій посилці, включно з тими, яких ніхто не просив (пуші).
    private static int s_NextId = 1;

    private static int NextId()
    {
        s_NextId = s_NextId + 1;
        return s_NextId;
    }

    // Прив'язка має ВЛАСНУ пару, а не сторінку в реєстрі.
    //
    // Сторінки проходять крізь OZ_PageAccess, і КПК підміняє його перевіркою
    // «чи є ця сторінка на цьому пристрої». Прив'язка через сторінку означала
    // б, що прив'язатись може лише власник КПК із потрібною сторінкою в
    // профілі -- а прив'язка потрібна рації, квестам і ІІ так само, і екран
    // їм ні до чого.
    static const string RPC_LINK_REQ = "OZ_LinkReq";
    static const string RPC_LINK_RES = "OZ_LinkRes";

    // Зміна ролей -- ВЛАСНА пара, з тієї ж причини, що й прив'язка: сторінки
    // проходять крізь OZ_PageAccess, тобто просити зміг би лише власник КПК.
    // Лідер без КПК і адмін без КПК мусять могти те саме.
    static const string RPC_ROLE_REQ = "OZ_RoleReq";
    static const string RPC_ROLE_RES = "OZ_RoleRes";

    // Адмінська консоль -- ВЛАСНА пара, з тієї ж причини, що прив'язка й ролі,
    // тільки гостріше. Сторінки проходять крізь OZ_PageAccess, тобто крізь
    // перевірку «чи є ця сторінка на приладі в руках», і поки адмінські
    // розділи були сторінками, гейт КПК мусив тримати для них виняток першим
    // рядком. Виняток у межі безпеки -- це двері збоку; розділ фракцій, який
    // винятку не мав, не проходив узагалі й мовчав.
    //
    // Тіло їде частинами так само, як у сторінок: конфіг у кілька кілобайт --
    // звичайна відповідь цієї консолі, а рушійний RPC псує рядки понад ~1024.
    static const string RPC_ADMIN_REQ      = "OZ_AdminReq";
    static const string RPC_ADMIN_RES      = "OZ_AdminRes";
    static const string RPC_ADMIN_REQ_PART = "OZ_AdminReqPart";
    static const string RPC_ADMIN_RES_PART = "OZ_AdminResPart";

    // «Покажи це» -- сервер клієнтові. Один рядок-команда, без корисного
    // навантаження.
    //
    // Потрібен тому, що клієнтська половина ДІЇ не спрацьовує там, де мала б:
    // ActionManager кличе Start() на клієнті лише після підтвердження від
    // сервера й лише якщо пройде повторна перевірка умов, і на екрані цього
    // не сталось жодного разу -- дія виконувалась, меню не відкривалось.
    // Замість того щоб гадати про чужий скінченний автомат, беремо канал,
    // який працює: сервер зробив -- сервер і сказав.
    static const string RPC_SHOW = "OZ_Show";

    // Зареєстрована функція МУСИТЬ мати рівно цю форму -- її задає диспетчер
    // CF (Param4 + CallFunctionParams), ніде не оголошуючи явно:
    //
    //   void Ім'я(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    //
    // Чотири параметри в цьому порядку, void, ім'я збігається з рядком
    // посимвольно, метод НЕ статичний.
    static void RegisterServer(Class inst)
    {
        GetRPCManager().AddRPC(OZ_Const.MOD, RPC_HELLO, inst, SingleplayerExecutionType.Server);
        GetRPCManager().AddRPC(OZ_Const.MOD, RPC_REQ,   inst, SingleplayerExecutionType.Server);
        GetRPCManager().AddRPC(OZ_Const.MOD, RPC_REQ_PART, inst, SingleplayerExecutionType.Server);
        GetRPCManager().AddRPC(OZ_Const.MOD, RPC_LINK_REQ, inst, SingleplayerExecutionType.Server);
        GetRPCManager().AddRPC(OZ_Const.MOD, RPC_ADMIN_REQ, inst, SingleplayerExecutionType.Server);
        GetRPCManager().AddRPC(OZ_Const.MOD, RPC_ADMIN_REQ_PART, inst, SingleplayerExecutionType.Server);
    }

    // ОКРЕМО ВІД RegisterServer, бо ролі більше не ядрові. Реєструє їх той,
    // хто їх і виконує -- мод фракцій; ядро лише тримає конверт. Без того
    // мода цей RPC ніхто не слухає, і прохання змінити роль тихо нікуди не
    // йде, що для сервера без фракцій і є правильною відповіддю.
    static void RegisterRoles(Class inst)
    {
        GetRPCManager().AddRPC(OZ_Const.MOD, RPC_ROLE_REQ, inst, SingleplayerExecutionType.Server);
    }

    // Клієнт ТЯГНЕ синхронізацію сам, коли вже готовий її прийняти.
    //
    // Штовхати з боку сервера на конекті не можна: перевірено на стенді --
    // хук сервера спрацьовує раніше, ніж клієнт устигає зареєструвати свій
    // обробник, і пакет іде в нікуди. Тяга від клієнта не залежить від
    // порядку взагалі.
    static void Hello()
    {
        GetRPCManager().SendRPC(OZ_Const.MOD, RPC_HELLO, new Param1<int>(OZ_Const.SCHEMA_SETTINGS), true);
    }

    static void RegisterClient(Class inst)
    {
        GetRPCManager().AddRPC(OZ_Const.MOD, RPC_SYNC, inst, SingleplayerExecutionType.Client);
        GetRPCManager().AddRPC(OZ_Const.MOD, RPC_RES,  inst, SingleplayerExecutionType.Client);
        GetRPCManager().AddRPC(OZ_Const.MOD, RPC_RES_PART, inst, SingleplayerExecutionType.Client);
        GetRPCManager().AddRPC(OZ_Const.MOD, RPC_LINK_RES, inst, SingleplayerExecutionType.Client);
        GetRPCManager().AddRPC(OZ_Const.MOD, RPC_ROLE_RES, inst, SingleplayerExecutionType.Client);
        GetRPCManager().AddRPC(OZ_Const.MOD, RPC_ADMIN_RES, inst, SingleplayerExecutionType.Client);
        GetRPCManager().AddRPC(OZ_Const.MOD, RPC_ADMIN_RES_PART, inst, SingleplayerExecutionType.Client);
        GetRPCManager().AddRPC(OZ_Const.MOD, RPC_SHOW, inst, SingleplayerExecutionType.Client);
    }

    // guaranteed за замовчуванням FALSE. Усе, що тут надсилається, має
    // значення, тому true передається явно скрізь.
    static void SendSync(PlayerIdentity to, string json)
    {
        GetRPCManager().SendRPC(OZ_Const.MOD, RPC_SYNC, new Param1<string>(json), true, to);
    }

    // Різати можна лише МІЖ символами: Length()/Substring() байтові, а тіло
    // UTF-8. Байт-продовження має вигляд 10xxxxxx -- відступаємо до початку
    // символу, інакше на шві лишається половина літери.
    //
    // Межі -- АБСОЛЮТНІ індекси в s: різак ходить по рядку зсувом і ніколи
    // не матеріалізує хвіст. Виміряно зондом на стенді: ВИХІД Substring
    // мовчки ріжеться до 8191 байтів, тож стара форма "json = хвіст"
    // втрачала все після ~9 КіБ на першій же ітерації.
    private static int CutSafe(string s, int from, int at)
    {
        int cut = at;
        while (cut > from && (s.Get(cut).ToAscii() & 0xC0) == 0x80)
            cut--;

        // Дійшли до початку вікна -- перед нами не UTF-8, а суцільні
        // хвостові байти (сміття з битого пейлоада). Ріжемо по сирій межі:
        // биті дані лишаться битими, але цикл відправки НЕ зациклиться
        // на порожньому кроці, який інакше вішає сервер назавжди.
        if (cut == from)
            return at;

        return cut;
    }

    // Клієнт -> сервер: одержувача не вказуємо, CF сам знає, куди.
    //
    // Частина несе ЛИШЕ номер і шматок: сторінку й операцію приймач читає з
    // конверта, який приїде останнім, і повторювати їх у кожному шматку --
    // зайві байти й другий спосіб помилитись.
    static void Request(string pageId, string op, string json)
    {
        int id  = NextId();
        int len = json.Length();
        int off = 0;

        while (len - off > OZ_Const.RPC_STR_CHUNK)
        {
            int cut = CutSafe(json, off, off + OZ_Const.RPC_STR_CHUNK);
            Param2<int, string> part = new Param2<int, string>(id, json.Substring(off, cut - off));
            GetRPCManager().SendRPC(OZ_Const.MOD, RPC_REQ_PART, part, true);
            off = cut;
        }

        string last = json;
        if (off > 0)
            last = json.Substring(off, len - off);

        Param4<int, string, string, string> p = new Param4<int, string, string, string>(id, pageId, op, last);
        GetRPCManager().SendRPC(OZ_Const.MOD, RPC_REQ, p, true);
    }

    static void Respond(PlayerIdentity to, string pageId, string op, bool ok, string json, string error)
    {
        int id  = NextId();
        int len = json.Length();
        int off = 0;

        while (len - off > OZ_Const.RPC_STR_CHUNK)
        {
            int cut = CutSafe(json, off, off + OZ_Const.RPC_STR_CHUNK);
            Param2<int, string> part = new Param2<int, string>(id, json.Substring(off, cut - off));
            GetRPCManager().SendRPC(OZ_Const.MOD, RPC_RES_PART, part, true, to);
            off = cut;
        }

        string last = json;
        if (off > 0)
            last = json.Substring(off, len - off);

        Param6<int, string, string, bool, string, string> p =
            new Param6<int, string, string, bool, string, string>(id, pageId, op, ok, last, error);

        GetRPCManager().SendRPC(OZ_Const.MOD, RPC_RES, p, true, to);
    }

    // ------------------------------------------------------- адмінська консоль
    //
    // Форма конверта та сама, що в сторінок -- (кому, операція, тіло), -- і це
    // навмисно: різати довге тіло вже вміє CutSafe, і другий спосіб різати той
    // самий JSON був би другим місцем, де ту саму пастку з UTF-8 можна
    // проґавити. Різниця не у формі, а в тому, ХТО відповідає за доступ:
    // сторінку пускає прилад у руках, розділ -- OZ_Perm.IsAdmin і більш ніщо.

    static void AdminRequest(string sectionId, string op, string json)
    {
        int id  = NextId();
        int len = json.Length();
        int off = 0;

        while (len - off > OZ_Const.RPC_STR_CHUNK)
        {
            int cut = CutSafe(json, off, off + OZ_Const.RPC_STR_CHUNK);
            Param2<int, string> part = new Param2<int, string>(id, json.Substring(off, cut - off));
            GetRPCManager().SendRPC(OZ_Const.MOD, RPC_ADMIN_REQ_PART, part, true);
            off = cut;
        }

        string last = json;
        if (off > 0)
            last = json.Substring(off, len - off);

        Param4<int, string, string, string> p = new Param4<int, string, string, string>(id, sectionId, op, last);
        GetRPCManager().SendRPC(OZ_Const.MOD, RPC_ADMIN_REQ, p, true);
    }

    static void AdminRespond(PlayerIdentity to, string sectionId, string op, bool ok, string json, string error)
    {
        int id  = NextId();
        int len = json.Length();
        int off = 0;

        while (len - off > OZ_Const.RPC_STR_CHUNK)
        {
            int cut = CutSafe(json, off, off + OZ_Const.RPC_STR_CHUNK);
            Param2<int, string> part = new Param2<int, string>(id, json.Substring(off, cut - off));
            GetRPCManager().SendRPC(OZ_Const.MOD, RPC_ADMIN_RES_PART, part, true, to);
            off = cut;
        }

        string last = json;
        if (off > 0)
            last = json.Substring(off, len - off);

        Param6<int, string, string, bool, string, string> p =
            new Param6<int, string, string, bool, string, string>(id, sectionId, op, ok, last, error);

        GetRPCManager().SendRPC(OZ_Const.MOD, RPC_ADMIN_RES, p, true, to);
    }

    // ------------------------------------------------------------ прив'язка

    static void LinkRequest(string op)
    {
        GetRPCManager().SendRPC(OZ_Const.MOD, RPC_LINK_REQ, new Param1<string>(op), true);
    }

    static void LinkRespond(PlayerIdentity to, string op, bool ok, string json, string error)
    {
        Param4<string, bool, string, string> p =
            new Param4<string, bool, string, string>(op, ok, json, error);
        GetRPCManager().SendRPC(OZ_Const.MOD, RPC_LINK_RES, p, true, to);
    }

    // ---------------------------------------------------------------- ролі

    // Кого міняємо -- ІМ'ЯМ, а не uid.
    //
    // Чужого Steam64 клієнт не бачить НІКОЛИ -- це межа, яку тримає вся
    // сторінка контактів, і вона не робиться винятком заради зручності. Кому
    // належить ім'я, вирішує сервер, і серед кого шукати -- теж він.
    //
    // Актора не називаємо взагалі: він завжди береться з sender.
    static void RoleRequest(string op, string targetName, string arg)
    {
        Param3<string, string, string> p =
            new Param3<string, string, string>(op, targetName, arg);
        GetRPCManager().SendRPC(OZ_Const.MOD, RPC_ROLE_REQ, p, true);
    }

    // `why` -- або ключ таблиці рядків (STR_OZ_...), або ГОТОВИЙ ТЕКСТ від
    // моста. Друге тому, що причину відмови Discord знає лише він, і «бот не
    // може керувати цією роллю» набагато корисніше за наш код помилки. Той,
    // хто малює, розрізняє їх за префіксом.
    static void RoleRespond(PlayerIdentity to, string op, bool ok, string why)
    {
        Param3<string, bool, string> p = new Param3<string, bool, string>(op, ok, why);
        GetRPCManager().SendRPC(OZ_Const.MOD, RPC_ROLE_RES, p, true, to);
    }

    // Сервер -> клієнт: показати щось. Що саме -- вирішує той, хто підписався
    // на OZ_Show; ядро про екрани не знає.
    static void Show(PlayerIdentity to, string what)
    {
        GetRPCManager().SendRPC(OZ_Const.MOD, RPC_SHOW, new Param1<string>(what), true, to);
    }
}
