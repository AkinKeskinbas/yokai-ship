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
                            int texVida, int texDisli, int texCpu, int texKey, int soundClick,
                            int texSkillDash, int texSkillWave, int texSkillBuff)
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
    m_soundClick = soundClick;
    m_texSkillDash = texSkillDash;
    m_texSkillWave = texSkillWave;
    m_texSkillBuff = texSkillBuff;

    SetupNodes();
    UpdateLayout();

    // Setup 5 Sectors
    m_sectors.clear();
    m_sectors.push_back({ 1, "CRYOGENIC FIELDS", false, true, false });
    m_sectors.push_back({ 2, "ASTEROID BELT", false, false, false });
    m_sectors.push_back({ 3, "NEBULA PASSAGE", false, false, false });
    m_sectors.push_back({ 4, "PLASMA STORM", false, false, false });
    m_sectors.push_back({ 5, "CALAMITY CORE (BOSS RUSH)", true, false, false });
    m_currentSectorIndex = 1;
}

void UpgradeTree::UnlockNextSector(int completedSector)
{
    for (int i = 0; i < (int)m_sectors.size(); ++i)
    {
        if (m_sectors[i].stageNumber == completedSector)
        {
            m_sectors[i].completed = true;
        }
        if (m_sectors[i].stageNumber == completedSector + 1)
        {
            m_sectors[i].unlocked = true;
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
        n.title = "COMMAND CORE HUB";
        n.categoryName = "CORE MODULE";
        n.description = "Central quantum mainframe powering and connecting all ship subsystems.";
        n.effectFormat = "Command Mainframe Online";
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
        n.title = "LASER OVERCLOCK I";
        n.categoryName = "WEAPONS / POWER";
        n.description = "Boosts laser core frequency to increase base DPS damage.";
        n.effectFormat = "+15 Laser Damage";
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
        n.title = "THERMAL MELTER II";
        n.categoryName = "WEAPONS / POWER";
        n.description = "Fires high-intensity thermal plasma to dissolve tough asteroid crusts.";
        n.effectFormat = "+30 Laser Damage";
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
        n.title = "PIERCING BEAM";
        n.categoryName = "WEAPONS / POWER";
        n.description = "Laser drills directly through the primary target to hit secondary targets behind.";
        n.effectFormat = "Piercing Beam: Dual Target Hit";
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
        n.title = "OVERHEAT PROTOCOL";
        n.categoryName = "WEAPONS / POWER";
        n.description = "Continuous beam focus on a single target for 1.2s doubles damage output.";
        n.effectFormat = "+100% Sustained Focus Damage";
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
        n.title = "CORE MELTDOWN";
        n.categoryName = "WEAPONS / POWER";
        n.description = "Destroyed asteroids and enemies detonate into an explosive thermal shockwave.";
        n.effectFormat = "AoE Thermal Blast on Kill";
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
        n.title = "DUAL PLASMA BEAM";
        n.categoryName = "WEAPONS / MULTI-BEAM";
        n.description = "Adds a secondary laser emitter locking onto 2 simultaneous targets.";
        n.effectFormat = "+1 Extra Concurrent Laser (2 Total)";
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
        n.title = "TRI-PLASMA BEAM";
        n.categoryName = "WEAPONS / MULTI-BEAM";
        n.description = "Triple optical channels allowing simultaneous engagement of 3 targets.";
        n.effectFormat = "+1 Extra Concurrent Laser (3 Total)";
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
        n.title = "CHAIN ARC DISCHARGE";
        n.categoryName = "WEAPONS / MULTI-BEAM";
        n.description = "Laser arcs from primary target to nearby enemies or asteroids as lightning.";
        n.effectFormat = "Arcing Electrical Laser Discharge";
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
        n.title = "PRISMATIC SPLIT";
        n.categoryName = "WEAPONS / MULTI-BEAM";
        n.description = "Shattering large asteroids splits the laser beam into twin rays for a short duration.";
        n.effectFormat = "Twin Laser Split on Asteroid Kill";
        n.branch = NodeBranch::North_Weapon;
        n.icon = NodeIconType::Crosshair;
        n.gridPos = { 1.1f, -4.6f };
        n.maxLevel = 1;
        n.levelCosts = { { 230, 26, 8, 2, 1 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 8 };
        m_nodes.push_back(n);
    }

    // Sub-branch 3: CRIT & CRYSTAL FOCUS
    {
        UpgradeNode n;
        n.id = 10;
        n.title = "CRITICAL FOCUS I";
        n.categoryName = "WEAPONS / CRITICAL";
        n.description = "Tunes laser frequency to trigger high-yield critical strikes.";
        n.effectFormat = "+15% Critical Chance";
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
        n.title = "CRITICAL MULTIPLIER II";
        n.categoryName = "WEAPONS / CRITICAL";
        n.description = "Amplifies critical strike damage to triple base intensity.";
        n.effectFormat = "+100% Critical Damage Multiplier";
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
        n.title = "WEAKPOINT RESONATOR";
        n.categoryName = "WEAPONS / CRITICAL";
        n.description = "Illuminates glowing weakpoints on asteroids for massive critical bursts.";
        n.effectFormat = "220% Critical Burst on Weakpoints";
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
        n.title = "ANCIENT SINGULARITY BEAM";
        n.categoryName = "LOST TECH";
        n.description = "Dark matter augmented alien beam that ignores armor and shielding.";
        n.effectFormat = "+80 Laser Damage & Pierce";
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
        n.categoryName = "CAPSTONE / ULTIMATE";
        n.description = "Transmutes lasers into a cataclysmic orbital plasma beam with infinite penetration.";
        n.effectFormat = "+100 Laser Damage & Infinite Pierce";
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
        n.title = "MINERAL RADAR I";
        n.categoryName = "MINING / SENSORS";
        n.description = "Boosts mineral resonance scanning to increase Reishi crystal yield.";
        n.effectFormat = "+25% Reishi Crystal Yield";
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
        n.title = "ORE VISION";
        n.categoryName = "MINING / SENSORS";
        n.description = "Reveals internal mineral deposits (Reishi, Screws, Gears, CPU) before mining.";
        n.effectFormat = "Shows Mineral Content Before Mining";
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
        n.title = "DEEP SPECTRAL SCAN";
        n.categoryName = "MINING / SENSORS";
        n.description = "Generates a shimmering glowing aura around asteroids carrying rare materials.";
        n.effectFormat = "Rare Ore Glowing Aura";
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
        n.title = "TREASURE SIGNAL RADAR";
        n.categoryName = "MINING / SENSORS";
        n.description = "HUD alerts indicate directional sonar vector towards Ancient Keys and rich veins.";
        n.effectFormat = "Anomaly & Key Asteroid Sonar";
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
        n.title = "JACKPOT VEIN";
        n.categoryName = "MINING / SENSORS";
        n.description = "Mining rare asteroids has a 20% chance to trigger a massive 5x loot explosion.";
        n.effectFormat = "20% Chance for 5x Mega Jackpot";
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
        n.title = "MAGNETIC TETHER I";
        n.categoryName = "MINING / ATTRACTION";
        n.description = "Generates a magnetic vacuum field to attract crystals and materials.";
        n.effectFormat = "+60 Unit Vacuum Pickup Radius";
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
        n.title = "HYPER VACUUM II";
        n.categoryName = "MINING / ATTRACTION";
        n.description = "High-potency vortex pulling floating debris rapidly into the ship.";
        n.effectFormat = "+100 Unit Extra Vacuum Range";
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
        n.title = "RESOURCE ORBIT";
        n.categoryName = "MINING / ATTRACTION";
        n.description = "Collected minerals swirl around the ship and absorb with +35% bonus yield.";
        n.effectFormat = "Orbiting Mineral Ring & +35% Yield";
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
        n.title = "INSTANT QUANTUM SIPHON";
        n.categoryName = "MINING / ATTRACTION";
        n.description = "Floating minerals within range instantly teleport into ship storage.";
        n.effectFormat = "Instant Mineral Cargo Teleport";
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
        n.title = "CHAIN FRACTURE";
        n.categoryName = "MINING / EXTRACTION";
        n.description = "Shattering large asteroids creates a kinetic shockwave fracturing nearby rocks.";
        n.effectFormat = "Kinetic Blast Fractures Nearby Rocks";
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
        n.title = "TITANIUM SCREW EXTRACTOR";
        n.categoryName = "MINING / EXTRACTION";
        n.description = "Increases drop chance of Screws from asteroids and enemy units.";
        n.effectFormat = "+50% Screw Drop Rate";
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
        n.title = "ALLOY GEAR HARVESTER";
        n.categoryName = "MINING / EXTRACTION";
        n.description = "Increases drop chance of Gears from elite asteroids and enemies.";
        n.effectFormat = "+40% Gear Drop Rate";
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
        n.title = "QUANTUM CPU SIFTER";
        n.categoryName = "MINING / EXTRACTION";
        n.description = "Increases drop chance of Quantum CPUs from bosses and elite targets.";
        n.effectFormat = "+35% CPU Drop Rate";
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
        n.title = "ANCIENT TRANSMUTATION CORE";
        n.categoryName = "LOST TECH";
        n.description = "Alien transmutation matrix massively multiplying all expedition resources.";
        n.effectFormat = "+80% Bonus to All Resources";
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
        n.categoryName = "CAPSTONE / ULTIMATE";
        n.description = "Triples all resource extraction and triggers continuous mega Jackpot explosions.";
        n.effectFormat = "+250% Resource Yield & Constant Jackpots";
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
        n.title = "THRUSTER TUNING I";
        n.categoryName = "ENGINE / SPEED";
        n.description = "Increases primary thruster output and ship maneuvering response.";
        n.effectFormat = "+30 Unit Flight Speed";
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
        n.title = "AFTERBURNER TRAIL";
        n.categoryName = "ENGINE / SPEED";
        n.description = "Leaves a glowing plasma thrust trail and boosts cruise speed.";
        n.effectFormat = "+25 Speed & Plasma Trail";
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
        n.title = "MOMENTUM CRUISE";
        n.categoryName = "ENGINE / SPEED";
        n.description = "Flying in a straight trajectory for 3 seconds grants +25% momentum speed.";
        n.effectFormat = "+25% Momentum Speed on Cruise";
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
        n.title = "GRAVITY SLINGSHOT";
        n.categoryName = "ENGINE / SPEED";
        n.description = "Skimming close past asteroids triggers an instantaneous kinetic slingshot boost.";
        n.effectFormat = "Slingshot Velocity Boost Near Rocks";
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
        n.title = "PHASE MANEUVER";
        n.categoryName = "ENGINE / DASH";
        n.description = "Reduces Phase Dash cooldown for rapid evasive repositioning.";
        n.effectFormat = "-20% Phase Dash Cooldown";
        n.branch = NodeBranch::South_Engine;
        n.icon = NodeIconType::Chevrons;
        n.gridPos = { -1.1f, 1.6f };
        n.maxLevel = 2;
        n.levelCosts = { { 50, 5, 1, 0, 0 }, { 90, 10, 2, 0, 0 } };
        n.levelValues = { 0.20f, 0.40f };
        n.prerequisiteIds = { 58 }; // Requires Phase Dash active skill first!
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 33;
        n.title = "GHOST DASH";
        n.categoryName = "ENGINE / DASH";
        n.description = "Ship becomes completely intangible during dash, phasing through bullets unharmed.";
        n.effectFormat = "Invulnerable Phasing Through Bullets";
        n.branch = NodeBranch::South_Engine;
        n.icon = NodeIconType::Thruster;
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
        n.title = "IMPACT DASH";
        n.categoryName = "ENGINE / DASH";
        n.description = "Ramming enemies while dashing delivers a massive kinetic strike (120 DMG).";
        n.effectFormat = "120 Kinetic Damage on Dash Contact";
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
        n.title = "MINING DASH";
        n.categoryName = "ENGINE / DASH";
        n.description = "Dashing into small and medium asteroids shatters them instantly on contact.";
        n.effectFormat = "Instantly Shatters Asteroids on Contact";
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
        n.title = "EXPANDED FUEL CELL";
        n.categoryName = "ENGINE / ENERGY";
        n.description = "Expands maximum voyage energy storage for longer deep-space expeditions.";
        n.effectFormat = "+40 Max Voyage Energy";
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
        n.title = "REACTOR EFFICIENCY";
        n.categoryName = "ENGINE / ENERGY";
        n.description = "Reduces voyage energy drain rate by 25% to conserve fuel.";
        n.effectFormat = "-25% Energy Drain Rate";
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
        n.title = "SOLAR REGENERATOR";
        n.categoryName = "ENGINE / ENERGY";
        n.description = "Shattering asteroids and destroying enemy ships restores voyage energy.";
        n.effectFormat = "+4 Fuel Restored on Kill & Mine";
        n.branch = NodeBranch::South_Engine;
        n.icon = NodeIconType::Battery;
        n.gridPos = { 1.1f, 3.6f };
        n.maxLevel = 1;
        n.levelCosts = { { 190, 22, 7, 2, 0 } };
        n.levelValues = { 4.0f };
        n.prerequisiteIds = { 37 };
        m_nodes.push_back(n);
    }

    // 🔒 LOST TECHNOLOGY 3 (SOUTH)
    {
        UpgradeNode n;
        n.id = 39;
        n.title = "ANCIENT PHASE REACTOR";
        n.categoryName = "LOST TECH";
        n.description = "Quantum reactor granting 3 seconds of zero fuel drain and speed surge after each dash.";
        n.effectFormat = "3s 0 Fuel Drain & +30% Speed on Dash";
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
        n.title = "PERPETUAL ENGINE PROTOCOL";
        n.categoryName = "CAPSTONE / ULTIMATE";
        n.description = "Halves fuel consumption while every kill and mined rock recharges massive energy.";
        n.effectFormat = "-50% Drain & Near-Infinite Voyage Energy";
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
        n.title = "REINFORCED HULL I";
        n.categoryName = "DEFENSE / HULL";
        n.description = "Reinforces vessel armor plating to add extra hull integrity hearts.";
        n.effectFormat = "+1 Max Hull Heart";
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
        n.title = "PLASMA FORCE SHIELD";
        n.categoryName = "DEFENSE / SHIELD";
        n.description = "Deploys a plasma shield bubble absorbing 1 incoming hit every 10 seconds.";
        n.effectFormat = "Absorbs 1 Hit (10s Recharge)";
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
        n.title = "DEFLECTOR MATRIX";
        n.categoryName = "DEFENSE / SHIELD";
        n.description = "Shield-deflected projectiles reflect back towards attackers as kinetic bolts.";
        n.effectFormat = "Reflects Blocked Bullets at Enemies";
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
        n.title = "AUTONOMOUS TURRET CORE";
        n.categoryName = "DEFENSE / TURRET";
        n.description = "Deploys an automated tactical defense turret engaging hostiles and asteroids in range.";
        n.effectFormat = "+1 Defense Station Turret";
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
        n.title = "KINETIC TURRET DRIVE";
        n.categoryName = "DEFENSE / TURRET";
        n.description = "Increases turret firing rate and enhances ballistic projectile damage.";
        n.effectFormat = "+15 Turret Damage & Rapid Fire";
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
        n.title = "RAILGUN TURRET (GUN SPEC)";
        n.categoryName = "DEFENSE / TURRET SPEC";
        n.description = "Upgrades turret to high-velocity armor-piercing kinetic railgun.";
        n.effectFormat = "Armor-Piercing Railgun & +30 Damage";
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
        n.title = "MINING LASER TURRET (MINING SPEC)";
        n.categoryName = "DEFENSE / TURRET SPEC";
        n.description = "Turret prioritizes asteroids dealing 2x mining damage to harvest rich ore.";
        n.effectFormat = "Automated Mining & 2x Asteroid Damage";
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
        n.title = "PLASMA MORTAR TURRET (PLASMA SPEC)";
        n.categoryName = "DEFENSE / TURRET SPEC";
        n.description = "Turret fires explosive plasma mortar shells causing wide AoE blast waves.";
        n.effectFormat = "Wide AoE Plasma Mortar Barrage";
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
        n.title = "ORBITAL DEFENSE GRID (3 TURRETS)";
        n.categoryName = "DEFENSE / TURRET";
        n.description = "Establishes 3 coordinated tactical defense stations controlling the sector.";
        n.effectFormat = "Total 3 Map Defense Turrets";
        n.branch = NodeBranch::West_Defense;
        n.icon = NodeIconType::Turret;
        n.gridPos = { -5.1f, 0.0f };
        n.maxLevel = 1;
        n.levelCosts = { { 300, 35, 12, 3, 1 } };
        n.levelValues = { 3.0f };
        n.prerequisiteIds = { 45, 46, 47 };
        m_nodes.push_back(n);
    }

    // Sub-branch 3: SHOCKWAVE
    {
        UpgradeNode n;
        n.id = 49;
        n.title = "EMP PULSE GENERATOR";
        n.categoryName = "DEFENSE / EMP";
        n.description = "Periodically emits an electromagnetic pulse clearing bullets and damaging foes.";
        n.effectFormat = "Bullet-Clearing EMP Wave Every 9s";
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
        n.title = "ANCIENT SENTINEL AI";
        n.categoryName = "LOST TECH";
        n.description = "Alien targeting cortex granting extreme range and lethal precision to all turrets.";
        n.effectFormat = "+100 Turret Range & +40 Damage";
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
        n.title = "ORBITAL FORTRESS CITADEL";
        n.categoryName = "CAPSTONE / ULTIMATE";
        n.description = "3 Heavy Turrets + Reflector Barrier + Continuous EMP form an impregnable fortress.";
        n.effectFormat = "Citadel: 3 Turrets, Shield & Constant EMP";
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
        n.title = "LASER EXCAVATOR";
        n.categoryName = "HYBRID / WEAPON+MINING";
        n.description = "Laser beam deals +100% bonus extraction damage against asteroid minerals.";
        n.effectFormat = "+100% Laser Damage to Asteroids";
        n.branch = NodeBranch::Hybrid;
        n.icon = NodeIconType::Hybrid;
        n.gridPos = { 2.7f, -2.7f };
        n.maxLevel = 1;
        n.isHybrid = true;
        n.levelCosts = { { 150, 18, 5, 1, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 1, 14 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 52;
        n.title = "SALVAGE DRONE";
        n.categoryName = "HYBRID / DEFENSE+MINING";
        n.description = "Debris and loot from targets destroyed by turrets are automatically collected.";
        n.effectFormat = "Turret Kills Auto-Collected";
        n.branch = NodeBranch::Hybrid;
        n.icon = NodeIconType::Hybrid;
        n.gridPos = { 2.7f, 2.7f };
        n.maxLevel = 1;
        n.isHybrid = true;
        n.levelCosts = { { 150, 18, 5, 1, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 19, 40 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 53;
        n.title = "VELOCITY CANNON";
        n.categoryName = "HYBRID / ENGINE+WEAPON";
        n.description = "Higher vessel flight velocity directly amplifies laser beam damage output.";
        n.effectFormat = "Laser Damage Scales with Speed (+1% per 5px)";
        n.branch = NodeBranch::Hybrid;
        n.icon = NodeIconType::Hybrid;
        n.gridPos = { -2.7f, -2.7f };
        n.maxLevel = 1;
        n.isHybrid = true;
        n.levelCosts = { { 150, 18, 5, 1, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 1, 28 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 54;
        n.title = "RETALIATION MATRIX";
        n.categoryName = "HYBRID / DEFENSE+WEAPON";
        n.description = "Taking damage or popping shields triggers a 4s hyper laser frenzy (+100% fire rate).";
        n.effectFormat = "4s Hyper Laser Frenzy on Hit (+100% Rate)";
        n.branch = NodeBranch::Hybrid;
        n.icon = NodeIconType::Hybrid;
        n.gridPos = { -3.8f, -2.2f };
        n.maxLevel = 1;
        n.isHybrid = true;
        n.levelCosts = { { 200, 25, 8, 2, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 1, 40 };
        m_nodes.push_back(n);
    }
    {
        UpgradeNode n;
        n.id = 55;
        n.title = "HYPER MAGNET";
        n.categoryName = "HYBRID / ENGINE+MINING";
        n.description = "Magnetic vacuum suction radius expands by +50% while cruising at high speed.";
        n.effectFormat = "+50% Vacuum Radius at High Speed";
        n.branch = NodeBranch::Hybrid;
        n.icon = NodeIconType::Hybrid;
        n.gridPos = { -2.7f, 2.7f };
        n.maxLevel = 1;
        n.isHybrid = true;
        n.levelCosts = { { 150, 18, 5, 1, 0 } };
        n.levelValues = { 1.0f };
        n.prerequisiteIds = { 19, 28 };
        m_nodes.push_back(n);
    }

    // =========================================================================
    // ⚡ ACTIVE SKILLS (56..58)
    // =========================================================================
    {
        UpgradeNode n;
        n.id = 56;
        n.title = "[Q] EMP NOVA";
        n.categoryName = "ACTIVE SKILL [Q]";
        n.description = "Emits a full-screen electromagnetic shockwave obliterating bullets and damaging targets.";
        n.effectFormat = "[Q] Key: Full Screen Bullet Clear & EMP Wave";
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
        n.title = "[E] OVERCHARGE";
        n.categoryName = "ACTIVE SKILL [E]";
        n.description = "Overclocks power conduit into a 4-second hyper laser frenzy.";
        n.effectFormat = "[E] Key: 4s Mega Laser Overdrive";
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
        n.title = "[SPACE] PHASE DASH";
        n.categoryName = "ACTIVE SKILL [SPACE]";
        n.description = "Instantly warps forward granting complete damage invulnerability.";
        n.effectFormat = "[SPACE] Key: Invulnerable Warp Dash";
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
        if (m_soundClick != -1) PlayAudio(m_soundClick);
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

    if (m_soundClick != -1)
    {
        PlayAudio(m_soundClick);
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

    // Advance Quests
    if (!m_quest.completed && reishi > 0)
    {
        m_quest.currentAmount += reishi;
        if (m_quest.currentAmount >= m_quest.targetAmount)
        {
            m_quest.currentAmount = m_quest.targetAmount;
            m_quest.completed = true;
            bank.key += 1; // Award +1 Sector Key!
        }
    }
}

void UpgradeTree::Update(float deltaTime, PlayerStats& stats, PlayerResources& bank, bool& outStartGame, int currentSector)
{
    (void)currentSector;
    outStartGame = false;
    m_globalTime += deltaTime;

    // Smooth Zoom interpolation
    m_zoom += (m_targetZoom - m_zoom) * 12.0f * deltaTime;
    m_panOffset.x += (m_targetPanOffset.x - m_panOffset.x) * 12.0f * deltaTime;
    m_panOffset.y += (m_targetPanOffset.y - m_panOffset.y) * 12.0f * deltaTime;

    int mouseX = InputMouse_GetX();
    int mouseY = InputMouse_GetY();

    // Mouse Panning with Right Click or Middle Click
    bool rBtn = InputMouse_IsPress(MOUSE_BUTTON_RIGHT);
    if (rBtn)
    {
        if (!m_isPanning)
        {
            m_isPanning = true;
            m_lastMouseX = mouseX;
            m_lastMouseY = mouseY;
        }
        else
        {
            float dx = (float)(mouseX - m_lastMouseX);
            float dy = (float)(mouseY - m_lastMouseY);
            m_targetPanOffset.x += dx;
            m_targetPanOffset.y += dy;
            m_lastMouseX = mouseX;
            m_lastMouseY = mouseY;
        }
    }
    else
    {
        m_isPanning = false;
    }

    // Mouse Wheel Zoom
    // Optional clamp offsets
    m_targetPanOffset.x = std::clamp(m_targetPanOffset.x, -500.0f, 500.0f);
    m_targetPanOffset.y = std::clamp(m_targetPanOffset.y, -350.0f, 350.0f);

    UpdateLayout();

    // Hover detection for nodes
    m_hoveredNodeId = -1;
    for (auto& node : m_nodes)
    {
        float halfW = m_nodeSize * m_zoom * 0.5f;
        float halfH = m_nodeSize * m_zoom * 0.5f;
        if (mouseX >= node.screenPos.x - halfW && mouseX <= node.screenPos.x + halfW &&
            mouseY >= node.screenPos.y - halfH && mouseY <= node.screenPos.y + halfH)
        {
            m_hoveredNodeId = node.id;
        }

        // Animate hover progress
        float targetHover = (m_hoveredNodeId == node.id) ? 1.0f : 0.0f;
        node.hoverProgress += (targetHover - node.hoverProgress) * 14.0f * deltaTime;

        // Animate pulse timer
        if (node.unlockPulseTimer > 0.0f)
        {
            node.unlockPulseTimer -= deltaTime * 2.0f;
        }
    }

    // Hover detection for Left Bank Resources
    m_hoveredBankIndex = -1;
    float cardX = 35.0f;
    float cardStartY = 110.0f;
    float cardW = 210.0f;
    float cardH = 50.0f;
    float cardGap = 10.0f;

    for (int i = 0; i < 5; ++i)
    {
        float cy = cardStartY + i * (cardH + cardGap);
        if (mouseX >= cardX && mouseX <= cardX + cardW &&
            mouseY >= cy && mouseY <= cy + cardH)
        {
            m_hoveredBankIndex = i;
            break;
        }
    }

    // Check Sector Stage Clicks (Right Panel)
    float panelRightX = (float)SCREEN_WIDTH - 275.0f;
    float panelRightW = 240.0f;
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
                    if (m_soundClick != -1) PlayAudio(m_soundClick);
                }
                else if (bank.key >= 1)
                {
                    // Unlock with Sector Key!
                    bank.key -= 1;
                    m_sectors[i].unlocked = true;
                    m_currentSectorIndex = m_sectors[i].stageNumber;
                    if (m_soundClick != -1) PlayAudio(m_soundClick);
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
            if (m_soundClick != -1) PlayAudio(m_soundClick);
        }
        else if (mouseX >= zPanelX + 96.0f && mouseX <= zPanelX + 124.0f)
        {
            m_targetZoom = std::max(0.65f, m_targetZoom - 0.12f);
            if (m_soundClick != -1) PlayAudio(m_soundClick);
        }
        else if (mouseX >= zPanelX + 132.0f && mouseX <= zPanelX + 252.0f)
        {
            m_targetZoom = 0.95f;
            m_targetPanOffset = { 0.0f, 0.0f };
            if (m_soundClick != -1) PlayAudio(m_soundClick);
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
            if (m_soundClick != -1) PlayAudio(m_soundClick);
            outStartGame = true;
        }
    }

    // Keyboard shortcut to start expedition
    if (InputKeyboard_IsTrigger(KK_SPACE))
    {
        if (m_soundClick != -1) PlayAudio(m_soundClick);
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

    // [RESET] Button
    Sprite_DrawRect(panelX + 132.0f, panelY + 5.0f, 120.0f, 24.0f, { 0.18f, 0.14f, 0.24f, 0.90f });
    Sprite_DrawRectBorder(panelX + 132.0f, panelY + 5.0f, 120.0f, 24.0f, 1.0f, { 1.0f, 0.85f, 0.4f, 0.8f });
    DrawTextMatrix(panelX + 148.0f, panelY + 11.0f, "RESET [R]", 1.6f, { 1.0f, 0.90f, 0.50f, 1.0f });

    // Hint text above
    DrawTextMatrix(cx - 180.0f, panelY - 18.0f, "MOUSE WHEEL: ZOOM  |  RIGHT DRAG: PAN  |  [R]: RESET", 1.4f, { 0.65f, 0.75f, 0.85f, 0.75f });
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

    // Header Title Box: UPGRADES (compact & sleek at the top)
    float titleBoxW = 380.0f;
    float titleBoxH = 38.0f;
    float titleBoxX = (float)SCREEN_WIDTH * 0.5f - titleBoxW * 0.5f;
    float titleBoxY = 18.0f;

    Sprite_DrawRect(titleBoxX, titleBoxY, titleBoxW, titleBoxH, { 0.14f, 0.09f, 0.18f, 0.80f });
    Sprite_DrawRectBorder(titleBoxX, titleBoxY, titleBoxW, titleBoxH, 1.5f, { 0.95f, 0.88f, 0.78f, 0.80f });

    DrawTextMatrix(titleBoxX + 80.0f, titleBoxY + 9.0f, "UPGRADES", 3.4f, { 0.96f, 0.92f, 0.84f, 1.0f });
}

void UpgradeTree::DrawLeftPanel(const PlayerResources& bank)
{
    float panelX = 40.0f;
    float panelY = 45.0f;
    float panelW = 280.0f;

    // STORAGE Header
    DrawTextMatrix(panelX, panelY, "STORAGE", 4.5f, { 0.35f, 0.90f, 0.95f, 1.0f });
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
    // MISSIONS (Quests / Missions)
    // ==========================================
    float questY = startY + 5 * rowSpacing + 50.0f;

    DrawTextMatrix(panelX, questY, "MISSIONS", 4.0f, { 0.95f, 0.40f, 0.75f, 1.0f });
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

    // SECTORS Header
    DrawTextMatrix(panelX, panelY, "SECTORS", 3.8f, { 1.0f, 0.65f, 0.25f, 1.0f });
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
        if (isCompleted && isCurrent)
        {
            DrawTextMatrix(panelX + 48.0f, cy + 30.0f, "[ SELECTED : VICTORY ]", 1.5f, { 0.35f, 0.95f, 0.50f, 1.0f });
        }
        else if (isCompleted)
        {
            DrawTextMatrix(panelX + 48.0f, cy + 30.0f, "[ VICTORY ]", 1.6f, { 0.35f, 0.95f, 0.50f, 1.0f });
        }
        else if (isCurrent)
        {
            DrawTextMatrix(panelX + 48.0f, cy + 30.0f, "[ ACTIVE TARGET / SELECTED ]", 1.5f, { 1.0f, 0.85f, 0.25f, 1.0f });
        }
        else if (isUnlocked)
        {
            DrawTextMatrix(panelX + 48.0f, cy + 30.0f, "[ CLICK TO SELECT ]", 1.6f, { 0.40f, 0.85f, 1.0f, 1.0f });
        }
        else
        {
            DrawTextMatrix(panelX + 48.0f, cy + 30.0f, "[ LOCKED: 1 KEY REQUIRED ]", 1.5f, { 0.85f, 0.35f, 0.35f, 0.9f });
        }
    }

    // ==========================================
    // LAUNCH BUTTON
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

    DrawTextMatrix(drawX + 70.0f, drawY + drawH * 0.5f - 10.0f, "LAUNCH MISSION", 2.3f, textCol);
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
        bool isSkillNode = (node.icon == NodeIconType::SkillEmp || node.icon == NodeIconType::SkillDash || node.icon == NodeIconType::SkillOvercharge);
        float baseSize = node.isCenterHub ? 52.0f : (isSkillNode ? 58.0f : (node.isCapstone ? 46.0f : m_nodeSize));
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

        if (isSkillNode)
        {
            float pulse = sinf(m_globalTime * 7.0f) * 0.20f + 0.80f;
            if (unlocked)
            {
                bgCol = DirectX::XMFLOAT4(0.10f, 0.40f, 0.45f, 0.98f);
                borderCol = DirectX::XMFLOAT4(0.40f, 1.0f, 0.90f, 1.0f);
                iconCol = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
            }
            else if (hasPrereq)
            {
                bgCol = DirectX::XMFLOAT4(0.20f, 0.15f, 0.35f, 0.95f);
                borderCol = DirectX::XMFLOAT4(1.0f, 0.82f, 0.20f, pulse);
                iconCol = DirectX::XMFLOAT4(1.0f, 0.92f, 0.40f, 1.0f);
            }
            else
            {
                bgCol = DirectX::XMFLOAT4(0.12f, 0.10f, 0.18f, 0.90f);
                borderCol = DirectX::XMFLOAT4(0.35f, 0.70f, 1.0f, 0.75f * pulse);
                iconCol = DirectX::XMFLOAT4(0.55f, 0.75f, 1.0f, 0.75f);
            }
        }
        else if (node.isCenterHub)
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
        float borderThickness = (node.hoverProgress > 0.1f || node.isCapstone || isSkillNode) ? 2.5f : 1.5f;
        Sprite_DrawRectBorder(x, y, w, h, borderThickness, borderCol);

        if (node.hoverProgress > 0.05f || (node.isCapstone && unlocked) || isSkillNode)
        {
            DirectX::XMFLOAT4 haloCol = borderCol;
            haloCol.w = std::max(0.35f, node.hoverProgress * 0.55f);
            Sprite_DrawRectBorder(x - 4.0f, y - 4.0f, w + 8.0f, h + 8.0f, 1.8f, haloCol);
        }

        // Draw Node Glyph / Icon (Skills ALWAYS render their icon image regardless of prereq state!)
        NodeIconType iconType = (node.isSealed && !node.isSealBroken) ? NodeIconType::Lock : (isSkillNode ? node.icon : (hasPrereq ? node.icon : NodeIconType::Question));
        DrawNodeGlyph(iconType, node.screenPos.x, node.screenPos.y, isSkillNode ? w * 0.78f : w * 0.60f, iconCol);

        if (isSkillNode)
        {
            // Keybinding / Skill Label Badge below node
            const char* skillTag = (node.icon == NodeIconType::SkillDash) ? "SPACE DASH" : (node.icon == NodeIconType::SkillEmp ? "Q EMP WAVE" : "E OVERCHARGE");
            DrawTextMatrix(node.screenPos.x - 32.0f, y + h + 4.0f, skillTag, 1.3f, borderCol);
        }

        // Draw Level Pips
        if (!node.isCenterHub && !isSkillNode && node.maxLevel > 1)
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
        DrawTextMatrix(cardX + 16.0f, cardY + 68.0f, "[ ⭐ ULTIMATE PROTOCOL - 1 CAPSTONE LIMIT ]", 1.7f, { 1.0f, 0.85f, 0.25f, 1.0f });
    }
    else if (node->isSealed && !node->isSealBroken)
    {
        DrawTextMatrix(cardX + 16.0f, cardY + 68.0f, "[ 🔒 SEALED ANCIENT TECH ]", 1.8f, { 1.0f, 0.45f, 0.65f, 1.0f });
    }
    else if (!node->isCenterHub)
    {
        char lvlBuf[32];
        sprintf_s(lvlBuf, "LEVEL: %d / %d", node->currentLevel, node->maxLevel);
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
        DrawTextMatrix(cardX + 16.0f, bottomY + 14.0f, "[ COMMAND CORE - ACTIVE ]", 1.9f, { 0.95f, 0.85f, 0.40f, 1.0f });
    }
    else if (node->isSealed && !node->isSealBroken)
    {
        DrawTextMatrix(cardX + 16.0f, bottomY + 8.0f, "UNSEAL REQUIREMENT:", 1.8f, { 1.0f, 0.55f, 0.70f, 1.0f });
        DrawBankIcon(4, cardX + 28.0f, bottomY + 36.0f, 24.0f);
        DrawTextMatrix(cardX + 48.0f, bottomY + 28.0f, "1 ANCIENT KEY", 1.9f, { 1.0f, 0.85f, 0.30f, 1.0f });

        if (bank.key >= node->keySealCost)
        {
            DrawTextMatrix(cardX + 16.0f, bottomY + 56.0f, "[ CLICK TO UNSEAL WITH 1 KEY ]", 1.9f, { 0.35f, 1.0f, 0.60f, 1.0f });
        }
        else
        {
            DrawTextMatrix(cardX + 16.0f, bottomY + 56.0f, "[ INSUFFICIENT KEYS (1 KEY REQUIRED) ]", 1.7f, { 0.95f, 0.35f, 0.35f, 1.0f });
        }
    }
    else if (node->isCapstone && m_activeCapstoneId != -1 && m_activeCapstoneId != node->id)
    {
        DrawTextMatrix(cardX + 16.0f, bottomY + 14.0f, "[ 🔒 CORE ALIGNED - 1 CAPSTONE LIMIT ]", 1.7f, { 1.0f, 0.40f, 0.35f, 1.0f });
    }
    else if (node->IsMaxLevel())
    {
        DrawTextMatrix(cardX + 16.0f, bottomY + 14.0f, "[ MAXIMUM LEVEL REACHED ]", 2.0f, { 1.0f, 0.85f, 0.30f, 1.0f });
    }
    else if (!HasPrerequisites(node->id))
    {
        DrawTextMatrix(cardX + 16.0f, bottomY + 14.0f, "[ LOCKED - UNLOCK PREVIOUS NODES ]", 1.9f, { 0.95f, 0.35f, 0.35f, 1.0f });
    }
    else
    {
        int nextLvl = node->currentLevel;
        const ResourceCost& cost = node->levelCosts[nextLvl];
        bool canAfford = CanUnlockNode(node->id, bank);

        DrawTextMatrix(cardX + 16.0f, bottomY + 8.0f, "REQUIRED RESOURCES:", 1.8f, { 0.95f, 0.85f, 0.70f, 1.0f });

        float costItemX = cardX + 16.0f;
        float costItemY = bottomY + 28.0f;

        struct CostEntry { int type; int amount; int bankAmount; std::string label; };
        std::vector<CostEntry> requiredItems;

        if (cost.reishi > 0) requiredItems.push_back({ 0, cost.reishi, bank.reishi, "Reishi" });
        if (cost.vida > 0)   requiredItems.push_back({ 1, cost.vida, bank.vida, "Screw" });
        if (cost.disli > 0)  requiredItems.push_back({ 2, cost.disli, bank.disli, "Gear" });
        if (cost.cpu > 0)    requiredItems.push_back({ 3, cost.cpu, bank.cpu, "CPU" });
        if (cost.key > 0)    requiredItems.push_back({ 4, cost.key, bank.key, "Key" });

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
            DrawTextMatrix(cardX + 16.0f, bottomY + 54.0f, "[ CLICK TO UPGRADE ]", 2.1f, { 0.30f, 1.0f, 0.60f, 1.0f });
        }
        else
        {
            DrawTextMatrix(cardX + 16.0f, bottomY + 54.0f, "[ INSUFFICIENT RESOURCES ]", 2.0f, { 0.95f, 0.35f, 0.35f, 1.0f });
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
            "REISHI ENERGY CRYSTAL",
            "[ PRIMARY CURRENCY ]",
            "Core energy ore used for ship upgrades and system enhancements.",
            "SOURCE: Extracted from all destroyed asteroids and enemies.",
            { 0.4f, 1.0f, 0.7f, 1.0f }
        },
        {
            "TITANIUM SCREW",
            "[ RARE CRAFTING MATERIAL ]",
            "Industrial fastener required for structural hull and kinetic upgrades.",
            "SOURCE: Rare drop from asteroids and space chests.",
            { 0.45f, 0.85f, 0.95f, 1.0f }
        },
        {
            "ALLOY GEAR",
            "[ HIGH-TECH COMPONENT ]",
            "Precision mechanical gear used in weapon drives and turret assemblies.",
            "SOURCE: Mined from anomalous asteroids and mechanical units.",
            { 0.95f, 0.65f, 0.25f, 1.0f }
        },
        {
            "QUANTUM PROCESSOR (CPU)",
            "[ ADVANCED TECH COMPONENT ]",
            "High-grade computing matrix for laser algorithms and auto-turrets.",
            "SOURCE: Found in Ancient Space Chests and elite sector bosses.",
            { 0.35f, 0.95f, 0.85f, 1.0f }
        },
        {
            "SECTOR / ANCIENT KEY",
            "[ LOST TECH KEY TOKEN ]",
            "Unlocks new galaxy sectors and unseals lost alien technology nodes.",
            "SOURCE: Dropped by Key Signal Asteroids and completed mission quests.",
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
            if (m_texSkillWave != -1)
            {
                Sprite_Draw(m_texSkillWave, cx - half * 1.15f, cy - half * 1.15f, size * 1.15f, size * 1.15f, { 1.0f, 1.0f, 1.0f, 1.0f });
            }
            else
            {
                DrawTextMatrix(cx - 10.0f, cy - 8.0f, "Q", 2.2f, col);
                Sprite_DrawRectBorder(cx - half * 0.7f, cy - half * 0.7f, size * 0.7f, size * 0.7f, 1.5f, col);
            }
            break;
        }
        case NodeIconType::SkillDash:
        {
            if (m_texSkillDash != -1)
            {
                Sprite_Draw(m_texSkillDash, cx - half * 1.15f, cy - half * 1.15f, size * 1.15f, size * 1.15f, { 1.0f, 1.0f, 1.0f, 1.0f });
            }
            else
            {
                Sprite_DrawLine(cx - half * 0.7f, cy, cx + half * 0.5f, cy, 2.5f, col);
                Sprite_DrawLine(cx + half * 0.2f, cy - half * 0.4f, cx + half * 0.6f, cy, 2.0f, col);
                Sprite_DrawLine(cx + half * 0.2f, cy + half * 0.4f, cx + half * 0.6f, cy, 2.0f, col);
            }
            break;
        }
        case NodeIconType::SkillOvercharge:
        {
            if (m_texSkillBuff != -1)
            {
                Sprite_Draw(m_texSkillBuff, cx - half * 1.15f, cy - half * 1.15f, size * 1.15f, size * 1.15f, { 1.0f, 1.0f, 1.0f, 1.0f });
            }
            else
            {
                DrawTextMatrix(cx - 10.0f, cy - 8.0f, "E", 2.2f, col);
                Sprite_DrawLine(cx, cy - half * 0.7f, cx + half * 0.4f, cy + half * 0.6f, 2.0f, col);
            }
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
