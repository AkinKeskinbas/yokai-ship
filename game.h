#ifndef GAME_H
#define GAME_H

#include <Windows.h>
#include <DirectXMath.h>
#include <vector>
#include <string>
#include <memory>

// Playable Scenes
enum class GameScene
{
    MainMenu,
    Gameplay,
    UpgradePlaceholder
};

// Gameplay States
enum class RunState
{
    Active,
    ShipEntering,    // Ship flies in from below screen and decelerates before handing control to the player
    PlayerDying,     // Player died: dramatic pause, multi-explosion cascade, camera shake
    EnergyDepleted,  // Energy/Time ran out: dialog popup ("Bugünlük bu kadar yeter enerjimiz bitti kaptan")
    BossDefeated,    // Boss defeated: massive explosion chain, super vacuum collects resources
    RunEnded,        // Summary card is displayed, awaiting player action
    TransitionToUpgrade
};

// Main Menu / Title Screen State Machine
enum class MainMenuPhase
{
    Boot,       // "INITIALIZE SYSTEM" terminal prompt, waiting for any key
    BootGlitch, // Brief scanline/glitch HUD activation animation
    Menu,       // Interactive holographic menu around the ship
    Launching   // START EXPEDITION selected: ship accelerates out of the title scene
};

// A lightweight decorative background object for the title screen's idle world
// (distant asteroid, drifting mineral, or a rare mysterious silhouette). Purely visual --
// no HP/collision, unlike the full Asteroid/Enemy gameplay entities.
struct MenuAmbientObject
{
    DirectX::XMFLOAT2 position{ 0.0f, 0.0f };
    DirectX::XMFLOAT2 velocity{ 0.0f, 0.0f };
    float scale = 1.0f;
    float rotation = 0.0f;
    float rotationSpeed = 0.0f;
    float alpha = 1.0f;
    int kind = 0; // 0: distant asteroid, 1: drifting mineral, 2: mysterious silhouette
};

#include "upgrade_tree.h"
#include "enemy_config.h"
#include "background_shader.h"

enum class TurretSpec
{
    Gun,       // Kinetic rapid bullet shooter, piercing
    Mining,    // Focuses on mining asteroids, boosts yield
    Plasma,    // Fires AoE explosive plasma mortar shells
    Orbital    // Full defense fortress
};

enum class DashType
{
    Standard,
    Ghost,   // Passes through projectiles safely
    Impact,  // Deals kinetic smash damage on collision
    Mining   // Instantly destroys small/medium asteroids on contact
};

// Configurable Player Stats (Upgrade-Ready & Deep Progression)
struct PlayerStats
{
    int maxHealth = 2;               // Starting player health (hearts)
    float moveSpeed = 220.0f;
    float laserDamage = 45.0f;       // Continuous damage per second
    float laserFireInterval = 0.16f; // Audio pulse interval
    float laserRange = 190.0f;       // Base tactical range (upgradable)
    int laserCount = 1;
    float pickupRadius = 120.0f;
    float maxFuel = 150.0f;          // More voyage energy
    float fuelDrainRate = 2.5f;      // Extended run duration (~60s)
    float resourceMultiplier = 1.0f;

    // Weapon mechanics
    bool piercingBeam = false;          // Lazer hedefleri delip geçer
    bool overheatEnabled = false;       // Aynı hedefe vuruldukça DPS katlanır
    bool coreMeltdown = false;          // Kırılan hedefler patlama yaratır
    bool chainLaser = false;            // Lazer 2. yakındaki hedefe seker
    bool prismaticSplit = false;        // Büyük asteroit kırılınca 2x ışın burst
    bool crystalWeakpoints = false;     // Asteroitlerde zayıf nokta oluşur
    float critChance = 0.0f;            // Kritik vuruş şansı
    float critMultiplier = 2.0f;        // Kritik hasar çarpanı

    // Mining & Prospecting
    bool rareScanner = false;           // Nadir asteroitlerin etrafında parıltı
    bool oreVision = false;             // Asteroit içindeki madeni kırılmadan gösterir
    bool deepScan = false;              // Asteroit içindeki kaynak ikonunu gösterir
    bool treasureSignal = false;        // Anomali anahtar ve hazine asteroitleri yönü
    bool chainFracture = false;         // Büyük asteroit kırılınca yakındakileri çatlatır
    float jackpotChance = 0.0f;         // 5x kaynak patlaması şansı (%5-25)
    bool resourceOrbit = true;          // Madenler gemi etrafında süzülüp emilir
    bool instantCollection = false;     // Yakın madenler direkt teleport olur
    float vidaBonus = 0.0f;             // Vida düşme şansı çarpanı
    float disliBonus = 0.0f;            // Dişli düşme şansı çarpanı
    float cpuBonus = 0.0f;              // CPU düşme şansı çarpanı

    // Engine & Mobility
    bool afterburner = false;           // İtiş izi ve hız artışı
    bool momentumDrive = false;         // 3s düz gidişte +20% hız
    bool asteroidSlingshot = false;     // Asteroide yakın geçince hız patlaması
    DashType dashType = DashType::Standard;
    float dashCooldown = 3.5f;
    bool zeroPointReactor = false;      // Madencilik & düşman kesme yakıt doldurur

    // Defense & Turrets
    int maxShield = 0;
    bool shieldBubbleUnlocked = false;
    bool reflectiveShield = false;      // Engellenen mermiler geri yansır
    float shieldRechargeTime = 18.0f;   // Kalkan 1 vuruş emdikten sonra yeniden dolma süresi (s)
    int turretCount = 0;
    float turretRange = 260.0f;
    float turretDamage = 35.0f;
    float turretFireInterval = 0.60f;
    TurretSpec turretSpec = TurretSpec::Gun;

    // Shockwave Pulse system
    bool shockwaveUnlocked = false;
    float shockwaveRadius = 220.0f;
    float shockwaveDamage = 0.0f;       // EMP clears projectiles only; no meaningful enemy/boss damage
    float shockwaveInterval = 16.0f;

    // Cross-Branch Hybrids
    bool laserExcavator = false;        // Asteroitlere +100% lazer hasarı
    bool salvageDrone = false;          // Taretlerin öldürdüğü loot'u oto toplar
    bool velocityCannon = false;        // Hız arttıkça lazer hasarı artar
    bool retaliationMatrix = false;     // Hasar alınca 4s +100% lazer atış hızı
    bool hyperMagnet = false;           // Hızlı giderken magnet yarıçapı +50%

    // Capstone Build (Only 1 active)
    int activeCapstone = 0;             // 1: DeathStar, 2: Midas, 3: Perpetual, 4: OrbitalFortress

    // Active Skills
    ActiveSkillType skill1 = ActiveSkillType::None; // [Q]
    ActiveSkillType skill2 = ActiveSkillType::None; // [E]
    ActiveSkillType skill3 = ActiveSkillType::None; // [SHIFT/SPACE]
};

// Boss Phase State Machine (Boss 1, 2, 3)
enum class BossPhase
{
    Enter,          // Spawning and descending onto the battlefield
    Patrol,         // Moving and occasionally firing standard projectiles (Boss 2)
    AlarmWarning,   // Stopped in place, pulsing warning alarm, charging spiral barrage (Boss 2)
    SpiralAttack,   // Rapid spinning and shooting spiral projectile streams from 4 nozzles (Boss 2)
    Cooldown,       // Brief recovery before resuming Patrol
    GlideTop,       // Boss 3: Glides horizontally across top of screen
    MoveToCenter,   // Boss 3: Moves smoothly to top center
    LaserTrack,     // Boss 3 Phase 1: Aim sight actively tracks player's position
    LaserLock,      // Boss 3 Phase 1: Aim sight locks in place, giving player dodge/escape window
    LaserFire,      // Boss 3 Phase 1: Massive purple death beam fires along locked vector
    CurtainWarning, // Boss 3 Phase 2: Vertical laser curtain telegraph (safe gap highlighted in green)
    CurtainFire,    // Boss 3 Phase 2: Vertical laser curtain active firing
    GridWarning,    // Boss 3 Phase 3: Crosshatch laser matrix telegraph (safe pocket highlighted in green)
    GridFire        // Boss 3 Phase 3: Crosshatch laser matrix active firing
};

// Final Boss (Boss 4 - Kitsune Yokai) Phase State Machine
enum class FinalBossPhase
{
    OrbShield,    // Phase 0: 4 destructible orbiting cores, boss invulnerable
    Phase1,       // Phase 1: 100% -> 70% HP (5-way fan & 8-dir radial bursts)
    Transition12, // Transition: Phase 1 -> Phase 2 (invulnerable, 0.8s)
    Phase2,       // Phase 2: 70% -> 40% HP (falling & embedded blade hazards)
    Transition23, // Transition: Phase 2 -> Phase 3 (invulnerable, 0.8s)
    Phase3,       // Phase 3: 40% -> 15% HP (ghost orb emitters + blade combo sequences)
    Transition3F, // Transition: Phase 3 -> Final (invulnerable, 0.8s)
    Final         // Final Phase: 15% -> 0% HP (permanent ghost orbs + all attacks combined)
};

// Ghost Orb Attack Pattern Sequencer
enum class GhostOrbPattern
{
    Spiral,       // Rotating spiral: orbs orbit and fire outward periodically
    AimedSequence,// Orbs fire 3-way spreads one after another toward player
    CrossFire     // Orbs stop at cardinal positions, telegraph, fire at captured player pos
};

// Final Boss Orbital Core (Phase 0 shield & Phase 3 ghost emitter)
struct BossOrb
{
    DirectX::XMFLOAT2 position{ 0.0f, 0.0f };
    float angle = 0.0f;           // Orbit angle around boss (radians)
    float orbitRadius = 155.0f;   // Distance from boss center
    float hp = 200.0f;
    float maxHp = 200.0f;
    float radius = 24.0f;         // Collision & hit radius
    float scale = 0.09f;          // boss_orb.png scale
    float fireTimer = 0.0f;
    float fireInterval = 2.0f;
    int attackPattern = 0;        // 0: Aimed 1-shot, 1: 3-way spread, 2: 6-way radial burst
    float flashTimer = 0.0f;
    bool alive = true;
    bool isGhost = false;         // Phase 3+ invulnerable ghost emitter
    float ghostLifetime = 0.0f;   // Only used if not permanent
    bool isPermanent = false;     // True in Final Phase: ghost orbs never expire
};

// Final Boss Falling Blade Hazard
enum class BladeState
{
    Warning,      // Red vertical marker at X
    Falling,      // Spawns above screen and drops vertically down
    Embedded,     // Embedded in arena: damages on contact, pulses 4-way, receives boss command shot
    Removed
};

struct BossBlade
{
    DirectX::XMFLOAT2 position{ 0.0f, 0.0f };
    float targetY = 500.0f;
    float warningTimer = 0.65f;
    float warningDuration = 0.65f;
    float pulseTimer = 1.5f;
    float pulseInterval = 1.5f;
    float lifetime = 14.0f;
    BladeState state = BladeState::Warning;
    float scale = 0.12f;          // boss_blade.png scale
    float radius = 24.0f;         // Hitbox radius
    float rotation = 1.5707963f;  // PI / 2 (pointing downwards)
    bool isPrisonBlade = false;   // Special Blade Prison attack
};

// Enemy Projectile (4-way bullets from enemies and boss spirals)
struct EnemyProjectile
{
    DirectX::XMFLOAT2 position;
    DirectX::XMFLOAT2 velocity;
    float radius = 11.0f;
    int damage = 1;
    float lifetime = 4.5f;
    bool isReflected = false;
    bool isBossSpiral = false; // Custom visual styling for boss barrage
};

// Enemy Drone
struct Enemy
{
    DirectX::XMFLOAT2 position;
    DirectX::XMFLOAT2 targetPosition;
    DirectX::XMFLOAT2 velocity;
    float rotation = 0.0f;
    float rotationSpeed = 2.0f;
    float hp = 45.0f;
    float maxHp = 45.0f;
    float radius = 22.0f;
    float scale = 0.12f;
    float shootTimer = 0.0f;
    float shootInterval = 1.6f;
    float changeTargetTimer = 0.0f;
    float flashTimer = 0.0f;
    bool destroyed = false;
};

// Calamity state
struct CalamityState
{
    int level = 1;
    float current = 0.0f;
    float required = 100.0f;
};

// Mining Target Asteroid & Bosses
struct Asteroid
{
    DirectX::XMFLOAT2 position;
    DirectX::XMFLOAT2 velocity;
    float rotation = 0.0f;
    float rotationSpeed = 0.0f;
    float hp = 0.0f;
    float maxHp = 0.0f;
    float radius = 0.0f;
    float scale = 1.0f;
    int resourceAmount = 0;
    int visualRow = 0;
    int visualCol = 0;
    bool isBoss = false;     // True if Calamity Boss (boss1.png / boss2.png / boss3.png / final_boss.png)
    int bossType = 1;        // 1: Boss 1 (Kaya), 2: Boss 2 (Void Destroyer), 3: Boss 3 (Torii Yokai), 4: Final Boss (Kitsune Yokai)
    bool destroyed = false;
    float flashTimer = 0.0f; // For hit flash feedback

    // Boss 2 & 3 AI State Machine & Timers
    BossPhase bossPhase = BossPhase::Enter;
    float bossPhaseTimer = 0.0f;
    float bossShootTimer = 0.0f;
    float bossSpiralFireTimer = 0.0f;
    DirectX::XMFLOAT2 bossTargetPos{ 800.0f, 220.0f };

    // Boss 3 Multi-Phase Purple Sweeping Laser state & Safe Zones
    float bossLaserAngle = 1.5707963f; // PI / 2 (pointing straight down initially)
    float bossLaserSweepFreq = 1.0f;
    float bossLaserDamageTimer = 0.0f;
    int boss3SafeGapIndex = 2;
    int boss3SafeGapIndex2 = 7;
    DirectX::XMFLOAT2 boss3SafePos{ 800.0f, 600.0f };

    // Final Boss (Boss 4) State Machine & Timers
    FinalBossPhase finalPhase = FinalBossPhase::OrbShield;
    bool invulnerable = false;
    float finalBossPhaseTimer = 0.0f;
    float finalAttackTimer = 0.0f;
    float finalBladeTimer = 0.0f;
    float bladePrisonTimer = 0.0f;
    float ghostOrbTimer = 0.0f;
    int finalAttackStep = 0;
    float transitionTimer = 0.0f;       // Phase transition countdown (0.8s)
    GhostOrbPattern ghostPattern = GhostOrbPattern::Spiral; // Current ghost orb attack pattern
    float ghostPatternTimer = 0.0f;     // Timer for ghost pattern sequencing
    int ghostPatternStep = 0;           // Sub-step within ghost pattern
    int phase3ComboStep = 0;            // Phase 3 combo attack sequence index
    float phase3ComboTimer = 0.0f;      // Phase 3 combo sequence timer
    int finalComboStep = 0;             // Final phase combo attack sequence index
    float finalComboTimer = 0.0f;       // Final phase combo sequence timer
    DirectX::XMFLOAT2 crossFireTarget{0.0f, 0.0f}; // Captured player pos for CrossFire telegraph
    float crossFireWarningTimer = 0.0f; // CrossFire telegraph warning countdown
    bool crossFireWarningActive = false;// CrossFire telegraph is showing

    // Weakpoint & Anomalies
    bool hasWeakpoint = false;
    float weakpointAngle = 0.0f;
    float weakpointRadius = 14.0f;
    bool isAnomalousSignal = false; // Key dropping golden asteroid
    DirectX::XMFLOAT4 auraColor{ 0.0f, 0.0f, 0.0f, 0.0f };
};

// Collectible Pickups (Reishi, Vida, Disli, CPU, Key)
enum class PickupType
{
    Reishi,
    Vida,
    Disli,
    Cpu,
    Key
};

struct ResourcePickup
{
    DirectX::XMFLOAT2 position;
    DirectX::XMFLOAT2 velocity;
    float scale = 1.0f;
    int visualRow = 0;
    int visualCol = 0;
    PickupType type = PickupType::Reishi;
    int amount = 1;
    bool collected = false;
    float driftTimer = 0.0f;
    float rotation = 0.0f;
};

// Orbiting Resource Particle (Juicy visual swirl before absorbing into ship)
struct OrbitingResource
{
    DirectX::XMFLOAT2 position{ 0.0f, 0.0f };
    float orbitAngle = 0.0f;
    float orbitSpeed = 7.0f;
    float orbitRadius = 45.0f;
    PickupType type = PickupType::Reishi;
    int amount = 1;
    float lifetime = 0.0f;
    float maxLifetime = 0.75f;
};

// Space Chest / Ancient Cache (chest.png)
struct AncientChest
{
    DirectX::XMFLOAT2 position{ 0.0f, 0.0f };
    DirectX::XMFLOAT2 velocity{ 0.0f, 0.0f };
    float rotation = 0.0f;
    float rotationSpeed = 0.0f;
    float radius = 32.0f;
    float scale = 0.18f;
    bool isOpened = false;
    bool opened = false;
    bool destroyed = false;
    float flashTimer = 0.0f;
};

// Auto laser shot visual representation
struct LaserInstance
{
    DirectX::XMFLOAT2 start;
    DirectX::XMFLOAT2 end;
    float maxLifetime = 0.12f;
    float lifetime = 0.12f;
    DirectX::XMFLOAT4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
};

// Autonomous Defense Station / Turret Platform stationed on the map
struct TurretInstance
{
    DirectX::XMFLOAT2 position{ 0.0f, 0.0f };
    float rotation = 0.0f;
    float fireCooldown = 0.0f;
    float defenseRadius = 320.0f;
    float energyPulse = 0.0f;
    bool isPlayerInZone = false;
    TurretSpec spec = TurretSpec::Gun;
};

// Turret Projectile
struct TurretProjectile
{
    DirectX::XMFLOAT2 position;
    DirectX::XMFLOAT2 velocity;
    float radius = 8.0f;
    float damage = 25.0f;
    float lifetime = 2.0f;
    bool isPlasmaAoE = false;
    float aoeRadius = 0.0f;
    bool isMiningBeam = false;
};

// Shockwave Pulse Instance
struct ShockwaveInstance
{
    DirectX::XMFLOAT2 center{ 0.0f, 0.0f };
    float currentRadius = 0.0f;
    float maxRadius = 240.0f;
    float lifetime = 0.0f;
    float maxLifetime = 0.45f;
    float damage = 40.0f;
};

// Active Skill Runtime State
struct ActiveSkillSlot
{
    ActiveSkillType type = ActiveSkillType::None;
    float cooldownTimer = 0.0f;
    float maxCooldown = 10.0f;
    std::string name;
    std::string keyLabel;
};

// VFX Particle instances
struct VFXInstance
{
    DirectX::XMFLOAT2 position;
    float rotation = 0.0f;
    float scale = 1.0f;
    float lifetime = 0.0f;
    float maxLifetime = 0.0f;
    
    // For sprite-sheet animations (like laser hit VFX)
    bool isSpriteSheet = false;
    int currentFrame = 0;
    float frameTimer = 0.0f;
    float frameDuration = 0.08f;
    int frameCount = 1;
    int textureId = -1;
    
    // For simple single frame explosions or sequences of files
    bool isMultiTexture = false;
    std::vector<int> textureSequence;
};

// Floating Damage Number Popup (World of Warcraft Style Combat Text)
struct DamagePopup
{
    DirectX::XMFLOAT2 position{ 0.0f, 0.0f };
    DirectX::XMFLOAT2 velocity{ 0.0f, 0.0f };
    int damageAmount = 0;
    bool isCritical = false;
    bool isWeakpoint = false;
    bool isMining = false;
    float baseScale = 1.0f;
    float lifetime = 0.0f;
    float maxLifetime = 0.80f;
    DirectX::XMFLOAT4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    bool isTextLabel = false;   // When true, renders `label` via matrix font instead of a damage number
    std::string label;
};

// Expedition / Run Statistics Tracker
struct RunStats
{
    int reishiCollected = 0;
    int vidaCollected = 0;
    int disliCollected = 0;
    int cpuCollected = 0;
    int keyCollected = 0;
    int enemiesKilled = 0;
    int asteroidsMined = 0;
    int chestsOpened = 0;
};

class Game
{
public:
    Game();
    ~Game() = default;

    bool Initialize(HWND hWnd);
    void Finalize();

    void Update(float deltaTime);
    void Draw();

    void DamagePlayer(int amount);
    void ConsumeShieldCharge(bool reflected); // Pops the shield (1 hit), restarts its recharge, shows a popup
    void TriggerCameraShake(float duration, float intensity);
    void SpawnDamagePopup(const DirectX::XMFLOAT2& pos, int damage, bool isCrit, bool isWeakpoint = false, bool isMining = false);
    void SpawnTextPopup(const DirectX::XMFLOAT2& pos, const char* text, const DirectX::XMFLOAT4& color);
    void TriggerShockwave(const DirectX::XMFLOAT2& center, float maxRadius, float damage, bool affectEnemies = true);

private:
    // Game loops & state management
    void UpdateGameplay(float deltaTime);
    void UpdateUpgrade(float deltaTime);
    void UpdateMainMenu(float deltaTime);
    void DrawGameplay();
    void DrawUpgrade();
    void DrawMainMenu();

    // Main Menu / Title Screen Helpers
    void InitMainMenu();                    // Reset boot/menu state when (re)entering the title screen
    void UpdateMenuAmbientWorld(float deltaTime); // Parallax stars & rare idle-world background events
    void DrawAmbientShip(float shipCenterX, float shipCenterY, float engineGlow, bool allowMouseTilt);
    void ConfirmMenuSelection(int index);   // Handles activating the currently selected menu option
    void DrawMainMenuOptions(float camX, float camY);
    bool AnyKeyPressed() const;
    void StartExpeditionQuickLaunch();      // Shared by the menu's launch cinematic and the Upgrade Tree's own Launch button
    void DrawSkillBar();
    void DrawRunSummary();
    void DrawChestModal();
    void DrawEnergyDepletedModal();
    
    void SpawnAsteroids(float deltaTime);
    void SpawnEnemies(float deltaTime);
    void SpawnChest();
    void SpawnAnomalousAsteroid();
    void TargetAndFireLasers(float deltaTime);
    void UpdateTurrets(float deltaTime);
    void TriggerBossEncounter(int bossType = 1);
    void SpawnBoss(int bossType, float startX = -1.0f, float startY = -150.0f);
    void ResetRun();

    // Final Boss Encounter Helpers
    void UpdateFinalBoss(Asteroid& boss, float deltaTime);
    void SpawnFinalBossBlades(int count, bool isPrison = false);
    void TriggerBladeCommandAimedShot();
    void SpawnGhostOrbs(const DirectX::XMFLOAT2& bossPos);
    void SpawnPermanentGhostOrbs(const DirectX::XMFLOAT2& bossPos); // Final Phase permanent ghosts
    void FireGhostSpiral();                // Ghost Pattern 1: rotating spiral shot
    void FireGhostAimedSequence();         // Ghost Pattern 2: sequential 3-way aimed spreads
    void FireGhostCrossFire();             // Ghost Pattern 3: cardinal stop + telegraphed volley
    void HaltGhostOrbFiring();             // Disarm ghost orb auto-fire between combo steps
    void ClampFinalBossHpFloor(Asteroid& ast); // Enforce per-phase HP floor so no damage source can skip a phase

    // Helper functions
    float RandomFloat(float min, float max);
    int RandomInt(int min, int max);

private:
    HWND m_hWnd;
    
    // Scene management
    GameScene m_currentScene = GameScene::MainMenu;
    RunState m_runState = RunState::Active;

    // Main Menu / Title Screen state
    MainMenuPhase m_menuPhase = MainMenuPhase::Boot;
    float m_menuPhaseTimer = 0.0f;
    float m_menuBootPulseTimer = 0.0f;
    int m_menuSelectedIndex = 0;
    float m_menuSelectPulse = 0.0f;         // Brief glow pulse when selection changes
    float m_menuEngineGlow = 0.45f;         // Idle engine glow, ramps up during Launching
    DirectX::XMFLOAT2 m_menuShakeOffset{ 0.0f, 0.0f };
    float m_menuShakeTimer = 0.0f;
    float m_menuShakeMaxDuration = 0.0f;
    float m_menuShakeIntensity = 0.0f;
    float m_menuScannerPulseTimer = 0.0f;   // Idle-world: ship scanner pulse ring
    float m_menuScannerPulseCooldown = 6.0f;
    float m_menuAmbientSpawnCooldown = 4.0f;
    std::vector<MenuAmbientObject> m_menuAmbient; // Distant idle-world asteroids/minerals/silhouettes
    std::vector<DirectX::XMFLOAT3> m_menuStars;   // Parallax stars (x, y, brightness)
    std::string m_menuPlaceholderMessage;         // Brief "module offline" note for Collection/Settings
    float m_menuPlaceholderTimer = 0.0f;

    // Background Procedural Shader
    BackgroundRenderer m_bgRenderer;

    // Textures
    int m_texSpaceship = -1;
    int m_texLaser = -1;
    int m_texLaserHit = -1;
    int m_texAsteroid = -1;       // astroid.png
    int m_texBoss1 = -1;          // boss1.png
    int m_texBoss2 = -1;          // boss2.png
    int m_texBoss3 = -1;          // boss3.png
    int m_texFinalBoss = -1;      // final_boss.png
    int m_texBossOrb = -1;        // boss_orb.png
    int m_texBossBlade = -1;      // boss_blade.png
    int m_texBossProjectile = -1; // projectile.png
    int m_texEnemy1 = -1;         // enemy1.png
    int m_texEnemy1Bullet = -1;   // enemy1bullet.png
    int m_texResources = -1;
    int m_texNumber = -1;
    int m_texHeart = -1;
    int m_texVida = -1;           // vida.png
    int m_texDisli = -1;          // disli.png
    int m_texCpu = -1;            // cpu.png
    int m_texKey = -1;            // key.png
    int m_texChest = -1;          // chest.png
    int m_texTaret = -1;          // taret.png
    int m_texSkillDash = -1;      // dashSkill.png
    int m_texSkillWave = -1;      // energyWaveSkill.png
    int m_texSkillBuff = -1;      // buffSkill.png
    std::vector<int> m_texExplosions;

    int m_soundShoot = -1;        // shoot.wav
    int m_soundPat = -1;          // pat.mpeg (enemy death sound)
    int m_soundClick = -1;        // retroClick.mpeg (UI/upgrade click sound)
    int m_soundMusic = -1;        // gameMusic.mpeg (background music)

    // Player state & Upgrade Tree
    DirectX::XMFLOAT2 m_playerPos{ 800.0f, 450.0f };
    DirectX::XMFLOAT2 m_playerVelocity{ 0.0f, 0.0f };
    DirectX::XMFLOAT2 m_shipEntryTargetPos{ 800.0f, 450.0f }; // Where the ship decelerates to on gameplay entry
    float m_shipEntryElapsed = 0.0f;
    float m_playerRotation = 0.0f;
    float m_playerTargetRotation = 0.0f;
    float m_totalTime = 0.0f;
    float m_fuel = 150.0f;
    int m_playerHealth = 2;
    float m_invincibleTimer = 0.0f;
    float m_playerHitboxRadius = 16.0f;
    int m_reishiCount = 0;
    PlayerStats m_stats;

    // Shield Bubble state
    int m_currentShield = 0;
    float m_shieldRechargeTimer = 0.0f;

    // Active Skills runtime
    ActiveSkillSlot m_skillSlots[3];
    bool m_isDashing = false;
    float m_dashTimer = 0.0f;
    DirectX::XMFLOAT2 m_dashDir{ 0.0f, -1.0f };
    bool m_isOvercharged = false;
    float m_overchargeTimer = 0.0f;

    // Turrets & Projectiles
    std::vector<TurretInstance> m_turrets;
    std::vector<TurretProjectile> m_turretProjectiles;
    std::vector<ShockwaveInstance> m_shockwaves;
    float m_shockwaveAutoTimer = 0.0f;

    UpgradeTree m_upgradeTree;
    PlayerResources m_resources;
    RunStats m_runStats;
    bool m_superVacuumActive = false;

    // Upgrade Tree entrance (holographic slide-in from the Main Menu)
    float m_upgradeIntroTimer = 1.0f;    // 0 = ship centered & tree off-screen right, 1 = settled
    bool m_upgradeEnteredFromMenu = false;

    // Camera Shake
    float m_cameraShakeTimer = 0.0f;
    float m_cameraShakeMaxDuration = 0.0f;
    float m_cameraShakeIntensity = 0.0f;
    DirectX::XMFLOAT2 m_cameraOffset{ 0.0f, 0.0f };

    // Spawner config
    float m_spawnTimer = 0.0f;
    float m_spawnInterval = 1.0f;
    int m_maxAliveAsteroids = 15;

    float m_enemySpawnTimer = 0.0f;
    float m_enemySpawnInterval = 3.0f;
    int m_maxAliveEnemies = 5;

    // Chest & Anomalous Signal Timers
    float m_chestSpawnTimer = 0.0f;
    float m_anomalousSignalTimer = 0.0f;
    float m_anomalousWarningDisplayTimer = 0.0f;
    bool m_isChestModalActive = false;
    int m_activeChestIndex = -1;

    // Combat dynamics (Overheat, Momentum, Slingshot, Retaliation)
    float m_overheatDuration = 0.0f;
    void* m_lastLaserTarget = nullptr;
    float m_straightMoveTimer = 0.0f;
    float m_retaliationTimer = 0.0f;
    float m_slingshotBoostTimer = 0.0f;

    // Laser weapon state
    float m_laserFireCooldown = 0.0f;
    float m_laserDamageTickTimer = 0.0f;

    // Calamity / Disaster Meter state
    CalamityState m_calamity;
    float m_calamityFillDisplay = 0.0f;
    bool m_bossTriggered = false;
    bool m_bossVictory = false;
    float m_bossWarningTimer = 0.0f;

    // Lists of gameplay objects
    std::vector<Asteroid> m_asteroids;
    std::vector<Enemy> m_enemies;
    std::vector<BossOrb> m_bossOrbs;
    std::vector<BossBlade> m_bossBlades;
    std::vector<ResourcePickup> m_pickups;
    std::vector<OrbitingResource> m_orbitingResources;
    std::vector<AncientChest> m_chests;
    std::vector<LaserInstance> m_lasers;
    std::vector<VFXInstance> m_vfxs;
    std::vector<EnemyProjectile> m_enemyProjectiles;
    std::vector<DamagePopup> m_damagePopups;

    // Transition timers & Sequences
    float m_endRunTimer = 0.0f;
    float m_deathSequenceTimer = 0.0f;
    float m_bossDeathTimer = 0.0f;
    float m_explosionStaggerTimer = 0.0f;
    float m_runSummaryInputDelay = 0.0f;
    DirectX::XMFLOAT2 m_defeatedBossPos{ 0.0f, 0.0f };

    // Sector 5 Multi-Boss Rush Queue
    std::vector<int> m_sector5BossQueue;
    int m_sector5DefeatedCount = 0;
};

#endif // GAME_H
