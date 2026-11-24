// ============================================================================
//  ХРОНИКИ ИГНАТОРА II — большая текстовая RPG на C++17
//
//  Сборка:  g++ -std=c++17 -O2 -o rpg rpg.cpp
//  Запуск:  ./rpg            (флаг --nocolor отключает цвета, --color форсирует)
//
//  Возможности:
//   * 3 класса с пассивными способностями, навыки до 10 уровня, свободные очки
//   * Пошаговые бои: крит, уклонение, яд, оглушение, бомбы, элитные враги,
//     боссы с телеграфируемыми атаками и фазой ярости
//   * Погода (ясно/дождь/туман/гроза) и смена дня и ночи влияют на игру
//   * Случайные события с выбором: сундуки и мимики, загадки, странники,
//     грибы, святилища, торговцы
//   * Деревня: лавка (товары дня, скидки за репутацию), кузница (шанс
//     неудачной заточки), алхимик, таверна (кости, эль, спутники),
//     храм (лечение, благословение), арена с «банком», доска заказов
//   * Сбор ресурсов, крафт, ежедневные заказы, квесты, репутация
//   * Спутники, бестиарий, достижения, карта мира
//   * 3 слота сохранения + автосохранение
//   * Стихии (огонь, лёд, яд, свет, молния): слабости и сопротивления врагов,
//     горение, заморозка; элементные бомбы и оружие
//   * Особый приём класса, который заряжается в бою (берсерк / арканный взрыв /
//     танец теней)
//   * Редкая экипировка (редкая, эпическая, легендарная) со случайными свойствами:
//     крит, вампиризм, уклонение, стихия; сохраняется вместе с игрой
//   * «Бездна» — процедурное подземелье: выбор дверей, стражи этажей,
//     контрольные точки, тёмный торговец
// ============================================================================
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <random>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif
using namespace std;

// ============================================================================
//  Случайность
// ============================================================================
mt19937 rng((unsigned)time(nullptr));
int rnd(int a, int b) {
    if (a > b) swap(a, b);
    return uniform_int_distribution<int>(a, b)(rng);
}
bool chance(int percent) { return rnd(1, 100) <= percent; }
template <class T>
const T& pickOne(const vector<T>& v) { return v[rnd(0, (int)v.size() - 1)]; }

// ============================================================================
//  ИНТЕРФЕЙС: цвета, рамки, полоски
// ============================================================================
bool useColor = true;
const int W = 66;  // внутренняя ширина панелей

namespace Col {
const char* RESET = "\033[0m";
const char* BOLD = "\033[1m";
const char* RED = "\033[31m";
const char* GREEN = "\033[32m";
const char* YELLOW = "\033[33m";
const char* BLUE = "\033[94m";
const char* MAGENTA = "\033[35m";
const char* CYAN = "\033[36m";
const char* GRAY = "\033[90m";
const char* ORANGE = "\033[38;5;208m";
}  // namespace Col

string paint(const string& s, const string& code) { return useColor ? code + s + Col::RESET : s; }
string red(const string& s) { return paint(s, Col::RED); }
string green(const string& s) { return paint(s, Col::GREEN); }
string yellow(const string& s) { return paint(s, Col::YELLOW); }
string blue(const string& s) { return paint(s, Col::BLUE); }
string magenta(const string& s) { return paint(s, Col::MAGENTA); }
string cyan(const string& s) { return paint(s, Col::CYAN); }
string gray(const string& s) { return paint(s, Col::GRAY); }
string orange(const string& s) { return paint(s, Col::ORANGE); }
string bold(const string& s) { return paint(s, Col::BOLD); }

// Видимая длина строки: UTF-8 символы считаются за один, ANSI-коды — за ноль.
int vlen(const string& s) {
    int n = 0;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = (unsigned char)s[i];
        if (c == 0x1b) {
            while (i < s.size() && s[i] != 'm') i++;
            if (i < s.size()) i++;
            continue;
        }
        if ((c & 0xC0) != 0x80) n++;
        i++;
    }
    return n;
}
string clip(const string& s, int n) {  // обрезка по символам UTF-8
    string r;
    int c = 0;
    for (size_t i = 0; i < s.size(); i++) {
        if (((unsigned char)s[i] & 0xC0) != 0x80) { if (c == n) break; c++; }
        r += s[i];
    }
    return r;
}
string rep(const string& s, int n) {
    string r;
    for (int i = 0; i < n; i++) r += s;
    return r;
}
string padR(const string& s, int w) { int n = vlen(s); return n < w ? s + string(w - n, ' ') : s; }
string padL(const string& s, int w) { int n = vlen(s); return n < w ? string(w - n, ' ') + s : s; }
string center(const string& s, int w) {
    int n = vlen(s);
    if (n >= w) return s;
    int l = (w - n) / 2;
    return string(l, ' ') + s + string(w - n - l, ' ');
}
string lr(const string& l, const string& r, int w = W - 2) {
    int sp = w - vlen(l) - vlen(r);
    if (sp < 1) sp = 1;
    return l + string(sp, ' ') + r;
}
vector<string> wrapText(const string& text, int width) {
    vector<string> out;
    istringstream is(text);
    string word, cur;
    while (is >> word) {
        if (cur.empty()) cur = word;
        else if (vlen(cur) + 1 + vlen(word) <= width) cur += " " + word;
        else { out.push_back(cur); cur = word; }
    }
    if (!cur.empty()) out.push_back(cur);
    if (out.empty()) out.push_back("");
    return out;
}

void panel(const string& title, const vector<string>& lines, const char* c = Col::CYAN) {
    int tl = title.empty() ? 0 : vlen(title) + 2;
    string top = paint("╭─", c);
    if (!title.empty()) top += " " + paint(title, string(Col::BOLD) + c) + " ";
    top += paint(rep("─", max(0, W - 1 - tl)) + "╮", c);
    cout << top << "\n";
    for (auto& l : lines) cout << paint("│", c) << " " << padR(l, W - 2) << " " << paint("│", c) << "\n";
    cout << paint("╰" + rep("─", W) + "╯", c) << "\n";
}
void panelText(const string& title, const string& text, const char* c = Col::CYAN) {
    panel(title, wrapText(text, W - 2), c);
}
void banner(const string& t, const char* c = Col::YELLOW) {
    cout << paint("╔" + rep("═", W) + "╗", c) << "\n";
    cout << paint("║", c) << center(paint(t, string(Col::BOLD) + c), W) << paint("║", c) << "\n";
    cout << paint("╚" + rep("═", W) + "╝", c) << "\n";
}
void rule(const string& t = "") {
    if (t.empty()) { cout << gray(rep("─", W + 2)) << "\n"; return; }
    cout << gray("── ") << bold(t) << " " << gray(rep("─", max(0, W + 2 - 4 - vlen(t)))) << "\n";
}

string bar(int cur, int mx, int w, const string& color) {
    if (mx <= 0) mx = 1;
    cur = max(0, min(cur, mx));
    int f = (int)((double)cur * w / mx + 0.5);
    if (cur > 0 && f == 0) f = 1;
    if (f > w) f = w;
    return paint(rep("█", f), color) + paint(rep("░", w - f), Col::GRAY);
}
string hpColor(int cur, int mx) {
    double r = (double)cur / max(1, mx);
    return r > 0.5 ? Col::GREEN : r > 0.25 ? Col::YELLOW : Col::RED;
}

void say(const string& s) { cout << "  " << s << "\n"; }
void good(const string& s) { say(green("✓ ") + s); }
void bad(const string& s) { say(red("✗ ") + s); }
void gain(const string& s) { say(yellow("★ ") + s); }
void note(const string& s) { say(gray("• " + s)); }
void warnMsg(const string& s) { say(orange("! ") + s); }

string key(int n, const string& label) { return cyan("[" + to_string(n) + "]") + " " + label; }
void grid(const vector<string>& cells, int cols = 2) {
    int cw = (W + 2) / cols;
    for (size_t i = 0; i < cells.size(); i += cols) {
        string row = "  ";
        for (int j = 0; j < cols && i + j < cells.size(); j++) row += padR(cells[i + j], cw - 2);
        cout << row << "\n";
    }
}

int askInt(int lo, int hi) {
    while (true) {
        cout << "  " << paint("▶ ", Col::CYAN);
        cout.flush();
        string s;
        if (!getline(cin, s)) { cout << "\n"; exit(0); }
        size_t a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
        if (a != string::npos) {
            s = s.substr(a, b - a + 1);
            bool digits = true;
            for (char ch : s) if (ch < '0' || ch > '9') digits = false;
            if (digits && s.size() <= 6) {
                int v = stoi(s);
                if (v >= lo && v <= hi) return v;
            }
        }
        bad("Введите число от " + to_string(lo) + " до " + to_string(hi) + ".");
    }
}
bool askYesNo(const string& q) {
    say(q);
    grid({key(1, "Да"), key(0, "Нет")}, 2);
    return askInt(0, 1) == 1;
}
int askQty(int mx) {
    if (mx <= 1) return max(0, mx);
    say("Сколько? " + gray("(0 — отмена, максимум " + to_string(mx) + ")"));
    return askInt(0, mx);
}
void waitEnter() {
    cout << gray("\n  [Enter — продолжить]");
    cout.flush();
    string s;
    getline(cin, s);
    cout << "\n";
}

// ============================================================================
//  ДАННЫЕ: предметы
// ============================================================================
enum Elem { EL_PHYS, EL_FIRE, EL_ICE, EL_POISON, EL_HOLY, EL_SHOCK };
const char* EL_NAMES[] = {"физ.", "огонь", "лёд", "яд", "свет", "молния"};
const char* EL_SUFFIX[] = {"", "огня", "льда", "яда", "света", "молний"};
string elemStr(int e) {
    const char* cols[] = {Col::GRAY, Col::ORANGE, Col::CYAN, Col::GREEN, Col::YELLOW, Col::MAGENTA};
    return paint(EL_NAMES[e], cols[e]);
}

enum IType { WEAPON, ARMOR, ACCESSORY, HEAL, MANA, MATERIAL, BOMB, CURE };

struct Item {
    int id;
    string name;
    IType type;
    int atk, def, hp, mp;  // экипировка: бонусы; расходники: hp/mp — эффект
    int price;
    string desc;
    int rarity = 0;  // 0 обычный, 1 редкий, 2 эпический, 3 легендарный
    int crit = 0, steal = 0, dodge = 0;  // бонусы к криту, вампиризму, уклонению (%)
    int elem = 0;                        // стихия оружия / бомбы
    int base = 0;                        // базовый предмет для сгенерированных
};
vector<Item> ITEMS;
int STATIC_ITEMS = 0;  // число «обычных» предметов; всё после — сгенерированная экипировка

enum ItemId {
    I_NONE, I_RUSTY, I_STEEL, I_AXE, I_STAFF1, I_STAFF2, I_DAGGER, I_MITHRIL,
    I_LEATHER, I_CHAIN, I_PLATE, I_ROBE, I_RING_STR, I_AMULET, I_RING_WIS,
    I_HP_S, I_HP_L, I_MP_S, I_MP_L, I_PELT, I_FANG, I_ORE, I_SCALE, I_CRYSTAL,
    I_WOLFARMOR, I_SPIDERDAGGER, I_DRAGONARMOR, I_DRAGONSWORD, I_ELIXIR,
    I_BOMB, I_ANTIDOTE, I_HERB, I_MOONFLOWER, I_TROLLCROWN, I_TROLLCLUB,
    I_ICEBOMB, I_ACID, I_HOLYWATER
};

void initItems() {
    auto add = [&](const string& n, IType t, int atk, int def, int hp, int mp, int price, const string& d) {
        ITEMS.push_back({(int)ITEMS.size(), n, t, atk, def, hp, mp, price, d});
    };
    add("-", MATERIAL, 0, 0, 0, 0, 0, "");
    add("Ржавый меч", WEAPON, 3, 0, 0, 0, 20, "Старый, но острый.");
    add("Стальной меч", WEAPON, 7, 0, 0, 0, 90, "Оружие городской стражи.");
    add("Боевой топор", WEAPON, 10, 0, 0, 0, 160, "Тяжёлый и беспощадный.");
    add("Посох ученика", WEAPON, 2, 0, 0, 10, 60, "Усиливает запас маны.");
    add("Посох архимага", WEAPON, 5, 0, 0, 30, 280, "Гудит от силы.");
    add("Кинжал", WEAPON, 5, 0, 0, 0, 50, "Лёгкий и быстрый.");
    add("Мифриловый клинок", WEAPON, 14, 0, 0, 0, 350, "Эльфийская ковка.");
    add("Кожаная куртка", ARMOR, 0, 2, 0, 0, 30, "Лучше, чем ничего.");
    add("Кольчуга", ARMOR, 0, 5, 0, 0, 120, "Звенит при каждом шаге.");
    add("Латные доспехи", ARMOR, 0, 10, 0, 0, 340, "Почти непробиваемы.");
    add("Роба мага", ARMOR, 0, 1, 0, 15, 70, "Шёлк, вышитый рунами.");
    add("Кольцо силы", ACCESSORY, 3, 0, 0, 0, 100, "Прибавляет мощи ударам.");
    add("Амулет жизни", ACCESSORY, 0, 0, 25, 0, 120, "Хранит жизненную силу.");
    add("Кольцо мудрости", ACCESSORY, 0, 0, 0, 20, 120, "Расширяет запас маны.");
    add("Малое зелье лечения", HEAL, 0, 0, 30, 0, 15, "Красная жидкость в склянке.");
    add("Большое зелье лечения", HEAL, 0, 0, 80, 0, 40, "Густая алая настойка.");
    add("Малое зелье маны", MANA, 0, 0, 0, 20, 15, "Голубая искрящаяся жидкость.");
    add("Большое зелье маны", MANA, 0, 0, 0, 50, 40, "Светится в темноте.");
    add("Волчья шкура", MATERIAL, 0, 0, 0, 0, 8, "Материал для крафта.");
    add("Клык паука", MATERIAL, 0, 0, 0, 0, 10, "Материал для крафта.");
    add("Железная руда", MATERIAL, 0, 0, 0, 0, 12, "Материал для кузнеца.");
    add("Драконья чешуя", MATERIAL, 0, 0, 0, 0, 100, "Тёплая на ощупь.");
    add("Магический кристалл", MATERIAL, 0, 0, 0, 0, 25, "Пульсирует светом.");
    add("Волчья броня", ARMOR, 0, 7, 10, 0, 150, "Выкована кузнецом.");
    add("Паучий кинжал", WEAPON, 9, 0, 0, 0, 150, "Покрыт ядовитой патиной.");
    add("Доспех дракона", ARMOR, 0, 16, 40, 0, 800, "Лучшая броня в мире.");
    add("Меч дракона", WEAPON, 22, 0, 0, 0, 900, "Горит в темноте.");
    add("Эликсир жизни", HEAL, 0, 0, 150, 50, 120, "Редчайшее снадобье алхимиков.");
    add("Бомба", BOMB, 0, 0, 45, 0, 35, "Метается в бою.");
    add("Противоядие", CURE, 0, 0, 0, 0, 20, "Нейтрализует яд в бою.");
    add("Целебная трава", MATERIAL, 0, 0, 0, 0, 5, "Основа многих зелий.");
    add("Лунный цветок", MATERIAL, 0, 0, 0, 0, 14, "Расцветает по ночам.");
    add("Корона Грока", ACCESSORY, 4, 2, 30, 0, 400, "Трофей короля троллей.");
    add("Дубина Грока", WEAPON, 17, 0, 0, 0, 450, "Просто огромная палка.");
    add("Ледяная бомба", BOMB, 0, 0, 42, 0, 38, "Морозный взрыв.");
    add("Кислотная склянка", BOMB, 0, 0, 40, 0, 30, "Разъедает плоть.");
    add("Святая вода", BOMB, 0, 0, 50, 0, 45, "Жжёт нежить и тьму.");
    ITEMS[I_BOMB].elem = EL_FIRE;
    ITEMS[I_ICEBOMB].elem = EL_ICE;
    ITEMS[I_ACID].elem = EL_POISON;
    ITEMS[I_HOLYWATER].elem = EL_HOLY;
    ITEMS[I_BOMB].desc = "Огненный взрыв.";
    ITEMS[I_ANTIDOTE].desc = "Снимает яд и горение.";
    STATIC_ITEMS = (int)ITEMS.size();
    ITEMS.reserve(STATIC_ITEMS + 4000);  // ссылки на предметы не должны инвалидироваться
}

string itemColor(IType t) {
    switch (t) {
        case WEAPON: return Col::YELLOW;
        case ARMOR: return Col::CYAN;
        case ACCESSORY: return Col::MAGENTA;
        case HEAL: return Col::GREEN;
        case MANA: return Col::BLUE;
        case BOMB: return Col::ORANGE;
        case CURE: return Col::GREEN;
        default: return Col::GRAY;
    }
}
string itemName(int id) {
    const Item& it = ITEMS[id];
    const char* rc[] = {Col::GRAY, Col::BLUE, Col::MAGENTA, Col::ORANGE};
    return paint(it.name, it.rarity > 0 ? string(rc[it.rarity]) : itemColor(it.type));
}
string statsStr(const Item& it) {
    vector<string> v;
    if (it.type == HEAL || it.type == MANA) {
        if (it.hp) v.push_back("+" + to_string(it.hp) + " HP");
        if (it.mp) v.push_back("+" + to_string(it.mp) + " MP");
    } else if (it.type == BOMB) v.push_back("урон ~" + to_string(it.hp) + ", " + elemStr(it.elem));
    else if (it.type == CURE) v.push_back("снимает яд и горение");
    else if (it.type == WEAPON || it.type == ARMOR || it.type == ACCESSORY) {
        if (it.atk) v.push_back("атк +" + to_string(it.atk));
        if (it.def) v.push_back("защ +" + to_string(it.def));
        if (it.hp) v.push_back("HP +" + to_string(it.hp));
        if (it.mp) v.push_back("MP +" + to_string(it.mp));
        if (it.crit) v.push_back("крит +" + to_string(it.crit) + "%");
        if (it.steal) v.push_back("вампиризм " + to_string(it.steal) + "%");
        if (it.dodge) v.push_back("уклон. +" + to_string(it.dodge) + "%");
        if (it.elem) v.push_back(elemStr(it.elem));
    } else v.push_back("материал");
    string s;
    for (size_t i = 0; i < v.size(); i++) s += (i ? ", " : "") + v[i];
    return s;
}
bool isEquip(IType t) { return t == WEAPON || t == ARMOR || t == ACCESSORY; }
bool isCombatItem(IType t) { return t == HEAL || t == MANA || t == BOMB || t == CURE; }

// ---------- Генерация экипировки со случайными свойствами ----------
map<string, int> GEN_KEYS;
const char* RAR_TAG[] = {"", "редк.", "эпич.", "легенд."};

int addGenItem(const Item& proto) {  // proto — полностью заполненный предмет
    char buf[160];
    snprintf(buf, sizeof buf, "%d|%d|%d|%d|%d|%d|%d|%d|%d|%d", proto.base, proto.rarity, proto.atk, proto.def,
             proto.hp, proto.mp, proto.crit, proto.steal, proto.dodge, proto.elem);
    auto it = GEN_KEYS.find(buf);
    if (it != GEN_KEYS.end()) return it->second;
    if ((int)ITEMS.size() >= STATIC_ITEMS + 3900) return proto.base;
    Item n = proto;
    n.id = (int)ITEMS.size();
    ITEMS.push_back(n);
    GEN_KEYS[buf] = n.id;
    return n.id;
}
Item buildGen(int baseId, int rarity, int atk, int def, int hp, int mp, int crit, int steal, int dodge, int elem, int price) {
    Item n = ITEMS[baseId];
    n.base = baseId; n.rarity = rarity; n.atk = atk; n.def = def; n.hp = hp; n.mp = mp;
    n.crit = crit; n.steal = steal; n.dodge = dodge; n.elem = elem; n.price = price;
    n.name = ITEMS[baseId].name + (elem ? string(" ") + EL_SUFFIX[elem] : string("")) + " [" + RAR_TAG[rarity] + "]";
    n.desc = "Предмет со случайными свойствами.";
    return n;
}
int rollGear(int baseId, int rarity) {
    if (rarity <= 0 || baseId <= 0 || baseId >= STATIC_ITEMS || !isEquip(ITEMS[baseId].type)) return baseId;
    const Item b = ITEMS[baseId];
    double mult = rarity == 1 ? 1.15 : rarity == 2 ? 1.35 : 1.6;
    int atk = (int)(b.atk * mult + 0.5), def = (int)(b.def * mult + 0.5);
    int hp = (int)(b.hp * mult + 0.5), mp = (int)(b.mp * mult + 0.5);
    int crit = 0, steal = 0, dodge = 0, elem = 0;
    vector<int> pool;  // 0 атк, 1 защ, 2 HP, 3 MP, 4 крит, 5 вампиризм, 6 уклонение, 7 стихия
    if (b.type == WEAPON) pool = {0, 4, 5, 7, 3};
    else if (b.type == ARMOR) pool = {1, 2, 6, 3};
    else pool = {2, 3, 4, 6, 0};
    shuffle(pool.begin(), pool.end(), rng);
    int n = min((int)pool.size(), rarity);
    for (int i = 0; i < n; i++) {
        switch (pool[i]) {
            case 0: atk += rnd(2, 3) + rarity; break;
            case 1: def += rnd(1, 2) + rarity; break;
            case 2: hp += rnd(8, 12) * rarity; break;
            case 3: mp += rnd(6, 10) * rarity; break;
            case 4: crit += 2 + rarity * 2; break;
            case 5: steal += 4 + rarity * 3; break;
            case 6: dodge += 2 + rarity * 2; break;
            default: elem = rnd(1, 5); break;
        }
    }
    int price = b.price * (rarity == 1 ? 2 : rarity == 2 ? 4 : 8) + n * 20;
    return addGenItem(buildGen(baseId, rarity, atk, def, hp, mp, crit, steal, dodge, elem, price));
}
int rollRarity(int bonus) {
    int r = rnd(1, 100) + bonus;
    return r >= 99 ? 3 : r >= 92 ? 2 : r >= 70 ? 1 : 0;
}
int randomGear(int lvl, int bonus, int minRarity = 0) {
    vector<int> pool;
    if (lvl < 4) pool = {I_RUSTY, I_STEEL, I_DAGGER, I_STAFF1, I_LEATHER, I_ROBE, I_CHAIN, I_RING_STR, I_AMULET};
    else if (lvl < 9) pool = {I_STEEL, I_AXE, I_STAFF2, I_CHAIN, I_PLATE, I_RING_STR, I_AMULET, I_RING_WIS, I_DAGGER, I_ROBE};
    else pool = {I_AXE, I_MITHRIL, I_STAFF2, I_PLATE, I_RING_STR, I_AMULET, I_RING_WIS, I_CHAIN};
    int base = pickOne(pool);
    return rollGear(base, max(minRarity, rollRarity(bonus)));
}

// ============================================================================
//  ДАННЫЕ: навыки и классы
// ============================================================================
enum SKind { S_DMG, S_HEAL, S_BUFF, S_POISON, S_STUN, S_CRIT, S_GUARD, S_MULTI };
struct Skill {
    string name;
    int cls, lvl, cost, kind;
    double mult;
    bool magic;
    string desc;
    int elem = 0;
};
vector<Skill> SKILLS;

void initSkills() {
    SKILLS = {
        {"Мощный удар", 0, 1, 4, S_DMG, 1.8, false, "Сильный удар (x1.8)"},
        {"Боевой клич", 0, 3, 6, S_BUFF, 0, false, "Атака +50% на 4 хода"},
        {"Оглушающий удар", 0, 5, 8, S_STUN, 1.2, false, "Шанс оглушить врага"},
        {"Вихрь клинков", 0, 8, 14, S_DMG, 2.8, false, "Разрушительный удар (x2.8)"},
        {"Удар титана", 0, 10, 20, S_DMG, 4.2, false, "Сокрушительный удар (x4.2)"},
        {"Огненный шар", 1, 1, 6, S_DMG, 2.0, true, "Огонь x2.0, может поджечь", EL_FIRE},
        {"Исцеление", 1, 2, 8, S_HEAL, 2.5, true, "Лечит по силе интеллекта"},
        {"Ледяная стрела", 1, 4, 9, S_STUN, 1.6, true, "Лёд x1.6, шанс заморозить", EL_ICE},
        {"Метеор", 1, 8, 22, S_DMG, 3.6, true, "Огонь x3.6, может поджечь", EL_FIRE},
        {"Звёздный дождь", 1, 10, 30, S_DMG, 4.8, true, "Молния x4.8", EL_SHOCK},
        {"Удар в спину", 2, 1, 4, S_CRIT, 2.0, false, "Высокий шанс крита"},
        {"Ядовитый клинок", 2, 3, 6, S_POISON, 1.0, false, "Яд: отравляет врага", EL_POISON},
        {"Уклонение", 2, 5, 5, S_GUARD, 0, false, "Уворот от следующей атаки"},
        {"Смертельный танец", 2, 8, 15, S_MULTI, 1.1, false, "Три быстрых удара"},
        {"Тень смерти", 2, 10, 20, S_CRIT, 3.4, false, "Почти всегда критует"},
    };
}

struct ClassInfo {
    string name, desc, passive;
    int hp, mp, str, def, intl, agi;
    int gHp, gMp, gStr, gDef, gInt, gAgi;
};
vector<ClassInfo> CLASSES = {
    {"Воин", "Крепкий боец ближнего боя.", "Стойкость: получаемый урон снижен на 10%.",
     60, 15, 8, 4, 2, 3, 12, 3, 3, 2, 1, 1},
    {"Маг", "Хрупкий, но смертоносный заклинатель.", "Поток маны: мана восстанавливается каждый раунд.",
     40, 50, 3, 2, 10, 4, 7, 9, 1, 1, 3, 1},
    {"Плут", "Ловкий и непредсказуемый.", "Меткий глаз: +10% к криту, контратака после уклонения.",
     48, 25, 6, 3, 4, 8, 9, 5, 2, 1, 1, 3},
};

// ============================================================================
//  ДАННЫЕ: враги, локации, квесты, рецепты, спутники, достижения
// ============================================================================
struct EnemyT {
    string name;
    int hp, atk, def, xp, gold;
    int special;                   // 0 нет, 1 яд, 2 оглушение, 3 тяжёлые удары, 4 регенерация
    vector<pair<int, int>> drops;  // (предмет, шанс %)
    bool boss;
    int ability;                   // 1 — дыхание огня (каждый 3-й ход), 2 — топот (каждый 4-й)
    int weak = 0, resist = 0;      // стихия-слабость (x1.5) и сопротивление (x0.6)
};
vector<EnemyT> ENEMIES;
enum EnemyId { E_RAT, E_WOLF, E_GOBLIN, E_BANDIT, E_SPIDER, E_SKELETON, E_TROLL, E_DARKMAGE,
               E_OGRE, E_GOLEM, E_DRAGON, E_TROLLKING, E_MIMIC };

void initEnemies() {
    ENEMIES = {
        {"Гигантская крыса", 18, 6, 0, 8, 5, 0, {{I_HP_S, 20}, {I_HERB, 20}}, false, 0, 3, 0},
        {"Серый волк", 30, 8, 1, 14, 8, 0, {{I_PELT, 60}, {I_HP_S, 15}}, false, 0, 1, 0},
        {"Гоблин", 35, 10, 2, 18, 14, 0, {{I_RUSTY, 10}, {I_HP_S, 20}, {I_HERB, 20}}, false, 0, 2, 0},
        {"Бандит", 45, 12, 3, 26, 25, 0, {{I_DAGGER, 10}, {I_MP_S, 15}, {I_LEATHER, 10}}, false, 0, 5, 0},
        {"Гигантский паук", 50, 13, 3, 30, 15, 1, {{I_FANG, 70}}, false, 0, 1, 3},
        {"Скелет-воин", 60, 15, 5, 40, 20, 0, {{I_ORE, 40}, {I_HP_L, 10}}, false, 0, 4, 3},
        {"Пещерный тролль", 120, 21, 6, 80, 60, 4, {{I_ORE, 60}, {I_AXE, 8}}, false, 0, 1, 2},
        {"Тёмный маг", 70, 23, 4, 90, 70, 2, {{I_CRYSTAL, 50}, {I_MP_L, 20}, {I_RING_WIS, 5}}, false, 0, 4, 5},
        {"Горный огр", 160, 27, 9, 130, 90, 3, {{I_ORE, 70}, {I_CHAIN, 12}, {I_HP_L, 30}}, false, 0, 5, 3},
        {"Каменный голем", 220, 29, 14, 180, 120, 0, {{I_CRYSTAL, 80}, {I_PLATE, 6}}, false, 0, 2, 3},
        {"ДРАКОН ИГНАТОР", 650, 46, 16, 1000, 1000, 3, {}, true, 1, 2, 1},
        {"Король троллей Грок", 340, 29, 10, 450, 400, 4, {{I_TROLLCROWN, 100}, {I_TROLLCLUB, 35}, {I_ORE, 100}}, true, 2, 1, 3},
        {"Мимик", 90, 19, 8, 60, 150, 0, {{I_CRYSTAL, 60}, {I_HP_L, 40}}, false, 0, 5, 0},
    };
}

struct Loc {
    string name, desc;
    bool town;
    vector<int> nb;
    vector<int> enemies;
    int minLvl;
    vector<pair<int, int>> gather;  // (предмет, вес)
};
vector<Loc> LOCS;
enum LocId { L_VILLAGE, L_FOREST, L_ROAD, L_CAVE, L_RUINS, L_DUNGEON, L_MOUNTAINS, L_LAIR };

void initLocs() {
    LOCS = {
        {"Деревня Рассвет", "Тихая деревня у подножия гор. Здесь есть всё нужное.", true, {1, 2}, {}, 1, {}},
        {"Тёмный лес", "Древние деревья едва пропускают свет.", false, {0, 3, 4},
         {E_RAT, E_WOLF, E_GOBLIN, E_SPIDER}, 1, {{I_HERB, 60}, {I_MOONFLOWER, 20}}},
        {"Разбойничий тракт", "Дорога, на которой пропадают торговцы.", false, {0, 4},
         {E_GOBLIN, E_BANDIT, E_WOLF}, 2, {{I_HERB, 50}, {I_ORE, 20}}},
        {"Паучья пещера", "Стены затянуты липкой паутиной.", false, {1, 5},
         {E_SPIDER, E_SKELETON, E_RAT}, 3, {{I_ORE, 40}, {I_FANG, 30}, {I_CRYSTAL, 8}}},
        {"Заброшенные руины", "Здесь жил знатный род. Теперь тут бродит нежить.", false, {1, 2, 6},
         {E_SKELETON, E_DARKMAGE, E_BANDIT}, 5, {{I_MOONFLOWER, 40}, {I_CRYSTAL, 25}, {I_HERB, 20}}},
        {"Подземелье троллей", "Сырые тоннели, пахнет сырым мясом.", false, {3, 6},
         {E_TROLL, E_SKELETON, E_SPIDER}, 6, {{I_ORE, 60}, {I_CRYSTAL, 20}}},
        {"Серые горы", "Холодный ветер и острые скалы.", false, {4, 5, 7},
         {E_OGRE, E_TROLL, E_DARKMAGE}, 8, {{I_ORE, 50}, {I_CRYSTAL, 25}, {I_HERB, 15}}},
        {"Логово дракона", "Воздух дрожит от жара. Кругом кости и золото.", false, {6},
         {E_GOLEM, E_OGRE}, 10, {{I_ORE, 40}, {I_CRYSTAL, 40}, {I_SCALE, 5}}},
    };
}
int enemyTier(int ei) {
    int t = 99;
    for (auto& L : LOCS)
        for (int e : L.enemies) if (e == ei) t = min(t, L.minLvl);
    return t == 99 ? -1 : t;
}

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
        {"Тёмные искусства", "Тёмные маги осквернили руины. Убейте 3.", Q_KILL, E_DARKMAGE, 3, 300, 500, I_RING_WIS, 6},
        {"Убить дракона", "Игнатор терроризирует королевство!", Q_KILL, E_DRAGON, 1, 2000, 3000, I_ELIXIR, 8},
        {"Лунный цветок", "Алхимику нужны 3 лунных цветка.", Q_COLLECT, I_MOONFLOWER, 3, 90, 120, I_ANTIDOTE, 2},
        {"Король троллей", "Грок правит подземельем. Свергните его!", Q_KILL, E_TROLLKING, 1, 600, 900, I_ELIXIR, 6},
        {"Паучья зачистка", "Очистите пещеру: убейте 5 пауков.", Q_KILL, E_SPIDER, 5, 130, 160, I_BOMB, 3},
    };
}

struct Recipe {
    int result;
    vector<pair<int, int>> mats;
    int gold;
};
vector<Recipe> SMITH = {
    {I_WOLFARMOR, {{I_PELT, 3}}, 30},
    {I_SPIDERDAGGER, {{I_FANG, 4}, {I_DAGGER, 1}}, 40},
    {I_MITHRIL, {{I_ORE, 6}, {I_CRYSTAL, 2}}, 120},
    {I_DRAGONARMOR, {{I_SCALE, 3}, {I_ORE, 4}}, 200},
    {I_DRAGONSWORD, {{I_SCALE, 3}, {I_CRYSTAL, 3}}, 300},
};
vector<Recipe> ALCH = {
    {I_HP_S, {{I_HERB, 2}}, 0},
    {I_HP_L, {{I_HERB, 4}, {I_MOONFLOWER, 1}}, 5},
    {I_MP_S, {{I_MOONFLOWER, 1}, {I_HERB, 1}}, 0},
    {I_MP_L, {{I_MOONFLOWER, 3}, {I_CRYSTAL, 1}}, 10},
    {I_ANTIDOTE, {{I_HERB, 2}, {I_FANG, 1}}, 0},
    {I_BOMB, {{I_ORE, 1}, {I_CRYSTAL, 1}}, 10},
    {I_ELIXIR, {{I_HERB, 5}, {I_MOONFLOWER, 3}, {I_CRYSTAL, 1}}, 40},
    {I_ICEBOMB, {{I_CRYSTAL, 1}, {I_MOONFLOWER, 1}}, 10},
    {I_ACID, {{I_FANG, 2}, {I_HERB, 1}}, 5},
    {I_HOLYWATER, {{I_MOONFLOWER, 2}, {I_CRYSTAL, 1}}, 15},
};

struct Comp {
    string name, desc;
    int cost, kind;  // 0 лучница, 1 жрец, 2 рыцарь
};
const int UPKEEP = 8;
vector<Comp> COMPS = {
    {"Лучница Ира", "Стреляет во врага каждый ход.", 150, 0},
    {"Жрец Томас", "Лечит вас на 7% здоровья каждый ход.", 200, 1},
    {"Рыцарь Бранн", "Принимает 30% ударов на свой щит.", 250, 2},
};

enum { A_FIRST, A_HUNTER, A_VET, A_MASTER, A_DRAGON, A_KING, A_RICH, A_ELITE, A_ARENA,
       A_SMITH, A_ALCH, A_FRIEND, A_GAMBLER, A_COLLECTOR, A_LEGEND, A_DEPTH, A_DEEP, A_COUNT };
struct Ach { string name, desc; };
vector<Ach> ACHS = {
    {"Первая кровь", "Победите первого врага"},
    {"Охотник", "Убейте 50 врагов"},
    {"Ветеран", "Достигните 5 уровня"},
    {"Мастер", "Достигните 10 уровня"},
    {"Драконоборец", "Победите дракона Игнатора"},
    {"Цареубийца", "Победите короля троллей"},
    {"Богач", "Накопите 1000 золота"},
    {"Охотник на элиту", "Убейте 5 элитных врагов"},
    {"Чемпион арены", "Пройдите арену до конца"},
    {"Кузнец", "Выкуйте предмет"},
    {"Алхимик", "Сварите зелье"},
    {"Друг деревни", "Репутация 50"},
    {"Азартный игрок", "Выиграйте в кости 5 раз"},
    {"Коллекционер", "Убейте врагов всех видов (бестиарий)"},
    {"Легенда", "Найдите легендарный предмет"},
    {"Исследователь Бездны", "Пройдите 5 этажей Бездны"},
    {"Покоритель Бездны", "Пройдите 10 этажей Бездны"},
};

// ============================================================================
//  ИГРОК
// ============================================================================
enum { WX_CLEAR, WX_RAIN, WX_FOG, WX_STORM };
const char* WX_NAMES[] = {"Ясно", "Дождь", "Туман", "Гроза"};
const char* WX_HINTS[] = {"Погода не влияет на бой.",
                          "Дождь: легче убежать (+15%).",
                          "Туман: проще уклоняться (+10%), но враги встречаются чаще.",
                          "Гроза: магия сильнее (+15%), но возможны удары молний."};
string weatherStr(int w) {
    const char* cols[] = {Col::YELLOW, Col::BLUE, Col::GRAY, Col::MAGENTA};
    return paint(WX_NAMES[w], cols[w]);
}
int rollWeather() {
    int r = rnd(1, 100);
    return r <= 50 ? WX_CLEAR : r <= 70 ? WX_RAIN : r <= 88 ? WX_FOG : WX_STORM;
}

struct Player {
    string name = "Герой";
    int cls = 0, level = 1, xp = 0, gold = 60;
    int hp = 1, mp = 1, baseHp = 1, baseMp = 1, str = 1, def = 1, intl = 1, agi = 1;
    int eq[3] = {0, 0, 0};  // оружие, броня, аксессуар
    int sharp = 0;
    map<int, int> inv, quest, qprog;  // квест: 0 нет, 1 активен, 2 сдан
    int loc = 0, tick = 8, kills = 0;
    bool dragonSlain = false, trollKingSlain = false;
    int points = 0, rep = 0, weather = 0, wperiod = 1, payDay = 1, comp = -1;
    int dealDay = 0, dealMask = 0;
    int bntDay = 0, bntEnemy = 0, bntNeed = 0, bntProg = 0, bntState = 0, bntGold = 0, bntXp = 0;
    int blessFights = 0, nextBuff = 0;
    unsigned ach = 0;
    int eliteKills = 0, diceWins = 0, arenaWins = 0;
    int maxHit = 0, bestFloor = 0;
    vector<int> beast;
    // временные боевые статусы
    int poison = 0, buff = 0, stun = 0, burn = 0;
    bool guard = false;

    int maxHp() const { int v = baseHp; for (int i = 0; i < 3; i++) v += ITEMS[eq[i]].hp; return v; }
    int maxMp() const { int v = baseMp; for (int i = 0; i < 3; i++) v += ITEMS[eq[i]].mp; return v; }
    int equipAtk() const { int v = sharp; for (int i = 0; i < 3; i++) v += ITEMS[eq[i]].atk; return v; }
    int attack() const { return str + equipAtk() + (buff > 0 ? str / 2 + 2 : 0); }
    int defense() const { int v = def; for (int i = 0; i < 3; i++) v += ITEMS[eq[i]].def; return v; }
    int magic() const { return intl + equipAtk() / 2; }
    int eqSum(int Item::*f) const { int v = 0; for (int i = 0; i < 3; i++) v += ITEMS[eq[i]].*f; return v; }
    int steal() const { return eqSum(&Item::steal); }
    int weaponElem() const { return ITEMS[eq[0]].elem; }
    int critChance() const { return min(60, 5 + agi / 2 + (cls == 2 ? 10 : 0) + eqSum(&Item::crit)); }
    int dodge() const { return min(40, agi + eqSum(&Item::dodge) + (weather == WX_FOG ? 10 : 0)); }
    int hour() const { return tick % 24; }
    int day() const { return tick / 24 + 1; }
    bool night() const { return hour() >= 20 || hour() < 6; }
    int xpNeed() const { return 40 * level + 10 * level * level; }
    int discount() const { return rep >= 100 ? 15 : rep >= 50 ? 10 : rep >= 20 ? 5 : 0; }

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
    void ensureBeast() { if (beast.size() < ENEMIES.size()) beast.resize(ENEMIES.size(), 0); }
};

string repTitle(int r) {
    return r >= 100 ? "Герой деревни" : r >= 50 ? "Уважаемый гость" : r >= 20 ? "Знакомый" : "Незнакомец";
}
string timeStr(const Player& p) {
    string t = "День " + to_string(p.day()) + ", " + (p.hour() < 10 ? "0" : "") + to_string(p.hour()) + ":00";
    return t + (p.night() ? blue("  ночь") : yellow("  день"));
}

// Ход времени: смена погоды, жалованье спутнику.
void passTime(Player& p, int hours) {
    p.tick += hours;
    int per = p.tick / 8;
    if (per != p.wperiod) {
        p.wperiod = per;
        int nw = rollWeather();
        if (nw != p.weather) {
            p.weather = nw;
            note("Погода изменилась: " + weatherStr(nw) + gray(" — " + string(WX_HINTS[nw])));
        }
    }
    while (p.payDay < p.day()) {
        p.payDay++;
        if (p.comp >= 0) {
            if (p.gold >= UPKEEP) { p.gold -= UPKEEP; note(COMPS[p.comp].name + " получает жалованье: " + to_string(UPKEEP) + " зол."); }
            else { bad(COMPS[p.comp].name + " ушёл(ла): вы не смогли заплатить."); p.comp = -1; }
        }
    }
}

void unlockAch(Player& p, int id) {
    if (p.ach & (1u << id)) return;
    p.ach |= (1u << id);
    cout << "\n";
    panel("Достижение получено!", {bold(ACHS[id].name) + gray(" — " + ACHS[id].desc)}, Col::YELLOW);
}
void checkAch(Player& p) {
    p.ensureBeast();
    if (p.kills >= 1) unlockAch(p, A_FIRST);
    if (p.kills >= 50) unlockAch(p, A_HUNTER);
    if (p.level >= 5) unlockAch(p, A_VET);
    if (p.level >= 10) unlockAch(p, A_MASTER);
    if (p.dragonSlain) unlockAch(p, A_DRAGON);
    if (p.trollKingSlain) unlockAch(p, A_KING);
    if (p.gold >= 1000) unlockAch(p, A_RICH);
    if (p.eliteKills >= 5) unlockAch(p, A_ELITE);
    if (p.arenaWins >= 1) unlockAch(p, A_ARENA);
    if (p.rep >= 50) unlockAch(p, A_FRIEND);
    if (p.diceWins >= 5) unlockAch(p, A_GAMBLER);
    bool all = true;
    for (int i = 0; i <= E_GOLEM; i++) if (p.beast[i] == 0) all = false;
    if (all) unlockAch(p, A_COLLECTOR);
    if (p.bestFloor >= 5) unlockAch(p, A_DEPTH);
    if (p.bestFloor >= 10) unlockAch(p, A_DEEP);
}

// ============================================================================
//  Экран игрока: HUD, лист персонажа
// ============================================================================
void drawHud(const Player& p) {
    const Loc& L = LOCS[p.loc];
    vector<string> v;
    v.push_back(lr(bold(p.name) + gray("  " + CLASSES[p.cls].name + ", ур. " + to_string(p.level)),
                   yellow("● " + to_string(p.gold) + " зол.")));
    v.push_back(lr(timeStr(p), weatherStr(p.weather)));
    v.push_back("HP " + bar(p.hp, p.maxHp(), 30, hpColor(p.hp, p.maxHp())) + " " + to_string(p.hp) + "/" + to_string(p.maxHp()));
    v.push_back("MP " + bar(p.mp, p.maxMp(), 30, Col::BLUE) + " " + to_string(p.mp) + "/" + to_string(p.maxMp()));
    v.push_back("XP " + bar(p.xp, p.xpNeed(), 30, Col::YELLOW) + " " + to_string(p.xp) + "/" + to_string(p.xpNeed()) +
                (p.points > 0 ? yellow("  +" + to_string(p.points) + " очк.") : ""));
    v.push_back(gray(L.desc));
    cout << "\n";
    panel(L.name, v, L.town ? Col::GREEN : Col::CYAN);
}

void drawEquipment(const Player& p) {
    const char* slots[3] = {"Оружие   ", "Броня    ", "Аксессуар"};
    vector<string> v;
    for (int i = 0; i < 3; i++) {
        string s = gray(slots[i]) + "  " + (p.eq[i] ? itemName(p.eq[i]) : gray("—"));
        if (i == 0 && p.sharp) s += yellow(" +" + to_string(p.sharp));
        string st = p.eq[i] ? gray("(" + statsStr(ITEMS[p.eq[i]]) + ")") : "";
        if (!st.empty() && vlen(s) + 2 + vlen(st) <= W - 2) v.push_back(s + "  " + st);
        else { v.push_back(s); if (!st.empty()) v.push_back("            " + st); }
    }
    panel("Снаряжение", v, Col::YELLOW);
}

void characterSheet(const Player& p) {
    vector<string> v;
    v.push_back(lr(bold(p.name), gray(CLASSES[p.cls].name + " • ур. " + to_string(p.level))));
    v.push_back("HP " + bar(p.hp, p.maxHp(), 30, hpColor(p.hp, p.maxHp())) + " " + to_string(p.hp) + "/" + to_string(p.maxHp()));
    v.push_back("MP " + bar(p.mp, p.maxMp(), 30, Col::BLUE) + " " + to_string(p.mp) + "/" + to_string(p.maxMp()));
    v.push_back("XP " + bar(p.xp, p.xpNeed(), 30, Col::YELLOW) + " " + to_string(p.xp) + "/" + to_string(p.xpNeed()));
    v.push_back("");
    v.push_back(padR("Сила: " + bold(to_string(p.str)), 20) + padR("Защита: " + bold(to_string(p.defense())), 20) +
                "Магия: " + bold(to_string(p.magic())));
    v.push_back(padR("Атака: " + bold(to_string(p.attack())), 20) + padR("Ловкость: " + bold(to_string(p.agi)), 20) +
                "Интеллект: " + bold(to_string(p.intl)));
    v.push_back(padR("Крит: " + bold(to_string(p.critChance()) + "%"), 20) + "Уклонение: " + bold(to_string(p.dodge()) + "%"));
    v.push_back("");
    v.push_back(green("Пассивка: ") + CLASSES[p.cls].passive.substr(0, CLASSES[p.cls].passive.find(':')));
    v.push_back(gray(CLASSES[p.cls].passive.substr(CLASSES[p.cls].passive.find(':') + 2)));
    v.push_back("Репутация: " + bold(to_string(p.rep)) + gray(" (" + repTitle(p.rep) + ", скидка " + to_string(p.discount()) + "%)"));
    v.push_back("Убито врагов: " + bold(to_string(p.kills)) + gray("   Рекорд удара: ") + bold(to_string(p.maxHit)) +
                gray("   Бездна: ") + bold(p.bestFloor ? to_string(p.bestFloor) + " эт." : string("—")));
    if (p.steal() > 0) v.push_back("Вампиризм: " + green(to_string(p.steal()) + "%") + gray(" от нанесённого урона"));
    if (p.weaponElem()) v.push_back("Стихия оружия: " + elemStr(p.weaponElem()));
    if (p.comp >= 0) v.push_back("Спутник: " + cyan(COMPS[p.comp].name));
    panel("Персонаж", v, Col::GREEN);
    drawEquipment(p);
    vector<string> sk;
    for (auto& s : SKILLS)
        if (s.cls == p.cls) {
            bool open = s.lvl <= p.level;
            string line = (open ? green("✓ ") : gray("✗ ")) + (open ? bold(s.name) : gray(s.name)) +
                   gray("  " + to_string(s.cost) + " MP, ур." + to_string(s.lvl) + " — " + s.desc);
            sk.push_back(line);
        }
    panel("Навыки", sk, Col::MAGENTA);
}

void allocPoints(Player& p) {
    while (p.points > 0) {
        panel("Распределение очков", {"Свободных очков: " + yellow(to_string(p.points))}, Col::YELLOW);
        grid({key(1, "Сила +1"), key(2, "Защита +1"), key(3, "Интеллект +1"), key(4, "Ловкость +1"),
              key(5, "Здоровье +6"), key(6, "Мана +5"), key(0, "Позже")}, 2);
        int c = askInt(0, 6);
        if (c == 0) return;
        switch (c) {
            case 1: p.str++; break;
            case 2: p.def++; break;
            case 3: p.intl++; break;
            case 4: p.agi++; break;
            case 5: p.baseHp += 6; p.hp += 6; break;
            default: p.baseMp += 5; p.mp += 5; break;
        }
        p.points--;
        good("Очко потрачено.");
    }
}
void characterMenu(Player& p) {
    characterSheet(p);
    if (p.points > 0) {
        if (askYesNo("У вас есть свободные очки (" + to_string(p.points) + "). Распределить?")) allocPoints(p);
    }
}

// ============================================================================
//  Предметы: использование, экипировка, инвентарь
// ============================================================================
bool useConsumable(Player& p, int id) {
    const Item& it = ITEMS[id];
    if (it.type != HEAL && it.type != MANA) return false;
    if (!p.take(id)) return false;
    int h = min(it.hp, p.maxHp() - p.hp), m = min(it.mp, p.maxMp() - p.mp);
    p.hp += h;
    p.mp += m;
    string s = "Вы используете " + itemName(id);
    if (h > 0) s += green(" (+" + to_string(h) + " HP)");
    if (m > 0) s += blue(" (+" + to_string(m) + " MP)");
    good(s);
    return true;
}
void equipItem(Player& p, int id) {
    const Item& it = ITEMS[id];
    int slot = it.type == WEAPON ? 0 : it.type == ARMOR ? 1 : 2;
    if (!p.take(id)) return;
    int a0 = p.attack(), d0 = p.defense(), h0 = p.maxHp(), c0 = p.critChance();
    if (p.eq[slot]) p.add(p.eq[slot]);
    p.eq[slot] = id;
    p.clampVitals();
    good("Экипировано: " + itemName(id) + gray(" (" + statsStr(it) + ")"));
    auto diff = [](const string& n, int a, int b) {
        if (a == b) return string("");
        return n + " " + to_string(a) + " → " + (b > a ? green(to_string(b)) : red(to_string(b))) + "  ";
    };
    string cmp = diff("Атака", a0, p.attack()) + diff("Защита", d0, p.defense()) + diff("HP", h0, p.maxHp()) +
                 diff("Крит%", c0, p.critChance());
    if (!cmp.empty()) note(cmp);
}
void unequipMenu(Player& p) {
    const char* slots[3] = {"Оружие", "Броня", "Аксессуар"};
    vector<string> c;
    for (int i = 0; i < 3; i++) c.push_back(key(i + 1, string(slots[i]) + ": " + (p.eq[i] ? itemName(p.eq[i]) : gray("—"))));
    c.push_back(key(0, "Назад"));
    grid(c, 1);
    int k = askInt(0, 3);
    if (k == 0 || !p.eq[k - 1]) return;
    p.add(p.eq[k - 1]);
    good("Снято: " + itemName(p.eq[k - 1]));
    p.eq[k - 1] = 0;
    p.clampVitals();
}

void inventoryMenu(Player& p) {
    while (true) {
        vector<int> ids;
        for (auto& kv : p.inv) ids.push_back(kv.first);
        sort(ids.begin(), ids.end(), [](int a, int b) {
            if (ITEMS[a].type != ITEMS[b].type) return ITEMS[a].type < ITEMS[b].type;
            return a < b;
        });
        vector<string> v;
        for (size_t i = 0; i < ids.size(); i++) {
            const Item& it = ITEMS[ids[i]];
            string l = key((int)i + 1, itemName(ids[i]) + gray(" x" + to_string(p.inv[ids[i]])));
            string st = gray(statsStr(it));
            if (vlen(l) < 34 && 34 + vlen(st) <= W - 2) v.push_back(padR(l, 34) + st);
            else { v.push_back(l); v.push_back("      " + st); }
        }
        if (ids.empty()) v.push_back(gray("(пусто)"));
        v.push_back("");
        v.push_back(lr(key((int)ids.size() + 1, "Снять экипировку"), key(0, "Назад")));
        panel("Инвентарь  ●" + to_string(p.gold), v, Col::YELLOW);
        int c = askInt(0, (int)ids.size() + 1);
        if (c == 0) return;
        if (c == (int)ids.size() + 1) { unequipMenu(p); continue; }
        int id = ids[c - 1];
        const Item& it = ITEMS[id];
        if (it.type == HEAL || it.type == MANA) {
            if (p.hp == p.maxHp() && it.hp > 0 && it.mp == 0) note("Вы и так здоровы.");
            else if (p.mp == p.maxMp() && it.mp > 0 && it.hp == 0) note("Мана и так полная.");
            else useConsumable(p, id);
        } else if (it.type == BOMB || it.type == CURE) note("Этот предмет используется только в бою.");
        else if (it.type == MATERIAL) note("Материал: пригодится у кузнеца или алхимика.");
        else equipItem(p, id);
    }
}

// ============================================================================
//  Враги и бой
// ============================================================================
struct Foe {
    string name, affix;
    int tmpl, hp, maxhp, atk, def, xp, gold, special, ability, weak, resist;
    bool boss, elite;
    vector<pair<int, int>> drops;
    int lootBonus = 0;            // сдвиг шанса редкой экипировки
    bool guaranteedGear = false;  // гарантированная экипировка
};
void applyElite(Foe& f) {
    f.elite = true;
    switch (rnd(0, 3)) {
        case 0: f.atk = f.atk * 125 / 100; f.affix = "Свирепость: атака +25%"; break;
        case 1: f.maxhp = f.hp = f.maxhp * 150 / 100; f.affix = "Мощь: здоровье +50%"; break;
        case 2: f.def += 3 + f.def / 4; f.affix = "Закалка: броня выше"; break;
        default: f.gold *= 3; f.affix = "Алчность: много золота"; break;
    }
}
Foe makeFoe(int ei) {
    const EnemyT& t = ENEMIES[ei];
    Foe f;
    f.name = t.name; f.affix = ""; f.tmpl = ei;
    f.maxhp = f.hp = t.hp; f.atk = t.atk; f.def = t.def; f.xp = t.xp; f.gold = t.gold;
    f.special = t.special; f.ability = t.ability; f.boss = t.boss; f.elite = false; f.drops = t.drops;
    f.weak = t.weak; f.resist = t.resist;
    if (!t.boss) {
        f.maxhp = f.hp = max(1, t.hp * rnd(90, 110) / 100);
        f.atk = max(1, t.atk * rnd(95, 105) / 100);
        if (chance(15)) applyElite(f);
    }
    return f;
}

int calcDmg(double raw, int edef) {
    int d = (int)(raw * (0.85 + rnd(0, 30) / 100.0)) - edef / 2;
    return max(1, d);
}

void levelUp(Player& p) {
    while (p.xp >= p.xpNeed()) {
        p.xp -= p.xpNeed();
        p.level++;
        auto& c = CLASSES[p.cls];
        p.baseHp += c.gHp; p.baseMp += c.gMp; p.str += c.gStr;
        p.def += c.gDef; p.intl += c.gInt; p.agi += c.gAgi;
        p.points += 3;
        p.hp = p.maxHp(); p.mp = p.maxMp();
        cout << "\n";
        banner("УРОВЕНЬ " + to_string(p.level) + "!", Col::YELLOW);
        good("Здоровье и мана полностью восстановлены. " + yellow("+3 свободных очка"));
        for (auto& s : SKILLS)
            if (s.cls == p.cls && s.lvl == p.level) gain("Новый навык: " + bold(s.name) + gray(" — " + s.desc));
    }
}

void receiveGear(Player& p, int id, vector<string>* log) {
    p.add(id);
    const Item& it = ITEMS[id];
    if (log) {
        log->push_back("Добыча: " + itemName(id));
        if (it.rarity > 0) log->push_back("   " + gray(statsStr(it)));
    } else {
        gain("Найдено: " + itemName(id) + (it.rarity > 0 ? gray(" (" + statsStr(it) + ")") : string("")));
    }
    if (it.rarity >= 3) unlockAch(p, A_LEGEND);
}

void giveRewards(Player& p, const Foe& f) {
    p.ensureBeast();
    int xp = f.xp, gold = f.gold + rnd(0, f.gold / 2);
    int tier = f.boss ? -1 : enemyTier(f.tmpl);
    if (tier > 0) {
        int diff = p.level - tier;
        if (diff >= 5) { xp = xp * 40 / 100; note("Враг слишком слаб: опыт сильно снижен."); }
        else if (diff >= 3) { xp = xp * 70 / 100; note("Враг слабоват: опыт снижен."); }
    }
    if (p.night()) xp = xp * 12 / 10;
    if (f.elite) { xp = xp * 3 / 2; gold = gold * 3 / 2; p.eliteKills++; }
    if (p.blessFights > 0) {
        xp = xp * 12 / 10; gold = gold * 11 / 10; p.blessFights--;
        note("Благословение храма усиливает награду (осталось боёв: " + to_string(p.blessFights) + ").");
    }
    cout << "\n";
    vector<string> v;
    v.push_back(green(bold("ПОБЕДА!")) + "  " + gray(f.name));
    v.push_back("Опыт: " + yellow("+" + to_string(xp)) + "    Золото: " + yellow("+" + to_string(gold)));
    p.xp += xp; p.gold += gold; p.kills++;
    if (f.tmpl >= 0 && f.tmpl < (int)p.beast.size()) p.beast[f.tmpl]++;
    int lootBonus = (f.elite ? 6 : 0) + f.lootBonus;
    for (auto& d : f.drops) {
        int ch = f.elite ? min(100, d.second * 3 / 2) : d.second;
        if (!chance(ch)) continue;
        int id = d.first;
        if (isEquip(ITEMS[id].type) && id != I_TROLLCROWN && id != I_TROLLCLUB) id = rollGear(id, rollRarity(lootBonus));
        if (isEquip(ITEMS[id].type)) receiveGear(p, id, &v);
        else { p.add(id); v.push_back("Добыча: " + itemName(id)); }
    }
    if ((f.elite && chance(35)) || f.guaranteedGear) receiveGear(p, randomGear(p.level, lootBonus, f.guaranteedGear ? 1 : 0), &v);
    if (f.tmpl == E_DRAGON || f.tmpl == E_TROLLKING) receiveGear(p, randomGear(p.level, 18, 2), &v);
    if (f.elite && chance(50)) {
        int extra = pickOne(vector<int>{I_HP_S, I_MP_S, I_HP_L, I_BOMB, I_CRYSTAL});
        p.add(extra);
        v.push_back("Бонус за элиту: " + itemName(extra));
    }
    if (f.tmpl == E_DRAGON) { p.add(I_SCALE, 3); v.push_back("Добыча: " + itemName(I_SCALE) + " x3"); }
    panel("Итоги боя", v, Col::GREEN);

    for (auto& kv : p.quest)
        if (kv.second == 1 && QUESTS[kv.first].type == Q_KILL && QUESTS[kv.first].target == f.tmpl) p.qprog[kv.first]++;
    if (p.bntState == 1 && p.bntEnemy == f.tmpl) p.bntProg++;

    if (f.tmpl == E_DRAGON) {
        p.dragonSlain = true; p.rep += 50;
        cout << "\n";
        banner("ИГНАТОР ПОВЕРЖЕН!", Col::MAGENTA);
        panelText("Эпилог", "Огненный дракон пал. Королевство спасено, а ваше имя навсегда войдёт в легенды. "
                            "Поздравляем с победой! Можно играть дальше: мир по-прежнему полон тайн.", Col::MAGENTA);
    }
    if (f.tmpl == E_TROLLKING) {
        p.trollKingSlain = true; p.rep += 20;
        banner("КОРОЛЬ ТРОЛЛЕЙ СВЕРГНУТ!", Col::MAGENTA);
    }
    levelUp(p);
    checkAch(p);
}

double elemMult(const Foe& f, int el) {
    if (el == EL_PHYS) return 1.0;
    if (f.weak == el) return 1.5;
    if (f.resist == el) return 0.6;
    return 1.0;
}

void drawCombat(const Player& p, const Foe& f, int ePoison, int eStun, int eBurn, int charge, bool known) {
    vector<string> v;
    string nm = f.boss ? magenta(bold(f.name)) : red(bold(string(f.elite ? "★ " : "") + f.name));
    string et;
    if (ePoison) et += green("[яд] ");
    if (eBurn) et += orange("[горит] ");
    if (eStun) et += yellow("[оглушён]");
    v.push_back(lr(nm, et));
    v.push_back(bar(f.hp, f.maxhp, 36, hpColor(f.hp, f.maxhp)) + " " + to_string(max(0, f.hp)) + "/" + to_string(f.maxhp));
    if (known) {
        string wk = "Слабость: " + (f.weak ? elemStr(f.weak) : gray("нет"));
        if (f.resist) wk += "   Сопротивление: " + elemStr(f.resist);
        v.push_back(wk);
    } else v.push_back(gray("Слабость: ???  (изучается: 3 победы или удачный удар)"));
    v.push_back(gray(rep("┄", W - 2)));
    string pt;
    if (p.poison) pt += green("[яд] ");
    if (p.burn) pt += orange("[горю] ");
    if (p.buff) pt += yellow("[сила] ");
    if (p.guard) pt += cyan("[уклонение]");
    v.push_back(lr(bold(p.name) + gray(" " + CLASSES[p.cls].name), pt));
    v.push_back("HP " + bar(p.hp, p.maxHp(), 28, hpColor(p.hp, p.maxHp())) + " " + to_string(max(0, p.hp)) + "/" + to_string(p.maxHp()));
    v.push_back("MP " + bar(p.mp, p.maxMp(), 28, Col::BLUE) + " " + to_string(p.mp) + "/" + to_string(p.maxMp()));
    v.push_back("Пр " + bar(charge, 100, 28, Col::ORANGE) + " " + (charge >= 100 ? yellow("ГОТОВ!") : to_string(charge) + "%"));
    if (p.comp >= 0) v.push_back(gray("Спутник: " + COMPS[p.comp].name));
    cout << "\n";
    panel("Бой", v, f.boss ? Col::MAGENTA : Col::RED);
}

// Возвращает: 1 — победа, 0 — смерть, 2 — бегство
int combat(Player& p, Foe f, bool arena = false) {
    int ePoison = 0, eStun = 0, eBurn = 0, eTurn = 0, charge = 20;
    bool enraged = false;
    p.ensureBeast();
    bool known = f.tmpl >= 0 && f.tmpl < (int)p.beast.size() && p.beast[f.tmpl] >= 3;
    int eatk = f.atk * (p.night() ? 115 : 100) / 100;
    int period = f.ability == 1 ? 3 : f.ability == 2 ? 4 : 0;
    p.poison = 0; p.stun = 0; p.burn = 0; p.guard = false;
    p.buff = p.nextBuff; p.nextBuff = 0;

    cout << "\n";
    if (f.boss) banner(f.name, Col::MAGENTA);
    else say(red("» БОЙ! ") + bold(string(f.elite ? "★ " : "") + f.name));
    if (f.elite) warnMsg("Элитный враг! " + f.affix);
    if (p.night()) note("Ночью враги яростнее (+15% урона).");
    if (p.buff) good("Боевой настрой даёт вам бонус к силе.");
    if (period) warnMsg("Босс готовит особые атаки — следите за предупреждениями!");

    int result = -1;
    auto addCharge = [&](int n) { charge = min(100, charge + n); };
    auto hurt = [&](int d, const string& src) {
        if (p.cls == 0) d = max(1, d * 90 / 100);
        p.hp -= d;
        addCharge(12);
        say(red(src) + " наносит вам " + red(bold(to_string(d))) + " урона.");
    };
    // удар игрока по врагу: учитывает стихии, вампиризм и поджог
    auto hit = [&](double raw, int el, bool magic, bool canSteal) {
        double m = elemMult(f, el);
        int d = calcDmg(raw * m, magic ? f.def / 2 : f.def);
        f.hp -= d;
        p.maxHit = max(p.maxHit, d);
        if (m > 1.0) { known = true; say(orange("Слабость! ") + elemStr(el) + gray(" x1.5")); }
        else if (m < 1.0) { known = true; say(gray("Сопротивление: ") + elemStr(el) + gray(" x0.6")); }
        if (canSteal && p.steal() > 0) {
            int h = min(p.maxHp() - p.hp, max(1, d * p.steal() / 100));
            if (h > 0) { p.hp += h; say(green("Вампиризм: +" + to_string(h) + " HP")); }
        }
        if (el == EL_FIRE && eBurn == 0 && f.resist != EL_FIRE && f.hp > 0 && chance(35)) {
            eBurn = 3;
            say(orange("Враг загорается!"));
        }
        return d;
    };

    while (result < 0) {
        drawCombat(p, f, ePoison, eStun, eBurn, charge, known);
        bool acted = false;
        if (p.stun > 0) {
            bad("Вы оглушены и пропускаете ход!");
            p.stun--;
            acted = true;
        }
        while (!acted) {
            string ultLbl = charge >= 100 ? yellow("Приём ★ГОТОВ") : gray("Приём " + to_string(charge) + "%");
            grid({key(1, "Атака"), key(2, "Навык"), key(3, "Предмет"), key(4, "Бегство"), key(5, ultLbl)}, 3);
            int c = askInt(1, 5);
            if (c == 1) {
                bool crit = chance(p.critChance());
                int d = hit(p.attack() * (crit ? 1.7 : 1.0), p.weaponElem(), false, true);
                if (crit) say(yellow("КРИТИЧЕСКИЙ УДАР! ") + "Вы наносите " + red(bold(to_string(d))));
                else say("Вы бьёте врага: " + red(to_string(d)));
                addCharge(10);
                acted = true;
            } else if (c == 2) {
                vector<const Skill*> av;
                for (auto& s : SKILLS) if (s.cls == p.cls && s.lvl <= p.level) av.push_back(&s);
                for (size_t i = 0; i < av.size(); i++) {
                    string cs = to_string(av[i]->cost) + " MP";
                    say(key((int)i + 1, bold(av[i]->name)) + " " + (p.mp >= av[i]->cost ? blue(cs) : gray(cs)) + gray(" — " + av[i]->desc));
                }
                say(key(0, "Назад"));
                int k = askInt(0, (int)av.size());
                if (k == 0) continue;
                const Skill& s = *av[k - 1];
                if (p.mp < s.cost) { bad("Не хватает маны!"); continue; }
                p.mp -= s.cost;
                double base = s.magic ? p.magic() * (p.weather == WX_STORM ? 1.15 : 1.0) : p.attack();
                int el = s.elem ? s.elem : (s.magic ? (int)EL_PHYS : p.weaponElem());
                say(cyan("» " + s.name) + (s.elem ? " " + elemStr(s.elem) : string("")));
                switch (s.kind) {
                    case S_DMG: {
                        int d = hit(base * s.mult, el, s.magic, true);
                        say("Урон: " + red(bold(to_string(d))));
                        break;
                    }
                    case S_STUN: {
                        int d = hit(base * s.mult, el, s.magic, true);
                        say("Урон: " + red(bold(to_string(d))));
                        if (f.hp > 0) {
                            if (chance(f.boss ? 25 : (el == EL_ICE ? 70 : 65))) { eStun = 1; good(el == EL_ICE ? "Враг заморожен!" : "Враг оглушён!"); }
                            else if (f.boss) note("Босс сопротивляется оглушению.");
                        }
                        break;
                    }
                    case S_CRIT: {
                        bool crit = chance(min(95, 50 + p.agi / 2 + (s.mult > 3 ? 25 : 0)));
                        int d = hit(base * s.mult * (crit ? 1.8 : 1.0), el, false, true);
                        say((crit ? yellow("КРИТ! ") : string("")) + "Урон: " + red(bold(to_string(d))));
                        break;
                    }
                    case S_POISON: {
                        int d = hit(base * s.mult, el, false, true);
                        say("Урон: " + red(to_string(d)));
                        if (f.resist == EL_POISON) note("Враг устойчив к яду.");
                        else { ePoison = 4; say("Враг " + green("отравлен") + "!"); }
                        break;
                    }
                    case S_MULTI: {
                        int tot = 0;
                        for (int i = 0; i < 3; i++) tot += hit(base * s.mult, el, false, true);
                        say("Три удара, суммарный урон: " + red(bold(to_string(tot))));
                        break;
                    }
                    case S_HEAL: {
                        int h = min((int)(p.intl * s.mult) + 5, p.maxHp() - p.hp);
                        p.hp += h; good("Восстановлено " + to_string(h) + " HP.");
                        break;
                    }
                    case S_BUFF: p.buff = 4; good("Ваша сила растёт на 4 хода!"); break;
                    case S_GUARD: p.guard = true; good("Вы готовитесь уклониться."); break;
                }
                addCharge(12);
                acted = true;
            } else if (c == 3) {
                vector<int> ids;
                for (auto& kv : p.inv) if (isCombatItem(ITEMS[kv.first].type)) ids.push_back(kv.first);
                if (ids.empty()) { note("У вас нет расходников."); continue; }
                for (size_t i = 0; i < ids.size(); i++)
                    say(key((int)i + 1, itemName(ids[i]) + gray(" x" + to_string(p.inv[ids[i]]) + "  " + statsStr(ITEMS[ids[i]]))));
                say(key(0, "Назад"));
                int k = askInt(0, (int)ids.size());
                if (k == 0) continue;
                int id = ids[k - 1];
                const Item it = ITEMS[id];
                if (it.type == BOMB) {
                    p.take(id);
                    say(orange("Бросок: ") + it.name);
                    int d = hit(it.hp * (0.9 + rnd(0, 20) / 100.0), it.elem, true, false);
                    say("Урон: " + red(bold(to_string(d))));
                } else if (it.type == CURE) {
                    if (!p.poison && !p.burn) { note("Вы не отравлены и не горите."); continue; }
                    p.take(id); p.poison = 0; p.burn = 0; good("Яд и огонь сняты.");
                } else useConsumable(p, id);
                acted = true;
            } else if (c == 4) {
                if (f.boss || arena) { bad(arena ? "С арены не сбежать!" : "От такого врага не убежать!"); continue; }
                int fc = min(85, 40 + p.agi + (p.weather == WX_RAIN ? 15 : 0));
                if (chance(fc)) { good("Вам удалось сбежать!"); result = 2; break; }
                bad("Побег не удался!");
                acted = true;
            } else {
                if (charge < 100) { bad("Приём ещё не заряжен (" + to_string(charge) + "/100). Заряд растёт от ударов и полученного урона."); continue; }
                charge = 0;
                if (p.cls == 0) {
                    say(orange(bold("» БЕРСЕРК!")));
                    int d = hit(p.attack() * 3.2, p.weaponElem(), false, true);
                    say("Урон: " + red(bold(to_string(d))));
                    int h = min(p.maxHp() - p.hp, p.maxHp() * 15 / 100);
                    p.hp += h; good("Ярость лечит вас: +" + to_string(h) + " HP.");
                } else if (p.cls == 1) {
                    say(orange(bold("» АРКАННЫЙ ВЗРЫВ!")));
                    int d = hit(p.magic() * 4.0 * (p.weather == WX_STORM ? 1.15 : 1.0), EL_SHOCK, true, false);
                    say("Урон: " + red(bold(to_string(d))));
                    int m = min(p.maxMp() - p.mp, p.maxMp() * 30 / 100);
                    p.mp += m; good("Магия возвращается: +" + to_string(m) + " MP.");
                } else {
                    say(orange(bold("» ТАНЕЦ ТЕНЕЙ!")));
                    int tot = 0;
                    for (int i = 0; i < 5; i++) tot += hit(p.attack() * (chance(60) ? 1.8 : 1.0), p.weaponElem(), false, true);
                    say("Пять ударов, суммарный урон: " + red(bold(to_string(tot))));
                    p.guard = true; good("Вы растворяетесь в тенях — следующая атака пройдёт мимо.");
                }
                acted = true;
            }
        }
        if (result >= 0) break;
        if (f.hp <= 0) { giveRewards(p, f); result = 1; break; }

        // спутник
        if (p.comp >= 0) {
            const Comp& c = COMPS[p.comp];
            if (c.kind == 0) {
                int d = max(1, 4 + p.level * 2 + rnd(0, p.level) - f.def / 4);
                f.hp -= d;
                say(cyan(c.name) + " стреляет во врага: " + red(to_string(d)));
            } else if (c.kind == 1) {
                int h = min(p.maxHp() - p.hp, p.maxHp() * 7 / 100 + p.level);
                if (h > 0) { p.hp += h; say(cyan(c.name) + " лечит вас: " + green("+" + to_string(h))); }
            }
            if (f.hp <= 0) { giveRewards(p, f); result = 1; break; }
        }
        // яд и огонь на враге
        if (ePoison > 0) {
            int d = 4 + p.level;
            f.hp -= d; ePoison--;
            say(f.name + " страдает от яда: " + red("-" + to_string(d)));
        }
        if (eBurn > 0) {
            int d = 3 + p.level + f.maxhp / 50;
            f.hp -= d; eBurn--;
            say(f.name + " " + orange("горит") + ": " + red("-" + to_string(d)));
        }
        if (f.hp <= 0) { giveRewards(p, f); result = 1; break; }
        // фаза ярости дракона
        if (f.ability == 1 && !enraged && f.hp * 2 < f.maxhp) {
            enraged = true;
            eatk = eatk * 125 / 100;
            warnMsg(magenta(bold("Игнатор впадает в ярость! Его атаки стали сильнее.")));
        }
        // ход врага
        if (eStun > 0) {
            say(f.name + " не может двигаться и пропускает ход.");
            eStun--;
        } else {
            eTurn++;
            bool ult = period && eTurn % period == 0;
            if (f.special == 4 && chance(25)) {
                int h = max(1, f.maxhp / 12);
                f.hp = min(f.maxhp, f.hp + h);
                say(f.name + " " + green("регенерирует") + " " + to_string(h) + " HP.");
            }
            if (ult && f.ability == 1) {
                say(red(bold("Игнатор изрыгает ПОТОК ОГНЯ!")));
                if (p.guard) { good("Вы нырнули под пламя и остались невредимы!"); p.guard = false; }
                else {
                    hurt(max(5, (int)(eatk * 1.7) - p.defense() / 4), "Пламя");
                    p.burn = 3;
                    bad("Вы охвачены огнём!");
                }
            } else if (ult && f.ability == 2) {
                say(red(bold(f.name + " обрушивает ТОПОТ!")));
                if (p.guard) { good("Вы отпрыгнули от ударной волны!"); p.guard = false; }
                else {
                    hurt(max(3, (int)(eatk * 1.1) - p.defense() / 2), "Топот");
                    if (chance(70)) { p.stun = 1; bad("Вы оглушены!"); }
                }
            } else {
                bool knight = p.comp >= 0 && COMPS[p.comp].kind == 2 && chance(30);
                if (knight) good(cyan(COMPS[p.comp].name) + " принимает удар на щит!");
                else if (p.guard) { good("Вы ловко уклоняетесь от атаки!"); p.guard = false; }
                else if (chance(p.dodge())) {
                    good("Вы уворачиваетесь от атаки " + f.name + "!");
                    addCharge(8);
                    if (p.cls == 2 && chance(35)) {
                        int d = hit(p.attack() * 0.6, p.weaponElem(), false, false);
                        good("Контратака! Урон: " + red(to_string(d)));
                    }
                } else {
                    double m = 1.0;
                    if (f.special == 3 && chance(25)) { m = 1.8; say(orange("Мощный удар!")); }
                    else if (chance(5)) { m = 1.5; say(orange("Враг наносит критический удар!")); }
                    int d = max(1, (int)(eatk * m * (0.85 + rnd(0, 30) / 100.0)) - p.defense() / 2);
                    hurt(d, f.name);
                    if (f.special == 1 && chance(35)) { p.poison = 3; bad("Вы отравлены!"); }
                    if (f.special == 2 && chance(20)) { p.stun = 1; bad("Вы оглушены!"); }
                }
            }
            if (period && (eTurn + 1) % period == 0 && f.hp > 0) {
                if (f.ability == 1) warnMsg(orange("Игнатор набирает воздух в грудь... В следующий ход — огненное дыхание!"));
                else warnMsg(orange(f.name + " поднимает ногу... В следующий ход — топот!"));
            }
        }
        // конец раунда
        if (p.poison > 0) {
            int d = 3 + p.level / 2;
            p.hp -= d; p.poison--;
            say(green("Яд") + " наносит вам " + red(to_string(d)) + " урона.");
        }
        if (p.burn > 0) {
            int d = 4 + p.level / 2;
            p.hp -= d; p.burn--;
            say(orange("Огонь") + " жжёт вас: " + red(to_string(d)));
        }
        if (p.cls == 1) {
            int m = min(p.maxMp() - p.mp, 2 + p.level / 3);
            if (m > 0) { p.mp += m; say(blue("Поток маны: +" + to_string(m) + " MP")); }
        }
        if (p.buff > 0) p.buff--;
        if (f.hp <= 0) { giveRewards(p, f); result = 1; break; }
        if (p.hp <= 0) { result = 0; break; }
    }
    p.poison = 0; p.burn = 0; p.buff = 0; p.stun = 0; p.guard = false;
    return result;
}

void handleDeath(Player& p) {
    cout << "\n";
    banner("ВЫ ПОГИБЛИ", Col::RED);
    int lost = p.gold * rnd(30, 50) / 100;
    p.gold -= lost;
    p.loc = 0;
    p.hp = max(1, p.maxHp() / 2);
    p.mp = p.maxMp() / 2;
    passTime(p, 12);
    panelText("Спасение", "Деревенские нашли вас без чувств и выходили. Потеряно золота: " + to_string(lost) +
                          ". Вы очнулись в деревне.", Col::RED);
}
bool fightFoe(Player& p, const Foe& f) {
    int r = combat(p, f, false);
    if (r == 0) { handleDeath(p); waitEnter(); return false; }
    return true;
}
bool fight(Player& p, int ei) { return fightFoe(p, makeFoe(ei)); }

// ============================================================================
//  Деревня: лавка
// ============================================================================
vector<int> dailyDeals(int day) {
    static const vector<int> pool = {I_ELIXIR, I_BOMB, I_ANTIDOTE, I_HP_L, I_MP_L, I_RING_STR,
                                     I_AMULET, I_RING_WIS, I_AXE, I_CHAIN, I_STAFF2, I_STEEL};
    mt19937 g((unsigned)day * 2654435761u + 17u);
    vector<int> v = pool;
    shuffle(v.begin(), v.end(), g);
    v.resize(2);
    return v;
}

void shopMenu(Player& p) {
    while (true) {
        if (p.dealDay != p.day()) { p.dealDay = p.day(); p.dealMask = 0; }
        struct Offer { int id, price, deal; };
        vector<Offer> offers;
        auto deals = dailyDeals(p.day());
        for (int i = 0; i < 2; i++)
            if (!(p.dealMask & (1 << i))) offers.push_back({deals[i], max(1, ITEMS[deals[i]].price * 70 / 100), i});
        vector<int> stock = {I_RUSTY, I_STEEL, I_DAGGER, I_STAFF1, I_LEATHER, I_ROBE, I_CHAIN,
                             I_HP_S, I_HP_L, I_MP_S, I_MP_L, I_ANTIDOTE, I_BOMB, I_ICEBOMB, I_ACID, I_HOLYWATER};
        if (p.level >= 4 || p.rep >= 20)
            for (int id : {I_AXE, I_STAFF2, I_PLATE, I_RING_STR, I_AMULET, I_RING_WIS}) stock.push_back(id);
        for (int id : stock) offers.push_back({id, max(1, ITEMS[id].price * (100 - p.discount()) / 100), -1});

        vector<string> v;
        for (size_t i = 0; i < offers.size(); i++) {
            const Offer& o = offers[i];
            string nm = key((int)i + 1, itemName(o.id)) + (o.deal >= 0 ? orange(" -30%") : "");
            string pr = (p.gold >= o.price ? yellow(to_string(o.price)) : gray(to_string(o.price))) + " зол.";
            string st = gray(statsStr(ITEMS[o.id]));
            v.push_back(padR(nm, 33) + padR(st, 19) + padL(pr, 10));
        }
        v.push_back("");
        v.push_back(lr("Золото: " + yellow(to_string(p.gold)) + gray("   Скидка: " + to_string(p.discount()) + "%"),
                       key((int)offers.size() + 1, "Продать") + "   " + key(0, "Выйти")));
        cout << "\n";
        panel("Лавка торговца", v, Col::YELLOW);
        int c = askInt(0, (int)offers.size() + 1);
        if (c == 0) return;
        if (c == (int)offers.size() + 1) {
            vector<int> ids;
            for (auto& kv : p.inv) ids.push_back(kv.first);
            if (ids.empty()) { note("Продавать нечего."); continue; }
            vector<string> s;
            for (size_t i = 0; i < ids.size(); i++)
                s.push_back(padR(key((int)i + 1, itemName(ids[i]) + gray(" x" + to_string(p.inv[ids[i]]))), 40) +
                            padL(yellow(to_string(max(1, ITEMS[ids[i]].price / 2))) + " зол.", 14));
            s.push_back(key(0, "Назад"));
            panel("Продажа", s, Col::YELLOW);
            int k = askInt(0, (int)ids.size());
            if (k == 0) continue;
            int id = ids[k - 1];
            int q = askQty(p.count(id));
            if (q <= 0) continue;
            int sum = max(1, ITEMS[id].price / 2) * q;
            p.take(id, q); p.gold += sum;
            good("Продано: " + itemName(id) + " x" + to_string(q) + " за " + yellow(to_string(sum)) + " зол.");
            continue;
        }
        const Offer& o = offers[c - 1];
        const Item& it = ITEMS[o.id];
        int q = 1;
        if (o.deal < 0 && !isEquip(it.type)) q = askQty(min(10, p.gold / o.price));
        if (q <= 0) { if (p.gold < o.price) bad("Недостаточно золота."); continue; }
        if (p.gold < o.price * q) { bad("Недостаточно золота."); continue; }
        p.gold -= o.price * q;
        p.add(o.id, q);
        if (o.deal >= 0) p.dealMask |= (1 << o.deal);
        good("Куплено: " + itemName(o.id) + " x" + to_string(q) + " за " + yellow(to_string(o.price * q)) + " зол.");
    }
}

// ============================================================================
//  Деревня: кузница и алхимик
// ============================================================================
void craftMenu(Player& p, const string& title, const vector<Recipe>& recs, bool alchemy) {
    while (true) {
        vector<string> v;
        for (size_t i = 0; i < recs.size(); i++) {
            const Recipe& r = recs[i];
            bool ok = p.gold >= r.gold;
            vector<string> toks;
            for (auto& m : r.mats) {
                bool have = p.count(m.first) >= m.second;
                if (!have) ok = false;
                toks.push_back((have ? green : red)(ITEMS[m.first].name + " " + to_string(p.count(m.first)) + "/" + to_string(m.second)));
            }
            if (r.gold) toks.push_back(gray("+ " + to_string(r.gold) + " зол."));
            v.push_back(key((int)i + 1, itemName(r.result) + gray("  " + statsStr(ITEMS[r.result])) + (ok ? green("  ✓") : "")));
            string cur = "   ";
            for (auto& t : toks) {  // перенос материалов, чтобы строка не вылезала за рамку
                if (vlen(cur) + vlen(t) + 2 > W - 2) { v.push_back(cur); cur = "   "; }
                cur += (cur == "   " ? "" : "  ") + t;
            }
            v.push_back(cur);
        }
        v.push_back("");
        v.push_back(lr("Золото: " + yellow(to_string(p.gold)), key(0, "Назад")));
        cout << "\n";
        panel(title, v, Col::ORANGE);
        int k = askInt(0, (int)recs.size());
        if (k == 0) return;
        const Recipe& r = recs[k - 1];
        bool ok = p.gold >= r.gold;
        for (auto& m : r.mats) if (p.count(m.first) < m.second) ok = false;
        if (!ok) { bad("Не хватает материалов или золота."); continue; }
        p.gold -= r.gold;
        for (auto& m : r.mats) p.take(m.first, m.second);
        int n = 1;
        if (alchemy && chance(15)) { n = 2; gain("Удачная варка! Получено вдвое больше."); }
        p.add(r.result, n);
        good((alchemy ? "Вы сварили: " : "Кузнец выковал: ") + itemName(r.result) + (n > 1 ? " x2" : "") + "!");
        unlockAch(p, alchemy ? A_ALCH : A_SMITH);
    }
}

void sharpenMenu(Player& p) {
    if (!p.eq[0]) { bad("Сначала наденьте оружие."); return; }
    if (p.sharp >= 5) { note("Оружие заточено до предела (+5)."); return; }
    int cost = 50 * (p.sharp + 1);
    int ch = p.sharp < 3 ? 95 : p.sharp == 3 ? 75 : 55;
    panelText("Заточка", "Заточка до +" + to_string(p.sharp + 1) + ": " + to_string(cost) + " зол. и 1 железная руда (у вас " +
                         to_string(p.count(I_ORE)) + "). Шанс успеха: " + to_string(ch) + "%. При неудаче ресурсы пропадают.", Col::ORANGE);
    if (!askYesNo("Попробовать?")) return;
    if (p.gold < cost || p.count(I_ORE) < 1) { bad("Не хватает ресурсов."); return; }
    p.gold -= cost; p.take(I_ORE);
    if (chance(ch)) { p.sharp++; good("Оружие заточено! Бонус к атаке: " + yellow("+" + to_string(p.sharp))); }
    else bad("Заточка сорвалась! Руда и золото потеряны.");
}

void smithMenu(Player& p) {
    while (true) {
        cout << "\n";
        panel("Кузница", {"Старый кузнец вытирает руки о фартук."}, Col::ORANGE);
        grid({key(1, "Ковать предметы"), key(2, "Заточить оружие"), key(0, "Выйти")}, 3);
        int c = askInt(0, 2);
        if (c == 0) return;
        if (c == 1) craftMenu(p, "Кузница: рецепты", SMITH, false);
        else sharpenMenu(p);
    }
}

// ============================================================================
//  Деревня: таверна, храм, арена, доска заданий
// ============================================================================
void companionMenu(Player& p) {
    vector<string> v;
    for (size_t i = 0; i < COMPS.size(); i++)
        v.push_back(key((int)i + 1, cyan(COMPS[i].name) + gray(" — " + COMPS[i].desc)) + "  " + yellow(to_string(COMPS[i].cost) + " зол."));
    v.push_back("");
    v.push_back(p.comp >= 0 ? "Сейчас с вами: " + cyan(COMPS[p.comp].name) : gray("Спутника нет."));
    v.push_back(gray("Жалованье: " + to_string(UPKEEP) + " зол. в день."));
    v.push_back(lr(p.comp >= 0 ? key(4, "Распустить") : "", key(0, "Назад")));
    panel("Наёмники", v, Col::CYAN);
    int c = askInt(0, 4);
    if (c == 0) return;
    if (c == 4) {
        if (p.comp >= 0) { note(COMPS[p.comp].name + " уходит."); p.comp = -1; }
        return;
    }
    const Comp& cm = COMPS[c - 1];
    if (p.comp == c - 1) { note("Он уже с вами."); return; }
    if (p.gold < cm.cost) { bad("Не хватает золота."); return; }
    if (p.comp >= 0 && !askYesNo("Заменить текущего спутника? Деньги не вернутся.")) return;
    p.gold -= cm.cost; p.comp = c - 1;
    good(cyan(cm.name) + " присоединяется к вам!");
}

void gambleMenu(Player& p) {
    while (true) {
        cout << "\n";
        panel("Азартные игры", {"Золото: " + yellow(to_string(p.gold))}, Col::MAGENTA);
        grid({key(1, "Дуэль костей (x2)"), key(2, "Больше/меньше 7"), key(0, "Назад")}, 3);
        int c = askInt(0, 2);
        if (c == 0) return;
        if (p.gold < 1) { bad("У вас нет денег."); return; }
        int maxBet = min(p.gold, 100);
        say("Ваша ставка (0 — отмена, макс. " + to_string(maxBet) + "):");
        int bet = askInt(0, maxBet);
        if (bet == 0) continue;
        if (c == 1) {
            int a = rnd(1, 6) + rnd(1, 6), b = rnd(1, 6) + rnd(1, 6);
            say("Вы бросили: " + bold(to_string(a)) + ", соперник: " + bold(to_string(b)));
            if (a > b) { p.gold += bet; p.diceWins++; good("Вы выиграли " + yellow(to_string(bet)) + " золота!"); }
            else if (a < b) { p.gold -= bet; bad("Вы проиграли " + to_string(bet) + " золота."); }
            else note("Ничья, ставка возвращена.");
        } else {
            grid({key(1, "Больше 7 (x2)"), key(2, "Меньше 7 (x2)"), key(3, "Ровно 7 (x5)")}, 3);
            int g = askInt(1, 3);
            int s = rnd(1, 6) + rnd(1, 6);
            say("Выпало: " + bold(to_string(s)));
            bool win = (g == 1 && s > 7) || (g == 2 && s < 7) || (g == 3 && s == 7);
            if (win) {
                int prize = bet * (g == 3 ? 4 : 1);
                p.gold += prize; p.diceWins++;
                good("Вы выиграли " + yellow(to_string(prize)) + " золота!");
            } else { p.gold -= bet; bad("Не повезло. Потеряно " + to_string(bet) + " золота."); }
        }
    }
}

bool saveGame(const Player& p, int slot);

void tavernMenu(Player& p) {
    static const vector<string> rumors = {
        "«Пауки в пещере ядовиты. Берите противоядие!»",
        "«Из драконьей чешуи кузнец куёт лучшие доспехи в мире.»",
        "«Ночью опаснее, но и опыта за врагов дают больше.»",
        "«Тролли регенерируют. Бейте их быстро и сильно!»",
        "«Тёмные маги умеют оглушать. Не зевайте.»",
        "«У дракона есть предупреждающий жест перед огненным дыханием. Уклонитесь!»",
        "«Заточка оружия иногда срывается — зато стоит недорого.»",
        "«Лунный цветок лучше искать ночью.»",
        "«Король троллей Грок топчет так, что земля дрожит. Готовьте уклонение.»",
        "«Если сундук слишком красив — в нём может сидеть мимик.»",
        "«Туман — друг ловкачей: уклоняться проще.»",
        "«Алхимик иногда варит двойную порцию — повезёт, так повезёт.»",
    };
    while (true) {
        cout << "\n";
        panel("Таверна «Пьяный грифон»", {lr("Золото: " + yellow(to_string(p.gold)), gray(timeStr(p)))}, Col::YELLOW);
        grid({key(1, "Снять комнату (10 зол.)"), key(2, "Послушать слухи"),
              key(3, "Выпить эля (6 зол.)"), key(4, "Азартные игры"),
              key(5, "Наёмники"), key(0, "Выйти")}, 2);
        int c = askInt(0, 5);
        if (c == 0) return;
        if (c == 1) {
            if (p.gold < 10) { bad("Не хватает золота."); continue; }
            p.gold -= 10;
            int h = p.hour();
            passTime(p, (h < 8) ? 8 - h : 24 - h + 8);
            p.hp = p.maxHp(); p.mp = p.maxMp();
            good("Вы отлично выспались. " + timeStr(p) + ". Здоровье и мана полностью восстановлены.");
            if (saveGame(p, 0)) note("Игра автоматически сохранена (слот «Автосохранение»).");
        } else if (c == 2) {
            say(gray("Посетитель шепчет: ") + pickOne(rumors));
        } else if (c == 3) {
            if (p.gold < 6) { bad("Не хватает золота."); continue; }
            p.gold -= 6;
            if (chance(75)) { p.nextBuff = 3; good("Боевой настрой! В следующем бою вы будете сильнее."); }
            else note("Эль оказался кислым. Ничего не произошло.");
        } else if (c == 4) gambleMenu(p);
        else companionMenu(p);
    }
}

void templeMenu(Player& p) {
    while (true) {
        cout << "\n";
        panel("Храм Рассвета",
              {"Жрица встречает вас тёплой улыбкой.",
               lr("Золото: " + yellow(to_string(p.gold)),
                  p.blessFights > 0 ? green("Благословение: боёв " + to_string(p.blessFights)) : gray("Без благословения"))},
              Col::MAGENTA);
        grid({key(1, "Исцеление (25 зол.)"), key(2, "Благословение (50 зол.)"),
              key(3, "Пожертвовать 50 зол."), key(0, "Выйти")}, 2);
        int c = askInt(0, 3);
        if (c == 0) return;
        if (c == 1) {
            if (p.hp == p.maxHp() && p.mp == p.maxMp()) { note("Вы полностью здоровы."); continue; }
            if (p.gold < 25) { bad("Не хватает золота."); continue; }
            p.gold -= 25; p.hp = p.maxHp(); p.mp = p.maxMp();
            good("Тёплый свет окутывает вас. Здоровье и мана восстановлены.");
        } else if (c == 2) {
            if (p.gold < 50) { bad("Не хватает золота."); continue; }
            p.gold -= 50; p.blessFights = 5;
            good("Благословение: следующие 5 побед дадут +20% опыта и +10% золота.");
        } else {
            if (p.gold < 50) { bad("Не хватает золота."); continue; }
            p.gold -= 50; p.rep += 3;
            good("Жрица благодарит вас. Репутация " + yellow("+3") + ".");
            if (chance(20)) { p.add(I_HP_L); gain("В дар вам вручают " + itemName(I_HP_L) + "!"); }
        }
    }
}

void arenaMenu(Player& p) {
    int fee = 30 + p.level * 5;
    cout << "\n";
    panelText("Арена «Кровавый песок»",
              "Три раунда подряд. Выигрыш копится в банке: между раундами его можно забрать и уйти, "
              "но если вы падёте — банк сгорит. Победа во всех раундах даёт бонус в размере трёх взносов. "
              "Взнос: " + to_string(fee) + " зол.", Col::RED);
    grid({key(1, "Участвовать"), key(0, "Выйти")}, 2);
    if (askInt(0, 1) == 0) return;
    if (p.gold < fee) { bad("Не хватает золота на взнос."); return; }
    p.gold -= fee;
    int pot = 0;
    for (int round = 1; round <= 3; round++) {
        vector<int> el;
        for (int i = 0; i < (int)ENEMIES.size(); i++) {
            if (ENEMIES[i].boss) continue;
            int t = enemyTier(i);
            if (t >= max(1, p.level - 3) && t <= p.level + round - 1) el.push_back(i);
        }
        if (el.empty()) for (int i = 0; i <= E_GOLEM; i++) if (enemyTier(i) <= p.level + round) el.push_back(i);
        if (el.empty()) el.push_back(E_RAT);
        int a = pickOne(el), b = pickOne(el);
        int ei = ENEMIES[a].hp >= ENEMIES[b].hp ? a : b;
        Foe f = makeFoe(ei);
        cout << "\n";
        banner("Арена: раунд " + to_string(round) + "/3", Col::RED);
        int r = combat(p, f, true);
        if (r == 0) {
            bad("Вы пали на арене. Банк сгорел.");
            p.hp = max(1, p.maxHp() / 4);
            p.mp = p.maxMp() / 4;
            waitEnter();
            return;
        }
        pot += f.gold * 2 + rnd(5, 20) * round;
        gain("Банк арены: " + yellow(to_string(pot)) + " зол.");
        if (round < 3) {
            p.hp = min(p.maxHp(), p.hp + p.maxHp() / 5);
            note("Вам дают короткую передышку (+20% здоровья).");
            if (askYesNo("Забрать " + to_string(pot) + " зол. и уйти?")) {
                p.gold += pot;
                good("Вы покидаете арену с выигрышем.");
                return;
            }
        }
    }
    pot += fee * 3;
    p.gold += pot; p.arenaWins++; p.rep += 2;
    banner("ЧЕМПИОН АРЕНЫ!", Col::YELLOW);
    good("Вы получаете " + yellow(to_string(pot)) + " золота. Репутация +2.");
    receiveGear(p, randomGear(p.level, 10, 1), nullptr);
    checkAch(p);
}

// ============================================================================
//  Квесты и ежедневный заказ
// ============================================================================
void ensureBounty(Player& p) {
    if (p.bntDay == p.day()) return;
    p.bntDay = p.day(); p.bntState = 0; p.bntProg = 0;
    vector<int> el;
    for (int i = 0; i < (int)ENEMIES.size(); i++) {
        if (ENEMIES[i].boss) continue;
        int t = enemyTier(i);
        if (t >= 1 && t <= p.level + 1) el.push_back(i);
    }
    if (el.empty()) el.push_back(E_RAT);
    int a = pickOne(el), b = pickOne(el);
    p.bntEnemy = ENEMIES[a].hp >= ENEMIES[b].hp ? a : b;
    p.bntNeed = rnd(3, 6);
    const EnemyT& e = ENEMIES[p.bntEnemy];
    p.bntGold = e.gold * p.bntNeed * 2 + rnd(10, 40);
    p.bntXp = e.xp * p.bntNeed;
}

bool questComplete(const Player& p, int q) {
    const Quest& Q = QUESTS[q];
    if (Q.type == Q_KILL) { auto it = p.qprog.find(q); return it != p.qprog.end() && it->second >= Q.count; }
    return p.count(Q.target) >= Q.count;
}
string questProgress(const Player& p, int q) {
    const Quest& Q = QUESTS[q];
    int cur = Q.type == Q_KILL ? (p.qprog.count(q) ? p.qprog.at(q) : 0) : p.count(Q.target);
    return to_string(min(cur, Q.count)) + "/" + to_string(Q.count);
}

void questBoard(Player& p) {
    while (true) {
        ensureBounty(p);
        struct Opt { int kind, id; };  // 1 взять, 2 сдать, 3 взять заказ, 4 сдать заказ
        vector<Opt> opts;
        vector<string> v;
        v.push_back(bold("Задания старосты"));
        bool any = false;
        for (size_t i = 0; i < QUESTS.size(); i++) {
            int st = p.quest.count((int)i) ? p.quest.at((int)i) : 0;
            const Quest& Q = QUESTS[i];
            if (st == 0 && p.level >= Q.minLvl) {
                opts.push_back({1, (int)i}); any = true;
                v.push_back(key((int)opts.size(), cyan("[Взять] ") + Q.title) + gray("  " + to_string(Q.gold) + " зол., " + to_string(Q.xp) + " оп."));
                v.push_back("     " + gray(Q.desc));
            } else if (st == 1) {
                opts.push_back({2, (int)i}); any = true;
                bool done = questComplete(p, (int)i);
                v.push_back(key((int)opts.size(), (done ? green("[Сдать] ") : yellow("[В работе] ")) + Q.title) + gray("  " + questProgress(p, (int)i)));
            } else if (st == 0) {
                v.push_back(gray("  [ур. " + to_string(Q.minLvl) + "+] " + Q.title));
            }
        }
        if (!any) v.push_back(gray("  Подходящих заданий пока нет."));
        v.push_back("");
        v.push_back(bold("Заказ дня") + gray("  (обновляется каждый день)"));
        const EnemyT& be = ENEMIES[p.bntEnemy];
        string bt = "Убить: " + be.name + " x" + to_string(p.bntNeed) + gray("  " + to_string(p.bntGold) + " зол., " + to_string(p.bntXp) + " оп.");
        if (p.bntState == 0) { opts.push_back({3, 0}); v.push_back(key((int)opts.size(), cyan("[Взять] ") + bt)); }
        else if (p.bntState == 1) {
            bool done = p.bntProg >= p.bntNeed;
            opts.push_back({4, 0});
            v.push_back(key((int)opts.size(), (done ? green("[Сдать] ") : yellow("[В работе] ")) + bt) + gray("  " + to_string(min(p.bntProg, p.bntNeed)) + "/" + to_string(p.bntNeed)));
        } else v.push_back(green("  ✓ Заказ дня выполнен. Приходите завтра."));
        v.push_back("");
        v.push_back(key(0, "Назад"));
        cout << "\n";
        panel("Доска заданий", v, Col::GREEN);
        int c = askInt(0, (int)opts.size());
        if (c == 0) return;
        Opt o = opts[c - 1];
        if (o.kind == 1) {
            p.quest[o.id] = 1; p.qprog[o.id] = 0;
            good("Задание принято: " + bold(QUESTS[o.id].title));
        } else if (o.kind == 2) {
            if (!questComplete(p, o.id)) { bad("Задание ещё не выполнено."); continue; }
            const Quest& Q = QUESTS[o.id];
            if (Q.type == Q_COLLECT) p.take(Q.target, Q.count);
            p.quest[o.id] = 2; p.gold += Q.gold; p.xp += Q.xp; p.rep += 3 + Q.xp / 100;
            gain("Задание выполнено! Награда: " + yellow(to_string(Q.gold)) + " зол., " + yellow(to_string(Q.xp)) + " опыта, репутация +" + to_string(3 + Q.xp / 100));
            if (Q.item) { p.add(Q.item); gain("Предмет: " + itemName(Q.item)); }
            levelUp(p);
            checkAch(p);
        } else if (o.kind == 3) {
            p.bntState = 1; p.bntProg = 0;
            good("Заказ принят.");
        } else {
            if (p.bntProg < p.bntNeed) { bad("Заказ ещё не выполнен."); continue; }
            p.bntState = 2; p.gold += p.bntGold; p.xp += p.bntXp; p.rep += 2;
            gain("Заказ выполнен! " + yellow(to_string(p.bntGold)) + " зол., " + yellow(to_string(p.bntXp)) + " опыта, репутация +2");
            if (chance(40)) {
                int it = pickOne(vector<int>{I_HP_L, I_MP_L, I_BOMB, I_CRYSTAL});
                p.add(it); gain("Бонус от заказчика: " + itemName(it));
            }
            levelUp(p);
            checkAch(p);
        }
    }
}

// ============================================================================
//  Журнал: задания, бестиарий, достижения, карта
// ============================================================================
void showQuests(const Player& p) {
    vector<string> v;
    for (auto& kv : p.quest)
        if (kv.second == 1) {
            v.push_back(bold(QUESTS[kv.first].title) + gray("  [" + questProgress(p, kv.first) + "]") +
                        (questComplete(p, kv.first) ? green("  ГОТОВО") : ""));
            v.push_back("   " + gray(QUESTS[kv.first].desc));
        }
    if (p.bntState == 1) {
        v.push_back(bold("Заказ дня: ") + ENEMIES[p.bntEnemy].name + gray("  [" + to_string(min(p.bntProg, p.bntNeed)) + "/" + to_string(p.bntNeed) + "]"));
    }
    if (v.empty()) v.push_back(gray("Активных заданий нет."));
    cout << "\n";
    panel("Журнал заданий", v, Col::GREEN);
}
void showBestiary(const Player& p) {
    vector<string> v;
    for (size_t i = 0; i < ENEMIES.size(); i++) {
        int k = i < p.beast.size() ? p.beast[i] : 0;
        const EnemyT& e = ENEMIES[i];
        if (k == 0) { v.push_back(gray(padR("???", 24) + "не встречен")); continue; }
        string nm = e.boss ? magenta(e.name) : e.name;
        string st = k >= 3 ? gray("HP " + to_string(e.hp) + " АТК " + to_string(e.atk) + " ЗАЩ " + to_string(e.def))
                           : gray("изучено: " + to_string(k) + "/3");
        v.push_back(padR(nm, 22) + padR(st, 26) + "убито: " + bold(to_string(k)));
        if (k >= 3) {
            string wk = "   слабость: " + (e.weak ? elemStr(e.weak) : gray("нет"));
            if (e.resist) wk += "   сопротивление: " + elemStr(e.resist);
            v.push_back(wk);
        }
    }
    cout << "\n";
    panel("Бестиарий", v, Col::RED);
}
void showAchievements(const Player& p) {
    vector<string> v;
    int n = 0;
    for (size_t i = 0; i < ACHS.size(); i++) {
        bool got = p.ach & (1u << i);
        if (got) n++;
        v.push_back((got ? yellow("★ ") + bold(ACHS[i].name) : gray("☆ " + ACHS[i].name)) + gray("  " + ACHS[i].desc));
    }
    v.push_back("");
    v.push_back("Получено: " + bold(to_string(n) + "/" + to_string(ACHS.size())));
    cout << "\n";
    panel("Достижения", v, Col::YELLOW);
}
void showMap(const Player& p) {
    vector<string> v;
    for (size_t i = 0; i < LOCS.size(); i++) {
        const Loc& L = LOCS[i];
        bool here = (int)i == p.loc;
        v.push_back((here ? cyan("◆ ") : "  ") + (here ? bold(L.name) : L.name) + gray("   рек. уровень: " + to_string(L.minLvl)));
        string nb;
        for (size_t j = 0; j < L.nb.size(); j++) nb += (j ? ", " : "") + LOCS[L.nb[j]].name;
        v.push_back("    " + gray("→ " + nb));
    }
    cout << "\n";
    panel("Карта мира", v, Col::CYAN);
}
void journalMenu(Player& p) {
    while (true) {
        cout << "\n";
        grid({key(1, "Задания"), key(2, "Бестиарий"), key(3, "Достижения"), key(4, "Карта мира"), key(0, "Назад")}, 3);
        int c = askInt(0, 4);
        if (c == 0) return;
        if (c == 1) showQuests(p);
        else if (c == 2) showBestiary(p);
        else if (c == 3) showAchievements(p);
        else showMap(p);
    }
}

// ============================================================================
//  Случайные события
// ============================================================================
struct Riddle { string q; vector<string> ans; };  // верный ответ — первый
static const vector<Riddle> RIDDLES = {
    {"Не лает, не кусает, а в дом не пускает. Что это?", {"Замок", "Забор", "Сторож"}},
    {"Чем больше из неё берёшь, тем больше она становится. Что это?", {"Яма", "Гора", "Река"}},
    {"Зимой и летом одним цветом. Что это?", {"Ель", "Камень", "Берёза"}},
    {"Без окон, без дверей — полна горница людей. Что это?", {"Огурец", "Амбар", "Колодец"}},
    {"Что можно увидеть с закрытыми глазами?", {"Сон", "Тьму вокруг", "Своё имя"}},
};

void eventChest(Player& p) {
    say("Среди корней виднеется старый окованный сундук.");
    if (!askYesNo("Открыть?")) { note("Вы проходите мимо."); return; }
    int r = rnd(1, 100);
    if (r <= 25) {
        warnMsg("Крышка раскрывается пастью — это " + bold("МИМИК") + "!");
        Foe f = makeFoe(E_MIMIC);
        f.maxhp = f.hp = 70 + p.level * 14;
        f.atk = 12 + p.level * 2;
        fightFoe(p, f);
    } else if (r <= 45) {
        int d = rnd(4, 9) + p.level * 2;
        p.hp = max(1, p.hp - d);
        bad("Из сундука вылетают отравленные иглы! " + red("-" + to_string(d) + " HP"));
    } else {
        int g = rnd(15, 40) * max(1, p.level) / 2 + 10;
        p.gold += g;
        gain("В сундуке лежит " + yellow(to_string(g)) + " золота!");
        if (chance(20)) receiveGear(p, randomGear(p.level, 4), nullptr);
        if (chance(35)) {
            int it = pickOne(vector<int>{I_HP_S, I_MP_S, I_HERB, I_ORE, I_CRYSTAL, I_BOMB});
            p.add(it);
            gain("А ещё: " + itemName(it));
        }
    }
}
void eventSpring(Player& p) {
    int h = p.maxHp() / 3, m = p.maxMp() / 5;
    p.hp = min(p.maxHp(), p.hp + h);
    p.mp = min(p.maxMp(), p.mp + m);
    good("Вы нашли целебный родник. " + green("+" + to_string(h) + " HP") + ", " + blue("+" + to_string(m) + " MP"));
}
void eventTrap(Player& p) {
    say("Под ногой щёлкает скрытая пластина...");
    if (chance(p.dodge() + 25)) { good("Вы вовремя отскочили — ловушка не сработала!"); return; }
    int d = rnd(3, 8) + p.level;
    p.hp = max(1, p.hp - d);
    bad("Ловушка! Потеряно " + red(to_string(d)) + " HP.");
}
void eventMerchant(Player& p) {
    static const vector<int> goods = {I_HP_L, I_MP_L, I_ANTIDOTE, I_BOMB, I_ELIXIR, I_HP_S, I_MP_S};
    int id = pickOne(goods);
    int price = max(1, ITEMS[id].price * rnd(70, 110) / 100);
    say("Вам встречается странствующий торговец. Он предлагает " + itemName(id) + " за " + yellow(to_string(price)) + " зол.");
    if (!askYesNo("Купить?")) return;
    if (p.gold < price) { bad("У вас не хватает золота."); return; }
    p.gold -= price; p.add(id);
    good("Сделка совершена.");
}
void eventShrine(Player& p) {
    say("Вы наткнулись на древнее святилище.");
    grid({key(1, "Помолиться"), key(2, "Принести дар (30 зол.)"), key(0, "Уйти")}, 1);
    int c = askInt(0, 2);
    if (c == 1) {
        if (chance(60)) {
            switch (rnd(1, 3)) {
                case 1: p.baseHp += 4; good("Боги благосклонны: максимальное здоровье " + yellow("+4") + "!"); break;
                case 2: p.str += 1; good("Боги благосклонны: сила " + yellow("+1") + "!"); break;
                default: p.intl += 1; good("Боги благосклонны: интеллект " + yellow("+1") + "!"); break;
            }
        } else {
            p.hp = max(1, p.hp - 8);
            bad("Святилище отвергло вас. Потеряно 8 HP.");
        }
    } else if (c == 2) {
        if (p.gold < 30) { bad("Не хватает золота."); return; }
        p.gold -= 30; p.nextBuff = 4;
        good("Дар принят: в следующем бою вас ждёт прилив сил.");
        if (chance(50)) { p.hp = min(p.maxHp(), p.hp + p.maxHp() / 5); good("Вас окутывает тёплый свет (+20% HP)."); }
    }
}
void eventTraveler(Player& p) {
    say("У дороги лежит раненый путник и тихо стонет.");
    grid({key(1, "Помочь (малое зелье лечения)"), key(2, "Обыскать карманы"), key(0, "Пройти мимо")}, 1);
    int c = askInt(0, 2);
    if (c == 1) {
        if (!p.take(I_HP_S)) {
            note("У вас нет малого зелья. Вы лишь перевязываете раны путника.");
            p.rep += 1;
            return;
        }
        if (chance(20)) { warnMsg("Это была ловушка! Путник оказался бандитом!"); fight(p, E_BANDIT); return; }
        int g = rnd(25, 70) + p.level * 5;
        p.gold += g; p.rep += 2;
        gain("Путник благодарит и отдаёт " + yellow(to_string(g)) + " золота. Репутация +2.");
        if (chance(25)) { p.add(I_HERB, 2); gain("И ещё дарит " + itemName(I_HERB) + " x2."); }
    } else if (c == 2) {
        p.rep = max(0, p.rep - 3);
        if (chance(35)) { warnMsg("Путник хватается за оружие — он не так слаб!"); fight(p, E_BANDIT); }
        else {
            int g = rnd(10, 35);
            p.gold += g;
            bad("Вы забрали " + to_string(g) + " золота. Совесть неспокойна: репутация падает.");
        }
    } else note("Вы идёте дальше.");
}
void eventMushroom(Player& p) {
    say("На полянке растут странные светящиеся грибы.");
    if (!askYesNo("Попробовать гриб?")) return;
    int r = rnd(1, 100);
    if (r <= 35) { int h = p.maxHp() / 4; p.hp = min(p.maxHp(), p.hp + h); good("Вкусно и сытно! " + green("+" + to_string(h) + " HP")); }
    else if (r <= 60) { int m = p.maxMp() * 4 / 10; p.mp = min(p.maxMp(), p.mp + m); good("В голове проясняется. " + blue("+" + to_string(m) + " MP")); }
    else if (r <= 80) { int d = max(1, p.maxHp() / 10); p.hp = max(1, p.hp - d); bad("Гриб оказался ядовитым. " + red("-" + to_string(d) + " HP")); }
    else { int x = 10 + p.level * 5; p.xp += x; gain("Вам открываются странные видения. Опыт " + yellow("+" + to_string(x))); levelUp(p); }
}
void eventRiddle(Player& p) {
    const Riddle& r = pickOne(RIDDLES);
    say("Седой отшельник преграждает путь: «" + r.q + "»");
    vector<int> idx = {0, 1, 2};
    shuffle(idx.begin(), idx.end(), rng);
    for (int i = 0; i < 3; i++) say(key(i + 1, r.ans[idx[i]]));
    say(key(0, "Промолчать"));
    int c = askInt(0, 3);
    if (c == 0) { note("Отшельник пожимает плечами."); return; }
    if (idx[c - 1] == 0) {
        int g = 20 + p.level * 10, x = 15 + p.level * 8;
        p.gold += g; p.xp += x;
        gain("Верно! Старик награждает вас: " + yellow(to_string(g)) + " зол. и " + yellow(to_string(x)) + " опыта.");
        levelUp(p);
    } else {
        int l = min(p.gold, 5 + p.level);
        p.gold -= l;
        bad("Неверно. Отшельник берёт " + to_string(l) + " зол. за урок мудрости.");
    }
}
void eventCampfire(Player& p) {
    say("У костра сидят путники и зовут разделить трапезу.");
    int h = p.maxHp() / 5, m = p.maxMp() / 5;
    p.hp = min(p.maxHp(), p.hp + h);
    p.mp = min(p.maxMp(), p.mp + m);
    good("Вы отдохнули у огня. " + green("+" + to_string(h) + " HP") + ", " + blue("+" + to_string(m) + " MP"));
    if (chance(30)) { p.add(I_HP_S); gain("На прощание вам дарят " + itemName(I_HP_S) + "."); }
}

void randomEvent(Player& p) {
    struct E { int w; void (*fn)(Player&); };
    static const vector<E> ev = {{14, eventChest}, {10, eventSpring}, {9, eventTrap}, {12, eventMerchant},
                                 {10, eventShrine}, {12, eventTraveler}, {8, eventMushroom},
                                 {10, eventRiddle}, {8, eventCampfire}};
    int tot = 0;
    for (auto& e : ev) tot += e.w;
    int r = rnd(1, tot);
    cout << "\n";
    rule("Происшествие");
    for (auto& e : ev) {
        if (r <= e.w) { e.fn(p); break; }
        r -= e.w;
    }
    p.clampVitals();
}

// ============================================================================
//  Дикая местность: исследование, сбор, лагерь, путешествие
// ============================================================================
void lightning(Player& p) {
    int d = max(3, p.maxHp() / 10);
    p.hp = max(1, p.hp - d);
    bad("Молния бьёт совсем рядом! " + red("-" + to_string(d) + " HP"));
}

void explore(Player& p) {
    passTime(p, 1);
    cout << "\n";
    rule("Исследование — " + timeStr(p));
    if (p.weather == WX_STORM && chance(8)) lightning(p);
    const Loc& L = LOCS[p.loc];
    int enc = 62 + (p.night() ? 10 : 0) + (p.weather == WX_FOG ? 10 : 0);
    if (chance(enc)) fight(p, pickOne(L.enemies));
    else randomEvent(p);
}

void gatherResources(Player& p) {
    passTime(p, 1);
    cout << "\n";
    rule("Сбор ресурсов — " + timeStr(p));
    const Loc& L = LOCS[p.loc];
    if (chance(18)) {
        warnMsg("Пока вы искали, на вас напали!");
        fight(p, pickOne(L.enemies));
        return;
    }
    vector<pair<int, int>> g = L.gather;
    int total = 0;
    for (auto& x : g) {
        if (x.first == I_MOONFLOWER && p.night()) x.second *= 3;
        total += x.second;
    }
    if (g.empty() || chance(15)) { note("Вы долго искали, но ничего не нашли."); return; }
    int r = rnd(1, total), id = g[0].first;
    for (auto& x : g) { if (r <= x.second) { id = x.first; break; } r -= x.second; }
    int n = rnd(1, 2) + (chance(25) ? 1 : 0) + (p.weather == WX_RAIN && id == I_HERB ? 1 : 0);
    p.add(id, n);
    gain("Найдено: " + itemName(id) + " x" + to_string(n));
}

void camp(Player& p) {
    cout << "\n";
    rule("Лагерь");
    passTime(p, 6);
    if (chance(25 + (p.night() ? 10 : 0))) {
        warnMsg("Ночёвку прервало нападение!");
        if (!fight(p, pickOne(LOCS[p.loc].enemies))) return;
    }
    p.hp = min(p.maxHp(), p.hp + p.maxHp() * 4 / 10);
    p.mp = min(p.maxMp(), p.mp + p.maxMp() * 4 / 10);
    good("Вы немного восстановили силы. " + timeStr(p));
}

void travel(Player& p) {
    const Loc& L = LOCS[p.loc];
    vector<string> v;
    for (size_t i = 0; i < L.nb.size(); i++) {
        const Loc& d = LOCS[L.nb[i]];
        string lvl = "ур. " + to_string(d.minLvl);
        v.push_back(key((int)i + 1, d.name) + "  " + (p.level + 2 < d.minLvl ? red(lvl + " ⚠") : gray(lvl)));
    }
    v.push_back(key(0, "Остаться"));
    cout << "\n";
    panel("Куда отправиться?", v, Col::CYAN);
    int c = askInt(0, (int)L.nb.size());
    if (c == 0) return;
    int dest = L.nb[c - 1];
    if (p.level + 2 < LOCS[dest].minLvl) {
        if (!askYesNo(orange("Место очень опасно для вашего уровня.") + " Всё равно идти?")) return;
    }
    p.loc = dest;
    passTime(p, 2);
    cout << "\n";
    good("Вы прибыли: " + bold(LOCS[dest].name) + gray(" — " + LOCS[dest].desc));
    if (p.weather == WX_STORM && !LOCS[dest].town && chance(8)) lightning(p);
    if (!LOCS[dest].town && chance(25 + (p.night() ? 10 : 0))) {
        warnMsg("На дороге вас подстерегала засада!");
        fight(p, pickOne(LOCS[dest].enemies));
    }
}

void bossFight(Player& p, int ei) {
    const EnemyT& t = ENEMIES[ei];
    string txt = ei == E_DRAGON
        ? "Огромный дракон поднимает голову. Чешуя раскалена, воздух дрожит. Перед дыханием огня он набирает воздух — "
          "успейте уклониться. Это самый опасный бой в игре."
        : "Король троллей Грок восседает на костях. Каждый четвёртый ход он топает так, что земля дрожит и можно "
          "потерять равновесие. Он быстро регенерирует.";
    cout << "\n";
    panelText(t.name, txt, Col::MAGENTA);
    if (!askYesNo("Начать бой? (Отступить можно только до его начала.)")) return;
    fight(p, ei);
}

// ============================================================================
//  БЕЗДНА: процедурно генерируемое подземелье
//  Каждый этаж — 4 комнаты (на выбор две двери со смутной подсказкой) и страж.
//  Враги и награды растут с глубиной; каждые 3 этажа — контрольная точка.
// ============================================================================
enum Door { D_FIGHT, D_ELITE, D_CHEST, D_REST, D_EVENT, D_MERCH };
const char* DOOR_HINT[] = {
    "Из темноты доносится рычание",
    "Воздух дрожит от тяжёлой ауры",
    "В щели между камнями блестит золото",
    "Тепло и тихо, пахнет костром",
    "Слышен странный шёпот",
    "Звенят монеты и посуда",
};
int pickDoor() {
    int r = rnd(1, 100);
    return r <= 38 ? D_FIGHT : r <= 50 ? D_ELITE : r <= 66 ? D_CHEST : r <= 76 ? D_REST : r <= 90 ? D_EVENT : D_MERCH;
}

Foe makeDepthFoe(const Player& p, int floor, bool elite, bool guardian) {
    vector<int> el;
    for (int i = 0; i < (int)ENEMIES.size(); i++) {
        if (ENEMIES[i].boss) continue;
        int t = enemyTier(i);
        if (t >= 1 && t <= p.level + floor / 2 + 1) el.push_back(i);
    }
    if (el.empty()) el.push_back(E_RAT);
    int ei;
    if (guardian) {
        ei = el[0];
        for (int e : el) if (ENEMIES[e].hp > ENEMIES[ei].hp) ei = e;
    } else {
        int a = pickOne(el), b = pickOne(el);
        ei = ENEMIES[a].hp >= ENEMIES[b].hp ? a : b;
    }
    Foe f = makeFoe(ei);
    double hs = 1.0 + 0.12 * (floor - 1), as = 1.0 + 0.07 * (floor - 1), rs = 1.0 + 0.2 * (floor - 1);
    if (guardian) { hs *= 1.6; as *= 1.2; }
    f.maxhp = f.hp = max(1, (int)(f.hp * hs));
    f.atk = max(1, (int)(f.atk * as));
    f.def += floor / 2;
    f.xp = (int)(f.xp * rs);
    f.gold = (int)(f.gold * rs);
    f.lootBonus = floor * 2;
    if (guardian) {
        f.name = "Страж: " + f.name;
        if (!f.elite) applyElite(f);
        f.guaranteedGear = true;
        if (f.special == 0) f.special = 3;
    } else if (elite && !f.elite) {
        applyElite(f);
        f.guaranteedGear = chance(60);
    }
    return f;
}

// true — игрок жив
bool depthFight(Player& p, int floor, bool elite, bool guardian) {
    return fightFoe(p, makeDepthFoe(p, floor, elite, guardian));
}

bool depthChest(Player& p, int floor) {
    say("В нише стоит окованный сундук, покрытый пылью веков.");
    if (!askYesNo("Открыть?")) { note("Вы проходите мимо."); return true; }
    int r = rnd(1, 100);
    if (r <= 15) {
        warnMsg("Сундук оживает — это " + bold("МИМИК") + "!");
        Foe f = makeFoe(E_MIMIC);
        f.maxhp = f.hp = 90 + p.level * 14 + floor * 25;
        f.atk = 14 + p.level * 2 + floor * 2;
        f.lootBonus = floor * 2;
        f.guaranteedGear = true;
        return fightFoe(p, f);
    }
    if (r <= 30) {
        int d = rnd(5, 10) + p.level * 2 + floor * 2;
        p.hp = max(1, p.hp - d);
        bad("Взрывная руна! " + red("-" + to_string(d) + " HP"));
        return true;
    }
    int g = rnd(20, 45) * floor + p.level * 5;
    p.gold += g;
    gain("В сундуке лежит " + yellow(to_string(g)) + " золота!");
    if (chance(40)) receiveGear(p, randomGear(p.level, floor * 2), nullptr);
    if (chance(30)) {
        int it = pickOne(vector<int>{I_HP_L, I_MP_L, I_BOMB, I_ICEBOMB, I_ACID, I_HOLYWATER, I_CRYSTAL});
        p.add(it);
        gain("А ещё: " + itemName(it));
    }
    return true;
}

void depthRest(Player& p) {
    int h = p.maxHp() * 35 / 100, m = p.maxMp() * 25 / 100;
    p.hp = min(p.maxHp(), p.hp + h);
    p.mp = min(p.maxMp(), p.mp + m);
    good("Вы немного отдыхаете у чьего-то остывающего костра. " + green("+" + to_string(h) + " HP") + ", " + blue("+" + to_string(m) + " MP"));
}

void depthMerchant(Player& p, int floor) {
    int gid = randomGear(p.level, 6 + floor * 2, 1);
    int price = ITEMS[gid].price;
    int packPrice = 60 + floor * 10;
    vector<string> v;
    v.push_back(gray("Закутанная фигура раскладывает товар на плаще."));
    v.push_back(key(1, itemName(gid)) + "  " + (p.gold >= price ? yellow(to_string(price)) : gray(to_string(price))) + " зол.");
    v.push_back("    " + gray(statsStr(ITEMS[gid])));
    v.push_back(key(2, "Мешок припасов (2 больших зелья, 1 бомба)") );
    v.push_back("    " + (p.gold >= packPrice ? yellow(to_string(packPrice)) : gray(to_string(packPrice))) + " зол.");
    v.push_back(key(0, "Уйти"));
    cout << "\n";
    panel("Тёмный торговец", v, Col::YELLOW);
    int c = askInt(0, 2);
    if (c == 1) {
        if (p.gold < price) { bad("Не хватает золота."); return; }
        p.gold -= price;
        receiveGear(p, gid, nullptr);
    } else if (c == 2) {
        if (p.gold < packPrice) { bad("Не хватает золота."); return; }
        p.gold -= packPrice;
        p.add(I_HP_L, 2); p.add(I_BOMB, 1);
        good("Вы получили 2 больших зелья лечения и бомбу.");
    }
}

void drawDepthHud(const Player& p, int floor, int room, int rooms) {
    vector<string> v;
    v.push_back(lr(bold("Этаж " + to_string(floor)) + gray(room > rooms ? "   зал стража" : "   комната " + to_string(room) + "/" + to_string(rooms)),
                   gray("пройдено: " + to_string(p.bestFloor)) + "  " + yellow("● " + to_string(p.gold))));
    v.push_back("HP " + bar(p.hp, p.maxHp(), 30, hpColor(p.hp, p.maxHp())) + " " + to_string(p.hp) + "/" + to_string(p.maxHp()));
    v.push_back("MP " + bar(p.mp, p.maxMp(), 30, Col::BLUE) + " " + to_string(p.mp) + "/" + to_string(p.maxMp()));
    cout << "\n";
    panel("Бездна", v, Col::MAGENTA);
}

void abyss(Player& p) {
    cout << "\n";
    panelText("Бездна",
              "Древний провал в руинах уходит в самую глубь земли. Каждый этаж — четыре комнаты и страж. "
              "Враги и награды растут с глубиной. Выход наверх доступен между комнатами, но смерть "
              "обходится дорого. Каждые 3 пройденных этажа открывают контрольную точку.", Col::MAGENTA);
    if (p.level < 5 && !askYesNo(orange("Рекомендуемый уровень — 5+.") + " Всё равно спуститься?")) return;
    int floor = 1;
    int cp = (p.bestFloor / 3) * 3 + 1;
    if (cp > 1) {
        grid({key(1, "С 1 этажа"), key(2, "С этажа " + to_string(cp) + " (контрольная точка)"), key(0, "Отмена")}, 1);
        int c = askInt(0, 2);
        if (c == 0) return;
        if (c == 2) floor = cp;
    }
    const int ROOMS = 4;
    int startLoc = p.loc;
    bool leave = false;
    while (!leave) {
        for (int room = 1; room <= ROOMS && !leave; room++) {
            int a = pickDoor(), b = pickDoor();
            for (int t = 0; t < 6 && b == a; t++) b = pickDoor();
            int c;
            while (true) {
                drawDepthHud(p, floor, room, ROOMS);
                say(key(1, "Левая дверь: " + gray(DOOR_HINT[a])));
                say(key(2, "Правая дверь: " + gray(DOOR_HINT[b])));
                grid({key(3, "Инвентарь"), key(4, "Персонаж"), key(5, "Подняться")}, 3);
                c = askInt(1, 5);
                if (c == 3) { inventoryMenu(p); continue; }
                if (c == 4) { characterMenu(p); continue; }
                if (c == 5 && !askYesNo("Подняться наверх? Прогресс этажа будет потерян.")) continue;
                break;
            }
            if (c == 5) { leave = true; break; }
            int door = c == 1 ? a : b;
            passTime(p, 1);
            cout << "\n";
            rule("Этаж " + to_string(floor) + " — " + DOOR_HINT[door]);
            bool alive = true;
            switch (door) {
                case D_FIGHT: alive = depthFight(p, floor, false, false); break;
                case D_ELITE: warnMsg("Это логово сильного противника!"); alive = depthFight(p, floor, true, false); break;
                case D_CHEST: alive = depthChest(p, floor); break;
                case D_REST: depthRest(p); break;
                case D_EVENT: randomEvent(p); break;
                default: depthMerchant(p, floor); break;
            }
            p.clampVitals();
            if (!alive || p.loc != startLoc) return;  // погибли — спасение уже обработано
        }
        if (leave) break;
        // страж этажа
        drawDepthHud(p, floor, ROOMS + 1, ROOMS);
        banner("СТРАЖ ЭТАЖА " + to_string(floor), Col::MAGENTA);
        if (!depthFight(p, floor, true, true) || p.loc != startLoc) return;
        p.bestFloor = max(p.bestFloor, floor);
        int g = floor * 50 + p.level * 10, x = floor * 30;
        p.gold += g; p.xp += x; p.rep += 1;
        int h = p.maxHp() * 30 / 100;
        p.hp = min(p.maxHp(), p.hp + h);
        panel("Этаж " + to_string(floor) + " пройден!",
              {"Награда: " + yellow(to_string(g)) + " зол., " + yellow(to_string(x)) + " опыта",
               green("Короткий отдых: +" + to_string(h) + " HP")}, Col::GREEN);
        if (floor % 3 == 0) gain("Открыта контрольная точка: следующий спуск можно начать с этажа " + to_string(floor + 1) + ".");
        levelUp(p);
        checkAch(p);
        grid({key(1, "Спуститься на этаж " + to_string(floor + 1)), key(2, "Подняться наверх")}, 1);
        if (askInt(1, 2) == 2) leave = true;
        else floor++;
    }
    passTime(p, 2);
    good("Вы выбираетесь из Бездны на поверхность.");
}

// ============================================================================
//  Сохранение / загрузка (3 слота + автосохранение)
// ============================================================================
string slotFile(int s) { return s == 0 ? "autosave.sav" : "slot" + to_string(s) + ".sav"; }
int toIntSafe(const string& s, int def) {
    try { return stoi(s); } catch (...) { return def; }
}

bool saveGame(const Player& p, int slot) {
    ofstream f(slotFile(slot));
    if (!f) return false;
    f << "RPGSAVE 3\n";
    f << "name " << p.name << "\n";
    auto w = [&](const char* k, long long v) { f << k << " " << v << "\n"; };
    w("cls", p.cls); w("level", p.level); w("xp", p.xp); w("gold", p.gold);
    w("hp", p.hp); w("mp", p.mp); w("baseHp", p.baseHp); w("baseMp", p.baseMp);
    w("str", p.str); w("def", p.def); w("intl", p.intl); w("agi", p.agi);
    w("eq0", p.eq[0]); w("eq1", p.eq[1]); w("eq2", p.eq[2]); w("sharp", p.sharp);
    w("loc", p.loc); w("tick", p.tick); w("kills", p.kills);
    w("dragon", p.dragonSlain); w("king", p.trollKingSlain);
    w("points", p.points); w("rep", p.rep); w("weather", p.weather); w("wperiod", p.wperiod);
    w("payDay", p.payDay); w("comp", p.comp); w("dealDay", p.dealDay); w("dealMask", p.dealMask);
    w("bntDay", p.bntDay); w("bntEnemy", p.bntEnemy); w("bntNeed", p.bntNeed); w("bntProg", p.bntProg);
    w("bntState", p.bntState); w("bntGold", p.bntGold); w("bntXp", p.bntXp);
    w("bless", p.blessFights); w("nextBuff", p.nextBuff); w("ach", p.ach);
    w("elite", p.eliteKills); w("dice", p.diceWins); w("arena", p.arenaWins);
    w("maxHit", p.maxHit); w("bestFloor", p.bestFloor);
    vector<int> gens;  // сгенерированная экипировка, на которую ссылается игрок
    for (int i = 0; i < 3; i++) if (p.eq[i] >= STATIC_ITEMS) gens.push_back(p.eq[i]);
    for (auto& kv : p.inv) if (kv.first >= STATIC_ITEMS) gens.push_back(kv.first);
    sort(gens.begin(), gens.end());
    gens.erase(unique(gens.begin(), gens.end()), gens.end());
    for (int id : gens) {
        const Item& g = ITEMS[id];
        f << "gen" << id << " " << g.base << " " << g.rarity << " " << g.atk << " " << g.def << " " << g.hp << " "
          << g.mp << " " << g.crit << " " << g.steal << " " << g.dodge << " " << g.elem << " " << g.price << "\n";
    }
    f << "inv";
    for (auto& kv : p.inv) f << " " << kv.first << ":" << kv.second;
    f << "\nquest";
    for (auto& kv : p.quest) f << " " << kv.first << ":" << kv.second << ":" << (p.qprog.count(kv.first) ? p.qprog.at(kv.first) : 0);
    f << "\nbeast";
    for (int b : p.beast) f << " " << b;
    f << "\n";
    return (bool)f;
}

bool readSave(const string& file, map<string, string>& m) {
    ifstream f(file);
    if (!f) return false;
    string l;
    if (!getline(f, l) || l.rfind("RPGSAVE 3", 0) != 0) return false;
    while (getline(f, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        size_t sp = l.find(' ');
        if (sp == string::npos) m[l] = "";
        else m[l.substr(0, sp)] = l.substr(sp + 1);
    }
    return true;
}

bool loadGame(Player& p, int slot) {
    map<string, string> m;
    if (!readSave(slotFile(slot), m)) return false;
    auto gi = [&](const string& k, int def) {
        auto it = m.find(k);
        return it == m.end() ? def : toIntSafe(it->second, def);
    };
    // пересоздаём сгенерированную экипировку и переназначаем её номера
    ITEMS.erase(ITEMS.begin() + STATIC_ITEMS, ITEMS.end());
    GEN_KEYS.clear();
    map<int, int> remap;
    for (auto& kv : m) {
        if (kv.first.rfind("gen", 0) != 0 || kv.first.size() < 4) continue;
        int oldId = toIntSafe(kv.first.substr(3), -1);
        istringstream is(kv.second);
        int v[11];
        bool okv = oldId >= STATIC_ITEMS;
        for (int i = 0; i < 11 && okv; i++) if (!(is >> v[i])) okv = false;
        if (!okv) continue;
        int base = v[0], rar = v[1];
        if (base <= 0 || base >= STATIC_ITEMS || !isEquip(ITEMS[base].type) || rar < 1 || rar > 3) continue;
        int elem = max(0, min(5, v[9]));
        remap[oldId] = addGenItem(buildGen(base, rar, max(0, v[2]), max(0, v[3]), max(0, v[4]), max(0, v[5]),
                                           max(0, v[6]), max(0, v[7]), max(0, v[8]), elem, max(1, v[10])));
    }
    auto mapId = [&](int id) {
        if (id < STATIC_ITEMS) return id;
        auto it = remap.find(id);
        return it == remap.end() ? 0 : it->second;
    };
    Player q;
    q.name = m.count("name") && !m["name"].empty() ? m["name"] : "Герой";
    q.cls = gi("cls", 0);
    if (q.cls < 0 || q.cls >= (int)CLASSES.size()) return false;
    q.level = max(1, min(99, gi("level", 1)));
    q.xp = max(0, gi("xp", 0)); q.gold = max(0, gi("gold", 0));
    q.baseHp = max(1, gi("baseHp", 1)); q.baseMp = max(0, gi("baseMp", 0));
    q.str = max(0, gi("str", 1)); q.def = max(0, gi("def", 1));
    q.intl = max(0, gi("intl", 1)); q.agi = max(0, gi("agi", 1));
    for (int i = 0; i < 3; i++) {
        int id = mapId(gi("eq" + to_string(i), 0));
        bool ok = id == 0 || (id > 0 && id < (int)ITEMS.size() &&
                              ITEMS[id].type == (i == 0 ? WEAPON : i == 1 ? ARMOR : ACCESSORY));
        q.eq[i] = ok ? id : 0;
    }
    q.sharp = max(0, min(5, gi("sharp", 0)));
    q.loc = gi("loc", 0);
    if (q.loc < 0 || q.loc >= (int)LOCS.size()) q.loc = 0;
    q.tick = max(0, gi("tick", 8)); q.kills = max(0, gi("kills", 0));
    q.dragonSlain = gi("dragon", 0) != 0; q.trollKingSlain = gi("king", 0) != 0;
    q.points = max(0, gi("points", 0)); q.rep = max(0, gi("rep", 0));
    q.weather = max(0, min(3, gi("weather", 0))); q.wperiod = gi("wperiod", q.tick / 8);
    q.payDay = gi("payDay", q.tick / 24 + 1);
    q.comp = gi("comp", -1);
    if (q.comp < -1 || q.comp >= (int)COMPS.size()) q.comp = -1;
    q.dealDay = gi("dealDay", 0); q.dealMask = gi("dealMask", 0);
    q.bntDay = gi("bntDay", 0); q.bntEnemy = gi("bntEnemy", 0); q.bntNeed = max(1, gi("bntNeed", 1));
    q.bntProg = max(0, gi("bntProg", 0)); q.bntState = max(0, min(2, gi("bntState", 0)));
    q.bntGold = max(0, gi("bntGold", 0)); q.bntXp = max(0, gi("bntXp", 0));
    if (q.bntEnemy < 0 || q.bntEnemy >= (int)ENEMIES.size()) q.bntEnemy = 0;
    q.blessFights = max(0, gi("bless", 0)); q.nextBuff = max(0, gi("nextBuff", 0));
    q.ach = (unsigned)gi("ach", 0);
    q.maxHit = max(0, gi("maxHit", 0)); q.bestFloor = max(0, gi("bestFloor", 0));
    q.eliteKills = max(0, gi("elite", 0)); q.diceWins = max(0, gi("dice", 0)); q.arenaWins = max(0, gi("arena", 0));
    q.hp = gi("hp", 1); q.mp = gi("mp", 0);

    auto parseList = [&](const string& key, function<void(const string&)> fn) {
        auto it = m.find(key);
        if (it == m.end()) return;
        istringstream is(it->second);
        string tok;
        while (is >> tok) fn(tok);
    };
    parseList("inv", [&](const string& t) {
        size_t c = t.find(':');
        if (c == string::npos) return;
        int id = mapId(toIntSafe(t.substr(0, c), 0)), n = toIntSafe(t.substr(c + 1), 0);
        if (id > 0 && id < (int)ITEMS.size() && n > 0) q.inv[id] += n;
    });
    parseList("quest", [&](const string& t) {
        size_t c1 = t.find(':');
        size_t c2 = c1 == string::npos ? string::npos : t.find(':', c1 + 1);
        if (c2 == string::npos) return;
        int id = toIntSafe(t.substr(0, c1), -1), st = toIntSafe(t.substr(c1 + 1, c2 - c1 - 1), 0), pr = toIntSafe(t.substr(c2 + 1), 0);
        if (id >= 0 && id < (int)QUESTS.size()) { q.quest[id] = max(0, min(2, st)); q.qprog[id] = max(0, pr); }
    });
    q.ensureBeast();
    {
        size_t i = 0;
        parseList("beast", [&](const string& t) { if (i < q.beast.size()) q.beast[i++] = max(0, toIntSafe(t, 0)); });
    }
    q.clampVitals();
    if (q.hp <= 0) q.hp = 1;
    p = q;
    return true;
}

string slotInfo(int slot) {
    map<string, string> m;
    if (!readSave(slotFile(slot), m)) return "";
    int cls = m.count("cls") ? toIntSafe(m["cls"], 0) : 0;
    if (cls < 0 || cls >= (int)CLASSES.size()) cls = 0;
    int tick = m.count("tick") ? toIntSafe(m["tick"], 8) : 8;
    return clip(m.count("name") ? m["name"] : string("?"), 14) + " — " + CLASSES[cls].name + ", ур. " +
           (m.count("level") ? m["level"] : string("1")) + ", день " + to_string(tick / 24 + 1);
}

int chooseSlot(bool forSave) {
    vector<string> v;
    for (int s = 1; s <= 3; s++) {
        string inf = slotInfo(s);
        v.push_back(key(s, inf.empty() ? gray("пусто") : inf));
    }
    if (!forSave) {
        string inf = slotInfo(0);
        v.push_back(key(4, "Авто: " + (inf.empty() ? gray("пусто") : inf)));
    }
    v.push_back(key(0, "Отмена"));
    cout << "\n";
    panel(forSave ? "Сохранение" : "Загрузка", v, Col::CYAN);
    int c = askInt(0, forSave ? 3 : 4);
    if (c == 0) return -1;
    return c == 4 ? 0 : c;
}
void saveMenu(const Player& p) {
    int s = chooseSlot(true);
    if (s < 0) return;
    if (!slotInfo(s).empty() && !askYesNo("Слот занят. Перезаписать?")) return;
    if (saveGame(p, s)) good("Игра сохранена в слот " + to_string(s) + ".");
    else bad("Не удалось записать файл сохранения.");
}

// ============================================================================
//  Создание персонажа, справка, главный цикл
// ============================================================================
void showHelp() {
    cout << "\n";
    panel("Справка", {
        bold("Бой: ") + "атака, навыки (тратят ману), предметы, бегство.",
        gray("  Боссы предупреждают о мощной атаке за ход до неё — используйте уклонение"),
        gray("  или лечитесь. Элитные враги (★) опаснее, но дают больше добычи."),
        bold("Время и погода: ") + "ночью враги сильнее, но опыта больше. Погода",
        gray("  влияет на бегство, уклонение и силу магии."),
        bold("Ресурсы: ") + "«Собирать ресурсы» в дикой местности даёт травы, руду и",
        gray("  кристаллы для кузницы и алхимика."),
        bold("Репутация: ") + "растёт за квесты и добрые дела, даёт скидки в лавке.",
        bold("Очки: ") + "за каждый уровень — 3 свободных очка (Персонаж → распределить).",
        bold("Арена: ") + "3 раунда; можно забрать банк между ними, но смерть сжигает его.",
        bold("Стихии: ") + "у врагов есть слабость (x1.5) и сопротивление (x0.6). Огонь,",
        gray("  лёд, яд, свет, молния — у навыков, бомб и редкого оружия. Огонь поджигает."),
        bold("Приём: ") + "шкала заряжается от ударов и урона; при 100% — особый приём класса.",
        bold("Редкая экипировка: ") + "редкая, эпическая, легендарная — со случайными свойствами",
        gray("  (крит, вампиризм, уклонение, стихия). Выпадает из элиты, боссов и сундуков."),
        bold("Бездна: ") + "подземелье в руинах: 4 комнаты и страж на этаж, глубже — лучше добыча.",
        bold("Сохранение: ") + "3 слота и автосохранение при ночлеге в таверне.",
    }, Col::CYAN);
}

void newCharacter(Player& p) {
    cout << "\n";
    say("Как вас зовут, герой?");
    cout << "  " << paint("▶ ", Col::CYAN);
    cout.flush();
    string n;
    if (!getline(cin, n)) exit(0);
    string clean;
    for (char ch : n) if ((unsigned char)ch >= 32) clean += ch;
    while (vlen(clean) > 24) {  // обрезаем по символам UTF-8, а не по байтам
        clean.pop_back();
        while (!clean.empty() && ((unsigned char)clean.back() & 0xC0) == 0x80) clean.pop_back();
        if (!clean.empty() && ((unsigned char)clean.back() & 0xC0) == 0xC0) clean.pop_back();
    }
    p = Player();
    p.name = clean.empty() ? "Герой" : clean;

    vector<string> v;
    for (size_t i = 0; i < CLASSES.size(); i++) {
        v.push_back(key((int)i + 1, bold(CLASSES[i].name) + gray(" — " + CLASSES[i].desc)));
        v.push_back("    " + gray(CLASSES[i].passive));
    }
    cout << "\n";
    panel("Выберите класс", v, Col::YELLOW);
    p.cls = askInt(1, (int)CLASSES.size()) - 1;
    auto& c = CLASSES[p.cls];
    p.baseHp = c.hp; p.baseMp = c.mp; p.str = c.str; p.def = c.def; p.intl = c.intl; p.agi = c.agi;
    if (p.cls == 0) { p.eq[0] = I_RUSTY; p.eq[1] = I_LEATHER; p.add(I_HP_S, 2); }
    else if (p.cls == 1) { p.eq[0] = I_STAFF1; p.eq[1] = I_ROBE; p.add(I_MP_S, 2); p.add(I_HP_S, 1); }
    else { p.eq[0] = I_DAGGER; p.eq[1] = I_LEATHER; p.add(I_HP_S, 2); }
    p.hp = p.maxHp(); p.mp = p.maxMp();
    p.weather = rollWeather(); p.wperiod = p.tick / 8;
    p.ensureBeast();

    cout << "\n";
    panelText("Пролог",
              "Королевство трепещет: огненный дракон Игнатор поселился в горах и сжигает деревни. Вы, " + p.name +
              ", начинаете путь в деревне Рассвет. Берите задания, исследуйте мир, копите силы — и бросьте вызов дракону!",
              Col::MAGENTA);
    waitEnter();
}

void gameLoop(Player& p) {
    bool running = true;
    while (running) {
        p.ensureBeast();
        ensureBounty(p);
        checkAch(p);
        drawHud(p);
        const Loc& L = LOCS[p.loc];

        vector<pair<string, function<void()>>> menu;
        if (L.town) {
            menu.push_back({"Лавка торговца", [&] { shopMenu(p); }});
            menu.push_back({"Кузница", [&] { smithMenu(p); }});
            menu.push_back({"Алхимик", [&] { craftMenu(p, "Алхимическая лавка", ALCH, true); }});
            menu.push_back({"Таверна", [&] { tavernMenu(p); }});
            menu.push_back({"Храм", [&] { templeMenu(p); }});
            menu.push_back({"Арена", [&] { arenaMenu(p); }});
            menu.push_back({"Доска заданий", [&] { questBoard(p); }});
        } else {
            menu.push_back({"Исследовать местность", [&] { explore(p); }});
            menu.push_back({"Собирать ресурсы", [&] { gatherResources(p); }});
            menu.push_back({"Разбить лагерь", [&] { camp(p); }});
            if (p.loc == L_DUNGEON && !p.trollKingSlain)
                menu.push_back({magenta("Бросить вызов Гроку"), [&] { bossFight(p, E_TROLLKING); }});
            if (p.loc == L_RUINS) menu.push_back({magenta("Спуститься в Бездну"), [&] { abyss(p); }});
            if (p.loc == L_LAIR && !p.dragonSlain)
                menu.push_back({magenta("Сразиться с Игнатором"), [&] { bossFight(p, E_DRAGON); }});
        }
        menu.push_back({"Отправиться в путь", [&] { travel(p); }});
        menu.push_back({"Инвентарь", [&] { inventoryMenu(p); }});
        menu.push_back({"Персонаж", [&] { characterMenu(p); }});
        menu.push_back({"Журнал", [&] { journalMenu(p); }});
        menu.push_back({"Сохранить игру", [&] { saveMenu(p); }});
        menu.push_back({"Выйти из игры", [&] {
            if (askYesNo("Выйти? Несохранённый прогресс будет потерян.")) running = false;
        }});

        vector<string> cells;
        for (size_t i = 0; i < menu.size(); i++) cells.push_back(key((int)i + 1, menu[i].first));
        grid(cells, 2);
        int c = askInt(1, (int)menu.size());
        menu[c - 1].second();
    }
}

void initTerminal(int argc, char** argv) {
    bool force = false;
    useColor = true;
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        if (a == "--nocolor") useColor = false;
        if (a == "--color") force = true;
    }
    if (getenv("NO_COLOR") && !force) useColor = false;
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode)) {
        if (!SetConsoleMode(h, mode | 0x0004)) useColor = false;
    } else if (!force) useColor = false;
#else
    if (!isatty(fileno(stdout)) && !force) useColor = false;
#endif
    if (const char* seed = getenv("RPG_SEED")) rng.seed((unsigned)atoi(seed));
}

int main(int argc, char** argv) {
    initTerminal(argc, argv);
    initItems(); initSkills(); initEnemies(); initLocs(); initQuests();

    Player p;
    cout << "\n";
    banner("ХРОНИКИ ИГНАТОРА II", Col::YELLOW);
    cout << center(gray("текстовая RPG"), W + 2) << "\n";
    while (true) {
        cout << "\n";
        grid({key(1, "Новая игра"), key(2, "Загрузить игру"), key(3, "Справка"), key(0, "Выход")}, 2);
        int c = askInt(0, 3);
        if (c == 0) return 0;
        if (c == 3) { showHelp(); continue; }
        if (c == 1) { newCharacter(p); break; }
        int s = chooseSlot(false);
        if (s < 0) continue;
        if (loadGame(p, s)) { good("Игра загружена. С возвращением, " + bold(p.name) + "!"); break; }
        bad("Сохранение не найдено или повреждено.");
    }
    gameLoop(p);
    cout << "\n" << center(gray("До новых встреч, герой!"), W + 2) << "\n";
    return 0;
}
