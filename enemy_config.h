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
    // ========================================================================
    // Oyun başlar başlamaz test için Boss 2'nin gelmesini sağlar.
    // Test bittiğinde 'TEST_SPAWN_BOSS2_AT_START = false;' yapabilirsiniz.
    inline constexpr bool TEST_SPAWN_BOSS2_AT_START = true;
    inline constexpr int  TEST_BOSS_TYPE             = 3;     // 1: Boss 1 (Kaya), 2: Boss 2 (Void Destroyer), 3: Boss 3 (Torii Yokai)

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
    // 5. BOSS 3 AYARLARI (BOSS 3 - TORII YOKAI / PURPLE DEATH BEAM)
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

        // Saldırı Döngüsü ve Faz Süreleri
        float glideDuration         = 3.8f;     // Tepede sağa-sola süzülme süresi
        float aimTrackingDuration   = 1.8f;     // Oyuncuyu hedefleme çizgisiyle takip etme süresi
        float aimTrackingTurnSpeed  = 2.2f;     // Hedefleme aşamasındaki takip hızı (rad/s)
        float aimLockPauseDuration  = 0.65f;    // KİLİTLENME VE KAÇIŞ ANI: Çizgi parlar ve sabitlenir, oyuncuya kaçma fırsatı verir (saniye)
        float laserFiringDuration   = 3.4f;     // Devasa mor lazer ateşleme süresi
        float laserTrackingTurnSpeed= 0.48f;    // Gecikmeli ağır takip hızı: Oyuncu hareket ettiği sürece lazer arkasında kalır (kaçabilir), sabit durursa lazer yetişip hasar verir (rad/s)
        float laserCooldownDuration = 1.2f;     // Saldırı bittikten sonraki dinlenme süresi

        // Lazer Parametreleri
        float laserDamageInterval   = 0.25f;    // Lazere temas edilince hasar yeme sıklığı (saniye)
        float laserBeamWidth        = 24.0f;    // Lazer ışınının çarpışma kalınlığı (piksel)

        // Boss 3 Ödül / Drop Miktarları
        int   reishiDropCount       = 45;
        int   reishiPerDrop         = 6;        // Toplam 270 Reishi
        int   vidaDropCount         = 16;
        int   disliDropCount        = 10;
        int   cpuDropCount          = 6;
        int   keyDropCount          = 3;        // 3 Sektör Anahtarı
    };
    inline constexpr Boss3Stats Boss3{};
}

#endif // ENEMY_CONFIG_H
