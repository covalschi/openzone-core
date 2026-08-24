// Просторовий пошук гравців -- спільна служба ядра.
//
// Живе тут, а не в КПК, бо потрібна щонайменше трьом: транспондеру на карті,
// матриці «хто кого чує» в рації, і прокси-чату. Три копії того самого коду
// означали б три різні швидкості й три різні набори помилок.
//
// ЧОМУ НЕ В ЛОБ. GetGame().GetPlayers() дає ВСІХ, і «хто поруч зі мною» в лоб
// -- це N порівнянь на кожного питальника, тобто N² на сервер. На 80 гравцях
// це 6400 перевірок відстані на КОЖЕН прохід. Скрипти DayZ живуть в одному
// потоці, і такий прохід щотіка з'їдає серверний FPS напряму.
//
// ЩО РОБИМО НАТОМІСТЬ. Гравці розкладаються по сітці клітин; запит «хто в
// радіусі R» обходить лише ті клітини, які радіус перетинає. Для 80 гравців
// на карті 15 км це майже завжди одна-дві клітини, тобто одиниці перевірок
// замість тисяч.
//
// Сітка перебудовується не частіше REBUILD_SECONDS. Гравець за секунду
// пробігає метрів шість; для «хто в радіусі 500 м» це шум, а перебудова
// щотіка коштувала б рівно те, чого ми уникаємо.

class OZ_Spatial
{
    // Сторона клітини в метрах. 128 обрано так, щоб типовий запит (радіус
    // 100-500 м) чіпав одиниці клітин, а сама сітка на карті 15x15 км
    // лишалась близько 14 тисяч можливих ключів -- при 80 гравцях зайнятими
    // будуть щонайбільше 80 із них.
    private static const float CELL = 128.0;

    private static const float REBUILD_SECONDS = 1.0;

    private static ref map<int, ref array<Man>> s_Cells;
    private static int s_BuiltAt = 0;

    private static int KeyOf(vector pos)
    {
        // Координати світу невід'ємні, тож зсув не потрібен. 16 біт на вісь
        // вистачає з запасом: 65536 * 128 м = 8388 км.
        int cx = Math.Floor(pos[0] / CELL);
        int cz = Math.Floor(pos[2] / CELL);
        return (cx << 16) | (cz & 0xFFFF);
    }

    // Перебудова -- ліниво, на першому запиті після того, як сітка застаріла.
    // Без таймера: немає запитів -- немає й роботи.
    private static void EnsureFresh()
    {
        int now = GetGame().GetTime();

        if (s_Cells && (now - s_BuiltAt) < REBUILD_SECONDS * 1000)
            return;

        s_Cells = new map<int, ref array<Man>>();
        s_BuiltAt = now;

        array<Man> players = new array<Man>();
        GetGame().GetPlayers(players);

        for (int i = 0; i < players.Count(); i++)
        {
            Man m = players[i];
            if (!m || !m.IsAlive())
                continue;

            int key = KeyOf(m.GetPosition());

            array<Man> cell;
            if (!s_Cells.Find(key, cell))
            {
                cell = new array<Man>();
                s_Cells.Insert(key, cell);
            }
            cell.Insert(m);
        }
    }

    // Хто в радіусі. Відстань ТРИВИМІРНА, і це свідомо: сталкер на триста
    // метрів нижче в бункері -- не сусід, хоч би як збігались координати на
    // площині. Рушійні запити радіуса, навпаки, міряють по горизонталі, тож
    // числа звідси й звідти сходитись не зобов'язані.
    static void PlayersInRadius(vector center, float radius, out array<Man> found)
    {
        if (!found)
            found = new array<Man>();
        found.Clear();

        if (radius <= 0)
            return;

        EnsureFresh();

        int cx0 = Math.Floor((center[0] - radius) / CELL);
        int cx1 = Math.Floor((center[0] + radius) / CELL);
        int cz0 = Math.Floor((center[2] - radius) / CELL);
        int cz1 = Math.Floor((center[2] + radius) / CELL);

        float r2 = radius * radius;

        for (int cx = cx0; cx <= cx1; cx++)
        {
            for (int cz = cz0; cz <= cz1; cz++)
            {
                int key = (cx << 16) | (cz & 0xFFFF);

                array<Man> cell;
                if (!s_Cells.Find(key, cell))
                    continue;

                for (int i = 0; i < cell.Count(); i++)
                {
                    Man m = cell[i];
                    if (!m)
                        continue;

                    if (vector.DistanceSq(m.GetPosition(), center) <= r2)
                        found.Insert(m);
                }
            }
        }
    }

    // Скільки клітин зайнято. Для діагностики: якщо на 80 гравцях зайнята
    // одна клітина, значить усі стоять купою й сітка нічого не економить --
    // це нормально, але знати про це корисно.
    static int OccupiedCells()
    {
        EnsureFresh();
        return s_Cells.Count();
    }

    // Примусово застарити сітку. Потрібно там, де рішення мусить спиратись на
    // позицію ЦІЄЇ ж миті -- наприклад одразу після телепорту.
    static void Invalidate()
    {
        s_BuiltAt = 0;
    }
}
