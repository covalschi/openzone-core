// Форма, якою сторінка «Фракція» говорить із сервером.
//
// ЖИВЕ В СКЛЕЙЦІ, а не в КПК. Сторінка фракції -- це фракційна система, яку
// власник виніс із ядра окремим модом 2026-08-31; КПК про неї більше не знає
// нічого. Сервер без мода фракцій просто не має цієї вкладки, так само як не
// має вкладки рації без склейки з рацією.

// --- сторінка «Фракція» ---

class OZ_FactionMember
{
    string Name = "";
    // Сталкерське звання -- особисте, поза фракцією.
    string Rank = "";
    // Внутрішньофракційне: підпис для екрана і слаг для арифметики
    // «наступне вище». Порожньо -- звання немає.
    string FRank   = "";
    string FRankId = "";
    bool Leader = false;
    bool Online = false;
    bool Me     = false;
}

class OZ_FactionState
{
    // Порожній slug -- одинак: сторінка чесно каже, що фракції немає.
    // УГРУПОВАННЯ гравця, або порожньо. Базова фракцiя сюди не потрапляє
    // нiколи: у неї немає ані складу, ані лiдера, i екран узагалi не про неї.
    string Org         = "";
    string FactionName = "";
    int    Color       = 0;
    string MyRank      = "";
    bool   MeLeader    = false;

    // Запрошення, що чекає САМЕ на мене.
    string InviteFaction = "";
    string InviteFrom    = "";

    ref array<ref OZ_FactionMember> Members;
    // Кого лідер може покликати: друзі поза фракцією, іменами.
    ref array<string> Candidates;

    // Драбина ЦІЄЇ фракції, знизу вгору: слаги й підписи поруч. Порожня --
    // звань у фракції не заводили, і кнопки підвищення нема сенсу малювати.
    ref array<string> RankIds;
    ref array<string> RankNames;

    void OZ_FactionState()
    {
        Members    = new array<ref OZ_FactionMember>();
        Candidates = new array<string>();
        RankIds    = new array<string>();
        RankNames  = new array<string>();
    }
}
