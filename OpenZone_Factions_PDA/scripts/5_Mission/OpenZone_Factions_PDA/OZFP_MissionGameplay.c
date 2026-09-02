// Клієнтська половина склейки: два підключення до чужих точок розширення.
//
// Перше -- сказати фабриці сторінок КПК, хто малює «Фракцію» і якою літерою
// вона підписана в стрічці вкладок.
//
// Друге -- пара з контактами. Коли на пристрої є обидві сторінки, вони
// ділять ОДНУ вкладку: ліворуч люди, праворуч свої. Раніше цю пару знало
// саме меню КПК -- тобто КПК мусив знати ім'я фракційної сторінки, якої в
// нього більше немає. Тепер пару оголошує той, хто її й утворює.
//
// На виділеному сервері MissionGameplay не створюється взагалі (там
// MissionServer), тож цей код туди просто не потрапляє.

modded class MissionGameplay
{
    override void OnInit()
    {
        super.OnInit();

        OZ_PdaPageFactory.Add(OZFP_Const.PAGE_FACTION, OZ_PdaPageFaction);
        OZ_PdaPageFactory.Letter(OZFP_Const.PAGE_FACTION, "F");
        OZ_PdaPageFactory.Pair(OZ_PdaConst.PAGE_CONTACTS, OZFP_Const.PAGE_FACTION);
    }
}
