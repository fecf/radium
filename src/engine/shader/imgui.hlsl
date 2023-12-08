cbuffer Vtx : register(b0)
{
    column_major float4x4 vtx;
}
Texture2D tex : register(t0);
SamplerState sampler_linear : register(s1);

struct VSInput
{
    float2 pos : POSITION;
    float4 col : COLOR0;
    float2 uv : TEXCOORD0;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
    float2 uv : TEXCOORD0;
};

VSOutput VS(VSInput input)
{
    VSOutput output;
    output.pos = mul(vtx, float4(input.pos.xy, 0.f, 1.f));
    output.col = input.col;
    output.uv = input.uv;
    return output;
};

float4 PS(VSOutput input) : SV_Target
{
    float4 c = input.col;
    float4 t = tex.Sample(sampler_linear, input.uv);
    return c * t;
}
