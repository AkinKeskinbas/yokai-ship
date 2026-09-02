#ifndef ENEMY_CONFIG_H
#define ENEMY_CONFIG_H

#include <DirectXMath.h>

// ============================================================================
// DÜŞMAN VE BOSS YAPILANDIRMA DOSYASI (ENEMY & BOSS CONFIGURATION)
// Tüm düşman ve boss değerlerini bu dosyadan tek bir yerden kolayca değiştirebilirsiniz.
// ============================================================================

namespace EnemyConfig
{
    // ========================================================================
    // 1. TEST VE DEBUG AYARLARI (TEST & DEBUG SETTINGS)
    // ==================================================================    // Test Modu: Oyunu baslatir baslatmaz Sektor 3 Boss'unu (Torii Yokai) spawn eder.
    inline constexpr bool TEST_SPAWN_BOSS2_AT_START = true;
    inline constexpr int  TEST_BOSS_TYPE             = 4;     // 1: Boss 1, 2: Boss 2, 3: Boss 3 (Torii Yokai), 4: Final Boss (Kitsune Yokai)

    // ========================================================================
    // 2. NORMAL DÜŞMAN DRONE AYARLARI (ENEMY DRONE 1 CONFIG)
    // ========================================================================
    struct DroneStats
    {
        float maxHp             = 45.0f;    // Düşman canı
        float moveSpeed         = 85.0f;    // Hareket hızı
        float radius            = 22.0f;    // Çarpışma yarıçapı
        float scale             = 0.12f;    // Görsel boyut ölçeği (enemy1.png)
        float shootInterval     = 1.6f;     // Ateş etme aralığı (saniye)
        float bulletSpeed       = 125.0f;   // Mermi hızı
        float bulletRadius      = 13.0f;    // Mermi çarpışma yarıçapı
        float bulletVisualSize  = 42.0f;    // Mermi ekrandaki görsel boyutu (piksel)
        int   bulletDamage      = 1;        // Mermi hasarı (kalp)
        float bulletLifetime    = 4.5f;     // Mermi menzil ömrü
        float calamityGainOnKill= 7.0f;     // Öldürülünce Felaket Sayacına eklenen puan

        // Ganimet / Drop Ayarları
        int   minReishiDrop     = 2;
        int   maxReishiDrop     = 4;
        float vidaDropChance    = 0.60f;    // Vida (%60)
        float disliDropChance   = 0.25f;    // Dişli (%25)
        float cpuDropChance     = 0.08f;    // CPU (%8)
    };
    inline constexpr DroneStats Drone{};

    // ========================================================================
    // 3. BOSS 1 AYARLARI (BOSS 1 - GARGANTUAN ROCK)
    // ========================================================================
    struct Boss1Stats
    {
        float baseHp            = 600.0f;   // Sektör 1 Canı
        float hpPerSectorLevel  = 300.0f;   // Sektör başına ek can
        float scale             = 0.22f;    // Görsel boyutu (boss1.png)
        float radius            = 85.0f;    // Çarpışma yarıçapı
        float moveSpeed         = 16.0f;    // İlerleme hızı
        float rotationSpeed     = 0.03f;    // Dönme hızı
        int   collisionDamage   = 1;        // Çarpışma hasarı

        // Ödül / Drop Miktarları
        int   reishiDropCount   = 25;
        int   reishiPerDrop     = 4;        // Toplam 100 Reishi
        int   vidaDropCount     = 8;
        int   disliDropCount    = 5;
        int   cpuDropCount      = 2;
        int   keyDropCount      = 1;
    };
    inline constexpr Boss1Stats Boss1{};

    // ========================================================================
    // 4. BOSS 2 AYARLARI (BOSS 2 - VOID DESTROYER / SPIRAL ATTACKER)
    // ========================================================================
    struct Boss2Stats
    {
        float baseHp            = 950.0f;   // Sektör 2 Canı
        float hpPerSectorLevel  = 400.0f;   // Sektör başına ek can
        float scale             = 0.26f;    // Görsel boyutu (boss2.png)
        float radius            = 90.0f;    // Çarpışma yarıçapı
        float moveSpeed         = 70.0f;    // Devriye / Gezinme hızı
        float normalRotationSpeed = 0.6f;   // Normal gezinirken dönüş hızı
        int   collisionDamage   = 1;        // Çarpışma hasarı

        // Normal Faz Ateşleme Ayarları
        float normalShootInterval   = 1.8f;     // Normal fazda 4 yönlü mermi atış aralığı
        float normalBulletSpeed     = 150.0f;   // Normal faz mermi hızı
        int   normalBulletDamage    = 1;        // Normal mermi hasarı

        // Özel Spiral Saldırı Fazı Ayarları (Spiral Attack Phase)
        float patrolDuration        = 6.0f;     // Gezinme süresi (sonra alarm fazına geçer)
        float alarmWarningDuration  = 1.4f;     // Spiral öncesi alarm / yanıp sönme uyarısı (saniye)
        float spiralAttackDuration  = 3.8f;     // Spiral mermi yağmuru süresi (saniye)
        float spiralCooldown        = 1.0f;     // Saldırı bittikten sonraki dinlenme süresi

        float spiralFireRate        = 0.08f;    // Spiral mermilerin çıkış sıklığı (her 0.08 saniyede 4 mermi)
        float spiralRotationSpeed   = 3.8f;     // Spiral ateş esnasında kendi etrafında dönme hızı (rad/s)
        float spiralBulletSpeed     = 175.0f;   // Spiral mermilerin fırlama hızı
        float spiralBulletRadius    = 12.0f;    // Spiral mermi boyutu
        int   spiralBulletDamage    = 1;        // Spiral mermi hasarı
        float spiralBulletLifetime  = 5.0f;     // Spiral mermi menzil ömrü

        // Boss 2 Ödül / Drop Miktarları
        int   reishiDropCount   = 35;
        int   reishiPerDrop     = 5;        // Toplam 175 Reishi
        int   vidaDropCount     = 12;
        int   disliDropCount    = 8;
        int   cpuDropCount      = 4;
        int   keyDropCount      = 2;        // 2 Sektör Anahtarı
    };
    inline constexpr Boss2Stats Boss2{};

    // ========================================================================
    // 5. BOSS 3 AYARLARI (BOSS 3 - TORII YOKAI / MULTI-PHASE LASER BOSS)
    // ========================================================================
    struct Boss3Stats
    {
        float baseHp                = 1200.0f;  // Sektör 3 Canı
        float hpPerSectorLevel      = 450.0f;   // Sektör başına ek can
        float scale                 = 0.30f;    // Görsel boyutu (boss3.png)
        float radius                = 92.0f;    // Çarpışma yarıçapı
        float moveSpeed             = 120.0f;   // Tepede süzülme hızı
        float hoverY                = 150.0f;   // Tepedeki Y yüksekliği
        int   collisionDamage       = 1;        // Çarpışma hasarı

        // Faz 1: Aimed Sweeping Laser (HP > 70%)
        float glideDuration             = 3.0f;     // Tepede sağa-sola süzülme süresi
        float aimTrackingDuration       = 1.4f;     // Oyuncuyu hedefleme çizgisiyle takip etme süresi
        float aimTrackingTurnSpeed      = 2.2f;     // Hedefleme aşamasındaki takip hızı (rad/s)
        float aimLockPauseDuration      = 0.75f;    // KİLİTLENME VE KAÇIŞ ANI: Çizgi sabitlenir, kaçış fırsatı verir (saniye)
        float laserFiringDuration       = 2.2f;     // Toplam mor lazer ateşleme süresi
        float laserActiveTrackingDuration= 1.2f;    // Lazer ateşlendikten sonraki ağır takip süresi
        float laserTrackingTurnSpeed    = 0.52f;    // Gecikmeli ağır takip hızı (rad/s)

        // Faz 2: Dikey Lazer Duvarı & Güvenli Aralık (HP 40% - 70%)
        float curtainWarningDuration    = 1.3f;     // Uyarılı uyarı süresi (yeşil güvenli koridor gösterilir)
        float curtainFiringDuration     = 2.2f;     // Dikey lazer sütunlarının ateşlenme süresi

        // Faz 3: Çapraz Izgara Lazer Matrisi (HP < 40%)
        float gridWarningDuration       = 1.4f;     // Izgara uyarı süresi (yeşil güvenli hücre gösterilir)
        float gridFiringDuration        = 2.4f;     // Çapraz lazer ızgarasının ateşlenme süresi

        float laserCooldownDuration     = 1.2f;     // Fazlar arası dinlenme süresi

        // Lazer Parametreleri
        float laserDamageInterval       = 0.25f;    // Lazere temas edilince hasar yeme sıklığı (saniye)
        float laserBeamWidth            = 24.0f;    // Lazer ışınının çarpışma kalınlığı (piksel)
        float laserMinAngle             = 0.35f;    // Lazerin minimum açısı (~20 derece)
        float laserMaxAngle             = 2.79f;    // Lazerin maksimum açısı (~160 derece)

        // Boss 3 Ödül / Drop Miktarları
        int   reishiDropCount           = 45;
        int   reishiPerDrop             = 6;        // Toplam 270 Reishi
        int   vidaDropCount             = 16;
        int   disliDropCount            = 10;
        int   cpuDropCount              = 6;
        int   keyDropCount              = 3;        // 3 Sektör Anahtarı
    };
    inline constexpr Boss3Stats Boss3{};

    // ========================================================================
    // 6. FİNAL BOSS AYARLARI (FINAL BOSS - KITSUNE YOKAI ENTITY)
    // ========================================================================
    struct BossFinalStats
    {
        float baseHp                    = 3000.0f;  // Final Boss HP (~35% increase for phase-gated fight)
        float hpPerSectorLevel          = 600.0f;   // Sektör başına ek can
        float scale                     = 0.28f;    // Görsel boyutu (final_boss.png)
        float radius                    = 95.0f;    // Çarpışma yarıçapı
        float moveSpeed                 = 80.0f;    // Üst bölgede gezinme hızı
        float hoverY                    = 160.0f;   // Tepedeki Y yüksekliği
        int   collisionDamage           = 1;        // Çarpışma hasarı

        // Faz 0: Koruyucu Orb Kalkanı (Orb Shield)
        float orbHp                     = 200.0f;   // Her bir yok edilebilir orb canı
        float orbRadius                 = 24.0f;    // Orb çarpışma yarıçapı
        float orbScale                  = 0.09f;    // Orb görsel boyutu (boss_orb.png)
        float orbOrbitRadius            = 155.0f;   // Boss etrafındaki dönüş yarıçapı
        float orbBaseRotationSpeed      = 1.10f;    // 4 orb canlıyken temel dönüş hızı (rad/s)
        float orbFireInterval           = 2.0f;     // Orb ateş etme aralığı
        float orbBulletSpeed            = 160.0f;   // Orb mermi hızı
        int   orbBulletDamage           = 1;        // Orb mermi hasarı

        // Faz 1: Temel Saldırılar (100% -> 70% HP)
        float phase1AttackInterval      = 2.2f;     // Saldırı aralığı (5'li fan ve 8 yönlü radial)
        float phase1BulletSpeed         = 165.0f;

        // Faz 2: Düşen Kılıç Tuzakları (70% -> 40% HP)
        float bladeRadius               = 24.0f;    // Kılıç çarpışma yarıçapı
        float bladeScale                = 0.12f;    // Kılıç görsel boyutu (boss_blade.png)
        float bladeWarningDuration      = 0.65f;    // Kırmızı uyarı sütunu süresi
        float bladeFallSpeed            = 950.0f;   // Kılıcın dikey iniş hızı
        float bladePulseInterval        = 1.5f;     // Saplanan kılıcın 4 yönlü mermi atma aralığı
        float bladePulseBulletSpeed     = 140.0f;
        float bladeCommandInterval      = 4.5f;     // Boss'un tüm saplı kılıçları oyuncuya ateşletme aralığı
        int   maxEmbeddedBlades         = 4;        // Sahada aynı anda bulunabilecek maksimum saplı kılıç
        float bladeLifetime             = 14.0f;    // Kılıcın sahadan silinme süresi

        // Phase Damage Gating (HP cannot fall below these thresholds during each phase)
        float phase1HpFloor             = 0.70f;    // Phase 1: HP cannot drop below 70%
        float phase2HpFloor             = 0.40f;    // Phase 2: HP cannot drop below 40%
        float phase3HpFloor             = 0.15f;    // Phase 3: HP cannot drop below 15%
        float transitionDuration        = 0.8f;     // Phase transition invulnerability duration

        // Phase 3: Ghost Orb Patterns + Blade Combo (40% -> 15% HP)
        float ghostOrbOrbitSpeed         = 1.4f;     // Ghost orb orbit speed (rad/s)
        float ghostOrbBulletSpeed        = 260.0f;   // Ghost orb projectile speed (Spiral & Aimed Sequence)
        float ghostSpiralFireInterval    = 0.45f;    // Spiral pattern: time between each orb shot
        float ghostAimedSequenceDelay    = 0.25f;    // Aimed Sequence: delay between each orb firing
        float ghostCrossFireWarning      = 0.6f;     // CrossFire: warning telegraph duration
        float ghostCrossFireBulletSpeed  = 320.0f;   // CrossFire: projectile speed
        float phase3ComboInterval        = 1.2f;     // Time between combo sequence steps in Phase 3
        float phase3RecoveryTime         = 0.8f;     // Short recovery pause between combo elements

        // Final Phase: Permanent Ghost Orbs + All Attacks (15% -> 0% HP)
        float finalGhostOrbitSpeed       = 1.8f;     // Faster ghost orbit in Final Phase
        float finalComboInterval         = 0.9f;     // ~25% faster combo cadence than Phase 3
        float finalRecoveryTime          = 0.6f;     // ~25% shorter recovery than Phase 3
        float bladePrisonCooldown        = 9.0f;     // Blade Prison special attack cooldown

        // Final Boss Ödül / Drop Miktarları
        int   reishiDropCount           = 60;
        int   reishiPerDrop             = 8;        // Toplam 480 Reishi
        int   vidaDropCount             = 20;
        int   disliDropCount            = 14;
        int   cpuDropCount              = 8;
        int   keyDropCount              = 4;        // 4 Sektör Anahtarı
    };
    inline constexpr BossFinalStats BossFinal{};
}

#endif // ENEMY_CONFIG_H
