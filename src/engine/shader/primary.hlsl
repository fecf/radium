#include "primary.h"


static const float pi = 3.141592635;

cbuffer Vtx : register(b0) {
    column_major float4x4 vtx;
    int array_src_width;
    int array_src_height;
}

ConstantBuffer<Constants> constants : register(b1);
Texture2DArray tex : register(t0);
SamplerState sampler_point : register(s0);
SamplerState sampler_linear : register(s1);

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

// https://godotshaders.com/shader/colorblindness-correction-shader/
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

float3 srgb_to_linear(float3 color)
{
    // approx pow(color, 2.2)
    return select(color < 0.04045, color / 12.92, pow(abs(color + 0.055) / 1.055, 2.4));
}

float3 linear_to_srgb(float3 color)
{
    // approx pow(color, 1.0 / .2)
    return select(color < 0.0031308, 12.92 * color, 1.055 * pow(abs(color), 1.0 / 2.4) - 0.055);
}

float3 pq_eotf(float3 color)
{
    const float c1 = 0.8359375; // 3424.f/4096.f;
    const float c2 = 18.8515625; // 2413.f/4096.f*32.f;
    const float c3 = 18.6875; // 2392.f/4096.f*32.f;
    const float m1 = 0.159301758125; // 2610.f / 4096.f / 4;
    const float m2 = 78.84375; // 2523.f / 4096.f * 128.f;
    float3 M = c2 - c3 * pow(abs(color), 1 / m2);
    float3 N = max(pow(abs(color), 1 / m2) - c1, 0);
    float3 L = pow(abs(N / M), 1 / m1);
    L = L * 10000.0;
    return L;
}

struct VSInput
{
    float4 pos : POSITION;
    float2 uv : TEXCOORD0;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOutput VS(VSInput input)
{
    VSOutput output;
    output.pos = mul(vtx, float4(input.pos.xyz, 1.f));
    output.uv = input.uv;
    return output;
};

float3 get_uv(float x, float y)
{
    float uw, uh, elems;
    tex.GetDimensions(uw, uh, elems);

    float src_w = array_src_width;
    float src_h = array_src_height;

    int col = x / uw;
    int row = y / uh;
    int cols = (src_w + uw - 1) / uw;
    int rows = (src_h + uh - 1) / uh;
    int pos = row * cols + col;

    float u0 = (col * uw) / (src_w - 1);
    float v0 = (row * uh) / (src_h - 1);
    float u1 = (col + 1) * uw / (src_w - 1);
    float v1 = (row + 1) * uh / (src_h - 1);
    float uv_x = x / (src_w - 1);
    float uv_y = y / (src_h - 1);
    float u = (uv_x - u0) / (u1 - u0);
    float v = (uv_y - v0) / (v1 - v0);

    return float3(saturate(u), saturate(v), pos);
}

float4 PS(VSOutput input) : SV_Target
{
    float x, y;
    if (array_src_width == 0 && array_src_height == 0) 
    {
        float uw, uh, elems;
        tex.GetDimensions(uw, uh, elems);
        x = input.uv.x * uw;
        y = input.uv.y * uh;
    }
    else
    {
        x = clamp(input.uv.x * (array_src_width - 1), 0, array_src_width - 1);
        y = clamp(input.uv.y * (array_src_height - 1), 0, array_src_height - 1);
    }

    // load texture / filtering
    float4 c;
    if (constants.filter == Filter::Nearest)
    {
        c = tex.Sample(sampler_point, get_uv(x, y));
    }
    else if (constants.filter == Filter::Bilinear)
    {
        if (ceil(x) == floor(x) & ceil(y) == floor(y))
        {
            c = tex.Sample(sampler_point, get_uv(x, y));
        }
        else if (ceil(x) == floor(x))
        {
            float4 q1 = tex.Sample(sampler_point, get_uv(x, floor(y)));
            float4 q2 = tex.Sample(sampler_point, get_uv(x, ceil(y)));
            c = q1 * (ceil(y) - y) + q2 * (y - floor(y));
        }
        else if (ceil(y) == floor(y))
        {
            float4 q1 = tex.Sample(sampler_point, get_uv(floor(x), y));
            float4 q2 = tex.Sample(sampler_point, get_uv(ceil(x), y));
            c = q1 * (ceil(x) - x) + q2 * (x - floor(x));
        }
        else
        {
            float4 v1 = tex.Sample(sampler_point, get_uv(floor(x), floor(y)));
            float4 v2 = tex.Sample(sampler_point, get_uv(ceil(x), floor(y)));
            float4 v3 = tex.Sample(sampler_point, get_uv(floor(x), ceil(y)));
            float4 v4 = tex.Sample(sampler_point, get_uv(ceil(x), ceil(y)));

            float4 q1 = v1 * (ceil(x) - x) + v2 * (x - floor(x));
            float4 q2 = v3 * (ceil(x) - x) + v4 * (x - floor(x));
            c = q1 * (ceil(y) - y) + q2 * (y - floor(y));
        }
    }

    // csc
    if (constants.cs_src == ColorSpace::sRGB)
    {
        c.rgb = srgb_to_linear(c.rgb);
    }
    else if (constants.cs_src == ColorSpace::Linear)
    {
    }
    else if (constants.cs_src == ColorSpace::Rec2020PQ)
    {
        // BT.2020 -> BT.709
        float3x3 mat = { 1.6605, -0.5877, -0.0728, -0.1246, 1.1330, -0.0084, -0.0182, -0.1006, 1.1187 };
        c.rgb = pq_eotf(c.rgb);

        c.r = c.r * mat[0][0] + c.g * mat[0][1] + c.b * mat[0][2];
        c.g = c.r * mat[1][0] + c.g * mat[1][1] + c.b * mat[1][2];
        c.b = c.r * mat[2][0] + c.g * mat[2][1] + c.b * mat[2][2];
        c.rgb /= 80.0;
    }

    // tone mapping (hdr -> sdr)
    if (constants.tone_mapping == ToneMapping::Standard)
    {
        c.rgb = standard(c.rgb);
    }
    else if (constants.tone_mapping == ToneMapping::Reinhard)
    {
        c.rgb = reinhard(c.rgb);
    }
    else if (constants.tone_mapping == ToneMapping::ACES)
    {
        c.rgb = aces(c.rgb);
    }

    // lch
    if ((constants.luminance != 0.0) || (constants.chroma != 0.0) || (constants.hue != 0.0))
    {
        float3 xyzd65 = srgb_to_xyzd65(c.rgb);
        float3 oklab = xyzd65_to_oklab(xyzd65);
        float3 oklch = oklab_to_oklch(oklab);
        if (constants.luminance != 0.0)
        { // 0.0f ~ 1.0f
            oklch.x += constants.luminance;
            oklch.x = clamp(oklch.x, 0.0, 1.0);
        }
        if (constants.chroma != 0.0)
        { // 0.0f ~ 0.4f
            oklch.y += constants.chroma * 0.4;
            oklch.y = clamp(oklch.y, 0.0, 0.4);
        }
        if (constants.hue != 0.0)
        { // 0.0f ~ 360.0f
            oklch.z += constants.hue;
            if (oklch.z < 0.0f)
            {
                oklch.z += 360.0f;
            }
            else if (oklch.z >= 360.0f)
            {
                oklch.z -= 360.0f;
            }
        }
        oklab = oklch_to_oklab(oklch);
        xyzd65 = oklab_to_xyzd65(oklab);
        c.rgb = xyzd65_to_srgb(xyzd65);
    }

    // colorblind
    if (constants.cb_mode != Colorblind::None)
    {
        c.rgb = simulate_color_blind(c.rgb, constants.cb_mode, constants.cb_intensity);
    }

    // lighting
    if (constants.brightness != 0.0)
    { // -1.0f ~ 1.0f
        c.rgb = max(0.0, c.rgb + constants.brightness);
    }

    if (constants.contrast != 0.0)
    { // -1.0f ~ 1.0f
        c.rgb = max(0.0, (c.rgb - 0.5) * (constants.contrast + 1.0) + 0.5);
    }

    if (constants.exposure != 0.0)
    { // -20.0f ~ 20.0f
        c.rgb = max(0.0, min(10.0, c.rgb * pow(2.0, constants.exposure)));
    }

    // tone mapping (sdr -> hdr)
    else if (constants.tone_mapping == ToneMapping::StandardInverse)
    {
        c.rgb = standard_inverse(c.rgb);
    }
    else if (constants.tone_mapping == ToneMapping::ReinhardInverse)
    {
        c.rgb = reinhard_inverse(c.rgb);
    }
    else if (constants.tone_mapping == ToneMapping::ACESInverse)
    {
        c.rgb = aces_inverse(c.rgb);
    }

    // csc
    if (constants.cs_dst == ColorSpace::sRGB) {
        c.rgb = linear_to_srgb(c.rgb);
    }

    c.rgb = clamp(c.rgb, 0.0, 1.0);
    c.a *= constants.alpha;

    // final result
    return max(0.0, c);
}

