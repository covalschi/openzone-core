// Серверний синглтон ядра на модульній системі CF.
//
// Enable* викликаються ТІЛЬКИ всередині OnInit і ТІЛЬКИ після super.OnInit():
// модулі конструюються на CF_LifecycleEvents.OnGameCreate, і до цього моменту
// CF_Modules<T>.Get() повертає null. Це не стиль, це порядок ініціалізації.

[CF_RegisterModule(OZ_Module)]
class OZ_Module : CF_ModuleWorld
{
    override void OnInit()
    {
        super.OnInit();

        EnableMissionStart();
        EnableMissionFinish();
    }

    override void OnMissionStart(Class sender, CF_EventArgs args)
    {
        super.OnMissionStart(sender, args);

        // Конфіги, права й сховище -- серверні. Клієнт отримує лише те, що
        // сервер сам йому вирішив надіслати.
        if (!GetGame().IsServer())
            return;

        OZ_Settings.ServerLoad();

        OZ_Settings s = OZ_Settings.Get();

        string dbg = "off";
        if (s.DebugMode)
            dbg = "on";

        // Два оператори, а не один довгий ланцюжок «+»: компілятор Enforce має
        // межу складності виразу й падає з «Formula too complex» -- у сусідньому
        // моді це знайшли емпірично на восьмому доданку.
        string summary = "core loaded: admins=" + s.AdminIds.Count();
        summary += " debug=" + dbg;
        OZ_Log.Info(summary);
    }

    override void OnMissionFinish(Class sender, CF_EventArgs args)
    {
        super.OnMissionFinish(sender, args);

        if (!GetGame().IsServer())
            return;

        OZ_Log.Dbg("core shutting down");
    }
}
