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
    static const int SCHEMA_SETTINGS = 1;

    // Межі, задані рушієм, а не нами: RestApi.SetOption приймає таймаут саме в
    // цих секундах. Конфіг, що обіцяє більше, обіцяє нездійсненне.
    static const int REST_TIMEOUT_MIN = 3;
    static const int REST_TIMEOUT_MAX = 120;
}
