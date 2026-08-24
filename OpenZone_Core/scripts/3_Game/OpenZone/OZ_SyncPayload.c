// Те, що РЕАЛЬНО їде на клієнт.
//
// Окремий тип, а не серіалізований OZ_Settings -- і це межа безпеки, а не
// стильова забаганка. У Settings лежить секрет моста; один недбалий SendRPC із
// тим об'єктом роздав би його кожному, хто зайшов на сервер. Тут фізично нема
// чого роздавати.

class OZ_SyncPageInfo
{
    string PageId;
    string TitleKey;
    string Icon;
}

class OZ_SyncPayload
{
    int  Schema;
    bool IsAdmin;

    // Прапорець відладки їде разом із рештою: інакше OZ_Log.Dbg на клієнті
    // мовчить назавжди -- Settings читає лише сервер.
    bool DebugMode;

    // МАСИВ, не map: map через RPC не передається. Клієнт за потреби збирає
    // з нього свою карту сам.
    ref array<ref OZ_SyncPageInfo> Pages;

    void OZ_SyncPayload()
    {
        Pages = new array<ref OZ_SyncPageInfo>();
    }
}
