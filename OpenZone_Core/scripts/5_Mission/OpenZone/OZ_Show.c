// «Покажи це» -- клієнтський бік.
//
// Ядро не знає ні про КПК, ні про будь-який інший екран, і не мусить. Воно
// лише розносить рядок-команду тим, хто підписався; що з ним робити --
// справа мода, який підписався.

class OZ_Show
{
    static ref ScriptInvoker OnShow = new ScriptInvoker();

    static void Take(string what)
    {
        OnShow.Invoke(what);
    }
}
