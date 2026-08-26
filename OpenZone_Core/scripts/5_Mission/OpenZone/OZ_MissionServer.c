// Серверна половина місії: єдине місце, де ядро втручається в появу гравця.
//
// CreateCharacter -- той самий шов, і тільки він. У всьому корпусі рушія його
// кличуть двічі (missionserver.c:560 і :576), обидва рази з OnClientNewEvent,
// тобто на створенні НОВОГО персонажа. Переприєднання сюди не заходить:
// OnClientReconnectEvent отримує вже готового PlayerBase. Тому гравець, який
// просто повернувся, лишається там, де вийшов -- і ми не можемо телепортувати
// його навіть помилково.
//
// Нічого не налаштовано -- нічого й не змінюється: Resolve повертає ту саму
// позицію, яку дав рушій.

modded class MissionServer
{
    override PlayerBase CreateCharacter(PlayerIdentity identity, vector pos, ParamsReadContext ctx, string characterName)
    {
        vector where = OZ_Spawns.Resolve(identity, pos);
        return super.CreateCharacter(identity, where, ctx, characterName);
    }
}
