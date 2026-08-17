// ============================================================================
// Background Pixel Shader [shader_background_ps.hlsl]
// Retro Psychedelic Animated Cosmic Plasma & Nebula (DirectX 11 HLSL)
// Low-luminance, low-contrast, non-distracting for maximum bullet-hell readability
// ============================================================================

cbuffer BackgroundBuffer : register(b0)
{
    float Time;                 // Elapsed time in seconds
    float DistortionStrength;   // Base 0.003, Boss 0.006
    float FlowSpeed;            // Base 0.25, Boss 0.45
    float PulseStrength;        // Base 0.02, Boss 0.04
    float HueShift;             // Subtle color shift
    float VignetteStrength;     // Edge darkness (0.85)
    float BossIntensity;        // 0.0 (normal) to 1.0 (boss encounter)
    float ScreenWidth;          // 1600.0
    float ScreenHeight;         // 900.0
    float CamOffsetX;           // Camera parallax shake X
    float CamOffsetY;           // Camera parallax shake Y
    float Padding;
};

struct PS_INPUT
{
    float4 posH : SV_POSITION;
    float2 uv   : TEXCOORD0;
};

// Pseudo-random hash for sparse star distribution
float Hash21(float2 p)
{
    p = frac(p * float2(234.34, 435.345));
    p += dot(p, p + 34.23);
    return frac(p.x * p.y);
}

// Sparse, soft, ultra-dim star layer (cannot be mistaken for bullets)
float SparseDimStars(float2 p, float time)
{
    float2 gridId = floor(p * 14.0);
    float2 gridUv = frac(p * 14.0) - 0.5;

    float rnd = Hash21(gridId);
    float dist = length(gridUv);
    // Soft Gaussian falloff rather than sharp points
    float star = smoothstep(0.35, 0.0, dist);
    
    // Slow gentle twinkle with unique phase
    float twinkle = 0.5 + 0.5 * sin(time * 1.8 + rnd * 62.83);
    
    // Only spawn stars in ~10% of cells, kept extremely dim (max 0.09 brightness)
    float mask = step(0.90, rnd);
    return star * twinkle * 0.09 * mask;
}

float4 main(PS_INPUT input) : SV_TARGET
{
    float2 uv = input.uv;

    // Aspect-corrected centered UV coordinates with subtle camera parallax
    float aspect = ScreenWidth / ScreenHeight;
    float2 p = (uv - 0.5) * float2(aspect, 1.0);
    p += float2(CamOffsetX, CamOffsetY) * 0.00015;

    float t = Time * FlowSpeed;

    // ------------------------------------------------------------------------
    // 1. HYPNOTIC DOMAIN-WARPED PLASMA / NEBULA (Balatro-style retro fluid)
    // ------------------------------------------------------------------------
    float2 q = p;
    float distFromCenter = length(q);
    float centerAngle = atan2(q.y, q.x);

    // Gentle center pulse wave
    float centerPulse = sin(distFromCenter * 4.0 - t * 1.2) * PulseStrength;
    q += (q / (distFromCenter + 0.001)) * centerPulse;

    // Slow spiral twist
    float swirlAngle = centerAngle + sin(distFromCenter * 2.5 - t * 0.8) * (0.35 + BossIntensity * 0.25);
    q = float2(cos(swirlAngle), sin(swirlAngle)) * distFromCenter;

    // Multi-octave sinusoidal harmonic domain warping
    [unroll]
    for (int i = 1; i <= 3; ++i)
    {
        float fi = float(i);
        float warpX = sin(fi * 2.4 * q.y + t * 0.75 + fi * 1.2) * (0.16 / fi);
        float warpY = cos(fi * 2.4 * q.x + t * 0.65 + fi * 0.9) * (0.16 / fi);
        q.x += warpX + DistortionStrength * fi;
        q.y += warpY + DistortionStrength * fi;
    }

    // Plasma harmonic ribbons
    float wave1 = sin(q.x * 2.2 + q.y * 1.8 + t * 0.45);
    float wave2 = cos(q.x * 1.5 - q.y * 2.5 + t * 0.35);
    float wave3 = sin(length(q) * 3.0 - t * 0.55);
    float wave4 = cos((q.x + q.y) * 2.0 + t * 0.6);

    float plasma = (wave1 + wave2 + wave3 + wave4) * 0.25; // [-1.0, 1.0]
    float normPlasma = plasma * 0.5 + 0.5; // [0.0, 1.0]

    // ------------------------------------------------------------------------
    // 2. DARK COSMIC COLOR PALETTE (Strictly low-luminance for gameplay clarity)
    // ------------------------------------------------------------------------
    // Base Colors (Low brightness, muted, harmonious):
    float3 colDeepVoid   = float3(0.012, 0.015, 0.032); // Deepest cosmic void (almost black)
    float3 colCosmicNavy = float3(0.022, 0.038, 0.082); // Midnight navy blue
    float3 colDarkPurple = float3(0.048, 0.020, 0.070); // Deep desaturated cosmic indigo
    float3 colSubtleTeal = float3(0.015, 0.045, 0.055); // Subtle dark teal accent

    // Sinister Boss Mode Tint (deep dark crimson/violet, still kept dark):
    float3 colBossCrimson = float3(0.075, 0.014, 0.042);
    colDarkPurple = lerp(colDarkPurple, colBossCrimson, BossIntensity * 0.85);

    // Multi-band color blending across plasma gradient
    float3 finalColor = colDeepVoid;
    finalColor = lerp(finalColor, colCosmicNavy, smoothstep(0.15, 0.50, normPlasma));
    finalColor = lerp(finalColor, colDarkPurple, smoothstep(0.45, 0.80, normPlasma));
    finalColor = lerp(finalColor, colSubtleTeal, smoothstep(0.75, 0.98, normPlasma) * 0.55);

    // Boss aura breathing along peripheral space
    if (BossIntensity > 0.01)
    {
        float bossPulse = sin(t * 2.2) * 0.5 + 0.5;
        float3 bossEdgeGlow = float3(0.045, 0.008, 0.025) * (BossIntensity * (0.6 + bossPulse * 0.4));
        finalColor += bossEdgeGlow * smoothstep(0.2, 0.9, distFromCenter);
    }

    // ------------------------------------------------------------------------
    // 3. SPARSE DIM STARS & COSMIC DUST
    // ------------------------------------------------------------------------
    float stars = SparseDimStars(p, Time);
    finalColor += float3(stars, stars * 0.95, stars * 1.1);

    // ------------------------------------------------------------------------
    // 4. RETRO VIGNETTE & ULTRA-SUBTLE SCANLINES
    // ------------------------------------------------------------------------
    // Smooth cinematic vignette
    float2 vigCoord = uv * (1.0 - uv.yx);
    float vig = vigCoord.x * vigCoord.y * 15.0;
    vig = clamp(pow(vig, 0.35 * VignetteStrength), 0.0, 1.0);
    finalColor *= vig;

    // Ultra-subtle scanlines (3% variance for gentle arcade texture without flicker)
    float scanline = 0.97 + 0.03 * sin(uv.y * ScreenHeight * 1.5707963);
    finalColor *= scanline;

    // Hard clamp to guarantee background luminance never exceeds 0.22 (100% readability)
    finalColor = min(finalColor, float3(0.22, 0.22, 0.24));

    return float4(finalColor, 1.0);
}
