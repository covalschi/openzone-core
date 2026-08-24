// Клієнт моста OpenZone. У #0 -- лише контракт і вимкнений стан.
//
// Чому взагалі тут, а не цілком у #4: наміри треба зафіксувати до того, як
// на них зіпреться чат. Форма конверта, місце секрета й спосіб отримання
// вхідних -- усе це вирішується один раз.
//
// СХЕМА, коротко:
//   гра -> міст   RestContext.POST -- вихідний запит, звичайний
//   міст -> гра   ДОВГИЙ опит: гра питає, міст ТРИМАЄ відповідь, поки не
//                 з'явиться що сказати (~25 с), тоді віддає пачку; гра
//                 одразу перепитує.
//
// Довгий опит потрібен, бо в гри НЕМАЄ вхідних з'єднань: сервер DayZ
// нікого не слухає, і Discord не може постукати в гру. Таймаут RestApi
// налаштовується від 3 до 120 секунд (ERESTOPTION_READOPERATION), і цього
// вистачає, щоб тримати запит відкритим. Затримка виходить майже нульова,
// а не «раз на п'ять секунд».
//
// Назовні ходить ТІЛЬКИ сервер. Ігровий клієнт секрета не бачить ніколи.

class OZ_BridgeCallback : RestCallback
{
    override void OnSuccess(string data, int dataSize)
    {
        // #4: розбір пачки й роздача її сторінкам через реєстр.
        //
        // Print не виводить рядок довший за 1024 байти (сказано в самій
        // ваніль-документації RestCallback), тож сире тіло сюди не пишемо
        // ніколи -- лише розмір.
        OZ_Log.Dbg("bridge: batch received, " + dataSize.ToString() + " bytes");
    }

    override void OnError(int errorCode)
    {
        // Може бути покликано КІЛЬКА разів на один запит, якщо RetryCount > 1.
        OZ_Log.Warn("bridge: request failed, code " + errorCode.ToString());
    }

    override void OnTimeout()
    {
        // Для довгого опиту таймаут -- НОРМА, а не помилка: міст просто не
        // мав що сказати. Рівень Dbg, і жодного відкату.
        OZ_Log.Dbg("bridge: poll timed out with nothing to say");
    }
}

class OZ_BridgeClient
{
    private static bool s_Running = false;
    private static int  s_Cursor  = 0;
    private static ref OZ_BridgeCallback s_Cb;

    static bool IsRunning()
    {
        return s_Running;
    }

    static int Cursor()
    {
        return s_Cursor;
    }

    static void Start()
    {
        OZ_BridgeSettings b = OZ_Settings.Get().Bridge;

        if (!b.Enabled)
        {
            OZ_Log.Info("bridge: disabled");
            return;
        }

        // #4: тут піднімається таймаут читання під довгий опит і
        // починається цикл. Поки -- чесно кажемо, що вміємо не все.
        //
        //   GetRestApi().SetOption(ERestOption.ERESTOPTION_READOPERATION,
        //                          b.PollTimeoutSec);
        //   RestContext ctx = GetRestApi().GetRestContext(b.Url);
        //   ctx.SetHeader("application/json");
        //   ctx.POST(s_Cb, "v1/poll", envelopeJson);

        s_Cb = new OZ_BridgeCallback();
        s_Running = true;

        OZ_Log.Info("bridge: configured but not implemented yet (#4)");
    }

    static void Stop()
    {
        s_Running = false;
        s_Cb = null;
    }

    // Вихідне повідомлення. У #0 мовчки ковтається -- сторінки вже можуть
    // це кликати, не питаючи, чи є міст.
    static void Post(string kind, string json)
    {
        if (!s_Running)
            return;
    }
}
