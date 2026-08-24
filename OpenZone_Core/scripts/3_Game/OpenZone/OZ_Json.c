// Файлова механіка навколо JSON: каталоги, резервні копії, карантин.
//
// Тут навмисно немає жодного знання про конкретний конфіг -- лише робота з
// файлами. Розбір і перевірку робить OZ_ConfigBase та його нащадки.

class OZ_Json
{
    static void EnsureDir(string dir)
    {
        if (!FileExist(dir))
            MakeDirectory(dir);
    }

    // MakeDirectory НЕ рекурсивний: батьківський каталог створюємо самі й
    // перед дитиною. Порядок тут значущий.
    static void EnsureTree()
    {
        EnsureDir(OZ_Const.PROFILE_DIR);
        EnsureDir(OZ_Const.BACKUP_DIR);
        EnsureDir(OZ_Const.PLAYERS_DIR);
        EnsureDir(OZ_Const.LANG_DIR);
    }

    // Відкладає зіпсований файл убік, а не переписує його. Причину дефекту
    // читають ПІСЛЯ падіння, і перезапис її знищує назавжди.
    static void Quarantine(string path, string tag)
    {
        EnsureDir(OZ_Const.BACKUP_DIR);
        string dst = OZ_Const.BACKUP_DIR + "\\" + tag + ".bad.json";
        CopyFile(path, dst);

        string msg = "corrupt config " + path;
        msg += " kept at " + dst;
        OZ_Log.Error(msg);
    }

    static void Backup(string path, string tag)
    {
        EnsureDir(OZ_Const.BACKUP_DIR);
        CopyFile(path, OZ_Const.BACKUP_DIR + "\\" + tag + ".bak.json");
    }
}
