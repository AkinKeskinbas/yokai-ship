#ifndef UPGRADE_TREE_H
#define UPGRADE_TREE_H

#include <Windows.h>
#include <DirectXMath.h>
#include <string>
#include <vector>

struct PlayerStats;

enum class NodeIconType
{
    CoreHub,
    Sword,
    LaserBeam,
    Fire,
    Explosion,
    Crosshair,
    Lightning,
    Magnet,
    Scanner,
    Crystal,
    ChartUp,
    Radar,
    Thruster,
    Chevrons,
    Battery,
    Gauge,
    Shield,
    Heart,
    Armor,
    Repair,
    Barrier,
    Turret,
    Shockwave,
    SkillEmp,
    SkillDash,
    SkillOvercharge,
    Lock,
    Star,
    Chest,
    Hybrid,
    Question
};

enum class NodeBranch
{
    Core,
    North_Weapon,
    East_Sensors,
    South_Engine,
    West_Defense,
    Hybrid,
    Active_Skills
};

enum class ActiveSkillType
{
    None,
    EmpWave,      // [Q] Screen-wide bullet eraser & AoE knockback/damage
    Overcharge,   // [E] 4s Hyper plasma laser frenzy
    PhaseDash,    // [SHIFT/SPACE] Invincible Phase Dash with i-frames
    NaniteAegis   // Emergency shield & heart repair
};

struct ResourceCost
{
    int reishi = 0;
    int vida = 0;
    int disli = 0;
    int cpu = 0;
    int key = 0;
};

struct UpgradeNode
{
    int id = 0;
    std::string title;
    std::string categoryName;
    std::string description;
    std::string effectFormat;
    NodeBranch branch = NodeBranch::Core;
    NodeIconType icon = NodeIconType::CoreHub;

    // Grid coordinates on screen
    DirectX::XMFLOAT2 gridPos{ 0.0f, 0.0f };
    DirectX::XMFLOAT2 screenPos{ 0.0f, 0.0f };

    // Level progression
    int currentLevel = 0;
    int maxLevel = 3;
    std::vector<ResourceCost> levelCosts;
    std::vector<float> levelValues;

    // Prerequisites (Parent node IDs required before unlocking this)
    std::vector<int> prerequisiteIds;

    // Lost Technology & Capstone flags
    bool isSealed = false;          // Locked by ancient seal (requires 1 Key)
    bool isSealBroken = false;      // True when Key has been spent to break seal
    int keySealCost = 1;            // Keys needed to break seal
    bool isCapstone = false;        // Ultimate 1-per-build capstone
    bool isHybrid = false;          // Cross-branch synergy node

    // Visual State & Animations
    bool isCenterHub = false;
    float hoverProgress = 0.0f;     // Smooth hover scale & glow
    float unlockPulseTimer = 0.0f;  // Flash when upgraded
    float idlePulsePhase = 0.0f;

    bool IsUnlocked() const { return currentLevel > 0 || isCenterHub; }
    bool IsMaxLevel() const { return currentLevel >= maxLevel; }
};

struct PlayerResources
{
    int reishi = 0;     // Main crystal currency
    int vida = 0;       // Screws / Bolts (vida.png)
    int disli = 0;      // Gears / Mechanical parts (disli.png)
    int cpu = 0;        // Quantum Processors / CPUs (cpu.png)
    int key = 0;        // Sector Keys / Ancient Key Tokens (key.png)
};

struct QuestData
{
    std::string title = "COLLECT 1,000 REISHI";
    int currentAmount = 0;
    int targetAmount = 1000;
    std::string rewardText = "+1 SECTOR KEY";
    bool completed = false;
};

struct SectorStage
{
    int stageNumber = 1;
    std::string name = "CRYOGENIC FIELDS";
    bool isBoss = false;
    bool unlocked = true;
    bool completed = false;
};

class UpgradeTree
{
public:
    UpgradeTree();
    ~UpgradeTree() = default;

    void Initialize(int texLaser, int texNumber, int texHeart, int texResources, int texSpaceship,
                    int texVida, int texDisli, int texCpu, int texKey, int soundClick,
                    int texSkillDash = -1, int texSkillWave = -1, int texSkillBuff = -1);
    void Update(float deltaTime, PlayerStats& stats, PlayerResources& bank, bool& outStartGame, int currentSector);
    void Draw(const PlayerResources& bank, int currentStage);

    // Apply all unlocked upgrades onto PlayerStats
    void ApplyStats(PlayerStats& outStats) const;

    // Helper to add resources from gameplay run
    void AddRunEarnings(PlayerResources& bank, int reishi, int vida, int disli, int cpu, int key);

    // Check if node can be unlocked/leveled
    bool CanUnlockNode(int nodeId, const PlayerResources& bank) const;
    bool HasPrerequisites(int nodeId) const;
    bool PurchaseUpgrade(int nodeId, PlayerResources& bank, PlayerStats& stats);
    bool UnsealLostTechNode(int nodeId, PlayerResources& bank);

    UpgradeNode* GetNodeById(int id);
    const UpgradeNode* GetNodeById(int id) const;

    // Capstone management (Only 1 active capstone allowed)
    int GetActiveCapstoneId() const { return m_activeCapstoneId; }
    void SetActiveCapstoneId(int id) { m_activeCapstoneId = id; }

    // Sector management
    void UnlockNextSector(int completedSector);
    int GetCurrentSectorIndex() const { return m_currentSectorIndex; }
    void SetCurrentSectorIndex(int idx) { m_currentSectorIndex = idx; }

private:
    void SetupNodes();
    void UpdateLayout();
    void DrawGridBackground();
    void DrawLeftPanel(const PlayerResources& bank);
    void DrawRightPanel(int currentStage);
    void DrawTreeNodes();
    void DrawBranchLines();
    void DrawTooltip(const UpgradeNode* node, const PlayerResources& bank);
    void DrawBankResourceTooltip(int index);
    void DrawNodeGlyph(NodeIconType icon, float x, float y, float size, const DirectX::XMFLOAT4& color);
    void DrawBankIcon(int type, float x, float y, float size);
    void DrawZoomControls();

private:
    std::vector<UpgradeNode> m_nodes;
    int m_hoveredNodeId = -1;
    int m_hoveredBankIndex = -1;
    int m_selectedNodeId = -1;
    float m_globalTime = 0.0f;

    // Zoom & Pan System
    float m_zoom = 0.95f;
    float m_targetZoom = 0.95f;
    DirectX::XMFLOAT2 m_panOffset{ 0.0f, 0.0f };
    DirectX::XMFLOAT2 m_targetPanOffset{ 0.0f, 0.0f };
    bool m_isPanning = false;
    int m_lastMouseX = 0;
    int m_lastMouseY = 0;

    // Quest & Sector data
    QuestData m_quest;
    std::vector<SectorStage> m_sectors;
    int m_currentSectorIndex = 1;
    int m_activeCapstoneId = -1;

    // Textures & Sounds
    int m_texLaser = -1;
    int m_texNumber = -1;
    int m_texHeart = -1;
    int m_texResources = -1;
    int m_texSpaceship = -1;
    int m_texVida = -1;
    int m_texDisli = -1;
    int m_texCpu = -1;
    int m_texKey = -1;
    int m_texSkillDash = -1;
    int m_texSkillWave = -1;
    int m_texSkillBuff = -1;
    int m_soundClick = -1;

    // Tree origin and sizing
    DirectX::XMFLOAT2 m_treeCenter{ 800.0f, 465.0f };
    float m_nodeSpacingX = 62.0f;
    float m_nodeSpacingY = 56.0f;
    float m_nodeSize = 34.0f;

    // Start Button state
    bool m_btnStartHovered = false;
    float m_btnStartHoverAnim = 0.0f;
};

#endif // UPGRADE_TREE_H
