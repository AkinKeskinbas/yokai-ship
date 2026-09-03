#include "game.h"
#include "direct3d.h"
#include "sprite.h"
#include "texture.h"
#include "input_keyboard.h"
#include "input_mouse.h"
#include "configuration.h"
#include "audio.h"
#include <cmath>
#include <algorithm>
#include <random>
#include <string>
#include <cstring>

static constexpr float PI = 3.14159265f;

// 3x5 Dot Matrix font mapping for UI rendering
static uint16_t GetCharMask(char c)
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
        default:  return 0b000000000000000;
    }
}

static void DrawMatrixString(float x, float y, const char* str, float size, int texture_id, DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f })
{
    if (texture_id == -1) return;
    float current_x = x;
    int src_x = 768;
    int src_y = 512;
    int src_size = 8;

    while (*str)
    {
        char c = *str;
        if (c == ' ')
        {
            current_x += 4.0f * size;
        }
        else
        {
            uint16_t mask = GetCharMask(c);
            for (int row = 0; row < 5; ++row)
            {
                for (int col = 0; col < 3; ++col)
                {
                    int bit_index = 14 - (row * 3 + col);
                    if ((mask >> bit_index) & 1)
                    {
                        float px = current_x + col * size;
                        float py = y + row * size;
                        Sprite_Draw(texture_id, px, py, size, size, src_x, src_y, src_size, src_size, color);
                    }
                }
            }
            current_x += 4.0f * size;
        }
        str++;
    }
}

// Matches DrawMatrixString's per-character advance (4.0f * size, spaces included) so text can
// be reliably centered instead of relying on hand-tuned pixel offsets that drift out of sync
// whenever the string or size changes.
static float MatrixTextWidth(const char* str, float size)
{
    return (float)strlen(str) * 4.0f * size;
}

static float CenteredTextX(const char* str, float size, float centerX)
{
    return centerX - MatrixTextWidth(str, size) * 0.5f;
}

// Simple word-wrap for DrawMatrixString: breaks `text` onto multiple lines no wider than
// maxWidth and returns how many lines it drew, so callers can size their box around it.
static int DrawWrappedMatrixText(const char* text, float x, float y, float maxWidth, float lineHeight,
    float size, int textureId, const DirectX::XMFLOAT4& color)
{
    std::string s(text);
    std::string word;
    float curX = x;
    float curY = y;
    int lineCount = 1;
    float spaceW = 4.0f * size;

    for (size_t i = 0; i <= s.size(); ++i)
    {
        bool atEnd = (i == s.size());
        if (atEnd || s[i] == ' ')
        {
            if (!word.empty())
            {
                float wordW = MatrixTextWidth(word.c_str(), size);
                if (curX > x && curX + wordW > x + maxWidth)
                {
                    curX = x;
                    curY += lineHeight;
                    ++lineCount;
                }
                DrawMatrixString(curX, curY, word.c_str(), size, textureId, color);
                curX += wordW + spaceW;
                word.clear();
            }
        }
        else
        {
            word += s[i];
        }
    }
    return lineCount;
}

static void DrawNumber(float x, float y, int value, int digitCount, float spacing, int textureId, DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f })
{
    if (textureId == -1) return;

    int temp = value;
    float digitWidth = 24.0f;
    float digitHeight = 24.0f;

    for (int i = 0; i < digitCount; i++)
    {
        int divisor = static_cast<int>(pow(10, digitCount - 1 - i));
        int digitVal = (temp / divisor) % 10;

        int src_x = digitVal * 64;
        int src_y = 0;

        float drawX = x + i * spacing;
        float drawY = y;

        Sprite_Draw(textureId, drawX, drawY, digitWidth, digitHeight, src_x, src_y, 64, 64, color);
    }
}

static void DrawDamageNumber(float centerX, float centerY, int value, float scale, int textureId, const DirectX::XMFLOAT4& color, bool isCrit, bool isWeakpoint, int laserTexId)
{
    if (textureId == -1 || value <= 0) return;

    std::string str = std::to_string(value);
    int numDigits = static_cast<int>(str.length());

    float digitSize = (isCrit || isWeakpoint ? 28.0f : 20.0f) * scale;
    float spacing = digitSize * 0.68f;
    float totalWidth = (numDigits - 1) * spacing + digitSize;
    float startX = centerX - totalWidth * 0.5f;
    float startY = centerY - digitSize * 0.5f;

    float alpha = color.w;

    // 1. WoW-Style Heavy 8-Way Black Contour Shadow (Extreme contrast against bright lasers/nebulae)
    DirectX::XMFLOAT4 shadowCol{ 0.02f, 0.02f, 0.04f, alpha * 0.95f };
    float outlineOffset = (isCrit || isWeakpoint) ? 2.6f : 1.8f;
    const float offsets[8][2] = {
        { -outlineOffset, 0.0f }, { outlineOffset, 0.0f },
        { 0.0f, -outlineOffset }, { 0.0f, outlineOffset },
        { -outlineOffset * 0.707f, -outlineOffset * 0.707f }, { outlineOffset * 0.707f, -outlineOffset * 0.707f },
        { -outlineOffset * 0.707f, outlineOffset * 0.707f }, { outlineOffset * 0.707f, outlineOffset * 0.707f }
    };

    for (int k = 0; k < 8; ++k)
    {
        for (int i = 0; i < numDigits; i++)
        {
            int digitVal = str[i] - '0';
            float drawX = startX + i * spacing + offsets[k][0];
            float drawY = startY + offsets[k][1];
            Sprite_Draw(textureId, drawX, drawY, digitSize, digitSize, digitVal * 64, 0, 64, 64, shadowCol);
        }
    }

    // 2. WoW-Style Radiant Critical Burst Accents / Exclamation Star Glints
    if (isCrit || isWeakpoint)
    {
        float glintSize = digitSize * 0.65f;
        float leftGlintX = startX - glintSize - 4.0f;
        float rightGlintX = startX + totalWidth + 4.0f;
        float glintY = startY + (digitSize - glintSize) * 0.5f;

        // Shadow for glints
        Sprite_DrawRect(leftGlintX - 1.0f, glintY + glintSize * 0.4f, glintSize + 2.0f, 3.5f, shadowCol);
        Sprite_DrawRect(leftGlintX + glintSize * 0.4f - 1.0f, glintY, 3.5f, glintSize, shadowCol);
        Sprite_DrawRect(rightGlintX - 1.0f, glintY + glintSize * 0.4f, glintSize + 2.0f, 3.5f, shadowCol);
        Sprite_DrawRect(rightGlintX + glintSize * 0.4f - 1.0f, glintY, 3.5f, glintSize, shadowCol);

        // Radiant foreground glints
        DirectX::XMFLOAT4 glintCol = isWeakpoint
            ? DirectX::XMFLOAT4(0.35f, 1.0f, 0.95f, alpha)
            : DirectX::XMFLOAT4(1.0f, 0.90f, 0.25f, alpha);

        Sprite_DrawRect(leftGlintX, glintY + glintSize * 0.4f, glintSize, 2.5f, glintCol);
        Sprite_DrawRect(leftGlintX + glintSize * 0.4f, glintY, 2.5f, glintSize, glintCol);
        Sprite_DrawRect(rightGlintX, glintY + glintSize * 0.4f, glintSize, 2.5f, glintCol);
        Sprite_DrawRect(rightGlintX + glintSize * 0.4f, glintY, 2.5f, glintSize, glintCol);

        // Header indicator tag above critical strike
        if (laserTexId != -1 && scale >= 1.05f)
        {
            if (isWeakpoint)
            {
                DrawMatrixString(centerX - 46.0f, startY - 15.0f, "* KRITIK VURUS *", 1.3f * scale, laserTexId, { 0.35f, 1.0f, 0.95f, alpha });
            }
            else
            {
                DrawMatrixString(centerX - 28.0f, startY - 15.0f, "* KRITIK! *", 1.3f * scale, laserTexId, { 1.0f, 0.85f, 0.20f, alpha });
            }
        }
    }

    // 3. Crisp Foreground Colored Digits
    for (int i = 0; i < numDigits; i++)
    {
        int digitVal = str[i] - '0';
        float drawX = startX + i * spacing;
        Sprite_Draw(textureId, drawX, startY, digitSize, digitSize, digitVal * 64, 0, 64, 64, color);
    }
}

Game::Game()
    : m_hWnd(nullptr)
{
}

bool Game::Initialize(HWND hWnd)
{
    m_hWnd = hWnd;

    // Initialize graphic systems
    Texture_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
    if (!Sprite_Initialize())
    {
        return false;
    }

    // Initialize Procedural Cosmic Background Shader (HLSL)
    if (!m_bgRenderer.Initialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext()))
    {
        return false;
    }

    // Load textures (Procedural background shader replaces background.jpeg)
    m_texSpaceship = Texture_Load(L"asset/spaceship.png");
    m_texLaser = Texture_Load(L"asset/laser_no_bg.png");
    m_texLaserHit = Texture_Load(L"asset/laser_effect_no_bg.png");
    m_texAsteroid = Texture_Load(L"asset/astroid.png");
    m_texBoss1 = Texture_Load(L"asset/first_boss.png"); // Replaced boss1.png with first_boss.png
    m_texBoss2 = Texture_Load(L"asset/boss2.png");
    m_texBoss3 = Texture_Load(L"asset/boss3.png");
    m_texFinalBoss = Texture_Load(L"asset/final_boss.png");
    m_texBossOrb = Texture_Load(L"asset/boss_orb.png");
    m_texBossBlade = Texture_Load(L"asset/boss_blade.png");
    m_texBossProjectile = Texture_Load(L"asset/projectile.png");
    m_texEnemy1 = Texture_Load(L"asset/enemy1.png");
    m_texEnemy1Bullet = Texture_Load(L"asset/enemy1bullet.png");
    m_texResources = Texture_Load(L"asset/resources_no_bg.png");
    m_texNumber = Texture_Load(L"asset/number.png");
    m_texHeart = Texture_Load(L"asset/hearth.png");
    m_texVida = Texture_Load(L"asset/vida.png");
    m_texDisli = Texture_Load(L"asset/disli.png");
    m_texCpu = Texture_Load(L"asset/cpu.png");
    m_texKey = Texture_Load(L"asset/key.png");
    m_texChest = Texture_Load(L"asset/chest.png");
    m_texSupporter = Texture_Load(L"asset/supporter.png"); // Tutorial dialogue portrait
    m_texTaret = Texture_Load(L"asset/taret.png");
    m_texSkillDash = Texture_Load(L"asset/dashSkill.png");
    m_texSkillWave = Texture_Load(L"asset/energyWaveSkill.png");
    m_texSkillBuff = Texture_Load(L"asset/buffSkill.png");

    m_texExplosions.resize(6);
    m_texExplosions[0] = Texture_Load(L"asset/exploding_1_no_bg.png");
    m_texExplosions[1] = Texture_Load(L"asset/exploding_2_no_bg.png");
    m_texExplosions[2] = Texture_Load(L"asset/exploding_3_no_bg.png");
    m_texExplosions[3] = Texture_Load(L"asset/exploding_4_no_bg.png");
    m_texExplosions[4] = Texture_Load(L"asset/exploding_5_no_bg.png");
    m_texExplosions[5] = Texture_Load(L"asset/exploding_6_no_bg.png");

    // Load audio
    m_soundShoot = LoadAudio("asset/shoot.wav");
    m_soundPat = LoadAudio("asset/pat.mpeg");
    m_soundClick = LoadAudio("asset/retroClick.mpeg");
    m_soundMusic = LoadAudio("asset/gameMusic.mpeg");

    // Background music loop at low volume ("gameMusic.mpeg kisik seste calsin")
    if (m_soundMusic != -1)
    {
        SetAudioVolume(m_soundMusic, 0.18f);
        PlayAudio(m_soundMusic, true);
    }

    // Initialize Upgrade Tree with real textures
    m_upgradeTree.Initialize(m_texLaser, m_texNumber, m_texHeart, m_texResources, m_texSpaceship,
                            m_texVida, m_texDisli, m_texCpu, m_texKey, m_soundClick,
                            m_texSkillDash, m_texSkillWave, m_texSkillBuff);
    m_upgradeTree.ApplyStats(m_stats);
    m_reishiCount = m_resources.reishi;

    m_calamity.level = 1;
    m_calamity.current = 0.0f;
    m_calamity.required = 100.0f;

    if (EnemyConfig::TEST_SPAWN_BOSS2_AT_START)
    {
        // Developer test mode: skip the title screen entirely and drop straight into gameplay.
        m_currentScene = GameScene::Gameplay;
        ResetRun(); // Prepares gameplay-run state (asteroids, spawn timers, boss trigger, ...)
    }
    else
    {
        // ResetRun() is gameplay-run setup only (and would otherwise place the ship below the
        // screen for the gameplay entry sequence) -- it runs later, when an expedition actually
        // starts. The title screen just needs its own idle state.
        m_currentScene = GameScene::MainMenu;
        InitMainMenu();
    }

    return true;
}

void Game::Finalize()
{
    if (m_soundMusic != -1)
    {
        StopAudio(m_soundMusic);
        UnloadAudio(m_soundMusic);
        m_soundMusic = -1;
    }
    if (m_soundClick != -1)
    {
        UnloadAudio(m_soundClick);
        m_soundClick = -1;
    }
    if (m_soundPat != -1)
    {
        UnloadAudio(m_soundPat);
        m_soundPat = -1;
    }
    if (m_soundShoot != -1)
    {
        UnloadAudio(m_soundShoot);
        m_soundShoot = -1;
    }

    m_bgRenderer.Finalize();

    m_texExplosions.clear();
    Texture_AllRelease();

    Sprite_Finalize();
    Texture_Finalize();
}

void Game::ResetRun()
{
    // Test Mode: match the sector level to the boss being tested (so HP/spawn scaling is
    // consistent with that sector), and fill resources so upgrades/skills can be tested freely.
    if (EnemyConfig::TEST_SPAWN_BOSS2_AT_START)
    {
        m_calamity.level = std::clamp(EnemyConfig::TEST_BOSS_TYPE, 1, 4);
        m_resources.reishi = 9999;
        m_resources.vida = 9999;
        m_resources.disli = 9999;
        m_resources.cpu = 9999;
        m_resources.key = 9999;
    }

    // Gameplay entry sequence: outside of test mode, the ship enters from below the screen and
    // smoothly decelerates into its normal starting position (see the ShipEntering RunState in
    // UpdateGameplay) before spawning or control begins. Test mode skips straight to Active.
    bool useShipEntrySequence = !EnemyConfig::TEST_SPAWN_BOSS2_AT_START;
    m_shipEntryTargetPos = DirectX::XMFLOAT2((float)SCREEN_WIDTH * 0.5f, (float)SCREEN_HEIGHT * 0.5f);
    m_playerPos = useShipEntrySequence
        ? DirectX::XMFLOAT2(m_shipEntryTargetPos.x, (float)SCREEN_HEIGHT + 90.0f)
        : m_shipEntryTargetPos;
    m_playerVelocity = DirectX::XMFLOAT2(0.0f, 0.0f);
    m_playerRotation = 0.0f;
    m_playerTargetRotation = 0.0f;
    m_totalTime = 0.0f;
    m_fuel = m_stats.maxFuel;
    m_playerHealth = m_stats.maxHealth;
    m_invincibleTimer = 0.0f;

    m_cameraShakeTimer = 0.0f;
    m_cameraShakeMaxDuration = 0.0f;
    m_cameraShakeIntensity = 0.0f;
    m_cameraOffset = { 0.0f, 0.0f };

    m_asteroids.clear();
    m_enemies.clear();
    m_bossOrbs.clear();
    m_bossBlades.clear();
    m_pickups.clear();
    m_orbitingResources.clear();
    m_chests.clear();
    m_lasers.clear();
    m_vfxs.clear();
    m_enemyProjectiles.clear();
    m_turretProjectiles.clear();
    m_damagePopups.clear();
    m_shockwaves.clear();

    m_isChestModalActive = false;
    m_activeChestIndex = -1;
    m_chestSpawnTimer = 0.0f;

    // Run-scoped tutorial state (m_tutorialCompleted is intentionally NOT reset here -- once
    // finished it stays finished for the rest of this session). StartExpeditionQuickLaunch()
    // re-arms it via StartTutorial() right after this call, when appropriate.
    m_tutorialActive = false;
    m_tutorialDialogueActive = false;
    m_tutorialIntroPending = false;
    m_tutorialAsteroidTriggered = false;
    m_tutorialQueue.clear();
    m_tutorialLineIndex = 0;
    m_tutorialPortraitPop = 0.0f;

    m_anomalousSignalTimer = 0.0f;
    m_anomalousWarningDisplayTimer = 0.0f;
    m_overheatDuration = 0.0f;
    m_lastLaserTarget = nullptr;
    m_straightMoveTimer = 0.0f;
    m_retaliationTimer = 0.0f;
    m_slingshotBoostTimer = 0.0f;

    m_spawnTimer = 0.0f;
    m_enemySpawnTimer = 0.0f;
    m_laserFireCooldown = 0.0f;
    m_laserDamageTickTimer = 0.0f;
    m_runState = useShipEntrySequence ? RunState::ShipEntering : RunState::Active;
    m_shipEntryElapsed = 0.0f;
    m_deathSequenceTimer = 0.0f;
    m_bossDeathTimer = 0.0f;
    m_explosionStaggerTimer = 0.0f;
    m_runSummaryInputDelay = 0.0f;
    m_defeatedBossPos = { 0.0f, 0.0f };
    m_bossTriggered = false;
    m_bossVictory = false;
    m_runStats = {};
    m_superVacuumActive = false;
    m_reishiCount = 0;

    // Sector difficulty & spawn scaling
    if (m_calamity.level <= 1)
    {
        m_maxAliveEnemies = 2; // Sector 1: Max 2 enemies simultaneously on screen
        m_enemySpawnInterval = 4.2f;
        m_maxAliveAsteroids = 14;
        m_spawnInterval = 0.95f;
    }
    else if (m_calamity.level == 2)
    {
        m_maxAliveEnemies = 4;
        m_enemySpawnInterval = 3.2f;
        m_maxAliveAsteroids = 16;
        m_spawnInterval = 0.85f;
    }
    else if (m_calamity.level == 3)
    {
        m_maxAliveEnemies = 6;
        m_enemySpawnInterval = 2.5f;
        m_maxAliveAsteroids = 18;
        m_spawnInterval = 0.70f;
    }
    else if (m_calamity.level == 4)
    {
        m_maxAliveEnemies = 9;
        m_enemySpawnInterval = 1.9f;
        m_maxAliveAsteroids = 20;
        m_spawnInterval = 0.60f;
    }
    else // Sector 5 (Afet Çekirdeği / Boss Stage)
    {
        m_maxAliveEnemies = 12;
        m_enemySpawnInterval = 1.4f;
        m_maxAliveAsteroids = 22;
        m_spawnInterval = 0.50f;
    }

    // Reset Shield
    m_currentShield = m_stats.maxShield;
    m_shieldRechargeTimer = m_stats.shieldRechargeTime;

    // Reset Dash & Overcharge
    m_isDashing = false;
    m_dashTimer = 0.0f;
    m_isOvercharged = false;
    m_overchargeTimer = 0.0f;
    m_shockwaveAutoTimer = 0.0f;

    // Setup Stationary Turrets on the Sector Map
    m_turrets.clear();
    if (m_stats.turretCount == 1)
    {
        TurretInstance t;
        t.position = { (float)SCREEN_WIDTH * 0.50f, (float)SCREEN_HEIGHT * 0.50f };
        t.defenseRadius = m_stats.turretRange;
        t.spec = m_stats.turretSpec;
        t.fireCooldown = 0.0f;
        m_turrets.push_back(t);
    }
    else if (m_stats.turretCount == 2)
    {
        TurretInstance t1;
        t1.position = { (float)SCREEN_WIDTH * 0.32f, (float)SCREEN_HEIGHT * 0.50f };
        t1.defenseRadius = m_stats.turretRange;
        t1.spec = m_stats.turretSpec;
        t1.fireCooldown = 0.0f;
        m_turrets.push_back(t1);

        TurretInstance t2;
        t2.position = { (float)SCREEN_WIDTH * 0.68f, (float)SCREEN_HEIGHT * 0.50f };
        t2.defenseRadius = m_stats.turretRange;
        t2.spec = m_stats.turretSpec;
        t2.fireCooldown = 0.0f;
        m_turrets.push_back(t2);
    }
    else if (m_stats.turretCount >= 3)
    {
        TurretInstance t1;
        t1.position = { (float)SCREEN_WIDTH * 0.50f, (float)SCREEN_HEIGHT * 0.30f };
        t1.defenseRadius = m_stats.turretRange;
        t1.spec = m_stats.turretSpec;
        t1.fireCooldown = 0.0f;
        m_turrets.push_back(t1);

        TurretInstance t2;
        t2.position = { (float)SCREEN_WIDTH * 0.28f, (float)SCREEN_HEIGHT * 0.70f };
        t2.defenseRadius = m_stats.turretRange;
        t2.spec = m_stats.turretSpec;
        t2.fireCooldown = 0.0f;
        m_turrets.push_back(t2);

        TurretInstance t3;
        t3.position = { (float)SCREEN_WIDTH * 0.72f, (float)SCREEN_HEIGHT * 0.70f };
        t3.defenseRadius = m_stats.turretRange;
        t3.spec = m_stats.turretSpec;
        t3.fireCooldown = 0.0f;
        m_turrets.push_back(t3);
    }

    // Setup Active Skill Slots
    m_skillSlots[0] = { m_stats.skill1, 0.0f, 20.0f, "EMP NOVA", "Q" };
    m_skillSlots[1] = { m_stats.skill2, 0.0f, 15.0f, "OVERCHARGE", "E" };
    m_skillSlots[2] = { m_stats.skill3, 0.0f, 3.5f, "PHASE DASH", "SPACE" };

    // Reset Calamity progress within the CURRENT Level (Higher sectors require more work!)
    m_calamity.current = 0.0f;
    m_calamity.required = 90.0f + (float)(m_calamity.level - 1) * 95.0f;
    m_calamityFillDisplay = 0.0f;
    m_bossTriggered = false;
    m_bossWarningTimer = 0.0f;
    m_bossVictory = false;

    // Test Mode: Immediately spawn boss on start if configured in enemy_config.h
    if (EnemyConfig::TEST_SPAWN_BOSS2_AT_START)
    {
        TriggerBossEncounter(EnemyConfig::TEST_BOSS_TYPE);
    }
}

float Game::RandomFloat(float min, float max)
{
    if (std::isnan(min) || std::isnan(max) || min >= max) return min;
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(min, max);
    return dis(gen);
}

int Game::RandomInt(int min, int max)
{
    if (min >= max) return min;
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(min, max);
    return dis(gen);
}

void Game::Update(float deltaTime)
{
    if (m_currentScene == GameScene::Gameplay)
    {
        if (InputKeyboard_IsTrigger(KK_U) || InputKeyboard_IsTrigger(KK_TAB))
        {
            // Mid-run pause to check the tree, not the Main Menu's holographic entrance --
            // make sure the tree/ship are in their settled state, not a stale slide-in offset.
            m_upgradeEnteredFromMenu = false;
            m_upgradeIntroTimer = 1.0f;
            m_upgradeTree.SetIntroOffsetX(0.0f);
            m_currentScene = GameScene::UpgradePlaceholder;
            return;
        }
        UpdateGameplay(deltaTime);
    }
    else if (m_currentScene == GameScene::UpgradePlaceholder)
    {
        // Reached from the Main Menu (not a mid-run pause) -- ESC returns to the title screen.
        if (m_upgradeEnteredFromMenu && InputKeyboard_IsTrigger(KK_ESCAPE))
        {
            m_currentScene = GameScene::MainMenu;
            return;
        }
        UpdateUpgrade(deltaTime);
    }
    else if (m_currentScene == GameScene::MainMenu)
    {
        UpdateMainMenu(deltaTime);
    }
}

void Game::TriggerShockwave(const DirectX::XMFLOAT2& center, float maxRadius, float damage, bool affectEnemies)
{
    ShockwaveInstance sw;
    sw.center = center;
    sw.currentRadius = 10.0f;
    sw.maxRadius = maxRadius;
    sw.lifetime = 0.0f;
    sw.maxLifetime = 0.45f;
    sw.damage = damage;
    m_shockwaves.push_back(sw);

    TriggerCameraShake(0.20f, 4.5f);

    // Push enemies in radius & deal damage
    if (affectEnemies)
    {
        for (auto& enemy : m_enemies)
        {
            if (enemy.destroyed) continue;
            float dx = enemy.position.x - center.x;
            float dy = enemy.position.y - center.y;
            float dist = sqrtf(dx * dx + dy * dy);
            if (dist <= maxRadius && dist > 0.001f)
            {
                enemy.hp -= damage;
                enemy.flashTimer = 0.15f;
                SpawnDamagePopup(enemy.position, (int)damage, true);
                float push = (maxRadius - dist) + 40.0f;
                enemy.position.x += (dx / dist) * push;
                enemy.position.y += (dy / dist) * push;
            }
        }
    }

    // Safely destroy enemy projectiles within shockwave radius without modifying vector during iteration
    for (auto& bullet : m_enemyProjectiles)
    {
        float dx = bullet.position.x - center.x;
        float dy = bullet.position.y - center.y;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist <= maxRadius)
        {
            VFXInstance spark;
            spark.position = bullet.position;
            spark.lifetime = 0.0f;
            spark.maxLifetime = 0.18f;
            spark.scale = 0.25f;
            spark.isSpriteSheet = true;
            spark.frameCount = 8;
            spark.frameDuration = 0.025f;
            spark.textureId = m_texLaserHit;
            m_vfxs.push_back(spark);

            bullet.lifetime = 0.0f; // Safely mark for removal
        }
    }
}

void Game::UpdateGameplay(float deltaTime)
{
    // =========================================================================
    // 📦 ANCIENT CHEST PAUSE MODAL INTERACTION (GAME PAUSED)
    // =========================================================================
    if (m_isChestModalActive)
    {
        int mouseX = InputMouse_GetX();
        int mouseY = InputMouse_GetY();
        bool leftClicked = InputMouse_IsTrigger(MOUSE_BUTTON_LEFT);

        // Modal Button coordinates
        float cardW = 540.0f;
        float cardH = 340.0f;
        float cardX = (float)SCREEN_WIDTH * 0.5f - cardW * 0.5f;
        float cardY = (float)SCREEN_HEIGHT * 0.5f - cardH * 0.5f;

        float btnW = 220.0f;
        float btnH = 50.0f;
        float btnY = cardY + cardH - 75.0f;
        float btn1X = cardX + 35.0f;
        float btn2X = cardX + cardW - btnW - 35.0f;

        bool hoverBtn1 = (mouseX >= btn1X && mouseX <= btn1X + btnW && mouseY >= btnY && mouseY <= btnY + btnH);
        bool hoverBtn2 = (mouseX >= btn2X && mouseX <= btn2X + btnW && mouseY >= btnY && mouseY <= btnY + btnH);

        // Option 1: Open with 1 Key [E] or Left Click
        if ((InputKeyboard_IsTrigger(KK_E) || (leftClicked && hoverBtn1)) && m_activeChestIndex >= 0 && m_activeChestIndex < (int)m_chests.size())
        {
            if (m_soundClick != -1) PlayAudio(m_soundClick);
            if (m_resources.key >= 1)
            {
                m_resources.key -= 1;
                auto chestPos = m_chests[m_activeChestIndex].position;

                // Big Ganimet Explosion! (+3 CPU, +5 Dişli, +10 Vida, +120 Reishi)
                for (int k = 0; k < 25; ++k)
                {
                    ResourcePickup r;
                    r.position = chestPos;
                    float angle = RandomFloat(0.0f, 2.0f * PI);
                    float spd = RandomFloat(120.0f, 260.0f);
                    r.velocity = { cosf(angle) * spd, sinf(angle) * spd };
                    r.scale = 0.09f;
                    r.type = PickupType::Reishi;
                    r.amount = 5;
                    m_pickups.push_back(r);
                }
                for (int k = 0; k < 10; ++k)
                {
                    ResourcePickup v;
                    v.position = chestPos;
                    float angle = RandomFloat(0.0f, 2.0f * PI);
                    float spd = RandomFloat(100.0f, 220.0f);
                    v.velocity = { cosf(angle) * spd, sinf(angle) * spd };
                    v.scale = 0.09f;
                    v.type = PickupType::Vida;
                    v.amount = 1;
                    m_pickups.push_back(v);
                }
                for (int k = 0; k < 5; ++k)
                {
                    ResourcePickup d;
                    d.position = chestPos;
                    float angle = RandomFloat(0.0f, 2.0f * PI);
                    float spd = RandomFloat(90.0f, 200.0f);
                    d.velocity = { cosf(angle) * spd, sinf(angle) * spd };
                    d.scale = 0.09f;
                    d.type = PickupType::Disli;
                    d.amount = 1;
                    m_pickups.push_back(d);
                }
                for (int k = 0; k < 3; ++k)
                {
                    ResourcePickup c;
                    c.position = chestPos;
                    float angle = RandomFloat(0.0f, 2.0f * PI);
                    float spd = RandomFloat(80.0f, 180.0f);
                    c.velocity = { cosf(angle) * spd, sinf(angle) * spd };
                    c.scale = 0.09f;
                    c.type = PickupType::Cpu;
                    c.amount = 1;
                    m_pickups.push_back(c);
                }

                TriggerCameraShake(0.55f, 14.0f);
                TriggerShockwave(chestPos, 220.0f, 0.0f);

                m_chests.erase(m_chests.begin() + m_activeChestIndex);
                m_isChestModalActive = false;
                m_activeChestIndex = -1;
            }
        }

        // Option 2: Decline & Save Key [ESC] or Left Click
        if (InputKeyboard_IsTrigger(KK_ESCAPE) || (leftClicked && hoverBtn2))
        {
            if (m_soundClick != -1) PlayAudio(m_soundClick);
            if (m_activeChestIndex >= 0 && m_activeChestIndex < (int)m_chests.size())
            {
                // Push chest away so it doesn't immediately retrigger
                m_chests[m_activeChestIndex].position.x += 100.0f;
            }
            m_isChestModalActive = false;
            m_activeChestIndex = -1;
        }

        return; // Complete Pause while modal is open!
    }

    if (m_tutorialDialogueActive)
    {
        UpdateTutorialModal(deltaTime);
        return; // Complete Pause while the tutorial dialogue is open, same as the chest modal
    }

    // Update Procedural Background Shader with dynamic boss status
    bool isBossAlive = m_bossTriggered && !m_bossVictory;
    m_bgRenderer.Update(deltaTime, isBossAlive);

    // =========================================================================
    // 🚀 GAMEPLAY ENTRY SEQUENCE: ship flies in from below screen, decelerates into its
    // starting position, settles briefly, THEN control (and spawning) begins.
    // =========================================================================
    if (m_runState == RunState::ShipEntering)
    {
        m_shipEntryElapsed += deltaTime;
        const float flightDuration = 0.85f;
        const float settleDuration = 0.30f;

        if (m_shipEntryElapsed < flightDuration)
        {
            float t = std::clamp(m_shipEntryElapsed / flightDuration, 0.0f, 1.0f);
            float eased = 1.0f - powf(1.0f - t, 3.0f); // Ease-out cubic: fast then decelerating
            float startY = (float)SCREEN_HEIGHT + 90.0f;
            m_playerPos.x = m_shipEntryTargetPos.x;
            m_playerPos.y = startY + (m_shipEntryTargetPos.y - startY) * eased;
        }
        else
        {
            m_playerPos = m_shipEntryTargetPos; // Settle exactly in place
            if (m_shipEntryElapsed >= flightDuration + settleDuration)
            {
                m_runState = RunState::Active; // Hand control to the player; spawning begins now
                if (m_tutorialIntroPending)
                {
                    m_tutorialIntroPending = false;
                    m_tutorialDialogueActive = true;
                }
            }
        }
        return;
    }

    if (m_runState == RunState::Active)
    {
        // Fuel Drain
        if (!m_stats.zeroPointReactor)
        {
            m_fuel -= m_stats.fuelDrainRate * deltaTime;
        }
        else
        {
            m_fuel -= (m_stats.fuelDrainRate * 0.40f) * deltaTime;
        }

        if (m_fuel <= 0.0f)
        {
            m_fuel = 0.0f;
            m_runState = RunState::RunEnded;
            m_runSummaryInputDelay = 0.50f;
            m_superVacuumActive = true; // Safely sweep remaining nearby materials
            TriggerCameraShake(0.20f, 4.0f);
        }

        // Calamity progression over time
        m_calamity.current += 0.8f * deltaTime;
        if (m_calamity.current >= m_calamity.required)
        {
            m_calamity.current = m_calamity.required;
            if (!m_bossTriggered)
            {
                int bossToSpawn = std::min(4, m_calamity.level);
                TriggerBossEncounter(bossToSpawn);
            }
        }

        float targetCalamityFill = (m_calamity.required > 0.0f) ? std::clamp(m_calamity.current / m_calamity.required, 0.0f, 1.0f) : 0.0f;
        m_calamityFillDisplay += (targetCalamityFill - m_calamityFillDisplay) * 10.0f * deltaTime;

        // Timer decays
        if (m_anomalousWarningDisplayTimer > 0.0f) m_anomalousWarningDisplayTimer -= deltaTime;
        if (m_retaliationTimer > 0.0f) m_retaliationTimer -= deltaTime;

        // ==========================================
        // ACTIVE SKILLS INPUT & TRIGGERING
        // ==========================================
        // Skill 1: EMP Energy Wave [Q]
        if (InputKeyboard_IsTrigger(KK_Q) && m_skillSlots[0].type == ActiveSkillType::EmpWave && m_skillSlots[0].cooldownTimer <= 0.0f)
        {
            m_skillSlots[0].cooldownTimer = m_skillSlots[0].maxCooldown;
            TriggerShockwave(m_playerPos, 450.0f, 0.0f, false); // EMP Nova: destroys projectiles only, does not damage drones
            if (m_soundShoot != -1) PlayAudio(m_soundShoot);
        }

        // Skill 2: Hyper Overcharge [E]
        if (InputKeyboard_IsTrigger(KK_E) && m_skillSlots[1].type == ActiveSkillType::Overcharge && m_skillSlots[1].cooldownTimer <= 0.0f)
        {
            m_skillSlots[1].cooldownTimer = m_skillSlots[1].maxCooldown;
            m_isOvercharged = true;
            m_overchargeTimer = 4.0f;
            TriggerCameraShake(0.25f, 5.0f);
            if (m_soundShoot != -1) PlayAudio(m_soundShoot);
        }

        // Skill 3: Phase Dash [SPACE]
        if (InputKeyboard_IsTrigger(KK_SPACE) &&
            m_skillSlots[2].type == ActiveSkillType::PhaseDash && m_skillSlots[2].cooldownTimer <= 0.0f)
        {
            m_skillSlots[2].cooldownTimer = m_stats.dashCooldown;
            m_isDashing = true;
            m_dashTimer = 0.40f;
            m_invincibleTimer = 0.40f; // i-frames

            // Dash directly in movement direction if moving, otherwise forward
            DirectX::XMFLOAT2 curInput{ 0.0f, 0.0f };
            if (InputKeyboard_IsPress(KK_W) || InputKeyboard_IsPress(KK_UP))    curInput.y -= 1.0f;
            if (InputKeyboard_IsPress(KK_S) || InputKeyboard_IsPress(KK_DOWN))  curInput.y += 1.0f;
            if (InputKeyboard_IsPress(KK_A) || InputKeyboard_IsPress(KK_LEFT))  curInput.x -= 1.0f;
            if (InputKeyboard_IsPress(KK_D) || InputKeyboard_IsPress(KK_RIGHT)) curInput.x += 1.0f;
            float inLen = sqrtf(curInput.x * curInput.x + curInput.y * curInput.y);

            if (inLen > 0.001f)
            {
                m_dashDir = { curInput.x / inLen, curInput.y / inLen };
            }
            else
            {
                m_dashDir = { sinf(m_playerRotation), -cosf(m_playerRotation) };
            }

            if (m_soundShoot != -1) PlayAudio(m_soundShoot);
        }

        // Skill cooldown decay
        for (auto& s : m_skillSlots)
        {
            if (s.cooldownTimer > 0.0f) s.cooldownTimer -= deltaTime;
        }

        // Boss Warning Banner Timer decay (disappears after 3.5 seconds)
        if (m_bossWarningTimer > 0.0f)
        {
            m_bossWarningTimer -= deltaTime;
        }

        // Anomalous Warning Banner Timer decay
        if (m_anomalousWarningDisplayTimer > 0.0f)
        {
            m_anomalousWarningDisplayTimer -= deltaTime;
        }

        // Overcharge timer
        if (m_isOvercharged)
        {
            m_overchargeTimer -= deltaTime;
            if (m_overchargeTimer <= 0.0f) m_isOvercharged = false;
        }

        // Shield Recharge
        if (m_stats.shieldBubbleUnlocked && m_currentShield < m_stats.maxShield)
        {
            m_shieldRechargeTimer -= deltaTime;
            if (m_shieldRechargeTimer <= 0.0f)
            {
                m_currentShield = m_stats.maxShield;
                m_shieldRechargeTimer = m_stats.shieldRechargeTime;
            }
        }

        // Auto Shockwave
        if (m_stats.shockwaveUnlocked)
        {
            m_shockwaveAutoTimer += deltaTime;
            if (m_shockwaveAutoTimer >= m_stats.shockwaveInterval)
            {
                m_shockwaveAutoTimer = 0.0f;
                // EMP Pulse Generator: clears nearby enemy projectiles only, no enemy/boss damage or knockback
                TriggerShockwave(m_playerPos, m_stats.shockwaveRadius, m_stats.shockwaveDamage, false);
            }
        }

        // =========================================================================
        // 🚀 MODERN 8-WAY OMNIDIRECTIONAL MOVEMENT & SMOOTH SHIP HEADING
        // =========================================================================
        DirectX::XMFLOAT2 moveInput{ 0.0f, 0.0f };
        if (InputKeyboard_IsPress(KK_W) || InputKeyboard_IsPress(KK_UP))    moveInput.y -= 1.0f;
        if (InputKeyboard_IsPress(KK_S) || InputKeyboard_IsPress(KK_DOWN))  moveInput.y += 1.0f;
        if (InputKeyboard_IsPress(KK_A) || InputKeyboard_IsPress(KK_LEFT))  moveInput.x -= 1.0f;
        if (InputKeyboard_IsPress(KK_D) || InputKeyboard_IsPress(KK_RIGHT)) moveInput.x += 1.0f;

        float inputLen = sqrtf(moveInput.x * moveInput.x + moveInput.y * moveInput.y);
        if (inputLen > 0.001f)
        {
            moveInput.x /= inputLen;
            moveInput.y /= inputLen;

            // Smooth & highly responsive ship nose rotation towards movement direction
            float targetRot = atan2f(moveInput.x, -moveInput.y);
            float rotDiff = targetRot - m_playerRotation;
            while (rotDiff > PI)  rotDiff -= 2.0f * PI;
            while (rotDiff < -PI) rotDiff += 2.0f * PI;

            float turnSpeed = 16.0f; // Fast, snappy, super-fluid steering
            m_playerRotation += rotDiff * std::min(1.0f, turnSpeed * deltaTime);
        }

        // Normalize rotation to [-PI, PI]
        while (m_playerRotation > PI)  m_playerRotation -= 2.0f * PI;
        while (m_playerRotation < -PI) m_playerRotation += 2.0f * PI;

        // Velocity with snappy acceleration & silky smooth space glide
        float targetSpeed = m_stats.moveSpeed;
        DirectX::XMFLOAT2 targetVel{ moveInput.x * targetSpeed, moveInput.y * targetSpeed };
        float accelRate = (inputLen > 0.001f) ? 14.0f : 8.5f;

        m_playerVelocity.x += (targetVel.x - m_playerVelocity.x) * accelRate * deltaTime;
        m_playerVelocity.y += (targetVel.y - m_playerVelocity.y) * accelRate * deltaTime;

        if (m_isDashing)
        {
            m_dashTimer -= deltaTime;
            m_playerPos.x += m_dashDir.x * 680.0f * deltaTime;
            m_playerPos.y += m_dashDir.y * 680.0f * deltaTime;

            // Dash Specialization Behaviors:
            if (m_stats.dashType == DashType::Impact)
            {
                // Impact Dash: 120 kinetic smash damage
                for (auto& enemy : m_enemies)
                {
                    if (enemy.destroyed) continue;
                    float dx = enemy.position.x - m_playerPos.x;
                    float dy = enemy.position.y - m_playerPos.y;
                    if (sqrtf(dx * dx + dy * dy) < m_playerHitboxRadius + enemy.radius + 15.0f)
                    {
                        enemy.hp -= 120.0f;
                        enemy.flashTimer = 0.08f;
                        SpawnDamagePopup(enemy.position, 120, true);
                        TriggerCameraShake(0.35f, 10.0f);
                        if (enemy.hp <= 0.0f) enemy.destroyed = true;
                    }
                }
                for (auto& ast : m_asteroids)
                {
                    if (ast.destroyed || ast.invulnerable) continue;
                    float dx = ast.position.x - m_playerPos.x;
                    float dy = ast.position.y - m_playerPos.y;
                    if (sqrtf(dx * dx + dy * dy) < m_playerHitboxRadius + ast.radius + 15.0f)
                    {
                        ast.hp -= 120.0f;
                        ast.flashTimer = 0.08f;
                        ClampFinalBossHpFloor(ast);
                        SpawnDamagePopup(ast.position, 120, true);
                        TriggerCameraShake(0.35f, 10.0f);
                        if (ast.hp <= 0.0f) ast.destroyed = true;
                    }
                }
            }
            else if (m_stats.dashType == DashType::Mining)
            {
                // Mining Dash: Instantly crush small asteroids
                for (auto& ast : m_asteroids)
                {
                    if (ast.destroyed || ast.isBoss) continue;
                    float dx = ast.position.x - m_playerPos.x;
                    float dy = ast.position.y - m_playerPos.y;
                    if (sqrtf(dx * dx + dy * dy) < m_playerHitboxRadius + ast.radius + 15.0f)
                    {
                        ast.hp = 0.0f;
                        ast.destroyed = true;
                        TriggerCameraShake(0.20f, 6.0f);
                    }
                }
            }

            if (m_dashTimer <= 0.0f) m_isDashing = false;
        }
        else
        {
            m_playerPos.x += m_playerVelocity.x * deltaTime;
            m_playerPos.y += m_playerVelocity.y * deltaTime;
        }

        m_playerPos.x = std::clamp(m_playerPos.x, 50.0f, (float)SCREEN_WIDTH - 50.0f);
        m_playerPos.y = std::clamp(m_playerPos.y, 50.0f, (float)SCREEN_HEIGHT - 50.0f);

        if (m_invincibleTimer > 0.0f)
        {
            m_invincibleTimer -= deltaTime;
        }

        // Asteroid & Enemy Spawning
        SpawnAsteroids(deltaTime);
        SpawnEnemies(deltaTime);

        // Continuous mining laser firing
        TargetAndFireLasers(deltaTime);

        // Autonomous Turrets Update
        UpdateTurrets(deltaTime);

        // Update Chests
        for (size_t i = 0; i < m_chests.size(); ++i)
        {
            auto& chest = m_chests[i];
            chest.position.x += chest.velocity.x * deltaTime;
            chest.position.y += chest.velocity.y * deltaTime;
            chest.rotation += chest.rotationSpeed * deltaTime;

            float pDx = m_playerPos.x - chest.position.x;
            float pDy = m_playerPos.y - chest.position.y;
            float pDist = sqrtf(pDx * pDx + pDy * pDy);

            if (pDist < m_playerHitboxRadius + chest.radius)
            {
                m_isChestModalActive = true;
                m_activeChestIndex = (int)i;
                break;
            }
        }

        // Update Enemies
        for (auto it = m_enemies.begin(); it != m_enemies.end(); )
        {
            if (it->destroyed)
            {
                if (m_soundPat != -1) PlayAudio(m_soundPat);

                // Enemy death drops: Reishi + chance of Vida
                int dropNum = RandomInt(2, 4);
                for (int k = 0; k < dropNum; ++k)
                {
                    ResourcePickup crystal;
                    crystal.position = it->position;
                    float angle = RandomFloat(0.0f, 2.0f * PI);
                    float speed = RandomFloat(100.0f, 200.0f);
                    crystal.velocity.x = cosf(angle) * speed;
                    crystal.velocity.y = sinf(angle) * speed;
                    crystal.scale = RandomFloat(0.07f, 0.09f);
                    crystal.type = PickupType::Reishi;
                    crystal.amount = 1;
                    m_pickups.push_back(crystal);
                }

                if (RandomFloat(0.0f, 1.0f) < 0.60f)
                {
                    ResourcePickup bolt;
                    bolt.position = it->position;
                    float angle = RandomFloat(0.0f, 2.0f * PI);
                    float speed = RandomFloat(80.0f, 160.0f);
                    bolt.velocity.x = cosf(angle) * speed;
                    bolt.velocity.y = sinf(angle) * speed;
                    bolt.scale = 0.08f;
                    bolt.type = PickupType::Vida;
                    bolt.amount = 1;
                    m_pickups.push_back(bolt);
                }

                // Advance Calamity / Boss Meter by killing enemies
                m_calamity.current += 7.0f;
                m_runStats.enemiesKilled++;

                // Zero Point Reactor fuel refill
                if (m_stats.zeroPointReactor)
                {
                    m_fuel = std::min(m_stats.maxFuel, m_fuel + (m_stats.activeCapstone == 3 ? 10.0f : 4.0f));
                }

                it = m_enemies.erase(it);
                continue;
            }

            if (it->flashTimer > 0.0f)
            {
                it->flashTimer -= deltaTime;
            }

            it->rotation += it->rotationSpeed * deltaTime;
            if (it->rotation > 2.0f * PI) it->rotation -= 2.0f * PI;
            if (it->rotation < -2.0f * PI) it->rotation += 2.0f * PI;

            float dx = it->targetPosition.x - it->position.x;
            float dy = it->targetPosition.y - it->position.y;
            float dist = sqrt(dx * dx + dy * dy);

            if (dist > 15.0f)
            {
                float moveSpd = 85.0f;
                it->position.x += (dx / dist) * moveSpd * deltaTime;
                it->position.y += (dy / dist) * moveSpd * deltaTime;
            }

            it->changeTargetTimer -= deltaTime;
            if (it->changeTargetTimer <= 0.0f)
            {
                it->changeTargetTimer = RandomFloat(3.0f, 5.0f);
                it->targetPosition.x = RandomFloat(120.0f, (float)SCREEN_WIDTH - 120.0f);
                it->targetPosition.y = RandomFloat(100.0f, (float)SCREEN_HEIGHT - 100.0f);
            }

            it->shootTimer -= deltaTime;
            if (it->shootTimer <= 0.0f)
            {
                it->shootTimer = it->shootInterval;
                float bSpeed = EnemyConfig::Drone.bulletSpeed + (float)(m_calamity.level - 1) * 18.0f;

                for (int k = 0; k < 4; k++)
                {
                    float angle = it->rotation + (float)k * (PI * 0.5f);
                    EnemyProjectile bullet;
                    bullet.position = it->position;
                    bullet.velocity.x = cosf(angle) * bSpeed;
                    bullet.velocity.y = sinf(angle) * bSpeed;
                    bullet.radius = EnemyConfig::Drone.bulletRadius;
                    bullet.damage = EnemyConfig::Drone.bulletDamage;
                    bullet.lifetime = EnemyConfig::Drone.bulletLifetime;
                    m_enemyProjectiles.push_back(bullet);
                }
            }

            float pDx = m_playerPos.x - it->position.x;
            float pDy = m_playerPos.y - it->position.y;
            float pDist = sqrt(pDx * pDx + pDy * pDy);
            float minDist = m_playerHitboxRadius + it->radius;

            if (pDist < minDist && pDist > 0.001f)
            {
                DamagePlayer(1);

                float push = (minDist - pDist) + 8.0f;
                m_playerPos.x += (pDx / pDist) * push;
                m_playerPos.y += (pDy / pDist) * push;
                m_playerPos.x = std::clamp(m_playerPos.x, 50.0f, (float)SCREEN_WIDTH - 50.0f);
                m_playerPos.y = std::clamp(m_playerPos.y, 50.0f, (float)SCREEN_HEIGHT - 50.0f);
            }

            ++it;
        }

        // Update Enemy Projectiles
        for (auto it = m_enemyProjectiles.begin(); it != m_enemyProjectiles.end(); )
        {
            it->lifetime -= deltaTime;
            if (it->lifetime <= 0.0f)
            {
                it = m_enemyProjectiles.erase(it);
                continue;
            }

            it->position.x += it->velocity.x * deltaTime;
            it->position.y += it->velocity.y * deltaTime;

            // If bullet is reflected, hit enemies!
            if (it->isReflected)
            {
                for (auto& e : m_enemies)
                {
                    if (e.destroyed) continue;
                    float edx = e.position.x - it->position.x;
                    float edy = e.position.y - it->position.y;
                    if (sqrtf(edx * edx + edy * edy) < e.radius + it->radius)
                    {
                        // Deflector Matrix: reflected bullets deal only 50% of their original (40) damage
                        e.hp -= 20.0f;
                        e.flashTimer = 0.08f;
                        SpawnDamagePopup(e.position, 20, true);
                        if (e.hp <= 0.0f) e.destroyed = true;
                        it->lifetime = 0.0f;
                        break;
                    }
                }
            }
            else
            {
                float pDx = m_playerPos.x - it->position.x;
                float pDy = m_playerPos.y - it->position.y;
                float pDist = sqrt(pDx * pDx + pDy * pDy);
                float minDist = m_playerHitboxRadius + it->radius;

                if (pDist < minDist)
                {
                    // Reflective Shield: still only 1 hit -- reflecting consumes the same
                    // shield charge the plain absorb does, so it can't deflect indefinitely.
                    if (m_stats.reflectiveShield && m_currentShield > 0 && m_invincibleTimer <= 0.0f && !m_isDashing)
                    {
                        ConsumeShieldCharge(true);
                        it->isReflected = true;
                        it->velocity.x = -it->velocity.x * 1.6f;
                        it->velocity.y = -it->velocity.y * 1.6f;
                        it->lifetime = 3.0f;
                        TriggerShockwave(it->position, 60.0f, 0.0f);
                        continue;
                    }
                    else
                    {
                        DamagePlayer(it->damage);
                        it = m_enemyProjectiles.erase(it);
                        continue;
                    }
                }
            }

            if (it->position.x < -50.0f || it->position.x > SCREEN_WIDTH + 50.0f ||
                it->position.y < -50.0f || it->position.y > SCREEN_HEIGHT + 50.0f)
            {
                it = m_enemyProjectiles.erase(it);
                continue;
            }

            ++it;
        }

        // ==========================================
        // UPDATE FINAL BOSS ORBS (m_bossOrbs)
        // ==========================================
        DirectX::XMFLOAT2 boss4Pos{ (float)SCREEN_WIDTH * 0.5f, EnemyConfig::BossFinal.hoverY };
        bool boss4CrossFireWarning = false;
        for (const auto& ast : m_asteroids)
        {
            if (ast.isBoss && ast.bossType == 4)
            {
                boss4Pos = ast.position;
                boss4CrossFireWarning = ast.crossFireWarningActive;
                break;
            }
        }

        // Count alive destructible orbs to compute rotation speed multiplier
        int aliveDestructibleCount = 0;
        for (const auto& orb : m_bossOrbs)
        {
            if (orb.alive && !orb.isGhost) aliveDestructibleCount++;
        }
        float speedMult = (aliveDestructibleCount == 4) ? 1.0f
                        : (aliveDestructibleCount == 3) ? 1.10f
                        : (aliveDestructibleCount == 2) ? 1.20f
                        : 1.35f;

        for (auto it = m_bossOrbs.begin(); it != m_bossOrbs.end(); )
        {
            if (!it->alive)
            {
                it = m_bossOrbs.erase(it);
                continue;
            }

            // Ghost lifetime (skip for permanent Final Phase ghosts)
            if (it->isGhost && !it->isPermanent)
            {
                it->ghostLifetime -= deltaTime;
                if (it->ghostLifetime <= 0.0f)
                {
                    it = m_bossOrbs.erase(it);
                    continue;
                }
            }

            // Flash timer
            if (it->flashTimer > 0.0f) it->flashTimer -= deltaTime;

            // Orbit update. Ghost orbs use their own configured orbit speed (faster & permanent
            // in the Final Phase) and freeze in place while a Cross Fire telegraph is showing so
            // the player can read their cardinal positions before the volley fires.
            if (it->isGhost)
            {
                if (!boss4CrossFireWarning)
                {
                    float ghostSpeed = it->isPermanent
                        ? EnemyConfig::BossFinal.finalGhostOrbitSpeed
                        : EnemyConfig::BossFinal.ghostOrbOrbitSpeed;
                    it->angle += ghostSpeed * deltaTime;
                }
            }
            else
            {
                it->angle += EnemyConfig::BossFinal.orbBaseRotationSpeed * speedMult * deltaTime;
            }
            it->position = { boss4Pos.x + cosf(it->angle) * it->orbitRadius, boss4Pos.y + sinf(it->angle) * it->orbitRadius };

            // Firing patterns (Cross Fire telegraph suppresses normal per-orb firing;
            // its volley is fired explicitly once the warning timer expires)
            it->fireTimer -= deltaTime;
            if (it->fireTimer <= 0.0f && !(it->isGhost && boss4CrossFireWarning))
            {
                it->fireTimer = it->fireInterval;

                float odx = m_playerPos.x - it->position.x;
                float ody = m_playerPos.y - it->position.y;
                float baseAngle = atan2f(ody, odx);

                // Ghost orbs fire faster projectiles than the destructible OrbShield orbs
                float bSpeed = it->isGhost ? EnemyConfig::BossFinal.ghostOrbBulletSpeed : EnemyConfig::BossFinal.orbBulletSpeed;

                if (it->attackPattern == 0)
                {
                    // Pattern A: 1 aimed projectile
                    EnemyProjectile bp;
                    bp.position = it->position;
                    bp.velocity = { cosf(baseAngle) * bSpeed, sinf(baseAngle) * bSpeed };
                    bp.radius = 11.0f;
                    bp.damage = EnemyConfig::BossFinal.orbBulletDamage;
                    bp.lifetime = 4.5f;
                    bp.isBossSpiral = true;
                    m_enemyProjectiles.push_back(bp);
                }
                else if (it->attackPattern == 1)
                {
                    // Pattern B: 3-way spread
                    for (int p = -1; p <= 1; ++p)
                    {
                        float angle = baseAngle + (float)p * 0.22f;
                        EnemyProjectile bp;
                        bp.position = it->position;
                        bp.velocity = { cosf(angle) * bSpeed, sinf(angle) * bSpeed };
                        bp.radius = 11.0f;
                        bp.damage = EnemyConfig::BossFinal.orbBulletDamage;
                        bp.lifetime = 4.5f;
                        bp.isBossSpiral = true;
                        m_enemyProjectiles.push_back(bp);
                    }
                }
                else if (it->attackPattern == 2)
                {
                    // Pattern C: 6-direction radial burst
                    float startRot = m_totalTime * 2.0f;
                    for (int p = 0; p < 6; ++p)
                    {
                        float angle = startRot + (float)p * (2.0f * PI / 6.0f);
                        EnemyProjectile bp;
                        bp.position = it->position;
                        bp.velocity = { cosf(angle) * bSpeed, sinf(angle) * bSpeed };
                        bp.radius = 11.0f;
                        bp.damage = EnemyConfig::BossFinal.orbBulletDamage;
                        bp.lifetime = 4.5f;
                        bp.isBossSpiral = true;
                        m_enemyProjectiles.push_back(bp);
                    }
                }
                else if (it->attackPattern == 3)
                {
                    // Ghost Pattern 1 (Rotating Spiral): single shot fired outward along the
                    // orb's current orbit angle. The spiral shape emerges purely from the orb
                    // continuously rotating while this fires periodically -- no spiral math needed.
                    EnemyProjectile bp;
                    bp.position = it->position;
                    bp.velocity = { cosf(it->angle) * bSpeed, sinf(it->angle) * bSpeed };
                    bp.radius = 11.0f;
                    bp.damage = EnemyConfig::BossFinal.orbBulletDamage;
                    bp.lifetime = 4.5f;
                    bp.isBossSpiral = true;
                    m_enemyProjectiles.push_back(bp);
                }
            }

            // Player vs Orb collision
            float pDx = m_playerPos.x - it->position.x;
            float pDy = m_playerPos.y - it->position.y;
            float pDist = sqrtf(pDx * pDx + pDy * pDy);
            if (pDist < m_playerHitboxRadius + it->radius)
            {
                DamagePlayer(1);
            }

            ++it;
        }

        // ==========================================
        // UPDATE FINAL BOSS BLADES (m_bossBlades)
        // ==========================================
        for (auto it = m_bossBlades.begin(); it != m_bossBlades.end(); )
        {
            if (it->state == BladeState::Warning)
            {
                it->warningTimer -= deltaTime;
                if (it->warningTimer <= 0.0f)
                {
                    it->state = BladeState::Falling;
                    it->position.y = -80.0f;
                }
            }
            else if (it->state == BladeState::Falling)
            {
                it->position.y += EnemyConfig::BossFinal.bladeFallSpeed * deltaTime;
                if (it->position.y >= it->targetY)
                {
                    it->position.y = it->targetY;
                    it->state = BladeState::Embedded;
                    TriggerCameraShake(0.15f, 3.5f);
                    // Impact sparks
                    VFXInstance spark;
                    spark.position = it->position;
                    spark.lifetime = 0.0f;
                    spark.maxLifetime = 0.35f;
                    spark.scale = 1.2f;
                    spark.isMultiTexture = true;
                    spark.textureSequence = m_texExplosions;
                    m_vfxs.push_back(spark);
                }
            }
            else if (it->state == BladeState::Embedded)
            {
                it->lifetime -= deltaTime;
                if (it->lifetime <= 0.0f)
                {
                    it = m_bossBlades.erase(it);
                    continue;
                }

                // 4-Way Pulse Attack every ~1.5s
                it->pulseTimer -= deltaTime;
                if (it->pulseTimer <= 0.0f)
                {
                    it->pulseTimer = it->pulseInterval;
                    float angles[4] = { -PI * 0.5f, PI * 0.5f, PI, 0.0f }; // Up, Down, Left, Right
                    for (int a = 0; a < 4; ++a)
                    {
                        EnemyProjectile bp;
                        bp.position = it->position;
                        bp.velocity = { cosf(angles[a]) * EnemyConfig::BossFinal.bladePulseBulletSpeed, sinf(angles[a]) * EnemyConfig::BossFinal.bladePulseBulletSpeed };
                        bp.radius = 10.0f;
                        bp.damage = 1;
                        bp.lifetime = 4.0f;
                        bp.isBossSpiral = true;
                        m_enemyProjectiles.push_back(bp);
                    }
                }

                // Player vs Embedded Blade collision
                float pDx = m_playerPos.x - it->position.x;
                float pDy = m_playerPos.y - it->position.y;
                float pDist = sqrtf(pDx * pDx + pDy * pDy);
                if (pDist < m_playerHitboxRadius + it->radius)
                {
                    DamagePlayer(1);
                }
            }

            ++it;
        }

        // Update Asteroids & Bosses
        for (auto it = m_asteroids.begin(); it != m_asteroids.end(); )
        {
            if (it->destroyed)
            {
                if (it->isBoss)
                {
                    m_runStats.enemiesKilled++;

                    TriggerCameraShake(0.95f, 26.0f);
                    TriggerShockwave(it->position, 360.0f, 0.0f);

                    // Core Explosion VFX on boss
                    VFXInstance coreExp;
                    coreExp.position = it->position;
                    coreExp.lifetime = 0.0f;
                    coreExp.maxLifetime = 0.70f;
                    coreExp.scale = 3.8f;
                    coreExp.isMultiTexture = true;
                    coreExp.textureSequence = m_texExplosions;
                    m_vfxs.push_back(coreExp);

                    if (m_soundPat != -1) PlayAudio(m_soundPat);

                    // Drops calculation
                    int reishiDropCount = (it->bossType == 4) ? EnemyConfig::BossFinal.reishiDropCount : (it->bossType == 3) ? EnemyConfig::Boss3.reishiDropCount : (it->bossType == 2) ? EnemyConfig::Boss2.reishiDropCount : EnemyConfig::Boss1.reishiDropCount;
                    int reishiPerDrop   = (it->bossType == 4) ? EnemyConfig::BossFinal.reishiPerDrop   : (it->bossType == 3) ? EnemyConfig::Boss3.reishiPerDrop   : (it->bossType == 2) ? EnemyConfig::Boss2.reishiPerDrop   : EnemyConfig::Boss1.reishiPerDrop;
                    int vidaDropCount   = (it->bossType == 4) ? EnemyConfig::BossFinal.vidaDropCount   : (it->bossType == 3) ? EnemyConfig::Boss3.vidaDropCount   : (it->bossType == 2) ? EnemyConfig::Boss2.vidaDropCount   : EnemyConfig::Boss1.vidaDropCount;
                    int disliDropCount  = (it->bossType == 4) ? EnemyConfig::BossFinal.disliDropCount  : (it->bossType == 3) ? EnemyConfig::Boss3.disliDropCount  : (it->bossType == 2) ? EnemyConfig::Boss2.disliDropCount  : EnemyConfig::Boss1.disliDropCount;
                    int cpuDropCount    = (it->bossType == 4) ? EnemyConfig::BossFinal.cpuDropCount    : (it->bossType == 3) ? EnemyConfig::Boss3.cpuDropCount    : (it->bossType == 2) ? EnemyConfig::Boss2.cpuDropCount    : EnemyConfig::Boss1.cpuDropCount;
                    int keyDropCount    = (it->bossType == 4) ? EnemyConfig::BossFinal.keyDropCount    : (it->bossType == 3) ? EnemyConfig::Boss3.keyDropCount    : (it->bossType == 2) ? EnemyConfig::Boss2.keyDropCount    : EnemyConfig::Boss1.keyDropCount;

                    // 1. Reishi Crystals
                    for (int k = 0; k < reishiDropCount; ++k)
                    {
                        ResourcePickup r;
                        r.position = it->position;
                        float angle = RandomFloat(0.0f, 2.0f * PI);
                        float spd = RandomFloat(120.0f, 280.0f);
                        r.velocity = { cosf(angle) * spd, sinf(angle) * spd };
                        r.scale = 0.09f;
                        r.type = PickupType::Reishi;
                        r.amount = reishiPerDrop;
                        m_pickups.push_back(r);
                    }
                    // 2. Screws
                    for (int k = 0; k < vidaDropCount; ++k)
                    {
                        ResourcePickup v;
                        v.position = it->position;
                        float angle = RandomFloat(0.0f, 2.0f * PI);
                        float spd = RandomFloat(100.0f, 240.0f);
                        v.velocity = { cosf(angle) * spd, sinf(angle) * spd };
                        v.scale = 0.09f;
                        v.type = PickupType::Vida;
                        v.amount = 1;
                        m_pickups.push_back(v);
                    }
                    // 3. Gears
                    for (int k = 0; k < disliDropCount; ++k)
                    {
                        ResourcePickup d;
                        d.position = it->position;
                        float angle = RandomFloat(0.0f, 2.0f * PI);
                        float spd = RandomFloat(90.0f, 220.0f);
                        d.velocity = { cosf(angle) * spd, sinf(angle) * spd };
                        d.scale = 0.09f;
                        d.type = PickupType::Disli;
                        d.amount = 1;
                        m_pickups.push_back(d);
                    }
                    // 4. CPUs
                    for (int k = 0; k < cpuDropCount; ++k)
                    {
                        ResourcePickup c;
                        c.position = it->position;
                        float angle = RandomFloat(0.0f, 2.0f * PI);
                        float spd = RandomFloat(80.0f, 200.0f);
                        c.velocity = { cosf(angle) * spd, sinf(angle) * spd };
                        c.scale = 0.09f;
                        c.type = PickupType::Cpu;
                        c.amount = 1;
                        m_pickups.push_back(c);
                    }
                    // 5. Keys
                    for (int k = 0; k < keyDropCount; ++k)
                    {
                        ResourcePickup ky;
                        ky.position = it->position;
                        float angle = RandomFloat(0.0f, 2.0f * PI);
                        float spd = RandomFloat(70.0f, 180.0f);
                        ky.velocity = { cosf(angle) * spd, sinf(angle) * spd };
                        ky.scale = 0.11f;
                        ky.type = PickupType::Key;
                        ky.amount = 1;
                        m_pickups.push_back(ky);
                    }

                    if (m_calamity.level >= 5)
                    {
                        m_sector5DefeatedCount++;
                        // If queue has remaining bosses, spawn next one!
                        if (!m_sector5BossQueue.empty())
                        {
                            int nextBoss = m_sector5BossQueue.back();
                            m_sector5BossQueue.pop_back();
                            float spawnX = RandomFloat((float)SCREEN_WIDTH * 0.25f, (float)SCREEN_WIDTH * 0.75f);
                            SpawnBoss(nextBoss, spawnX, -150.0f);
                        }

                        // Check if all bosses are defeated in Sector 5
                        bool anyOtherBossAlive = false;
                        for (const auto& otherAst : m_asteroids)
                        {
                            if (&otherAst != &(*it) && otherAst.isBoss && !otherAst.destroyed)
                            {
                                anyOtherBossAlive = true;
                                break;
                            }
                        }

                        if (!anyOtherBossAlive && m_sector5BossQueue.empty())
                        {
                            m_bossVictory = true;
                            m_runState = RunState::BossDefeated;
                            m_bossDeathTimer = 3.6f;
                            m_defeatedBossPos = it->position;
                            m_superVacuumActive = true;
                            m_enemyProjectiles.clear();
                            m_bossOrbs.clear();
                            m_bossBlades.clear();

                            m_upgradeTree.UnlockNextSector(5);
                            m_upgradeTree.SetCurrentSectorIndex(5);
                        }
                    }
                    else
                    {
                        m_bossVictory = true;
                        m_runState = RunState::BossDefeated;
                        m_bossDeathTimer = 3.6f;
                        m_defeatedBossPos = it->position;
                        m_superVacuumActive = true;
                        m_enemyProjectiles.clear();
                        m_bossOrbs.clear();
                        m_bossBlades.clear();

                        int defeatedLevel = std::max(it->bossType, m_calamity.level);
                        m_upgradeTree.UnlockNextSector(defeatedLevel);
                        m_upgradeTree.SetCurrentSectorIndex(std::min(5, defeatedLevel + 1));
                    }
                }
                else
                {
                    if (m_soundPat != -1) PlayAudio(m_soundPat);

                    // Anomalous asteroid drops guaranteed Key!
                    if (it->isAnomalousSignal)
                    {
                        ResourcePickup ky;
                        ky.position = it->position;
                        float angle = RandomFloat(0.0f, 2.0f * PI);
                        float spd = RandomFloat(80.0f, 160.0f);
                        ky.velocity = { cosf(angle) * spd, sinf(angle) * spd };
                        ky.scale = 0.11f;
                        ky.type = PickupType::Key;
                        ky.amount = 1;
                        m_pickups.push_back(ky);
                    }

                    // Regular asteroid drops: Reishi + chance of Vida/Dişli
                    bool isJackpot = (m_stats.jackpotChance > 0.0f && RandomFloat(0.0f, 1.0f) < m_stats.jackpotChance);
                    int dropNum = isJackpot ? (it->resourceAmount * 4 + 8) : it->resourceAmount;

                    if (isJackpot)
                    {
                        TriggerCameraShake(0.35f, 8.0f);
                        TriggerShockwave(it->position, 150.0f, 0.0f);
                        SpawnDamagePopup(it->position, 777, true, false, false);
                    }

                    // 1. Reishi drops
                    for (int k = 0; k < dropNum; ++k)
                    {
                        ResourcePickup crystal;
                        crystal.position = it->position;
                        float angle = RandomFloat(0.0f, 2.0f * PI);
                        float speed = RandomFloat(100.0f, isJackpot ? 260.0f : 200.0f);
                        crystal.velocity.x = cosf(angle) * speed;
                        crystal.velocity.y = sinf(angle) * speed;
                        crystal.scale = isJackpot ? 0.10f : RandomFloat(0.07f, 0.09f);
                        crystal.type = PickupType::Reishi;
                        crystal.amount = isJackpot ? 2 : 1;
                        m_pickups.push_back(crystal);
                    }

                    // 2. Rare Materials (Vida, Dişli, CPU)
                    float rareRoll = RandomFloat(0.0f, 1.0f);
                    float vidaThreshold = 0.25f * (1.0f + m_stats.vidaBonus);
                    float disliThreshold = 0.15f * (1.0f + m_stats.disliBonus);
                    float cpuThreshold = 0.06f * (1.0f + m_stats.cpuBonus);

                    if (isJackpot)
                    {
                        // Guaranteed loot fiesta on jackpot!
                        for (int k = 0; k < 2; ++k)
                        {
                            ResourcePickup v; v.position = it->position;
                            float a = RandomFloat(0.0f, 2.0f * PI); float sp = RandomFloat(80.0f, 180.0f);
                            v.velocity = { cosf(a) * sp, sinf(a) * sp }; v.scale = 0.085f;
                            v.type = PickupType::Vida; v.amount = 1; m_pickups.push_back(v);
                        }
                        ResourcePickup d; d.position = it->position;
                        float a = RandomFloat(0.0f, 2.0f * PI); float sp = RandomFloat(80.0f, 180.0f);
                        d.velocity = { cosf(a) * sp, sinf(a) * sp }; d.scale = 0.085f;
                        d.type = PickupType::Disli; d.amount = 1; m_pickups.push_back(d);
                    }
                    else if (rareRoll < cpuThreshold)
                    {
                        ResourcePickup item; item.position = it->position;
                        float a = RandomFloat(0.0f, 2.0f * PI); float sp = RandomFloat(70.0f, 150.0f);
                        item.velocity = { cosf(a) * sp, sinf(a) * sp }; item.scale = 0.085f;
                        item.type = PickupType::Cpu; item.amount = 1; m_pickups.push_back(item);
                    }
                    else if (rareRoll < (cpuThreshold + disliThreshold))
                    {
                        ResourcePickup item; item.position = it->position;
                        float a = RandomFloat(0.0f, 2.0f * PI); float sp = RandomFloat(70.0f, 150.0f);
                        item.velocity = { cosf(a) * sp, sinf(a) * sp }; item.scale = 0.085f;
                        item.type = PickupType::Disli; item.amount = 1; m_pickups.push_back(item);
                    }
                    else if (rareRoll < (cpuThreshold + disliThreshold + vidaThreshold))
                    {
                        ResourcePickup item; item.position = it->position;
                        float a = RandomFloat(0.0f, 2.0f * PI); float sp = RandomFloat(70.0f, 150.0f);
                        item.velocity = { cosf(a) * sp, sinf(a) * sp }; item.scale = 0.085f;
                        item.type = PickupType::Vida; item.amount = 1; m_pickups.push_back(item);
                    }

                    // 3. Chain Fracture: crack nearby asteroids!
                    if (m_stats.chainFracture)
                    {
                        for (auto& otherAst : m_asteroids)
                        {
                            if (&otherAst == &(*it) || otherAst.destroyed || otherAst.isBoss) continue;
                            float cdx = otherAst.position.x - it->position.x;
                            float cdy = otherAst.position.y - it->position.y;
                            float cdist = sqrtf(cdx * cdx + cdy * cdy);
                            if (cdist <= 150.0f)
                            {
                                otherAst.hp -= 45.0f;
                                otherAst.flashTimer = 0.10f;
                                SpawnDamagePopup(otherAst.position, 45, false, false, true);
                                if (otherAst.hp <= 0.0f) otherAst.destroyed = true;
                            }
                        }
                    }

                    // Advance Calamity / Boss Meter by mining asteroids
                    m_calamity.current += 3.5f;
                    m_runStats.asteroidsMined++;
                    TriggerTutorialAsteroidMilestone();

                    // Zero Point Reactor fuel refill
                    if (m_stats.zeroPointReactor)
                    {
                        m_fuel = std::min(m_stats.maxFuel, m_fuel + (m_stats.activeCapstone == 3 ? 10.0f : 4.0f));
                    }
                }

                it = m_asteroids.erase(it);
                continue;
            }

            if (it->flashTimer > 0.0f)
            {
                it->flashTimer -= deltaTime;
            }

            // Boss 2 Custom AI & Attack Phase Logic
            if (it->isBoss && it->bossType == 2)
            {
                if (it->bossPhase == BossPhase::Enter)
                {
                    it->position.y += it->velocity.y * deltaTime;
                    it->rotation += it->rotationSpeed * deltaTime;
                    if (it->position.y >= it->bossTargetPos.y)
                    {
                        it->position.y = it->bossTargetPos.y;
                        it->bossPhase = BossPhase::Patrol;
                        it->bossPhaseTimer = EnemyConfig::Boss2.patrolDuration;
                        it->velocity = { 0.0f, 0.0f };
                    }
                }
                else if (it->bossPhase == BossPhase::Patrol)
                {
                    it->rotation += EnemyConfig::Boss2.normalRotationSpeed * deltaTime;
                    float bdx = it->bossTargetPos.x - it->position.x;
                    float bdy = it->bossTargetPos.y - it->position.y;
                    float bdist = sqrtf(bdx * bdx + bdy * bdy);
                    if (bdist > 15.0f)
                    {
                        it->position.x += (bdx / bdist) * EnemyConfig::Boss2.moveSpeed * deltaTime;
                        it->position.y += (bdy / bdist) * EnemyConfig::Boss2.moveSpeed * deltaTime;
                    }
                    else
                    {
                        it->bossTargetPos = { RandomFloat(250.0f, (float)SCREEN_WIDTH - 250.0f), RandomFloat(160.0f, 320.0f) };
                    }

                    // Periodic 4-way pulse shots
                    it->bossShootTimer -= deltaTime;
                    if (it->bossShootTimer <= 0.0f)
                    {
                        it->bossShootTimer = EnemyConfig::Boss2.normalShootInterval;
                        for (int k = 0; k < 4; ++k)
                        {
                            float ang = it->rotation + (float)k * (PI * 0.5f);
                            EnemyProjectile bp;
                            bp.position = it->position;
                            bp.velocity.x = cosf(ang) * EnemyConfig::Boss2.normalBulletSpeed;
                            bp.velocity.y = sinf(ang) * EnemyConfig::Boss2.normalBulletSpeed;
                            bp.radius = 12.0f;
                            bp.damage = EnemyConfig::Boss2.normalBulletDamage;
                            bp.lifetime = 4.5f;
                            bp.isBossSpiral = true;
                            m_enemyProjectiles.push_back(bp);
                        }
                        if (m_soundShoot != -1) PlayAudio(m_soundShoot);
                    }

                    it->bossPhaseTimer -= deltaTime;
                    if (it->bossPhaseTimer <= 0.0f)
                    {
                        it->bossPhase = BossPhase::AlarmWarning;
                        it->bossPhaseTimer = EnemyConfig::Boss2.alarmWarningDuration;
                    }
                }
                else if (it->bossPhase == BossPhase::AlarmWarning)
                {
                    // Stationary & warning alarm charge
                    it->rotation += (EnemyConfig::Boss2.normalRotationSpeed * 0.5f) * deltaTime;
                    it->bossPhaseTimer -= deltaTime;
                    if (it->bossPhaseTimer <= 0.0f)
                    {
                        it->bossPhase = BossPhase::SpiralAttack;
                        it->bossPhaseTimer = EnemyConfig::Boss2.spiralAttackDuration;
                        it->bossSpiralFireTimer = 0.0f;
                        TriggerCameraShake(0.20f, 5.0f);
                    }
                }
                else if (it->bossPhase == BossPhase::SpiralAttack)
                {
                    // Rapid spinning & 4-way spiral projectile barrage!
                    it->rotation += EnemyConfig::Boss2.spiralRotationSpeed * deltaTime;
                    if (it->rotation > 2.0f * PI) it->rotation -= 2.0f * PI;

                    it->bossSpiralFireTimer -= deltaTime;
                    if (it->bossSpiralFireTimer <= 0.0f)
                    {
                        it->bossSpiralFireTimer = EnemyConfig::Boss2.spiralFireRate;
                        for (int k = 0; k < 4; ++k)
                        {
                            float ang = it->rotation + (float)k * (PI * 0.5f);
                            EnemyProjectile bp;
                            bp.position.x = it->position.x + cosf(ang) * (it->radius * 0.70f);
                            bp.position.y = it->position.y + sinf(ang) * (it->radius * 0.70f);
                            bp.velocity.x = cosf(ang) * EnemyConfig::Boss2.spiralBulletSpeed;
                            bp.velocity.y = sinf(ang) * EnemyConfig::Boss2.spiralBulletSpeed;
                            bp.radius = EnemyConfig::Boss2.spiralBulletRadius;
                            bp.damage = EnemyConfig::Boss2.spiralBulletDamage;
                            bp.lifetime = EnemyConfig::Boss2.spiralBulletLifetime;
                            bp.isBossSpiral = true;
                            m_enemyProjectiles.push_back(bp);
                        }
                        if (m_soundShoot != -1) PlayAudio(m_soundShoot);
                    }

                    it->bossPhaseTimer -= deltaTime;
                    if (it->bossPhaseTimer <= 0.0f)
                    {
                        it->bossPhase = BossPhase::Cooldown;
                        it->bossPhaseTimer = EnemyConfig::Boss2.spiralCooldown;
                    }
                }
                else if (it->bossPhase == BossPhase::Cooldown)
                {
                    it->rotation += EnemyConfig::Boss2.normalRotationSpeed * deltaTime;
                    it->bossPhaseTimer -= deltaTime;
                    if (it->bossPhaseTimer <= 0.0f)
                    {
                        it->bossPhase = BossPhase::Patrol;
                        it->bossPhaseTimer = EnemyConfig::Boss2.patrolDuration;
                        it->bossTargetPos = { RandomFloat(250.0f, (float)SCREEN_WIDTH - 250.0f), RandomFloat(160.0f, 320.0f) };
                    }
                }
            }
            // Boss 3 (Torii Yokai) Multi-Phase Death Laser Logic
            else if (it->isBoss && it->bossType == 3)
            {
                DirectX::XMFLOAT2 eyePos = { it->position.x, it->position.y + 10.0f };
                float hpPct = std::clamp(it->hp / it->maxHp, 0.0f, 1.0f);

                if (it->bossPhase == BossPhase::Enter)
                {
                    it->position.y += it->velocity.y * deltaTime;
                    if (it->position.y >= EnemyConfig::Boss3.hoverY)
                    {
                        it->position.y = EnemyConfig::Boss3.hoverY;
                        it->bossPhase = BossPhase::GlideTop;
                        it->bossPhaseTimer = EnemyConfig::Boss3.glideDuration;
                        it->bossTargetPos = { RandomFloat(250.0f, (float)SCREEN_WIDTH - 250.0f), EnemyConfig::Boss3.hoverY };
                        it->velocity = { 0.0f, 0.0f };
                    }
                }
                else if (it->bossPhase == BossPhase::GlideTop)
                {
                    // Gentle vertical levitation bobbing
                    float hoverBob = sinf(m_totalTime * 2.8f) * 6.0f;
                    it->position.y = EnemyConfig::Boss3.hoverY + hoverBob;

                    // Smooth horizontal glide across the top
                    float bdx = it->bossTargetPos.x - it->position.x;
                    if (fabsf(bdx) > 15.0f)
                    {
                        float dirX = (bdx > 0.0f) ? 1.0f : -1.0f;
                        it->position.x += dirX * EnemyConfig::Boss3.moveSpeed * deltaTime;
                    }
                    else
                    {
                        // Pick new glide target on opposite side of screen
                        float newX = (it->position.x < (float)SCREEN_WIDTH * 0.5f)
                            ? RandomFloat((float)SCREEN_WIDTH * 0.55f, (float)SCREEN_WIDTH - 220.0f)
                            : RandomFloat(220.0f, (float)SCREEN_WIDTH * 0.45f);
                        it->bossTargetPos = { newX, EnemyConfig::Boss3.hoverY };
                    }

                    it->bossPhaseTimer -= deltaTime;
                    if (it->bossPhaseTimer <= 0.0f)
                    {
                        it->bossPhase = BossPhase::MoveToCenter;
                        it->bossTargetPos = { (float)SCREEN_WIDTH * 0.5f, EnemyConfig::Boss3.hoverY };
                    }
                }
                else if (it->bossPhase == BossPhase::MoveToCenter)
                {
                    // Move directly to top center
                    float bdx = it->bossTargetPos.x - it->position.x;
                    float bdy = it->bossTargetPos.y - it->position.y;
                    float bdist = sqrtf(bdx * bdx + bdy * bdy);

                    if (bdist > 10.0f)
                    {
                        it->position.x += (bdx / bdist) * (EnemyConfig::Boss3.moveSpeed * 1.35f) * deltaTime;
                        it->position.y += (bdy / bdist) * (EnemyConfig::Boss3.moveSpeed * 1.35f) * deltaTime;
                    }
                    else
                    {
                        it->position.x = (float)SCREEN_WIDTH * 0.5f;
                        it->position.y = EnemyConfig::Boss3.hoverY;

                        // Branch attack type based on HP percentage
                        if (hpPct > 0.70f)
                        {
                            // Phase 1: Aimed Sweeping Laser
                            it->bossPhase = BossPhase::LaserTrack;
                            it->bossPhaseTimer = EnemyConfig::Boss3.aimTrackingDuration;
                            it->bossLaserAngle = 1.5707963f; // 90 deg, pointing straight down
                        }
                        else if (hpPct > 0.40f)
                        {
                            // Phase 2: Super High-Density Vertical Laser Curtain Wall (16 columns across screen)
                            it->bossPhase = BossPhase::CurtainWarning;
                            it->bossPhaseTimer = EnemyConfig::Boss3.curtainWarningDuration;
                            it->boss3SafeGapIndex = RandomInt(2, 5);  // Safe gap 1
                            it->boss3SafeGapIndex2 = RandomInt(10, 13); // Safe gap 2
                        }
                        else
                        {
                            // Phase 3: Super High-Density Crosshatch Grid Laser Matrix (30 intersecting lines)
                            it->bossPhase = BossPhase::GridWarning;
                            it->bossPhaseTimer = EnemyConfig::Boss3.gridWarningDuration;
                        }
                    }
                }
                // PHASE 1: Aimed Sweeping Beam
                else if (it->bossPhase == BossPhase::LaserTrack)
                {
                    it->position.x = (float)SCREEN_WIDTH * 0.5f;
                    it->position.y = EnemyConfig::Boss3.hoverY + sinf(m_totalTime * 4.0f) * 2.0f;

                    float targetAngle = atan2f(m_playerPos.y - eyePos.y, m_playerPos.x - eyePos.x);
                    targetAngle = std::clamp(targetAngle, EnemyConfig::Boss3.laserMinAngle, EnemyConfig::Boss3.laserMaxAngle);

                    float diff = targetAngle - it->bossLaserAngle;
                    while (diff > PI)  diff -= 2.0f * PI;
                    while (diff < -PI) diff += 2.0f * PI;

                    float turnStep = EnemyConfig::Boss3.aimTrackingTurnSpeed * deltaTime;
                    if (fabsf(diff) <= turnStep)
                    {
                        it->bossLaserAngle = targetAngle;
                    }
                    else
                    {
                        it->bossLaserAngle += (diff > 0.0f ? turnStep : -turnStep);
                    }
                    it->bossLaserAngle = std::clamp(it->bossLaserAngle, EnemyConfig::Boss3.laserMinAngle, EnemyConfig::Boss3.laserMaxAngle);

                    it->bossPhaseTimer -= deltaTime;
                    if (it->bossPhaseTimer <= 0.0f)
                    {
                        it->bossPhase = BossPhase::LaserLock;
                        it->bossPhaseTimer = EnemyConfig::Boss3.aimLockPauseDuration;
                        TriggerCameraShake(0.18f, 3.5f);
                    }
                }
                else if (it->bossPhase == BossPhase::LaserLock)
                {
                    it->position.x = (float)SCREEN_WIDTH * 0.5f;
                    it->position.y = EnemyConfig::Boss3.hoverY + sinf(m_totalTime * 8.0f) * 2.0f;

                    it->bossPhaseTimer -= deltaTime;
                    if (it->bossPhaseTimer <= 0.0f)
                    {
                        it->bossPhase = BossPhase::LaserFire;
                        it->bossPhaseTimer = EnemyConfig::Boss3.laserFiringDuration;
                        it->bossLaserDamageTimer = 0.0f;
                        TriggerCameraShake(0.40f, 6.5f);
                        if (m_soundShoot != -1) PlayAudio(m_soundShoot);
                    }
                }
                else if (it->bossPhase == BossPhase::LaserFire)
                {
                    it->position.x = (float)SCREEN_WIDTH * 0.5f;
                    it->position.y = EnemyConfig::Boss3.hoverY + sinf(m_totalTime * 12.0f) * 2.0f;

                    float timeElapsedInFire = EnemyConfig::Boss3.laserFiringDuration - it->bossPhaseTimer;
                    if (timeElapsedInFire <= EnemyConfig::Boss3.laserActiveTrackingDuration)
                    {
                        float targetAngle = atan2f(m_playerPos.y - eyePos.y, m_playerPos.x - eyePos.x);
                        targetAngle = std::clamp(targetAngle, EnemyConfig::Boss3.laserMinAngle, EnemyConfig::Boss3.laserMaxAngle);

                        float diff = targetAngle - it->bossLaserAngle;
                        while (diff > PI)  diff -= 2.0f * PI;
                        while (diff < -PI) diff += 2.0f * PI;

                        float turnStep = EnemyConfig::Boss3.laserTrackingTurnSpeed * deltaTime;
                        if (fabsf(diff) <= turnStep)
                        {
                            it->bossLaserAngle = targetAngle;
                        }
                        else
                        {
                            it->bossLaserAngle += (diff > 0.0f ? turnStep : -turnStep);
                        }
                        it->bossLaserAngle = std::clamp(it->bossLaserAngle, EnemyConfig::Boss3.laserMinAngle, EnemyConfig::Boss3.laserMaxAngle);
                    }

                    TriggerCameraShake(0.08f, 3.2f);

                    // Player Hitbox vs Sweeping Beam Line
                    DirectX::XMFLOAT2 beamDir = { cosf(it->bossLaserAngle), sinf(it->bossLaserAngle) };
                    float beamLen = 1400.0f;
                    DirectX::XMFLOAT2 beamEnd = { eyePos.x + beamDir.x * beamLen, eyePos.y + beamDir.y * beamLen };

                    float abX = beamEnd.x - eyePos.x;
                    float abY = beamEnd.y - eyePos.y;
                    float apX = m_playerPos.x - eyePos.x;
                    float apY = m_playerPos.y - eyePos.y;
                    float abLenSq = abX * abX + abY * abY;
                    float t = (abLenSq > 0.001f) ? std::clamp((apX * abX + apY * abY) / abLenSq, 0.0f, 1.0f) : 0.0f;
                    DirectX::XMFLOAT2 closestPt = { eyePos.x + t * abX, eyePos.y + t * abY };

                    float dX = m_playerPos.x - closestPt.x;
                    float dY = m_playerPos.y - closestPt.y;
                    float distToBeam = sqrtf(dX * dX + dY * dY);

                    float hitThreshold = (EnemyConfig::Boss3.laserBeamWidth * 0.5f) + m_playerHitboxRadius;
                    if (distToBeam < hitThreshold)
                    {
                        it->bossLaserDamageTimer -= deltaTime;
                        if (it->bossLaserDamageTimer <= 0.0f)
                        {
                            it->bossLaserDamageTimer = EnemyConfig::Boss3.laserDamageInterval;
                            DamagePlayer(1);
                            TriggerCameraShake(0.20f, 6.0f);
                        }
                    }

                    it->bossPhaseTimer -= deltaTime;
                    if (it->bossPhaseTimer <= 0.0f)
                    {
                        it->bossPhase = BossPhase::Cooldown;
                        it->bossPhaseTimer = EnemyConfig::Boss3.laserCooldownDuration;
                    }
                }
                // PHASE 2: Super High-Density Vertical Laser Curtain Wall
                else if (it->bossPhase == BossPhase::CurtainWarning)
                {
                    it->position.x = (float)SCREEN_WIDTH * 0.5f;
                    it->position.y = EnemyConfig::Boss3.hoverY + sinf(m_totalTime * 6.0f) * 2.0f;

                    it->bossPhaseTimer -= deltaTime;
                    if (it->bossPhaseTimer <= 0.0f)
                    {
                        it->bossPhase = BossPhase::CurtainFire;
                        it->bossPhaseTimer = EnemyConfig::Boss3.curtainFiringDuration;
                        it->bossLaserDamageTimer = 0.0f;
                        TriggerCameraShake(0.35f, 6.0f);
                        if (m_soundShoot != -1) PlayAudio(m_soundShoot);
                    }
                }
                else if (it->bossPhase == BossPhase::CurtainFire)
                {
                    it->position.x = (float)SCREEN_WIDTH * 0.5f;
                    it->position.y = EnemyConfig::Boss3.hoverY + sinf(m_totalTime * 10.0f) * 2.0f;

                    TriggerCameraShake(0.08f, 3.0f);

                    // Check player collision vs non-safe columns (16 columns across screen)
                    float colSpacing = (float)SCREEN_WIDTH / 16.0f;
                    for (int c = 0; c < 16; ++c)
                    {
                        if (c == it->boss3SafeGapIndex || c == it->boss3SafeGapIndex2) continue; // Natural empty gap!

                        float colX = colSpacing * ((float)c + 0.5f);
                        if (fabsf(m_playerPos.x - colX) < (13.0f + m_playerHitboxRadius))
                        {
                            it->bossLaserDamageTimer -= deltaTime;
                            if (it->bossLaserDamageTimer <= 0.0f)
                            {
                                it->bossLaserDamageTimer = EnemyConfig::Boss3.laserDamageInterval;
                                DamagePlayer(1);
                                TriggerCameraShake(0.20f, 6.0f);
                            }
                        }
                    }

                    it->bossPhaseTimer -= deltaTime;
                    if (it->bossPhaseTimer <= 0.0f)
                    {
                        it->bossPhase = BossPhase::Cooldown;
                        it->bossPhaseTimer = EnemyConfig::Boss3.laserCooldownDuration;
                    }
                }
                // PHASE 3: Super High-Density Crosshatch Grid Laser Matrix (30 Intersecting Lines)
                else if (it->bossPhase == BossPhase::GridWarning)
                {
                    it->position.x = (float)SCREEN_WIDTH * 0.5f;
                    it->position.y = EnemyConfig::Boss3.hoverY + sinf(m_totalTime * 6.0f) * 2.0f;

                    it->bossPhaseTimer -= deltaTime;
                    if (it->bossPhaseTimer <= 0.0f)
                    {
                        it->bossPhase = BossPhase::GridFire;
                        it->bossPhaseTimer = EnemyConfig::Boss3.gridFiringDuration;
                        it->bossLaserDamageTimer = 0.0f;
                        TriggerCameraShake(0.40f, 7.0f);
                        if (m_soundShoot != -1) PlayAudio(m_soundShoot);
                    }
                }
                else if (it->bossPhase == BossPhase::GridFire)
                {
                    it->position.x = (float)SCREEN_WIDTH * 0.5f;
                    it->position.y = EnemyConfig::Boss3.hoverY + sinf(m_totalTime * 12.0f) * 2.0f;

                    TriggerCameraShake(0.09f, 3.5f);

                    // Check collision with dense ızgara laser grid lines (k in -7..7, total 30 lines)
                    bool hitGrid = false;
                    float gridSpacing = 140.0f;

                    // 15 Left-to-Right diagonal lines: x - y = C1[k]
                    for (int k = -7; k <= 7; ++k)
                    {
                        float C1 = (float)k * gridSpacing;
                        float d1 = fabsf(m_playerPos.x - m_playerPos.y - C1) / 1.4142f;
                        if (d1 < (10.0f + m_playerHitboxRadius)) { hitGrid = true; break; }
                    }

                    // 15 Right-to-Left diagonal lines: x + y - midSum = C2[k]
                    if (!hitGrid)
                    {
                        float midSum = (float)SCREEN_WIDTH * 0.5f + (float)SCREEN_HEIGHT * 0.5f;
                        for (int k = -7; k <= 7; ++k)
                        {
                            float C2 = midSum + (float)k * gridSpacing;
                            float d2 = fabsf(m_playerPos.x + m_playerPos.y - C2) / 1.4142f;
                            if (d2 < (10.0f + m_playerHitboxRadius)) { hitGrid = true; break; }
                        }
                    }

                    if (hitGrid)
                    {
                        it->bossLaserDamageTimer -= deltaTime;
                        if (it->bossLaserDamageTimer <= 0.0f)
                        {
                            it->bossLaserDamageTimer = EnemyConfig::Boss3.laserDamageInterval;
                            DamagePlayer(1);
                            TriggerCameraShake(0.20f, 6.0f);
                        }
                    }

                    it->bossPhaseTimer -= deltaTime;
                    if (it->bossPhaseTimer <= 0.0f)
                    {
                        it->bossPhase = BossPhase::Cooldown;
                        it->bossPhaseTimer = EnemyConfig::Boss3.laserCooldownDuration;
                    }
                }
                else if (it->bossPhase == BossPhase::Cooldown)
                {
                    it->bossPhaseTimer -= deltaTime;
                    if (it->bossPhaseTimer <= 0.0f)
                    {
                        it->bossPhase = BossPhase::GlideTop;
                        it->bossPhaseTimer = EnemyConfig::Boss3.glideDuration;
                        it->bossTargetPos = { RandomFloat(250.0f, (float)SCREEN_WIDTH - 250.0f), EnemyConfig::Boss3.hoverY };
                    }
                }
            }
            // Boss 4 (Final Boss - Kitsune Yokai Entity) Multi-Phase AI
            else if (it->isBoss && it->bossType == 4)
            {
                UpdateFinalBoss(*it, deltaTime);
            }
            else
            {
                // Normal Asteroid or Boss 1 movement
                it->position.x += it->velocity.x * deltaTime;
                it->position.y += it->velocity.y * deltaTime;
                it->rotation += it->rotationSpeed * deltaTime;
            }

            // Player vs Asteroid / Boss Collision
            float pDx = m_playerPos.x - it->position.x;
            float pDy = m_playerPos.y - it->position.y;
            float pDist = sqrt(pDx * pDx + pDy * pDy);
            float minDist = m_playerHitboxRadius + it->radius;

            if (pDist < minDist && pDist > 0.001f)
            {
                DamagePlayer(1);
                float push = (minDist - pDist) + 12.0f;
                m_playerPos.x += (pDx / pDist) * push;
                m_playerPos.y += (pDy / pDist) * push;
            }

            if (!it->isBoss && (it->position.x < -150.0f || it->position.x > SCREEN_WIDTH + 150.0f ||
                it->position.y < -150.0f || it->position.y > SCREEN_HEIGHT + 150.0f))
            {
                it = m_asteroids.erase(it);
                continue;
            }

            ++it;
        }

        // Magnet attraction & Resource collection
        float currentPickupRadius = m_superVacuumActive ? 3000.0f : m_stats.pickupRadius;
        if (m_stats.hyperMagnet) currentPickupRadius *= 1.5f;
        float currentMagnetSpeed = m_superVacuumActive ? 850.0f : 480.0f;

        for (auto it = m_pickups.begin(); it != m_pickups.end(); )
        {
            it->driftTimer += deltaTime;
            it->position.x += it->velocity.x * deltaTime;
            it->position.y += it->velocity.y * deltaTime;
            it->velocity.x *= 0.95f;
            it->velocity.y *= 0.95f;

            float pDx = m_playerPos.x - it->position.x;
            float pDy = m_playerPos.y - it->position.y;
            float pDist = sqrt(pDx * pDx + pDy * pDy);

            if (pDist < currentPickupRadius)
            {
                it->position.x += (pDx / pDist) * currentMagnetSpeed * deltaTime;
                it->position.y += (pDy / pDist) * currentMagnetSpeed * deltaTime;
            }

            if (pDist < 28.0f)
            {
                if (m_stats.resourceOrbit && !m_superVacuumActive)
                {
                    // Convert into Orbiting Swirl particle before absorbing!
                    OrbitingResource orb;
                    orb.type = it->type;
                    orb.amount = it->amount;
                    orb.orbitAngle = RandomFloat(0.0f, 2.0f * PI);
                    orb.orbitRadius = 38.0f;
                    orb.lifetime = 0.0f;
                    orb.maxLifetime = 0.75f;
                    m_orbitingResources.push_back(orb);
                }
                else
                {
                    // Direct collection
                    if (it->type == PickupType::Reishi)
                    {
                        int gain = (int)ceilf((float)it->amount * m_stats.resourceMultiplier);
                        m_reishiCount += gain;
                        m_runStats.reishiCollected += gain;
                        m_upgradeTree.AddRunEarnings(m_resources, gain, 0, 0, 0, 0);
                    }
                    else if (it->type == PickupType::Vida)
                    {
                        m_runStats.vidaCollected += it->amount;
                        m_upgradeTree.AddRunEarnings(m_resources, 0, it->amount, 0, 0, 0);
                    }
                    else if (it->type == PickupType::Disli)
                    {
                        m_runStats.disliCollected += it->amount;
                        m_upgradeTree.AddRunEarnings(m_resources, 0, 0, it->amount, 0, 0);
                    }
                    else if (it->type == PickupType::Cpu)
                    {
                        m_runStats.cpuCollected += it->amount;
                        m_upgradeTree.AddRunEarnings(m_resources, 0, 0, 0, it->amount, 0);
                    }
                    else if (it->type == PickupType::Key)
                    {
                        m_runStats.keyCollected += it->amount;
                        m_upgradeTree.AddRunEarnings(m_resources, 0, 0, 0, 0, it->amount);
                    }
                }

                it = m_pickups.erase(it);
                continue;
            }

            ++it;
        }

        // Update Orbiting Resources (swirling around player)
        for (auto it = m_orbitingResources.begin(); it != m_orbitingResources.end(); )
        {
            it->lifetime += deltaTime;
            it->orbitAngle += 7.0f * deltaTime;
            it->orbitRadius -= 25.0f * deltaTime;

            if (it->lifetime >= it->maxLifetime || it->orbitRadius <= 6.0f)
            {
                // Finished orbit swirl: add to bank!
                if (it->type == PickupType::Reishi)
                {
                    int gain = (int)ceilf((float)it->amount * m_stats.resourceMultiplier);
                    m_reishiCount += gain;
                    m_runStats.reishiCollected += gain;
                    m_upgradeTree.AddRunEarnings(m_resources, gain, 0, 0, 0, 0);
                }
                else if (it->type == PickupType::Vida)
                {
                    m_runStats.vidaCollected += it->amount;
                    m_upgradeTree.AddRunEarnings(m_resources, 0, it->amount, 0, 0, 0);
                }
                else if (it->type == PickupType::Disli)
                {
                    m_runStats.disliCollected += it->amount;
                    m_upgradeTree.AddRunEarnings(m_resources, 0, 0, it->amount, 0, 0);
                }
                else if (it->type == PickupType::Cpu)
                {
                    m_runStats.cpuCollected += it->amount;
                    m_upgradeTree.AddRunEarnings(m_resources, 0, 0, 0, it->amount, 0);
                }
                else if (it->type == PickupType::Key)
                {
                    m_runStats.keyCollected += it->amount;
                    m_upgradeTree.AddRunEarnings(m_resources, 0, 0, 0, 0, it->amount);
                }

                it = m_orbitingResources.erase(it);
                continue;
            }

            ++it;
        }

        // Update Lasers
        for (auto it = m_lasers.begin(); it != m_lasers.end(); )
        {
            it->lifetime -= deltaTime;
            if (it->lifetime <= 0.0f)
            {
                it = m_lasers.erase(it);
            }
            else
            {
                ++it;
            }
        }

        // Update VFX
        for (auto it = m_vfxs.begin(); it != m_vfxs.end(); )
        {
            it->lifetime += deltaTime;
            if (it->isSpriteSheet)
            {
                it->frameTimer += deltaTime;
                if (it->frameTimer >= it->frameDuration)
                {
                    it->frameTimer = 0.0f;
                    it->currentFrame++;
                    if (it->currentFrame >= it->frameCount)
                    {
                        it = m_vfxs.erase(it);
                        continue;
                    }
                }
            }
            else if (it->isMultiTexture)
            {
                int totalFrames = (int)it->textureSequence.size();
                int f = (int)((it->lifetime / it->maxLifetime) * (float)totalFrames);
                if (f >= totalFrames)
                {
                    it = m_vfxs.erase(it);
                    continue;
                }
                it->currentFrame = f;
            }

            if (it->lifetime >= it->maxLifetime)
            {
                it = m_vfxs.erase(it);
                continue;
            }
            ++it;
        }

        // Update Shockwaves
        for (auto it = m_shockwaves.begin(); it != m_shockwaves.end(); )
        {
            it->lifetime += deltaTime;
            it->currentRadius = (it->lifetime / it->maxLifetime) * it->maxRadius;
            if (it->lifetime >= it->maxLifetime)
            {
                it = m_shockwaves.erase(it);
            }
            else
            {
                ++it;
            }
        }

        // Update Damage Popups (World of Warcraft Parabolic Combat Text Physics)
        for (auto it = m_damagePopups.begin(); it != m_damagePopups.end(); )
        {
            it->lifetime += deltaTime;
            it->position.x += it->velocity.x * deltaTime;
            it->position.y += it->velocity.y * deltaTime;
            it->velocity.y += 220.0f * deltaTime; // Gravity curve for satisfying upward bounce & arc

            if (it->lifetime >= it->maxLifetime)
            {
                it = m_damagePopups.erase(it);
            }
            else
            {
                ++it;
            }
        }

        // Camera Shake decay
        if (m_cameraShakeTimer > 0.0f)
        {
            m_cameraShakeTimer -= deltaTime;
            float progress = (m_cameraShakeMaxDuration > 0.0001f) ? (m_cameraShakeTimer / m_cameraShakeMaxDuration) : 0.0f;
            progress = std::clamp(progress, 0.0f, 1.0f);
            float curIntensity = m_cameraShakeIntensity * progress;
            if (curIntensity > 0.01f)
            {
                m_cameraOffset.x = RandomFloat(-curIntensity, curIntensity);
                m_cameraOffset.y = RandomFloat(-curIntensity, curIntensity);
            }
            else
            {
                m_cameraOffset = { 0.0f, 0.0f };
            }
        }
        else
        {
            m_cameraOffset = { 0.0f, 0.0f };
        }
    }
    else if (m_runState == RunState::PlayerDying)
    {
        m_deathSequenceTimer -= deltaTime;
        m_explosionStaggerTimer -= deltaTime;

        // Multi-staged explosion cascade on player ship
        if (m_explosionStaggerTimer <= 0.0f && m_deathSequenceTimer > 0.35f)
        {
            m_explosionStaggerTimer = 0.28f;
            VFXInstance subExp;
            subExp.position = { m_playerPos.x + RandomFloat(-34.0f, 34.0f), m_playerPos.y + RandomFloat(-34.0f, 34.0f) };
            subExp.lifetime = 0.0f;
            subExp.maxLifetime = 0.55f;
            subExp.scale = RandomFloat(1.6f, 2.5f);
            subExp.isMultiTexture = true;
            subExp.textureSequence = m_texExplosions;
            m_vfxs.push_back(subExp);

            TriggerCameraShake(0.35f, 9.0f);
            if (m_soundShoot != -1) PlayAudio(m_soundShoot);
        }

        // Update VFXs & Damage popups during death pause
        for (auto it = m_vfxs.begin(); it != m_vfxs.end(); )
        {
            it->lifetime += deltaTime;
            if (it->isMultiTexture)
            {
                int totalFrames = (int)it->textureSequence.size();
                int frame = (int)((it->lifetime / it->maxLifetime) * (float)totalFrames);
                it->currentFrame = std::min(frame, totalFrames - 1);
            }
            else if (it->isSpriteSheet)
            {
                int frame = (int)(it->lifetime / it->frameDuration);
                it->currentFrame = std::min(frame, it->frameCount - 1);
            }

            if (it->lifetime >= it->maxLifetime)
            {
                it = m_vfxs.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (auto it = m_damagePopups.begin(); it != m_damagePopups.end(); )
        {
            it->lifetime += deltaTime;
            it->position.x += it->velocity.x * deltaTime;
            it->position.y += it->velocity.y * deltaTime;
            it->velocity.y += 120.0f * deltaTime;
            if (it->lifetime >= it->maxLifetime)
            {
                it = m_damagePopups.erase(it);
            }
            else
            {
                ++it;
            }
        }

        // Camera Shake decay
        if (m_cameraShakeTimer > 0.0f)
        {
            m_cameraShakeTimer -= deltaTime;
            float progress = (m_cameraShakeMaxDuration > 0.0001f) ? (m_cameraShakeTimer / m_cameraShakeMaxDuration) : 0.0f;
            progress = std::clamp(progress, 0.0f, 1.0f);
            float curIntensity = m_cameraShakeIntensity * progress;
            if (curIntensity > 0.01f)
            {
                m_cameraOffset.x = RandomFloat(-curIntensity, curIntensity);
                m_cameraOffset.y = RandomFloat(-curIntensity, curIntensity);
            }
            else
            {
                m_cameraOffset = { 0.0f, 0.0f };
            }
        }
        else
        {
            m_cameraOffset = { 0.0f, 0.0f };
        }

        if (m_deathSequenceTimer <= 0.0f)
        {
            m_runState = RunState::RunEnded;
            m_runSummaryInputDelay = 1.2f; // Debounce so player doesn't accidentally skip death report
        }
    }
    else if (m_runState == RunState::BossDefeated)
    {
        m_bossDeathTimer -= deltaTime;
        m_explosionStaggerTimer -= deltaTime;

        // Player can still move around freely using WASD
        DirectX::XMFLOAT2 moveInput{ 0.0f, 0.0f };
        if (InputKeyboard_IsPress(KK_W) || InputKeyboard_IsPress(KK_UP))    moveInput.y -= 1.0f;
        if (InputKeyboard_IsPress(KK_S) || InputKeyboard_IsPress(KK_DOWN))  moveInput.y += 1.0f;
        if (InputKeyboard_IsPress(KK_A) || InputKeyboard_IsPress(KK_LEFT))  moveInput.x -= 1.0f;
        if (InputKeyboard_IsPress(KK_D) || InputKeyboard_IsPress(KK_RIGHT)) moveInput.x += 1.0f;
        float inputLen = sqrtf(moveInput.x * moveInput.x + moveInput.y * moveInput.y);
        if (inputLen > 0.001f)
        {
            moveInput.x /= inputLen;
            moveInput.y /= inputLen;
            float targetRot = atan2f(moveInput.x, -moveInput.y);
            float rotDiff = targetRot - m_playerRotation;
            while (rotDiff > PI)  rotDiff -= 2.0f * PI;
            while (rotDiff < -PI) rotDiff += 2.0f * PI;
            m_playerRotation += rotDiff * std::min(1.0f, 16.0f * deltaTime);
        }
        while (m_playerRotation > PI)  m_playerRotation -= 2.0f * PI;
        while (m_playerRotation < -PI) m_playerRotation += 2.0f * PI;
        DirectX::XMFLOAT2 targetVel{ moveInput.x * m_stats.moveSpeed, moveInput.y * m_stats.moveSpeed };
        float accelRate = (inputLen > 0.001f) ? 14.0f : 8.5f;
        m_playerVelocity.x += (targetVel.x - m_playerVelocity.x) * accelRate * deltaTime;
        m_playerVelocity.y += (targetVel.y - m_playerVelocity.y) * accelRate * deltaTime;
        m_playerPos.x += m_playerVelocity.x * deltaTime;
        m_playerPos.y += m_playerVelocity.y * deltaTime;
        m_playerPos.x = std::clamp(m_playerPos.x, 50.0f, (float)SCREEN_WIDTH - 50.0f);
        m_playerPos.y = std::clamp(m_playerPos.y, 50.0f, (float)SCREEN_HEIGHT - 50.0f);

        // Cascading multi-explosion sequence on boss wreckage
        if (m_explosionStaggerTimer <= 0.0f && m_bossDeathTimer > 0.35f)
        {
            m_explosionStaggerTimer = 0.22f;
            VFXInstance subExp;
            subExp.position = { m_defeatedBossPos.x + RandomFloat(-85.0f, 85.0f), m_defeatedBossPos.y + RandomFloat(-85.0f, 85.0f) };
            subExp.lifetime = 0.0f;
            subExp.maxLifetime = 0.60f;
            subExp.scale = RandomFloat(2.2f, 3.4f);
            subExp.isMultiTexture = true;
            subExp.textureSequence = m_texExplosions;
            m_vfxs.push_back(subExp);

            TriggerCameraShake(0.32f, 8.5f);
            if (m_soundShoot != -1) PlayAudio(m_soundShoot);
        }

        // Two-Phase Boss Drop Dynamics:
        // Phase 1 (First 1.0s, timer > 3.0s): Resources burst and scatter outward naturally like fireworks
        // Phase 2 (Remaining 3.0s, timer <= 3.0s): Super Vacuum turns on and magnetically accelerates resources to the ship
        bool isMagnetActive = (m_bossDeathTimer <= 3.0f);
        float magnetSpeed = isMagnetActive ? (550.0f + (3.0f - m_bossDeathTimer) * 260.0f) : 0.0f;

        for (auto it = m_pickups.begin(); it != m_pickups.end(); )
        {
            it->position.x += it->velocity.x * deltaTime;
            it->position.y += it->velocity.y * deltaTime;
            it->velocity.x *= 0.95f;
            it->velocity.y *= 0.95f;

            float pDx = m_playerPos.x - it->position.x;
            float pDy = m_playerPos.y - it->position.y;
            float pDist = sqrt(pDx * pDx + pDy * pDy);

            if (isMagnetActive && pDist > 1.0f)
            {
                it->position.x += (pDx / pDist) * magnetSpeed * deltaTime;
                it->position.y += (pDy / pDist) * magnetSpeed * deltaTime;
            }

            if (pDist < 36.0f)
            {
                if (it->type == PickupType::Reishi)
                {
                    int gain = (int)ceilf((float)it->amount * m_stats.resourceMultiplier);
                    m_reishiCount += gain;
                    m_runStats.reishiCollected += gain;
                    m_upgradeTree.AddRunEarnings(m_resources, gain, 0, 0, 0, 0);
                    SpawnDamagePopup(m_playerPos, gain, false, false, true);
                }
                else if (it->type == PickupType::Vida)
                {
                    m_runStats.vidaCollected += it->amount;
                    m_upgradeTree.AddRunEarnings(m_resources, 0, it->amount, 0, 0, 0);
                }
                else if (it->type == PickupType::Disli)
                {
                    m_runStats.disliCollected += it->amount;
                    m_upgradeTree.AddRunEarnings(m_resources, 0, 0, it->amount, 0, 0);
                }
                else if (it->type == PickupType::Cpu)
                {
                    m_runStats.cpuCollected += it->amount;
                    m_upgradeTree.AddRunEarnings(m_resources, 0, 0, 0, it->amount, 0);
                }
                else if (it->type == PickupType::Key)
                {
                    m_runStats.keyCollected += it->amount;
                    m_upgradeTree.AddRunEarnings(m_resources, 0, 0, 0, 0, it->amount);
                }

                it = m_pickups.erase(it);
                continue;
            }
            ++it;
        }

        // Update VFXs, Shockwaves, and Damage popups
        for (auto it = m_vfxs.begin(); it != m_vfxs.end(); )
        {
            it->lifetime += deltaTime;
            if (it->isMultiTexture)
            {
                int totalFrames = (int)it->textureSequence.size();
                int frame = (int)((it->lifetime / it->maxLifetime) * (float)totalFrames);
                it->currentFrame = std::min(frame, totalFrames - 1);
            }
            else if (it->isSpriteSheet)
            {
                int frame = (int)(it->lifetime / it->frameDuration);
                it->currentFrame = std::min(frame, it->frameCount - 1);
            }

            if (it->lifetime >= it->maxLifetime)
            {
                it = m_vfxs.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (auto it = m_shockwaves.begin(); it != m_shockwaves.end(); )
        {
            it->lifetime += deltaTime;
            float progress = it->lifetime / it->maxLifetime;
            it->currentRadius = 10.0f + (it->maxRadius - 10.0f) * progress;
            if (it->lifetime >= it->maxLifetime)
            {
                it = m_shockwaves.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (auto it = m_damagePopups.begin(); it != m_damagePopups.end(); )
        {
            it->lifetime += deltaTime;
            it->position.x += it->velocity.x * deltaTime;
            it->position.y += it->velocity.y * deltaTime;
            it->velocity.y += 120.0f * deltaTime;
            if (it->lifetime >= it->maxLifetime)
            {
                it = m_damagePopups.erase(it);
            }
            else
            {
                ++it;
            }
        }

        // Camera Shake decay
        if (m_cameraShakeTimer > 0.0f)
        {
            m_cameraShakeTimer -= deltaTime;
            float progress = (m_cameraShakeMaxDuration > 0.0001f) ? (m_cameraShakeTimer / m_cameraShakeMaxDuration) : 0.0f;
            progress = std::clamp(progress, 0.0f, 1.0f);
            float curIntensity = m_cameraShakeIntensity * progress;
            if (curIntensity > 0.01f)
            {
                m_cameraOffset.x = RandomFloat(-curIntensity, curIntensity);
                m_cameraOffset.y = RandomFloat(-curIntensity, curIntensity);
            }
            else
            {
                m_cameraOffset = { 0.0f, 0.0f };
            }
        }
        else
        {
            m_cameraOffset = { 0.0f, 0.0f };
        }

        // Transition to summary card after sequence completes
        if (m_bossDeathTimer <= 0.0f)
        {
            m_runState = RunState::RunEnded;
            m_runSummaryInputDelay = 1.0f; // Debounce so firing click doesn't skip victory report
        }
    }
    else if (m_runState == RunState::RunEnded)
    {
        m_endRunTimer -= deltaTime;
        if (m_runSummaryInputDelay > 0.0f)
        {
            m_runSummaryInputDelay -= deltaTime;
        }

        // Active super vacuum pull to suck all remaining crystals and drops to the player ship!
        float currentPickupRadius = 3000.0f;
        float currentMagnetSpeed = 850.0f;
        for (auto it = m_pickups.begin(); it != m_pickups.end(); )
        {
            it->position.x += it->velocity.x * deltaTime;
            it->position.y += it->velocity.y * deltaTime;
            it->velocity.x *= 0.95f;
            it->velocity.y *= 0.95f;

            float pDx = m_playerPos.x - it->position.x;
            float pDy = m_playerPos.y - it->position.y;
            float pDist = sqrt(pDx * pDx + pDy * pDy);

            if (pDist < currentPickupRadius)
            {
                it->position.x += (pDx / pDist) * currentMagnetSpeed * deltaTime;
                it->position.y += (pDy / pDist) * currentMagnetSpeed * deltaTime;
            }

            if (pDist < 28.0f)
            {
                if (it->type == PickupType::Reishi)
                {
                    int gain = (int)ceilf((float)it->amount * m_stats.resourceMultiplier);
                    m_reishiCount += gain;
                    m_runStats.reishiCollected += gain;
                    m_upgradeTree.AddRunEarnings(m_resources, gain, 0, 0, 0, 0);
                }
                else if (it->type == PickupType::Vida)
                {
                    m_runStats.vidaCollected += it->amount;
                    m_upgradeTree.AddRunEarnings(m_resources, 0, it->amount, 0, 0, 0);
                }
                else if (it->type == PickupType::Disli)
                {
                    m_runStats.disliCollected += it->amount;
                    m_upgradeTree.AddRunEarnings(m_resources, 0, 0, it->amount, 0, 0);
                }
                else if (it->type == PickupType::Cpu)
                {
                    m_runStats.cpuCollected += it->amount;
                    m_upgradeTree.AddRunEarnings(m_resources, 0, 0, 0, it->amount, 0);
                }
                else if (it->type == PickupType::Key)
                {
                    m_runStats.keyCollected += it->amount;
                    m_upgradeTree.AddRunEarnings(m_resources, 0, 0, 0, 0, it->amount);
                }

                it = m_pickups.erase(it);
                continue;
            }
            ++it;
        }

        // Explicitly proceed to Market / Upgrade Tree via SPACE or Left Click only after delay
        bool userProceed = (m_runSummaryInputDelay <= 0.0f) && (InputKeyboard_IsTrigger(KK_SPACE) || InputMouse_IsTrigger(MOUSE_BUTTON_LEFT));

        if (userProceed)
        {
            m_currentScene = GameScene::UpgradePlaceholder;
            ResetRun();
        }
    }

    // =========================================================================
    // ⚡ ENERGY DEPLETED STATE (Peaceful voyage end)
    // =========================================================================
    if (m_runState == RunState::EnergyDepleted)
    {
        if (m_runSummaryInputDelay > 0.0f)
        {
            m_runSummaryInputDelay -= deltaTime;
        }

        // Vacuum remaining nearby pickups
        for (auto it = m_pickups.begin(); it != m_pickups.end(); )
        {
            float dx = m_playerPos.x - it->position.x;
            float dy = m_playerPos.y - it->position.y;
            float dist = sqrtf(dx * dx + dy * dy);
            if (dist > 1.0f)
            {
                it->position.x += (dx / dist) * 750.0f * deltaTime;
                it->position.y += (dy / dist) * 750.0f * deltaTime;
            }
            if (dist < 32.0f)
            {
                int gain = (int)ceilf((float)it->amount * m_stats.resourceMultiplier);
                if (it->type == PickupType::Reishi)
                {
                    m_resources.reishi += gain;
                    m_runStats.reishiCollected += gain;
                    m_upgradeTree.AddRunEarnings(m_resources, gain, 0, 0, 0, 0);
                }
                else if (it->type == PickupType::Vida)
                {
                    m_resources.vida += it->amount;
                    m_runStats.vidaCollected += it->amount;
                    m_upgradeTree.AddRunEarnings(m_resources, 0, it->amount, 0, 0, 0);
                }
                else if (it->type == PickupType::Disli)
                {
                    m_resources.disli += it->amount;
                    m_runStats.disliCollected += it->amount;
                    m_upgradeTree.AddRunEarnings(m_resources, 0, 0, it->amount, 0, 0);
                }
                else if (it->type == PickupType::Cpu)
                {
                    m_resources.cpu += it->amount;
                    m_runStats.cpuCollected += it->amount;
                    m_upgradeTree.AddRunEarnings(m_resources, 0, 0, 0, it->amount, 0);
                }
                else if (it->type == PickupType::Key)
                {
                    m_resources.key += it->amount;
                    m_runStats.keyCollected += it->amount;
                    m_upgradeTree.AddRunEarnings(m_resources, 0, 0, 0, 0, it->amount);
                }
                it = m_pickups.erase(it);
                continue;
            }
            ++it;
        }

        bool userProceed = (m_runSummaryInputDelay <= 0.0f) && (InputKeyboard_IsTrigger(KK_SPACE) || InputMouse_IsTrigger(MOUSE_BUTTON_LEFT));
        if (userProceed)
        {
            if (m_soundClick != -1) PlayAudio(m_soundClick);
            m_currentScene = GameScene::UpgradePlaceholder;
            ResetRun();
        }
    }

    m_totalTime += deltaTime;
}

// Shared by the Upgrade Tree's own [LAUNCH] button and the Main Menu's START EXPEDITION --
// both start a fresh run using whatever sector is currently selected in the tree.
void Game::StartExpeditionQuickLaunch()
{
    m_calamity.level = m_upgradeTree.GetCurrentSectorIndex();
    m_upgradeTree.ApplyStats(m_stats);
    ResetRun();
    m_currentScene = GameScene::Gameplay;
    m_upgradeEnteredFromMenu = false;

    if (m_calamity.level == 1 && !m_tutorialCompleted)
    {
        StartTutorial();
    }
}

void Game::UpdateUpgrade(float deltaTime)
{
    m_bgRenderer.Update(deltaTime, false);

    // Holographic entrance from the Main Menu: ship eases toward its dock position on the left
    // while the Upgrade Tree slides in from the right. Reused systems only -- just easing the
    // existing ship position and the tree's own pan offset, no new rendering path.
    if (m_upgradeEnteredFromMenu && m_upgradeIntroTimer < 1.0f)
    {
        m_upgradeIntroTimer = std::min(1.0f, m_upgradeIntroTimer + deltaTime / 0.55f);
        float eased = 1.0f - powf(1.0f - m_upgradeIntroTimer, 3.0f);
        float startX = (float)SCREEN_WIDTH * 0.5f;
        float dockX = (float)SCREEN_WIDTH * 0.16f;
        m_playerPos.x = startX + (dockX - startX) * eased;
        m_playerPos.y = (float)SCREEN_HEIGHT * 0.5f;
        m_upgradeTree.SetIntroOffsetX((1.0f - eased) * 650.0f);
    }

    bool startGame = false;
    m_upgradeTree.Update(deltaTime, m_stats, m_resources, startGame, m_upgradeTree.GetCurrentSectorIndex());

    if (startGame)
    {
        StartExpeditionQuickLaunch();
    }
}

void Game::SpawnChest()
{
    if ((int)m_chests.size() >= 2) return;

    AncientChest c;
    int side = RandomInt(0, 3);
    float speed = RandomFloat(25.0f, 50.0f);
    float angle = RandomFloat(0.0f, 2.0f * PI);

    if (side == 0) { c.position = { RandomFloat(100.0f, (float)SCREEN_WIDTH - 100.0f), -40.0f }; c.velocity = { cosf(angle) * speed, fabsf(sinf(angle)) * speed }; }
    else if (side == 1) { c.position = { (float)SCREEN_WIDTH + 40.0f, RandomFloat(100.0f, (float)SCREEN_HEIGHT - 100.0f) }; c.velocity = { -fabsf(cosf(angle)) * speed, sinf(angle) * speed }; }
    else if (side == 2) { c.position = { RandomFloat(100.0f, (float)SCREEN_WIDTH - 100.0f), (float)SCREEN_HEIGHT + 40.0f }; c.velocity = { cosf(angle) * speed, -fabsf(sinf(angle)) * speed }; }
    else { c.position = { -40.0f, RandomFloat(100.0f, (float)SCREEN_HEIGHT - 100.0f) }; c.velocity = { fabsf(cosf(angle)) * speed, sinf(angle) * speed }; }

    c.scale = 0.18f;
    c.radius = 28.0f;
    c.rotation = RandomFloat(0.0f, 2.0f * PI);
    c.rotationSpeed = RandomFloat(-0.35f, 0.35f);
    c.isOpened = false;

    m_chests.push_back(c);
}

void Game::SpawnAnomalousAsteroid()
{
    Asteroid a;
    a.position = { RandomFloat(200.0f, (float)SCREEN_WIDTH - 200.0f), -80.0f };
    a.velocity = { RandomFloat(-20.0f, 20.0f), RandomFloat(35.0f, 65.0f) };
    a.rotation = RandomFloat(0.0f, 2.0f * PI);
    a.rotationSpeed = 0.45f;
    a.maxHp = 340.0f;
    a.hp = a.maxHp;
    a.scale = 0.44f;
    a.radius = 56.0f;
    a.resourceAmount = 18;
    a.isBoss = false;
    a.destroyed = false;
    a.flashTimer = 0.0f;
    a.isAnomalousSignal = true;
    a.auraColor = { 1.0f, 0.85f, 0.25f, 0.95f };

    m_asteroids.push_back(a);
    m_anomalousWarningDisplayTimer = 4.0f;
    TriggerCameraShake(0.40f, 10.0f);
}

void Game::SpawnAsteroids(float deltaTime)
{
    // Chest spawn timer
    m_chestSpawnTimer += deltaTime;
    if (m_chestSpawnTimer >= 50.0f)
    {
        m_chestSpawnTimer = 0.0f;
        SpawnChest();
    }

    // Anomalous Signal Asteroid timer
    if (m_stats.treasureSignal)
    {
        m_anomalousSignalTimer += deltaTime;
        if (m_anomalousSignalTimer >= 65.0f)
        {
            m_anomalousSignalTimer = 0.0f;
            SpawnAnomalousAsteroid();
        }
    }

    m_spawnTimer += deltaTime;
    if (m_spawnTimer >= m_spawnInterval && (int)m_asteroids.size() < m_maxAliveAsteroids)
    {
        m_spawnTimer = 0.0f;

        Asteroid a;
        int side = RandomInt(0, 3);
        float speed = RandomFloat(40.0f, 90.0f);
        float angle = RandomFloat(0.0f, 2.0f * PI);

        if (side == 0) { a.position = { RandomFloat(0.0f, (float)SCREEN_WIDTH), -40.0f }; a.velocity = { cosf(angle) * speed, fabsf(sinf(angle)) * speed }; }
        else if (side == 1) { a.position = { (float)SCREEN_WIDTH + 40.0f, RandomFloat(0.0f, (float)SCREEN_HEIGHT) }; a.velocity = { -fabsf(cosf(angle)) * speed, sinf(angle) * speed }; }
        else if (side == 2) { a.position = { RandomFloat(0.0f, (float)SCREEN_WIDTH), (float)SCREEN_HEIGHT + 40.0f }; a.velocity = { cosf(angle) * speed, -fabsf(sinf(angle)) * speed }; }
        else { a.position = { -40.0f, RandomFloat(0.0f, (float)SCREEN_HEIGHT) }; a.velocity = { fabsf(cosf(angle)) * speed, sinf(angle) * speed }; }

        a.rotation = RandomFloat(0.0f, 2.0f * PI);
        a.rotationSpeed = RandomFloat(-1.2f, 1.2f);
        a.maxHp = RandomFloat(50.0f, 120.0f);
        a.hp = a.maxHp;
        a.scale = RandomFloat(0.18f, 0.28f);
        a.radius = 28.0f * (a.scale / 0.22f);
        a.resourceAmount = RandomInt(2, 5);
        a.isBoss = false;
        a.destroyed = false;
        a.flashTimer = 0.0f;

        // Crystal Weakpoint
        if (m_stats.crystalWeakpoints && RandomFloat(0.0f, 1.0f) < 0.45f)
        {
            a.hasWeakpoint = true;
            a.weakpointAngle = RandomFloat(0.0f, 2.0f * PI);
            a.weakpointRadius = a.radius * 0.70f;
        }

        // Rare Scanner Aura
        if (m_stats.rareScanner && RandomFloat(0.0f, 1.0f) < 0.28f)
        {
            a.auraColor = DirectX::XMFLOAT4(0.40f, 0.85f, 1.0f, 0.85f);
            a.resourceAmount *= 2;
        }

        m_asteroids.push_back(a);
    }
}

void Game::SpawnEnemies(float deltaTime)
{
    // Enemy drones do not spawn in Sector 1; they start appearing in Sector 2 (Round 2) onwards!
    // They keep spawning during single-boss fights for background pressure. They only pause
    // during the Sector 5 Boss Rush, where multiple simultaneous bosses are already chaotic enough.
    bool isBossRush = m_bossTriggered && m_calamity.level >= 5;
    if (m_calamity.level < 2 || isBossRush)
    {
        return;
    }

    m_enemySpawnTimer += deltaTime;
    if (m_enemySpawnTimer >= m_enemySpawnInterval && (int)m_enemies.size() < m_maxAliveEnemies)
    {
        m_enemySpawnTimer = 0.0f;

        Enemy e;
        e.position = { RandomFloat(100.0f, (float)SCREEN_WIDTH - 100.0f), -50.0f };
        e.targetPosition = { RandomFloat(120.0f, (float)SCREEN_WIDTH - 120.0f), RandomFloat(100.0f, (float)SCREEN_HEIGHT - 100.0f) };
        e.velocity = { 0.0f, 0.0f };
        e.rotation = 0.0f;
        e.rotationSpeed = 2.0f;

        e.maxHp = EnemyConfig::Drone.maxHp + (float)(m_calamity.level - 1) * 15.0f;
        e.hp = e.maxHp;
        e.radius = EnemyConfig::Drone.radius;
        e.scale = EnemyConfig::Drone.scale;
        e.shootInterval = std::max(0.9f, EnemyConfig::Drone.shootInterval - (float)(m_calamity.level - 1) * 0.15f);
        e.shootTimer = RandomFloat(0.6f, e.shootInterval);
        e.changeTargetTimer = 3.0f;
        e.flashTimer = 0.0f;
        e.destroyed = false;

        m_enemies.push_back(e);
    }
}

void Game::TargetAndFireLasers(float deltaTime)
{
    float fireInterval = m_isOvercharged ? 0.05f : m_stats.laserFireInterval;
    if (m_retaliationTimer > 0.0f)
    {
        fireInterval *= 0.40f; // Retaliation Matrix triple fire rate
    }

    float currentDamage = m_isOvercharged ? (m_stats.laserDamage * 2.5f) : m_stats.laserDamage;

    // Velocity Cannon: +Damage proportional to ship speed
    if (m_stats.velocityCannon)
    {
        float speed = m_stats.moveSpeed;
        currentDamage *= (1.0f + (speed / 300.0f) * 0.35f);
    }

    m_laserFireCooldown -= deltaTime;
    m_laserDamageTickTimer += deltaTime;

    bool tickDamage = false;
    if (m_laserDamageTickTimer >= 0.08f)
    {
        tickDamage = true;
        m_laserDamageTickTimer = 0.0f;
    }

    m_lasers.clear();
    DirectX::XMFLOAT2 shipCenter = m_playerPos;

    struct TargetItem
    {
        float dist;
        Enemy* enemy;
        Asteroid* asteroid;
        BossOrb* bossOrb;
    };
    std::vector<TargetItem> targets;

    for (auto& enemy : m_enemies)
    {
        if (enemy.destroyed) continue;
        float dx = enemy.position.x - m_playerPos.x;
        float dy = enemy.position.y - m_playerPos.y;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist <= m_stats.laserRange)
        {
            targets.push_back({ dist, &enemy, nullptr, nullptr });
        }
    }

    for (auto& ast : m_asteroids)
    {
        if (ast.destroyed) continue;
        float dx = ast.position.x - m_playerPos.x;
        float dy = ast.position.y - m_playerPos.y;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist <= m_stats.laserRange)
        {
            targets.push_back({ dist, nullptr, &ast, nullptr });
        }
    }

    for (auto& orb : m_bossOrbs)
    {
        if (!orb.alive || orb.isGhost) continue;
        float dx = orb.position.x - m_playerPos.x;
        float dy = orb.position.y - m_playerPos.y;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist <= m_stats.laserRange)
        {
            targets.push_back({ dist, nullptr, nullptr, &orb });
        }
    }

    std::sort(targets.begin(), targets.end(), [](const TargetItem& a, const TargetItem& b) {
        return a.dist < b.dist;
    });

    // Overheat Tracking
    if (!targets.empty() && m_stats.overheatEnabled)
    {
        void* curPrimary = targets[0].enemy ? (void*)targets[0].enemy : targets[0].asteroid ? (void*)targets[0].asteroid : (void*)targets[0].bossOrb;
        if (curPrimary == m_lastLaserTarget)
        {
            m_overheatDuration += deltaTime;
            if (m_overheatDuration >= 1.2f)
            {
                currentDamage *= 2.0f; // Overheat double damage!
            }
        }
        else
        {
            m_lastLaserTarget = curPrimary;
            m_overheatDuration = 0.0f;
        }
    }
    else
    {
        m_lastLaserTarget = nullptr;
        m_overheatDuration = 0.0f;
    }

    int beamsToFire = std::min((int)targets.size(), m_stats.laserCount);
    bool anyFired = false;

    for (int i = 0; i < beamsToFire; ++i)
    {
        const auto& target = targets[i];
        DirectX::XMFLOAT2 targetPos = target.enemy ? target.enemy->position : target.asteroid ? target.asteroid->position : target.bossOrb->position;

        DirectX::XMFLOAT4 laserColor = m_isOvercharged
            ? DirectX::XMFLOAT4(1.0f, 0.85f, 0.2f, 1.0f)
            : (m_overheatDuration >= 1.2f)
                ? DirectX::XMFLOAT4(1.0f, 0.25f, 0.35f, 1.0f) // Crimson overheat beam!
                : DirectX::XMFLOAT4(0.35f, 0.85f, 1.0f, 1.0f);

        LaserInstance laser;
        laser.start = shipCenter;
        laser.end = targetPos;
        laser.lifetime = 0.10f;
        laser.maxLifetime = 0.10f;
        laser.color = laserColor;
        m_lasers.push_back(laser);
        anyFired = true;

        // Piercing Beam: Continues through target to secondary target behind
        if (m_stats.piercingBeam && (int)targets.size() > beamsToFire)
        {
            const auto& secTarget = targets[beamsToFire];
            DirectX::XMFLOAT2 secPos = secTarget.enemy ? secTarget.enemy->position : secTarget.asteroid ? secTarget.asteroid->position : secTarget.bossOrb->position;
            LaserInstance pierceBeam;
            pierceBeam.start = targetPos;
            pierceBeam.end = secPos;
            pierceBeam.lifetime = 0.08f;
            pierceBeam.maxLifetime = 0.08f;
            pierceBeam.color = DirectX::XMFLOAT4(1.0f, 0.4f, 0.9f, 0.85f);
            m_lasers.push_back(pierceBeam);

            if (tickDamage)
            {
                float pierceDmg = currentDamage * 0.05f;
                if (secTarget.enemy) { secTarget.enemy->hp -= pierceDmg; secTarget.enemy->flashTimer = 0.05f; }
                else if (secTarget.asteroid && !secTarget.asteroid->invulnerable) { secTarget.asteroid->hp -= pierceDmg; secTarget.asteroid->flashTimer = 0.05f; ClampFinalBossHpFloor(*secTarget.asteroid); }
                else if (secTarget.bossOrb) { secTarget.bossOrb->hp -= pierceDmg; secTarget.bossOrb->flashTimer = 0.05f; }
            }
        }

        // Chain Laser Arc
        if (m_stats.chainLaser && (int)targets.size() > 1 && i == 0)
        {
            const auto& chainTarget = targets[1];
            DirectX::XMFLOAT2 chainPos = chainTarget.enemy ? chainTarget.enemy->position : chainTarget.asteroid ? chainTarget.asteroid->position : chainTarget.bossOrb->position;
            LaserInstance chainBeam;
            chainBeam.start = targetPos;
            chainBeam.end = chainPos;
            chainBeam.lifetime = 0.07f;
            chainBeam.maxLifetime = 0.07f;
            chainBeam.color = DirectX::XMFLOAT4(0.4f, 1.0f, 0.9f, 0.9f);
            m_lasers.push_back(chainBeam);

            if (tickDamage)
            {
                float chainDmg = currentDamage * 0.04f;
                if (chainTarget.enemy) { chainTarget.enemy->hp -= chainDmg; chainTarget.enemy->flashTimer = 0.05f; }
                else if (chainTarget.asteroid && !chainTarget.asteroid->invulnerable) { chainTarget.asteroid->hp -= chainDmg; chainTarget.asteroid->flashTimer = 0.05f; ClampFinalBossHpFloor(*chainTarget.asteroid); }
                else if (chainTarget.bossOrb) { chainTarget.bossOrb->hp -= chainDmg; chainTarget.bossOrb->flashTimer = 0.05f; }
            }
        }

        if (tickDamage)
        {
            float dmg = currentDamage * 0.08f;

            // Laser Excavator: 2x damage against asteroids
            if (m_stats.laserExcavator && target.asteroid)
            {
                dmg *= 2.0f;
            }

            if (target.enemy)
            {
                target.enemy->hp -= dmg;
                target.enemy->flashTimer = 0.06f;
                bool isCrit = m_isOvercharged || (m_overheatDuration >= 1.2f);
                SpawnDamagePopup(target.enemy->position, (int)ceilf(dmg), isCrit, false, false);
                if (target.enemy->hp <= 0.0f) target.enemy->destroyed = true;
            }
            else if (target.bossOrb)
            {
                target.bossOrb->hp -= dmg;
                target.bossOrb->flashTimer = 0.06f;
                bool isCrit = m_isOvercharged || (m_overheatDuration >= 1.2f);
                SpawnDamagePopup(target.bossOrb->position, (int)ceilf(dmg), isCrit, false, false);
                if (target.bossOrb->hp <= 0.0f)
                {
                    target.bossOrb->alive = false;

                    // Core Destruction Explosion VFX
                    VFXInstance orbExp;
                    orbExp.position = target.bossOrb->position;
                    orbExp.lifetime = 0.0f;
                    orbExp.maxLifetime = 0.50f;
                    orbExp.scale = 2.0f;
                    orbExp.isMultiTexture = true;
                    orbExp.textureSequence = m_texExplosions;
                    m_vfxs.push_back(orbExp);

                    if (m_soundPat != -1) PlayAudio(m_soundPat);

                    // Drop Reishi crystals from destroyed shield orb
                    for (int r = 0; r < 3; ++r)
                    {
                        ResourcePickup p;
                        p.position = target.bossOrb->position;
                        float rAngle = RandomFloat(0.0f, 2.0f * PI);
                        float rSpd = RandomFloat(60.0f, 150.0f);
                        p.velocity = { cosf(rAngle) * rSpd, sinf(rAngle) * rSpd };
                        p.scale = 0.08f;
                        p.type = PickupType::Reishi;
                        p.amount = 2;
                        m_pickups.push_back(p);
                    }

                    // Check if all 4 shield orbs are dead
                    bool anyAlive = false;
                    for (const auto& ob : m_bossOrbs)
                    {
                        if (ob.alive && !ob.isGhost) { anyAlive = true; break; }
                    }
                    if (!anyAlive)
                    {
                        // Remove Boss 4 Invulnerability & Transition to Phase 1!
                        for (auto& ast : m_asteroids)
                        {
                            if (ast.isBoss && ast.bossType == 4)
                            {
                                ast.invulnerable = false;
                                ast.finalPhase = FinalBossPhase::Phase1;
                                ast.finalAttackTimer = 1.0f;
                                TriggerShockwave(ast.position, 280.0f, 0.0f);
                                break;
                            }
                        }
                        TriggerCameraShake(0.50f, 8.0f);
                    }
                }
            }
            else if (target.asteroid)
            {
                if (target.asteroid->isBoss && target.asteroid->invulnerable)
                {
                    // Deflected by Golden Orb Shield!
                    target.asteroid->flashTimer = 0.03f;
                }
                else
                {
                    // Weakpoint check
                    if (target.asteroid->hasWeakpoint)
                    {
                        dmg *= 2.2f;
                        SpawnDamagePopup(target.asteroid->position, (int)ceilf(dmg), true, true, false);
                    }
                    else
                    {
                        SpawnDamagePopup(target.asteroid->position, (int)ceilf(dmg), m_isOvercharged, false, true);
                    }

                    target.asteroid->hp -= dmg;
                    target.asteroid->flashTimer = 0.06f;
                    ClampFinalBossHpFloor(*target.asteroid);

                    if (target.asteroid->hp <= 0.0f)
                    {
                        target.asteroid->destroyed = true;

                        // Core Meltdown Explosion
                        if (m_stats.coreMeltdown)
                        {
                            TriggerShockwave(target.asteroid->position, 140.0f, 45.0f);
                        }
                    }
                }
            }
        }
    }

    if (anyFired && m_laserFireCooldown <= 0.0f)
    {
        m_laserFireCooldown = fireInterval;
        // Player laser sound is disabled as requested ("player lazer sesini kapatalim")
    }
}

void Game::UpdateTurrets(float deltaTime)
{
    if (m_stats.turretCount <= 0)
    {
        m_turrets.clear();
        return;
    }

    // Ensure turrets are placed on the map
    if ((int)m_turrets.size() != m_stats.turretCount)
    {
        m_turrets.clear();
        if (m_stats.turretCount == 1)
        {
            TurretInstance t;
            t.position = { (float)SCREEN_WIDTH * 0.50f, (float)SCREEN_HEIGHT * 0.50f };
            t.defenseRadius = m_stats.turretRange;
            t.spec = m_stats.turretSpec;
            t.fireCooldown = 0.0f;
            m_turrets.push_back(t);
        }
        else if (m_stats.turretCount == 2)
        {
            TurretInstance t1;
            t1.position = { (float)SCREEN_WIDTH * 0.32f, (float)SCREEN_HEIGHT * 0.50f };
            t1.defenseRadius = m_stats.turretRange;
            t1.spec = m_stats.turretSpec;
            t1.fireCooldown = 0.0f;
            m_turrets.push_back(t1);

            TurretInstance t2;
            t2.position = { (float)SCREEN_WIDTH * 0.68f, (float)SCREEN_HEIGHT * 0.50f };
            t2.defenseRadius = m_stats.turretRange;
            t2.spec = m_stats.turretSpec;
            t2.fireCooldown = 0.0f;
            m_turrets.push_back(t2);
        }
        else if (m_stats.turretCount >= 3)
        {
            TurretInstance t1;
            t1.position = { (float)SCREEN_WIDTH * 0.50f, (float)SCREEN_HEIGHT * 0.30f };
            t1.defenseRadius = m_stats.turretRange;
            t1.spec = m_stats.turretSpec;
            t1.fireCooldown = 0.0f;
            m_turrets.push_back(t1);

            TurretInstance t2;
            t2.position = { (float)SCREEN_WIDTH * 0.28f, (float)SCREEN_HEIGHT * 0.70f };
            t2.defenseRadius = m_stats.turretRange;
            t2.spec = m_stats.turretSpec;
            t2.fireCooldown = 0.0f;
            m_turrets.push_back(t2);

            TurretInstance t3;
            t3.position = { (float)SCREEN_WIDTH * 0.72f, (float)SCREEN_HEIGHT * 0.70f };
            t3.defenseRadius = m_stats.turretRange;
            t3.spec = m_stats.turretSpec;
            t3.fireCooldown = 0.0f;
            m_turrets.push_back(t3);
        }
    }

    for (size_t i = 0; i < m_turrets.size(); ++i)
    {
        auto& turret = m_turrets[i];
        turret.spec = m_stats.turretSpec;
        turret.defenseRadius = m_stats.turretRange;

        // Check if player is inside the turret's operational defense zone
        float pDx = m_playerPos.x - turret.position.x;
        float pDy = m_playerPos.y - turret.position.y;
        float pDist = sqrtf(pDx * pDx + pDy * pDy);
        turret.isPlayerInZone = (pDist <= turret.defenseRadius + 50.0f);

        if (turret.fireCooldown > 0.0f)
        {
            turret.fireCooldown -= deltaTime;
        }

        // Turret actively defends its zone when player is in the zone
        if (turret.isPlayerInZone)
        {
            float bestDist = turret.defenseRadius;
            DirectX::XMFLOAT2 targetPos{ 0.0f, 0.0f };
            bool foundTarget = false;
            Enemy* targetEnemy = nullptr;
            Asteroid* targetAst = nullptr;

            // Mining Turret prioritizes asteroids
            if (turret.spec == TurretSpec::Mining)
            {
                for (auto& ast : m_asteroids)
                {
                    if (ast.destroyed) continue;
                    float dx = ast.position.x - turret.position.x;
                    float dy = ast.position.y - turret.position.y;
                    float dist = sqrtf(dx * dx + dy * dy);
                    if (dist < bestDist)
                    {
                        bestDist = dist;
                        targetPos = ast.position;
                        foundTarget = true;
                        targetAst = &ast;
                    }
                }
            }
            else
            {
                for (auto& enemy : m_enemies)
                {
                    if (enemy.destroyed) continue;
                    float dx = enemy.position.x - turret.position.x;
                    float dy = enemy.position.y - turret.position.y;
                    float dist = sqrtf(dx * dx + dy * dy);
                    if (dist < bestDist)
                    {
                        bestDist = dist;
                        targetPos = enemy.position;
                        foundTarget = true;
                        targetEnemy = &enemy;
                    }
                }

                if (!foundTarget)
                {
                    for (auto& ast : m_asteroids)
                    {
                        if (ast.destroyed) continue;
                        float dx = ast.position.x - turret.position.x;
                        float dy = ast.position.y - turret.position.y;
                        float dist = sqrtf(dx * dx + dy * dy);
                        if (dist < bestDist)
                        {
                            bestDist = dist;
                            targetPos = ast.position;
                            foundTarget = true;
                            targetAst = &ast;
                        }
                    }
                }
            }

            if (foundTarget)
            {
                // Face target smoothly
                float fdx = targetPos.x - turret.position.x;
                float fdy = targetPos.y - turret.position.y;
                turret.rotation = atan2f(fdy, fdx) + (PI * 0.5f);

                if (turret.fireCooldown <= 0.0f)
                {
                    turret.fireCooldown = m_stats.turretFireInterval;

                    if (turret.spec == TurretSpec::Plasma)
                    {
                        LaserInstance beam;
                        beam.start = turret.position;
                        beam.end = targetPos;
                        beam.lifetime = 0.16f;
                        beam.maxLifetime = 0.16f;
                        beam.color = DirectX::XMFLOAT4(0.95f, 0.40f, 1.0f, 1.0f);
                        m_lasers.push_back(beam);

                        TriggerShockwave(targetPos, 90.0f, m_stats.turretDamage * 1.5f);
                    }
                    else
                    {
                        LaserInstance beam;
                        beam.start = turret.position;
                        beam.end = targetPos;
                        beam.lifetime = 0.14f;
                        beam.maxLifetime = 0.14f;
                        beam.color = (turret.spec == TurretSpec::Mining)
                            ? DirectX::XMFLOAT4(0.35f, 1.0f, 0.50f, 1.0f)
                            : DirectX::XMFLOAT4(0.35f, 0.95f, 1.0f, 1.0f);
                        m_lasers.push_back(beam);

                        int dmg = (int)m_stats.turretDamage;
                        if (turret.spec == TurretSpec::Mining && targetAst) dmg *= 2;

                        if (targetEnemy)
                        {
                            targetEnemy->hp -= (float)dmg;
                            targetEnemy->flashTimer = 0.08f;
                            SpawnDamagePopup(targetEnemy->position, dmg, false);
                            if (targetEnemy->hp <= 0.0f) targetEnemy->destroyed = true;
                        }
                        else if (targetAst && !targetAst->invulnerable)
                        {
                            targetAst->hp -= (float)dmg;
                            targetAst->flashTimer = 0.08f;
                            ClampFinalBossHpFloor(*targetAst);
                            SpawnDamagePopup(targetAst->position, dmg, false);
                            if (targetAst->hp <= 0.0f) targetAst->destroyed = true;
                        }
                    }

                    if (m_soundShoot != -1) PlayAudio(m_soundShoot);
                }
            }
        }
    }
}

void Game::ConsumeShieldCharge(bool reflected)
{
    // Pops the shield's single charge and restarts its recharge -- shared by the plain-absorb
    // path (DamagePlayer) and the Deflector Matrix reflect path, so the shield always blocks
    // exactly 1 hit total no matter which path stops the incoming projectile.
    m_currentShield--;
    m_shieldRechargeTimer = m_stats.shieldRechargeTime;
    TriggerCameraShake(0.25f, 6.0f);
    SpawnTextPopup(m_playerPos, reflected ? "SHIELD DEFLECTED" : "SHIELD ABSORBED", { 0.40f, 0.85f, 1.0f, 1.0f });
}

void Game::DamagePlayer(int amount)
{
    if (m_invincibleTimer > 0.0f || m_runState != RunState::Active)
    {
        return;
    }

    if (m_isDashing)
    {
        return; // Invulnerable during Dash
    }

    if (m_currentShield > 0)
    {
        ConsumeShieldCharge(false);
        TriggerShockwave(m_playerPos, 160.0f, 20.0f); // Shield burst
        return;
    }

    m_playerHealth -= amount;
    m_invincibleTimer = 1.5f;
    TriggerCameraShake(0.35f, 8.5f);

    VFXInstance hit;
    hit.position = m_playerPos;
    hit.lifetime = 0.0f;
    hit.maxLifetime = 0.28f;
    hit.scale = 0.35f;
    hit.isSpriteSheet = true;
    hit.frameCount = 8;
    hit.frameDuration = 0.035f;
    hit.textureId = m_texLaserHit;
    m_vfxs.push_back(hit);

    if (m_playerHealth <= 0)
    {
        m_playerHealth = 0;
        m_runState = RunState::PlayerDying;
        m_deathSequenceTimer = 2.4f; // 2.4 seconds dramatic explosion pause
        m_explosionStaggerTimer = 0.0f;
        m_superVacuumActive = false;

        TriggerCameraShake(0.85f, 20.0f);
        TriggerShockwave(m_playerPos, 280.0f, 0.0f);

        VFXInstance exp;
        exp.position = m_playerPos;
        exp.lifetime = 0.0f;
        exp.maxLifetime = 0.65f;
        exp.scale = 2.8f;
        exp.isMultiTexture = true;
        exp.textureSequence = m_texExplosions;
        m_vfxs.push_back(exp);

        if (m_soundShoot != -1) PlayAudio(m_soundShoot);
    }
}

void Game::TriggerCameraShake(float duration, float intensity)
{
    m_cameraShakeTimer = duration;
    m_cameraShakeMaxDuration = duration;
    m_cameraShakeIntensity = intensity;
}

void Game::SpawnDamagePopup(const DirectX::XMFLOAT2& pos, int damage, bool isCrit, bool isWeakpoint, bool isMining)
{
    if (damage <= 0) return;

    DamagePopup dp;
    dp.position = pos;
    // WoW-style scatter around the impact point
    dp.position.x += RandomFloat(-16.0f, 16.0f);
    dp.position.y += RandomFloat(-10.0f, 8.0f);

    dp.damageAmount = damage;
    dp.isCritical = isCrit;
    dp.isWeakpoint = isWeakpoint;
    dp.isMining = isMining;
    dp.lifetime = 0.0f;
    dp.maxLifetime = (isCrit || isWeakpoint) ? 0.90f : 0.78f;

    // WoW-style parabolic upward arc initial velocities
    if (isCrit)
    {
        dp.velocity = { RandomFloat(-36.0f, 36.0f), -RandomFloat(150.0f, 210.0f) };
        dp.baseScale = 1.45f;
        dp.color = DirectX::XMFLOAT4(1.0f, 0.86f, 0.16f, 1.0f); // Blazing Sunburst Gold
    }
    else if (isWeakpoint)
    {
        dp.velocity = { RandomFloat(-30.0f, 30.0f), -RandomFloat(160.0f, 220.0f) };
        dp.baseScale = 1.50f;
        dp.color = DirectX::XMFLOAT4(0.35f, 1.0f, 0.95f, 1.0f); // Electric Cyan
    }
    else if (isMining)
    {
        dp.velocity = { RandomFloat(-25.0f, 25.0f), -RandomFloat(100.0f, 140.0f) };
        dp.baseScale = 1.05f;
        dp.color = DirectX::XMFLOAT4(0.40f, 1.0f, 0.65f, 1.0f); // Mineral Emerald
    }
    else
    {
        dp.velocity = { RandomFloat(-28.0f, 28.0f), -RandomFloat(115.0f, 160.0f) };
        dp.baseScale = 1.0f;
        dp.color = DirectX::XMFLOAT4(1.0f, 0.96f, 0.82f, 1.0f); // Crisp Ivory Gold
    }

    m_damagePopups.push_back(dp);
}

void Game::SpawnTextPopup(const DirectX::XMFLOAT2& pos, const char* text, const DirectX::XMFLOAT4& color)
{
    DamagePopup dp;
    dp.position = pos;
    dp.position.x += RandomFloat(-10.0f, 10.0f);
    dp.position.y += RandomFloat(-8.0f, 4.0f);

    dp.isTextLabel = true;
    dp.label = text;
    dp.color = color;
    dp.baseScale = 1.0f;
    dp.lifetime = 0.0f;
    dp.maxLifetime = 0.85f;
    dp.velocity = { RandomFloat(-14.0f, 14.0f), -RandomFloat(90.0f, 120.0f) };

    m_damagePopups.push_back(dp);
}

void Game::TriggerBossEncounter(int bossType)
{
    m_bossTriggered = true;
    m_bossWarningTimer = 3.5f;

    if (m_calamity.level >= 5)
    {
        // Sector 5 Multi-Boss Rush:
        // All 4 bosses participate. 2 spawn simultaneously, remaining bosses spawn as each is defeated!
        m_sector5BossQueue = { 1, 2, 3, 4 };
        std::shuffle(m_sector5BossQueue.begin(), m_sector5BossQueue.end(), std::mt19937(std::random_device{}()));
        m_sector5DefeatedCount = 0;

        int b1 = m_sector5BossQueue.back(); m_sector5BossQueue.pop_back();
        int b2 = m_sector5BossQueue.back(); m_sector5BossQueue.pop_back();

        SpawnBoss(b1, (float)SCREEN_WIDTH * 0.30f, -140.0f);
        SpawnBoss(b2, (float)SCREEN_WIDTH * 0.70f, -140.0f);
    }
    else
    {
        SpawnBoss(bossType, (float)SCREEN_WIDTH * 0.5f, -150.0f);
    }
}

void Game::SpawnBoss(int bossType, float startX, float startY)
{
    if (startX < 0.0f) startX = (float)SCREEN_WIDTH * 0.5f;

    Asteroid boss;
    boss.isBoss = true;
    boss.destroyed = false;
    boss.flashTimer = 0.0f;
    boss.bossType = bossType;

    if (bossType == 1)
    {
        boss.position = DirectX::XMFLOAT2(startX, startY);
        boss.velocity = DirectX::XMFLOAT2(0.0f, EnemyConfig::Boss1.moveSpeed);
        boss.rotation = 0.0f;
        boss.rotationSpeed = EnemyConfig::Boss1.rotationSpeed;
        boss.maxHp = EnemyConfig::Boss1.baseHp + (float)(m_calamity.level - 1) * EnemyConfig::Boss1.hpPerSectorLevel;
        boss.hp = boss.maxHp;
        boss.scale = EnemyConfig::Boss1.scale;
        boss.radius = EnemyConfig::Boss1.radius;
        boss.resourceAmount = 50;
    }
    else if (bossType == 2) // Boss 2 (Void Destroyer)
    {
        boss.position = DirectX::XMFLOAT2(startX, startY);
        boss.velocity = DirectX::XMFLOAT2(0.0f, 65.0f);
        boss.rotation = 0.0f;
        boss.rotationSpeed = EnemyConfig::Boss2.normalRotationSpeed;
        boss.maxHp = EnemyConfig::Boss2.baseHp + (float)(m_calamity.level - 1) * EnemyConfig::Boss2.hpPerSectorLevel;
        boss.hp = boss.maxHp;
        boss.scale = EnemyConfig::Boss2.scale;
        boss.radius = EnemyConfig::Boss2.radius;
        boss.resourceAmount = 80;

        // AI State Machine setup
        boss.bossPhase = BossPhase::Enter;
        boss.bossPhaseTimer = 0.0f;
        boss.bossShootTimer = EnemyConfig::Boss2.normalShootInterval;
        boss.bossSpiralFireTimer = 0.0f;
        boss.bossTargetPos = DirectX::XMFLOAT2(startX, 220.0f);
    }
    else if (bossType == 3) // Boss 3 (Torii Yokai / Sweeping Laser Boss)
    {
        boss.position = DirectX::XMFLOAT2(startX, startY);
        boss.velocity = DirectX::XMFLOAT2(0.0f, 60.0f);
        boss.rotation = 0.0f;
        boss.rotationSpeed = 0.0f;
        boss.maxHp = EnemyConfig::Boss3.baseHp + (float)(m_calamity.level - 1) * EnemyConfig::Boss3.hpPerSectorLevel;
        boss.hp = boss.maxHp;
        boss.scale = EnemyConfig::Boss3.scale;
        boss.radius = EnemyConfig::Boss3.radius;
        boss.resourceAmount = 120;

        // AI State Machine setup
        boss.bossPhase = BossPhase::Enter;
        boss.bossPhaseTimer = 0.0f;
        boss.bossShootTimer = 0.0f;
        boss.bossLaserAngle = 1.5707963f; // PI / 2 (pointing straight down)
        boss.bossLaserSweepFreq = EnemyConfig::Boss3.laserTrackingTurnSpeed;
        boss.bossLaserDamageTimer = 0.0f;
        boss.bossTargetPos = DirectX::XMFLOAT2(startX, EnemyConfig::Boss3.hoverY);
    }
    else // Boss 4 (Final Boss - Kitsune Yokai Entity)
    {
        boss.position = DirectX::XMFLOAT2(startX, startY);
        boss.velocity = DirectX::XMFLOAT2(0.0f, 60.0f);
        boss.rotation = 0.0f;
        boss.rotationSpeed = 0.0f;
        boss.maxHp = EnemyConfig::BossFinal.baseHp + (float)(m_calamity.level - 1) * EnemyConfig::BossFinal.hpPerSectorLevel;
        boss.hp = boss.maxHp;
        boss.scale = EnemyConfig::BossFinal.scale;
        boss.radius = EnemyConfig::BossFinal.radius;
        boss.resourceAmount = 220;

        // AI State Machine setup
        boss.finalPhase = FinalBossPhase::OrbShield;
        boss.invulnerable = true; // Boss is invulnerable while any shield orb is alive!
        boss.bossPhase = BossPhase::Enter;
        boss.bossPhaseTimer = 0.0f;
        boss.bossShootTimer = 0.0f;
        boss.finalAttackTimer = EnemyConfig::BossFinal.phase1AttackInterval;
        boss.finalBladeTimer = 3.5f;
        boss.bladePrisonTimer = EnemyConfig::BossFinal.bladePrisonCooldown;
        boss.ghostOrbTimer = 0.0f;
        boss.finalAttackStep = 0;
        boss.bossTargetPos = DirectX::XMFLOAT2(startX, EnemyConfig::BossFinal.hoverY);

        // Spawn 4 initial destructible shield orbs around the boss
        m_bossOrbs.clear();
        m_bossBlades.clear();
        float angles[4] = { 0.0f, 1.5707963f, 3.14159265f, 4.712389f };
        for (int k = 0; k < 4; ++k)
        {
            BossOrb orb;
            orb.angle = angles[k];
            orb.orbitRadius = EnemyConfig::BossFinal.orbOrbitRadius;
            orb.position = { boss.position.x + cosf(orb.angle) * orb.orbitRadius, boss.position.y + sinf(orb.angle) * orb.orbitRadius };
            orb.hp = EnemyConfig::BossFinal.orbHp;
            orb.maxHp = EnemyConfig::BossFinal.orbHp;
            orb.radius = EnemyConfig::BossFinal.orbRadius;
            orb.scale = EnemyConfig::BossFinal.orbScale;
            orb.fireInterval = EnemyConfig::BossFinal.orbFireInterval;
            orb.fireTimer = 0.4f + (float)k * 0.45f;
            orb.attackPattern = k % 3;
            orb.flashTimer = 0.0f;
            orb.alive = true;
            orb.isGhost = false;
            orb.ghostLifetime = 9999.0f;
            m_bossOrbs.push_back(orb);
        }
    }

    m_asteroids.push_back(boss);
}

void Game::Draw()
{
    if (m_currentScene == GameScene::Gameplay)
    {
        DrawGameplay();
    }
    else if (m_currentScene == GameScene::UpgradePlaceholder)
    {
        DrawUpgrade();
    }
    else if (m_currentScene == GameScene::MainMenu)
    {
        DrawMainMenu();
    }
}

void Game::DrawSkillBar()
{
    float cardW = 54.0f;
    float cardH = 54.0f;
    float gap = 12.0f;
    float totalW = 3 * cardW + 2 * gap;
    float startX = SCREEN_WIDTH * 0.5f - totalW * 0.5f;
    float startY = SCREEN_HEIGHT - 128.0f;

    for (int i = 0; i < 3; ++i)
    {
        float x = startX + i * (cardW + gap);
        float y = startY;

        const auto& slot = m_skillSlots[i];
        bool isUnlocked = (slot.type != ActiveSkillType::None);
        bool isReady = (isUnlocked && slot.cooldownTimer <= 0.0f);

        DirectX::XMFLOAT4 bgCol = isReady
            ? DirectX::XMFLOAT4(0.12f, 0.08f, 0.18f, 0.90f)
            : DirectX::XMFLOAT4(0.08f, 0.05f, 0.10f, 0.85f);

        DirectX::XMFLOAT4 borderCol = isReady
            ? DirectX::XMFLOAT4(0.40f, 0.95f, 1.0f, 1.0f)
            : DirectX::XMFLOAT4(0.30f, 0.25f, 0.35f, 0.6f);

        Sprite_DrawRect(x, y, cardW, cardH, bgCol);
        Sprite_DrawRectBorder(x, y, cardW, cardH, 2.0f, borderCol);

        if (isUnlocked)
        {
            float iconSize = 42.0f;
            float iconX = x + cardW * 0.5f - iconSize * 0.5f;
            float iconY = y + cardH * 0.44f - iconSize * 0.5f;
            DirectX::XMFLOAT4 iconCol = isReady
                ? DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f)
                : DirectX::XMFLOAT4(0.40f, 0.35f, 0.45f, 0.65f);

            if (slot.type == ActiveSkillType::EmpWave)
            {
                if (m_texSkillWave != -1)
                {
                    Sprite_Draw(m_texSkillWave, iconX, iconY, iconSize, iconSize, iconCol);
                }
                else
                {
                    Sprite_DrawRectBorder(iconX, iconY, iconSize, iconSize, 1.5f, iconCol);
                    Sprite_DrawLine(iconX + 2.0f, iconY + iconSize * 0.5f, iconX + iconSize - 2.0f, iconY + iconSize * 0.5f, 2.0f, iconCol);
                }
            }
            else if (slot.type == ActiveSkillType::Overcharge)
            {
                if (m_texSkillBuff != -1)
                {
                    Sprite_Draw(m_texSkillBuff, iconX, iconY, iconSize, iconSize, iconCol);
                }
                else
                {
                    Sprite_DrawLine(iconX + iconSize * 0.5f, iconY + 2.0f, iconX + iconSize - 4.0f, iconY + iconSize - 4.0f, 2.0f, iconCol);
                    Sprite_DrawLine(iconX + iconSize * 0.5f, iconY + 2.0f, iconX + 4.0f, iconY + iconSize - 4.0f, 2.0f, iconCol);
                }
            }
            else if (slot.type == ActiveSkillType::PhaseDash)
            {
                if (m_texSkillDash != -1)
                {
                    Sprite_Draw(m_texSkillDash, iconX, iconY, iconSize, iconSize, iconCol);
                }
                else
                {
                    Sprite_DrawLine(iconX + 4.0f, iconY + iconSize * 0.3f, iconX + iconSize * 0.5f, iconY + iconSize * 0.5f, 2.0f, iconCol);
                    Sprite_DrawLine(iconX + 4.0f, iconY + iconSize * 0.7f, iconX + iconSize * 0.5f, iconY + iconSize * 0.5f, 2.0f, iconCol);
                }
            }

            if (slot.cooldownTimer > 0.0f)
            {
                float cdPct = slot.cooldownTimer / slot.maxCooldown;
                Sprite_DrawRect(x, y + cardH * (1.0f - cdPct), cardW, cardH * cdPct, { 0.05f, 0.03f, 0.08f, 0.80f });
                int cdSec = (int)ceilf(slot.cooldownTimer);
                DrawNumber(x + cardW * 0.5f - 8.0f, y + cardH * 0.5f - 8.0f, cdSec, 1, 14.0f, m_texNumber, { 1.0f, 0.4f, 0.4f, 1.0f });
            }
        }
        else
        {
            DrawMatrixString(x + cardW * 0.5f - 4.0f, y + cardH * 0.5f - 6.0f, "?", 2.2f, m_texLaser, { 0.35f, 0.30f, 0.35f, 0.6f });
        }

        // Hotkey badge
        float badgeW = 26.0f;
        float badgeH = 12.0f;
        float badgeX = x + cardW * 0.5f - badgeW * 0.5f;
        float badgeY = y + cardH - 8.0f;

        Sprite_DrawRect(badgeX, badgeY, badgeW, badgeH, { 0.15f, 0.10f, 0.20f, 0.95f });
        Sprite_DrawRectBorder(badgeX, badgeY, badgeW, badgeH, 1.0f, { 0.8f, 0.75f, 0.6f, 0.8f });
        DrawMatrixString(badgeX + 3.0f, badgeY + 1.0f, slot.keyLabel.c_str(), 1.4f, m_texLaser, { 1.0f, 0.9f, 0.6f, 1.0f });
    }
}

void Game::DrawGameplay()
{
    float camX = m_cameraOffset.x;
    float camY = m_cameraOffset.y;

    // 1. Draw Procedural Cosmic Background Shader (Pure HLSL)
    m_bgRenderer.Render(camX, camY);

    // 2. Draw Asteroids & Weakpoints
    for (const auto& ast : m_asteroids)
    {
        if (ast.destroyed) continue;

        // Draw Anomaly / Rare Aura
        if (ast.auraColor.w > 0.05f)
        {
            float pulse = sinf(m_totalTime * 6.0f) * 0.15f + 0.85f;
            DirectX::XMFLOAT4 aCol = ast.auraColor;
            aCol.w *= pulse;
            Sprite_DrawCircle(ast.position.x + camX, ast.position.y + camY, ast.radius + 8.0f, 3.0f, aCol, 36);
        }

        if (ast.isBoss)
        {
            int bossTex = (ast.bossType == 4 && m_texFinalBoss != -1) ? m_texFinalBoss
                        : (ast.bossType == 3 && m_texBoss3 != -1) ? m_texBoss3
                        : (ast.bossType == 2 && m_texBoss2 != -1) ? m_texBoss2
                        : m_texBoss1;
            int texW = (bossTex != -1) ? Texture_GetWidth(bossTex) : 500;
            int texH = (bossTex != -1) ? Texture_GetHeight(bossTex) : 500;
            float w = (float)texW * ast.scale;
            float h = (float)texH * ast.scale;

            DirectX::XMFLOAT4 col = (ast.flashTimer > 0.0f)
                ? DirectX::XMFLOAT4(1.0f, 0.3f, 0.3f, 1.0f)
                : (ast.bossType == 2 && ast.bossPhase == BossPhase::AlarmWarning)
                    ? DirectX::XMFLOAT4(1.0f, 0.4f + 0.6f * sinf(m_totalTime * 22.0f), 0.3f, 1.0f)
                    : (ast.bossType == 3 && ast.bossPhase == BossPhase::LaserLock)
                        ? DirectX::XMFLOAT4(1.0f, 0.3f + 0.7f * sinf(m_totalTime * 25.0f), 0.4f, 1.0f)
                        : (ast.bossType == 3 && ast.bossPhase == BossPhase::LaserTrack)
                            ? DirectX::XMFLOAT4(1.0f, 0.6f + 0.4f * sinf(m_totalTime * 12.0f), 1.0f, 1.0f)
                            : DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

            // Boss 2: In AlarmWarning phase - draw pulsing expanding alarm aura circles and warning text
            if (ast.bossType == 2 && ast.bossPhase == BossPhase::AlarmWarning)
            {
                float pulseRing = fmodf(m_totalTime * 3.5f, 1.0f);
                float ringRad = ast.radius * (0.8f + pulseRing * 0.9f);
                Sprite_DrawCircle(ast.position.x + camX, ast.position.y + camY, ringRad, 3.0f,
                    { 1.0f, 0.25f, 0.20f, 1.0f - pulseRing }, 32);
                DrawMatrixString(ast.position.x + camX - 75.0f, ast.position.y + camY - h * 0.5f - 38.0f,
                    "! DIKKAT : SPIRAL SALDIRI !", 1.4f, m_texLaser, { 1.0f, 0.3f, 0.3f, 1.0f });
            }

            // Boss 3: Eye Emitter & Multi-Phase Telegraph Laser Visuals
            if (ast.bossType == 3)
            {
                float eyeX = ast.position.x + camX;
                float eyeY = ast.position.y + camY + 10.0f;
                DirectX::XMFLOAT2 aimDir = { cosf(ast.bossLaserAngle), sinf(ast.bossLaserAngle) };

                // Central Eye Flare Pulse
                float eyePulse = sinf(m_totalTime * 8.0f) * 0.20f + 0.80f;
                Sprite_DrawCircle(eyeX, eyeY, 13.0f, 2.0f, { 0.90f, 0.25f, 1.0f, eyePulse }, 18);

                // Phase 1: LaserTrack - Active Aim Tracking with Smooth Dotted/Continuous Guide Ray
                if (ast.bossPhase == BossPhase::LaserTrack)
                {
                    float startX = eyeX;
                    float startY = eyeY;
                    float endX = eyeX + aimDir.x * 1400.0f;
                    float endY = eyeY + aimDir.y * 1400.0f;

                    // Multi-layer subtle tracking guide ray
                    Sprite_DrawLine(startX, startY, endX, endY, 6.0f, { 0.75f, 0.20f, 0.95f, 0.22f });
                    Sprite_DrawLine(startX, startY, endX, endY, 2.5f, { 0.90f, 0.40f, 1.0f, 0.60f });
                    Sprite_DrawLine(startX, startY, endX, endY, 1.0f, { 1.0f, 0.85f, 1.0f, 0.85f });

                    DrawMatrixString(ast.position.x + camX - 65.0f, ast.position.y + camY - h * 0.5f - 38.0f,
                        "! HEDEFLENIYOR !", 1.4f, m_texLaser, { 0.90f, 0.45f, 1.0f, 1.0f });
                }
                // Phase 2: LaserLock - Solid Locked Warning Beam (Player's Escape Opportunity Window!)
                else if (ast.bossPhase == BossPhase::LaserLock)
                {
                    // Collapsing energy rings into central eye
                    float ringProgress = fmodf(m_totalTime * 3.5f, 1.0f);
                    Sprite_DrawCircle(eyeX, eyeY, 50.0f * (1.0f - ringProgress) + 10.0f, 2.5f, { 1.0f, 0.25f, 0.45f, ringProgress }, 24);

                    float startX = eyeX;
                    float startY = eyeY;
                    float endX = eyeX + aimDir.x * 1400.0f;
                    float endY = eyeY + aimDir.y * 1400.0f;

                    // Solid warning locked line
                    Sprite_DrawLine(startX, startY, endX, endY, 14.0f, { 1.0f, 0.15f, 0.35f, 0.40f });
                    Sprite_DrawLine(startX, startY, endX, endY, 6.0f,  { 1.0f, 0.35f, 0.55f, 0.85f });
                    Sprite_DrawLine(startX, startY, endX, endY, 2.0f,  { 1.0f, 1.0f, 1.0f, 0.98f });

                    DrawMatrixString(ast.position.x + camX - 75.0f, ast.position.y + camY - h * 0.5f - 38.0f,
                        "! KILITLENDI : KAC !", 1.4f, m_texLaser, { 1.0f, 0.30f, 0.30f, 1.0f });
                }
                // Phase 3: LaserFire - Massive Colossal Laser Beam (Boss-scale counterpart of player's laser beam)
                else if (ast.bossPhase == BossPhase::LaserFire)
                {
                    float startX = eyeX;
                    float startY = eyeY;
                    float beamLen = 1400.0f;
                    float endX = eyeX + cosf(ast.bossLaserAngle) * beamLen;
                    float endY = eyeY + sinf(ast.bossLaserAngle) * beamLen;

                    // 4-Layer Colossal Boss Laser Beam (Matching player laser architecture with boss-scale power)
                    // Layer 1: Huge Outer Violet Bloom Aura
                    Sprite_DrawLine(startX, startY, endX, endY, 46.0f, { 0.70f, 0.12f, 0.98f, 0.32f });
                    // Layer 2: Vivid Electric Neon Purple Beam
                    Sprite_DrawLine(startX, startY, endX, endY, 26.0f, { 0.85f, 0.30f, 1.0f, 0.65f });
                    // Layer 3: High-Power Magenta Core Beam
                    Sprite_DrawLine(startX, startY, endX, endY, 13.0f, { 0.98f, 0.55f, 1.0f, 0.95f });
                    // Layer 4: Ultra-Intense White Hot Plazma Core
                    Sprite_DrawLine(startX, startY, endX, endY, 4.5f,  { 1.0f, 1.0f, 1.0f, 1.0f });

                    // Corona Flare & Pulsing Energy Rings at Emitter Eye
                    Sprite_DrawCircle(eyeX, eyeY, 28.0f, 3.5f, { 0.95f, 0.40f, 1.0f, 0.95f }, 24);
                    Sprite_DrawCircle(eyeX, eyeY, 14.0f, 2.2f, { 1.0f, 1.0f, 1.0f, 1.0f }, 16);

                    // Flowing energy plasma sparks along the beam
                    for (int s = 0; s < 6; ++s)
                    {
                        float sDist = fmodf(m_totalTime * 850.0f + (float)s * 220.0f, beamLen);
                        Sprite_DrawCircle(eyeX + cosf(ast.bossLaserAngle) * sDist, eyeY + sinf(ast.bossLaserAngle) * sDist, 7.5f, 1.8f, { 1.0f, 0.75f, 1.0f, 0.85f }, 12);
                    }

                    // Contact impact sparkle at target end
                    if (m_texLaserHit != -1)
                    {
                        int frame = (int)(m_totalTime * 24.0f) % 8;
                        int hitCol = frame % 4;
                        int hitRow = frame / 4;
                        int fW = 384;
                        int fH = 512;
                        float hitW = 54.0f;
                        float hitH = 72.0f;
                        Sprite_Draw(m_texLaserHit, endX - hitW * 0.5f, endY - hitH * 0.5f, hitW, hitH,
                            hitCol * fW, hitRow * fH, fW, fH, 0.0f, { 1.0f, 1.0f }, { 0.95f, 0.50f, 1.0f, 0.9f });
                    }
                }
                // Phase 2: CurtainWarning (Telegraph) & CurtainFire (Super High-Density Vertical Laser Wall)
                else if (ast.bossPhase == BossPhase::CurtainWarning || ast.bossPhase == BossPhase::CurtainFire)
                {
                    float colSpacing = (float)SCREEN_WIDTH / 16.0f;
                    float warnPulse = sinf(m_totalTime * 20.0f) * 0.25f + 0.75f;

                    for (int c = 0; c < 16; ++c)
                    {
                        if (c == ast.boss3SafeGapIndex || c == ast.boss3SafeGapIndex2) continue; // Natural empty gap!

                        float colX = colSpacing * ((float)c + 0.5f);

                        if (ast.bossPhase == BossPhase::CurtainWarning)
                        {
                            // Warning Column Guide Ray
                            Sprite_DrawRect(colX - 11.0f + camX, 0.0f, 22.0f, (float)SCREEN_HEIGHT, { 1.0f, 0.15f, 0.35f, 0.25f * warnPulse });
                            Sprite_DrawLine(colX + camX, 0.0f, colX + camX, (float)SCREEN_HEIGHT, 1.8f, { 1.0f, 0.30f, 0.40f, 0.80f * warnPulse });
                            DrawMatrixString(colX + camX - 6.0f, 22.0f, "!", 1.8f, m_texLaser, { 1.0f, 0.30f, 0.30f, 1.0f });
                        }
                        else
                        {
                            // Full Active Vertical Laser Beam Pillar
                            Sprite_DrawRect(colX - 13.0f + camX, 0.0f, 26.0f, (float)SCREEN_HEIGHT, { 0.75f, 0.12f, 0.98f, 0.35f });
                            Sprite_DrawLine(colX + camX, 0.0f, colX + camX, (float)SCREEN_HEIGHT, 14.0f, { 0.90f, 0.30f, 1.0f, 0.75f });
                            Sprite_DrawLine(colX + camX, 0.0f, colX + camX, (float)SCREEN_HEIGHT, 3.0f,  { 1.0f, 1.0f, 1.0f, 1.0f });
                        }
                    }

                    if (ast.bossPhase == BossPhase::CurtainWarning)
                    {
                        DrawMatrixString(ast.position.x + camX - 120.0f, ast.position.y + camY - h * 0.5f - 38.0f,
                            "! LAZER DUVARI UYARISI !", 1.4f, m_texLaser, { 1.0f, 0.30f, 0.30f, 1.0f });
                    }
                }
                // Phase 3: GridWarning (Telegraph) & GridFire (Super High-Density Crosshatch Matrix)
                else if (ast.bossPhase == BossPhase::GridWarning || ast.bossPhase == BossPhase::GridFire)
                {
                    float warnPulse = sinf(m_totalTime * 22.0f) * 0.25f + 0.75f;
                    float gridSpacing = 140.0f;

                    // 15 Left-to-Right diagonal lines: x - y = C1[k]
                    for (int k = -7; k <= 7; ++k)
                    {
                        float C1 = (float)k * gridSpacing;
                        float x1 = 0.0f; float y1 = -C1;
                        float x2 = (float)SCREEN_WIDTH; float y2 = (float)SCREEN_WIDTH - C1;

                        if (ast.bossPhase == BossPhase::GridWarning)
                        {
                            Sprite_DrawLine(x1 + camX, y1 + camY, x2 + camX, y2 + camY, 1.8f, { 1.0f, 0.25f, 0.40f, 0.70f * warnPulse });
                        }
                        else
                        {
                            Sprite_DrawLine(x1 + camX, y1 + camY, x2 + camX, y2 + camY, 14.0f, { 0.75f, 0.12f, 0.98f, 0.35f });
                            Sprite_DrawLine(x1 + camX, y1 + camY, x2 + camX, y2 + camY, 7.0f,  { 0.90f, 0.35f, 1.0f, 0.70f });
                            Sprite_DrawLine(x1 + camX, y1 + camY, x2 + camX, y2 + camY, 2.0f,  { 1.0f, 1.0f, 1.0f, 1.0f });
                        }
                    }

                    // 15 Right-to-Left diagonal lines: x + y = C2[k]
                    float midSum = (float)SCREEN_WIDTH * 0.5f + (float)SCREEN_HEIGHT * 0.5f;
                    for (int k = -7; k <= 7; ++k)
                    {
                        float C2 = midSum + (float)k * gridSpacing;
                        float x1 = 0.0f; float y1 = C2;
                        float x2 = (float)SCREEN_WIDTH; float y2 = C2 - (float)SCREEN_WIDTH;

                        if (ast.bossPhase == BossPhase::GridWarning)
                        {
                            Sprite_DrawLine(x1 + camX, y1 + camY, x2 + camX, y2 + camY, 1.8f, { 1.0f, 0.25f, 0.40f, 0.70f * warnPulse });
                        }
                        else
                        {
                            Sprite_DrawLine(x1 + camX, y1 + camY, x2 + camX, y2 + camY, 14.0f, { 0.75f, 0.12f, 0.98f, 0.35f });
                            Sprite_DrawLine(x1 + camX, y1 + camY, x2 + camX, y2 + camY, 7.0f,  { 0.90f, 0.35f, 1.0f, 0.70f });
                            Sprite_DrawLine(x1 + camX, y1 + camY, x2 + camX, y2 + camY, 2.0f,  { 1.0f, 1.0f, 1.0f, 1.0f });
                        }
                    }

                    if (ast.bossPhase == BossPhase::GridWarning)
                    {
                        DrawMatrixString(ast.position.x + camX - 120.0f, ast.position.y + camY - h * 0.5f - 38.0f,
                            "! IZGARA MATRIS UYARISI !", 1.4f, m_texLaser, { 1.0f, 0.30f, 0.30f, 1.0f });
                    }
                }
            }

            // Boss 4: Golden Celestial Barrier Shield when invulnerable
            if (ast.bossType == 4 && ast.invulnerable)
            {
                float shieldRad = ast.radius * 1.35f;
                float pulse = sinf(m_totalTime * 6.0f) * 0.15f + 0.85f;
                Sprite_DrawCircle(ast.position.x + camX, ast.position.y + camY, shieldRad, 3.5f, { 1.0f, 0.82f, 0.20f, 0.85f * pulse }, 36);
                Sprite_DrawCircle(ast.position.x + camX, ast.position.y + camY, shieldRad - 8.0f, 1.8f, { 1.0f, 0.95f, 0.45f, 0.55f * pulse }, 28);
            }

            if (bossTex != -1)
            {
                Sprite_Draw(bossTex, ast.position.x + camX - w * 0.5f, ast.position.y + camY - h * 0.5f, w, h,
                    0, 0, texW, texH, ast.rotation, { 1.0f, 1.0f }, col);
            }

            // Boss Health Bar
            float barW = (ast.bossType == 4) ? 180.0f : 150.0f;
            float barH = 10.0f;
            float barX = ast.position.x + camX - barW * 0.5f;
            float barY = ast.position.y + camY - h * 0.5f - 18.0f;
            Sprite_DrawRect(barX, barY, barW, barH, { 0.2f, 0.05f, 0.05f, 0.9f });
            Sprite_DrawRectBorder(barX, barY, barW, barH, 1.5f, { 0.85f, 0.2f, 0.2f, 1.0f });
            float hpPct = std::clamp(ast.hp / ast.maxHp, 0.0f, 1.0f);
            Sprite_DrawRect(barX, barY, barW * hpPct, barH, { 1.0f, 0.2f, 0.25f, 1.0f });

            // Boss Title Badge
            const char* bossTitle = (ast.bossType == 4)
                ? (ast.invulnerable ? "BOSS IV : KITSUNE YOKAI [ SHIELD ACTIVE ]" : "BOSS IV : KITSUNE YOKAI")
                : (ast.bossType == 3) ? "BOSS III : TORII YOKAI"
                : (ast.bossType == 2) ? "BOSS II : VOID DESTROYER"
                : "BOSS I : GARGANTUAN";
            DrawMatrixString(barX - 10.0f, barY - 14.0f, bossTitle, 1.3f, m_texLaser, { 1.0f, 0.85f, 0.3f, 1.0f });
        }
        else if (m_texAsteroid != -1)
        {
            int texW = Texture_GetWidth(m_texAsteroid);
            int texH = Texture_GetHeight(m_texAsteroid);
            float w = (float)texW * ast.scale;
            float h = (float)texH * ast.scale;

            DirectX::XMFLOAT4 col = (ast.flashTimer > 0.0f)
                ? DirectX::XMFLOAT4(1.0f, 0.3f, 0.3f, 1.0f)
                : ast.isAnomalousSignal
                    ? DirectX::XMFLOAT4(1.0f, 0.92f, 0.40f, 1.0f) // Glowing golden tint
                    : (m_stats.rareScanner || m_stats.oreVision || m_stats.deepScan) && (ast.resourceAmount >= 12)
                        ? DirectX::XMFLOAT4(0.50f, 1.0f, 0.85f, 1.0f) // Glowing emerald/cyan tint
                        : (m_stats.oreVision || m_stats.deepScan)
                            ? DirectX::XMFLOAT4(0.88f, 0.82f, 1.0f, 1.0f) // Subtle crystalline shimmer
                            : DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

            // Shimmer aura ring for valuable asteroids
            if ((m_stats.rareScanner || m_stats.oreVision || m_stats.deepScan) && (ast.isAnomalousSignal || ast.resourceAmount >= 12))
            {
                float shimmerPulse = sinf(m_totalTime * 6.0f + ast.rotation) * 0.20f + 0.80f;
                DirectX::XMFLOAT4 auraCol = ast.isAnomalousSignal
                    ? DirectX::XMFLOAT4(1.0f, 0.85f, 0.20f, 0.80f * shimmerPulse)
                    : DirectX::XMFLOAT4(0.30f, 0.95f, 0.75f, 0.70f * shimmerPulse);
                Sprite_DrawCircle(ast.position.x + camX, ast.position.y + camY, ast.radius + 6.0f, 2.0f, auraCol, 24);
            }

            Sprite_Draw(m_texAsteroid, ast.position.x + camX - w * 0.5f, ast.position.y + camY - h * 0.5f, w, h,
                0, 0, texW, texH, ast.rotation, { 1.0f, 1.0f }, col);

            // Draw Crystal Weakpoint Reticle
            if (ast.hasWeakpoint)
            {
                float wpX = ast.position.x + camX + cosf(ast.weakpointAngle) * ast.weakpointRadius;
                float wpY = ast.position.y + camY + sinf(ast.weakpointAngle) * ast.weakpointRadius;
                float pulse = sinf(m_totalTime * 8.0f) * 0.25f + 0.75f;
                Sprite_DrawCircle(wpX, wpY, 8.0f, 2.0f, { 1.0f, 0.25f, 0.35f, pulse }, 16);
                Sprite_DrawRect(wpX - 2.0f, wpY - 2.0f, 4.0f, 4.0f, { 1.0f, 0.85f, 0.3f, 1.0f });
            }

            // Attached Floating Resource Preview Badge hovering above the asteroid
            if (m_stats.oreVision || m_stats.deepScan || m_stats.rareScanner || m_stats.treasureSignal)
            {
                float tagY = ast.position.y + camY - h * 0.5f - 16.0f;
                if (ast.isAnomalousSignal)
                {
                    float tagW = 90.0f;
                    float tagX = ast.position.x + camX - tagW * 0.5f;
                    Sprite_DrawRect(tagX, tagY, tagW, 14.0f, { 0.14f, 0.09f, 0.04f, 0.92f });
                    Sprite_DrawRectBorder(tagX, tagY, tagW, 14.0f, 1.0f, { 1.0f, 0.85f, 0.25f, 0.95f });
                    DrawMatrixString(tagX + 12.0f, tagY + 2.0f, "[ 1 KEY ]", 1.4f, m_texLaser, { 1.0f, 0.88f, 0.30f, 1.0f });
                }
                else if (ast.resourceAmount >= 12)
                {
                    char buf[36];
                    sprintf_s(buf, "+%d REISHI+SCREWS", ast.resourceAmount);
                    float tagW = 126.0f;
                    float tagX = ast.position.x + camX - tagW * 0.5f;
                    Sprite_DrawRect(tagX, tagY, tagW, 14.0f, { 0.04f, 0.14f, 0.12f, 0.92f });
                    Sprite_DrawRectBorder(tagX, tagY, tagW, 14.0f, 1.0f, { 0.35f, 0.95f, 0.75f, 0.90f });
                    DrawMatrixString(tagX + 6.0f, tagY + 2.0f, buf, 1.2f, m_texLaser, { 0.40f, 1.0f, 0.80f, 1.0f });
                }
                else if (m_stats.deepScan)
                {
                    char buf[36];
                    sprintf_s(buf, "+%d REISHI", ast.resourceAmount);
                    float tagW = 82.0f;
                    float tagX = ast.position.x + camX - tagW * 0.5f;
                    Sprite_DrawRect(tagX, tagY, tagW, 13.0f, { 0.08f, 0.06f, 0.14f, 0.88f });
                    Sprite_DrawRectBorder(tagX, tagY, tagW, 13.0f, 1.0f, { 0.70f, 0.60f, 0.90f, 0.75f });
                    DrawMatrixString(tagX + 6.0f, tagY + 2.0f, buf, 1.3f, m_texLaser, { 0.85f, 0.78f, 0.98f, 0.95f });
                }
            }
        }
    }

    // 2b. Draw Ancient Space Chests (chest.png)
    for (const auto& chest : m_chests)
    {
        if (chest.isOpened) continue;
        float cX = chest.position.x + camX;
        float cY = chest.position.y + camY;
        float pulse = sinf(m_totalTime * 5.0f) * 0.20f + 0.80f;

        // Golden aura ring around chest
        Sprite_DrawCircle(cX, cY, chest.radius + 6.0f, 2.5f, { 1.0f, 0.85f, 0.25f, pulse }, 28);

        if (m_texChest != -1)
        {
            float size = 52.0f;
            Sprite_Draw(m_texChest, cX - size * 0.5f, cY - size * 0.5f, size, size,
                0, 0, Texture_GetWidth(m_texChest), Texture_GetHeight(m_texChest),
                chest.rotation, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f });
        }

        // Label above chest
        DrawMatrixString(cX - 40.0f, cY - chest.radius - 18.0f, "[ 1 KEY ]", 1.6f, m_texLaser, { 1.0f, 0.85f, 0.25f, 1.0f });
    }

    // 2c. Draw Final Boss Orbiting Cores / Ghost Orbs (boss_orb.png)
    if (m_texBossOrb != -1)
    {
        int oW = Texture_GetWidth(m_texBossOrb);
        int oH = Texture_GetHeight(m_texBossOrb);
        for (const auto& orb : m_bossOrbs)
        {
            if (!orb.alive) continue;
            float w = (float)oW * orb.scale;
            float h = (float)oH * orb.scale;
            DirectX::XMFLOAT4 col = (orb.flashTimer > 0.0f)
                ? DirectX::XMFLOAT4(1.0f, 0.3f, 0.3f, 1.0f)
                : orb.isGhost
                    ? DirectX::XMFLOAT4(0.65f, 0.85f, 1.0f, 0.75f + 0.20f * sinf(m_totalTime * 12.0f))
                    : DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

            // Glowing energy core ring
            Sprite_DrawCircle(orb.position.x + camX, orb.position.y + camY, orb.radius + 4.0f, 2.0f,
                orb.isGhost ? DirectX::XMFLOAT4(0.4f, 0.9f, 1.0f, 0.6f) : DirectX::XMFLOAT4(1.0f, 0.55f, 0.2f, 0.75f), 20);

            Sprite_Draw(m_texBossOrb, orb.position.x + camX - w * 0.5f, orb.position.y + camY - h * 0.5f, w, h,
                0, 0, oW, oH, orb.angle * 2.0f, { 1.0f, 1.0f }, col);

            // If destructible, draw mini HP indicator bar
            if (!orb.isGhost)
            {
                float obw = 32.0f;
                float obh = 4.0f;
                float obx = orb.position.x + camX - obw * 0.5f;
                float oby = orb.position.y + camY - h * 0.5f - 8.0f;
                Sprite_DrawRect(obx, oby, obw, obh, { 0.2f, 0.05f, 0.05f, 0.8f });
                float hpRatio = std::clamp(orb.hp / orb.maxHp, 0.0f, 1.0f);
                Sprite_DrawRect(obx, oby, obw * hpRatio, obh, { 1.0f, 0.45f, 0.2f, 1.0f });
            }
        }
    }

    // 2c-bis. Ghost Cross Fire telegraph: warning glow at the captured target + tether lines
    // from each Ghost Orb, so the player can read where the volley is about to converge.
    for (const auto& ast : m_asteroids)
    {
        if (ast.isBoss && ast.bossType == 4)
        {
            if (ast.crossFireWarningActive)
            {
                float warnRatio = std::clamp(ast.crossFireWarningTimer / EnemyConfig::BossFinal.ghostCrossFireWarning, 0.0f, 1.0f);
                float pulse = sinf(m_totalTime * 24.0f) * 0.20f + 0.80f;
                float tX = ast.crossFireTarget.x + camX;
                float tY = ast.crossFireTarget.y + camY;

                // Shrinking outer ring converging on the target as the warning runs out
                Sprite_DrawCircle(tX, tY, 14.0f + 46.0f * warnRatio, 2.5f, { 1.0f, 0.25f, 0.30f, 0.85f * pulse }, 28);
                Sprite_DrawCircle(tX, tY, 8.0f, 2.0f, { 1.0f, 0.55f, 0.35f, 0.90f }, 16);
                DrawMatrixString(tX - 6.0f, tY - 34.0f, "!", 2.0f, m_texLaser, { 1.0f, 0.30f, 0.30f, 1.0f });

                // Tether lines from each Ghost Orb to the captured target
                for (const auto& orb : m_bossOrbs)
                {
                    if (!orb.isGhost || !orb.alive) continue;
                    Sprite_DrawLine(orb.position.x + camX, orb.position.y + camY, tX, tY, 1.5f, { 1.0f, 0.35f, 0.35f, 0.55f * pulse });
                }
            }
            break;
        }
    }

    // 2d. Draw Final Boss Falling & Embedded Blades (boss_blade.png)
    for (const auto& blade : m_bossBlades)
    {
        if (blade.state == BladeState::Warning)
        {
            // Vertical transparent red warning column
            float warnPulse = sinf(m_totalTime * 28.0f) * 0.25f + 0.65f;
            DirectX::XMFLOAT4 warnCol{ 1.0f, 0.20f, 0.20f, warnPulse * 0.35f };
            DirectX::XMFLOAT4 lineCol{ 1.0f, 0.30f, 0.30f, warnPulse * 0.85f };

            Sprite_DrawRect(blade.position.x + camX - 16.0f, 0.0f, 32.0f, (float)SCREEN_HEIGHT, warnCol);
            Sprite_DrawLine(blade.position.x + camX, 0.0f, blade.position.x + camX, (float)SCREEN_HEIGHT, 2.0f, lineCol);

            // Top and target position exclamation markers
            DrawMatrixString(blade.position.x + camX - 6.0f, 25.0f, "!", 2.2f, m_texLaser, { 1.0f, 0.3f, 0.3f, 1.0f });
            Sprite_DrawCircle(blade.position.x + camX, blade.targetY + camY, 20.0f, 2.0f, lineCol, 18);
        }
        else if ((blade.state == BladeState::Falling || blade.state == BladeState::Embedded) && m_texBossBlade != -1)
        {
            int bW = Texture_GetWidth(m_texBossBlade);
            int bH = Texture_GetHeight(m_texBossBlade);
            float w = (float)bW * blade.scale;
            float h = (float)bH * blade.scale;

            // Rotating 90 degrees so sword points downwards
            Sprite_Draw(m_texBossBlade, blade.position.x + camX - w * 0.5f, blade.position.y + camY - h * 0.5f, w, h,
                0, 0, bW, bH, blade.rotation, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f });

            if (blade.state == BladeState::Embedded)
            {
                // Ground impact energy ring
                float pulse = sinf(m_totalTime * 8.0f) * 0.20f + 0.80f;
                Sprite_DrawCircle(blade.position.x + camX, blade.position.y + camY + 8.0f, 18.0f, 2.0f,
                    { 1.0f, 0.40f, 0.25f, 0.75f * pulse }, 18);
            }
        }
    }

    // 3. Draw Enemies
    for (const auto& enemy : m_enemies)
    {
        if (enemy.destroyed || m_texEnemy1 == -1) continue;
        int texW = Texture_GetWidth(m_texEnemy1);
        int texH = Texture_GetHeight(m_texEnemy1);
        float w = (float)texW * enemy.scale;
        float h = (float)texH * enemy.scale;

        DirectX::XMFLOAT4 col = (enemy.flashTimer > 0.0f) ? DirectX::XMFLOAT4(1.0f, 0.3f, 0.3f, 1.0f) : DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        Sprite_Draw(m_texEnemy1, enemy.position.x + camX - w * 0.5f, enemy.position.y + camY - h * 0.5f, w, h,
            0, 0, texW, texH, enemy.rotation, { 1.0f, 1.0f }, col);
    }

    // 4. Draw Enemy Projectiles
    for (const auto& bullet : m_enemyProjectiles)
    {
        DirectX::XMFLOAT4 bCol = bullet.isReflected
            ? DirectX::XMFLOAT4(0.35f, 1.0f, 0.60f, 1.0f) // Reflected green
            : DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

        if (bullet.isBossSpiral && m_texBossProjectile != -1)
        {
            // Boss 2 Custom Projectile (projectile.png)
            // The texture image stands vertically pointing UP.
            // Velocity heading angle theta where (0,-1) is UP:
            float rotAngle = atan2f(bullet.velocity.x, -bullet.velocity.y);

            int texW = Texture_GetWidth(m_texBossProjectile);
            int texH = Texture_GetHeight(m_texBossProjectile);
            float scale = 0.055f;
            float pw = (float)texW * scale;
            float ph = (float)texH * scale;

            Sprite_Draw(m_texBossProjectile,
                bullet.position.x + camX - pw * 0.5f,
                bullet.position.y + camY - ph * 0.5f,
                pw, ph,
                0, 0, texW, texH,
                rotAngle, { 1.0f, 1.0f }, bCol);
        }
        else if (m_texEnemy1Bullet != -1)
        {
            float bX = bullet.position.x + camX;
            float bY = bullet.position.y + camY;
            float bSize = EnemyConfig::Drone.bulletVisualSize;

            int texW = Texture_GetWidth(m_texEnemy1Bullet);
            int texH = Texture_GetHeight(m_texEnemy1Bullet);
            if (texW <= 0) texW = 500;
            if (texH <= 0) texH = 500;

            float rotAngle = atan2f(bullet.velocity.x, -bullet.velocity.y);

            // 1. Soft organic ambient glow behind sprite (clean, no geometric rings)
            float glowSize = bSize * 1.28f;
            DirectX::XMFLOAT4 glowCol = bullet.isReflected
                ? DirectX::XMFLOAT4(0.20f, 1.0f, 0.40f, 0.45f)
                : DirectX::XMFLOAT4(1.0f, 0.30f, 0.15f, 0.50f);

            Sprite_Draw(m_texEnemy1Bullet,
                bX - glowSize * 0.5f,
                bY - glowSize * 0.5f,
                glowSize, glowSize,
                0, 0, texW, texH,
                rotAngle, { 1.0f, 1.0f }, glowCol);

            // 2. Crisp, ultra-bright high-contrast core bullet
            DirectX::XMFLOAT4 coreCol = bullet.isReflected
                ? DirectX::XMFLOAT4(0.50f, 1.20f, 0.70f, 1.0f)
                : DirectX::XMFLOAT4(1.25f, 1.15f, 1.05f, 1.0f);

            Sprite_Draw(m_texEnemy1Bullet,
                bX - bSize * 0.5f,
                bY - bSize * 0.5f,
                bSize, bSize,
                0, 0, texW, texH,
                rotAngle, { 1.0f, 1.0f }, coreCol);
        }
    }

    // 5. Draw Collectible Pickups (with glowing halos and clear textures)
    for (const auto& p : m_pickups)
    {
        float bob = sinf(m_totalTime * 4.5f + p.position.x * 0.05f) * 3.0f;
        float pX = p.position.x + camX;
        float pY = p.position.y + camY + bob;

        if (p.type == PickupType::Reishi && m_texResources != -1)
        {
            float rw = 38.0f;
            float rh = 50.0f;
            Sprite_Draw(m_texResources, pX - rw * 0.5f, pY - rh * 0.5f, rw, rh,
                0, 0, 256, 341, { 1.0f, 1.0f, 1.0f, 1.0f });
        }
        else if (p.type == PickupType::Vida && m_texVida != -1)
        {
            float size = 44.0f;
            Sprite_DrawCircle(pX, pY, 24.0f, 2.0f, { 0.40f, 0.85f, 1.0f, 0.70f }, 18);
            Sprite_Draw(m_texVida, pX - size * 0.5f, pY - size * 0.5f, size, size,
                0, 0, Texture_GetWidth(m_texVida), Texture_GetHeight(m_texVida), { 1.0f, 1.0f, 1.0f, 1.0f });
        }
        else if (p.type == PickupType::Disli && m_texDisli != -1)
        {
            float size = 48.0f;
            Sprite_DrawCircle(pX, pY, 26.0f, 2.5f, { 1.0f, 0.65f, 0.20f, 0.75f }, 20);
            Sprite_Draw(m_texDisli, pX - size * 0.5f, pY - size * 0.5f, size, size,
                0, 0, Texture_GetWidth(m_texDisli), Texture_GetHeight(m_texDisli), { 1.0f, 1.0f, 1.0f, 1.0f });
        }
        else if (p.type == PickupType::Cpu && m_texCpu != -1)
        {
            float size = 50.0f;
            Sprite_DrawCircle(pX, pY, 28.0f, 2.5f, { 0.35f, 0.95f, 1.0f, 0.85f }, 24);
            Sprite_Draw(m_texCpu, pX - size * 0.5f, pY - size * 0.5f, size, size,
                0, 0, Texture_GetWidth(m_texCpu), Texture_GetHeight(m_texCpu), { 1.0f, 1.0f, 1.0f, 1.0f });
        }
        else if (p.type == PickupType::Key && m_texKey != -1)
        {
            float size = 54.0f;
            Sprite_DrawCircle(pX, pY, 30.0f, 3.0f, { 1.0f, 0.85f, 0.25f, 0.95f }, 24);
            Sprite_Draw(m_texKey, pX - size * 0.5f, pY - size * 0.5f, size, size,
                0, 0, Texture_GetWidth(m_texKey), Texture_GetHeight(m_texKey), { 1.0f, 1.0f, 1.0f, 1.0f });
        }
    }

    // 5b. Draw Orbiting Swirl Resource Particles around Player
    for (const auto& orb : m_orbitingResources)
    {
        float ox = m_playerPos.x + camX + cosf(orb.orbitAngle) * orb.orbitRadius;
        float oy = m_playerPos.y + camY + sinf(orb.orbitAngle) * orb.orbitRadius;
        DirectX::XMFLOAT4 orbCol{ 1.0f, 0.85f, 0.35f, 0.95f };
        if (orb.type == PickupType::Vida) orbCol = { 0.45f, 0.85f, 1.0f, 0.95f };
        else if (orb.type == PickupType::Disli) orbCol = { 1.0f, 0.65f, 0.20f, 0.95f };
        else if (orb.type == PickupType::Cpu) orbCol = { 0.35f, 0.95f, 1.0f, 0.95f };
        else if (orb.type == PickupType::Key) orbCol = { 1.0f, 0.90f, 0.30f, 1.0f };

        Sprite_DrawCircle(ox, oy, 4.5f, 2.0f, orbCol, 12);
        Sprite_DrawRect(ox - 1.5f, oy - 1.5f, 3.0f, 3.0f, { 1.0f, 1.0f, 1.0f, 1.0f });
    }

    // 6. Draw Player spaceship
    bool isShipVisible = (m_playerHealth > 0 && m_runState == RunState::Active) ||
                         m_runState == RunState::ShipEntering ||
                         (m_runState == RunState::BossDefeated) ||
                         (m_runState == RunState::PlayerDying && m_deathSequenceTimer > 2.05f);

    if (m_texSpaceship != -1 && isShipVisible)
    {
        float w = 48.0f;
        float h = 72.0f;
        float bob = sin(m_totalTime * 2.2f) * 2.0f;

        DirectX::XMFLOAT4 shipColor{ 1.0f, 1.0f, 1.0f, 1.0f };

        if (m_isDashing)
        {
            shipColor = DirectX::XMFLOAT4(0.4f, 0.85f, 1.0f, 0.70f); // Cyan ghosting
        }
        else if (m_invincibleTimer > 0.0f)
        {
            int phase = (int)(m_invincibleTimer * 16.0f) % 2;
            shipColor = (phase == 0) ? DirectX::XMFLOAT4(1.0f, 0.35f, 0.35f, 0.45f) : DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 0.95f);
        }

        // Entry sequence: bright engine flame trailing below the ship as it decelerates in
        if (m_runState == RunState::ShipEntering)
        {
            float flamePulse = sinf(m_totalTime * 24.0f) * 0.15f + 0.85f;
            float flameLen = 34.0f * flamePulse;
            Sprite_DrawCircle(m_playerPos.x + camX, m_playerPos.y + camY + h * 0.42f, 10.0f * flamePulse, 0.0f,
                { 0.55f, 0.85f, 1.0f, 0.85f }, 16);
            Sprite_DrawLine(m_playerPos.x + camX, m_playerPos.y + camY + h * 0.30f,
                             m_playerPos.x + camX, m_playerPos.y + camY + h * 0.30f + flameLen,
                             6.0f, { 0.45f, 0.80f, 1.0f, 0.55f });
            bob = 0.0f; // No idle bob while actively decelerating into position
        }

        Sprite_Draw(m_texSpaceship, m_playerPos.x + camX - w * 0.5f, m_playerPos.y + camY - h * 0.5f + bob, w, h,
            0, 0, Texture_GetWidth(m_texSpaceship), Texture_GetHeight(m_texSpaceship),
            m_playerRotation, { 1.0f, 1.0f }, shipColor);

        // Draw Shield Bubble (Circular forcefield) around ship if active
        if (m_currentShield > 0)
        {
            float shieldRad = 38.0f;
            float pulse = sinf(m_totalTime * 6.0f) * 0.15f + 0.85f;
            Sprite_DrawCircle(m_playerPos.x + camX, m_playerPos.y + camY, shieldRad, 2.5f, { 0.3f, 0.85f, 1.0f, pulse }, 36);
            Sprite_DrawCircle(m_playerPos.x + camX, m_playerPos.y + camY, shieldRad - 3.0f, 1.2f, { 0.6f, 0.95f, 1.0f, pulse * 0.5f }, 32);
        }
    }

    // 7. Draw Stationary Sentry Defense Stations (taret.png)
    for (const auto& turret : m_turrets)
    {
        float tx = turret.position.x + camX;
        float ty = turret.position.y + camY;
        float rad = turret.defenseRadius;

        // A. Operational Defense Field Perimeter
        if (turret.isPlayerInZone)
        {
            float pulse = sinf(m_totalTime * 6.0f) * 0.15f + 0.85f;
            DirectX::XMFLOAT4 activeBorder{ 0.30f, 1.0f, 0.65f, 0.60f * pulse };

            // Primary active zone ring
            Sprite_DrawCircle(tx, ty, rad, 2.2f, activeBorder, 48);

            // Radar scan wave sweep
            float sweepRad = fmodf(m_totalTime * 95.0f, rad);
            float sweepAlpha = (1.0f - (sweepRad / rad)) * 0.35f;
            Sprite_DrawCircle(tx, ty, sweepRad, 1.5f, { 0.35f, 1.0f, 0.70f, sweepAlpha }, 36);

            // Status Badge above Turret
            DrawMatrixString(tx - 42.0f, ty - 38.0f, "[ TARET AKTIF ]", 1.4f, m_texLaser, { 0.35f, 1.0f, 0.60f, 0.95f });
        }
        else
        {
            // Standby perimeter ring
            Sprite_DrawCircle(tx, ty, rad, 1.2f, { 0.35f, 0.65f, 0.90f, 0.25f }, 48);

            // Standby Status Badge
            DrawMatrixString(tx - 36.0f, ty - 38.0f, "[ ALANA GIR ]", 1.3f, m_texLaser, { 0.60f, 0.75f, 0.95f, 0.55f });
        }

        // B. Mechanical Base Sentry Platform
        Sprite_DrawCircle(tx, ty, 28.0f, 2.5f, { 0.15f, 0.22f, 0.35f, 0.90f }, 24);
        DirectX::XMFLOAT4 coreCol = turret.isPlayerInZone
            ? DirectX::XMFLOAT4(0.30f, 1.0f, 0.65f, 0.90f)
            : DirectX::XMFLOAT4(0.35f, 0.65f, 0.95f, 0.50f);
        Sprite_DrawCircle(tx, ty, 14.0f, 1.8f, coreCol, 16);

        // C. Turret Sprite (asset/taret.png)
        float tSize = 46.0f;
        if (m_texTaret != -1)
        {
            Sprite_Draw(m_texTaret, tx - tSize * 0.5f, ty - tSize * 0.5f, tSize, tSize,
                0, 0, Texture_GetWidth(m_texTaret), Texture_GetHeight(m_texTaret),
                turret.rotation, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f });
        }
        else
        {
            Sprite_DrawRect(tx - 12.0f, ty - 12.0f, 24.0f, 24.0f, { 0.15f, 0.25f, 0.35f, 0.95f });
            Sprite_DrawRectBorder(tx - 12.0f, ty - 12.0f, 24.0f, 24.0f, 2.0f, coreCol);
        }
    }

    // 8. Draw Active Lasers (Multi-pass glowing beam)
    for (const auto& laser : m_lasers)
    {
        float startX = laser.start.x + camX;
        float startY = laser.start.y + camY;
        float endX = laser.end.x + camX;
        float endY = laser.end.y + camY;

        DirectX::XMFLOAT4 glowCol = laser.color;
        glowCol.w = 0.35f;
        float outerThick = m_isOvercharged ? 16.0f : 10.0f;
        float midThick = m_isOvercharged ? 9.0f : 6.0f;
        float coreThick = m_isOvercharged ? 4.0f : 2.5f;

        // Layer 1: Outer glow aura
        Sprite_DrawLine(startX, startY, endX, endY, outerThick, glowCol);

        // Layer 2: Main colored plasma beam
        Sprite_DrawLine(startX, startY, endX, endY, midThick, laser.color);

        // Layer 3: Intense white energy core
        Sprite_DrawLine(startX, startY, endX, endY, coreThick, { 1.0f, 1.0f, 1.0f, 0.95f });

        // Contact sparkle at target point
        if (m_texLaserHit != -1)
        {
            int frame = (int)(m_totalTime * 24.0f) % 8;
            int col = frame % 4;
            int row = frame / 4;
            int fW = 384;
            int fH = 512;
            float hitW = m_isOvercharged ? 48.0f : 36.0f;
            float hitH = m_isOvercharged ? 64.0f : 48.0f;
            Sprite_Draw(m_texLaserHit, endX - hitW * 0.5f, endY - hitH * 0.5f, hitW, hitH,
                col * fW, row * fH, fW, fH);
        }
    }

    // 9. Draw Circular Shockwaves (Halka / Rings)
    for (const auto& sw : m_shockwaves)
    {
        float alpha = 1.0f - (sw.lifetime / sw.maxLifetime);
        alpha = std::clamp(alpha, 0.0f, 1.0f);
        float rad = sw.currentRadius;

        // Primary outer shockwave ring
        Sprite_DrawCircle(sw.center.x + camX, sw.center.y + camY, rad, 3.5f, { 0.35f, 0.95f, 1.0f, alpha }, 48);

        // Secondary inner glowing ring
        if (rad > 15.0f)
        {
            Sprite_DrawCircle(sw.center.x + camX, sw.center.y + camY, rad * 0.85f, 2.0f, { 0.70f, 1.0f, 1.0f, alpha * 0.6f }, 36);
        }
    }

    // 10. Draw VFXs
    for (const auto& vfx : m_vfxs)
    {
        if (vfx.isSpriteSheet)
        {
            int col = vfx.currentFrame % 4;
            int row = vfx.currentFrame / 4;
            int fW = 384;
            int fH = 512;
            float w = (float)fW * vfx.scale;
            float h = (float)fH * vfx.scale;
            Sprite_Draw(m_texLaserHit, vfx.position.x + camX - w * 0.5f, vfx.position.y + camY - h * 0.5f, w, h,
                col * fW, row * fH, fW, fH);
        }
        else if (vfx.isMultiTexture)
        {
            int tex = vfx.textureSequence[vfx.currentFrame];
            float w = 128.0f * vfx.scale;
            float h = 128.0f * vfx.scale;
            Sprite_Draw(tex, vfx.position.x + camX - w * 0.5f, vfx.position.y + camY - h * 0.5f, w, h);
        }
    }

    // 11. Draw Floating Damage Popups (World of Warcraft Style Combat Text)
    if (m_texNumber != -1)
    {
        for (const auto& popup : m_damagePopups)
        {
            float t = popup.lifetime;
            float maxT = popup.maxLifetime;

            // WoW-style Elastic Pop Scale Animation:
            // High energy initial punch in first 0.12s, then settles with elastic spring
            float curScale = popup.baseScale;
            if (t < 0.12f)
            {
                float popFactor = 1.0f + (0.12f - t) * 7.5f; // Starts up to 1.9x
                curScale *= popFactor;
            }

            // Alpha smooth fade-out in the last 40% of lifetime
            float alpha = 1.0f;
            if (t > maxT * 0.55f)
            {
                float fadeProgress = (t - maxT * 0.55f) / (maxT * 0.45f);
                alpha = std::clamp(1.0f - fadeProgress * fadeProgress, 0.0f, 1.0f);
            }

            DirectX::XMFLOAT4 col = popup.color;
            col.w = alpha;

            if (popup.isTextLabel)
            {
                float fontSize = 1.5f * curScale;
                float textWidth = (float)popup.label.length() * 4.0f * fontSize;
                DrawMatrixString(popup.position.x + camX - textWidth * 0.5f, popup.position.y + camY - (5.0f * fontSize) * 0.5f,
                    popup.label.c_str(), fontSize, m_texLaser, col);
            }
            else
            {
                DrawDamageNumber(popup.position.x + camX, popup.position.y + camY, popup.damageAmount, curScale, m_texNumber, col, popup.isCritical, popup.isWeakpoint, m_texLaser);
            }
        }
    }

    // 11b. Draw Anomalous & Treasure Signal Banners
    if (m_anomalousWarningDisplayTimer > 0.0f)
    {
        float bannerW = 600.0f;
        float bannerH = 40.0f;
        float bannerX = (float)SCREEN_WIDTH * 0.5f - bannerW * 0.5f;
        float bannerY = 85.0f;
        float pulse = sinf(m_totalTime * 10.0f) * 0.25f + 0.75f;

        Sprite_DrawRect(bannerX, bannerY, bannerW, bannerH, { 0.25f, 0.18f, 0.06f, 0.95f });
        Sprite_DrawRectBorder(bannerX, bannerY, bannerW, bannerH, 2.0f, { 1.0f, 0.85f, 0.25f, pulse });
        DrawMatrixString(bannerX + 24.0f, bannerY + 10.0f, "! ANOMALOUS SIGNAL DETECTED: GOLDEN KEY ASTEROID !", 2.0f, m_texLaser, { 1.0f, 0.90f, 0.35f, 1.0f });
    }
    else if (m_stats.treasureSignal)
    {
        bool foundRich = false;
        for (const auto& ast : m_asteroids)
        {
            if (!ast.destroyed && !ast.isBoss && (ast.isAnomalousSignal || ast.resourceAmount >= 15))
            {
                foundRich = true;
                break;
            }
        }
        if (foundRich)
        {
            float pulse = sinf(m_totalTime * 6.0f) * 0.20f + 0.80f;
            float bW = 420.0f;
            float bH = 30.0f;
            float bX = (float)SCREEN_WIDTH * 0.5f - bW * 0.5f;
            float bY = 88.0f;
            Sprite_DrawRect(bX, bY, bW, bH, { 0.08f, 0.18f, 0.22f, 0.85f });
            Sprite_DrawRectBorder(bX, bY, bW, bH, 1.5f, { 0.35f, 0.95f, 0.85f, pulse });
            DrawMatrixString(bX + 16.0f, bY + 7.0f, "* TREASURE SIGNAL: RICH VEIN DETECTED *", 1.5f, m_texLaser, { 0.40f, 1.0f, 0.85f, 1.0f });
        }
    }

    // 12. HUD Elements
    // Top Left: Reishi Counts
    DrawMatrixString(40.0f, 25.0f, "REISHI", 2.5f, m_texLaser, { 0.6f, 0.9f, 1.0f, 1.0f });
    DrawNumber(40.0f, 48.0f, m_reishiCount, 5, 18.0f, m_texNumber);

    // Health Hearts under Reishi
    if (m_texHeart != -1)
    {
        float heartSize = 26.0f;
        float startX = 40.0f;
        float startY = 82.0f;

        for (int i = 0; i < m_stats.maxHealth; ++i)
        {
            float hX = startX + i * (heartSize + 6.0f);
            DirectX::XMFLOAT4 hColor = (i < m_playerHealth)
                ? DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f)
                : DirectX::XMFLOAT4(0.25f, 0.25f, 0.25f, 0.40f);

            Sprite_Draw(m_texHeart, hX, startY, heartSize, heartSize,
                0, 0, Texture_GetWidth(m_texHeart), Texture_GetHeight(m_texHeart),
                hColor);
        }
    }

    // Top Right: Voyage Energy (Fuel)
    DrawMatrixString(SCREEN_WIDTH - 240.0f, 30.0f, "VOYAGE ENERGY", 2.2f, m_texLaser, { 0.6f, 0.9f, 1.0f, 1.0f });
    
    // Fuel Bar
    Sprite_Draw(m_texLaser, SCREEN_WIDTH - 240.0f, 55.0f, 200.0f, 16.0f, 768, 512, 10, 10, { 0.1f, 0.2f, 0.3f, 1.0f });
    float fuelPct = m_fuel / m_stats.maxFuel;
    if (fuelPct > 0.0f)
    {
        Sprite_Draw(m_texLaser, SCREEN_WIDTH - 240.0f, 55.0f, 200.0f * fuelPct, 16.0f, 768, 512, 10, 10, { 0.0f, 0.95f, 1.0f, 1.0f });
    }
    int fuelDisplayVal = (int)(fuelPct * 100.0f);
    DrawNumber(SCREEN_WIDTH - 290.0f, 51.0f, fuelDisplayVal, 3, 16.0f, m_texNumber);

    // Skill Bar HUD (Bottom Center, above Boss Meter)
    DrawSkillBar();

    // 13. Bottom Calamity Progress & Boss Encounter Bar
    float meterY = SCREEN_HEIGHT - 54.0f;
    float barW = (float)SCREEN_WIDTH - 80.0f;
    float barH = 14.0f;
    float barX = 40.0f;

    // Left Title
    DrawMatrixString(barX, meterY, "CALAMITY PROGRESS", 2.2f, m_texLaser, { 1.0f, 0.40f, 0.40f, 1.0f });

    // Center Percentage
    int calPercent = (int)(std::clamp(m_calamityFillDisplay, 0.0f, 1.0f) * 100.0f);
    char pctBuf[32];
    sprintf_s(pctBuf, "BOSS APPROACH: %d%%", calPercent);
    DrawMatrixString(SCREEN_WIDTH * 0.5f - 110.0f, meterY, pctBuf, 2.0f, m_texLaser, { 1.0f, 0.90f, 0.85f, 1.0f });

    // Right Stage Indicator
    char secBuf[32];
    sprintf_s(secBuf, "SECTOR %d", m_calamity.level);
    DrawMatrixString(barX + barW - 120.0f, meterY, secBuf, 2.2f, m_texLaser, { 1.0f, 0.70f, 0.25f, 1.0f });

    // Bar Background
    Sprite_DrawRect(barX, meterY + 18.0f, barW, barH, { 0.16f, 0.06f, 0.08f, 0.92f });
    Sprite_DrawRectBorder(barX, meterY + 18.0f, barW, barH, 2.0f, { 0.85f, 0.25f, 0.30f, 0.85f });

    // Filled Bar
    if (m_calamityFillDisplay > 0.001f)
    {
        float fillW = (barW - 4.0f) * std::clamp(m_calamityFillDisplay, 0.0f, 1.0f);
        Sprite_DrawRect(barX + 2.0f, meterY + 20.0f, fillW, barH - 4.0f, { 0.95f, 0.20f, 0.25f, 1.0f });
        // Leading edge glow
        Sprite_DrawRect(barX + 2.0f + fillW - 4.0f, meterY + 20.0f, 4.0f, barH - 4.0f, { 1.0f, 0.90f, 0.80f, 1.0f });
    }

    // Boss Warning Flashing
    if (m_bossWarningTimer > 0.0f)
    {
        if ((int)(m_bossWarningTimer * 4.0f) % 2 == 0)
        {
            DrawMatrixString(SCREEN_WIDTH * 0.5f - 240.0f, SCREEN_HEIGHT * 0.5f - 40.0f, "CALAMITY ENCOUNTER", 4.0f, m_texLaser, { 1.0f, 0.2f, 0.2f, 1.0f });
            DrawMatrixString(SCREEN_WIDTH * 0.5f - 140.0f, SCREEN_HEIGHT * 0.5f + 10.0f, "WARNING WARNING", 3.0f, m_texLaser, { 1.0f, 0.2f, 0.2f, 1.0f });
        }
    }

    // Tutorial: pulsing highlight around whichever HUD element the current line is teaching
    DrawTutorialHighlights();

    // Tutorial: small objective reminder once the intro dialogue closes and control is handed
    // back, until the player destroys their first asteroid and the walkthrough continues
    if (m_tutorialActive && !m_tutorialDialogueActive && !m_tutorialAsteroidTriggered)
    {
        float bannerW = 420.0f;
        float bannerH = 34.0f;
        float bannerX = (float)SCREEN_WIDTH * 0.5f - bannerW * 0.5f;
        float bannerY = 90.0f;
        float pulse = sinf(m_totalTime * 4.0f) * 0.15f + 0.85f;
        Sprite_DrawRect(bannerX, bannerY, bannerW, bannerH, { 0.06f, 0.08f, 0.12f, 0.80f });
        Sprite_DrawRectBorder(bannerX, bannerY, bannerW, bannerH, 1.5f, { 1.0f, 0.85f, 0.25f, pulse });
        const char* objectiveText = "OBJECTIVE: DESTROY AN ASTEROID";
        DrawMatrixString(CenteredTextX(objectiveText, 1.6f, (float)SCREEN_WIDTH * 0.5f), bannerY + 9.0f, objectiveText, 1.6f, m_texLaser, { 1.0f, 0.90f, 0.60f, 1.0f });
    }

    // Ancient Chest Pause Modal
    if (m_isChestModalActive)
    {
        DrawChestModal();
    }

    // Energy Depleted Dialog Popup Modal
    if (m_runState == RunState::EnergyDepleted)
    {
        DrawEnergyDepletedModal();
    }

    // End of Run summary modal
    if (m_runState == RunState::RunEnded)
    {
        DrawRunSummary();
    }

    // Tutorial dialogue box draws on top of everything else, same as the other modals
    DrawTutorialDialogue();
}

void Game::DrawChestModal()
{
    // Dim background overlay (Game paused)
    Sprite_DrawRect(0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, { 0.03f, 0.02f, 0.05f, 0.86f });

    float cardW = 540.0f;
    float cardH = 340.0f;
    float cardX = (float)SCREEN_WIDTH * 0.5f - cardW * 0.5f;
    float cardY = (float)SCREEN_HEIGHT * 0.5f - cardH * 0.5f;

    // Glowing Modal Frame
    float pulse = sinf(m_totalTime * 6.0f) * 0.15f + 0.85f;
    Sprite_DrawRect(cardX, cardY, cardW, cardH, { 0.09f, 0.06f, 0.12f, 0.98f });
    Sprite_DrawRectBorder(cardX, cardY, cardW, cardH, 2.5f, { 1.0f, 0.85f, 0.25f, pulse });
    Sprite_DrawRectBorder(cardX - 4.0f, cardY - 4.0f, cardW + 8.0f, cardH + 8.0f, 1.0f, { 1.0f, 0.85f, 0.25f, 0.35f });

    // Chest Icon
    float iconSize = 72.0f;
    float iconX = cardX + cardW * 0.5f - iconSize * 0.5f;
    float iconY = cardY + 28.0f;
    if (m_texChest != -1)
    {
        Sprite_Draw(m_texChest, iconX, iconY, iconSize, iconSize,
            0, 0, Texture_GetWidth(m_texChest), Texture_GetHeight(m_texChest), { 1.0f, 1.0f, 1.0f, 1.0f });
    }

    // Title
    DrawMatrixString(cardX + 60.0f, cardY + 115.0f, "ANCIENT SPACE CHEST DISCOVERED!", 2.4f, m_texLaser, { 1.0f, 0.88f, 0.30f, 1.0f });

    // Description
    DrawMatrixString(cardX + 45.0f, cardY + 150.0f, "Contains massive amounts of CPU, Gears, Screws, and Reishi.", 1.7f, m_texLaser, { 0.85f, 0.88f, 0.92f, 0.9f });

    // Status: Current keys
    char keyBuf[48];
    sprintf_s(keyBuf, "AVAILABLE KEYS: %d", m_resources.key);
    DirectX::XMFLOAT4 keyCol = (m_resources.key >= 1) ? DirectX::XMFLOAT4(0.35f, 0.95f, 0.55f, 1.0f) : DirectX::XMFLOAT4(0.95f, 0.35f, 0.35f, 1.0f);
    DrawMatrixString(cardX + 185.0f, cardY + 185.0f, keyBuf, 1.9f, m_texLaser, keyCol);

    // Buttons
    float btnW = 220.0f;
    float btnH = 50.0f;
    float btnY = cardY + cardH - 75.0f;
    float btn1X = cardX + 35.0f;
    float btn2X = cardX + cardW - btnW - 35.0f;

    bool canOpen = (m_resources.key >= 1);

    // Button 1: Open with 1 Key
    DirectX::XMFLOAT4 btn1Bg = canOpen ? DirectX::XMFLOAT4(0.18f, 0.28f, 0.15f, 0.95f) : DirectX::XMFLOAT4(0.12f, 0.10f, 0.12f, 0.8f);
    DirectX::XMFLOAT4 btn1Border = canOpen ? DirectX::XMFLOAT4(0.40f, 1.0f, 0.60f, 1.0f) : DirectX::XMFLOAT4(0.40f, 0.35f, 0.40f, 0.6f);
    Sprite_DrawRect(btn1X, btnY, btnW, btnH, btn1Bg);
    Sprite_DrawRectBorder(btn1X, btnY, btnW, btnH, 2.0f, btn1Border);

    if (canOpen)
    {
        DrawMatrixString(btn1X + 16.0f, btnY + 16.0f, "[ OPEN WITH 1 KEY (E) ]", 1.8f, m_texLaser, { 0.4f, 1.0f, 0.7f, 1.0f });
    }
    else
    {
        DrawMatrixString(btn1X + 24.0f, btnY + 16.0f, "[ INSUFFICIENT KEYS ]", 1.8f, m_texLaser, { 0.6f, 0.5f, 0.5f, 0.7f });
    }

    // Button 2: Decline & Save Key
    Sprite_DrawRect(btn2X, btnY, btnW, btnH, { 0.16f, 0.10f, 0.16f, 0.95f });
    Sprite_DrawRectBorder(btn2X, btnY, btnW, btnH, 2.0f, { 0.85f, 0.45f, 0.45f, 0.9f });
    DrawMatrixString(btn2X + 38.0f, btnY + 16.0f, "[ DECLINE (ESC) ]", 1.8f, m_texLaser, { 0.95f, 0.75f, 0.75f, 1.0f });
}

void Game::DrawRunSummary()
{
    // Dim background overlay
    Sprite_DrawRect(0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, { 0.04f, 0.02f, 0.06f, 0.84f });

    // Center Modal Card
    float cardW = 680.0f;
    float cardH = 460.0f;
    float cardX = (float)SCREEN_WIDTH * 0.5f - cardW * 0.5f;
    float cardY = (float)SCREEN_HEIGHT * 0.5f - cardH * 0.5f;

    DirectX::XMFLOAT4 borderCol = m_bossVictory
        ? DirectX::XMFLOAT4(1.0f, 0.85f, 0.25f, 0.95f) // Radiant Gold
        : (m_playerHealth <= 0)
            ? DirectX::XMFLOAT4(0.95f, 0.30f, 0.30f, 0.95f) // Crimson
            : DirectX::XMFLOAT4(0.35f, 0.85f, 1.0f, 0.95f); // Cyan

    // Modal background & glowing border
    Sprite_DrawRect(cardX, cardY, cardW, cardH, { 0.09f, 0.06f, 0.13f, 0.98f });
    Sprite_DrawRectBorder(cardX, cardY, cardW, cardH, 2.5f, borderCol);
    Sprite_DrawRectBorder(cardX - 4.0f, cardY - 4.0f, cardW + 8.0f, cardH + 8.0f, 1.0f, { borderCol.x, borderCol.y, borderCol.z, 0.35f });

    // Top Header Banner
    if (m_bossVictory)
    {
        DrawMatrixString(cardX + 45.0f, cardY + 24.0f, "SECTOR VICTORY: CALAMITY BOSS ELIMINATED!", 2.6f, m_texLaser, { 1.0f, 0.88f, 0.25f, 1.0f });
    }
    else if (m_playerHealth <= 0)
    {
        DrawMatrixString(cardX + 48.0f, cardY + 24.0f, "HULL BREACH: EMERGENCY EVACUATION PROTOCOL", 2.5f, m_texLaser, { 1.0f, 0.35f, 0.35f, 1.0f });
    }
    else
    {
        DrawMatrixString(cardX + 45.0f, cardY + 24.0f, "ENERGY DEPLETED: EXPEDITION CONCLUDED", 2.4f, m_texLaser, { 0.40f, 0.95f, 1.0f, 1.0f });
    }

    DrawMatrixString(cardX + 165.0f, cardY + 58.0f, "MISSION LOOT & EXPEDITION REPORT", 1.8f, m_texLaser, { 0.85f, 0.85f, 0.90f, 0.85f });
    Sprite_DrawRect(cardX + 24.0f, cardY + 84.0f, cardW - 48.0f, 1.5f, { 0.35f, 0.45f, 0.60f, 0.5f });

    // ==========================================
    // 5 RESOURCE LOOT CARDS (Reishi, Screws, Gears, CPU, Keys)
    // ==========================================
    float startLootX = cardX + 28.0f;
    float lootY = cardY + 104.0f;
    float lootCardW = 112.0f;
    float lootCardH = 110.0f;
    float lootGap = 16.0f;

    struct LootItem
    {
        int type;
        const char* label;
        int amount;
        int textureId;
        DirectX::XMFLOAT4 col;
    };

    const LootItem loot[5] = {
        { 0, "REISHI", m_runStats.reishiCollected, m_texResources, { 0.70f, 0.35f, 1.0f, 1.0f } },
        { 1, "SCREWS", m_runStats.vidaCollected, m_texVida, { 0.40f, 0.85f, 1.0f, 1.0f } },
        { 2, "GEARS", m_runStats.disliCollected, m_texDisli, { 1.0f, 0.65f, 0.20f, 1.0f } },
        { 3, "CPU", m_runStats.cpuCollected, m_texCpu, { 0.35f, 0.95f, 1.0f, 1.0f } },
        { 4, "KEYS", m_runStats.keyCollected, m_texKey, { 1.0f, 0.85f, 0.25f, 1.0f } }
    };

    for (int i = 0; i < 5; ++i)
    {
        float lx = startLootX + i * (lootCardW + lootGap);
        Sprite_DrawRect(lx, lootY, lootCardW, lootCardH, { 0.14f, 0.10f, 0.18f, 0.95f });
        Sprite_DrawRectBorder(lx, lootY, lootCardW, lootCardH, 1.5f, loot[i].col);

        // Icon
        float iconSize = 34.0f;
        float iconCenterX = lx + lootCardW * 0.5f;
        float iconCenterY = lootY + 28.0f;

        if (loot[i].type == 0 && m_texResources != -1)
        {
            Sprite_Draw(m_texResources, iconCenterX - 14.0f, iconCenterY - 18.0f, 28.0f, 36.0f, 0, 0, 256, 341, { 1.0f, 1.0f, 1.0f, 1.0f });
        }
        else if (loot[i].textureId != -1)
        {
            Sprite_Draw(loot[i].textureId, iconCenterX - iconSize * 0.5f, iconCenterY - iconSize * 0.5f, iconSize, iconSize,
                0, 0, Texture_GetWidth(loot[i].textureId), Texture_GetHeight(loot[i].textureId), { 1.0f, 1.0f, 1.0f, 1.0f });
        }

        // Label
        DrawMatrixString(lx + (lootCardW - (float)strlen(loot[i].label) * 8.0f) * 0.5f, lootY + 54.0f, loot[i].label, 1.6f, m_texLaser, { 0.85f, 0.85f, 0.90f, 0.9f });

        // Count: "+X"
        char cntBuf[16];
        sprintf_s(cntBuf, "+%d", loot[i].amount);
        DirectX::XMFLOAT4 numCol = (loot[i].amount > 0)
            ? DirectX::XMFLOAT4(0.35f, 0.95f, 0.55f, 1.0f) // Bright green
            : DirectX::XMFLOAT4(0.55f, 0.55f, 0.60f, 0.7f);

        DrawMatrixString(lx + (lootCardW - (float)strlen(cntBuf) * 10.0f) * 0.5f, lootY + 76.0f, cntBuf, 2.2f, m_texLaser, numCol);
    }

    // ==========================================
    // COMBAT & MINING STATS CARDS (Images + "x5" counts)
    // ==========================================
    float statsY = cardY + 230.0f;
    float statCardW = 150.0f;
    float statCardH = 76.0f;
    float statGap = 24.0f;
    float startStatsX = cardX + (cardW - (statCardW * 2.0f + statGap)) * 0.5f;

    // Card 1: Enemy Drone
    float s1X = startStatsX;
    Sprite_DrawRect(s1X, statsY, statCardW, statCardH, { 0.16f, 0.08f, 0.12f, 0.95f });
    Sprite_DrawRectBorder(s1X, statsY, statCardW, statCardH, 1.5f, { 1.0f, 0.30f, 0.35f, 0.8f });

    if (m_texEnemy1 != -1)
    {
        Sprite_Draw(m_texEnemy1, s1X + 16.0f, statsY + 14.0f, 48.0f, 48.0f,
            0, 0, Texture_GetWidth(m_texEnemy1), Texture_GetHeight(m_texEnemy1), { 1.0f, 1.0f, 1.0f, 1.0f });
    }
    char enemyBuf[16];
    sprintf_s(enemyBuf, "x%d", m_runStats.enemiesKilled);
    DrawMatrixString(s1X + 76.0f, statsY + 26.0f, enemyBuf, 2.8f, m_texLaser, { 1.0f, 0.35f, 0.35f, 1.0f });

    // Card 2: Asteroid
    float s2X = startStatsX + statCardW + statGap;
    Sprite_DrawRect(s2X, statsY, statCardW, statCardH, { 0.10f, 0.12f, 0.18f, 0.95f });
    Sprite_DrawRectBorder(s2X, statsY, statCardW, statCardH, 1.5f, { 0.35f, 0.80f, 1.0f, 0.8f });

    if (m_texAsteroid != -1)
    {
        Sprite_Draw(m_texAsteroid, s2X + 16.0f, statsY + 14.0f, 48.0f, 48.0f,
            0, 0, Texture_GetWidth(m_texAsteroid), Texture_GetHeight(m_texAsteroid), { 1.0f, 1.0f, 1.0f, 1.0f });
    }
    char astBuf[16];
    sprintf_s(astBuf, "x%d", m_runStats.asteroidsMined);
    DrawMatrixString(s2X + 76.0f, statsY + 26.0f, astBuf, 2.8f, m_texLaser, { 0.40f, 0.90f, 1.0f, 1.0f });

    // Divider Line
    Sprite_DrawRect(cardX + 24.0f, cardY + 320.0f, cardW - 48.0f, 1.5f, { 0.35f, 0.45f, 0.60f, 0.5f });

    // ==========================================
    // BOTTOM PROCEED BUTTON (Market / Upgrade Tree)
    // ==========================================
    float btnW = 440.0f;
    float btnH = 50.0f;
    float btnX = cardX + (cardW - btnW) * 0.5f;
    float btnY = cardY + 350.0f;

    float pulse = sinf(m_totalTime * 6.0f) * 0.15f + 0.85f;
    DirectX::XMFLOAT4 btnBg{ 0.20f, 0.12f, 0.25f, 0.95f };
    DirectX::XMFLOAT4 btnBorder{ borderCol.x, borderCol.y, borderCol.z, pulse };

    Sprite_DrawRect(btnX, btnY, btnW, btnH, btnBg);
    Sprite_DrawRectBorder(btnX, btnY, btnW, btnH, 2.0f, btnBorder);

    DrawMatrixString(btnX + 32.0f, btnY + 16.0f, "[ PROCEED / UPGRADE TREE ]", 2.0f, m_texLaser, { 1.0f, 0.95f, 0.85f, 1.0f });

    // Small help tip below
    DrawMatrixString(cardX + 225.0f, cardY + 416.0f, "( CLICK OR PRESS SPACE )", 1.5f, m_texLaser, { 0.70f, 0.70f, 0.75f, 0.7f });
}

void Game::DrawEnergyDepletedModal()
{
    // Dim background overlay
    Sprite_DrawRect(0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, { 0.03f, 0.02f, 0.06f, 0.88f });

    float cardW = 640.0f;
    float cardH = 360.0f;
    float cardX = (float)SCREEN_WIDTH * 0.5f - cardW * 0.5f;
    float cardY = (float)SCREEN_HEIGHT * 0.5f - cardH * 0.5f;

    // Glowing Modal Frame
    float pulse = sinf(m_totalTime * 6.0f) * 0.15f + 0.85f;
    DirectX::XMFLOAT4 borderCol{ 0.35f, 0.85f, 1.0f, pulse };
    Sprite_DrawRect(cardX, cardY, cardW, cardH, { 0.08f, 0.06f, 0.12f, 0.98f });
    Sprite_DrawRectBorder(cardX, cardY, cardW, cardH, 2.5f, borderCol);
    Sprite_DrawRectBorder(cardX - 4.0f, cardY - 4.0f, cardW + 8.0f, cardH + 8.0f, 1.0f, { 0.35f, 0.85f, 1.0f, 0.30f });

    // Header Title
    DrawMatrixString(cardX + 175.0f, cardY + 32.0f, "ENERGY DEPLETED", 3.4f, m_texLaser, { 1.0f, 0.85f, 0.25f, 1.0f });
    Sprite_DrawRect(cardX + 40.0f, cardY + 68.0f, cardW - 80.0f, 2.0f, { 1.0f, 0.85f, 0.25f, 0.50f });

    // Main Narrative Dialogue
    DrawMatrixString(cardX + 38.0f, cardY + 98.0f, "\"That's enough for today, we are out of energy Captain!\"", 2.0f, m_texLaser, { 0.98f, 0.95f, 0.88f, 1.0f });

    DrawMatrixString(cardX + 40.0f, cardY + 145.0f, "All minerals and rare components gathered during the expedition", 1.7f, m_texLaser, { 0.80f, 0.85f, 0.92f, 0.90f });
    DrawMatrixString(cardX + 75.0f, cardY + 172.0f, "have been safely transferred to the mothership cargo bay.", 1.7f, m_texLaser, { 0.80f, 0.85f, 0.92f, 0.90f });

    // Mini Earnings preview row
    char rBuf[96];
    sprintf_s(rBuf, "+%d REISHI | +%d SCREWS | +%d GEARS | +%d CPU | +%d KEYS",
        m_runStats.reishiCollected, m_runStats.vidaCollected, m_runStats.disliCollected, m_runStats.cpuCollected, m_runStats.keyCollected);
    DrawMatrixString(cardX + 50.0f, cardY + 218.0f, rBuf, 1.5f, m_texLaser, { 0.40f, 0.95f, 0.65f, 1.0f });

    // Return Button (Center Bottom)
    float btnW = 340.0f;
    float btnH = 54.0f;
    float btnX = cardX + cardW * 0.5f - btnW * 0.5f;
    float btnY = cardY + cardH - 80.0f;

    Sprite_DrawRect(btnX, btnY, btnW, btnH, { 0.15f, 0.25f, 0.35f, 0.95f });
    Sprite_DrawRectBorder(btnX, btnY, btnW, btnH, 2.0f, { 0.40f, 0.95f, 1.0f, 1.0f });
    DrawMatrixString(btnX + 22.0f, btnY + 17.0f, "[ RETURN TO BASE (SPACE / CLICK) ]", 1.7f, m_texLaser, { 1.0f, 1.0f, 1.0f, 1.0f });
}

// ============================================================================
// FIRST-TIME TUTORIAL (Sector 1 only, shown once per session)
// ============================================================================

void Game::StartTutorial()
{
    m_tutorialActive = true;
    m_tutorialIntroPending = true; // Popped open once the ship settles into Active
    m_tutorialAsteroidTriggered = false;
    m_tutorialPhase = TutorialPhase::Intro;
    m_tutorialQueue = {
        { "WELCOME ABOARD, CAPTAIN. USE W A S D TO STEER YOUR SHIP.", TutorialHighlight::None },
        { "YOUR LASER FIRES AUTOMATICALLY -- JUST GET CLOSE TO A TARGET.", TutorialHighlight::None },
        { "SEE THOSE ASTEROIDS? DESTROY THEM TO HARVEST VALUABLE RESOURCES.", TutorialHighlight::None },
    };
    m_tutorialLineIndex = 0;
    m_tutorialDialogueActive = false;
    m_tutorialPortraitPop = 0.30f;
}

void Game::TriggerTutorialAsteroidMilestone()
{
    if (!m_tutorialActive || m_tutorialAsteroidTriggered) return;

    m_tutorialAsteroidTriggered = true;
    m_tutorialPhase = TutorialPhase::PostAsteroid;
    m_tutorialQueue = {
        { "NICE SHOT! ASTEROIDS SCATTER REISHI AND OTHER RESOURCES WHEN DESTROYED.", TutorialHighlight::Resources },
        { "COLLECT THEM TO UPGRADE YOUR SHIP LATER FROM THE UPGRADE TREE.", TutorialHighlight::Resources },
        { "THIS IS YOUR VOYAGE ENERGY -- IT DRAINS OVER TIME. RUN OUT AND YOUR EXPEDITION ENDS.", TutorialHighlight::Time },
        { "THIS IS YOUR HULL INTEGRITY -- WATCH OUT FOR ENEMY FIRE, OR YOUR SHIP WILL FALL.", TutorialHighlight::Health },
        { "AND THIS GAUGE FILLS AS YOU EXPLORE -- WHEN IT'S FULL, A DANGEROUS THREAT WILL ARRIVE.", TutorialHighlight::BossTimer },
    };
    m_tutorialLineIndex = 0;
    m_tutorialDialogueActive = true;
    m_tutorialPortraitPop = 0.30f;
}

void Game::UpdateTutorialModal(float deltaTime)
{
    m_totalTime += deltaTime; // Keep the portrait bob/pulse animations alive while paused

    if (m_tutorialPortraitPop > 0.0f)
    {
        m_tutorialPortraitPop -= deltaTime;
    }

    bool confirm = InputKeyboard_IsTrigger(KK_SPACE) || InputKeyboard_IsTrigger(KK_ENTER) || InputMouse_IsTrigger(MOUSE_BUTTON_LEFT);
    if (!confirm) return;

    if (m_soundClick != -1) PlayAudio(m_soundClick);
    m_tutorialLineIndex++;
    m_tutorialPortraitPop = 0.30f;

    if (m_tutorialLineIndex >= (int)m_tutorialQueue.size())
    {
        m_tutorialDialogueActive = false;
        if (m_tutorialPhase == TutorialPhase::PostAsteroid)
        {
            m_tutorialActive = false;
            m_tutorialCompleted = true;
        }
    }
}

void Game::DrawTutorialDialogue()
{
    if (!m_tutorialDialogueActive || m_tutorialQueue.empty() || m_tutorialLineIndex >= (int)m_tutorialQueue.size()) return;

    const TutorialLine& line = m_tutorialQueue[m_tutorialLineIndex];

    float boxW = (float)SCREEN_WIDTH - 140.0f;
    float boxH = 168.0f;
    float boxX = 70.0f;
    float boxY = (float)SCREEN_HEIGHT - boxH - 34.0f;

    // Dim the world slightly behind the dialogue so it reads as a focused conversation
    Sprite_DrawRect(0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, { 0.0f, 0.0f, 0.02f, 0.18f });

    Sprite_DrawRect(boxX, boxY, boxW, boxH, { 0.05f, 0.06f, 0.10f, 0.94f });
    Sprite_DrawRectBorder(boxX, boxY, boxW, boxH, 2.5f, { 0.40f, 0.85f, 1.0f, 0.90f });
    Sprite_DrawRect(boxX, boxY, boxW, 4.0f, { 0.40f, 0.85f, 1.0f, 0.70f });

    // Portrait frame (pixel-art VN style: framed portrait on the left, text on the right)
    float portraitSize = 138.0f;
    float portraitX = boxX + 16.0f;
    float portraitY = boxY + (boxH - portraitSize) * 0.5f;
    Sprite_DrawRect(portraitX - 4.0f, portraitY - 4.0f, portraitSize + 8.0f, portraitSize + 8.0f, { 0.02f, 0.03f, 0.05f, 0.95f });
    Sprite_DrawRectBorder(portraitX - 4.0f, portraitY - 4.0f, portraitSize + 8.0f, portraitSize + 8.0f, 2.0f, { 0.45f, 0.90f, 1.0f, 1.0f });

    if (m_texSupporter != -1)
    {
        // Talking animation: gentle continuous bob, plus a quick pop-scale kick on line change
        float bob = sinf(m_totalTime * 3.0f) * 3.0f;
        float popT = std::clamp(m_tutorialPortraitPop / 0.30f, 0.0f, 1.0f);
        float drawSize = portraitSize * (1.0f + popT * 0.10f);
        float cx = portraitX + portraitSize * 0.5f;
        float cy = portraitY + portraitSize * 0.5f + bob;
        Sprite_Draw(m_texSupporter, cx - drawSize * 0.5f, cy - drawSize * 0.5f, drawSize, drawSize,
            0, 0, Texture_GetWidth(m_texSupporter), Texture_GetHeight(m_texSupporter),
            { 1.0f, 1.0f, 1.0f, 1.0f });
    }

    float textX = portraitX + portraitSize + 28.0f;

    // Speaker label
    DrawMatrixString(textX, boxY + 16.0f, "SHIP SUPPORT", 1.8f, m_texLaser, { 0.45f, 0.95f, 1.0f, 1.0f });
    Sprite_DrawRect(textX, boxY + 36.0f, 220.0f, 1.5f, { 0.45f, 0.95f, 1.0f, 0.6f });

    // Dialogue text, word-wrapped to fit beside the portrait
    float textMaxWidth = (boxX + boxW - 30.0f) - textX;
    DrawWrappedMatrixText(line.text.c_str(), textX, boxY + 56.0f, textMaxWidth, 26.0f, 1.9f, m_texLaser, { 0.92f, 0.95f, 1.0f, 1.0f });

    // Progress + Next prompt
    char progBuf[16];
    sprintf_s(progBuf, "%d / %d", m_tutorialLineIndex + 1, (int)m_tutorialQueue.size());
    DrawMatrixString(boxX + boxW - 210.0f, boxY + boxH - 28.0f, progBuf, 1.6f, m_texLaser, { 0.6f, 0.7f, 0.8f, 0.8f });

    float blink = (sinf(m_totalTime * 6.0f) > 0.0f) ? 1.0f : 0.4f;
    DrawMatrixString(boxX + boxW - 140.0f, boxY + boxH - 28.0f, "NEXT >", 1.7f, m_texLaser, { 1.0f, 0.85f, 0.30f, blink });
}

void Game::DrawTutorialHighlights()
{
    if (!m_tutorialDialogueActive || m_tutorialQueue.empty() || m_tutorialLineIndex >= (int)m_tutorialQueue.size()) return;

    TutorialHighlight hl = m_tutorialQueue[m_tutorialLineIndex].highlight;
    if (hl == TutorialHighlight::None) return;

    float pulse = sinf(m_totalTime * 6.0f) * 0.25f + 0.75f;
    DirectX::XMFLOAT4 glowCol{ 1.0f, 0.85f, 0.25f, pulse };

    if (hl == TutorialHighlight::Resources)
    {
        Sprite_DrawRectBorder(28.0f, 14.0f, 170.0f, 62.0f, 3.0f, glowCol);
    }
    else if (hl == TutorialHighlight::Health)
    {
        float heartsW = (float)m_stats.maxHealth * 32.0f - 6.0f;
        Sprite_DrawRectBorder(32.0f, 76.0f, heartsW + 16.0f, 38.0f, 3.0f, glowCol);
    }
    else if (hl == TutorialHighlight::Time)
    {
        Sprite_DrawRectBorder((float)SCREEN_WIDTH - 252.0f, 20.0f, 264.0f, 58.0f, 3.0f, glowCol);
    }
    else if (hl == TutorialHighlight::BossTimer)
    {
        float meterY = (float)SCREEN_HEIGHT - 54.0f;
        float barW = (float)SCREEN_WIDTH - 80.0f;
        Sprite_DrawRectBorder(32.0f, meterY - 6.0f, barW + 16.0f, 46.0f, 3.0f, glowCol);
    }
}

void Game::DrawUpgrade()
{
    m_bgRenderer.Render(0.0f, 0.0f);

    // Ambient ship dock, only when this screen was reached via the Main Menu's holographic
    // entrance (a mid-run TAB pause keeps the classic tree-only view untouched).
    if (m_upgradeEnteredFromMenu)
    {
        DrawAmbientShip(m_playerPos.x, m_playerPos.y, false);
    }

    m_upgradeTree.Draw(m_resources, m_upgradeTree.GetCurrentSectorIndex());
}

// ============================================================================
// MAIN MENU / TITLE SCREEN
// Reuses the existing background shader, player ship asset & rendering, VFX-style
// draw primitives, and input systems -- no separate render/player/input architecture.
// ============================================================================

void Game::InitMainMenu()
{
    m_menuPhase = MainMenuPhase::Boot;
    m_menuPhaseTimer = 0.0f;
    m_menuBootPulseTimer = 0.0f;
    m_menuSelectedIndex = 0;
    m_menuSelectPulse = 0.0f;
    m_menuEngineGlow = 0.45f;
    m_menuShakeOffset = { 0.0f, 0.0f };
    m_menuShakeTimer = 0.0f;
    m_menuScannerPulseTimer = 0.0f;
    m_menuScannerPulseCooldown = RandomFloat(4.0f, 7.0f);
    m_menuAmbientSpawnCooldown = RandomFloat(3.0f, 6.0f);
    m_menuAmbient.clear();
    m_menuPlaceholderTimer = 0.0f;

    m_playerPos = DirectX::XMFLOAT2((float)SCREEN_WIDTH * 0.5f, (float)SCREEN_HEIGHT * 0.62f);
    m_playerRotation = 0.0f;
    m_totalTime = 0.0f;

    if (m_menuStars.empty())
    {
        for (int i = 0; i < 90; ++i)
        {
            m_menuStars.push_back(DirectX::XMFLOAT3(
                RandomFloat(0.0f, (float)SCREEN_WIDTH),
                RandomFloat(0.0f, (float)SCREEN_HEIGHT),
                RandomFloat(0.25f, 1.0f)));
        }
    }
}

bool Game::AnyKeyPressed() const
{
    for (int k = 0x08; k <= 0xFE; ++k)
    {
        if (InputKeyboard_IsTrigger((Keyboard_Keys)k)) return true;
    }
    return InputMouse_IsTrigger(MOUSE_BUTTON_LEFT);
}

void Game::UpdateMenuAmbientWorld(float deltaTime)
{
    // Rare idle-world events: a distant asteroid, a drifting mineral, or (very rarely) a
    // mysterious silhouette crossing far in the background. Kept sparse and subtle.
    m_menuAmbientSpawnCooldown -= deltaTime;
    if (m_menuAmbientSpawnCooldown <= 0.0f)
    {
        float roll = RandomFloat(0.0f, 1.0f);
        bool fromLeft = RandomFloat(0.0f, 1.0f) < 0.5f;
        MenuAmbientObject obj;

        if (roll < 0.06f)
        {
            // Rare mysterious silhouette -- huge, faint, slow, far back
            obj.kind = 2;
            obj.scale = RandomFloat(0.55f, 0.85f);
            obj.alpha = 0.0f; // Fades in via the update loop below
            obj.position = { fromLeft ? -260.0f : (float)SCREEN_WIDTH + 260.0f, RandomFloat(60.0f, 220.0f) };
            obj.velocity = { (fromLeft ? 1.0f : -1.0f) * RandomFloat(10.0f, 16.0f), 0.0f };
            m_menuAmbientSpawnCooldown = RandomFloat(35.0f, 55.0f);
        }
        else if (roll < 0.55f)
        {
            // Distant asteroid drifting across
            obj.kind = 0;
            obj.scale = RandomFloat(0.05f, 0.11f);
            obj.alpha = RandomFloat(0.35f, 0.60f);
            obj.position = { fromLeft ? -80.0f : (float)SCREEN_WIDTH + 80.0f, RandomFloat(80.0f, (float)SCREEN_HEIGHT * 0.55f) };
            obj.velocity = { (fromLeft ? 1.0f : -1.0f) * RandomFloat(18.0f, 32.0f), RandomFloat(-4.0f, 4.0f) };
            obj.rotationSpeed = RandomFloat(-0.3f, 0.3f);
            m_menuAmbientSpawnCooldown = RandomFloat(9.0f, 16.0f);
        }
        else
        {
            // Small mineral passing by
            obj.kind = 1;
            obj.scale = 0.06f;
            obj.alpha = RandomFloat(0.55f, 0.85f);
            obj.position = { fromLeft ? -40.0f : (float)SCREEN_WIDTH + 40.0f, RandomFloat(150.0f, (float)SCREEN_HEIGHT * 0.65f) };
            obj.velocity = { (fromLeft ? 1.0f : -1.0f) * RandomFloat(45.0f, 75.0f), RandomFloat(-8.0f, 8.0f) };
            obj.rotationSpeed = RandomFloat(-1.5f, 1.5f);
            m_menuAmbientSpawnCooldown = RandomFloat(7.0f, 13.0f);
        }

        m_menuAmbient.push_back(obj);
    }

    for (auto it = m_menuAmbient.begin(); it != m_menuAmbient.end(); )
    {
        it->position.x += it->velocity.x * deltaTime;
        it->position.y += it->velocity.y * deltaTime;
        it->rotation += it->rotationSpeed * deltaTime;

        if (it->kind == 2)
        {
            it->alpha += (0.14f - it->alpha) * 1.2f * deltaTime; // Slow fade-in, holds faint
        }

        if (it->position.x < -320.0f || it->position.x > (float)SCREEN_WIDTH + 320.0f)
        {
            it = m_menuAmbient.erase(it);
            continue;
        }
        ++it;
    }

    // Ship scanner pulse: a small ring that occasionally sweeps out from the ship
    if (m_menuScannerPulseTimer > 0.0f)
    {
        m_menuScannerPulseTimer -= deltaTime;
    }
    else
    {
        m_menuScannerPulseCooldown -= deltaTime;
        if (m_menuScannerPulseCooldown <= 0.0f)
        {
            m_menuScannerPulseTimer = 0.9f;
            m_menuScannerPulseCooldown = RandomFloat(8.0f, 14.0f);
        }
    }
}

// Draws the reusable player ship sprite as an ambient/idle presence (title screen, or docked
// beside the Upgrade Tree) -- same asset & draw call as gameplay, just non-interactive here.
void Game::DrawAmbientShip(float shipCenterX, float shipCenterY, bool allowMouseTilt)
{
    if (m_texSpaceship == -1) return;

    float w = 64.0f;
    float h = 96.0f;
    float bob = sinf(m_totalTime * 1.4f) * 5.0f;

    float tiltShiftX = 0.0f;
    float tiltRot = 0.0f;
    if (allowMouseTilt)
    {
        float mouseX = (float)InputMouse_GetX();
        float mouseY = (float)InputMouse_GetY();
        float dx = mouseX - shipCenterX;
        float dy = mouseY - shipCenterY;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist > 1.0f)
        {
            tiltShiftX = std::clamp(dx / dist, -1.0f, 1.0f) * 7.0f;
            tiltRot = std::clamp(dx / 420.0f, -0.16f, 0.16f);
        }
    }

    float drawX = shipCenterX + tiltShiftX;
    float drawY = shipCenterY + bob;

    Sprite_Draw(m_texSpaceship, drawX - w * 0.5f, drawY - h * 0.5f, w, h,
        0, 0, Texture_GetWidth(m_texSpaceship), Texture_GetHeight(m_texSpaceship),
        tiltRot, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f });
}

void Game::ConfirmMenuSelection(int index)
{
    if (m_soundClick != -1) PlayAudio(m_soundClick);

    switch (index)
    {
    case 0: // START EXPEDITION
        m_menuPhase = MainMenuPhase::Launching;
        m_menuPhaseTimer = 0.0f;
        m_menuShakeTimer = 0.5f;
        m_menuShakeMaxDuration = 0.5f;
        m_menuShakeIntensity = 5.0f;
        break;
    case 1: // UPGRADE TREE -- ship docks left, tree slides in from the right
        m_upgradeEnteredFromMenu = true;
        m_upgradeIntroTimer = 0.0f;
        m_playerPos = DirectX::XMFLOAT2((float)SCREEN_WIDTH * 0.5f, (float)SCREEN_HEIGHT * 0.5f);
        m_upgradeTree.SetIntroOffsetX(650.0f);
        m_currentScene = GameScene::UpgradePlaceholder;
        break;
    case 2: // COLLECTION
    case 3: // SETTINGS
        m_menuPlaceholderMessage = "MODULE OFFLINE -- AVAILABLE IN A FUTURE UPDATE";
        m_menuPlaceholderTimer = 2.2f;
        break;
    case 4: // EXIT
        PostMessage(m_hWnd, WM_CLOSE, 0, 0);
        break;
    default:
        break;
    }
}

void Game::UpdateMainMenu(float deltaTime)
{
    m_totalTime += deltaTime;
    m_bgRenderer.Update(deltaTime, m_menuPhase == MainMenuPhase::Launching); // Reuse boss-intensity for the launch kick
    UpdateMenuAmbientWorld(deltaTime);

    if (m_menuPlaceholderTimer > 0.0f)
    {
        m_menuPlaceholderTimer -= deltaTime;
    }

    if (m_menuSelectPulse > 0.0f)
    {
        m_menuSelectPulse -= deltaTime;
    }

    // Camera kick decay (same shake-offset idiom used by gameplay's TriggerCameraShake)
    if (m_menuShakeTimer > 0.0f)
    {
        m_menuShakeTimer -= deltaTime;
        float progress = (m_menuShakeMaxDuration > 0.0001f) ? std::max(0.0f, m_menuShakeTimer / m_menuShakeMaxDuration) : 0.0f;
        float curIntensity = m_menuShakeIntensity * progress;
        m_menuShakeOffset.x = RandomFloat(-curIntensity, curIntensity);
        m_menuShakeOffset.y = RandomFloat(-curIntensity, curIntensity);
        if (m_menuShakeTimer <= 0.0f) m_menuShakeOffset = { 0.0f, 0.0f };
    }

    if (m_menuPhase == MainMenuPhase::Boot)
    {
        m_menuBootPulseTimer += deltaTime;
        if (AnyKeyPressed())
        {
            m_menuPhase = MainMenuPhase::BootGlitch;
            m_menuPhaseTimer = 0.0f;
            if (m_soundClick != -1) PlayAudio(m_soundClick);
        }
    }
    else if (m_menuPhase == MainMenuPhase::BootGlitch)
    {
        m_menuPhaseTimer += deltaTime;
        if (m_menuPhaseTimer >= 0.40f)
        {
            m_menuPhase = MainMenuPhase::Menu;
            m_menuPhaseTimer = 0.0f;
            m_menuSelectedIndex = 0;
            m_menuSelectPulse = 0.35f;
        }
    }
    else if (m_menuPhase == MainMenuPhase::Menu)
    {
        const int kMenuCount = 5;

        // Arrow keys / mouse only -- W/A/S/D are the gameplay movement keys and must stay
        // completely inert here so the ship never reacts to them on the title screen.
        if (InputKeyboard_IsTrigger(KK_DOWN))
        {
            m_menuSelectedIndex = (m_menuSelectedIndex + 1) % kMenuCount;
            m_menuSelectPulse = 0.35f;
            if (m_soundClick != -1) PlayAudio(m_soundClick);
        }
        else if (InputKeyboard_IsTrigger(KK_UP))
        {
            m_menuSelectedIndex = (m_menuSelectedIndex - 1 + kMenuCount) % kMenuCount;
            m_menuSelectPulse = 0.35f;
            if (m_soundClick != -1) PlayAudio(m_soundClick);
        }

        // Mouse hover selection
        float mouseX = (float)InputMouse_GetX();
        float mouseY = (float)InputMouse_GetY();
        float menuX = (float)SCREEN_WIDTH * 0.68f;
        float menuStartY = (float)SCREEN_HEIGHT * 0.30f;
        float itemH = 54.0f;
        for (int i = 0; i < kMenuCount; ++i)
        {
            float iy = menuStartY + (float)i * itemH;
            if (mouseX >= menuX - 40.0f && mouseX <= menuX + 340.0f && mouseY >= iy && mouseY <= iy + itemH - 8.0f)
            {
                if (m_menuSelectedIndex != i)
                {
                    m_menuSelectedIndex = i;
                    m_menuSelectPulse = 0.35f;
                }
            }
        }

        if (InputKeyboard_IsTrigger(KK_ENTER) || InputKeyboard_IsTrigger(KK_SPACE) || InputMouse_IsTrigger(MOUSE_BUTTON_LEFT))
        {
            ConfirmMenuSelection(m_menuSelectedIndex);
        }
    }
    else if (m_menuPhase == MainMenuPhase::Launching)
    {
        m_menuPhaseTimer += deltaTime;
        m_menuEngineGlow = std::min(1.0f, m_menuEngineGlow + deltaTime * 1.8f);

        // Accelerate upward and out of the title scene
        float accel = 900.0f * std::min(1.0f, m_menuPhaseTimer / 0.6f);
        m_playerPos.y -= accel * deltaTime;

        if (m_playerPos.y < -180.0f || m_menuPhaseTimer > 1.3f)
        {
            StartExpeditionQuickLaunch();
        }
    }
}

void Game::DrawMainMenuOptions(float camX, float camY)
{
    static const char* kMenuLabels[5] = {
        "START EXPEDITION", "UPGRADE TREE", "COLLECTION", "SETTINGS", "EXIT"
    };

    float menuX = (float)SCREEN_WIDTH * 0.68f;
    float menuStartY = (float)SCREEN_HEIGHT * 0.30f;
    float itemH = 54.0f;

    for (int i = 0; i < 5; ++i)
    {
        float iy = menuStartY + (float)i * itemH;
        bool isSelected = (i == m_menuSelectedIndex);
        float pulse = isSelected ? (sinf(m_totalTime * 7.0f) * 0.2f + 0.8f) : 1.0f;
        float selectBoost = isSelected ? (m_menuSelectPulse / 0.35f) : 0.0f;

        DirectX::XMFLOAT4 lineCol = isSelected
            ? DirectX::XMFLOAT4(0.55f * pulse, 0.95f * pulse, 1.0f * pulse, 1.0f)
            : DirectX::XMFLOAT4(0.55f, 0.62f, 0.72f, 0.65f);

        if (isSelected)
        {
            // Thin animated glow underline, not a filled UI box
            float glowW = 260.0f + selectBoost * 40.0f;
            Sprite_DrawRect(menuX - 24.0f + camX, iy + itemH - 14.0f + camY, glowW, 2.0f, { 0.45f, 0.9f, 1.0f, 0.55f * pulse });

            // Holographic cursor: a small pulsing chevron to the left of the label
            float cursorBob = sinf(m_totalTime * 8.0f) * 4.0f;
            float cx = menuX - 34.0f + cursorBob + camX;
            float cy = iy + 8.0f + camY;
            Sprite_DrawLine(cx, cy, cx + 10.0f, cy + 8.0f, 2.2f, lineCol);
            Sprite_DrawLine(cx, cy + 16.0f, cx + 10.0f, cy + 8.0f, 2.2f, lineCol);
        }

        char buf[48];
        sprintf_s(buf, "%s %s", isSelected ? ">" : " ", kMenuLabels[i]);
        DrawMatrixString(menuX + camX, iy + camY, buf, isSelected ? 2.1f : 1.9f, m_texLaser, lineCol);
    }

    DrawMatrixString(menuX + camX, menuStartY + 5.0f * itemH + 20.0f + camY,
        "ARROWS/MOUSE: SELECT   ENTER/CLICK: CONFIRM", 1.2f, m_texLaser, { 0.5f, 0.6f, 0.7f, 0.7f });
}

void Game::DrawMainMenu()
{
    m_bgRenderer.Render(m_menuShakeOffset.x, m_menuShakeOffset.y);

    float camX = m_menuShakeOffset.x;
    float camY = m_menuShakeOffset.y;

    // Parallax stars, with a very subtle mouse-driven drift for a lively feel
    float mouseX = (float)InputMouse_GetX();
    float mouseY = (float)InputMouse_GetY();
    float parX = (mouseX - (float)SCREEN_WIDTH * 0.5f) * 0.01f;
    float parY = (mouseY - (float)SCREEN_HEIGHT * 0.5f) * 0.01f;
    for (const auto& star : m_menuStars)
    {
        float twinkle = sinf(m_totalTime * 1.6f + star.x * 0.05f) * 0.35f + 0.65f;
        float b = star.z * twinkle;
        Sprite_DrawRect(star.x + parX + camX, star.y + parY + camY, 1.6f, 1.6f, { 0.85f, 0.92f, 1.0f, b });
    }

    // Idle-world ambient background objects
    for (const auto& obj : m_menuAmbient)
    {
        if (obj.kind == 0 && m_texAsteroid != -1)
        {
            int tw = Texture_GetWidth(m_texAsteroid);
            int th = Texture_GetHeight(m_texAsteroid);
            Sprite_Draw(m_texAsteroid, obj.position.x + camX - (float)tw * obj.scale * 0.5f, obj.position.y + camY - (float)th * obj.scale * 0.5f,
                (float)tw * obj.scale, (float)th * obj.scale, 0, 0, tw, th, obj.rotation, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, obj.alpha });
        }
        else if (obj.kind == 1 && m_texVida != -1)
        {
            int tw = Texture_GetWidth(m_texVida);
            int th = Texture_GetHeight(m_texVida);
            Sprite_Draw(m_texVida, obj.position.x + camX - (float)tw * obj.scale * 0.5f, obj.position.y + camY - (float)th * obj.scale * 0.5f,
                (float)tw * obj.scale, (float)th * obj.scale, 0, 0, tw, th, obj.rotation, { 1.0f, 1.0f }, { 0.6f, 0.9f, 1.0f, obj.alpha });
        }
        else if (obj.kind == 2 && m_texFinalBoss != -1)
        {
            int tw = Texture_GetWidth(m_texFinalBoss);
            int th = Texture_GetHeight(m_texFinalBoss);
            Sprite_Draw(m_texFinalBoss, obj.position.x + camX - (float)tw * obj.scale * 0.5f, obj.position.y + camY - (float)th * obj.scale * 0.5f,
                (float)tw * obj.scale, (float)th * obj.scale, 0, 0, tw, th, 0.0f, { 1.0f, 1.0f }, { 0.05f, 0.05f, 0.10f, obj.alpha });
        }
    }

    // Ship scanner pulse ring
    if (m_menuScannerPulseTimer > 0.0f)
    {
        float t = 1.0f - (m_menuScannerPulseTimer / 0.9f);
        float rad = 20.0f + t * 130.0f;
        float alpha = (1.0f - t) * 0.55f;
        Sprite_DrawCircle(m_playerPos.x + camX, m_playerPos.y + camY, rad, 1.6f, { 0.45f, 0.90f, 1.0f, alpha }, 40);
    }

    bool interactiveMenu = (m_menuPhase == MainMenuPhase::Menu);
    DrawAmbientShip(m_playerPos.x + camX, m_playerPos.y + camY, interactiveMenu);

    // Title / Logo -- centered from actual rendered text width, not a hand-tuned offset
    float screenCenterX = (float)SCREEN_WIDTH * 0.5f;
    float titleY = (float)SCREEN_HEIGHT * 0.16f;
    float titlePulse = sinf(m_totalTime * 1.4f) * 0.08f + 0.92f;
    const char* titleText = "YOKAI SHIP";
    const char* subtitleText = "DEEP SPACE EXPEDITION";
    DrawMatrixString(CenteredTextX(titleText, 5.2f, screenCenterX) + camX, titleY + camY, titleText, 5.2f, m_texLaser,
        { 0.55f * titlePulse, 0.90f * titlePulse, 1.0f * titlePulse, 1.0f });
    DrawMatrixString(CenteredTextX(subtitleText, 1.6f, screenCenterX) + camX, titleY + 46.0f + camY, subtitleText, 1.6f, m_texLaser,
        { 0.55f, 0.70f, 0.85f, 0.85f });

    if (m_menuPhase == MainMenuPhase::Boot)
    {
        float blink = (sinf(m_menuBootPulseTimer * 3.2f) > 0.0f) ? 1.0f : 0.35f;
        const char* bootText = "> INITIALIZE SYSTEM";
        const char* pressText = "PRESS ANY KEY";
        DrawMatrixString(CenteredTextX(bootText, 2.0f, screenCenterX) + camX, (float)SCREEN_HEIGHT * 0.80f + camY, bootText, 2.0f, m_texLaser,
            { 0.45f, 1.0f, 0.85f, 1.0f });
        DrawMatrixString(CenteredTextX(pressText, 1.5f, screenCenterX) + camX, (float)SCREEN_HEIGHT * 0.80f + 26.0f + camY, pressText, 1.5f, m_texLaser,
            { 0.85f, 0.90f, 0.95f, blink });
    }
    else if (m_menuPhase == MainMenuPhase::BootGlitch)
    {
        float t = m_menuPhaseTimer / 0.40f;
        float flashAlpha = std::max(0.0f, 0.65f - t * 0.65f);
        Sprite_DrawRect(0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, { 0.75f, 0.95f, 1.0f, flashAlpha * 0.25f });
        for (int i = 0; i < 10; ++i)
        {
            float ly = RandomFloat(0.0f, (float)SCREEN_HEIGHT);
            float lw = RandomFloat(120.0f, (float)SCREEN_WIDTH);
            float lx = RandomFloat(0.0f, (float)SCREEN_WIDTH - lw);
            Sprite_DrawRect(lx, ly, lw, RandomFloat(1.0f, 3.0f), { 0.6f, 0.95f, 1.0f, RandomFloat(0.25f, 0.6f) });
        }
        const char* onlineText = "SYSTEM ONLINE";
        DrawMatrixString(CenteredTextX(onlineText, 2.0f, screenCenterX), (float)SCREEN_HEIGHT * 0.80f, onlineText, 2.0f, m_texLaser,
            { 0.55f, 1.0f, 0.75f, 1.0f });
    }
    else if (m_menuPhase == MainMenuPhase::Menu)
    {
        DrawMainMenuOptions(camX, camY);
    }

    if (m_menuPlaceholderTimer > 0.0f)
    {
        float alpha = std::min(1.0f, m_menuPlaceholderTimer);
        float boxW = 560.0f;
        float boxH = 50.0f;
        float boxX = (float)SCREEN_WIDTH * 0.5f - boxW * 0.5f;
        float boxY = (float)SCREEN_HEIGHT * 0.88f;
        Sprite_DrawRect(boxX, boxY, boxW, boxH, { 0.08f, 0.08f, 0.14f, 0.85f * alpha });
        Sprite_DrawRectBorder(boxX, boxY, boxW, boxH, 1.5f, { 0.85f, 0.35f, 0.35f, alpha });
        DrawMatrixString(boxX + 20.0f, boxY + 17.0f, m_menuPlaceholderMessage.c_str(), 1.4f, m_texLaser, { 1.0f, 0.75f, 0.55f, alpha });
    }
}

// ============================================================================
// FINAL BOSS (BOSS 4 - KITSUNE YOKAI) HELPER IMPLEMENTATIONS
// ============================================================================

void Game::SpawnFinalBossBlades(int count, bool isPrison)
{
    // Enforce max embedded blades limit: remove oldest blades first
    while ((int)m_bossBlades.size() + count > EnemyConfig::BossFinal.maxEmbeddedBlades && !m_bossBlades.empty())
    {
        m_bossBlades.erase(m_bossBlades.begin());
    }

    if (isPrison)
    {
        // BLADE PRISON: Spawns 4 vertical blades surrounding player's current X region
        float offsets[4] = { -150.0f, -50.0f, 50.0f, 150.0f };
        int spawnN = std::min(4, count);
        for (int i = 0; i < spawnN; ++i)
        {
            BossBlade blade;
            float spawnX = std::clamp(m_playerPos.x + offsets[i], 120.0f, (float)SCREEN_WIDTH - 120.0f);
            blade.position = DirectX::XMFLOAT2(spawnX, -80.0f);
            blade.targetY = std::clamp(m_playerPos.y + RandomFloat(-30.0f, 30.0f), 280.0f, (float)SCREEN_HEIGHT - 100.0f);
            blade.warningTimer = EnemyConfig::BossFinal.bladeWarningDuration;
            blade.warningDuration = EnemyConfig::BossFinal.bladeWarningDuration;
            blade.pulseTimer = 1.0f + (float)i * 0.25f;
            blade.pulseInterval = EnemyConfig::BossFinal.bladePulseInterval;
            blade.lifetime = 8.0f;
            blade.state = BladeState::Warning;
            blade.scale = EnemyConfig::BossFinal.bladeScale;
            blade.radius = EnemyConfig::BossFinal.bladeRadius;
            blade.rotation = 1.5707963f; // 90 deg pointing downwards
            blade.isPrisonBlade = true;
            m_bossBlades.push_back(blade);
        }
    }
    else
    {
        // Normal falling blades
        for (int i = 0; i < count; ++i)
        {
            BossBlade blade;
            float spawnX = RandomFloat(160.0f, (float)SCREEN_WIDTH - 160.0f);
            blade.position = DirectX::XMFLOAT2(spawnX, -80.0f);
            blade.targetY = RandomFloat(300.0f, (float)SCREEN_HEIGHT - 120.0f);
            blade.warningTimer = EnemyConfig::BossFinal.bladeWarningDuration;
            blade.warningDuration = EnemyConfig::BossFinal.bladeWarningDuration;
            blade.pulseTimer = 1.0f + (float)i * 0.35f;
            blade.pulseInterval = EnemyConfig::BossFinal.bladePulseInterval;
            blade.lifetime = EnemyConfig::BossFinal.bladeLifetime;
            blade.state = BladeState::Warning;
            blade.scale = EnemyConfig::BossFinal.bladeScale;
            blade.radius = EnemyConfig::BossFinal.bladeRadius;
            blade.rotation = 1.5707963f;
            blade.isPrisonBlade = false;
            m_bossBlades.push_back(blade);
        }
    }
}

void Game::TriggerBladeCommandAimedShot()
{
    for (auto& blade : m_bossBlades)
    {
        if (blade.state != BladeState::Embedded) continue;

        float bdx = m_playerPos.x - blade.position.x;
        float bdy = m_playerPos.y - blade.position.y;
        float bdist = sqrtf(bdx * bdx + bdy * bdy);
        if (bdist < 0.001f) bdist = 1.0f;

        EnemyProjectile bp;
        bp.position = blade.position;
        bp.velocity = { (bdx / bdist) * 175.0f, (bdy / bdist) * 175.0f };
        bp.radius = 12.0f;
        bp.damage = 1;
        bp.lifetime = 4.5f;
        bp.isBossSpiral = true;
        m_enemyProjectiles.push_back(bp);

        // Core flash ring
        TriggerShockwave(blade.position, 35.0f, 0.0f);
    }
}

void Game::ClampFinalBossHpFloor(Asteroid& ast)
{
    // Enforce the per-phase HP floor on every damage path (lasers, pierce, chain, turrets,
    // dash impacts, ...) so high player DPS can never skip a phase transition.
    if (!(ast.isBoss && ast.bossType == 4) || ast.hp <= 0.0f) return;

    float maxHp = ast.maxHp;
    float hpFloor = 0.0f;
    if (ast.finalPhase == FinalBossPhase::Phase1)
        hpFloor = maxHp * EnemyConfig::BossFinal.phase1HpFloor;
    else if (ast.finalPhase == FinalBossPhase::Phase2)
        hpFloor = maxHp * EnemyConfig::BossFinal.phase2HpFloor;
    else if (ast.finalPhase == FinalBossPhase::Phase3)
        hpFloor = maxHp * EnemyConfig::BossFinal.phase3HpFloor;

    if (hpFloor > 0.0f && ast.hp < hpFloor)
        ast.hp = hpFloor;
}

void Game::SpawnGhostOrbs(const DirectX::XMFLOAT2& bossPos)
{
    // Clear old ghost orbs if any
    for (auto it = m_bossOrbs.begin(); it != m_bossOrbs.end(); )
    {
        if (it->isGhost) it = m_bossOrbs.erase(it);
        else ++it;
    }

    float angles[4] = { 0.0f, 1.5707963f, 3.14159265f, 4.712389f };
    for (int k = 0; k < 4; ++k)
    {
        BossOrb gOrb;
        gOrb.angle = angles[k];
        gOrb.orbitRadius = EnemyConfig::BossFinal.orbOrbitRadius;
        gOrb.position = { bossPos.x + cosf(gOrb.angle) * gOrb.orbitRadius, bossPos.y + sinf(gOrb.angle) * gOrb.orbitRadius };
        gOrb.hp = 9999.0f;
        gOrb.maxHp = 9999.0f;
        gOrb.radius = EnemyConfig::BossFinal.orbRadius;
        gOrb.scale = EnemyConfig::BossFinal.orbScale;
        gOrb.fireInterval = 9999.0f; // Idle by default; only fires when a combo step arms it
        gOrb.fireTimer = 9999.0f;
        gOrb.attackPattern = 3; // Outward (spiral) is the default pattern when armed
        gOrb.flashTimer = 0.0f;
        gOrb.alive = true;
        gOrb.isGhost = true;
        gOrb.isPermanent = false;
        gOrb.ghostLifetime = 12.0f; // Long but finite for Phase 3
        m_bossOrbs.push_back(gOrb);
    }
}

void Game::SpawnPermanentGhostOrbs(const DirectX::XMFLOAT2& bossPos)
{
    // Clear old ghost orbs if any
    for (auto it = m_bossOrbs.begin(); it != m_bossOrbs.end(); )
    {
        if (it->isGhost) it = m_bossOrbs.erase(it);
        else ++it;
    }

    float angles[4] = { 0.0f, 1.5707963f, 3.14159265f, 4.712389f };
    for (int k = 0; k < 4; ++k)
    {
        BossOrb gOrb;
        gOrb.angle = angles[k];
        gOrb.orbitRadius = EnemyConfig::BossFinal.orbOrbitRadius;
        gOrb.position = { bossPos.x + cosf(gOrb.angle) * gOrb.orbitRadius, bossPos.y + sinf(gOrb.angle) * gOrb.orbitRadius };
        gOrb.hp = 9999.0f;
        gOrb.maxHp = 9999.0f;
        gOrb.radius = EnemyConfig::BossFinal.orbRadius;
        gOrb.scale = EnemyConfig::BossFinal.orbScale;
        gOrb.fireInterval = 9999.0f; // Idle by default; only fires when a combo step arms it
        gOrb.fireTimer = 9999.0f;
        gOrb.attackPattern = 3; // Outward (spiral) is the default pattern when armed
        gOrb.flashTimer = 0.0f;
        gOrb.alive = true;
        gOrb.isGhost = true;
        gOrb.isPermanent = true; // Never expires in Final Phase
        gOrb.ghostLifetime = 9999.0f;
        m_bossOrbs.push_back(gOrb);
    }
}

void Game::FireGhostSpiral()
{
    // Each ghost orb fires one projectile outward immediately, then is armed to keep firing
    // outward every ghostSpiralFireInterval while it continues orbiting -- the spiral shape
    // emerges purely from the rotating emitter positions, no spiral projectile math needed.
    for (auto& orb : m_bossOrbs)
    {
        if (!orb.isGhost || !orb.alive) continue;

        float outAngle = orb.angle; // Fire outward from orbit center
        EnemyProjectile bp;
        bp.position = orb.position;
        bp.velocity = { cosf(outAngle) * EnemyConfig::BossFinal.ghostOrbBulletSpeed, sinf(outAngle) * EnemyConfig::BossFinal.ghostOrbBulletSpeed };
        bp.radius = 11.0f;
        bp.damage = EnemyConfig::BossFinal.orbBulletDamage;
        bp.lifetime = 4.5f;
        bp.isBossSpiral = true;
        m_enemyProjectiles.push_back(bp);

        orb.attackPattern = 3; // Outward
        orb.fireInterval = EnemyConfig::BossFinal.ghostSpiralFireInterval;
        orb.fireTimer = orb.fireInterval;
    }
}

void Game::HaltGhostOrbFiring()
{
    // Disarm all ghost orbs so they stop auto-firing between combo steps -- firing is only
    // ever driven explicitly by the Fire* pattern functions and this idle state.
    for (auto& orb : m_bossOrbs)
    {
        if (!orb.isGhost) continue;
        orb.fireInterval = 9999.0f;
        orb.fireTimer = 9999.0f;
    }
}

void Game::FireGhostAimedSequence()
{
    // Ghost Orbs fire one after another (staggered via fireTimer).
    // Each orb fires a 3-way spread toward the player's position at the moment of firing.
    // This is called to SET UP the sequence — actual firing happens via the orb update loop's fireTimer.
    int idx = 0;
    for (auto& orb : m_bossOrbs)
    {
        if (!orb.isGhost || !orb.alive) continue;
        orb.attackPattern = 1; // 3-way spread aimed at player
        orb.fireTimer = EnemyConfig::BossFinal.ghostAimedSequenceDelay * (float)idx;
        orb.fireInterval = 99.0f; // Fire once then wait for next pattern
        ++idx;
    }
}

void Game::FireGhostCrossFire()
{
    // Stop four Ghost Orbs at cardinal positions and capture player position for telegraph.
    // The actual firing happens in UpdateFinalBoss after the warning timer expires.
    // Find boss position
    DirectX::XMFLOAT2 bossPos{ (float)SCREEN_WIDTH * 0.5f, EnemyConfig::BossFinal.hoverY };
    for (const auto& ast : m_asteroids)
    {
        if (ast.isBoss && ast.bossType == 4)
        {
            bossPos = ast.position;
            // Set crossfire state on the boss
            break;
        }
    }

    // Snap ghost orbs to cardinal positions
    float cardinalAngles[4] = { 0.0f, 1.5707963f, 3.14159265f, 4.712389f };
    int idx = 0;
    for (auto& orb : m_bossOrbs)
    {
        if (!orb.isGhost || !orb.alive) continue;
        if (idx < 4)
        {
            orb.angle = cardinalAngles[idx];
            orb.position = { bossPos.x + cosf(orb.angle) * orb.orbitRadius, bossPos.y + sinf(orb.angle) * orb.orbitRadius };
        }
        orb.fireTimer = 99.0f; // Don't fire during telegraph
        ++idx;
    }
}

void Game::UpdateFinalBoss(Asteroid& boss, float deltaTime)
{
    float hpPct = std::clamp(boss.hp / boss.maxHp, 0.0f, 1.0f);

    // 1. Entrance / Hover Movement
    if (boss.bossPhase == BossPhase::Enter)
    {
        boss.position.y += boss.velocity.y * deltaTime;
        if (boss.position.y >= EnemyConfig::BossFinal.hoverY)
        {
            boss.position.y = EnemyConfig::BossFinal.hoverY;
            boss.bossPhase = BossPhase::Patrol;
            boss.bossTargetPos = DirectX::XMFLOAT2((float)SCREEN_WIDTH * 0.75f, EnemyConfig::BossFinal.hoverY);
        }
    }
    else
    {
        // Smooth horizontal patrol in upper screen area with gentle vertical floating bob
        float targetX = boss.bossTargetPos.x;
        float dx = targetX - boss.position.x;
        float moveDir = (dx > 0.0f) ? 1.0f : -1.0f;
        boss.position.x += moveDir * EnemyConfig::BossFinal.moveSpeed * deltaTime;
        boss.position.y = EnemyConfig::BossFinal.hoverY + sinf(m_totalTime * 2.5f) * 8.0f;

        if (fabsf(dx) < 20.0f)
        {
            // Pick opposite target X
            float nextX = (boss.position.x < (float)SCREEN_WIDTH * 0.5f)
                ? RandomFloat((float)SCREEN_WIDTH * 0.60f, (float)SCREEN_WIDTH - 180.0f)
                : RandomFloat(180.0f, (float)SCREEN_WIDTH * 0.40f);
            boss.bossTargetPos.x = nextX;
        }
    }

    // ==============================================================
    // 2. Phase Transitions by HP percentage (with damage gating)
    // ==============================================================
    if (!boss.invulnerable)
    {
        if (boss.finalPhase == FinalBossPhase::Phase1 && hpPct <= EnemyConfig::BossFinal.phase1HpFloor)
        {
            // Transition Phase 1 -> Phase 2
            boss.finalPhase = FinalBossPhase::Transition12;
            boss.transitionTimer = EnemyConfig::BossFinal.transitionDuration;
            boss.invulnerable = true;
            boss.hp = boss.maxHp * EnemyConfig::BossFinal.phase1HpFloor; // Clamp to floor
            boss.finalAttackTimer = 0.0f;
            boss.flashTimer = 0.3f;
            TriggerCameraShake(0.40f, 6.0f);
            TriggerShockwave(boss.position, 200.0f, 0.0f);
        }
        else if (boss.finalPhase == FinalBossPhase::Phase2 && hpPct <= EnemyConfig::BossFinal.phase2HpFloor)
        {
            // Transition Phase 2 -> Phase 3
            boss.finalPhase = FinalBossPhase::Transition23;
            boss.transitionTimer = EnemyConfig::BossFinal.transitionDuration;
            boss.invulnerable = true;
            boss.hp = boss.maxHp * EnemyConfig::BossFinal.phase2HpFloor;
            boss.finalAttackTimer = 0.0f;
            boss.finalBladeTimer = 0.0f;
            boss.flashTimer = 0.3f;
            TriggerCameraShake(0.45f, 7.0f);
            TriggerShockwave(boss.position, 240.0f, 0.0f);
        }
        else if (boss.finalPhase == FinalBossPhase::Phase3 && hpPct <= EnemyConfig::BossFinal.phase3HpFloor)
        {
            // Transition Phase 3 -> Final
            boss.finalPhase = FinalBossPhase::Transition3F;
            boss.transitionTimer = EnemyConfig::BossFinal.transitionDuration;
            boss.invulnerable = true;
            boss.hp = boss.maxHp * EnemyConfig::BossFinal.phase3HpFloor;
            boss.finalAttackTimer = 0.0f;
            boss.phase3ComboTimer = 0.0f;
            boss.flashTimer = 0.3f;
            TriggerCameraShake(0.50f, 8.0f);
            TriggerShockwave(boss.position, 280.0f, 0.0f);
        }
    }

    // ==============================================================
    // 3. Handle Transition States (invulnerable, timer-based)
    // ==============================================================
    if (boss.finalPhase == FinalBossPhase::Transition12)
    {
        boss.transitionTimer -= deltaTime;
        if (boss.transitionTimer <= 0.0f)
        {
            boss.finalPhase = FinalBossPhase::Phase2;
            boss.invulnerable = false;
            boss.finalAttackTimer = 1.0f;
            boss.finalBladeTimer = 0.8f;
            boss.finalAttackStep = 0;
            TriggerCameraShake(0.35f, 5.5f);
        }
        return; // No attacks during transition
    }
    else if (boss.finalPhase == FinalBossPhase::Transition23)
    {
        boss.transitionTimer -= deltaTime;
        if (boss.transitionTimer <= 0.0f)
        {
            boss.finalPhase = FinalBossPhase::Phase3;
            boss.invulnerable = false;
            boss.phase3ComboStep = 0;
            boss.phase3ComboTimer = 0.5f; // Brief initial delay
            boss.ghostPattern = GhostOrbPattern::Spiral;
            boss.ghostPatternTimer = 0.0f;
            boss.ghostPatternStep = 0;
            SpawnGhostOrbs(boss.position);
            TriggerCameraShake(0.40f, 6.0f);
        }
        return;
    }
    else if (boss.finalPhase == FinalBossPhase::Transition3F)
    {
        boss.transitionTimer -= deltaTime;
        if (boss.transitionTimer <= 0.0f)
        {
            boss.finalPhase = FinalBossPhase::Final;
            boss.invulnerable = false;
            boss.finalComboStep = 0;
            boss.finalComboTimer = 0.5f;
            boss.bladePrisonTimer = EnemyConfig::BossFinal.bladePrisonCooldown;
            boss.ghostPattern = GhostOrbPattern::Spiral;
            boss.ghostPatternTimer = 0.0f;
            boss.ghostPatternStep = 0;
            SpawnPermanentGhostOrbs(boss.position); // Permanent ghost orbs!
            TriggerCameraShake(0.50f, 7.5f);
        }
        return;
    }

    // ==============================================================
    // 4. Phase Attack Execution
    // ==============================================================
    if (boss.finalPhase == FinalBossPhase::OrbShield)
    {
        // Boss is invulnerable. Destructible shield orbs handle firing and player must destroy all 4 to proceed!
        int aliveDestructibleCount = 0;
        for (const auto& orb : m_bossOrbs)
        {
            if (orb.alive && !orb.isGhost) aliveDestructibleCount++;
        }

        if (aliveDestructibleCount == 0)
        {
            boss.invulnerable = false;
            boss.finalPhase = FinalBossPhase::Phase1;
            boss.finalAttackTimer = 1.0f;
            TriggerCameraShake(0.50f, 8.0f);
            TriggerShockwave(boss.position, 280.0f, 0.0f);
        }
    }
    else if (boss.finalPhase == FinalBossPhase::Phase1)
    {
        // Phase 1: Alternate Attack A (5-way fan) & Attack B (8-dir radial burst)
        boss.finalAttackTimer -= deltaTime;
        if (boss.finalAttackTimer <= 0.0f)
        {
            boss.finalAttackTimer = EnemyConfig::BossFinal.phase1AttackInterval;
            boss.finalAttackStep = (boss.finalAttackStep + 1) % 2;

            if (boss.finalAttackStep == 0)
            {
                // Attack A: 5-way projectile fan toward player
                float bdx = m_playerPos.x - boss.position.x;
                float bdy = m_playerPos.y - boss.position.y;
                float baseAngle = atan2f(bdy, bdx);
                float spread = 0.18f; // ~10 degrees apart
                for (int p = -2; p <= 2; ++p)
                {
                    float angle = baseAngle + (float)p * spread;
                    EnemyProjectile bp;
                    bp.position = boss.position;
                    bp.velocity = { cosf(angle) * EnemyConfig::BossFinal.phase1BulletSpeed, sinf(angle) * EnemyConfig::BossFinal.phase1BulletSpeed };
                    bp.radius = 12.0f;
                    bp.damage = 1;
                    bp.lifetime = 4.8f;
                    bp.isBossSpiral = true;
                    m_enemyProjectiles.push_back(bp);
                }
            }
            else
            {
                // Attack B: 8-direction radial projectile burst
                float startRot = m_totalTime * 1.5f;
                for (int p = 0; p < 8; ++p)
                {
                    float angle = startRot + (float)p * (2.0f * PI / 8.0f);
                    EnemyProjectile bp;
                    bp.position = boss.position;
                    bp.velocity = { cosf(angle) * EnemyConfig::BossFinal.phase1BulletSpeed, sinf(angle) * EnemyConfig::BossFinal.phase1BulletSpeed };
                    bp.radius = 12.0f;
                    bp.damage = 1;
                    bp.lifetime = 4.8f;
                    bp.isBossSpiral = true;
                    m_enemyProjectiles.push_back(bp);
                }
            }
            if (m_soundShoot != -1) PlayAudio(m_soundShoot);
        }
    }
    else if (boss.finalPhase == FinalBossPhase::Phase2)
    {
        // Phase 2: Falling blades + Boss attacks + Blade command triggers
        boss.finalAttackTimer -= deltaTime;
        if (boss.finalAttackTimer <= 0.0f)
        {
            boss.finalAttackTimer = EnemyConfig::BossFinal.phase1AttackInterval;
            // 5-way fan
            float bdx = m_playerPos.x - boss.position.x;
            float bdy = m_playerPos.y - boss.position.y;
            float baseAngle = atan2f(bdy, bdx);
            for (int p = -2; p <= 2; ++p)
            {
                float angle = baseAngle + (float)p * 0.16f;
                EnemyProjectile bp;
                bp.position = boss.position;
                bp.velocity = { cosf(angle) * EnemyConfig::BossFinal.phase1BulletSpeed, sinf(angle) * EnemyConfig::BossFinal.phase1BulletSpeed };
                bp.radius = 12.0f;
                bp.damage = 1;
                bp.lifetime = 4.8f;
                bp.isBossSpiral = true;
                m_enemyProjectiles.push_back(bp);
            }
            if (m_soundShoot != -1) PlayAudio(m_soundShoot);
        }

        // Falling Blade Spawns
        boss.finalBladeTimer -= deltaTime;
        if (boss.finalBladeTimer <= 0.0f)
        {
            int bladeCount = (hpPct < 0.52f) ? 3 : 2;
            boss.finalBladeTimer = (hpPct < 0.52f) ? 4.8f : 5.8f;
            SpawnFinalBossBlades(bladeCount);
            TriggerBladeCommandAimedShot();
        }
    }
    else if (boss.finalPhase == FinalBossPhase::Phase3)
    {
        // ==========================================================
        // Phase 3: Ghost Orb Pattern Combo Sequence
        // Combo: Ghost Spiral -> recovery -> drop 2 blades ->
        //        Ghost Aimed Sequence -> embedded blade pulse ->
        //        recovery -> Ghost Cross Fire -> repeat
        // ==========================================================

        // Handle CrossFire warning/firing
        if (boss.crossFireWarningActive)
        {
            boss.crossFireWarningTimer -= deltaTime;
            if (boss.crossFireWarningTimer <= 0.0f)
            {
                // Fire all ghost orbs at the captured position!
                boss.crossFireWarningActive = false;
                for (auto& orb : m_bossOrbs)
                {
                    if (!orb.isGhost || !orb.alive) continue;
                    float cdx = boss.crossFireTarget.x - orb.position.x;
                    float cdy = boss.crossFireTarget.y - orb.position.y;
                    float cdist = sqrtf(cdx * cdx + cdy * cdy);
                    if (cdist < 0.001f) cdist = 1.0f;

                    // Each orb fires 2 projectiles in a tight spread at the captured position
                    for (int s = -1; s <= 1; s += 2)
                    {
                        float cfAngle = atan2f(cdy, cdx) + (float)s * 0.08f;
                        EnemyProjectile bp;
                        bp.position = orb.position;
                        bp.velocity = { cosf(cfAngle) * EnemyConfig::BossFinal.ghostCrossFireBulletSpeed, sinf(cfAngle) * EnemyConfig::BossFinal.ghostCrossFireBulletSpeed };
                        bp.radius = 12.0f;
                        bp.damage = 1;
                        bp.lifetime = 4.5f;
                        bp.isBossSpiral = true;
                        m_enemyProjectiles.push_back(bp);
                    }
                    orb.flashTimer = 0.15f;
                }
                TriggerCameraShake(0.25f, 5.0f);
                if (m_soundShoot != -1) PlayAudio(m_soundShoot);

                // Stay idle -- the next combo step (Ghost Spiral) explicitly arms firing again
                HaltGhostOrbFiring();
            }
            // During crossfire warning, don't advance combo
            return;
        }

        // Ensure ghost orbs are alive
        bool hasGhosts = false;
        for (const auto& ob : m_bossOrbs) { if (ob.isGhost) { hasGhosts = true; break; } }
        if (!hasGhosts) SpawnGhostOrbs(boss.position);

        boss.phase3ComboTimer -= deltaTime;
        if (boss.phase3ComboTimer <= 0.0f)
        {
            // Combo sequence steps
            // 0: Ghost Spiral
            // 1: Recovery
            // 2: Drop 2 blades
            // 3: Ghost Aimed Sequence
            // 4: Embedded blade pulse (command shot)
            // 5: Recovery
            // 6: Ghost Cross Fire (telegraph)
            // 7: (crossfire fires after warning — handled above)
            switch (boss.phase3ComboStep)
            {
            case 0: // Ghost Spiral
                FireGhostSpiral();
                boss.phase3ComboTimer = EnemyConfig::BossFinal.phase3ComboInterval;
                if (m_soundShoot != -1) PlayAudio(m_soundShoot);
                break;
            case 1: // Recovery
                HaltGhostOrbFiring(); // Stop the spiral's periodic outward fire
                boss.phase3ComboTimer = EnemyConfig::BossFinal.phase3RecoveryTime;
                break;
            case 2: // Drop 2 blades
                SpawnFinalBossBlades(2);
                boss.phase3ComboTimer = EnemyConfig::BossFinal.phase3ComboInterval;
                break;
            case 3: // Ghost Aimed Sequence
                FireGhostAimedSequence();
                boss.phase3ComboTimer = EnemyConfig::BossFinal.phase3ComboInterval + 0.4f; // Extra time for staggered fire
                if (m_soundShoot != -1) PlayAudio(m_soundShoot);
                break;
            case 4: // Embedded blade pulse
                TriggerBladeCommandAimedShot();
                boss.phase3ComboTimer = EnemyConfig::BossFinal.phase3ComboInterval;
                break;
            case 5: // Recovery
                boss.phase3ComboTimer = EnemyConfig::BossFinal.phase3RecoveryTime;
                HaltGhostOrbFiring(); // Ensure the Aimed Sequence's staggered shots are done
                break;
            case 6: // Ghost Cross Fire telegraph
                FireGhostCrossFire();
                boss.crossFireTarget = m_playerPos; // Capture player position NOW
                boss.crossFireWarningActive = true;
                boss.crossFireWarningTimer = EnemyConfig::BossFinal.ghostCrossFireWarning;
                boss.phase3ComboTimer = 0.1f; // Tiny delay then warning takes over
                break;
            default:
                boss.phase3ComboStep = -1; // Will increment to 0
                boss.phase3ComboTimer = EnemyConfig::BossFinal.phase3RecoveryTime;
                break;
            }
            boss.phase3ComboStep++;
            if (boss.phase3ComboStep > 7) boss.phase3ComboStep = 0;
        }

        // Also fire boss's own 5-way fan periodically during Phase 3
        boss.finalAttackTimer -= deltaTime;
        if (boss.finalAttackTimer <= 0.0f)
        {
            boss.finalAttackTimer = 3.2f; // Slower boss attacks — ghost orbs provide main pressure
            float bdx = m_playerPos.x - boss.position.x;
            float bdy = m_playerPos.y - boss.position.y;
            float baseAngle = atan2f(bdy, bdx);
            for (int p = -2; p <= 2; ++p)
            {
                float angle = baseAngle + (float)p * 0.16f;
                EnemyProjectile bp;
                bp.position = boss.position;
                bp.velocity = { cosf(angle) * EnemyConfig::BossFinal.phase1BulletSpeed, sinf(angle) * EnemyConfig::BossFinal.phase1BulletSpeed };
                bp.radius = 12.0f;
                bp.damage = 1;
                bp.lifetime = 4.8f;
                bp.isBossSpiral = true;
                m_enemyProjectiles.push_back(bp);
            }
            if (m_soundShoot != -1) PlayAudio(m_soundShoot);
        }
    }
    else if (boss.finalPhase == FinalBossPhase::Final)
    {
        // ==========================================================
        // Final Phase: Permanent Ghost Orbs + All Attacks Combined
        // Combo: Ghost Spiral -> Falling Blades -> Boss Radial Burst ->
        //        Embedded Blade Pulse -> Ghost Cross Fire -> Boss Fan Attack -> repeat
        // Recovery times ~25% shorter than Phase 3
        // ==========================================================

        // Handle CrossFire warning/firing (same logic as Phase 3)
        if (boss.crossFireWarningActive)
        {
            boss.crossFireWarningTimer -= deltaTime;
            if (boss.crossFireWarningTimer <= 0.0f)
            {
                boss.crossFireWarningActive = false;
                for (auto& orb : m_bossOrbs)
                {
                    if (!orb.isGhost || !orb.alive) continue;
                    float cdx = boss.crossFireTarget.x - orb.position.x;
                    float cdy = boss.crossFireTarget.y - orb.position.y;
                    float cdist = sqrtf(cdx * cdx + cdy * cdy);
                    if (cdist < 0.001f) cdist = 1.0f;

                    for (int s = -1; s <= 1; s += 2)
                    {
                        float cfAngle = atan2f(cdy, cdx) + (float)s * 0.08f;
                        EnemyProjectile bp;
                        bp.position = orb.position;
                        bp.velocity = { cosf(cfAngle) * EnemyConfig::BossFinal.ghostCrossFireBulletSpeed, sinf(cfAngle) * EnemyConfig::BossFinal.ghostCrossFireBulletSpeed };
                        bp.radius = 12.0f;
                        bp.damage = 1;
                        bp.lifetime = 4.5f;
                        bp.isBossSpiral = true;
                        m_enemyProjectiles.push_back(bp);
                    }
                    orb.flashTimer = 0.15f;
                }
                TriggerCameraShake(0.25f, 5.0f);
                if (m_soundShoot != -1) PlayAudio(m_soundShoot);
                HaltGhostOrbFiring(); // Stay idle -- next Ghost Spiral combo step arms firing again
            }
            return;
        }

        // Ensure permanent ghost orbs exist
        bool hasGhosts = false;
        for (const auto& ob : m_bossOrbs) { if (ob.isGhost) { hasGhosts = true; break; } }
        if (!hasGhosts) SpawnPermanentGhostOrbs(boss.position);

        boss.finalComboTimer -= deltaTime;
        if (boss.finalComboTimer <= 0.0f)
        {
            // Final Phase combo sequence (6 steps, looping):
            // 0: Ghost Spiral
            // 1: Falling Blades (2 blades)
            // 2: Boss 8-dir Radial Burst
            // 3: Embedded Blade Pulse (command shot)
            // 4: Ghost Cross Fire (telegraph)
            // 5: Boss 5-way Fan Attack
            switch (boss.finalComboStep)
            {
            case 0: // Ghost Spiral
                FireGhostSpiral();
                boss.finalComboTimer = EnemyConfig::BossFinal.finalComboInterval;
                if (m_soundShoot != -1) PlayAudio(m_soundShoot);
                break;
            case 1: // Falling Blades
                HaltGhostOrbFiring(); // Stop the spiral's periodic outward fire
                SpawnFinalBossBlades(2);
                boss.finalComboTimer = EnemyConfig::BossFinal.finalComboInterval;
                break;
            case 2: // Boss 8-dir Radial Burst
            {
                float startRot = m_totalTime * 2.0f;
                for (int p = 0; p < 8; ++p)
                {
                    float angle = startRot + (float)p * (2.0f * PI / 8.0f);
                    EnemyProjectile bp;
                    bp.position = boss.position;
                    bp.velocity = { cosf(angle) * 160.0f, sinf(angle) * 160.0f };
                    bp.radius = 12.0f;
                    bp.damage = 1;
                    bp.lifetime = 4.5f;
                    bp.isBossSpiral = true;
                    m_enemyProjectiles.push_back(bp);
                }
                boss.finalComboTimer = EnemyConfig::BossFinal.finalComboInterval;
                if (m_soundShoot != -1) PlayAudio(m_soundShoot);
                break;
            }
            case 3: // Embedded Blade Pulse
                TriggerBladeCommandAimedShot();
                boss.finalComboTimer = EnemyConfig::BossFinal.finalRecoveryTime;
                break;
            case 4: // Ghost Cross Fire telegraph
                FireGhostCrossFire();
                boss.crossFireTarget = m_playerPos;
                boss.crossFireWarningActive = true;
                boss.crossFireWarningTimer = EnemyConfig::BossFinal.ghostCrossFireWarning;
                boss.finalComboTimer = 0.1f;
                break;
            case 5: // Boss 5-way Fan Attack
            {
                float bdx = m_playerPos.x - boss.position.x;
                float bdy = m_playerPos.y - boss.position.y;
                float baseAngle = atan2f(bdy, bdx);
                for (int p = -2; p <= 2; ++p)
                {
                    float angle = baseAngle + (float)p * 0.16f;
                    EnemyProjectile bp;
                    bp.position = boss.position;
                    bp.velocity = { cosf(angle) * 165.0f, sinf(angle) * 165.0f };
                    bp.radius = 12.0f;
                    bp.damage = 1;
                    bp.lifetime = 4.5f;
                    bp.isBossSpiral = true;
                    m_enemyProjectiles.push_back(bp);
                }
                boss.finalComboTimer = EnemyConfig::BossFinal.finalRecoveryTime;
                if (m_soundShoot != -1) PlayAudio(m_soundShoot);
                break;
            }
            default:
                boss.finalComboStep = -1;
                boss.finalComboTimer = EnemyConfig::BossFinal.finalRecoveryTime;
                break;
            }
            boss.finalComboStep++;
            if (boss.finalComboStep > 6) boss.finalComboStep = 0;
        }

        // Special Attack: BLADE PRISON (periodic)
        boss.bladePrisonTimer -= deltaTime;
        if (boss.bladePrisonTimer <= 0.0f)
        {
            boss.bladePrisonTimer = EnemyConfig::BossFinal.bladePrisonCooldown;
            SpawnFinalBossBlades(4, true); // Blade Prison around player!

            // Boss fires slow wave of projectiles to navigate through
            float bdx = m_playerPos.x - boss.position.x;
            float bdy = m_playerPos.y - boss.position.y;
            float baseAngle = atan2f(bdy, bdx);
            for (int p = -3; p <= 3; ++p)
            {
                float angle = baseAngle + (float)p * 0.14f;
                EnemyProjectile bp;
                bp.position = boss.position;
                bp.velocity = { cosf(angle) * 110.0f, sinf(angle) * 110.0f }; // Slow navigating wave
                bp.radius = 13.0f;
                bp.damage = 1;
                bp.lifetime = 6.0f;
                bp.isBossSpiral = true;
                m_enemyProjectiles.push_back(bp);
            }
            TriggerCameraShake(0.35f, 5.0f);
        }
    }
}


