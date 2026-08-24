// Форма даних, якими ядро говорить із мостом OpenZone.
//
// Контракт зафіксовано ЗАРАЗ, хоча реалізація приїде в #4: від нього
// залежить форма чату (#3), і переробляти її потім дорожче, ніж домовитись
// один раз наперед.

class OZ_BridgeEnvelope
{
    // Секрет їде В ТІЛІ, а не в заголовку. DayZ не дає задати заголовки
    // запиту: RestContext.SetHeader керує лише Content-Type. Тому канал
    // ЗОБОВ'ЯЗАНИЙ бути https -- відкритий http роздав би секрет усім,
    // хто дивиться трафік.
    string Secret;

    // Хто питає. Один міст обслуговує кілька стендів і серверів.
    string ServerId;

    // Скільки повідомлень уже отримано. Міст віддає лише те, що новіше.
    int Cursor;

    // Що це: "chat", "presence", "link", "role" ... Розбирає міст, не ядро.
    string Kind;

    // Корисне навантаження, вкладене як рядок JSON. Ядро в нього не
    // заглядає -- воно возить конверти, а не читає листи.
    string Json;
}

class OZ_BridgeBatch
{
    int Cursor;
    ref array<ref OZ_BridgeEnvelope> Items;

    void OZ_BridgeBatch()
    {
        Items = new array<ref OZ_BridgeEnvelope>();
    }
}
