#include "primary.h"
#include "../../gfx/color_space.h"

#include "color_space.hlsli"

cbuffer Vtx : register(b0) {
    column_major float4x4 vtx;
    int array_src_width;
    int array_src_height;
}

ConstantBuffer<Constants> constants : register(b1);
Texture2DArray tex : register(t0);
SamplerState sampler_point : register(s0);
SamplerState sampler_linear : register(s1);

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

    // convert to scRGB Linear (DXGI_FORMAT_R16G16B16A16_FLOAT)
    switch (constants.color_primaries)
    {
        case ColorPrimaries::Unknown:
            break;
        case ColorPrimaries::BT709:
            break;
        case ColorPrimaries::BT601:
            c.rgb = bt601_to_bt709(c.rgb);
            break;
        case ColorPrimaries::BT2020:
            c.rgb = bt2020_to_bt709(c.rgb);
            break;
    }
    switch (constants.transfer_characteristics)
    {
        case TransferCharacteristics::Linear:
            break;
        case TransferCharacteristics::Unknown:
            c.rgb = srgb_eotf(c.rgb);
            c.rgb /= 80.0;
            break;
        case TransferCharacteristics::sRGB:
            c.rgb = srgb_eotf(c.rgb);
            c.rgb /= 80.0;
            break;
        case TransferCharacteristics::BT601:
            c.rgb = bt601_eotf(c.rgb);
            c.rgb /= 80.0;
            break;
        case TransferCharacteristics::BT709:
            c.rgb = bt709_eotf(c.rgb);
            c.rgb /= 80.0;
            break;
        case TransferCharacteristics::ST2084:
            c.rgb = pq_eotf(c.rgb);
            c.rgb /= 80.0;
            break;
        case TransferCharacteristics::STDB67:
            c.rgb = hlg_eotf(c.rgb);
            c.rgb /= 80.0;
            break;
    }

    /*
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
    */

    // DXGI_FORMAT_R16G16B16A16_FLOAT: scRGB Linear
    // c.rgb = srgb_oetf(c.rgb);

    c.rgb = clamp(c.rgb, -0.5, 7.4999);
    c.a *= constants.alpha;
    return c;
}

