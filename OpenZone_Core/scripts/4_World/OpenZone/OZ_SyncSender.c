// Пакет синхронізації: скласти й відправити одному гравцеві.
//
// Було приватним методом OZ_Module і їхало рівно раз -- коли клієнт, з'явившись
// у світі, сам його попросив. ТЗ-5 R-C1.3 вимагає ДЕЛЬТИ: стан прив'язки
// Discord міняється посеред сесії (гравець пройшов /link), і ворота на
// клієнті мусять дізнатись про це зараз, а не після перезаходу. Тому
// відправник -- окремий і статичний: його кличе і OZ_Module на запит, і
// OZ_Link.Confirm на зміну.
//
// Пакет крихітний і подія рідкісна, тож жодного «лише різниця»: їде той самий
// повний конверт, і клієнт застосовує його так само, як перший. Одна форма
// пакета -- одна дорога його розбору.

class OZ_SyncSender
{
    static void Send(PlayerIdentity to, string why)
    {
        if (!to)
            return;

        OZ_SyncPayload p = new OZ_SyncPayload();
        p.Schema    = OZ_Const.SCHEMA_SETTINGS;
        p.DebugMode = OZ_Settings.Get().DebugMode;

        // Прив'язка їде тим самим конвертом: на вході це найраніша мить, коли
        // є кому показати ворота, а посеред сесії -- та сама дорога, якою
        // вони й відчиняються.
        p.Linked       = OZ_Link.IsLinked(to.GetPlainId());
        p.LinkRequired = OZ_Link.Gated(to.GetPlainId());
        OZ_PageRegistry.FillPayload(p);

        // Моди докладають своє (OZ_SyncExtras): ядро не знає, що саме, і не
        // мусить.
        OZ_SyncExtras.OnFill().Invoke(p);

        string json;
        string err;
        // prettyPrint=false: у провід не треба ані відступів, ані переносів.
        if (!JsonFileLoader<OZ_SyncPayload>.MakeData(p, json, err, false))
        {
            OZ_Log.Error("cannot serialise sync payload: " + err);
            return;
        }

        OZ_Rpc.SendSync(to, json);

        string line = "sync: sent to " + to.GetPlainId() + " " + why;
        line += " (extras=" + p.Extras.Count().ToString() + ")";
        OZ_Log.Dbg(line);
    }
}
