#pragma once

static const float pi = 3.141592635;

float3 reinhard(float3 hdr, float k = 1.0)
{
    return hdr / (hdr + k);
}

float3 reinhard_inverse(float3 sdr, float k = 1.0)
{
    return k * sdr / (k - sdr);
}

float3 standard(float3 hdr)
{
    return reinhard(hdr * sqrt(hdr), sqrt(4.0 / 27.0));
}

float3 standard_inverse(float3 sdr)
{
    return pow(reinhard_inverse(sdr, sqrt(4.0 / 27.0)), 2.0 / 3.0);
}

float3 aces(float3 hdr)
{
    const float A = 2.51, B = 0.03, C = 2.43, D = 0.59, E = 0.14;
    return saturate((hdr * (A * hdr + B)) / (hdr * (C * hdr + D) + E));
}

float3 aces_inverse(float3 sdr)
{
    const float A = 2.51, B = 0.03, C = 2.43, D = 0.59, E = 0.14;
    return 0.5 *
         (D * sdr -
             sqrt(((D * D - 4 * C * E) * sdr + 4 * A * E - 2 * B * D) * sdr +
                  B * B) -
             B) /
         (A - C * sdr);
}

// ref. https://godotshaders.com/shader/colorblindness-correction-shader/
float3 simulate_color_blind(float3 rgb, Colorblind mode, float intensity)
{
    float L = (17.8824 * rgb.r) + (43.5161 * rgb.g) + (4.11935 * rgb.b);
    float M = (3.45565 * rgb.r) + (27.1554 * rgb.g) + (3.86714 * rgb.b);
    float S = (0.0299566 * rgb.r) + (0.184309 * rgb.g) + (1.46709 * rgb.b);

    float l, m, s;
    if (mode == Colorblind::Protanopia)
    {
        l = 0.0 * L + 2.02344 * M + -2.52581 * S;
        m = 0.0 * L + 1.0 * M + 0.0 * S;
        s = 0.0 * L + 0.0 * M + 1.0 * S;
    }
    else if (mode == Colorblind::Deuteranopia)
    {
        l = 1.0 * L + 0.0 * M + 0.0 * S;
        m = 0.494207 * L + 0.0 * M + 1.24827 * S;
        s = 0.0 * L + 0.0 * M + 1.0 * S;
    }
    else if (mode == Colorblind::Tritanopia)
    {
        l = 1.0 * L + 0.0 * M + 0.0 * S;
        m = 0.0 * L + 1.0 * M + 0.0 * S;
        s = -0.395913 * L + 0.801109 * M + 0.0 * S;
    }

    float3 error;
    error.r = (0.0809444479 * l) + (-0.130504409 * m) + (0.116721066 * s);
    error.g = (-0.0102485335 * l) + (0.0540193266 * m) + (-0.113614708 * s);
    error.b = (-0.000365296938 * l) + (-0.00412161469 * m) + (0.693511405 * s);
    float3 diff = rgb - error;
    float3 correction;
    correction.r = 0.0;
    correction.g = (diff.r * 0.7) + (diff.g * 1.0);
    correction.b = (diff.r * 0.7) + (diff.b * 1.0);
    correction = rgb + correction * intensity;
    return correction;
}

float3 srgb_to_xyzd65(float3 srgb)
{
    const float3x3 m =
    {
        { 506752.0 / 1228815.0, 87881.0 / 245763.0, 12673.0 / 70218.0 },
        { 87098.0 / 409605.0, 175762.0 / 245763.0, 12673.0 / 175545.0 },
        { 7918.0 / 409605.0, 87881.0 / 737289.0, 1001167.0 / 1053270.0 },
    };
    return mul(m, srgb);
}

float3 xyzd65_to_srgb(float3 xyz)
{
    const float3x3 m =
    {
        { 12831.0 / 3959.0, -329.0 / 214.0, -1974.0 / 3959.0 },
        { -851781.0 / 878810.0, 1648619.0 / 878810.0, 36519.0 / 878810.0 },
        { 705.0 / 12673.0, -2585.0 / 12673.0, 705.0 / 667.0 },
    };
    return mul(m, xyz);
}

float3 xyzd65_to_oklab(float3 xyz)
{
    float3x3 xyz_to_lms =
    {
        { 0.8190224432164319, 0.3619062562801221, -0.12887378261216414 },
        { 0.0329836671980271, 0.9292868468965546, 0.03614466816999844 },
        { 0.048177199566046255, 0.26423952494422764, 0.6335478258136937 }
    };
    float3x3 lms_to_oklab =
    {
        { 0.2104542553, 0.7936177850, -0.0040720468 },
        { 1.9779984951, -2.4285922050, 0.4505937099 },
        { 0.0259040371, 0.7827717662, -0.8086757660 }
    };
    float3 lms = mul(xyz_to_lms, xyz);
    float3 oklab = mul(lms_to_oklab, pow(max(0.0, lms), 1.0 / 3.0));
    return oklab;
}

float3 oklab_to_xyzd65(float3 oklab)
{
    float3x3 lms_to_xyz =
    {
        { 1.2268798733741557, -0.5578149965554813, 0.28139105017721583 },
        { -0.04057576262431372, 1.1122868293970594, -0.07171106666151701 },
        { -0.07637294974672142, -0.4214933239627914, 1.5869240244272418 }
    };
    float3x3 oklab_to_lms =
    {
        { 0.99999999845051981432, 0.39633779217376785678, 0.21580375806075880339 },
        { 1.0000000088817607767, -0.1055613423236563494, -0.063854174771705903402 },
        { 1.0000000546724109177, -0.089484182094965759684, -1.2914855378640917399 }
    };

    float3 lmsnl = mul(oklab_to_lms, oklab);
    float3 xyzd65 = mul(lms_to_xyz, pow(lmsnl, 3.0));
    return xyzd65;
}

float3 oklab_to_oklch(float3 oklab)
{
    const float e = 0.0002;
    float hue;
    if (abs(oklab.y) < e && abs(oklab.z) < e)
    {
        hue = 0.0;
    }
    else
    {
        hue = atan2(oklab.z, oklab.y) * 180 / pi;
    }

    return float3(
		oklab.x,
		sqrt(oklab.y * oklab.y + oklab.z * oklab.z),
		(hue >= 0.0) ? hue : (hue + 360.0)
	);
}

float3 oklch_to_oklab(float3 oklch)
{
    float a, b;
    if (isnan(oklch.z))
    {
        a = 0;
        b = 0;
    }
    else
    {
        a = oklch.y * cos(oklch.z * pi / 180.0);
        b = oklch.y * sin(oklch.z * pi / 180.0);
    }
    return float3(oklch.x, a, b);
}

float3 bt601_to_bt709(float3 c)
{
    float3x3 rgb_to_yuv =
    {
        { 0.299, 0.587, 0.114 },
        { -0.168736, -0.331264, 0.5 },
        { 0.5, -0.418688, -0.081312 },
    };

    float3x3 yuv_to_rgb =
    {
        { 1, 0, 1.5748 },
        { 1, -0.01873, -0.4681 },
        { 1, 1.8556, 0 },
    };
        
    return mul(yuv_to_rgb, mul(rgb_to_yuv, c));
}

float3 bt2020_to_bt709(float3 c)
{
    float3x3 mat =
    {
        { 1.6605, -0.5877, -0.0728 },
        { -0.1246, 1.1330, -0.0084 },
        { -0.0182, -0.1006, 1.1187 },
    };
    return mul(mat, c);
}

// to linear
float3 srgb_eotf(float3 E)
{
    float3 dark = E / 12.92;
    float3 light = pow(abs((E + 0.055) / (1 + 0.055)), 2.4);
    return select(E.xyz <= 0.04045, dark, light) * 100.0;
}

float3 bt601_eotf(float3 L)
{
    float3 dark = 4.500 * L;
    float3 light = 1.009 * pow(L, 0.45) - 0.099;
    return select(L < 0.018, dark, light) * 100.0;
}

float3 bt709_eotf(float3 L)
{
    // ref. https://aomedia.googlesource.com/av1-xbox-one/+/refs/heads/main/testing/uwp_dx12_player/SamplePixelShader.hlsl
    const float a = 1.09929682f;
    const float b = 0.0180539685f;
    float3 dark = L / 4.5f;
    float3 light = pow(abs((L + a - 1.0f) / a), 1.0f / 0.45f);
    return select(L < 0.081242864, dark, light) * 100.0f;
}

float3 pq_eotf(float3 E)
{
    const float c1 = 0.8359375; // 3424.f/4096.f;
    const float c2 = 18.8515625; // 2413.f/4096.f*32.f;
    const float c3 = 18.6875; // 2392.f/4096.f*32.f;
    const float m1 = 0.159301758125; // 2610.f / 4096.f / 4;
    const float m2 = 78.84375; // 2523.f / 4096.f * 128.f;
    float3 M = c2 - c3 * pow(abs(E), 1 / m2);
    float3 N = max(pow(abs(E), 1 / m2) - c1, 0);
    float3 L = pow(abs(N / M), 1 / m1);
    L = L * 10000.0;
    return L;
}

float3 hlg_eotf(float3 E)
{
    const float a = 0.17883277f;
    const float b = 0.28466892f;
    const float c = 0.55991073f;
    const float Lmax = 12.0f;
    float3 dark = (E * 2.0f) * (E * 2.0f);
    float3 light = exp((E - c) / a) + b;
    return select(E <= 0.5f, dark, light) / Lmax * 1000.0;
}

// from linear
float3 srgb_oetf(float3 L)
{
    L = L / 100.0f;
    float3 dark = L * 12.92;
    float3 light = 1.055 * (float3) pow(abs(L), 1.0 / 2.4) - 0.055;
    float3 r = select(L <= 0.0031308, dark, light);
// #define SHOW_COLOR
#ifdef SHOW_COLOR
    float Y = 0.2126f * r.x + 0.7152f * r.y + 0.0722f * r.z;
    float3 ret = r;
    float gray = (3.0 - Y) / 4.0;
    float3 gray3 = float3(gray, gray, gray);
    if (Y > 3.0)
    {
        ret = float3(1.0, 0.0, 0.0);
    }
    else if (gray < 0.5)
    {
        ret = gray3;
    }
    return ret;
#else
    return r;
#endif
}

