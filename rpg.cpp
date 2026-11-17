// ============================================================================
//  ХРОНИКИ ИГНАТОРА — большая текстовая RPG на C++17
//  Сборка:  g++ -std=c++17 -O2 -o rpg rpg.cpp
//  Запуск:  ./rpg
//
//  Что внутри:
//   - 3 класса (Воин, Маг, Плут) со своими навыками и ростом характеристик
//   - Уровни, опыт, характеристики (сила, защита, интеллект, ловкость)
//   - Пошаговые бои: атака, навыки, предметы, бегство, крит, уклонение,
//     яд, оглушение, баффы, боссы
//   - Инвентарь, экипировка (оружие / броня / аксессуар), расходники
//   - Карта мира из 8 локаций, время суток (ночью враги сильнее)
//   - Магазин (покупка/продажа), кузнец (крафт и заточка), таверна
//   - Квесты (убить / собрать) с наградами
//   - Случайные события, лагерь, кости в таверне
//   - Смерть со штрафом, сохранение и загрузка
// ============================================================================
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <random>
#include <fstream>
#include <algorithm>
#include <functional>
#include <ctime>
#include <cstdlib>
#ifdef _WIN32
#include <windows.h>
#endif
using namespace std;

// ----------------------------------------------------------------------------
//  Утилиты
// ----------------------------------------------------------------------------
mt19937 rng((unsigned)time(nullptr));
int rnd(int a, int b) { return uniform_int_distribution<int>(a, b)(rng); }
bool chance(int percent) { return rnd(1, 100) <= percent; }

int askInt(int lo, int hi) {
    while (true) {
        cout << "> ";
        string s;
        if (!getline(cin, s)) exit(0);
        try {
            size_t pos;
            int v = stoi(s, &pos);
            if (v >= lo && v <= hi) return v;
        } catch (...) {}
        cout << "Введите число от " << lo << " до " << hi << ".\n";
    }
}
void waitEnter() {
    cout << "\n[Enter — продолжить]";
    string s;
    getline(cin, s);
}
void line() { cout << "------------------------------------------------------------\n"; }

// ----------------------------------------------------------------------------
//  Предметы
// ----------------------------------------------------------------------------
enum IType { WEAPON, ARMOR, ACCESSORY, HEAL, MANA, MATERIAL };

struct Item {
    int id;
    string name;
    IType type;
    int atk, def, hp, mp;  // у экипировки — бонусы; у расходников hp/mp — восстановление
    int price;
    string desc;
};
vector<Item> ITEMS;

enum ItemId {
    I_NONE, I_RUSTY, I_STEEL, I_AXE, I_STAFF1, I_STAFF2, I_DAGGER, I_MITHRIL,
    I_LEATHER, I_CHAIN, I_PLATE, I_ROBE, I_RING_STR, I_AMULET, I_RING_WIS,
    I_HP_S, I_HP_L, I_MP_S, I_MP_L, I_PELT, I_FANG, I_ORE, I_SCALE, I_CRYSTAL,
    I_WOLFARMOR, I_SPIDERDAGGER, I_DRAGONARMOR, I_DRAGONSWORD, I_ELIXIR
};

void initItems() {
    auto add = [&](const string& n, IType t, int atk, int def, int hp, int mp, int price, const string& d) {
        ITEMS.push_back({(int)ITEMS.size(), n, t, atk, def, hp, mp, price, d});
    };
    add("-", MATERIAL, 0, 0, 0, 0, 0, "");
    add("Ржавый меч", WEAPON, 3, 0, 0, 0, 20, "Старый, но острый.");
    add("Стальной меч", WEAPON, 7, 0, 0, 0, 90, "Надёжное оружие стражника.");
    add("Боевой топор", WEAPON, 10, 0, 0, 0, 160, "Тяжёлый и беспощадный.");
    add("Посох ученика", WEAPON, 2, 0, 0, 10, 60, "Усиливает запас маны.");
    add("Посох архимага", WEAPON, 5, 0, 0, 30, 280, "Гудит от силы.");
    add("Кинжал", WEAPON, 5, 0, 0, 0, 50, "Лёгкий и быстрый.");
    add("Мифриловый клинок", WEAPON, 14, 0, 0, 0, 350, "Эльфийская ковка.");
    add("Кожаная куртка", ARMOR, 0, 2, 0, 0, 30, "Лучше, чем ничего.");
    add("Кольчуга", ARMOR, 0, 5, 0, 0, 120, "Звенит при каждом шаге.");
    add("Латные доспехи", ARMOR, 0, 10, 0, 0, 340, "Почти непробиваемы.");
    add("Роба мага", ARMOR, 0, 1, 0, 15, 70, "Шёлк, вышитый рунами.");
    add("Кольцо силы", ACCESSORY, 3, 0, 0, 0, 100, "+3 к атаке.");
    add("Амулет жизни", ACCESSORY, 0, 0, 25, 0, 120, "+25 к здоровью.");
    add("Кольцо мудрости", ACCESSORY, 0, 0, 0, 20, 120, "+20 к мане.");
    add("Малое зелье лечения", HEAL, 0, 0, 30, 0, 15, "Восстанавливает 30 HP.");
    add("Большое зелье лечения", HEAL, 0, 0, 80, 0, 40, "Восстанавливает 80 HP.");
    add("Малое зелье маны", MANA, 0, 0, 0, 20, 15, "Восстанавливает 20 MP.");
    add("Большое зелье маны", MANA, 0, 0, 0, 50, 40, "Восстанавливает 50 MP.");
    add("Волчья шкура", MATERIAL, 0, 0, 0, 0, 8, "Материал для крафта.");
    add("Клык паука", MATERIAL, 0, 0, 0, 0, 10, "Материал для крафта.");
    add("Железная руда", MATERIAL, 0, 0, 0, 0, 12, "Материал для кузнеца.");
    add("Драконья чешуя", MATERIAL, 0, 0, 0, 0, 100, "Тёплая на ощупь.");
    add("Магический кристалл", MATERIAL, 0, 0, 0, 0, 25, "Пульсирует светом.");
    add("Волчья броня", ARMOR, 0, 7, 10, 0, 150, "Выкована кузнецом.");
    add("Паучий кинжал", WEAPON, 9, 0, 0, 0, 150, "Покрыт ядовитой патиной.");
    add("Доспех дракона", ARMOR, 0, 16, 40, 0, 800, "Лучшая броня в мире.");
    add("Меч дракона", WEAPON, 22, 0, 0, 0, 900, "Горит в темноте.");
    add("Эликсир жизни", HEAL, 0, 0, 150, 50, 120, "Восстанавливает 150 HP и 50 MP.");
}

// ----------------------------------------------------------------------------
//  Навыки
// ----------------------------------------------------------------------------
enum SKind { S_DMG, S_HEAL, S_BUFF, S_POISON, S_STUN, S_CRIT, S_GUARD, S_MULTI };
struct Skill {
    string name;
    int cls, lvl, cost, kind;
    double mult;
    bool magic;
    string desc;
};
vector<Skill> SKILLS;

void initSkills() {
    // Воин (0)
    SKILLS.push_back({"Мощный удар", 0, 1, 4, S_DMG, 1.8, false, "Сильный удар (x1.8)"});
    SKILLS.push_back({"Боевой клич", 0, 3, 6, S_BUFF, 0, false, "Атака +50% на 4 хода"});
    SKILLS.push_back({"Оглушающий удар", 0, 5, 8, S_STUN, 1.2, false, "Шанс оглушить врага"});
    SKILLS.push_back({"Вихрь клинков", 0, 8, 14, S_DMG, 2.8, false, "Разрушительный удар (x2.8)"});
    // Маг (1)
    SKILLS.push_back({"Огненный шар", 1, 1, 6, S_DMG, 2.0, true, "Магический урон (x2.0)"});
    SKILLS.push_back({"Исцеление", 1, 2, 8, S_HEAL, 2.5, true, "Лечит по силе интеллекта"});
    SKILLS.push_back({"Ледяная стрела", 1, 4, 9, S_STUN, 1.6, true, "Урон и шанс заморозить"});
    SKILLS.push_back({"Метеор", 1, 8, 22, S_DMG, 3.6, true, "Небесная кара (x3.6)"});
    // Плут (2)
    SKILLS.push_back({"Удар в спину", 2, 1, 4, S_CRIT, 2.0, false, "Высокий шанс крита"});
    SKILLS.push_back({"Ядовитый клинок", 2, 3, 6, S_POISON, 1.0, false, "Отравляет врага"});
    SKILLS.push_back({"Уклонение", 2, 5, 5, S_GUARD, 0, false, "Уворот от следующей атаки"});
    SKILLS.push_back({"Смертельный танец", 2, 8, 15, S_MULTI, 1.1, false, "Три быстрых удара"});
}

// ----------------------------------------------------------------------------
//  Классы
// ----------------------------------------------------------------------------
struct ClassInfo {
    string name, desc;
    int hp, mp, str, def, intl, agi;     // стартовые
    int gHp, gMp, gStr, gDef, gInt, gAgi; // рост за уровень
};
vector<ClassInfo> CLASSES = {
    {"Воин", "Крепкий боец ближнего боя.", 60, 15, 8, 4, 2, 3, 12, 3, 3, 2, 1, 1},
    {"Маг", "Хрупкий, но смертоносный заклинатель.", 40, 50, 3, 2, 10, 4, 7, 9, 1, 1, 3, 1},
    {"Плут", "Ловкий и непредсказуемый.", 48, 25, 6, 3, 4, 8, 9, 5, 2, 1, 1, 3},
};

// ----------------------------------------------------------------------------
//  Враги
// ----------------------------------------------------------------------------
struct EnemyT {
    string name;
    int hp, atk, def, xp, gold;
    int special;  // 0 нет, 1 яд, 2 оглушение, 3 тяжёлые удары, 4 регенерация
    vector<pair<int, int>> drops;  // (предмет, шанс %)
    bool boss;
};
vector<EnemyT> ENEMIES;
enum EnemyId { E_RAT, E_WOLF, E_GOBLIN, E_BANDIT, E_SPIDER, E_SKELETON, E_TROLL, E_DARKMAGE, E_OGRE, E_GOLEM, E_DRAGON };

void initEnemies() {
    ENEMIES = {
        {"Гигантская крыса", 18, 6, 0, 8, 5, 0, {{I_HP_S, 20}}, false},
        {"Серый волк", 30, 8, 1, 14, 8, 0, {{I_PELT, 60}, {I_HP_S, 15}}, false},
        {"Гоблин", 35, 10, 2, 18, 14, 0, {{I_RUSTY, 10}, {I_HP_S, 20}}, false},
        {"Бандит", 45, 12, 3, 26, 25, 0, {{I_DAGGER, 10}, {I_MP_S, 15}, {I_LEATHER, 10}}, false},
        {"Гигантский паук", 50, 13, 3, 30, 15, 1, {{I_FANG, 70}}, false},
        {"Скелет-воин", 60, 15, 5, 40, 20, 0, {{I_ORE, 40}, {I_HP_L, 10}}, false},
        {"Пещерный тролль", 120, 21, 6, 80, 60, 4, {{I_ORE, 60}, {I_AXE, 8}}, false},
        {"Тёмный маг", 70, 23, 4, 90, 70, 2, {{I_CRYSTAL, 50}, {I_MP_L, 20}, {I_RING_WIS, 5}}, false},
        {"Горный огр", 160, 27, 9, 130, 90, 3, {{I_ORE, 70}, {I_CHAIN, 12}, {I_HP_L, 30}}, false},
        {"Каменный голем", 220, 29, 14, 180, 120, 0, {{I_CRYSTAL, 80}, {I_PLATE, 6}}, false},
        {"ДРАКОН ИГНАТОР", 600, 46, 16, 1000, 1000, 3, {}, true},
    };
}

// ----------------------------------------------------------------------------
//  Локации
// ----------------------------------------------------------------------------
struct Loc {
    string name, desc;
    bool town;
    vector<int> nb;
    vector<int> enemies;
    int minLvl;
};
vector<Loc> LOCS;

void initLocs() {
    LOCS = {
        {"Деревня Рассвет", "Тихая деревня у подножия гор. Здесь есть всё необходимое.", true, {1, 2}, {}, 1},
        {"Тёмный лес", "Древние деревья едва пропускают свет.", false, {0, 3, 4}, {E_RAT, E_WOLF, E_GOBLIN, E_SPIDER}, 1},
        {"Разбойничий тракт", "Дорога, на которой пропадают торговцы.", false, {0, 4}, {E_GOBLIN, E_BANDIT, E_WOLF}, 2},
        {"Паучья пещера", "Стены затянуты липкой паутиной.", false, {1, 5}, {E_SPIDER, E_SKELETON, E_RAT}, 3},
        {"Заброшенные руины", "Здесь когда-то стоял замок. Теперь тут обитает нежить.", false, {1, 2, 6}, {E_SKELETON, E_DARKMAGE, E_BANDIT}, 5},
        {"Подземелье троллей", "Сырые тоннели, пахнет сырым мясом.", false, {3, 6}, {E_TROLL, E_SKELETON, E_SPIDER}, 6},
        {"Серые горы", "Холодный ветер и острые скалы.", false, {4, 5, 7}, {E_OGRE, E_TROLL, E_DARKMAGE}, 8},
        {"Логово дракона", "Воздух дрожит от жара. Кругом кости и золото.", false, {6}, {E_GOLEM, E_OGRE}, 10},
    };
}

// ----------------------------------------------------------------------------
//  Квесты
// ----------------------------------------------------------------------------
enum QType { Q_KILL, Q_COLLECT };
struct Quest {
    string title, desc;
    int type, target, count, gold, xp, item, minLvl;
};
vector<Quest> QUESTS;

void initQuests() {
    QUESTS = {
        {"Крысиная напасть", "Староста просит избавить амбары от крыс.", Q_KILL, E_RAT, 5, 40, 40, I_HP_S, 1},
        {"Волчья охота", "Волки задирают скот. Убейте 4 волков.", Q_KILL, E_WOLF, 4, 60, 70, I_LEATHER, 1},
        {"Шёлк и яд", "Алхимик ищет 3 клыка пауков.", Q_COLLECT, I_FANG, 3, 80, 90, I_MP_S, 3},
        {"Дорога свободна", "Убейте 4 бандитов на тракте.", Q_KILL, E_BANDIT, 4, 120, 150, I_RING_STR, 3},
        {"Руда для кузнеца", "Кузнец ждёт 5 кусков железной руды.", Q_COLLECT, I_ORE, 5, 150, 200, I_HP_L, 5},
        {"Охота на тролля", "Тролли выходят к деревне. Убейте 3.", Q_KILL, E_TROLL, 3, 250, 400, I_AMULET, 6},
        {"Тёмные искусства", "Некромант-маги осквернили руины. Убейте 3.", Q_KILL, E_DARKMAGE, 3, 300, 500, I_RING_WIS, 6},
        {"Убить дракона", "Игнатор терроризирует королевство. Остановите его!", Q_KILL, E_DRAGON, 1, 2000, 3000, I_ELIXIR, 8},
    };
}

// ----------------------------------------------------------------------------
//  Рецепты кузнеца
// ----------------------------------------------------------------------------
struct Recipe {
    int result;
    vector<pair<int, int>> mats;
    int gold;
};
vector<Recipe> RECIPES = {
    {I_WOLFARMOR, {{I_PELT, 3}}, 30},
    {I_SPIDERDAGGER, {{I_FANG, 4}, {I_DAGGER, 1}}, 40},
    {I_MITHRIL, {{I_ORE, 6}, {I_CRYSTAL, 2}}, 120},
    {I_DRAGONARMOR, {{I_SCALE, 3}, {I_ORE, 4}}, 200},
    {I_DRAGONSWORD, {{I_SCALE, 3}, {I_CRYSTAL, 3}}, 300},
};

// ----------------------------------------------------------------------------
//  Игрок
// ----------------------------------------------------------------------------
struct Player {
    string name = "Герой";
    int cls = 0, level = 1, xp = 0, gold = 50;
    int hp = 1, mp = 1, baseHp = 1, baseMp = 1, str = 1, def = 1, intl = 1, agi = 1;
    int eq[3] = {0, 0, 0};  // оружие, броня, аксессуар
    int sharp = 0;          // заточка оружия
    map<int, int> inv;
    map<int, int> quest;    // 0 нет, 1 активен, 2 сдан
    map<int, int> qprog;    // прогресс убийств
    int loc = 0, tick = 8, kills = 0;
    bool dragonSlain = false;
    // боевые статусы (временные)
    int poison = 0, buff = 0, stun = 0;
    bool guard = false;

    int maxHp() const { int v = baseHp; for (int i = 0; i < 3; i++) v += ITEMS[eq[i]].hp; return v; }
    int maxMp() const { int v = baseMp; for (int i = 0; i < 3; i++) v += ITEMS[eq[i]].mp; return v; }
    int equipAtk() const { int v = sharp; for (int i = 0; i < 3; i++) v += ITEMS[eq[i]].atk; return v; }
    int attack() const { return str + equipAtk() + (buff > 0 ? str / 2 + 2 : 0); }
    int defense() const { int v = def; for (int i = 0; i < 3; i++) v += ITEMS[eq[i]].def; return v; }
    int magic() const { return intl + equipAtk() / 2; }
    int hour() const { return tick % 24; }
    int day() const { return tick / 24 + 1; }
    bool night() const { return hour() >= 20 || hour() < 6; }
    int xpNeed() const { return 40 * level + 10 * level * level; }

    int count(int id) const { auto it = inv.find(id); return it == inv.end() ? 0 : it->second; }
    void add(int id, int n = 1) { inv[id] += n; }
    bool take(int id, int n = 1) {
        if (count(id) < n) return false;
        inv[id] -= n;
        if (inv[id] <= 0) inv.erase(id);
        return true;
    }
    void clampVitals() {
        hp = max(0, min(hp, maxHp()));
        mp = max(0, min(mp, maxMp()));
    }
};

string timeStr(const Player& p) {
    return "День " + to_string(p.day()) + ", " + to_string(p.hour()) + ":00" + (p.night() ? " (ночь)" : "");
}

void showStatus(const Player& p) {
    line();
    cout << p.name << " — " << CLASSES[p.cls].name << ", уровень " << p.level << "\n";
    cout << "Опыт: " << p.xp << "/" << p.xpNeed() << "   Золото: " << p.gold << "\n";
    cout << "HP: " << p.hp << "/" << p.maxHp() << "   MP: " << p.mp << "/" << p.maxMp() << "\n";
    cout << "Атака: " << p.attack() << "  Защита: " << p.defense() << "  Магия: " << p.magic()
         << "  Ловкость: " << p.agi << "\n";
    const char* slots[3] = {"Оружие", "Броня", "Аксессуар"};
    for (int i = 0; i < 3; i++) {
        cout << slots[i] << ": " << (p.eq[i] ? ITEMS[p.eq[i]].name : "—");
        if (i == 0 && p.sharp) cout << " +" << p.sharp;
        cout << "\n";
    }
    cout << "Навыки: ";
    bool any = false;
    for (auto& s : SKILLS)
        if (s.cls == p.cls && s.lvl <= p.level) { cout << (any ? ", " : "") << s.name; any = true; }
    if (!any) cout << "—";
    cout << "\nУбито врагов: " << p.kills << "\n";
    line();
}

void levelUp(Player& p) {
    while (p.xp >= p.xpNeed()) {
        p.xp -= p.xpNeed();
        p.level++;
        auto& c = CLASSES[p.cls];
        p.baseHp += c.gHp; p.baseMp += c.gMp; p.str += c.gStr;
        p.def += c.gDef; p.intl += c.gInt; p.agi += c.gAgi;
        p.hp = p.maxHp(); p.mp = p.maxMp();
        cout << "\n*** УРОВЕНЬ " << p.level << "! Здоровье и мана восстановлены. ***\n";
        for (auto& s : SKILLS)
            if (s.cls == p.cls && s.lvl == p.level) cout << "*** Новый навык: " << s.name << " — " << s.desc << " ***\n";
    }
}

// ----------------------------------------------------------------------------
//  Предметы: использование и экипировка
// ----------------------------------------------------------------------------
bool useConsumable(Player& p, int id) {
    const Item& it = ITEMS[id];
    if (it.type != HEAL && it.type != MANA) return false;
    if (!p.take(id)) return false;
    int h = min(it.hp, p.maxHp() - p.hp), m = min(it.mp, p.maxMp() - p.mp);
    p.hp += h; p.mp += m;
    cout << "Вы использовали «" << it.name << "»";
    if (h > 0) cout << " (+" << h << " HP)";
    if (m > 0) cout << " (+" << m << " MP)";
    cout << ".\n";
    return true;
}

void equipItem(Player& p, int id) {
    const Item& it = ITEMS[id];
    int slot = it.type == WEAPON ? 0 : it.type == ARMOR ? 1 : 2;
    if (!p.take(id)) return;
    if (p.eq[slot]) p.add(p.eq[slot]);
    p.eq[slot] = id;
    p.clampVitals();
    cout << "Вы экипировали «" << it.name << "».\n";
}

void inventoryMenu(Player& p) {
    while (true) {
        line();
        cout << "ИНВЕНТАРЬ   Золото: " << p.gold << "\n";
        vector<int> ids;
        for (auto& kv : p.inv) ids.push_back(kv.first);
        for (size_t i = 0; i < ids.size(); i++) {
            const Item& it = ITEMS[ids[i]];
            cout << " " << i + 1 << ". " << it.name << " x" << p.inv[ids[i]] << " — " << it.desc << "\n";
        }
        if (ids.empty()) cout << " (пусто)\n";
        cout << " 0. Назад\nВыберите предмет, чтобы использовать или надеть.\n";
        int c = askInt(0, (int)ids.size());
        if (c == 0) return;
        int id = ids[c - 1];
        if (ITEMS[id].type == HEAL || ITEMS[id].type == MANA) useConsumable(p, id);
        else if (ITEMS[id].type == MATERIAL) cout << "Это материал для крафта.\n";
        else equipItem(p, id);
    }
}

// ----------------------------------------------------------------------------
//  Бой
// ----------------------------------------------------------------------------
int calcDmg(double raw, int edef) {
    int d = (int)(raw * (0.85 + rnd(0, 30) / 100.0)) - edef / 2;
    return max(1, d);
}

void giveRewards(Player& p, int ei) {
    const EnemyT& t = ENEMIES[ei];
    int xp = t.xp, gold = t.gold + rnd(0, t.gold / 2);
    if (p.night()) xp = xp * 12 / 10;
    cout << "\nПобеда! Вы получили " << xp << " опыта и " << gold << " золота.\n";
    p.xp += xp; p.gold += gold; p.kills++;
    for (auto& d : t.drops)
        if (chance(d.second)) { p.add(d.first); cout << "Добыча: " << ITEMS[d.first].name << "\n"; }
    if (t.boss) { p.add(I_SCALE, 3); cout << "Добыча: Драконья чешуя x3\n"; }
    for (auto& kv : p.quest)
        if (kv.second == 1 && QUESTS[kv.first].type == Q_KILL && QUESTS[kv.first].target == ei)
            p.qprog[kv.first]++;
    levelUp(p);
}

// Возвращает: 1 — победа, 0 — смерть, 2 — бегство
int combat(Player& p, int ei) {
    const EnemyT& t = ENEMIES[ei];
    int ehp = t.hp, ePoison = 0, eStun = 0;
    int eatk = t.atk * (p.night() ? 115 : 100) / 100;
    p.poison = 0; p.buff = 0; p.stun = 0; p.guard = false;

    line();
    cout << (t.boss ? "!!! БОСС: " : "Вам встретился: ") << t.name << " !!!\n";
    if (p.night()) cout << "(Ночью враги яростнее.)\n";

    int result = -1;
    while (result < 0) {
        cout << "\n" << t.name << ": " << ehp << "/" << t.hp << " HP"
             << (ePoison ? " [яд]" : "") << (eStun ? " [оглушён]" : "") << "\n";
        cout << p.name << ": " << p.hp << "/" << p.maxHp() << " HP, " << p.mp << "/" << p.maxMp() << " MP"
             << (p.poison ? " [яд]" : "") << (p.buff ? " [бафф]" : "") << "\n";

        bool acted = false;
        if (p.stun > 0) {
            cout << "Вы оглушены и пропускаете ход!\n";
            p.stun--; acted = true;
        }
        while (!acted) {
            cout << "1. Атаковать  2. Навык  3. Предмет  4. Бежать\n";
            int c = askInt(1, 4);
            if (c == 1) {
                double m = chance(5 + p.agi / 2) ? 1.7 : 1.0;
                int d = calcDmg(p.attack() * m, t.def);
                ehp -= d;
                cout << (m > 1 ? "КРИТИЧЕСКИЙ удар! " : "Вы бьёте врага. ") << "Урон: " << d << "\n";
                acted = true;
            } else if (c == 2) {
                vector<const Skill*> av;
                for (auto& s : SKILLS) if (s.cls == p.cls && s.lvl <= p.level) av.push_back(&s);
                for (size_t i = 0; i < av.size(); i++)
                    cout << " " << i + 1 << ". " << av[i]->name << " (" << av[i]->cost << " MP) — " << av[i]->desc << "\n";
                cout << " 0. Назад\n";
                int k = askInt(0, (int)av.size());
                if (k == 0) continue;
                const Skill& s = *av[k - 1];
                if (p.mp < s.cost) { cout << "Не хватает маны!\n"; continue; }
                p.mp -= s.cost;
                double base = s.magic ? p.magic() : p.attack();
                cout << "Вы применяете «" << s.name << "». ";
                switch (s.kind) {
                    case S_DMG: { int d = calcDmg(base * s.mult, s.magic ? t.def / 2 : t.def); ehp -= d; cout << "Урон: " << d << "\n"; break; }
                    case S_STUN: {
                        int d = calcDmg(base * s.mult, t.def); ehp -= d; cout << "Урон: " << d << "\n";
                        if (chance(65)) { eStun = 1; cout << "Враг оглушён!\n"; }
                        break;
                    }
                    case S_CRIT: {
                        double m = s.mult * (chance(50 + p.agi / 2) ? 1.8 : 1.0);
                        int d = calcDmg(base * m, t.def); ehp -= d;
                        cout << (m > s.mult ? "КРИТ! " : "") << "Урон: " << d << "\n";
                        break;
                    }
                    case S_POISON: {
                        int d = calcDmg(base * s.mult, t.def); ehp -= d; ePoison = 4;
                        cout << "Урон: " << d << ". Враг отравлен!\n";
                        break;
                    }
                    case S_MULTI: {
                        int tot = 0;
                        for (int i = 0; i < 3; i++) tot += calcDmg(base * s.mult, t.def);
                        ehp -= tot; cout << "Три удара, суммарный урон: " << tot << "\n";
                        break;
                    }
                    case S_HEAL: {
                        int h = min((int)(p.intl * s.mult) + 5, p.maxHp() - p.hp);
                        p.hp += h; cout << "Восстановлено " << h << " HP.\n";
                        break;
                    }
                    case S_BUFF: p.buff = 4; cout << "Ваша сила растёт!\n"; break;
                    case S_GUARD: p.guard = true; cout << "Вы готовитесь уклониться.\n"; break;
                }
                acted = true;
            } else if (c == 3) {
                vector<int> ids;
                for (auto& kv : p.inv) if (ITEMS[kv.first].type == HEAL || ITEMS[kv.first].type == MANA) ids.push_back(kv.first);
                if (ids.empty()) { cout << "У вас нет расходников.\n"; continue; }
                for (size_t i = 0; i < ids.size(); i++)
                    cout << " " << i + 1 << ". " << ITEMS[ids[i]].name << " x" << p.inv[ids[i]] << "\n";
                cout << " 0. Назад\n";
                int k = askInt(0, (int)ids.size());
                if (k == 0) continue;
                useConsumable(p, ids[k - 1]);
                acted = true;
            } else {
                if (t.boss) { cout << "От дракона не убежать!\n"; continue; }
                if (chance(min(85, 40 + p.agi))) { cout << "Вам удалось сбежать!\n"; result = 2; break; }
                cout << "Побег не удался!\n";
                acted = true;
            }
        }
        if (result >= 0) break;

        if (ehp <= 0) { giveRewards(p, ei); result = 1; break; }

        // ход врага
        if (ePoison > 0) {
            int d = 4 + p.level; ehp -= d; ePoison--;
            cout << t.name << " страдает от яда (-" << d << ").\n";
            if (ehp <= 0) { giveRewards(p, ei); result = 1; break; }
        }
        if (eStun > 0) {
            cout << t.name << " оглушён и пропускает ход.\n"; eStun--;
        } else {
            if (t.special == 4 && chance(25)) {
                int h = t.hp / 12; ehp = min(t.hp, ehp + h);
                cout << t.name << " восстанавливает " << h << " HP.\n";
            }
            if (p.guard) { cout << "Вы ловко уклоняетесь от атаки!\n"; p.guard = false; }
            else if (chance(min(35, p.agi))) cout << "Вы уворачиваетесь от атаки " << t.name << "!\n";
            else {
                double m = 1.0;
                if (t.special == 3 && chance(25)) { m = 1.8; cout << "Мощный удар! "; }
                int d = max(1, (int)(eatk * m * (0.85 + rnd(0, 30) / 100.0)) - p.defense() / 2);
                p.hp -= d;
                cout << t.name << " наносит вам " << d << " урона.\n";
                if (t.special == 1 && chance(35)) { p.poison = 3; cout << "Вы отравлены!\n"; }
                if (t.special == 2 && chance(20)) { p.stun = 1; cout << "Вы оглушены!\n"; }
            }
        }
        if (p.poison > 0) {
            int d = 3 + p.level / 2; p.hp -= d; p.poison--;
            cout << "Яд наносит вам " << d << " урона.\n";
        }
        if (p.buff > 0) p.buff--;
        if (p.hp <= 0) { result = 0; break; }
    }
    p.poison = 0; p.buff = 0; p.stun = 0; p.guard = false;
    return result;
}

void handleDeath(Player& p) {
    cout << "\n=== ВЫ ПОГИБЛИ ===\n";
    int lost = p.gold / 2;
    p.gold -= lost;
    cout << "Жители деревни нашли вас и выходили. Потеряно золота: " << lost << ".\n";
    p.loc = 0; p.hp = p.maxHp() / 2; p.mp = p.maxMp() / 2; p.tick += 12;
}

// Бой + обработка смерти + эпилог. Возвращает false, если игрок умер.
bool fight(Player& p, int ei) {
    int r = combat(p, ei);
    if (r == 0) { handleDeath(p); waitEnter(); return false; }
    if (r == 1 && ENEMIES[ei].boss) {
        p.dragonSlain = true;
        cout << "\n****************************************************\n"
                " Игнатор повержен! Королевство спасено, а ваше имя\n"
                " навсегда войдёт в легенды. ПОЗДРАВЛЯЕМ С ПОБЕДОЙ!\n"
                " (Вы можете продолжить играть и дальше.)\n"
                "****************************************************\n";
    }
    return true;
}

// ----------------------------------------------------------------------------
//  Магазин, кузнец, таверна, квесты
// ----------------------------------------------------------------------------
void shopMenu(Player& p) {
    vector<int> stock = {I_RUSTY, I_STEEL, I_AXE, I_DAGGER, I_STAFF1, I_STAFF2, I_LEATHER, I_CHAIN, I_PLATE, I_ROBE,
                         I_RING_STR, I_AMULET, I_RING_WIS, I_HP_S, I_HP_L, I_MP_S, I_MP_L};
    while (true) {
        line();
        cout << "ЛАВКА   Золото: " << p.gold << "\n1. Купить  2. Продать  0. Выйти\n";
        int c = askInt(0, 2);
        if (c == 0) return;
        if (c == 1) {
            for (size_t i = 0; i < stock.size(); i++) {
                const Item& it = ITEMS[stock[i]];
                cout << " " << i + 1 << ". " << it.name << " — " << it.price << " зол. (" << it.desc << ")\n";
            }
            cout << " 0. Назад\n";
            int k = askInt(0, (int)stock.size());
            if (k == 0) continue;
            const Item& it = ITEMS[stock[k - 1]];
            if (p.gold < it.price) { cout << "Недостаточно золота.\n"; continue; }
            p.gold -= it.price; p.add(it.id);
            cout << "Куплено: " << it.name << ".\n";
        } else {
            vector<int> ids;
            for (auto& kv : p.inv) ids.push_back(kv.first);
            for (size_t i = 0; i < ids.size(); i++)
                cout << " " << i + 1 << ". " << ITEMS[ids[i]].name << " x" << p.inv[ids[i]]
                     << " — " << ITEMS[ids[i]].price / 2 << " зол.\n";
            if (ids.empty()) { cout << "Нечего продавать.\n"; continue; }
            cout << " 0. Назад\n";
            int k = askInt(0, (int)ids.size());
            if (k == 0) continue;
            int id = ids[k - 1];
            p.take(id); p.gold += ITEMS[id].price / 2;
            cout << "Продано: " << ITEMS[id].name << ".\n";
        }
    }
}

void smithMenu(Player& p) {
    while (true) {
        line();
        cout << "КУЗНИЦА   Золото: " << p.gold << "\n1. Ковать предметы  2. Заточить оружие  0. Выйти\n";
        int c = askInt(0, 2);
        if (c == 0) return;
        if (c == 1) {
            for (size_t i = 0; i < RECIPES.size(); i++) {
                auto& r = RECIPES[i];
                cout << " " << i + 1 << ". " << ITEMS[r.result].name << " — " << r.gold << " зол.; нужно: ";
                for (auto& m : r.mats) cout << ITEMS[m.first].name << " x" << m.second << " (у вас " << p.count(m.first) << ") ";
                cout << "\n";
            }
            cout << " 0. Назад\n";
            int k = askInt(0, (int)RECIPES.size());
            if (k == 0) continue;
            auto& r = RECIPES[k - 1];
            bool ok = p.gold >= r.gold;
            for (auto& m : r.mats) if (p.count(m.first) < m.second) ok = false;
            if (!ok) { cout << "Не хватает материалов или золота.\n"; continue; }
            p.gold -= r.gold;
            for (auto& m : r.mats) p.take(m.first, m.second);
            p.add(r.result);
            cout << "Кузнец выковал: " << ITEMS[r.result].name << "!\n";
        } else {
            if (!p.eq[0]) { cout << "Сначала наденьте оружие.\n"; continue; }
            if (p.sharp >= 5) { cout << "Оружие уже заточено до предела (+5).\n"; continue; }
            int cost = 50 * (p.sharp + 1);
            cout << "Заточка до +" << p.sharp + 1 << ": " << cost << " зол. и 1 железная руда (есть: " << p.count(I_ORE) << "). 1 — да, 0 — нет\n";
            if (askInt(0, 1) == 0) continue;
            if (p.gold < cost || p.count(I_ORE) < 1) { cout << "Не хватает ресурсов.\n"; continue; }
            p.gold -= cost; p.take(I_ORE); p.sharp++;
            cout << "Оружие заточено! Бонус к атаке: +" << p.sharp << ".\n";
        }
    }
}

void tavernMenu(Player& p) {
    static const char* rumors[] = {
        "«Пауки в пещере отравляют — берите противоядие... то есть, побольше зелий.»",
        "«Из драконьей чешуи кузнец куёт лучшие доспехи в мире.»",
        "«Ночью в лесу опаснее, но и награда выше.»",
        "«Тролли регенерируют. Бейте их быстро и сильно!»",
        "«Тёмные маги оглушают заклинаниями. Не зевайте.»",
        "«Дракон Игнатор живёт на самой вершине. Лучше иметь 10-й уровень.»",
        "«Заточка оружия у кузнеца — недорогой способ стать сильнее.»",
    };
    while (true) {
        line();
        cout << "ТАВЕРНА «Пьяный грифон»   Золото: " << p.gold << "\n";
        cout << "1. Снять комнату (10 зол., полное восстановление)\n2. Послушать слухи\n3. Сыграть в кости\n0. Выйти\n";
        int c = askInt(0, 3);
        if (c == 0) return;
        if (c == 1) {
            if (p.gold < 10) { cout << "Не хватает золота.\n"; continue; }
            p.gold -= 10;
            int h = p.hour();
            p.tick += (h < 8) ? 8 - h : 24 - h + 8;
            p.hp = p.maxHp(); p.mp = p.maxMp();
            cout << "Вы отлично выспались. " << timeStr(p) << ". Здоровье и мана полностью восстановлены.\n";
        } else if (c == 2) {
            cout << rumors[rnd(0, 6)] << "\n";
        } else {
            if (p.gold < 1) { cout << "У вас нет денег.\n"; continue; }
            int maxBet = min(p.gold, 100);
            cout << "Ваша ставка (0 — отмена, макс. " << maxBet << "):\n";
            int bet = askInt(0, maxBet);
            if (bet == 0) continue;
            int a = rnd(1, 6) + rnd(1, 6), b = rnd(1, 6) + rnd(1, 6);
            cout << "Вы бросили: " << a << ", соперник: " << b << "\n";
            if (a > b) { p.gold += bet; cout << "Вы выиграли " << bet << " золота!\n"; }
            else if (a < b) { p.gold -= bet; cout << "Вы проиграли " << bet << " золота.\n"; }
            else cout << "Ничья, ставка возвращена.\n";
        }
    }
}

bool questComplete(const Player& p, int q) {
    auto& Q = QUESTS[q];
    if (Q.type == Q_KILL) { auto it = p.qprog.find(q); return it != p.qprog.end() && it->second >= Q.count; }
    return p.count(Q.target) >= Q.count;
}

string questProgress(const Player& p, int q) {
    auto& Q = QUESTS[q];
    int cur = Q.type == Q_KILL ? (p.qprog.count(q) ? p.qprog.at(q) : 0) : p.count(Q.target);
    return to_string(min(cur, Q.count)) + "/" + to_string(Q.count);
}

void journal(const Player& p) {
    line();
    cout << "ЖУРНАЛ ЗАДАНИЙ\n";
    bool any = false;
    for (auto& kv : p.quest)
        if (kv.second == 1) {
            any = true;
            cout << " • " << QUESTS[kv.first].title << " [" << questProgress(p, kv.first) << "]"
                 << (questComplete(p, kv.first) ? " — ГОТОВО, сдайте на доске заданий" : "") << "\n   " << QUESTS[kv.first].desc << "\n";
        }
    if (!any) cout << "Активных заданий нет.\n";
}

void questBoard(Player& p) {
    while (true) {
        line();
        cout << "ДОСКА ЗАДАНИЙ\n";
        vector<pair<int, int>> opts;  // (тип: 1 взять, 2 сдать; id)
        for (size_t i = 0; i < QUESTS.size(); i++) {
            int st = p.quest.count(i) ? p.quest[i] : 0;
            if (st == 0 && p.level >= QUESTS[i].minLvl) {
                opts.push_back({1, (int)i});
                cout << " " << opts.size() << ". [Взять] " << QUESTS[i].title << " — " << QUESTS[i].desc
                     << " Награда: " << QUESTS[i].gold << " зол., " << QUESTS[i].xp << " оп.\n";
            } else if (st == 1) {
                opts.push_back({2, (int)i});
                cout << " " << opts.size() << ". [Сдать] " << QUESTS[i].title << " [" << questProgress(p, i) << "]\n";
            }
        }
        if (opts.empty()) cout << "Пока нет подходящих заданий. Возвращайтесь на новых уровнях.\n";
        cout << " 0. Назад\n";
        int c = askInt(0, (int)opts.size());
        if (c == 0) return;
        int q = opts[c - 1].second;
        if (opts[c - 1].first == 1) {
            p.quest[q] = 1; p.qprog[q] = 0;
            cout << "Задание принято: " << QUESTS[q].title << ".\n";
        } else {
            if (!questComplete(p, q)) { cout << "Задание ещё не выполнено.\n"; continue; }
            auto& Q = QUESTS[q];
            if (Q.type == Q_COLLECT) p.take(Q.target, Q.count);
            p.quest[q] = 2; p.gold += Q.gold; p.xp += Q.xp;
            cout << "Задание выполнено! Награда: " << Q.gold << " золота, " << Q.xp << " опыта";
            if (Q.item) { p.add(Q.item); cout << ", предмет «" << ITEMS[Q.item].name << "»"; }
            cout << ".\n";
            levelUp(p);
        }
    }
}

// ----------------------------------------------------------------------------
//  Путешествия и исследование
// ----------------------------------------------------------------------------
void randomEvent(Player& p) {
    int tier = max(1, p.level);
    switch (rnd(0, 4)) {
        case 0: { int g = rnd(10, 30) * tier / 2 + 5; p.gold += g; cout << "Вы нашли старый сундук с " << g << " золотом!\n"; break; }
        case 1: { int h = p.maxHp() / 3; p.hp = min(p.maxHp(), p.hp + h); cout << "Вы нашли целебный родник. Восстановлено " << h << " HP.\n"; break; }
        case 2: {
            int d = rnd(3, 8) + tier;
            p.hp -= d;
            cout << "Вы угодили в ловушку! Потеряно " << d << " HP.\n";
            if (p.hp <= 0) { p.hp = 1; cout << "Вы чудом остались живы.\n"; }
            break;
        }
        case 3: {
            cout << "Вам встретился странствующий торговец. Он предлагает «Большое зелье лечения» за 30 золота.\n1 — купить, 0 — отказаться\n";
            if (askInt(0, 1) == 1) {
                if (p.gold >= 30) { p.gold -= 30; p.add(I_HP_L); cout << "Сделка совершена.\n"; }
                else cout << "У вас не хватает золота.\n";
            }
            break;
        }
        default: {
            cout << "Вы наткнулись на древнее святилище. Помолиться? 1 — да, 0 — нет\n";
            if (askInt(0, 1) == 1) {
                if (chance(60)) { p.baseHp += 3; cout << "Боги благосклонны: максимальное здоровье +3!\n"; }
                else { p.hp = max(1, p.hp - 8); cout << "Святилище отвергло вас. Вы потеряли 8 HP.\n"; }
            }
            break;
        }
    }
    p.clampVitals();
}

void explore(Player& p) {
    p.tick += 1;
    auto& L = LOCS[p.loc];
    cout << "\nВы исследуете окрестности (" << timeStr(p) << ")...\n";
    if (chance(70)) {
        int ei = L.enemies[rnd(0, (int)L.enemies.size() - 1)];
        fight(p, ei);
    } else {
        randomEvent(p);
    }
}

void camp(Player& p) {
    p.tick += 6;
    cout << "\nВы разбили лагерь и отдохнули (" << timeStr(p) << ").\n";
    if (chance(30)) {
        cout << "Ночью на вас напали!\n";
        auto& L = LOCS[p.loc];
        if (!fight(p, L.enemies[rnd(0, (int)L.enemies.size() - 1)])) return;
    }
    p.hp = min(p.maxHp(), p.hp + p.maxHp() * 4 / 10);
    p.mp = min(p.maxMp(), p.mp + p.maxMp() * 4 / 10);
    cout << "Вы немного восстановили силы. HP: " << p.hp << "/" << p.maxHp() << ", MP: " << p.mp << "/" << p.maxMp() << "\n";
}

void travel(Player& p) {
    auto& L = LOCS[p.loc];
    cout << "\nКуда отправиться?\n";
    for (size_t i = 0; i < L.nb.size(); i++)
        cout << " " << i + 1 << ". " << LOCS[L.nb[i]].name << " (рекомендуемый уровень: " << LOCS[L.nb[i]].minLvl << ")\n";
    cout << " 0. Остаться\n";
    int c = askInt(0, (int)L.nb.size());
    if (c == 0) return;
    int dest = L.nb[c - 1];
    if (dest == 7 && p.level < 8) {
        cout << "Путь в логово слишком опасен. Рекомендуется минимум 8 уровень. Всё равно идти? 1 — да, 0 — нет\n";
        if (askInt(0, 1) == 0) return;
    }
    p.loc = dest; p.tick += 2;
    cout << "\nВы прибыли: " << LOCS[dest].name << ". " << LOCS[dest].desc << "\n";
    if (!LOCS[dest].town && chance(25)) {
        cout << "На дороге вас подстерегала засада!\n";
        fight(p, LOCS[dest].enemies[rnd(0, (int)LOCS[dest].enemies.size() - 1)]);
    }
}

// ----------------------------------------------------------------------------
//  Сохранение / загрузка
// ----------------------------------------------------------------------------
const char* SAVE_FILE = "savegame.txt";

void saveGame(const Player& p) {
    ofstream f(SAVE_FILE);
    if (!f) { cout << "Не удалось сохранить игру.\n"; return; }
    f << p.name << "\n"
      << p.cls << " " << p.level << " " << p.xp << " " << p.gold << " " << p.hp << " " << p.mp << " "
      << p.baseHp << " " << p.baseMp << " " << p.str << " " << p.def << " " << p.intl << " " << p.agi << " "
      << p.eq[0] << " " << p.eq[1] << " " << p.eq[2] << " " << p.sharp << " " << p.loc << " " << p.tick << " "
      << p.kills << " " << (int)p.dragonSlain << "\n";
    f << p.inv.size() << "\n";
    for (auto& kv : p.inv) f << kv.first << " " << kv.second << "\n";
    f << p.quest.size() << "\n";
    for (auto& kv : p.quest) f << kv.first << " " << kv.second << " " << (p.qprog.count(kv.first) ? p.qprog.at(kv.first) : 0) << "\n";
    cout << "Игра сохранена.\n";
}

bool loadGame(Player& p) {
    ifstream f(SAVE_FILE);
    if (!f) return false;
    Player q;
    getline(f, q.name);
    int dr;
    f >> q.cls >> q.level >> q.xp >> q.gold >> q.hp >> q.mp >> q.baseHp >> q.baseMp >> q.str >> q.def >> q.intl >> q.agi
      >> q.eq[0] >> q.eq[1] >> q.eq[2] >> q.sharp >> q.loc >> q.tick >> q.kills >> dr;
    q.dragonSlain = dr;
    size_t n;
    f >> n;
    for (size_t i = 0; i < n; i++) { int id, c; f >> id >> c; q.inv[id] = c; }
    f >> n;
    for (size_t i = 0; i < n; i++) { int id, st, pr; f >> id >> st >> pr; q.quest[id] = st; q.qprog[id] = pr; }
    if (!f) return false;
    p = q;
    return true;
}

// ----------------------------------------------------------------------------
//  Создание персонажа
// ----------------------------------------------------------------------------
void newCharacter(Player& p) {
    cout << "\nКак вас зовут, герой?\n> ";
    string n;
    getline(cin, n);
    p = Player();
    p.name = n.empty() ? "Герой" : n;
    cout << "\nВыберите класс:\n";
    for (size_t i = 0; i < CLASSES.size(); i++)
        cout << " " << i + 1 << ". " << CLASSES[i].name << " — " << CLASSES[i].desc << "\n";
    p.cls = askInt(1, (int)CLASSES.size()) - 1;
    auto& c = CLASSES[p.cls];
    p.baseHp = c.hp; p.baseMp = c.mp; p.str = c.str; p.def = c.def; p.intl = c.intl; p.agi = c.agi;
    p.hp = p.maxHp(); p.mp = p.maxMp();
    if (p.cls == 0) { p.eq[0] = I_RUSTY; p.eq[1] = I_LEATHER; p.add(I_HP_S, 2); }
    else if (p.cls == 1) { p.eq[0] = I_STAFF1; p.eq[1] = I_ROBE; p.add(I_MP_S, 2); p.add(I_HP_S, 1); }
    else { p.eq[0] = I_DAGGER; p.eq[1] = I_LEATHER; p.add(I_HP_S, 2); }
    p.clampVitals();
    p.hp = p.maxHp(); p.mp = p.maxMp();

    cout << "\nКоролевство трепещет: огненный дракон Игнатор поселился в горах и сжигает деревни.\n"
            "Вы, " << p.name << ", начинаете свой путь в деревне Рассвет. Станьте сильнее — и бросьте вызов дракону!\n";
    waitEnter();
}

// ----------------------------------------------------------------------------
//  Основной цикл
// ----------------------------------------------------------------------------
void gameLoop(Player& p) {
    bool running = true;
    while (running) {
        auto& L = LOCS[p.loc];
        line();
        cout << "[" << L.name << "] " << timeStr(p) << "   HP " << p.hp << "/" << p.maxHp()
             << "  MP " << p.mp << "/" << p.maxMp() << "  Зол. " << p.gold << "\n";
        cout << L.desc << "\n";

        vector<pair<string, function<void()>>> menu;
        if (L.town) {
            menu.push_back({"Лавка торговца", [&] { shopMenu(p); }});
            menu.push_back({"Кузница", [&] { smithMenu(p); }});
            menu.push_back({"Таверна", [&] { tavernMenu(p); }});
            menu.push_back({"Доска заданий", [&] { questBoard(p); }});
        } else {
            menu.push_back({"Исследовать местность", [&] { explore(p); }});
            menu.push_back({"Разбить лагерь", [&] { camp(p); }});
            if (p.loc == 7)
                menu.push_back({"Сразиться с драконом Игнатором", [&] { fight(p, E_DRAGON); }});
        }
        menu.push_back({"Отправиться в путь", [&] { travel(p); }});
        menu.push_back({"Инвентарь", [&] { inventoryMenu(p); }});
        menu.push_back({"Персонаж", [&] { showStatus(p); }});
        menu.push_back({"Журнал заданий", [&] { journal(p); }});
        menu.push_back({"Сохранить игру", [&] { saveGame(p); }});
        menu.push_back({"Выйти из игры", [&] { running = false; }});

        for (size_t i = 0; i < menu.size(); i++) cout << " " << i + 1 << ". " << menu[i].first << "\n";
        int c = askInt(1, (int)menu.size());
        menu[c - 1].second();
    }
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    initItems(); initSkills(); initEnemies(); initLocs(); initQuests();

    cout << "============================================\n"
            "       ХРОНИКИ ИГНАТОРА — текстовая RPG\n"
            "============================================\n";
    Player p;
    while (true) {
        cout << "1. Новая игра\n2. Загрузить игру\n3. Выход\n";
        int c = askInt(1, 3);
        if (c == 1) { newCharacter(p); break; }
        if (c == 2) {
            if (loadGame(p)) { cout << "Игра загружена. С возвращением, " << p.name << "!\n"; break; }
            cout << "Сохранение не найдено или повреждено.\n";
        }
        if (c == 3) return 0;
    }
    gameLoop(p);
    cout << "До новых встреч!\n";
    return 0;
}
