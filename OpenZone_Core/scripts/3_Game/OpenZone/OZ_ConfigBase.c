// База версіонованого конфігу і загальний порядок його завантаження.
//
// Головне правило, від якого тут усе залежить: КОНФІГ НІКОЛИ НЕ Є ПРИЧИНОЮ НЕ
// ЗАВАНТАЖИТИСЬ. Зіпсований файл, застаріла схема, безглузде значення -- усе це
// дає запис у лог і дефолт, але сервер піднімається. Мод, який відмовляється
// стартувати через кому не в тому місці, адміну не потрібен.

class OZ_ConfigBase
{
    int Version;

    // Нащадки перевизначають усі чотири.
    //
    // LoadDefaults МУСИТЬ виставити КОЖНЕ поле: його кличуть і на порожньому
    // об'єкті, і поверх частково прочитаного сміття після невдалого розбору.
    int  LatestVersion()            { return 1; }
    void LoadDefaults()             { Version = LatestVersion(); }
    bool Migrate(int from)          { Version = LatestVersion(); return true; }
    void Validate(out int warnings) { warnings = 0; }
}

class OZ_ConfigLoader<Class T>
{
    // cfg приходить УЖЕ створеним. new T() всередині дженерика в Enforce
    // ненадійний, тому конкретний тип створює викликач -- саме так це зроблено
    // і в ZP_Research, тільки там без дженерика зовсім.
    //
    // Порядок: прочитати -> мігрувати -> перевірити -> перезаписати, якщо щось
    // змінилось. Провал будь-якого кроку відкидає на дефолти й відкладає
    // зіпсоване вбік.
    static void Load(string path, string tag, inout T cfg, bool backupOnWrite = true)
    {
        bool fresh = false;
        bool salvaged = false;
        // Карантин не вдався -- файл на диску НЕ ЧІПАЄМО взагалі. Інакше
        // єдиний примірник того, що зламав адмін, зникає назавжди, і в лозі
        // лишається сама лише назва помилки розбору.
        bool keepFile = false;
        string err;

        if (!FileExist(path))
        {
            cfg.LoadDefaults();
            fresh = true;
        }
        else if (!JsonFileLoader<T>.LoadFile(path, cfg, err))
        {
            OZ_Log.Error("read " + path + ": " + err);
            bool kept = OZ_Json.Quarantine(path, tag);
            cfg.LoadDefaults();
            fresh = true;
            salvaged = true;
            if (!kept)
                keepFile = true;
        }
        else if (cfg.Version != cfg.LatestVersion())
        {
            int from = cfg.Version;
            if (!cfg.Migrate(from))
            {
                string bad = "cannot migrate " + tag;
                bad += " from v" + from;
                OZ_Log.Error(bad);
                bool keptM = OZ_Json.Quarantine(path, tag);
                cfg.LoadDefaults();
                salvaged = true;
                if (!keptM)
                    keepFile = true;
            }
            else
            {
                string ok = "migrated " + tag;
                ok += " v" + from;
                ok += " -> v" + cfg.Version;
                OZ_Log.Info(ok);
            }
            fresh = true;
        }

        int warnings;
        cfg.Validate(warnings);

        // ПОЧИНКИ VALIDATE ТЕЖ ЇДУТЬ НА ДИСК.
        //
        // Раніше писалось лише `fresh` -- тобто новий файл, невдалий розбір і
        // міграція. Файл правильної версії, який Validate тихо полагодив,
        // лишався на диску зламаним, і та сама починка повторювалась КОЖЕН
        // старт. Для настройок це не косметика: серед таких починок є та, що
        // вимикає міст (Bridge.Enabled = 0 при порожньому Url), і адмін бачив
        // мовчазно вимкнений міст щоразу, полагодивши файл.
        bool write = fresh;
        if (warnings > 0)
            write = true;

        // Після карантину резервну копію НЕ оновлюємо. Інакше .bak затерся б
        // тим самим сміттям, що вже лежить у .bad, і єдина копія, з якої можна
        // було відновитись, зникла б через один зіпсований старт.
        if (write && !keepFile)
            Save(path, tag, cfg, backupOnWrite && !salvaged);
    }

    // backup=false для файлів гравців: їх сотні, і копія кожного перед кожним
    // записом засмітила б Backup так, що знайти в ньому щось стане неможливо.
    static void Save(string path, string tag, T cfg, bool backup = true)
    {
        if (backup && FileExist(path))
            OZ_Json.Backup(path, tag);

        string err;
        if (!JsonFileLoader<T>.SaveFile(path, cfg, err))
            OZ_Log.Error("write " + path + ": " + err);
    }
}
