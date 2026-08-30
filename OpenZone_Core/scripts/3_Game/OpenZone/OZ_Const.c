// Константи, спільні для всього фреймворку.
//
// Шляхи в $profile: пишемо з екранованим зворотним слешем. Пряма коса теж
// працює (ваніль так і робить у CameraTools), але змішувати два стилі в одному
// проєкті -- шукати собі проблем на рівному місці.

class OZ_Const
{
    // Ім'я мода для CF RPCManager. Разом з іменем функції утворює ключ, тому
    // зіткнення з чужим модом можливе лише при збігу ОБОХ рядків.
    static const string MOD        = "OpenZone";
    static const string LOG_PREFIX = "[OpenZone] ";

    static const string PROFILE_DIR = "$profile:OpenZone";
    static const string BACKUP_DIR  = "$profile:OpenZone\\Backup";
    static const string PLAYERS_DIR = "$profile:OpenZone\\players";
    static const string LANG_DIR    = "$profile:OpenZone\\Lang";
    static const string SETTINGS    = "$profile:OpenZone\\Settings.json";

    // Версія схеми Settings.json. Зростає лише разом із міграцією.
    static const int SCHEMA_SETTINGS = 2;

    // Сторiнка адмiнської консолi. Реєструється в загальному реєстрi, але
    // вкладкою КПК не стає: вкладки роздає профiль пристрою, а ця сторiнка
    // нi в один профiль не входить. Ворота на нiй -- OZ_Perm.IsAdmin.
    static const string PAGE_ADMIN = "admin";

    // Межі, задані рушієм, а не нами: RestApi.SetOption приймає таймаут саме в
    // цих секундах. Конфіг, що обіцяє більше, обіцяє нездійсненне.
    static const int REST_TIMEOUT_MIN = 3;
    static const int REST_TIMEOUT_MAX = 120;

    // Відповідь буде, але пізніше й не звідси. Сторінка каже це замість
    // ключа помилки, і диспетчер тоді мовчить: інакше клієнт побачив би
    // «не вдалося» за секунду до того, як приїде справжня відповідь.
    static const string DEFER = "@defer";

    // Рядок у рушійному RPC живе в буфері ~1024 байти: 1011 байт корисного
    // JSON проходили, 1386 приїздили як "String CORRUPTED" (зміряно на
    // стенді, список нотаток). Довші за поріг тіла їдуть частинами
    // OZ_ReqPart/OZ_ResPart попереду свого конверта; приймач приклеює їх
    // перед тілом фінального. Порядок гарантує канал guaranteed-RPC.
    static const int RPC_STR_CHUNK = 900;
}
