// Пресети спорядження мода фракцій (ТЗ-3 §5-§6): конфіг, драбина й реалізація
// служби ядра OZ_LoadoutService. Ядро роздягає й одягає; ХТО й ЩО -- тут.
//
// Файл: $profile:OpenZone\OZ_Factions_Loadouts.json.
//
//   Presets   -- пресети за id (OZ_LoadoutPreset: Items[] з ClassName, SlotName,
//                Quantity, Health01, QuickBar, Inside[]).
//   Ladder    -- щаблі: Faction + Rank -> Preset. Rank порожній -- на всю
//                фракцію; для угруповання Rank -- слаг внутрішнього звання
//                (FRank), для базової фракції -- слаг сталкерського (Rank).
//   Modifiers -- за фракцією список OZ_LoadoutMod (Key -- слаг посади чи
//                мітки; Replace за слотом, Add у контейнер), у порядку
//                оголошення (R4.3-R4.4).
//
// ПОРОЖНЯ ДРАБИНА -- ФУНКЦІЯ ВИМКНЕНА. Адмін, який пресетів не заводив, не
// має отримувати WARNING на кожній появі (R3.3 говорить про фракцію без
// пресета, коли пресети взагалі є). Про це -- один рядок Info при старті.

class OZF_LoadoutRung
{
    string Faction = "";
    string Rank    = "";
    string Preset  = "";
}

class OZF_LoadoutFactionMods
{
    string Faction = "";
    ref array<ref OZ_LoadoutMod> Mods;

    void OZF_LoadoutFactionMods()
    {
        Mods = new array<ref OZ_LoadoutMod>();
    }
}

class OZF_LoadoutsConfig : OZ_ConfigBase
{
    ref array<ref OZ_LoadoutPreset>       Presets;
    ref array<ref OZF_LoadoutRung>        Ladder;
    ref array<ref OZF_LoadoutFactionMods> Modifiers;

    override int LatestVersion()
    {
        return 1;
    }

    // Приклад форми, НЕ підвішений до жодного щабля: файл народжується з
    // одним пресетом і порожньою драбиною, тобто нічого не робить, поки
    // адмін не впише щабель. Приймання 9.1/9.7 залежать саме від цього.
    override void LoadDefaults()
    {
        Version   = LatestVersion();
        Presets   = new array<ref OZ_LoadoutPreset>();
        Ladder    = new array<ref OZF_LoadoutRung>();
        Modifiers = new array<ref OZF_LoadoutFactionMods>();

        OZ_LoadoutPreset ex = new OZ_LoadoutPreset();
        ex.Id = "example-stalker";

        OZ_LoadoutItem body = new OZ_LoadoutItem();
        body.ClassName = "Hoodie_Blue";
        body.SlotName      = "Body";
        OZ_LoadoutItem rag = new OZ_LoadoutItem();
        rag.ClassName = "BandageDressing";
        rag.QuickBar  = 1;
        body.Inside.Insert(rag);
        ex.Items.Insert(body);

        OZ_LoadoutItem legs = new OZ_LoadoutItem();
        legs.ClassName = "Jeans_Blue";
        legs.SlotName      = "Legs";
        ex.Items.Insert(legs);

        OZ_LoadoutItem feet = new OZ_LoadoutItem();
        feet.ClassName = "Sneakers_Black";
        feet.SlotName      = "Feet";
        ex.Items.Insert(feet);

        Presets.Insert(ex);
    }

    override bool Migrate(int from)
    {
        Version = LatestVersion();
        return true;
    }

    // Класнейми перевіряються ТУТ, при завантаженні, а не на появі (R6.4);
    // щабель без відомого пресета -- попередження й геть, інакше він
    // «спрацював» би в нікуди й гравець лишився б голим.
    override void Validate(out int warnings)
    {
        warnings = 0;

        if (!Presets)
            Presets = new array<ref OZ_LoadoutPreset>();
        if (!Ladder)
            Ladder = new array<ref OZF_LoadoutRung>();
        if (!Modifiers)
            Modifiers = new array<ref OZF_LoadoutFactionMods>();

        for (int p = 0; p < Presets.Count(); p++)
        {
            OZ_LoadoutPreset pr = Presets[p];
            if (!pr)
                continue;
            if (!pr.Items)
                pr.Items = new array<ref OZ_LoadoutItem>();
            if (pr.Id == "")
            {
                OZ_Log.Warn("loadouts: a preset without Id is ignored");
                warnings++;
                continue;
            }
            CheckItems(pr.Items, pr.Id, warnings);
        }

        for (int r = Ladder.Count() - 1; r >= 0; r--)
        {
            OZF_LoadoutRung rung = Ladder[r];
            if (!rung || rung.Faction == "" || rung.Preset == "" || !Has(rung.Preset))
            {
                string what = "loadouts: ladder rung dropped -";
                if (!rung || rung.Faction == "")
                    what += " no faction";
                else if (rung.Preset == "")
                    what += " no preset for " + rung.Faction;
                else
                    what += " unknown preset \"" + rung.Preset + "\" for " + rung.Faction;
                OZ_Log.Warn(what);
                warnings++;
                Ladder.RemoveOrdered(r);
            }
        }

        for (int m = 0; m < Modifiers.Count(); m++)
        {
            OZF_LoadoutFactionMods fm = Modifiers[m];
            if (!fm)
                continue;
            if (!fm.Mods)
                fm.Mods = new array<ref OZ_LoadoutMod>();
            for (int k = 0; k < fm.Mods.Count(); k++)
            {
                OZ_LoadoutMod mod = fm.Mods[k];
                if (!mod)
                    continue;
                if (!mod.Replace)
                    mod.Replace = new array<ref OZ_LoadoutItem>();
                if (!mod.Add)
                    mod.Add = new array<ref OZ_LoadoutItem>();
                CheckItems(mod.Replace, fm.Faction + ":" + mod.Key, warnings);
                CheckItems(mod.Add, fm.Faction + ":" + mod.Key, warnings);
            }
        }
    }

    private bool Has(string presetId)
    {
        for (int i = 0; i < Presets.Count(); i++)
        {
            if (Presets[i] && Presets[i].Id == presetId)
                return true;
        }
        return false;
    }

    private void CheckItems(array<ref OZ_LoadoutItem> items, string owner, inout int warnings)
    {
        if (!items)
            return;
        for (int i = 0; i < items.Count(); i++)
        {
            OZ_LoadoutItem it = items[i];
            if (!it)
                continue;
            if (!it.Inside)
                it.Inside = new array<ref OZ_LoadoutItem>();
            if (it.ClassName == "" || !GetGame().ConfigIsExisting("CfgVehicles " + it.ClassName))
            {
                OZ_Log.Warn("loadouts " + owner + ": unknown class \"" + it.ClassName + "\" - it will not be created");
                warnings++;
            }
            CheckItems(it.Inside, owner, warnings);
        }
    }
}

class OZF_Loadouts
{
    private static ref OZF_LoadoutsConfig s_Cfg;

    static void ServerLoad()
    {
        if (!GetGame().IsServer())
            return;

        s_Cfg = new OZF_LoadoutsConfig();
        OZ_ConfigLoader<OZF_LoadoutsConfig>.Load(OZ_Const.PROFILE_DIR + "\\OZ_Factions_Loadouts.json", "loadouts", s_Cfg);

        string line = "loadouts: presets=" + s_Cfg.Presets.Count().ToString();
        line += " rungs=" + s_Cfg.Ladder.Count().ToString();
        line += " modifier sets=" + s_Cfg.Modifiers.Count().ToString();
        if (s_Cfg.Ladder.Count() == 0)
            line += " - no ladder, spawns are left to the mission";
        OZ_Log.Info(line);
    }

    static bool Enabled()
    {
        return s_Cfg && s_Cfg.Ladder && s_Cfg.Ladder.Count() > 0;
    }

    static OZ_LoadoutPreset Find(string id)
    {
        if (!s_Cfg || !s_Cfg.Presets || id == "")
            return null;
        for (int i = 0; i < s_Cfg.Presets.Count(); i++)
        {
            if (s_Cfg.Presets[i] && s_Cfg.Presets[i].Id == id)
                return s_Cfg.Presets[i];
        }
        return null;
    }

    // Щабель: точний збіг фракції й звання (порожнє звання -- на всю фракцію).
    static OZ_LoadoutPreset Rung(string faction, string rank)
    {
        if (!s_Cfg || !s_Cfg.Ladder || faction == "")
            return null;
        for (int i = 0; i < s_Cfg.Ladder.Count(); i++)
        {
            OZF_LoadoutRung r = s_Cfg.Ladder[i];
            if (!r || r.Faction != faction || r.Rank != rank)
                continue;
            return Find(r.Preset);
        }
        return null;
    }

    static bool HasAnyRung(string faction)
    {
        if (!s_Cfg || !s_Cfg.Ladder || faction == "")
            return false;
        for (int i = 0; i < s_Cfg.Ladder.Count(); i++)
        {
            if (s_Cfg.Ladder[i] && s_Cfg.Ladder[i].Faction == faction)
                return true;
        }
        return false;
    }

    static array<ref OZ_LoadoutMod> ModsOf(string faction)
    {
        if (!s_Cfg || !s_Cfg.Modifiers || faction == "")
            return null;
        for (int i = 0; i < s_Cfg.Modifiers.Count(); i++)
        {
            if (s_Cfg.Modifiers[i] && s_Cfg.Modifiers[i].Faction == faction)
                return s_Cfg.Modifiers[i].Mods;
        }
        return null;
    }
}

// Реалізація служби ядра. Драбина (R3.1): угруповання+FRank, угруповання,
// базова+Rank, базова, NAKED (базова оголошена, гравцеві ще не призначена),
// NO_OPINION + WARNING (базової в конфігу немає взагалі). Щаблі 3-4
// невідчужувані разом із Rank (R3.2); при мертвому мості базова відома з
// файла гравця, звання -- ні, і драбина чесно опускається на щабель 4 (R3.5).
class OZF_LoadoutService : OZ_LoadoutService
{
    // Складений пресет НАЛЕЖИТЬ службі (R1.3): ядро читає його після
    // повернення, і локальний об'єкт на цю мить був би вже знищений.
    private ref OZ_LoadoutPreset m_Composed;

    void OZF_LoadoutService()
    {
        m_Composed = new OZ_LoadoutPreset();
    }

    override bool Preset(string id, out OZ_LoadoutPreset preset)
    {
        preset = OZF_Loadouts.Find(id);
        return preset != null;
    }

    override OZ_LoadoutVerdict ForPlayer(string uid, out OZ_LoadoutPreset preset)
    {
        preset = null;

        if (!OZF_Loadouts.Enabled())
            return OZ_LoadoutVerdict.NO_OPINION;

        string org  = OZ_Factions.OrgOfUid(uid);
        string base = OZ_Factions.BaseOfUid(uid);

        OZ_LoadoutPreset hit = null;
        string why = "";

        if (org != "")
        {
            string frank = OZ_Roles.FRankOf(uid);
            if (frank != "")
            {
                hit = OZF_Loadouts.Rung(org, frank);
                why = org + "+" + frank;
            }
            if (!hit)
            {
                hit = OZF_Loadouts.Rung(org, "");
                why = org;
            }
        }

        if (!hit && base != "")
        {
            string rank = OZ_Roles.RankOf(uid);
            if (rank != "")
            {
                hit = OZF_Loadouts.Rung(base, rank);
                why = base + "+" + rank;
            }
            if (!hit)
            {
                hit = OZF_Loadouts.Rung(base, "");
                why = base;
            }
        }

        if (!hit)
        {
            if (base == "")
            {
                // Голий за задумом -- лише коли базова взагалі є, а цьому
                // гравцеві її ще не призначили (R3.4). Немає жодної базової
                // -- це одруківка адміна, і карати за неї гравця не можна.
                if (OZ_Factions.FirstBaseId() == "")
                {
                    OZ_Log.Warn("loadouts: no base faction is declared in OZ_Core_Factions.json - the spawn of " + uid + " is left to the mission");
                    return OZ_LoadoutVerdict.NO_OPINION;
                }
                return OZ_LoadoutVerdict.NAKED;
            }

            // Фракція без жодного пресета -- NO_OPINION і WARNING з іменем
            // фракції, не NAKED (R3.3).
            string named = base;
            if (org != "" && !OZF_Loadouts.HasAnyRung(base))
                named = base;
            else if (org != "")
                named = org;
            OZ_Log.Warn("loadouts: faction " + named + " has no preset - the spawn of " + uid + " is left to the mission");
            return OZ_LoadoutVerdict.NO_OPINION;
        }

        Compose(uid, hit, base, org);
        preset = m_Composed;

        OZ_Log.Dbg("loadouts: " + uid + " ladder hit " + why + " -> " + hit.Id);
        return OZ_LoadoutVerdict.PRESET;
    }

    // Базовий щабель плюс модифікатори (R4.3): спершу модифікатори базової
    // фракції, потім угруповання, кожен список -- у порядку конфігу. Add
    // накопичуються; Replace за слотом -- останній виграє, тому угруповання
    // перебиває базову. Предмети не копіюються: складений пресет тримає
    // посилання на предмети конфігу, і живуть вони, поки живе конфіг.
    private void Compose(string uid, OZ_LoadoutPreset hit, string base, string org)
    {
        m_Composed.Id = hit.Id;
        m_Composed.Items.Clear();
        for (int i = 0; i < hit.Items.Count(); i++)
            m_Composed.Items.Insert(hit.Items[i]);

        ApplyMods(uid, OZF_Loadouts.ModsOf(base));
        if (org != "" && org != base)
            ApplyMods(uid, OZF_Loadouts.ModsOf(org));
    }

    private void ApplyMods(string uid, array<ref OZ_LoadoutMod> mods)
    {
        if (!mods)
            return;

        for (int m = 0; m < mods.Count(); m++)
        {
            OZ_LoadoutMod mod = mods[m];
            if (!mod || mod.Key == "")
                continue;
            if (!OZ_Roles.HasPost(uid, mod.Key) && !OZ_Roles.HasTrait(uid, mod.Key))
                continue;

            for (int r = 0; r < mod.Replace.Count(); r++)
                ReplaceSlot(mod.Replace[r]);
            for (int a = 0; a < mod.Add.Count(); a++)
                m_Composed.Items.Insert(mod.Add[a]);

            OZ_Log.Dbg("loadouts: " + uid + " modifier " + mod.Key + " applied");
        }
    }

    private void ReplaceSlot(OZ_LoadoutItem it)
    {
        if (!it)
            return;

        if (it.SlotName != "")
        {
            for (int i = 0; i < m_Composed.Items.Count(); i++)
            {
                if (m_Composed.Items[i] && m_Composed.Items[i].SlotName == it.SlotName)
                {
                    m_Composed.Items.Set(i, it);
                    return;
                }
            }
        }

        // Без слота (або слот ще порожній) -- заміняти нема чого: додаємо.
        m_Composed.Items.Insert(it);
    }
}
