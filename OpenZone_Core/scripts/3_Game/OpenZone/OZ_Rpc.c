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
        GetRPCManager().AddRPC(OZ_Const.MOD, RPC_LINK_REQ, inst, SingleplayerExecutionType.Server);
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
        GetRPCManager().AddRPC(OZ_Const.MOD, RPC_LINK_RES, inst, SingleplayerExecutionType.Client);
        GetRPCManager().AddRPC(OZ_Const.MOD, RPC_ROLE_RES, inst, SingleplayerExecutionType.Client);
    }

    // guaranteed за замовчуванням FALSE. Усе, що тут надсилається, має
    // значення, тому true передається явно скрізь.
    static void SendSync(PlayerIdentity to, string json)
    {
        GetRPCManager().SendRPC(OZ_Const.MOD, RPC_SYNC, new Param1<string>(json), true, to);
    }

    // Клієнт -> сервер: одержувача не вказуємо, CF сам знає, куди.
    static void Request(string pageId, string op, string json)
    {
        Param3<string, string, string> p = new Param3<string, string, string>(pageId, op, json);
        GetRPCManager().SendRPC(OZ_Const.MOD, RPC_REQ, p, true);
    }

    static void Respond(PlayerIdentity to, string pageId, string op, bool ok, string json, string error)
    {
        Param5<string, string, bool, string, string> p =
            new Param5<string, string, bool, string, string>(pageId, op, ok, json, error);
        GetRPCManager().SendRPC(OZ_Const.MOD, RPC_RES, p, true, to);
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
}
