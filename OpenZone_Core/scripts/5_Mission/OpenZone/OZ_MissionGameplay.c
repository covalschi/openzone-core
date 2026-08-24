// Клієнтська точка входу ядра.
//
// Чому саме тут, а не в модулі CF: модуль живе в 4_World, а OZ_ClientState --
// у 5_Mission, і знизу вгору видимості немає. Порядок компіляції жорсткий:
// 3_Game -> 4_World -> 5_Mission.
//
// На виділеному сервері MissionGameplay не створюється взагалі (там
// MissionServer), тож цей код туди просто не потрапляє.

modded class MissionGameplay
{
    private bool m_OZ_Greeted = false;

    override void OnInit()
    {
        super.OnInit();

        OZ_Rpc.RegisterClient(OZ_ClientState.Instance());
    }

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);

        // Вітаємось РІВНО ОДИН раз і лише коли гравець уже існує: поява
        // гравця означає, що ми у світі й канал працює. OnInit для цього
        // зарано -- там ще нема кому відповідати.
        if (!m_OZ_Greeted && GetGame().GetPlayer())
        {
            m_OZ_Greeted = true;
            OZ_Rpc.Hello();

#ifdef OZ_SELFTEST
            // Тимчасова перевірка межі безпеки: сторінки "ghost" не існує,
            // сервер мусить відмовити й записати це в лог. Вмикається лише
            // дефайном, у поставку не потрапляє.
            OZ_Rpc.Request("ghost", "noop", "{}");
#endif
        }
    }
}
