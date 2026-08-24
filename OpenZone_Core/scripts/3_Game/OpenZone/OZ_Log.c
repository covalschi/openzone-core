// Логер із рівнями й спільним префіксом.
//
// Префікс потрібен не для краси: вердикт MCP шукає в RPT саме його, а сервер із
// модпаком друкує тисячі чужих рядків. Без префікса наші губляться.
//
// Dbg мовчить, доки Settings.Debug не увімкнено -- інакше жвавий цикл засипле
// лог і зробить його марним саме тоді, коли він потрібен.

class OZ_Log
{
    private static bool s_Debug = false;

    static void SetDebug(bool on)
    {
        s_Debug = on;
    }

    static bool IsDebug()
    {
        return s_Debug;
    }

    static void Info(string msg)
    {
        Print(OZ_Const.LOG_PREFIX + msg);
    }

    // Саме WARNING, а не WARN: вердикт MCP шукає в лозі \bWARNING\b
    // (logparse.py:34). Скорочене слово під це не підпадає, і бюджет
    // max_warnings у профілі перестав би стежити за нашими ж перевірками --
    // регресія у Validate проходила б повз вердикт мовчки.
    static void Warn(string msg)
    {
        Print(OZ_Const.LOG_PREFIX + "WARNING: " + msg);
    }

    static void Error(string msg)
    {
        Print(OZ_Const.LOG_PREFIX + "ERROR: " + msg);
    }

    static void Dbg(string msg)
    {
        if (s_Debug)
            Print(OZ_Const.LOG_PREFIX + "dbg: " + msg);
    }
}
