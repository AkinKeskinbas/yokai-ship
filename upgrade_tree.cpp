#include "upgrade_tree.h"
#include "game.h"
#include "sprite.h"
#include "texture.h"
#include "input_mouse.h"
#include "input_keyboard.h"
#include "configuration.h"
#include "audio.h"
#include <cmath>
#include <algorithm>
#include <cstdio>

static constexpr float PI = 3.14159265f;

// 3x5 Dot Matrix font mapping for UI rendering
static uint16_t GetMatrixCharMask(char c)
{
    c = static_cast<char>(toupper(c));
    switch (c)
    {
        case '0': return 0b111101101101111;
        case '1': return 0b010010010010010;
        case '2': return 0b111001111100111;
        case '3': return 0b111001111001111;
        case '4': return 0b101101111001001;
        case '5': return 0b111100111001111;
        case '6': return 0b111100111101111;
        case '7': return 0b111001001001001;
        case '8': return 0b111101111101111;
        case '9': return 0b111101111001111;
        
        case 'A': return 0b111101111101101;
        case 'B': return 0b110101110101110;
        case 'C': return 0b111100100100111;
        case 'D': return 0b110101101101110;
        case 'E': return 0b111100111100111;
        case 'F': return 0b111100111100100;
        case 'G': return 0b111100101101111;
        case 'H': return 0b101101111101101;
        case 'I': return 0b111010010010111;
        case 'J': return 0b001001001101110;
        case 'K': return 0b101110110101101;
        case 'L': return 0b100100100100111;
        case 'M': return 0b101111101101101;
        case 'N': return 0b111101101101101;
        case 'O': return 0b111101101101111;
        case 'P': return 0b111101111100100;
        case 'Q': return 0b111101101111011;
        case 'R': return 0b111101111101101;
        case 'S': return 0b111100111001111;
        case 'T': return 0b111010010010010;
        case 'U': return 0b101101101101111;
        case 'V': return 0b101101101101010;
        case 'W': return 0b101101101111101;
        case 'X': return 0b101101010101101;
        case 'Y': return 0b101101010010010;
        case 'Z': return 0b111001010100111;

        case '-': return 0b000000111000000;
        case ':': return 0b000010000010000;
        case '\'': return 0b010010000000000;
        case '.': return 0b000000000000010;
        case '+': return 0b000010111010000;
        case '/': return 0b001010010010100;
        case '%': return 0b101010010010101;
        case '[': return 0b110100100100110;
        case ']': return 0b011001001001011;
        case '>': return 0b100010001010100;
        case '<': return 0b001010100010001;
        case '?': return 0b111001010000010;
        default:  return 0b000000000000000;
    }
}

static void DrawTextMatrix(float x, float y, const char* str, float size, const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f })
{
    float current_x = x;
    while (*str)
    {
        char c = *str;
        if (c == ' ')
        {
            current_x += 4.0f * size;
        }
        else
        {
            uint16_t mask = GetMatrixCharMask(c);
            for (int row = 0; row < 5; ++row)
            {
                for (int col = 0; col < 3; ++col)
                {
                    int bit_index = 14 - (row * 3 + col);
                    if ((mask >> bit_index) & 1)
                    {
                        float px = current_x + col * size;
                        float py = y + row * size;
                        Sprite_DrawRect(px, py, size, size, color);
                    }
                }
            }
            current_x += 4.0f * size;
        }
        str++;
    }
}

static void DrawTextMatrixWrapped(float x, float y, const char* str, float size, float maxWidth, const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f }, float lineSpacing = 16.0f)
{
    float charW = 4.0f * size;
    float curX = x;
    float curY = y;
    
    std::string text(str);
    size_t start = 0;
    
    while (start < text.length())
    {
        size_t end = text.find(' ', start);
        if (end == std::string::npos) end = text.length();
        
        std::string word = text.substr(start, end - start);
        float wordW = (float)word.length() * charW;
        
        if (curX + wordW > x + maxWidth && curX > x)
        {
            curX = x;
            curY += lineSpacing;
        }
        
        DrawTextMatrix(curX, curY, word.c_str(), size, color);
        curX += wordW + charW;
        
        start = end + 1;
    }
}

static void DrawFormattedNumber(float x, float y, int value, int digitCount, float spacing, int textureId, const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f })
{
    if (textureId == -1) return;
    int temp = value;
    float digitWidth = 22.0f;
    float digitHeight = 22.0f;

    for (int i = 0; i < digitCount; i++)
    {
        int divisor = static_cast<int>(pow(10, digitCount - 1 - i));
        int digitVal = (temp / divisor) % 10;
        int src_x = digitVal * 64;
        float drawX = x + i * spacing;
        Sprite_Draw(textureId, drawX, y, digitWidth, digitHeight, src_x, 0, 64, 64, color);
    }
}

UpgradeTree::UpgradeTree()
{
}

void UpgradeTree::Initialize(int texLaser, int texNumber, int texHeart, int texResources, int texSpaceship,
                            int texVida, int texDisli, int texCpu, int texKey, int soundShoot)
{
    m_texLaser = texLaser;
    m_texNumber = texNumber;
    m_texHeart = texHeart;
    m_texResources = texResources;
    m_texSpaceship = texSpaceship;
    m_texVida = texVida;
    m_texDisli = texDisli;
    m_texCpu = texCpu;
    m_texKey = texKey;
    m_soundShoot = soundShoot;

    SetupNodes();
    UpdateLayout();

    // Setup 5 Sectors
    m_sectors.clear();
    m_sectors.push_back({ 1, "SOGUK TARLALARI", false, true, false });
    m_sectors.push_back({ 2, "ASTEROID KUSAGI", false, false, false });
    m_sectors.push_back({ 3, "NEBULA GECIDI", false, false, false });
    m_sectors.push_back({ 4, "PLAZMA FIRTINASI", false, false, false });
    m_sectors.push_back({ 5, "AFET CEKIRDEGI (BOSS)", true, false, false });
    m_currentSectorIndex = 1;
}

void UpgradeTree::UnlockNextSector(int completedSector)
{
    for (int i = 0; i < (int)m_sectors.size(); ++i)
    {
        if (m_sectors[i].stageNumber == completedSector)
        {
            m_sectors[i].completed = true;
            if (i + 1 < (int)m_sectors.size())
            {
                m_sectors[i + 1].unlocked = true;
                m_currentSectorIndex = m_sectors[i + 1].stageNumber;
            }
        }
    }
}

void UpgradeTree::SetupNodes()
{
    m_nodes.clear();

    // 0: Center Motherboard Core Hub
    {
        UpgradeNode n;
        n.id = 0;
        n.title = "ANA KOMUTA MERKEZI";
        n.categoryName = "ANA MODUL";
        n.description = "Tum gemi alt sistemlerinin bagli oldugu merkezi kuantum cekirdegi.";
        n.effectFormat = "Merkezi Ag Aktif";
        n.branch = NodeBranch::Core;
        n.icon = NodeIconType::CoreHub;
        n.gridPos = { 0.0f, 0.0f };
        n.currentLevel = 1;
        n.maxLevel = 1;
        n.isCenterHub = true;
        m_nodes.push_back(n);
    }

    // =========================================================================
    // 🔴 NORTH BRANCH: WEAPON / LASER SYSTEM (1..13, 99)
    // =========================================================================
    // Sub-branch 1: POWER & PIERCING
    {
        UpgradeNode n;
        n.id = 1;
        n.title = "LAZER GUC ASIRIMI I";
        n.categoryName = "SILAH / GUC";
        n.description = "Lazer enerji rezonansini yukselterek temel DPS hasarini artirir.";
        n.effectFormat = "+15 Lazer Hasari";
        n.branch = NodeBranch::North_Weapon;
        n.icon = NodeIconType::LaserBeam;
        n.gridPos = { 0.0f, -1.0f };
        n.maxLevel = 3;
        n.levelCosts = { { 20, 0, 0, 0, 0 }, { 35, 2, 0, 0, 0 }, { 55, 4, 1, 0, 0 } };
        n.levelValues = { 15.0f, 30.0f, 50.0f };
        n.prerequisiteIds = { 0 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 2;
        n.title = "TERMAL DELICI II";
        n.categoryName = "SILAH / GUC";
        n.description = "Yuksek frekansli termal plazma ile hedef materyali hizla eritir.";
        n.effectFormat = "+30 Lazer Hasari";
        n.branch = NodeBranch::North_Weapon;
        n.icon = NodeIconType::Fire;
        n.gridPos = { 0.0f, -2.0f };
        n.maxLevel = 2;
        n.levelCosts = { { 65, 6, 2, 0, 0 }, { 110, 12, 4, 1, 0 } };
        n.levelValues = { 30.0f, 65.0f };
        n.prerequisiteIds = { 1 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 3;
        n.title = "DELICI ISIN (PIERCING BEAM)";
        n.categoryName = "SILAH / GUC";
        n.description = "Lazer ilk hedefi delip gecer ve arkadaki ikinci hedefe de hasar verir.";
        n.effectFormat = "Delici Isin: Cift Hedef Vurusu";
        n.branch = NodeBranch::North_Weapon;
        n.icon = NodeIconType::Sword;
        n.gridPos = { 0.0f, -3.0f };
        n.maxLevel = 1;
        n.levelCosts = { { 130, 15, 4, 1, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 2 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 4;
        n.title = "ASIRI ISINMA (OVERHEAT)";
        n.categoryName = "SILAH / GUC";
        n.description = "Ayni hedefe 1.2 sn kesintisiz vuruldugunda hasar 2 katina cikar ve kirmizi isina donusur.";
        n.effectFormat = "Kesintisiz Atista +100% Hasar";
        n.branch = NodeBranch::North_Weapon;
        n.icon = NodeIconType::Fire;
        n.gridPos = { 0.0f, -4.0f };
        n.maxLevel = 1;
        n.levelCosts = { { 180, 20, 6, 2, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 3 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 5;
        n.title = "CEKIRDEK ERIMESI (CORE MELTDOWN)";
        n.categoryName = "SILAH / GUC";
        n.description = "Yok edilen asteroit ve dusmanlar kucuk bir termal patlama yaratarak cevreye hasar verir.";
        n.effectFormat = "Imha Aninda Alan Patlamasi";
        n.branch = NodeBranch::North_Weapon;
        n.icon = NodeIconType::Explosion;
        n.gridPos = { 0.0f, -5.0f };
        n.maxLevel = 1;
        n.levelCosts = { { 240, 28, 8, 2, 1 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 4 };
        m_nodes.push_back(n);
    }

    // Sub-branch 2: MULTI-BEAM
    {
        UpgradeNode n;
        n.id = 6;
        n.title = "CIFT PLAZMA ISINI";
        n.categoryName = "SILAH / COKLU ISIN";
        n.description = "Gemi cevresindeki 2 hedefe eszamanli kilitlenen ikincil lazer ekler.";
        n.effectFormat = "+1 Ekstra Lazer Isini (Toplam 2)";
        n.branch = NodeBranch::North_Weapon;
        n.icon = NodeIconType::Crosshair;
        n.gridPos = { 1.1f, -1.6f };
        n.maxLevel = 1;
        n.levelCosts = { { 60, 6, 1, 0, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 1 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 7;
        n.title = "UCLU PLAZMA ISINI";
        n.categoryName = "SILAH / COKLU ISIN";
        n.description = "Ayni anda 3 ayri hedefe saldiri baslatabilen cok kanalli optik modulu.";
        n.effectFormat = "+1 Ekstra Lazer Isini (Toplam 3)";
        n.branch = NodeBranch::North_Weapon;
        n.icon = NodeIconType::Crosshair;
        n.gridPos = { 1.1f, -2.6f };
        n.maxLevel = 1;
        n.levelCosts = { { 150, 18, 5, 1, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 6 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 8;
        n.title = "ZINCIRLEME LAZER ARKI (CHAIN ARC)";
        n.categoryName = "SILAH / COKLU ISIN";
        n.description = "Vurulan bir hedeften en yakindaki ikinci kayaya veya dusmana elektrikli lazer seker.";
        n.effectFormat = "Sekmeli Lazer Elektrik Arki";
        n.branch = NodeBranch::North_Weapon;
        n.icon = NodeIconType::Lightning;
        n.gridPos = { 1.1f, -3.6f };
        n.maxLevel = 1;
        n.levelCosts = { { 190, 22, 6, 2, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 7 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 9;
        n.title = "PRIZMATIK BOLUNME (PRISMATIC SPLIT)";
        n.categoryName = "SILAH / COKLU ISIN";
        n.description = "Buyuk asteroit kirildiginda lazer kisa sureligine ikiye ayrilarak etrafa yayilir.";
        n.effectFormat = "Kaya Kirilinca Cift Lazer Salinimi";
        n.branch = NodeBranch::North_Weapon;
        n.icon = NodeIconType::Crosshair;
        n.gridPos = { 1.1f, -4.6f };
        n.maxLevel = 1;
        n.levelCosts = { { 230, 26, 8, 2, 1 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 8 };
        m_nodes.push_back(n);
    }

    // Sub-branch 3: CRIT & KRISTAL ODAK
    {
        UpgradeNode n;
        n.id = 10;
        n.title = "KRITIK ODAK I";
        n.categoryName = "SILAH / KRITIK";
        n.description = "Lazer darbelerinde kritik hasar vurma olasiligi kazandirir.";
        n.effectFormat = "+15% Kritik Sansi";
        n.branch = NodeBranch::North_Weapon;
        n.icon = NodeIconType::Sword;
        n.gridPos = { -1.1f, -1.6f };
        n.maxLevel = 2;
        n.levelCosts = { { 50, 5, 1, 0, 0 }, { 90, 10, 2, 0, 0 } };
        n.levelValues = { 0.15f, 0.30f };
        n.prerequisiteIds = { 1 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 11;
        n.title = "KRITIK HASAR CARPANI II";
        n.categoryName = "SILAH / KRITIK";
        n.description = "Kritik vuruslarin hasarini normalin 3 katina cikarir.";
        n.effectFormat = "+100% Kritik Hasar Carpani";
        n.branch = NodeBranch::North_Weapon;
        n.icon = NodeIconType::Sword;
        n.gridPos = { -1.1f, -2.6f };
        n.maxLevel = 1;
        n.levelCosts = { { 120, 14, 4, 1, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 10 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 12;
        n.title = "KRISTAL ZAYIF NOKTA TESPITI";
        n.categoryName = "SILAH / KRITIK";
        n.description = "Asteroitlerde parildayan zayif noktalar olusur; oraya vuruldugunda dev kritik hasar patlar.";
        n.effectFormat = "Zayif Noktada %220 Kritik Patlamasi";
        n.branch = NodeBranch::North_Weapon;
        n.icon = NodeIconType::Crystal;
        n.gridPos = { -1.1f, -3.6f };
        n.maxLevel = 1;
        n.levelCosts = { { 200, 24, 7, 2, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 11 };
        m_nodes.push_back(n);
    }

    // 🔒 LOST TECHNOLOGY 1 (NORTH)
    {
        UpgradeNode n;
        n.id = 13;
        n.title = "KADIM TEKILLIK LAZERI";
        n.categoryName = "KADIM TEKNOLOJI";
        n.description = "Karanlik madde ile guclendirilmis, kalkan ve zırh tanimayan kadim lazer.";
        n.effectFormat = "+80 Lazer Hasari & Delici Isin";
        n.branch = NodeBranch::North_Weapon;
        n.icon = NodeIconType::Lock;
        n.gridPos = { -1.1f, -4.8f };
        n.maxLevel = 2;
        n.isSealed = true;
        n.keySealCost = 1;
        n.levelCosts = { { 200, 25, 8, 2, 0 }, { 350, 40, 15, 4, 0 } };
        n.levelValues = { 40.0f, 80.0f };
        n.prerequisiteIds = { 4 };
        m_nodes.push_back(n);
    }

    // ⭐ CAPSTONE 1 (NORTH)
    {
        UpgradeNode n;
        n.id = 99;
        n.title = "DEATH STAR PROTOCOL";
        n.categoryName = "CAPSTONE / NIHAI";
        n.description = "Tum lazerler devasa plazma sutununa donusur. Delici isin, asiri isinma ve +100 hasar birlestirilir.";
        n.effectFormat = "+100 Lazer Hasari & Sinirsiz Delici Kırıcı";
        n.branch = NodeBranch::North_Weapon;
        n.icon = NodeIconType::Star;
        n.gridPos = { 0.0f, -6.1f };
        n.maxLevel = 1;
        n.isCapstone = true;
        n.levelCosts = { { 500, 60, 20, 5, 1 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 5, 9, 12 };
        m_nodes.push_back(n);
    }

    // =========================================================================
    // 🟢 EAST BRANCH: MINING, SENSORS & PROSPECTING (14..27, 98)
    // =========================================================================
    // Sub-branch 1: PROSPECTING & SENSORS
    {
        UpgradeNode n;
        n.id = 14;
        n.title = "CEVHER RADARI I";
        n.categoryName = "MADENCILIK / SENSOR";
        n.description = "Cevredeki asteroitlerin maden yogunlugunu ve Reishi verimini artirir.";
        n.effectFormat = "+25% Reishi Kazanimi";
        n.branch = NodeBranch::East_Sensors;
        n.icon = NodeIconType::Scanner;
        n.gridPos = { 1.0f, 0.0f };
        n.maxLevel = 3;
        n.levelCosts = { { 20, 0, 0, 0, 0 }, { 35, 2, 0, 0, 0 }, { 55, 4, 1, 0, 0 } };
        n.levelValues = { 0.25f, 0.50f, 0.80f };
        n.prerequisiteIds = { 0 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 15;
        n.title = "CEVHER GOZU (ORE VISION)";
        n.categoryName = "MADENCILIK / SENSOR";
        n.description = "Asteroitlerin icindeki maden turu (Reishi, Vida, Disli, CPU) kirilmadan parildayarak gorunur.";
        n.effectFormat = "Kayalar Kirilmadan Maden Gosterimi";
        n.branch = NodeBranch::East_Sensors;
        n.icon = NodeIconType::Scanner;
        n.gridPos = { 1.6f, -1.1f };
        n.maxLevel = 1;
        n.levelCosts = { { 50, 5, 1, 0, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 14 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 16;
        n.title = "DERIN TARAMA (DEEP SCAN)";
        n.categoryName = "MADENCILIK / SENSOR";
        n.description = "Nadir element tasiyan asteroitlerin etrafinda ozel parilti aurası olusur.";
        n.effectFormat = "Nadir Cevher Parlama Vurgusu";
        n.branch = NodeBranch::East_Sensors;
        n.icon = NodeIconType::Radar;
        n.gridPos = { 2.6f, -1.1f };
        n.maxLevel = 1;
        n.levelCosts = { { 100, 10, 3, 0, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 15 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 17;
        n.title = "HAZINE SINYALI RADARI";
        n.categoryName = "MADENCILIK / SENSOR";
        n.description = "HUD uzerinde altin maden ve anomali anahtar sinyalleri tespit edilip yon gosterilir.";
        n.effectFormat = "Anomali & Altin Maden Radar Uyarisı";
        n.branch = NodeBranch::East_Sensors;
        n.icon = NodeIconType::Radar;
        n.gridPos = { 3.6f, -1.1f };
        n.maxLevel = 1;
        n.levelCosts = { { 170, 18, 5, 1, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 16 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 18;
        n.title = "JACKPOT DAMARI (JACKPOT VEIN)";
        n.categoryName = "MADENCILIK / SENSOR";
        n.description = "Nadir kayalar kirildiginda %20 sansla devasa 5x kaynak patlamasi (Jackpot) firlatir.";
        n.effectFormat = "%20 Sansla 5x Ganimet Patlamasi";
        n.branch = NodeBranch::East_Sensors;
        n.icon = NodeIconType::Explosion;
        n.gridPos = { 4.6f, -1.1f };
        n.maxLevel = 1;
        n.levelCosts = { { 250, 28, 8, 2, 1 } };
        n.levelValues = { 0.20f };
        n.prerequisiteIds = { 17 };
        m_nodes.push_back(n);
    }

    // Sub-branch 2: COLLECTION & MAGNET
    {
        UpgradeNode n;
        n.id = 19;
        n.title = "MANYETIK CEKIM I";
        n.categoryName = "MADENCILIK / CEKIM";
        n.description = "Kristal ve materyalleri kendine ceken manyetik vakum alani kurar.";
        n.effectFormat = "+60 Birim Cekim Yaricapi";
        n.branch = NodeBranch::East_Sensors;
        n.icon = NodeIconType::Magnet;
        n.gridPos = { 2.0f, 0.0f };
        n.maxLevel = 2;
        n.levelCosts = { { 40, 4, 1, 0, 0 }, { 80, 8, 2, 0, 0 } };
        n.levelValues = { 60.0f, 130.0f };
        n.prerequisiteIds = { 14 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 20;
        n.title = "HIPER VAKUM II";
        n.categoryName = "MADENCILIK / CEKIM";
        n.description = "Gemi cevresindeki genis alandaki tum parcalari yuksek hizla iceri ceker.";
        n.effectFormat = "+100 Birim Ekstra Cekim Alani";
        n.branch = NodeBranch::East_Sensors;
        n.icon = NodeIconType::Magnet;
        n.gridPos = { 3.0f, 0.0f };
        n.maxLevel = 1;
        n.levelCosts = { { 110, 12, 3, 1, 0 } };
        n.levelValues = { 100.0f };
        n.prerequisiteIds = { 19 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 21;
        n.title = "CEVHER YORUNGESI (RESOURCE ORBIT)";
        n.categoryName = "MADENCILIK / CEKIM";
        n.description = "Toplanan madenler gemi cevresinde suzulerek ekstra %35 verimle emilir.";
        n.effectFormat = "Donen Maden Halkasi & +35% Verim";
        n.branch = NodeBranch::East_Sensors;
        n.icon = NodeIconType::Crystal;
        n.gridPos = { 4.0f, 0.0f };
        n.maxLevel = 1;
        n.levelCosts = { { 160, 18, 5, 1, 0 } };
        n.levelValues = { 0.35f };
        n.prerequisiteIds = { 20 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 22;
        n.title = "ANLIK CEKIM (INSTANT SIPHON)";
        n.categoryName = "MADENCILIK / CEKIM";
        n.description = "Yakin cevredeki tum madenler havada beklemeden dogrudan gemi envanterine isinlanir.";
        n.effectFormat = "Madenler Aninda Teleport Olur";
        n.branch = NodeBranch::East_Sensors;
        n.icon = NodeIconType::Lightning;
        n.gridPos = { 5.0f, 0.0f };
        n.maxLevel = 1;
        n.levelCosts = { { 230, 26, 8, 2, 1 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 21 };
        m_nodes.push_back(n);
    }

    // Sub-branch 3: RECYCLING & CHAIN FRACTURE
    {
        UpgradeNode n;
        n.id = 23;
        n.title = "ZINCIRLEME KIRILMA (CHAIN FRACTURE)";
        n.categoryName = "MADENCILIK / KAZANIM";
        n.description = "Buyuk bir asteroit kirildiginda cikan sok dalgasi cevredeki kucuk kayalari da catlatir.";
        n.effectFormat = "Buyuk Kaya Kirilinca Yandakileri Catlatir";
        n.branch = NodeBranch::East_Sensors;
        n.icon = NodeIconType::Explosion;
        n.gridPos = { 1.6f, 1.1f };
        n.maxLevel = 1;
        n.levelCosts = { { 55, 6, 1, 0, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 14 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 24;
        n.title = "VIDA VE CELIK AYRISTIRICI";
        n.categoryName = "MADENCILIK / KAZANIM";
        n.description = "Kayalardan ve dusmanlardan ekstra Vida (vida.png) dusme sansini yukseltir.";
        n.effectFormat = "+50% Vida Dusme Sansi";
        n.branch = NodeBranch::East_Sensors;
        n.icon = NodeIconType::Repair;
        n.gridPos = { 2.6f, 1.1f };
        n.maxLevel = 2;
        n.levelCosts = { { 80, 8, 2, 0, 0 }, { 140, 15, 4, 1, 0 } };
        n.levelValues = { 0.50f, 1.0f };
        n.prerequisiteIds = { 23 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 25;
        n.title = "DISLI VE MEKANIZMA KAZANIMI";
        n.categoryName = "MADENCILIK / KAZANIM";
        n.description = "Elit asteroit ve dusmanlardan Disli (disli.png) dusme sansini artirir.";
        n.effectFormat = "+40% Disli Dusme Sansi";
        n.branch = NodeBranch::East_Sensors;
        n.icon = NodeIconType::Crystal;
        n.gridPos = { 3.6f, 1.1f };
        n.maxLevel = 2;
        n.levelCosts = { { 110, 12, 3, 0, 0 }, { 180, 20, 6, 1, 0 } };
        n.levelValues = { 0.40f, 0.80f };
        n.prerequisiteIds = { 24 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 26;
        n.title = "KUANTUM ISLEMCI (CPU) AYIKLAYICI";
        n.categoryName = "MADENCILIK / KAZANIM";
        n.description = "Sektor Boss'larindan ve elit hedeflerden CPU (cpu.png) dusme oranini artirir.";
        n.effectFormat = "+35% CPU Dusme Sansi";
        n.branch = NodeBranch::East_Sensors;
        n.icon = NodeIconType::Crystal;
        n.gridPos = { 4.6f, 1.1f };
        n.maxLevel = 1;
        n.levelCosts = { { 240, 28, 8, 2, 1 } };
        n.levelValues = { 0.35f };
        n.prerequisiteIds = { 25 };
        m_nodes.push_back(n);
    }

    // 🔒 LOST TECHNOLOGY 2 (EAST)
    {
        UpgradeNode n;
        n.id = 27;
        n.title = "KADIM ALTIN DAMAR CEKIRDEGI";
        n.categoryName = "KADIM TEKNOLOJI";
        n.description = "Madencilik verimini devasa olcude katlayan kadim maden transmütasyon reaktoru.";
        n.effectFormat = "+80% Tum Kaynaklara Bonus";
        n.branch = NodeBranch::East_Sensors;
        n.icon = NodeIconType::Lock;
        n.gridPos = { 4.8f, 2.3f };
        n.maxLevel = 2;
        n.isSealed = true;
        n.keySealCost = 1;
        n.levelCosts = { { 200, 25, 8, 2, 0 }, { 350, 40, 15, 4, 0 } };
        n.levelValues = { 0.40f, 0.80f };
        n.prerequisiteIds = { 26 };
        m_nodes.push_back(n);
    }

    // ⭐ CAPSTONE 2 (EAST)
    {
        UpgradeNode n;
        n.id = 98;
        n.title = "MIDAS PROTOCOL";
        n.categoryName = "CAPSTONE / NIHAI";
        n.description = "Tum kaynak kazanimi 3 katina cikar. Asteroitlerin yarisi aninda dev Jackpot patlamasina donusur.";
        n.effectFormat = "+250% Kaynak Verimi & Surekli Jackpot";
        n.branch = NodeBranch::East_Sensors;
        n.icon = NodeIconType::Star;
        n.gridPos = { 6.1f, 0.0f };
        n.maxLevel = 1;
        n.isCapstone = true;
        n.levelCosts = { { 500, 60, 20, 5, 1 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 18, 22, 26 };
        m_nodes.push_back(n);
    }

    // =========================================================================
    // 🔵 SOUTH BRANCH: ENGINE, DASH & EXPEDITION FUEL (28..39, 97)
    // =========================================================================
    // Sub-branch 1: MOBILITY & SLINGSHOT
    {
        UpgradeNode n;
        n.id = 28;
        n.title = "ITICI AYARI I";
        n.categoryName = "MOTOR / HIZ";
        n.description = "Temel itis gucunu ve gemi manevra kabiliyetini yukseltir.";
        n.effectFormat = "+30 Birim Hiz";
        n.branch = NodeBranch::South_Engine;
        n.icon = NodeIconType::Thruster;
        n.gridPos = { 0.0f, 1.0f };
        n.maxLevel = 3;
        n.levelCosts = { { 20, 0, 0, 0, 0 }, { 35, 2, 0, 0, 0 }, { 55, 4, 1, 0, 0 } };
        n.levelValues = { 30.0f, 65.0f, 105.0f };
        n.prerequisiteIds = { 0 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 29;
        n.title = "AFTERBURNER (ITIS IZI)";
        n.categoryName = "MOTOR / HIZ";
        n.description = "Gemi arkasinda parlayan plazma itis izi birakir ve ekstra hiz kazandirir.";
        n.effectFormat = "+25 Hiz & Plazma Itis Izi";
        n.branch = NodeBranch::South_Engine;
        n.icon = NodeIconType::Chevrons;
        n.gridPos = { 0.0f, 2.0f };
        n.maxLevel = 1;
        n.levelCosts = { { 60, 6, 1, 0, 0 } };
        n.levelValues = { 25.0f };
        n.prerequisiteIds = { 28 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 30;
        n.title = "MOMENTUM SURUCUSU";
        n.categoryName = "MOTOR / HIZ";
        n.description = "3 saniye boyunca duz hatta ilerlendiginde gemi %25 ekstra seyir hizi kazanir.";
        n.effectFormat = "Seyirde +25% Momentum Hizi";
        n.branch = NodeBranch::South_Engine;
        n.icon = NodeIconType::Chevrons;
        n.gridPos = { 0.0f, 3.0f };
        n.maxLevel = 1;
        n.levelCosts = { { 120, 14, 4, 1, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 29 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 31;
        n.title = "SAPAN HIZLANMASI (SLINGSHOT)";
        n.categoryName = "MOTOR / HIZ";
        n.description = "Asteroitlerin yakinindan gecerken yercekimi sapani etkisiyle anlik ivme patlamasi saglar.";
        n.effectFormat = "Asteroit Yanindan Gecerken Hiz Patlamasi";
        n.branch = NodeBranch::South_Engine;
        n.icon = NodeIconType::Thruster;
        n.gridPos = { 0.0f, 4.0f };
        n.maxLevel = 1;
        n.levelCosts = { { 210, 24, 7, 2, 1 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 30 };
        m_nodes.push_back(n);
    }

    // Sub-branch 2: DASH SPECIALIZATION
    {
        UpgradeNode n;
        n.id = 32;
        n.title = "FAZ MANEVRASI";
        n.categoryName = "MOTOR / DASH";
        n.description = "Dash bekleme suresini kisaltarak daha sik kacinma manevrasi saglar.";
        n.effectFormat = "-20% Dash Bekleme Suresi";
        n.branch = NodeBranch::South_Engine;
        n.icon = NodeIconType::SkillDash;
        n.gridPos = { -1.1f, 1.6f };
        n.maxLevel = 2;
        n.levelCosts = { { 50, 5, 1, 0, 0 }, { 90, 10, 2, 0, 0 } };
        n.levelValues = { 0.20f, 0.40f };
        n.prerequisiteIds = { 28 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 33;
        n.title = "HAYALET ATILMA (GHOST DASH)";
        n.categoryName = "MOTOR / DASH";
        n.description = "Dash esnasinda gemi tamamen saydamlasir ve dusman mermilerinin icinden hasarsiz gecer.";
        n.effectFormat = "Mermilerin Icinden Hasarsiz Gecis";
        n.branch = NodeBranch::South_Engine;
        n.icon = NodeIconType::SkillDash;
        n.gridPos = { -1.1f, 2.6f };
        n.maxLevel = 1;
        n.levelCosts = { { 110, 12, 3, 1, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 32 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 34;
        n.title = "KINETIK CARPMA (IMPACT DASH)";
        n.categoryName = "MOTOR / DASH";
        n.description = "Dash sirasinda temas edilen dusmanlara devasa kinetik ezme hasari (120 DMG) verir.";
        n.effectFormat = "Carpmada 120 Kinetik Hasar";
        n.branch = NodeBranch::South_Engine;
        n.icon = NodeIconType::Explosion;
        n.gridPos = { -1.1f, 3.6f };
        n.maxLevel = 1;
        n.levelCosts = { { 160, 18, 5, 1, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 33 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 35;
        n.title = "MADENCI ATILMASI (MINING DASH)";
        n.categoryName = "MOTOR / DASH";
        n.description = "Dash atildiginda temas edilen tum kucuk ve orta asteroitler tek vurusla aninda parcalanir.";
        n.effectFormat = "Temas Edilen Asteroitleri Aninda Kirar";
        n.branch = NodeBranch::South_Engine;
        n.icon = NodeIconType::Repair;
        n.gridPos = { -1.1f, 4.6f };
        n.maxLevel = 1;
        n.levelCosts = { { 230, 26, 8, 2, 1 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 34 };
        m_nodes.push_back(n);
    }

    // Sub-branch 3: FUEL & VOYAGE ENERGY
    {
        UpgradeNode n;
        n.id = 36;
        n.title = "GENISLETILMIS YAKIT DEPOSU";
        n.categoryName = "MOTOR / YAKIT";
        n.description = "Maksimum sefer enerjisini artirarak uzayda kalis suresini uzatir.";
        n.effectFormat = "+40 Sefer Enerjisi";
        n.branch = NodeBranch::South_Engine;
        n.icon = NodeIconType::Battery;
        n.gridPos = { 1.1f, 1.6f };
        n.maxLevel = 2;
        n.levelCosts = { { 40, 4, 1, 0, 0 }, { 80, 8, 2, 0, 0 } };
        n.levelValues = { 40.0f, 90.0f };
        n.prerequisiteIds = { 28 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 37;
        n.title = "REAKTOR VERIMLILIGI";
        n.categoryName = "MOTOR / YAKIT";
        n.description = "Enerji tuketim hizini (Drain Rate) %25 oraninda dusurerek yakit tasarrufu saglar.";
        n.effectFormat = "-25% Yakit Tukenis Hizi";
        n.branch = NodeBranch::South_Engine;
        n.icon = NodeIconType::Gauge;
        n.gridPos = { 1.1f, 2.6f };
        n.maxLevel = 1;
        n.levelCosts = { { 100, 10, 3, 1, 0 } };
        n.levelValues = { 0.25f };
        n.prerequisiteIds = { 36 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 38;
        n.title = "SIFIR NOKTASI REAKTORU";
        n.categoryName = "MOTOR / YAKIT";
        n.description = "Asteroit kirmak ve dusman yok etmek gemiye aninda yakit doldurur.";
        n.effectFormat = "Maden & Kill Basina +4 Yakit Yenileme";
        n.branch = NodeBranch::South_Engine;
        n.icon = NodeIconType::Battery;
        n.gridPos = { 1.1f, 3.6f };
        n.maxLevel = 1;
        n.levelCosts = { { 190, 22, 7, 2, 0 } };
        n.levelValues = { 4.0f };
        n.prerequisiteIds = { 37 }; // Prereq corrected to 37 (Reactor Efficiency)!
        m_nodes.push_back(n);
    }

    // 🔒 LOST TECHNOLOGY 3 (SOUTH)
    {
        UpgradeNode n;
        n.id = 39;
        n.title = "KADIM FAZ REAKTORU";
        n.categoryName = "KADIM TEKNOLOJI";
        n.description = "Dash sonrasi 3 saniye boyunca sinirsiz enerji saglayan kuantum reaktoru.";
        n.effectFormat = "Dash Sonrasi 3s 0 Yakit Tuketimi & +30% Hiz";
        n.branch = NodeBranch::South_Engine;
        n.icon = NodeIconType::Lock;
        n.gridPos = { 1.1f, 4.8f };
        n.maxLevel = 2;
        n.isSealed = true;
        n.keySealCost = 1;
        n.levelCosts = { { 200, 25, 8, 2, 0 }, { 350, 40, 15, 4, 0 } };
        n.levelValues = { 1.0f, 2.0f };
        n.prerequisiteIds = { 38 };
        m_nodes.push_back(n);
    }

    // ⭐ CAPSTONE 3 (SOUTH)
    {
        UpgradeNode n;
        n.id = 97;
        n.title = "PERPETUAL ENGINE";
        n.categoryName = "CAPSTONE / NIHAI";
        n.description = "Yakit tuketimi yariya iner. Her madencilik ve dusman imhasi yuksek yakit geri dondurur.";
        n.effectFormat = "-50% Tuketim & Sinirsiz Sefer Enerjisi";
        n.branch = NodeBranch::South_Engine;
        n.icon = NodeIconType::Star;
        n.gridPos = { 0.0f, 6.1f };
        n.maxLevel = 1;
        n.isCapstone = true;
        n.levelCosts = { { 500, 60, 20, 5, 1 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 31, 35, 38 };
        m_nodes.push_back(n);
    }

    // =========================================================================
    // 🟡 WEST BRANCH: DEFENSE, SHIELD & 3-WAY TURRET SPECIALIZATION (40..50, 96)
    // =========================================================================
    // Sub-branch 1: HULL & SHIELD
    {
        UpgradeNode n;
        n.id = 40;
        n.title = "GUCENDIRILMIS GOVDE I";
        n.categoryName = "SAVUNMA / GOVDE";
        n.description = "Gemi zırhını guclendirerek fazladan dayanıklılık kalbi ekler.";
        n.effectFormat = "+1 Can Kalbi";
        n.branch = NodeBranch::West_Defense;
        n.icon = NodeIconType::Heart;
        n.gridPos = { -1.0f, 0.0f };
        n.maxLevel = 2;
        n.levelCosts = { { 30, 2, 0, 0, 0 }, { 75, 8, 2, 0, 0 } };
        n.levelValues = { 1.0f, 2.0f };
        n.prerequisiteIds = { 0 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 41;
        n.title = "PLAZMA KALKAN BALONU";
        n.categoryName = "SAVUNMA / KALKAN";
        n.description = "Her 10 saniyede bir 1 hasari tamamen engelleyen plazma kalkani uretir.";
        n.effectFormat = "1 Hasar Engelleyen Koruma Kalkani";
        n.branch = NodeBranch::West_Defense;
        n.icon = NodeIconType::Shield;
        n.gridPos = { -1.6f, -1.1f };
        n.maxLevel = 1;
        n.levelCosts = { { 60, 6, 2, 0, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 40 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 42;
        n.title = "YANSITICI KALKAN (REFLECTIVE)";
        n.categoryName = "SAVUNMA / KALKAN";
        n.description = "Kalkanin engelledigi dusman mermileri saldirganlara dogru geri firlar.";
        n.effectFormat = "Engellenen Mermiler Dusmana Geri Doner";
        n.branch = NodeBranch::West_Defense;
        n.icon = NodeIconType::Barrier;
        n.gridPos = { -2.6f, -1.1f };
        n.maxLevel = 1;
        n.levelCosts = { { 130, 15, 4, 1, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 41 };
        m_nodes.push_back(n);
    }

    // Sub-branch 2: 3-WAY TURRET SPECIALIZATION
    {
        UpgradeNode n;
        n.id = 43;
        n.title = "OTONOM TARET CEKIRDEGI";
        n.categoryName = "SAVUNMA / TARET";
        n.description = "Haritada sabit savunma istasyonu kuran ve alana girince otomatik ates eden 1 taret.";
        n.effectFormat = "+1 Harita Konumlu Savunma Tareti";
        n.branch = NodeBranch::West_Defense;
        n.icon = NodeIconType::Turret;
        n.gridPos = { -2.0f, 0.0f };
        n.maxLevel = 1;
        n.levelCosts = { { 60, 6, 2, 0, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 40 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 44;
        n.title = "KINETIK TARET PLATFORMU";
        n.categoryName = "SAVUNMA / TARET";
        n.description = "Taretin atis araligini kisaltir ve hasarini yukseltir.";
        n.effectFormat = "+15 Taret Hasari & Seri Atis";
        n.branch = NodeBranch::West_Defense;
        n.icon = NodeIconType::Turret;
        n.gridPos = { -3.0f, 0.0f };
        n.maxLevel = 1;
        n.levelCosts = { { 100, 10, 3, 1, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 43 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 45;
        n.title = "DELICI SILAH TARETI (GUN SPEC)";
        n.categoryName = "SAVUNMA / TARET SPEC";
        n.description = "Taret yuksek hizli delici makineli tufege donusur; dusmanlari bicer.";
        n.effectFormat = "Delici Muharebe Tareti & +30 Hasar";
        n.branch = NodeBranch::West_Defense;
        n.icon = NodeIconType::Sword;
        n.gridPos = { -3.6f, -1.1f };
        n.maxLevel = 1;
        n.levelCosts = { { 160, 18, 5, 1, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 44 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 46;
        n.title = "CEVHER MADENCI TARETI (MINING SPEC)";
        n.categoryName = "SAVUNMA / TARET SPEC";
        n.description = "Taret oncelikli olarak asteroitleri hedefler ve 2 kat maden hasari ile zengin cevher tarar.";
        n.effectFormat = "Otomatik Madencilik & 2x Kaya Hasari";
        n.branch = NodeBranch::West_Defense;
        n.icon = NodeIconType::Repair;
        n.gridPos = { -4.0f, 0.0f };
        n.maxLevel = 1;
        n.levelCosts = { { 160, 18, 5, 1, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 44 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 47;
        n.title = "PLAZMA MORTAR TARETI (PLASMA SPEC)";
        n.categoryName = "SAVUNMA / TARET SPEC";
        n.description = "Taret patlayici plazma toplari atarak genis alandaki hedefleri sok dalgasiyla yok eder.";
        n.effectFormat = "Genis AoE Plazma Havan Atisi";
        n.branch = NodeBranch::West_Defense;
        n.icon = NodeIconType::Explosion;
        n.gridPos = { -3.6f, 1.1f };
        n.maxLevel = 1;
        n.levelCosts = { { 160, 18, 5, 1, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 44 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 48;
        n.title = "ORBITAL SAVUNMA AGI (3 TARET)";
        n.categoryName = "SAVUNMA / TARET";
        n.description = "Haritada 3 ayri stratejik noktaya savunma istasyonu kurar ve tum sektoru kontrol altina alir.";
        n.effectFormat = "Toplam 3 Harita Savunma Istasyonu";
        n.branch = NodeBranch::West_Defense;
        n.icon = NodeIconType::Turret;
        n.gridPos = { -5.1f, 0.0f };
        n.maxLevel = 1;
        n.levelCosts = { { 300, 35, 12, 3, 1 } };
        n.levelValues = { 3.0f };
        n.prerequisiteIds = { 45, 46, 47 }; // Any 3-way specialization enables orbital network!
        m_nodes.push_back(n);
    }

    // Sub-branch 3: SHOCKWAVE
    {
        UpgradeNode n;
        n.id = 49;
        n.title = "SOK DALGASI JENERATORU";
        n.categoryName = "SAVUNMA / EMP";
        n.description = "Periyodik olarak tum dusman mermilerini silen ve hasar veren sok dalgasi yayar.";
        n.effectFormat = "9 Sn'de Bir Mermi Silen EMP Dalgasi";
        n.branch = NodeBranch::West_Defense;
        n.icon = NodeIconType::Shockwave;
        n.gridPos = { -1.6f, 1.1f };
        n.maxLevel = 1;
        n.levelCosts = { { 55, 6, 2, 0, 0 } };
        n.levelValues = { 45.0f };
        n.prerequisiteIds = { 40 };
        m_nodes.push_back(n);
    }

    // 🔒 LOST TECHNOLOGY 4 (WEST)
    {
        UpgradeNode n;
        n.id = 50;
        n.title = "KADIM MUHAFIZ YAPAY ZEKASI";
        n.categoryName = "KADIM TEKNOLOJI";
        n.description = "Taretlerin kritik noktalara isabet oranini artirir ve menzillerini genisletir.";
        n.effectFormat = "+100 Taret Menzili & +40 Hasar";
        n.branch = NodeBranch::West_Defense;
        n.icon = NodeIconType::Lock;
        n.gridPos = { -4.8f, 2.3f };
        n.maxLevel = 2;
        n.isSealed = true;
        n.keySealCost = 1;
        n.levelCosts = { { 200, 25, 8, 2, 0 }, { 350, 40, 15, 4, 0 } };
        n.levelValues = { 1.0f, 2.0f };
        n.prerequisiteIds = { 48 };
        m_nodes.push_back(n);
    }

    // ⭐ CAPSTONE 4 (WEST)
    {
        UpgradeNode n;
        n.id = 96;
        n.title = "ORBITAL FORTRESS";
        n.categoryName = "CAPSTONE / NIHAI";
        n.description = "3 Taret + Yansitici Kalkan + Surekli Sok Dalgasi mukemmel sinerjiye girer.";
        n.effectFormat = "Gecilmez Kale: 3 Taret, Ekstra Kalkan & Surekli EMP";
        n.branch = NodeBranch::West_Defense;
        n.icon = NodeIconType::Star;
        n.gridPos = { -6.1f, 0.0f };
        n.maxLevel = 1;
        n.isCapstone = true;
        n.levelCosts = { { 500, 60, 20, 5, 1 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 42, 48, 49 };
        m_nodes.push_back(n);
    }

    // =========================================================================
    // 🔀 CROSS-BRANCH HYBRID NODES (51..55)
    // =========================================================================
    {
        UpgradeNode n;
        n.id = 51;
        n.title = "LAZER KAZICI (LASER EXCAVATOR)";
        n.categoryName = "HIBRIT / SILAH+MADEN";
        n.description = "Lazer hasari asteroitlere karsi +100% ekstra gucle calisir.";
        n.effectFormat = "Asteroitlere +100% Lazer Maden Hasari";
        n.branch = NodeBranch::Hybrid;
        n.icon = NodeIconType::Hybrid;
        n.gridPos = { 2.7f, -2.7f };
        n.maxLevel = 1;
        n.isHybrid = true;
        n.levelCosts = { { 150, 18, 5, 1, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 1, 14 }; // Weapon 1 + Mining 14
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 52;
        n.title = "HURDA TOPLAYICI DRON (SALVAGE DRONE)";
        n.categoryName = "HIBRIT / SAVUNMA+MADEN";
        n.description = "Taretlerin yok ettigi veya kirdigi tum hedeflerin ganimeti aninda toplanir.";
        n.effectFormat = "Taretlerin Vurdugu Loot Otomatik Toplanir";
        n.branch = NodeBranch::Hybrid;
        n.icon = NodeIconType::Hybrid;
        n.gridPos = { 2.7f, 2.7f };
        n.maxLevel = 1;
        n.isHybrid = true;
        n.levelCosts = { { 150, 18, 5, 1, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 19, 40 }; // Mining 19 + Defense 40
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 53;
        n.title = "HIZ TOPU (VELOCITY CANNON)";
        n.categoryName = "HIBRIT / MOTOR+SILAH";
        n.description = "Gemi hareket hizi arttikca lazerin verdigi hasar dogrudan yukselir.";
        n.effectFormat = "Hizlandikca Lazer Hasari Artar (+1% her 5px hiz)";
        n.branch = NodeBranch::Hybrid;
        n.icon = NodeIconType::Hybrid;
        n.gridPos = { -2.7f, -2.7f };
        n.maxLevel = 1;
        n.isHybrid = true;
        n.levelCosts = { { 150, 18, 5, 1, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 1, 28 }; // Weapon 1 + Engine 28 (Corrected!)
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 54;
        n.title = "MISILLEME MATRISI (RETALIATION MATRIX)";
        n.categoryName = "HIBRIT / SAVUNMA+SILAH";
        n.description = "Hasar alindiginda veya kalkan patladiginda 4 sn boyunca +100% atis hizi patlamasi.";
        n.effectFormat = "Hasar Alinca 4s Lazer Cilginligi (+100% Hiz)";
        n.branch = NodeBranch::Hybrid;
        n.icon = NodeIconType::Hybrid;
        n.gridPos = { -3.8f, -2.2f };
        n.maxLevel = 1;
        n.isHybrid = true;
        n.levelCosts = { { 200, 25, 8, 2, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 1, 40 }; // Weapon 1 + Defense 40 (Corrected!)
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 55;
        n.title = "HIPER CEKIM ALANI (HYPER MAGNET)";
        n.categoryName = "HIBRIT / MOTOR+MADEN";
        n.description = "Yuksek hizla ilerlerken manyetik cekim yaricapi %50 oraninda genisler.";
        n.effectFormat = "Yuksek Hizda +50% Cekim Alani";
        n.branch = NodeBranch::Hybrid;
        n.icon = NodeIconType::Hybrid;
        n.gridPos = { -2.7f, 2.7f };
        n.maxLevel = 1;
        n.isHybrid = true;
        n.levelCosts = { { 150, 18, 5, 1, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 19, 28 }; // Mining 19 + Engine 28 (Corrected!)
        m_nodes.push_back(n);
    }

    // =========================================================================
    // ⚡ ACTIVE SKILLS (56..58)
    // =========================================================================
    {
        UpgradeNode n;
        n.id = 56;
        n.title = "[Q] ENERJI DALGASI (EMP NOVA)";
        n.categoryName = "AKTIF BECERI [Q]";
        n.description = "Ekrani kaplayan sok dalgasi tum dusman mermilerini siler ve hasar verir.";
        n.effectFormat = "[Q] Tusu ile Mermi Sici ve EMP Dalgasi";
        n.branch = NodeBranch::Active_Skills;
        n.icon = NodeIconType::SkillEmp;
        n.gridPos = { -4.2f, -3.8f };
        n.maxLevel = 1;
        n.levelCosts = { { 50, 5, 1, 0, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 54 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 57;
        n.title = "[E] ASIRI YUKLEME (OVERCHARGE)";
        n.categoryName = "AKTIF BECERI [E]";
        n.description = "4 saniye boyunca hiper plazma lazer frekansini tetikler.";
        n.effectFormat = "[E] Tusu ile 4s Mega Lazer Cilginligi";
        n.branch = NodeBranch::Active_Skills;
        n.icon = NodeIconType::SkillOvercharge;
        n.gridPos = { 4.2f, -3.8f };
        n.maxLevel = 1;
        n.levelCosts = { { 80, 8, 2, 0, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 51 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 58;
        n.title = "[SHIFT] FAZ ATILMASI (PHASE DASH)";
        n.categoryName = "AKTIF BECERI [SHIFT]";
        n.description = "Gemi aninda ileri atilir ve hasar gormez.";
        n.effectFormat = "[SHIFT] veya [SPACE] ile Dokunulmaz Dash";
        n.branch = NodeBranch::Active_Skills;
        n.icon = NodeIconType::SkillDash;
        n.gridPos = { -4.2f, 3.8f };
        n.maxLevel = 1;
        n.levelCosts = { { 40, 4, 1, 0, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 55 };
        m_nodes.push_back(n);
    }
}

UpgradeNode* UpgradeTree::GetNodeById(int id)
{
    for (auto& n : m_nodes)
    {
        if (n.id == id) return &n;
    }
    return nullptr;
}

const UpgradeNode* UpgradeTree::GetNodeById(int id) const
{
    for (const auto& n : m_nodes)
    {
        if (n.id == id) return &n;
    }
    return nullptr;
}

bool UpgradeTree::HasPrerequisites(int nodeId) const
{
    const UpgradeNode* node = GetNodeById(nodeId);
    if (!node) return false;
    if (node->isCenterHub) return true;
    if (node->prerequisiteIds.empty()) return true;
     
    // For Hybrid nodes (isHybrid == true): require ALL prerequisites!
    if (node->isHybrid)
    {
        for (int parentId : node->prerequisiteIds)
        {
            const UpgradeNode* parent = GetNodeById(parentId);
            if (!parent || !parent->IsUnlocked())
            {
                return false;
            }
        }
        return true;
    }

    // For standard nodes: if any prerequisite is unlocked, it can be reached!
    for (int parentId : node->prerequisiteIds)
    {
        const UpgradeNode* parent = GetNodeById(parentId);
        if (parent && parent->IsUnlocked())
        {
            return true;
        }
    }
    return false;
}

void UpgradeTree::UpdateLayout()
{
    m_treeCenter = DirectX::XMFLOAT2((float)SCREEN_WIDTH * 0.5f, (float)SCREEN_HEIGHT * 0.52f);
    m_nodeSpacingX = 62.0f * m_zoom;
    m_nodeSpacingY = 56.0f * m_zoom;
    m_nodeSize = 34.0f * m_zoom;

    for (auto& node : m_nodes)
    {
        node.screenPos.x = m_treeCenter.x + m_panOffset.x + node.gridPos.x * m_nodeSpacingX;
        node.screenPos.y = m_treeCenter.y + m_panOffset.y + node.gridPos.y * m_nodeSpacingY;
    }
}

void UpgradeTree::ApplyStats(PlayerStats& outStats) const
{
    // Reset base stats
    outStats.maxHealth = 2;
    outStats.moveSpeed = 220.0f;
    outStats.laserDamage = 45.0f;
    outStats.laserFireInterval = 0.16f;
    outStats.laserRange = 280.0f;
    outStats.laserCount = 1;
    outStats.pickupRadius = 120.0f;
    outStats.maxFuel = 150.0f;
    outStats.fuelDrainRate = 2.5f;
    outStats.resourceMultiplier = 1.0f;

    // Reset Weapon Flags
    outStats.piercingBeam = false;
    outStats.overheatEnabled = false;
    outStats.coreMeltdown = false;
    outStats.chainLaser = false;
    outStats.prismaticSplit = false;
    outStats.crystalWeakpoints = false;
    outStats.critChance = 0.0f;
    outStats.critMultiplier = 2.0f;

    // Reset Mining Flags
    outStats.rareScanner = false;
    outStats.oreVision = false;
    outStats.deepScan = false;
    outStats.treasureSignal = false;
    outStats.chainFracture = false;
    outStats.jackpotChance = 0.0f;
    outStats.resourceOrbit = true;
    outStats.instantCollection = false;
    outStats.vidaBonus = 0.0f;
    outStats.disliBonus = 0.0f;
    outStats.cpuBonus = 0.0f;

    // Reset Engine Flags
    outStats.afterburner = false;
    outStats.momentumDrive = false;
    outStats.asteroidSlingshot = false;
    outStats.dashType = DashType::Standard;
    outStats.dashCooldown = 3.5f;
    outStats.zeroPointReactor = false;

    // Reset Defense Flags
    outStats.maxShield = 0;
    outStats.shieldBubbleUnlocked = false;
    outStats.reflectiveShield = false;
    outStats.turretCount = 0;
    outStats.turretRange = 260.0f;
    outStats.turretDamage = 35.0f;
    outStats.turretFireInterval = 0.60f;
    outStats.turretSpec = TurretSpec::Gun;

    outStats.shockwaveUnlocked = false;
    outStats.shockwaveRadius = 220.0f;
    outStats.shockwaveDamage = 45.0f;
    outStats.shockwaveInterval = 9.0f;

    // Reset Cross-Branch Hybrids
    outStats.laserExcavator = false;
    outStats.salvageDrone = false;
    outStats.velocityCannon = false;
    outStats.retaliationMatrix = false;
    outStats.hyperMagnet = false;

    // Capstone
    outStats.activeCapstone = 0;

    // Reset Active Skills
    outStats.skill1 = ActiveSkillType::None;
    outStats.skill2 = ActiveSkillType::None;
    outStats.skill3 = ActiveSkillType::None;

    // Apply modifiers from each unlocked node
    for (const auto& n : m_nodes)
    {
        if (n.currentLevel <= 0) continue;
        int lvlIdx = n.currentLevel - 1;
        float val = (lvlIdx < (int)n.levelValues.size()) ? n.levelValues[lvlIdx] : 0.0f;

        switch (n.id)
        {
            // North: Weapon
            case 1: outStats.laserDamage += val; break;
            case 2: outStats.laserDamage += val; break;
            case 3: outStats.piercingBeam = true; break;
            case 4: outStats.overheatEnabled = true; break;
            case 5: outStats.coreMeltdown = true; break;
            case 6: outStats.laserCount = std::max(2, outStats.laserCount); break;
            case 7: outStats.laserCount = std::max(3, outStats.laserCount); break;
            case 8: outStats.chainLaser = true; break;
            case 9: outStats.prismaticSplit = true; break;
            case 10: outStats.critChance += val; break;
            case 11: outStats.critMultiplier += val; break;
            case 12: outStats.crystalWeakpoints = true; break;
            case 13: outStats.laserDamage += val; outStats.piercingBeam = true; break;
            case 99: outStats.activeCapstone = 1; outStats.laserDamage += 100.0f; outStats.piercingBeam = true; outStats.overheatEnabled = true; break;

            // East: Mining
            case 14: outStats.resourceMultiplier += val; break;
            case 15: outStats.oreVision = true; break;
            case 16: outStats.deepScan = true; break;
            case 17: outStats.treasureSignal = true; break;
            case 18: outStats.jackpotChance += val; break;
            case 19: outStats.pickupRadius += val; break;
            case 20: outStats.pickupRadius += 100.0f; break;
            case 21: outStats.resourceOrbit = true; outStats.resourceMultiplier += val; break;
            case 22: outStats.instantCollection = true; break;
            case 23: outStats.chainFracture = true; break;
            case 24: outStats.vidaBonus += val; break;
            case 25: outStats.disliBonus += val; break;
            case 26: outStats.cpuBonus += val; break;
            case 27: outStats.resourceMultiplier += val; outStats.jackpotChance += 0.15f; break;
            case 98: outStats.activeCapstone = 2; outStats.resourceMultiplier += 2.5f; outStats.jackpotChance += 0.25f; break;

            // South: Engine
            case 28: outStats.moveSpeed += val; break;
            case 29: outStats.afterburner = true; outStats.moveSpeed += 25.0f; break;
            case 30: outStats.momentumDrive = true; break;
            case 31: outStats.asteroidSlingshot = true; break;
            case 32: outStats.dashCooldown *= (1.0f - val); break;
            case 33: outStats.dashType = DashType::Ghost; break;
            case 34: outStats.dashType = DashType::Impact; break;
            case 35: outStats.dashType = DashType::Mining; break;
            case 36: outStats.maxFuel += val; break;
            case 37: outStats.fuelDrainRate *= (1.0f - val); break;
            case 38: outStats.zeroPointReactor = true; break;
            case 39: outStats.moveSpeed += 30.0f; outStats.zeroPointReactor = true; break;
            case 97: outStats.activeCapstone = 3; outStats.fuelDrainRate *= 0.5f; outStats.zeroPointReactor = true; break;

            // West: Defense & 3-Way Turret Specialization
            case 40: outStats.maxHealth += (int)val; break;
            case 41: outStats.shieldBubbleUnlocked = true; outStats.maxShield = 1; break;
            case 42: outStats.reflectiveShield = true; break;
            case 43: outStats.turretCount = std::max(1, outStats.turretCount); outStats.turretSpec = TurretSpec::Gun; break;
            case 44: outStats.turretDamage += 15.0f; outStats.turretFireInterval *= 0.85f; break;
            case 45: outStats.turretSpec = TurretSpec::Gun; outStats.turretDamage += 30.0f; outStats.turretFireInterval = 0.28f; break;
            case 46: outStats.turretSpec = TurretSpec::Mining; outStats.turretDamage += 25.0f; break;
            case 47: outStats.turretSpec = TurretSpec::Plasma; outStats.turretDamage += 40.0f; break;
            case 48: outStats.turretCount = 3; break;
            case 49: outStats.shockwaveUnlocked = true; outStats.shockwaveDamage = 45.0f; break;
            case 50: outStats.turretDamage += 40.0f; outStats.turretRange += 100.0f; break;
            case 96: outStats.activeCapstone = 4; outStats.turretCount = 3; outStats.reflectiveShield = true; outStats.shockwaveUnlocked = true; break;

            // Hybrids
            case 51: outStats.laserExcavator = true; break;
            case 52: outStats.salvageDrone = true; break;
            case 53: outStats.velocityCannon = true; break;
            case 54: outStats.retaliationMatrix = true; break;
            case 55: outStats.hyperMagnet = true; break;

            // Active Skills
            case 56: outStats.skill1 = ActiveSkillType::EmpWave; break;
            case 57: outStats.skill2 = ActiveSkillType::Overcharge; break;
            case 58: outStats.skill3 = ActiveSkillType::PhaseDash; break;

            default: break;
        }
    }
}

bool UpgradeTree::CanUnlockNode(int nodeId, const PlayerResources& bank) const
{
    const UpgradeNode* node = GetNodeById(nodeId);
    if (!node) return false;
    if (node->isCenterHub) return false;
    if (node->IsMaxLevel()) return false;
    if (!HasPrerequisites(nodeId)) return false;

    // Sealed node check: must unseal first with Key
    if (node->isSealed && !node->isSealBroken)
    {
        return false;
    }

    // Capstone 1-per-build limit
    if (node->isCapstone)
    {
        if (m_activeCapstoneId != -1 && m_activeCapstoneId != node->id)
        {
            return false;
        }
    }

    int nextLevel = node->currentLevel;
    if (nextLevel >= (int)node->levelCosts.size()) return false;

    const ResourceCost& cost = node->levelCosts[nextLevel];
    if (bank.reishi < cost.reishi) return false;
    if (bank.vida < cost.vida) return false;
    if (bank.disli < cost.disli) return false;
    if (bank.cpu < cost.cpu) return false;
    if (bank.key < cost.key) return false;

    return true;
}

bool UpgradeTree::UnsealLostTechNode(int nodeId, PlayerResources& bank)
{
    UpgradeNode* node = GetNodeById(nodeId);
    if (!node || !node->isSealed || node->isSealBroken) return false;
    if (!HasPrerequisites(nodeId)) return false;

    if (bank.key >= node->keySealCost)
    {
        bank.key -= node->keySealCost;
        node->isSealBroken = true;
        node->unlockPulseTimer = 1.0f;
        if (m_soundShoot != -1) PlayAudio(m_soundShoot);
        return true;
    }
    return false;
}

bool UpgradeTree::PurchaseUpgrade(int nodeId, PlayerResources& bank, PlayerStats& stats)
{
    UpgradeNode* node = GetNodeById(nodeId);
    if (!node) return false;

    // If node is sealed and player clicks to unseal it:
    if (node->isSealed && !node->isSealBroken)
    {
        return UnsealLostTechNode(nodeId, bank);
    }

    if (!CanUnlockNode(nodeId, bank)) return false;

    int nextLevel = node->currentLevel;
    const ResourceCost& cost = node->levelCosts[nextLevel];

    bank.reishi -= cost.reishi;
    bank.vida -= cost.vida;
    bank.disli -= cost.disli;
    bank.cpu -= cost.cpu;
    bank.key -= cost.key;

    node->currentLevel++;
    node->unlockPulseTimer = 0.40f;

    if (node->isCapstone)
    {
        m_activeCapstoneId = node->id;
    }

    ApplyStats(stats);

    if (m_soundShoot != -1)
    {
        PlayAudio(m_soundShoot);
    }

    return true;
}

void UpgradeTree::AddRunEarnings(PlayerResources& bank, int reishi, int vida, int disli, int cpu, int key)
{
    bank.reishi += reishi;
    bank.vida += vida;
    bank.disli += disli;
    bank.cpu += cpu;
    bank.key += key;

    m_quest.currentAmount += reishi;
    if (m_quest.currentAmount >= m_quest.targetAmount && !m_quest.completed)
    {
        m_quest.completed = true;
        bank.key += 1;
    }
}

void UpgradeTree::Update(float deltaTime, PlayerStats& stats, PlayerResources& bank, bool& outStartGame, int currentSector)
{
    m_globalTime += deltaTime;
    m_currentSectorIndex = currentSector;
    outStartGame = false;

    int mouseX = InputMouse_GetX();
    int mouseY = InputMouse_GetY();

    // 1. Mouse Wheel Zoom In / Out
    int wheel = InputMouse_GetScrollWheel();
    if (wheel > 0)
    {
        m_targetZoom = std::min(1.50f, m_targetZoom + 0.10f);
    }
    else if (wheel < 0)
    {
        m_targetZoom = std::max(0.65f, m_targetZoom - 0.10f);
    }

    // 2. Right-Click or Middle-Click Mouse Drag (Pan)
    if (InputMouse_IsPress(MOUSE_BUTTON_RIGHT) || InputMouse_IsPress(MOUSE_BUTTON_MIDDLE))
    {
        if (!m_isPanning)
        {
            m_isPanning = true;
            m_lastMouseX = mouseX;
            m_lastMouseY = mouseY;
        }
        else
        {
            int dx = mouseX - m_lastMouseX;
            int dy = mouseY - m_lastMouseY;
            m_targetPanOffset.x += (float)dx;
            m_targetPanOffset.y += (float)dy;
            m_lastMouseX = mouseX;
            m_lastMouseY = mouseY;
        }
    }
    else
    {
        m_isPanning = false;
    }

    // 3. Reset Zoom & Pan Shortcut [R]
    if (InputKeyboard_IsTrigger(KK_R))
    {
        m_targetZoom = 0.95f;
        m_targetPanOffset = { 0.0f, 0.0f };
    }

    // 4. Smooth Interpolation for Zoom and Pan
    m_zoom += (m_targetZoom - m_zoom) * 14.0f * deltaTime;
    m_panOffset.x += (m_targetPanOffset.x - m_panOffset.x) * 14.0f * deltaTime;
    m_panOffset.y += (m_targetPanOffset.y - m_panOffset.y) * 14.0f * deltaTime;

    // Recalculate node screen positions based on current zoom & pan
    UpdateLayout();

    m_hoveredNodeId = -1;

    // Check hover over each node
    for (auto& node : m_nodes)
    {
        float curSize = node.isCenterHub ? (50.0f * m_zoom) : (node.isCapstone ? (44.0f * m_zoom) : m_nodeSize);
        float halfW = curSize * 0.5f;
        float halfH = curSize * 0.5f;

        bool isInside = (mouseX >= node.screenPos.x - halfW && mouseX <= node.screenPos.x + halfW &&
                         mouseY >= node.screenPos.y - halfH && mouseY <= node.screenPos.y + halfH);

        if (isInside)
        {
            m_hoveredNodeId = node.id;
        }

        // Hover animation lerp
        float targetHover = isInside ? 1.0f : 0.0f;
        node.hoverProgress += (targetHover - node.hoverProgress) * 12.0f * deltaTime;

        // Unlock pulse decay
        if (node.unlockPulseTimer > 0.0f)
        {
            node.unlockPulseTimer -= deltaTime;
        }
    }

    // Check hover over Bank Resource rows
    m_hoveredBankIndex = -1;
    float panelX = 40.0f;
    float panelY = 45.0f;
    float startY = panelY + 55.0f;
    float rowSpacing = 48.0f;
    for (int i = 0; i < 5; ++i)
    {
        float curY = startY + i * rowSpacing;
        if (mouseX >= panelX && mouseX <= panelX + 280.0f &&
            mouseY >= curY && mouseY <= curY + 42.0f)
        {
            m_hoveredBankIndex = i;
            break;
        }
    }

    // Check click on Right Panel Sector cards
    float panelRightX = 1320.0f;
    float panelRightW = 250.0f;
    float startSectorY = 45.0f + 48.0f;
    float sectorCardH = 54.0f;
    float sectorSpacing = 62.0f;

    bool mouseLeftClick = InputMouse_IsTrigger(MOUSE_BUTTON_LEFT);

    for (int i = 0; i < (int)m_sectors.size(); ++i)
    {
        float cy = startSectorY + i * sectorSpacing;
        if (mouseX >= panelRightX && mouseX <= panelRightX + panelRightW &&
            mouseY >= cy && mouseY <= cy + sectorCardH)
        {
            if (mouseLeftClick)
            {
                if (m_sectors[i].unlocked)
                {
                    m_currentSectorIndex = m_sectors[i].stageNumber;
                }
                else if (bank.key >= 1)
                {
                    // Unlock with Sector Key!
                    bank.key -= 1;
                    m_sectors[i].unlocked = true;
                    m_currentSectorIndex = m_sectors[i].stageNumber;
                }
            }
        }
    }

    // Check Zoom UI button clicks (Bottom Center)
    float zCx = (float)SCREEN_WIDTH * 0.5f;
    float zPanelW = 260.0f;
    float zPanelX = zCx - zPanelW * 0.5f;
    float zPanelY = (float)SCREEN_HEIGHT - 54.0f;

    if (mouseLeftClick && mouseY >= zPanelY && mouseY <= zPanelY + 34.0f)
    {
        if (mouseX >= zPanelX + 6.0f && mouseX <= zPanelX + 34.0f)
        {
            m_targetZoom = std::min(1.50f, m_targetZoom + 0.12f);
        }
        else if (mouseX >= zPanelX + 96.0f && mouseX <= zPanelX + 124.0f)
        {
            m_targetZoom = std::max(0.65f, m_targetZoom - 0.12f);
        }
        else if (mouseX >= zPanelX + 132.0f && mouseX <= zPanelX + 252.0f)
        {
            m_targetZoom = 0.95f;
            m_targetPanOffset = { 0.0f, 0.0f };
        }
    }

    // Check Start Button hover (Bottom right)
    float btnX = 1320.0f;
    float btnY = 770.0f;
    float btnW = 250.0f;
    float btnH = 60.0f;
    m_btnStartHovered = (mouseX >= btnX && mouseX <= btnX + btnW && mouseY >= btnY && mouseY <= btnY + btnH);
    float targetBtnAnim = m_btnStartHovered ? 1.0f : 0.0f;
    m_btnStartHoverAnim += (targetBtnAnim - m_btnStartHoverAnim) * 10.0f * deltaTime;

    // Mouse click handling for Tree Nodes & Start Button
    if (mouseLeftClick)
    {
        if (m_hoveredNodeId != -1)
        {
            PurchaseUpgrade(m_hoveredNodeId, bank, stats);
        }
        else if (m_btnStartHovered)
        {
            outStartGame = true;
        }
    }

    // Keyboard shortcut to start expedition
    if (InputKeyboard_IsTrigger(KK_SPACE))
    {
        outStartGame = true;
    }
}

void UpgradeTree::Draw(const PlayerResources& bank, int currentStage)
{
    DrawGridBackground();
    DrawLeftPanel(bank);
    DrawBranchLines();
    DrawTreeNodes();
    DrawRightPanel(currentStage);
    DrawZoomControls();

    // Floating Tooltip if hovering over any node or bank resource
    if (m_hoveredNodeId != -1)
    {
        const UpgradeNode* hoveredNode = GetNodeById(m_hoveredNodeId);
        if (hoveredNode)
        {
            DrawTooltip(hoveredNode, bank);
        }
    }
    else if (m_hoveredBankIndex != -1)
    {
        DrawBankResourceTooltip(m_hoveredBankIndex);
    }
}

void UpgradeTree::DrawZoomControls()
{
    float cx = (float)SCREEN_WIDTH * 0.5f;
    float panelW = 260.0f;
    float panelH = 34.0f;
    float panelX = cx - panelW * 0.5f;
    float panelY = (float)SCREEN_HEIGHT - 54.0f;

    Sprite_DrawRect(panelX, panelY, panelW, panelH, { 0.10f, 0.07f, 0.14f, 0.92f });
    Sprite_DrawRectBorder(panelX, panelY, panelW, panelH, 1.5f, { 0.40f, 0.85f, 0.95f, 0.70f });

    // [+] Button
    Sprite_DrawRect(panelX + 6.0f, panelY + 5.0f, 28.0f, 24.0f, { 0.18f, 0.14f, 0.24f, 0.90f });
    Sprite_DrawRectBorder(panelX + 6.0f, panelY + 5.0f, 28.0f, 24.0f, 1.0f, { 0.6f, 0.9f, 1.0f, 0.8f });
    DrawTextMatrix(panelX + 15.0f, panelY + 9.0f, "+", 2.2f, { 1.0f, 1.0f, 1.0f, 1.0f });

    // Zoom %
    char zoomBuf[32];
    sprintf_s(zoomBuf, "%d%%", (int)(m_zoom * 100.0f));
    DrawTextMatrix(panelX + 44.0f, panelY + 11.0f, zoomBuf, 1.8f, { 0.40f, 1.0f, 0.85f, 1.0f });

    // [-] Button
    Sprite_DrawRect(panelX + 96.0f, panelY + 5.0f, 28.0f, 24.0f, { 0.18f, 0.14f, 0.24f, 0.90f });
    Sprite_DrawRectBorder(panelX + 96.0f, panelY + 5.0f, 28.0f, 24.0f, 1.0f, { 0.6f, 0.9f, 1.0f, 0.8f });
    DrawTextMatrix(panelX + 106.0f, panelY + 9.0f, "-", 2.2f, { 1.0f, 1.0f, 1.0f, 1.0f });

    // [SIFIRLA] Button
    Sprite_DrawRect(panelX + 132.0f, panelY + 5.0f, 120.0f, 24.0f, { 0.18f, 0.14f, 0.24f, 0.90f });
    Sprite_DrawRectBorder(panelX + 132.0f, panelY + 5.0f, 120.0f, 24.0f, 1.0f, { 1.0f, 0.85f, 0.4f, 0.8f });
    DrawTextMatrix(panelX + 144.0f, panelY + 11.0f, "SIFIRLA [R]", 1.6f, { 1.0f, 0.90f, 0.50f, 1.0f });

    // Hint text above
    DrawTextMatrix(cx - 180.0f, panelY - 18.0f, "MOUSE WHEEL: ZOOM  |  SAG TIK: SURUKLE  |  [R]: SIFIRLA", 1.4f, { 0.65f, 0.75f, 0.85f, 0.75f });
}

void UpgradeTree::DrawGridBackground()
{
    // Deep eggplant dark base
    Sprite_DrawRect(0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, { 0.09f, 0.06f, 0.12f, 1.0f });

    // Subtle grid lines
    DirectX::XMFLOAT4 gridCol{ 0.22f, 0.15f, 0.28f, 0.35f };
    float gridStep = 40.0f;
    for (float x = 0; x < SCREEN_WIDTH; x += gridStep)
    {
        Sprite_DrawLine(x, 0.0f, x, (float)SCREEN_HEIGHT, 1.0f, gridCol);
    }
    for (float y = 0; y < SCREEN_HEIGHT; y += gridStep)
    {
        Sprite_DrawLine(0.0f, y, (float)SCREEN_WIDTH, y, 1.0f, gridCol);
    }

    // Header Title Box: YÜKSELTMELER (compact & sleek at the top)
    float titleBoxW = 380.0f;
    float titleBoxH = 38.0f;
    float titleBoxX = (float)SCREEN_WIDTH * 0.5f - titleBoxW * 0.5f;
    float titleBoxY = 18.0f;

    Sprite_DrawRect(titleBoxX, titleBoxY, titleBoxW, titleBoxH, { 0.14f, 0.09f, 0.18f, 0.80f });
    Sprite_DrawRectBorder(titleBoxX, titleBoxY, titleBoxW, titleBoxH, 1.5f, { 0.95f, 0.88f, 0.78f, 0.80f });

    DrawTextMatrix(titleBoxX + 50.0f, titleBoxY + 9.0f, "YUKSELTMELER", 3.4f, { 0.96f, 0.92f, 0.84f, 1.0f });
}

void UpgradeTree::DrawLeftPanel(const PlayerResources& bank)
{
    float panelX = 40.0f;
    float panelY = 45.0f;
    float panelW = 280.0f;

    // BANKA Header
    DrawTextMatrix(panelX, panelY, "BANKA", 4.5f, { 0.35f, 0.90f, 0.95f, 1.0f });
    Sprite_DrawRect(panelX, panelY + 36.0f, panelW, 2.5f, { 0.35f, 0.90f, 0.95f, 0.6f });

    // Resource rows with real texture visuals!
    float startY = panelY + 55.0f;
    float rowSpacing = 48.0f;

    const int amounts[5] = { bank.reishi, bank.vida, bank.disli, bank.cpu, bank.key };

    for (int i = 0; i < 5; ++i)
    {
        float curY = startY + i * rowSpacing;

        // Draw Resource Icon (real texture / sprite)
        DrawBankIcon(i, panelX + 16.0f, curY + 14.0f, 30.0f);

        // Draw Amount Number
        DrawFormattedNumber(panelX + 65.0f, curY + 4.0f, amounts[i], 5, 20.0f, m_texNumber, { 0.45f, 0.95f, 1.0f, 1.0f });

        // Divider line
        Sprite_DrawRect(panelX, curY + 34.0f, panelW, 1.5f, { 0.20f, 0.50f, 0.60f, 0.4f });
    }

    // ==========================================
    // GÖREVLER (Quests / Missions)
    // ==========================================
    float questY = startY + 5 * rowSpacing + 50.0f;

    DrawTextMatrix(panelX, questY, "GOREVLER", 4.0f, { 0.95f, 0.40f, 0.75f, 1.0f });
    Sprite_DrawRect(panelX, questY + 32.0f, panelW, 2.5f, { 0.95f, 0.40f, 0.75f, 0.6f });

    // Quest Title
    DrawTextMatrix(panelX, questY + 46.0f, m_quest.title.c_str(), 2.2f, { 0.92f, 0.92f, 0.92f, 1.0f });

    // Quest Progress Icon & Value
    DrawBankIcon(0, panelX + 14.0f, questY + 92.0f, 22.0f);
    DrawFormattedNumber(panelX + 45.0f, questY + 82.0f, m_quest.currentAmount, 4, 16.0f, m_texNumber, { 0.95f, 0.85f, 0.75f, 1.0f });
    DrawTextMatrix(panelX + 115.0f, questY + 86.0f, "/", 2.5f, { 0.7f, 0.7f, 0.7f, 1.0f });
    DrawFormattedNumber(panelX + 130.0f, questY + 82.0f, m_quest.targetAmount, 4, 16.0f, m_texNumber, { 0.95f, 0.85f, 0.75f, 1.0f });

    // Glowing Progress Bar
    float barX = panelX;
    float barY = questY + 118.0f;
    float barW = panelW;
    float barH = 14.0f;

    Sprite_DrawRect(barX, barY, barW, barH, { 0.25f, 0.10f, 0.20f, 1.0f });
    Sprite_DrawRectBorder(barX, barY, barW, barH, 1.5f, { 0.85f, 0.35f, 0.65f, 0.8f });

    float pct = std::clamp((float)m_quest.currentAmount / (float)m_quest.targetAmount, 0.0f, 1.0f);
    if (pct > 0.0f)
    {
        Sprite_DrawRect(barX + 2.0f, barY + 2.0f, (barW - 4.0f) * pct, barH - 4.0f, { 0.95f, 0.30f, 0.70f, 1.0f });
        Sprite_DrawRect(barX + (barW - 4.0f) * pct - 2.0f, barY + 2.0f, 4.0f, barH - 4.0f, { 1.0f, 0.9f, 0.95f, 1.0f });
    }

    // Quest Reward Text
    DrawTextMatrix(panelX + panelW - 130.0f, barY + 22.0f, m_quest.rewardText.c_str(), 1.8f, { 0.4f, 1.0f, 0.7f, 1.0f });
}

void UpgradeTree::DrawRightPanel(int currentStage)
{
    float panelX = 1320.0f;
    float panelY = 45.0f;
    float panelW = 250.0f;

    // SEKTÖRLER Header
    DrawTextMatrix(panelX, panelY, "SEKTORLER", 3.8f, { 1.0f, 0.65f, 0.25f, 1.0f });
    Sprite_DrawRect(panelX, panelY + 32.0f, panelW, 2.5f, { 1.0f, 0.65f, 0.25f, 0.6f });

    // Sector List (5 Stages)
    float startY = panelY + 48.0f;
    float cardH = 54.0f;
    float spacing = 62.0f;

    for (int i = 0; i < (int)m_sectors.size(); ++i)
    {
        float cy = startY + i * spacing;
        bool isCurrent = (m_sectors[i].stageNumber == currentStage);
        bool isUnlocked = m_sectors[i].unlocked;
        bool isCompleted = m_sectors[i].completed;

        DirectX::XMFLOAT4 cardBg;
        DirectX::XMFLOAT4 borderCol;

        if (isCurrent)
        {
            float pulse = sinf(m_globalTime * 6.0f) * 0.15f + 0.85f;
            cardBg = DirectX::XMFLOAT4(0.28f, 0.18f, 0.08f, 0.96f);
            borderCol = DirectX::XMFLOAT4(1.0f, 0.85f, 0.25f, pulse);
        }
        else if (isCompleted)
        {
            cardBg = DirectX::XMFLOAT4(0.10f, 0.22f, 0.14f, 0.92f);
            borderCol = DirectX::XMFLOAT4(0.30f, 0.85f, 0.45f, 0.85f);
        }
        else if (isUnlocked)
        {
            cardBg = DirectX::XMFLOAT4(0.14f, 0.10f, 0.18f, 0.90f);
            borderCol = DirectX::XMFLOAT4(0.40f, 0.75f, 0.90f, 0.80f);
        }
        else
        {
            cardBg = DirectX::XMFLOAT4(0.08f, 0.06f, 0.10f, 0.80f);
            borderCol = DirectX::XMFLOAT4(0.30f, 0.25f, 0.32f, 0.50f);
        }

        // Draw Card Background & Border
        Sprite_DrawRect(panelX, cy, panelW, cardH, cardBg);
        Sprite_DrawRectBorder(panelX, cy, panelW, cardH, isCurrent ? 2.5f : 1.5f, borderCol);

        // Number Badge on left
        float badgeW = 34.0f;
        float badgeH = cardH - 12.0f;
        float badgeX = panelX + 6.0f;
        float badgeY = cy + 6.0f;
        Sprite_DrawRect(badgeX, badgeY, badgeW, badgeH, { 0.06f, 0.04f, 0.08f, 0.95f });
        Sprite_DrawRectBorder(badgeX, badgeY, badgeW, badgeH, 1.0f, borderCol);

        if (m_sectors[i].isBoss)
        {
            DrawTextMatrix(badgeX + 6.0f, badgeY + 11.0f, "B", 2.8f, { 1.0f, 0.30f, 0.30f, 1.0f });
        }
        else
        {
            char numBuf[8];
            sprintf_s(numBuf, "%d", m_sectors[i].stageNumber);
            DrawTextMatrix(badgeX + 11.0f, badgeY + 11.0f, numBuf, 2.8f, { 1.0f, 0.90f, 0.40f, 1.0f });
        }

        // Sector Name
        DirectX::XMFLOAT4 titleCol = isUnlocked ? DirectX::XMFLOAT4(0.98f, 0.95f, 0.90f, 1.0f) : DirectX::XMFLOAT4(0.55f, 0.50f, 0.55f, 0.8f);
        DrawTextMatrix(panelX + 48.0f, cy + 10.0f, m_sectors[i].name.c_str(), 1.7f, titleCol);

        // Status Label / Action hint
        if (isCurrent)
        {
            DrawTextMatrix(panelX + 48.0f, cy + 30.0f, "[ AKTIF HEDEF / SECILI ]", 1.6f, { 1.0f, 0.85f, 0.25f, 1.0f });
        }
        else if (isCompleted)
        {
            DrawTextMatrix(panelX + 48.0f, cy + 30.0f, "[ ZAFER KAZANILDI ]", 1.6f, { 0.35f, 0.95f, 0.50f, 1.0f });
        }
        else if (isUnlocked)
        {
            DrawTextMatrix(panelX + 48.0f, cy + 30.0f, "[ TIKLA VE SEC ]", 1.6f, { 0.40f, 0.85f, 1.0f, 1.0f });
        }
        else
        {
            DrawTextMatrix(panelX + 48.0f, cy + 30.0f, "[ KILITLI: 1 ANAHTAR GEREKIR ]", 1.5f, { 0.85f, 0.35f, 0.35f, 0.9f });
        }
    }

    // ==========================================
    // BAŞLAT BUTTON (Launch Button)
    // ==========================================
    float btnX = 1320.0f;
    float btnY = 770.0f;
    float btnW = 250.0f;
    float btnH = 60.0f;

    DirectX::XMFLOAT4 btnBg = m_btnStartHovered
        ? DirectX::XMFLOAT4(0.95f, 0.55f, 0.15f, 0.95f)
        : DirectX::XMFLOAT4(0.18f, 0.12f, 0.15f, 0.90f);

    DirectX::XMFLOAT4 btnBorder = m_btnStartHovered
        ? DirectX::XMFLOAT4(1.0f, 0.90f, 0.40f, 1.0f)
        : DirectX::XMFLOAT4(0.95f, 0.60f, 0.20f, 0.85f);

    float scalePop = 1.0f + m_btnStartHoverAnim * 0.04f;
    float drawW = btnW * scalePop;
    float drawH = btnH * scalePop;
    float drawX = btnX - (drawW - btnW) * 0.5f;
    float drawY = btnY - (drawH - btnH) * 0.5f;

    Sprite_DrawRect(drawX, drawY, drawW, drawH, btnBg);
    Sprite_DrawRectBorder(drawX, drawY, drawW, drawH, 3.0f, btnBorder);

    // Spaceship icon & Text
    if (m_texSpaceship != -1)
    {
        Sprite_Draw(m_texSpaceship, drawX + 18.0f, drawY + drawH * 0.5f - 18.0f, 36.0f, 36.0f,
            0, 0, Texture_GetWidth(m_texSpaceship), Texture_GetHeight(m_texSpaceship),
            0.0f, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f });
    }

    DirectX::XMFLOAT4 textCol = m_btnStartHovered
        ? DirectX::XMFLOAT4(0.1f, 0.05f, 0.05f, 1.0f)
        : DirectX::XMFLOAT4(1.0f, 0.85f, 0.35f, 1.0f);

    DrawTextMatrix(drawX + 75.0f, drawY + drawH * 0.5f - 10.0f, "SEFERI BASLAT", 2.6f, textCol);
}

void UpgradeTree::DrawBranchLines()
{
    for (const auto& node : m_nodes)
    {
        for (int parentId : node.prerequisiteIds)
        {
            const UpgradeNode* parent = GetNodeById(parentId);
            if (!parent) continue;

            bool isUnlockedBranch = (parent->IsUnlocked() && node.IsUnlocked());
            bool isAvailableBranch = (parent->IsUnlocked());

            DirectX::XMFLOAT4 lineCol;
            float thickness = 2.0f;

            if (isUnlockedBranch)
            {
                lineCol = DirectX::XMFLOAT4(0.95f, 0.88f, 0.70f, 0.95f);
                thickness = 3.0f;
            }
            else if (isAvailableBranch)
            {
                float pulse = sinf(m_globalTime * 5.0f) * 0.25f + 0.75f;
                lineCol = DirectX::XMFLOAT4(0.45f, 0.75f, 0.85f, 0.65f * pulse);
                thickness = 2.0f;
            }
            else
            {
                lineCol = DirectX::XMFLOAT4(0.25f, 0.18f, 0.25f, 0.45f);
                thickness = 1.5f;
            }

            Sprite_DrawLine(parent->screenPos.x, parent->screenPos.y,
                            node.screenPos.x, node.screenPos.y,
                            thickness, lineCol);
        }
    }
}

void UpgradeTree::DrawTreeNodes()
{
    for (const auto& node : m_nodes)
    {
        float baseSize = node.isCenterHub ? 52.0f : (node.isCapstone ? 46.0f : m_nodeSize);
        float scale = 1.0f + node.hoverProgress * 0.22f;
        if (node.unlockPulseTimer > 0.0f)
        {
            scale += node.unlockPulseTimer * 0.5f;
        }

        float w = baseSize * scale;
        float h = baseSize * scale;
        float x = node.screenPos.x - w * 0.5f;
        float y = node.screenPos.y - h * 0.5f;

        bool unlocked = node.IsUnlocked();
        bool hasPrereq = HasPrerequisites(node.id);

        DirectX::XMFLOAT4 bgCol;
        DirectX::XMFLOAT4 borderCol;
        DirectX::XMFLOAT4 iconCol;

        if (node.isCenterHub)
        {
            bgCol = DirectX::XMFLOAT4(0.96f, 0.92f, 0.82f, 1.0f);
            borderCol = DirectX::XMFLOAT4(1.0f, 0.85f, 0.40f, 1.0f);
            iconCol = DirectX::XMFLOAT4(0.12f, 0.08f, 0.15f, 1.0f);
        }
        else if (node.isCapstone)
        {
            float pulse = sinf(m_globalTime * 6.0f) * 0.25f + 0.75f;
            if (unlocked)
            {
                bgCol = DirectX::XMFLOAT4(0.98f, 0.85f, 0.25f, 1.0f);
                borderCol = DirectX::XMFLOAT4(1.0f, 0.95f, 0.50f, 1.0f);
                iconCol = DirectX::XMFLOAT4(0.12f, 0.08f, 0.15f, 1.0f);
            }
            else if (hasPrereq)
            {
                bgCol = DirectX::XMFLOAT4(0.28f, 0.18f, 0.08f, 0.95f);
                borderCol = DirectX::XMFLOAT4(1.0f, 0.85f, 0.25f, pulse);
                iconCol = DirectX::XMFLOAT4(1.0f, 0.85f, 0.35f, 1.0f);
            }
            else
            {
                bgCol = DirectX::XMFLOAT4(0.12f, 0.08f, 0.10f, 0.80f);
                borderCol = DirectX::XMFLOAT4(0.40f, 0.30f, 0.20f, 0.60f);
                iconCol = DirectX::XMFLOAT4(0.45f, 0.35f, 0.25f, 0.50f);
            }
        }
        else if (node.isSealed && !node.isSealBroken)
        {
            float pulse = sinf(m_globalTime * 5.0f) * 0.25f + 0.75f;
            bgCol = DirectX::XMFLOAT4(0.25f, 0.08f, 0.14f, 0.95f);
            borderCol = DirectX::XMFLOAT4(0.95f, 0.35f, 0.55f, pulse);
            iconCol = DirectX::XMFLOAT4(1.0f, 0.45f, 0.65f, 1.0f);
        }
        else if (node.isHybrid)
        {
            if (unlocked)
            {
                bgCol = DirectX::XMFLOAT4(0.35f, 0.85f, 0.80f, 0.95f);
                borderCol = DirectX::XMFLOAT4(0.60f, 1.0f, 0.95f, 1.0f);
                iconCol = DirectX::XMFLOAT4(0.08f, 0.15f, 0.15f, 1.0f);
            }
            else if (hasPrereq)
            {
                float pulse = sinf(m_globalTime * 6.0f) * 0.3f + 0.7f;
                bgCol = DirectX::XMFLOAT4(0.12f, 0.18f, 0.24f, 0.95f);
                borderCol = DirectX::XMFLOAT4(0.35f, 0.90f, 1.0f, pulse);
                iconCol = DirectX::XMFLOAT4(0.45f, 0.95f, 1.0f, 0.95f);
            }
            else
            {
                bgCol = DirectX::XMFLOAT4(0.10f, 0.12f, 0.16f, 0.85f);
                borderCol = DirectX::XMFLOAT4(0.25f, 0.30f, 0.35f, 0.60f);
                iconCol = DirectX::XMFLOAT4(0.35f, 0.40f, 0.45f, 0.50f);
            }
        }
        else if (unlocked)
        {
            if (node.IsMaxLevel())
            {
                bgCol = DirectX::XMFLOAT4(0.92f, 0.88f, 0.78f, 1.0f);
                borderCol = DirectX::XMFLOAT4(1.0f, 0.85f, 0.30f, 1.0f);
                iconCol = DirectX::XMFLOAT4(0.12f, 0.08f, 0.15f, 1.0f);
            }
            else
            {
                bgCol = DirectX::XMFLOAT4(0.35f, 0.85f, 0.55f, 0.95f);
                borderCol = DirectX::XMFLOAT4(0.60f, 1.0f, 0.75f, 1.0f);
                iconCol = DirectX::XMFLOAT4(0.08f, 0.15f, 0.10f, 1.0f);
            }
        }
        else if (hasPrereq)
        {
            float pulse = sinf(m_globalTime * 6.0f) * 0.3f + 0.7f;
            bgCol = DirectX::XMFLOAT4(0.18f, 0.12f, 0.22f, 0.95f);
            borderCol = DirectX::XMFLOAT4(0.40f, 0.90f, 1.0f, pulse);
            iconCol = DirectX::XMFLOAT4(0.85f, 0.85f, 0.95f, 0.9f);
        }
        else
        {
            bgCol = DirectX::XMFLOAT4(0.14f, 0.09f, 0.15f, 0.85f);
            borderCol = DirectX::XMFLOAT4(0.28f, 0.18f, 0.25f, 0.60f);
            iconCol = DirectX::XMFLOAT4(0.45f, 0.35f, 0.40f, 0.50f);
        }

        // Draw Node Background & Border
        Sprite_DrawRect(x, y, w, h, bgCol);
        float borderThickness = (node.hoverProgress > 0.1f || node.isCapstone) ? 2.5f : 1.5f;
        Sprite_DrawRectBorder(x, y, w, h, borderThickness, borderCol);

        if (node.hoverProgress > 0.05f || (node.isCapstone && unlocked))
        {
            DirectX::XMFLOAT4 haloCol = borderCol;
            haloCol.w = std::max(0.25f, node.hoverProgress * 0.55f);
            Sprite_DrawRectBorder(x - 3.0f, y - 3.0f, w + 6.0f, h + 6.0f, 1.5f, haloCol);
        }

        // Draw Node Glyph / Icon
        NodeIconType iconType = (node.isSealed && !node.isSealBroken) ? NodeIconType::Lock : (hasPrereq ? node.icon : NodeIconType::Question);
        DrawNodeGlyph(iconType, node.screenPos.x, node.screenPos.y, w * 0.60f, iconCol);

        // Draw Level Pips
        if (!node.isCenterHub && node.maxLevel > 1)
        {
            float pipW = 4.0f;
            float pipH = 3.0f;
            float pipGap = 2.0f;
            float totalPipW = node.maxLevel * pipW + (node.maxLevel - 1) * pipGap;
            float pipStartX = node.screenPos.x - totalPipW * 0.5f;
            float pipY = y + h + 3.0f;

            for (int p = 0; p < node.maxLevel; ++p)
            {
                DirectX::XMFLOAT4 pCol = (p < node.currentLevel)
                    ? DirectX::XMFLOAT4(1.0f, 0.85f, 0.25f, 1.0f)
                    : DirectX::XMFLOAT4(0.25f, 0.20f, 0.25f, 0.7f);

                Sprite_DrawRect(pipStartX + p * (pipW + pipGap), pipY, pipW, pipH, pCol);
            }
        }
    }
}

void UpgradeTree::DrawTooltip(const UpgradeNode* node, const PlayerResources& bank)
{
    float cardW = 430.0f;
    float cardH = 260.0f;

    float cardX = node->screenPos.x + 35.0f;
    float cardY = node->screenPos.y - cardH * 0.5f;

    if (cardX + cardW > (float)SCREEN_WIDTH - 260.0f)
    {
        cardX = node->screenPos.x - cardW - 35.0f;
    }
    if (cardY < 60.0f) cardY = 60.0f;
    if (cardY + cardH > (float)SCREEN_HEIGHT - 60.0f) cardY = (float)SCREEN_HEIGHT - cardH - 60.0f;

    Sprite_DrawRect(cardX, cardY, cardW, cardH, { 0.08f, 0.05f, 0.12f, 0.96f });
    
    DirectX::XMFLOAT4 cardBorderCol = node->isCapstone 
        ? DirectX::XMFLOAT4(1.0f, 0.85f, 0.25f, 0.95f)
        : (node->isSealed && !node->isSealBroken)
            ? DirectX::XMFLOAT4(0.95f, 0.35f, 0.60f, 0.95f)
            : DirectX::XMFLOAT4(0.35f, 0.85f, 0.95f, 0.9f);

    Sprite_DrawRectBorder(cardX, cardY, cardW, cardH, 2.0f, cardBorderCol);

    // Category Tag
    DrawTextMatrix(cardX + 16.0f, cardY + 14.0f, node->categoryName.c_str(), 1.8f, cardBorderCol);

    // Title (wrapped)
    DrawTextMatrixWrapped(cardX + 16.0f, cardY + 30.0f, node->title.c_str(), 2.1f, cardW - 32.0f, { 0.98f, 0.95f, 0.88f, 1.0f }, 16.0f);
    Sprite_DrawRect(cardX + 16.0f, cardY + 60.0f, cardW - 32.0f, 1.0f, { 0.30f, 0.40f, 0.50f, 0.5f });

    // Level info / Special tags
    if (node->isCapstone)
    {
        DrawTextMatrix(cardX + 16.0f, cardY + 68.0f, "[ ⭐ NIHAI PROTOKOL - 1 CAPSTONE LIMITI ]", 1.8f, { 1.0f, 0.85f, 0.25f, 1.0f });
    }
    else if (node->isSealed && !node->isSealBroken)
    {
        DrawTextMatrix(cardX + 16.0f, cardY + 68.0f, "[ 🔒 MUHURLU KADIM TEKNOLOJI ]", 1.8f, { 1.0f, 0.45f, 0.65f, 1.0f });
    }
    else if (!node->isCenterHub)
    {
        char lvlBuf[32];
        sprintf_s(lvlBuf, "SEVIYE: %d / %d", node->currentLevel, node->maxLevel);
        DrawTextMatrix(cardX + 16.0f, cardY + 68.0f, lvlBuf, 2.0f, { 1.0f, 0.85f, 0.30f, 1.0f });
    }

    // Description (wrapped)
    DrawTextMatrixWrapped(cardX + 16.0f, cardY + 88.0f, node->description.c_str(), 1.7f, cardW - 32.0f, { 0.82f, 0.85f, 0.90f, 1.0f }, 15.0f);

    // Effect (wrapped)
    DrawTextMatrixWrapped(cardX + 16.0f, cardY + 138.0f, node->effectFormat.c_str(), 1.9f, cardW - 32.0f, { 0.40f, 1.0f, 0.70f, 1.0f }, 16.0f);

    // Cost section & Status
    float bottomY = cardY + 172.0f;
    Sprite_DrawRect(cardX + 16.0f, bottomY, cardW - 32.0f, 1.0f, { 0.30f, 0.40f, 0.50f, 0.5f });

    if (node->isCenterHub)
    {
        DrawTextMatrix(cardX + 16.0f, bottomY + 14.0f, "[ ANA MERKEZ - AKTIF ]", 1.9f, { 0.95f, 0.85f, 0.40f, 1.0f });
    }
    else if (node->isSealed && !node->isSealBroken)
    {
        DrawTextMatrix(cardX + 16.0f, bottomY + 8.0f, "MUHRU KIRMAK ICIN:", 1.8f, { 1.0f, 0.55f, 0.70f, 1.0f });
        DrawBankIcon(4, cardX + 28.0f, bottomY + 36.0f, 24.0f);
        DrawTextMatrix(cardX + 48.0f, bottomY + 28.0f, "1 KADIM ANAHTAR", 1.9f, { 1.0f, 0.85f, 0.30f, 1.0f });

        if (bank.key >= node->keySealCost)
        {
            DrawTextMatrix(cardX + 16.0f, bottomY + 56.0f, "[ TIKLA VE 1 ANAHTAR ILE MUHRU KIR ]", 2.0f, { 0.35f, 1.0f, 0.60f, 1.0f });
        }
        else
        {
            DrawTextMatrix(cardX + 16.0f, bottomY + 56.0f, "[ YETERSIZ ANAHTAR (1 ANAHTAR GEREKIR) ]", 1.8f, { 0.95f, 0.35f, 0.35f, 1.0f });
        }
    }
    else if (node->isCapstone && m_activeCapstoneId != -1 && m_activeCapstoneId != node->id)
    {
        DrawTextMatrix(cardX + 16.0f, bottomY + 14.0f, "[ 🔒 CEKIRDEK HIZALANDI - 1 CAPSTONE LIMITI ]", 1.8f, { 1.0f, 0.40f, 0.35f, 1.0f });
    }
    else if (node->IsMaxLevel())
    {
        DrawTextMatrix(cardX + 16.0f, bottomY + 14.0f, "[ MAKSIMUM SEVIYEYE ULASILDI ]", 2.0f, { 1.0f, 0.85f, 0.30f, 1.0f });
    }
    else if (!HasPrerequisites(node->id))
    {
        DrawTextMatrix(cardX + 16.0f, bottomY + 14.0f, "[ KILITLI - ONCEKI DALLARI ACIN ]", 2.0f, { 0.95f, 0.35f, 0.35f, 1.0f });
    }
    else
    {
        int nextLvl = node->currentLevel;
        const ResourceCost& cost = node->levelCosts[nextLvl];
        bool canAfford = CanUnlockNode(node->id, bank);

        DrawTextMatrix(cardX + 16.0f, bottomY + 8.0f, "GEREKLI KAYNAKLAR:", 1.8f, { 0.95f, 0.85f, 0.70f, 1.0f });

        float costItemX = cardX + 16.0f;
        float costItemY = bottomY + 28.0f;

        struct CostEntry { int type; int amount; int bankAmount; std::string label; };
        std::vector<CostEntry> requiredItems;

        if (cost.reishi > 0) requiredItems.push_back({ 0, cost.reishi, bank.reishi, "Reishi" });
        if (cost.vida > 0)   requiredItems.push_back({ 1, cost.vida, bank.vida, "Vida" });
        if (cost.disli > 0)  requiredItems.push_back({ 2, cost.disli, bank.disli, "Disli" });
        if (cost.cpu > 0)    requiredItems.push_back({ 3, cost.cpu, bank.cpu, "CPU" });
        if (cost.key > 0)    requiredItems.push_back({ 4, cost.key, bank.key, "Anahtar" });

        for (const auto& item : requiredItems)
        {
            // Draw real image icon with card backing
            DrawBankIcon(item.type, costItemX + 11.0f, costItemY + 9.0f, 22.0f);

            // Amount text
            char amtBuf[32];
            sprintf_s(amtBuf, "%d", item.amount);

            bool hasEnough = (item.bankAmount >= item.amount);
            DirectX::XMFLOAT4 amtCol = hasEnough
                ? DirectX::XMFLOAT4(0.35f, 0.95f, 0.55f, 1.0f) // Green
                : DirectX::XMFLOAT4(0.95f, 0.35f, 0.35f, 1.0f); // Red

            DrawTextMatrix(costItemX + 26.0f, costItemY + 3.0f, amtBuf, 1.9f, amtCol);

            costItemX += 26.0f + (float)strlen(amtBuf) * 8.0f + 16.0f;
        }

        if (canAfford)
        {
            DrawTextMatrix(cardX + 16.0f, bottomY + 54.0f, "[ TIKLA VE YUKSELT ]", 2.1f, { 0.30f, 1.0f, 0.60f, 1.0f });
        }
        else
        {
            DrawTextMatrix(cardX + 16.0f, bottomY + 54.0f, "[ YETERSIZ KAYNAK ]", 2.1f, { 0.95f, 0.35f, 0.35f, 1.0f });
        }
    }
}

void UpgradeTree::DrawBankResourceTooltip(int index)
{
    struct ResInfo
    {
        const char* name;
        const char* typeTag;
        const char* desc;
        const char* source;
        DirectX::XMFLOAT4 color;
    };

    const ResInfo info[5] = {
        {
            "REISHI MADEN KRISTALI",
            "[ TEMEL PARA BIRIMI ]",
            "Gemi gelistirmelerinde ve altyapi sistemlerinde kullanilan ana enerji cevheri.",
            "KAYNAK: Tum asteroitlerin kirilmasindan ve dusmanlardan elde edilir.",
            { 0.4f, 1.0f, 0.7f, 1.0f }
        },
        {
            "VIDA VE CIVATA PARCASI",
            "[ HAFIF HURDA METAL ]",
            "Mekanik alt sistemleri ve silah donanimlarini guclendiren vida bilesenleri.",
            "KAYNAK: Dusman dronlarindan ve kucuk asteroitlerden duser.",
            { 0.45f, 0.85f, 0.95f, 1.0f }
        },
        {
            "DISLI VE MEKANIK GUC BIRIMI",
            "[ AGIR SANAYI PARCASI ]",
            "Motor, taret ve agir savunma zirhi yapiminda kullanilan dayanikli celik disli.",
            "KAYNAK: Elit asteroitlerden ve Sektor Boss zaferlerinden cikar.",
            { 0.95f, 0.65f, 0.25f, 1.0f }
        },
        {
            "KUANTUM ISLEMCI (CPU)",
            "[ NADIR MIKROCIP ]",
            "Yuksek teknolojili aktif yetenekler ve otonom taret zekasi icin gerekli islemci.",
            "KAYNAK: Sektor Boss'larindan nadir olarak duser.",
            { 0.35f, 0.95f, 0.85f, 1.0f }
        },
        {
            "KADIM TEKNOLOJI ANAHTARI",
            "[ PRESTIJ ERISIM BILETI ]",
            "Agactaki Muhurlu Kadim Teknolojileri ve uzaydaki Kadim Sandiklari acar.",
            "KAYNAK: Sektor Boss zaferleri ve Anomali Asteroidi avindan kazanilir.",
            { 1.0f, 0.85f, 0.30f, 1.0f }
        }
    };

    if (index < 0 || index >= 5) return;
    const auto& res = info[index];

    int mouseX = InputMouse_GetX();
    int mouseY = InputMouse_GetY();

    float cardW = 390.0f;
    float cardH = 175.0f;
    float cardX = (float)mouseX + 25.0f;
    float cardY = (float)mouseY - cardH * 0.5f;

    if (cardX + cardW > (float)SCREEN_WIDTH - 20.0f) cardX = (float)SCREEN_WIDTH - cardW - 20.0f;
    if (cardY < 60.0f) cardY = 60.0f;
    if (cardY + cardH > (float)SCREEN_HEIGHT - 60.0f) cardY = (float)SCREEN_HEIGHT - cardH - 60.0f;

    Sprite_DrawRect(cardX, cardY, cardW, cardH, { 0.08f, 0.05f, 0.12f, 0.96f });
    Sprite_DrawRectBorder(cardX, cardY, cardW, cardH, 2.0f, res.color);

    // Icon + Tag + Name
    DrawBankIcon(index, cardX + 28.0f, cardY + 28.0f, 32.0f);
    DrawTextMatrix(cardX + 54.0f, cardY + 14.0f, res.typeTag, 1.7f, res.color);
    DrawTextMatrix(cardX + 54.0f, cardY + 28.0f, res.name, 2.0f, { 0.98f, 0.95f, 0.88f, 1.0f });

    Sprite_DrawRect(cardX + 16.0f, cardY + 52.0f, cardW - 32.0f, 1.0f, { 0.30f, 0.40f, 0.50f, 0.5f });

    // Description (wrapped)
    DrawTextMatrixWrapped(cardX + 16.0f, cardY + 62.0f, res.desc, 1.7f, cardW - 32.0f, { 0.85f, 0.85f, 0.90f, 1.0f }, 15.0f);

    // Source (wrapped)
    DrawTextMatrixWrapped(cardX + 16.0f, cardY + 118.0f, res.source, 1.7f, cardW - 32.0f, { 1.0f, 0.85f, 0.30f, 1.0f }, 14.0f);
}

void UpgradeTree::DrawNodeGlyph(NodeIconType icon, float cx, float cy, float size, const DirectX::XMFLOAT4& col)
{
    float half = size * 0.5f;

    switch (icon)
    {
        case NodeIconType::CoreHub:
        {
            Sprite_DrawRect(cx - half * 0.7f, cy - half * 0.7f, half * 1.4f, half * 1.4f, col);
            Sprite_DrawRect(cx - half, cy - 2.0f, size, 4.0f, col);
            Sprite_DrawRect(cx - 2.0f, cy - half, 4.0f, size, col);
            break;
        }
        case NodeIconType::LaserBeam:
        {
            Sprite_DrawRect(cx - half * 0.8f, cy - 2.0f, size * 0.8f, 4.0f, col);
            Sprite_DrawRect(cx + half * 0.4f, cy - half * 0.5f, 3.0f, size * 0.5f, col);
            break;
        }
        case NodeIconType::Sword:
        {
            Sprite_DrawLine(cx - half * 0.7f, cy - half * 0.7f, cx + half * 0.7f, cy + half * 0.7f, 3.0f, col);
            Sprite_DrawLine(cx - half * 0.4f, cy - half * 0.1f, cx - half * 0.1f, cy - half * 0.4f, 3.0f, col);
            break;
        }
        case NodeIconType::Fire:
        {
            Sprite_DrawRect(cx - 3.0f, cy - half * 0.8f, 6.0f, size * 0.8f, col);
            Sprite_DrawRect(cx - half * 0.5f, cy, half * 1.0f, 6.0f, col);
            break;
        }
        case NodeIconType::Explosion:
        {
            Sprite_DrawLine(cx - half, cy, cx + half, cy, 2.5f, col);
            Sprite_DrawLine(cx, cy - half, cx, cy + half, 2.5f, col);
            Sprite_DrawLine(cx - half * 0.7f, cy - half * 0.7f, cx + half * 0.7f, cy + half * 0.7f, 2.0f, col);
            Sprite_DrawLine(cx - half * 0.7f, cy + half * 0.7f, cx + half * 0.7f, cy - half * 0.7f, 2.0f, col);
            break;
        }
        case NodeIconType::Crosshair:
        {
            Sprite_DrawRectBorder(cx - half * 0.7f, cy - half * 0.7f, size * 0.7f, size * 0.7f, 2.0f, col);
            Sprite_DrawRect(cx - 2.0f, cy - 2.0f, 4.0f, 4.0f, col);
            break;
        }
        case NodeIconType::Lightning:
        {
            Sprite_DrawLine(cx - 3.0f, cy - half * 0.8f, cx + 3.0f, cy, 2.5f, col);
            Sprite_DrawLine(cx + 3.0f, cy, cx - 2.0f, cy + 2.0f, 2.5f, col);
            Sprite_DrawLine(cx - 2.0f, cy + 2.0f, cx + 4.0f, cy + half * 0.8f, 2.5f, col);
            break;
        }
        case NodeIconType::Magnet:
        {
            Sprite_DrawRect(cx - half * 0.7f, cy - half * 0.6f, 4.0f, size * 0.6f, col);
            Sprite_DrawRect(cx + half * 0.7f - 4.0f, cy - half * 0.6f, 4.0f, size * 0.6f, col);
            Sprite_DrawRect(cx - half * 0.7f, cy + half * 0.6f - 4.0f, size * 0.7f, 4.0f, col);
            break;
        }
        case NodeIconType::Scanner:
        case NodeIconType::Radar:
        {
            Sprite_DrawRectBorder(cx - half * 0.8f, cy - half * 0.8f, size * 0.8f, size * 0.8f, 1.5f, col);
            Sprite_DrawRectBorder(cx - half * 0.4f, cy - half * 0.4f, size * 0.4f, size * 0.4f, 1.5f, col);
            break;
        }
        case NodeIconType::Crystal:
        {
            Sprite_DrawLine(cx, cy - half * 0.8f, cx + half * 0.7f, cy, 2.0f, col);
            Sprite_DrawLine(cx + half * 0.7f, cy, cx, cy + half * 0.8f, 2.0f, col);
            Sprite_DrawLine(cx, cy + half * 0.8f, cx - half * 0.7f, cy, 2.0f, col);
            Sprite_DrawLine(cx - half * 0.7f, cy, cx, cy - half * 0.8f, 2.0f, col);
            break;
        }
        case NodeIconType::ChartUp:
        {
            Sprite_DrawRect(cx - half * 0.6f, cy + half * 0.2f, 3.0f, half * 0.5f, col);
            Sprite_DrawRect(cx - half * 0.1f, cy - half * 0.1f, 3.0f, half * 0.8f, col);
            Sprite_DrawRect(cx + half * 0.4f, cy - half * 0.6f, 3.0f, half * 1.3f, col);
            break;
        }
        case NodeIconType::Thruster:
        {
            Sprite_DrawLine(cx, cy - half * 0.8f, cx + half * 0.6f, cy + half * 0.6f, 2.0f, col);
            Sprite_DrawLine(cx, cy - half * 0.8f, cx - half * 0.6f, cy + half * 0.6f, 2.0f, col);
            Sprite_DrawRect(cx - half * 0.4f, cy + half * 0.4f, size * 0.4f, 3.0f, col);
            break;
        }
        case NodeIconType::Chevrons:
        {
            Sprite_DrawLine(cx - half * 0.5f, cy - half * 0.5f, cx, cy, 2.5f, col);
            Sprite_DrawLine(cx, cy, cx - half * 0.5f, cy + half * 0.5f, 2.5f, col);
            Sprite_DrawLine(cx, cy - half * 0.5f, cx + half * 0.5f, cy, 2.5f, col);
            Sprite_DrawLine(cx + half * 0.5f, cy, cx, cy + half * 0.5f, 2.5f, col);
            break;
        }
        case NodeIconType::Battery:
        {
            Sprite_DrawRectBorder(cx - half * 0.5f, cy - half * 0.6f, size * 0.5f, size * 0.7f, 2.0f, col);
            Sprite_DrawRect(cx - 3.0f, cy - half * 0.8f, 6.0f, 3.0f, col);
            break;
        }
        case NodeIconType::Gauge:
        {
            Sprite_DrawRectBorder(cx - half * 0.7f, cy - half * 0.4f, size * 0.7f, size * 0.7f, 2.0f, col);
            Sprite_DrawLine(cx, cy + half * 0.2f, cx + half * 0.4f, cy - half * 0.3f, 2.5f, col);
            break;
        }
        case NodeIconType::Shield:
        case NodeIconType::Barrier:
        {
            Sprite_DrawLine(cx - half * 0.7f, cy - half * 0.7f, cx + half * 0.7f, cy - half * 0.7f, 2.0f, col);
            Sprite_DrawLine(cx + half * 0.7f, cy - half * 0.7f, cx + half * 0.7f, cy, 2.0f, col);
            Sprite_DrawLine(cx + half * 0.7f, cy, cx, cy + half * 0.8f, 2.0f, col);
            Sprite_DrawLine(cx, cy + half * 0.8f, cx - half * 0.7f, cy, 2.0f, col);
            Sprite_DrawLine(cx - half * 0.7f, cy, cx - half * 0.7f, cy - half * 0.7f, 2.0f, col);
            break;
        }
        case NodeIconType::Heart:
        {
            Sprite_DrawRect(cx - half * 0.6f, cy - half * 0.5f, half * 0.5f, half * 0.5f, col);
            Sprite_DrawRect(cx + half * 0.1f, cy - half * 0.5f, half * 0.5f, half * 0.5f, col);
            Sprite_DrawLine(cx - half * 0.6f, cy, cx, cy + half * 0.6f, 2.0f, col);
            Sprite_DrawLine(cx + half * 0.6f, cy, cx, cy + half * 0.6f, 2.0f, col);
            break;
        }
        case NodeIconType::Turret:
        {
            Sprite_DrawRectBorder(cx - half * 0.6f, cy - half * 0.6f, size * 0.6f, size * 0.6f, 2.0f, col);
            Sprite_DrawRect(cx - 2.0f, cy - half * 0.9f, 4.0f, size * 0.5f, col);
            Sprite_DrawRect(cx - 3.0f, cy - 3.0f, 6.0f, 6.0f, col);
            break;
        }
        case NodeIconType::Shockwave:
        {
            Sprite_DrawRectBorder(cx - half * 0.8f, cy - half * 0.8f, size * 0.8f, size * 0.8f, 1.5f, col);
            Sprite_DrawRectBorder(cx - half * 0.4f, cy - half * 0.4f, size * 0.4f, size * 0.4f, 1.5f, col);
            Sprite_DrawLine(cx - half, cy, cx + half, cy, 1.5f, col);
            Sprite_DrawLine(cx, cy - half, cx, cy + half, 1.5f, col);
            break;
        }
        case NodeIconType::Lock:
        {
            Sprite_DrawRect(cx - half * 0.6f, cy - half * 0.2f, size * 0.6f, size * 0.6f, col);
            Sprite_DrawRectBorder(cx - half * 0.4f, cy - half * 0.7f, size * 0.4f, size * 0.5f, 2.0f, col);
            Sprite_DrawRect(cx - 1.5f, cy + 1.0f, 3.0f, 5.0f, { 0.08f, 0.05f, 0.10f, 1.0f });
            break;
        }
        case NodeIconType::Star:
        {
            Sprite_DrawLine(cx, cy - half * 0.85f, cx, cy + half * 0.85f, 2.5f, col);
            Sprite_DrawLine(cx - half * 0.85f, cy, cx + half * 0.85f, cy, 2.5f, col);
            Sprite_DrawLine(cx - half * 0.55f, cy - half * 0.55f, cx + half * 0.55f, cy + half * 0.55f, 2.0f, col);
            Sprite_DrawLine(cx - half * 0.55f, cy + half * 0.55f, cx + half * 0.55f, cy - half * 0.55f, 2.0f, col);
            break;
        }
        case NodeIconType::Chest:
        {
            Sprite_DrawRectBorder(cx - half * 0.7f, cy - half * 0.5f, size * 0.7f, size * 0.7f, 2.0f, col);
            Sprite_DrawLine(cx - half * 0.7f, cy - half * 0.1f, cx + half * 0.7f, cy - half * 0.1f, 2.0f, col);
            Sprite_DrawRect(cx - 2.0f, cy - 2.0f, 4.0f, 5.0f, col);
            break;
        }
        case NodeIconType::Hybrid:
        {
            Sprite_DrawRectBorder(cx - half * 0.5f, cy - half * 0.5f, size * 0.5f, size * 0.5f, 1.5f, col);
            Sprite_DrawLine(cx - half * 0.7f, cy - half * 0.7f, cx + half * 0.7f, cy + half * 0.7f, 2.0f, col);
            Sprite_DrawLine(cx - half * 0.7f, cy + half * 0.7f, cx + half * 0.7f, cy - half * 0.7f, 2.0f, col);
            break;
        }
        case NodeIconType::SkillEmp:
        {
            DrawTextMatrix(cx - 10.0f, cy - 8.0f, "Q", 2.2f, col);
            Sprite_DrawRectBorder(cx - half * 0.7f, cy - half * 0.7f, size * 0.7f, size * 0.7f, 1.5f, col);
            break;
        }
        case NodeIconType::SkillDash:
        {
            Sprite_DrawLine(cx - half * 0.7f, cy, cx + half * 0.5f, cy, 2.5f, col);
            Sprite_DrawLine(cx + half * 0.2f, cy - half * 0.4f, cx + half * 0.6f, cy, 2.0f, col);
            Sprite_DrawLine(cx + half * 0.2f, cy + half * 0.4f, cx + half * 0.6f, cy, 2.0f, col);
            break;
        }
        case NodeIconType::SkillOvercharge:
        {
            DrawTextMatrix(cx - 10.0f, cy - 8.0f, "E", 2.2f, col);
            Sprite_DrawLine(cx, cy - half * 0.7f, cx + half * 0.4f, cy + half * 0.6f, 2.0f, col);
            break;
        }
        case NodeIconType::Question:
        default:
        {
            DrawTextMatrix(cx - 3.0f, cy - 6.0f, "?", 2.8f, col);
            break;
        }
    }
}

void UpgradeTree::DrawBankIcon(int type, float x, float y, float size)
{
    float half = size * 0.5f;

    // Card background for icon
    Sprite_DrawRect(x - half, y - half, size, size, { 0.12f, 0.08f, 0.16f, 0.95f });
    Sprite_DrawRectBorder(x - half, y - half, size, size, 1.5f, { 0.35f, 0.75f, 0.85f, 0.6f });

    float iconInnerSize = size * 0.75f;
    float iconX = x - iconInnerSize * 0.5f;
    float iconY = y - iconInnerSize * 0.5f;

    switch (type)
    {
        case 0: // Reishi Crystal / Coin (resources_no_bg.png)
        {
            if (m_texResources != -1)
            {
                Sprite_Draw(m_texResources, iconX, iconY, iconInnerSize, iconInnerSize,
                    0, 0, 256, 341, { 1.0f, 1.0f, 1.0f, 1.0f });
            }
            else
            {
                DirectX::XMFLOAT4 coinCol{ 0.95f, 0.75f, 0.35f, 1.0f };
                Sprite_DrawRect(x - half * 0.6f, y - half * 0.6f, size * 0.6f, size * 0.6f, coinCol);
            }
            break;
        }
        case 1: // Screw / Bolt (vida.png)
        {
            if (m_texVida != -1)
            {
                Sprite_Draw(m_texVida, iconX, iconY, iconInnerSize, iconInnerSize,
                    0, 0, Texture_GetWidth(m_texVida), Texture_GetHeight(m_texVida),
                    { 1.0f, 1.0f, 1.0f, 1.0f });
            }
            else
            {
                DirectX::XMFLOAT4 boltCol{ 0.45f, 0.85f, 0.95f, 1.0f };
                Sprite_DrawLine(x - half * 0.6f, y - half * 0.6f, x + half * 0.6f, y + half * 0.6f, 3.5f, boltCol);
            }
            break;
        }
        case 2: // Gear / Mechanism (disli.png)
        {
            if (m_texDisli != -1)
            {
                Sprite_Draw(m_texDisli, iconX, iconY, iconInnerSize, iconInnerSize,
                    0, 0, Texture_GetWidth(m_texDisli), Texture_GetHeight(m_texDisli),
                    { 1.0f, 1.0f, 1.0f, 1.0f });
            }
            else
            {
                DirectX::XMFLOAT4 gearCol{ 0.95f, 0.65f, 0.25f, 1.0f };
                Sprite_DrawRect(x - half * 0.5f, y - half * 0.5f, size * 0.5f, size * 0.5f, gearCol);
            }
            break;
        }
        case 3: // Quantum CPU / Chip (cpu.png)
        {
            if (m_texCpu != -1)
            {
                Sprite_Draw(m_texCpu, iconX, iconY, iconInnerSize, iconInnerSize,
                    0, 0, Texture_GetWidth(m_texCpu), Texture_GetHeight(m_texCpu),
                    { 1.0f, 1.0f, 1.0f, 1.0f });
            }
            else
            {
                DirectX::XMFLOAT4 cpuCol{ 0.35f, 0.95f, 0.85f, 1.0f };
                Sprite_DrawRectBorder(x - half * 0.6f, y - half * 0.6f, size * 0.6f, size * 0.6f, 2.0f, cpuCol);
            }
            break;
        }
        case 4: // Sector Key (key.png)
        {
            if (m_texKey != -1)
            {
                Sprite_Draw(m_texKey, iconX, iconY, iconInnerSize, iconInnerSize,
                    0, 0, Texture_GetWidth(m_texKey), Texture_GetHeight(m_texKey),
                    { 1.0f, 1.0f, 1.0f, 1.0f });
            }
            else
            {
                DirectX::XMFLOAT4 keyCol{ 1.0f, 0.85f, 0.30f, 1.0f };
                Sprite_DrawLine(x - half * 0.5f, y, x + half * 0.5f, y, 3.0f, keyCol);
                Sprite_DrawRect(x + half * 0.2f, y, 4.0f, 6.0f, keyCol);
            }
            break;
        }
        default: break;
    }
}
