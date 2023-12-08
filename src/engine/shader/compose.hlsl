Texture2D ui : register(t0);
SamplerState sampler_point : register(s0);

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
    output.pos = float4(input.pos.xy, 0.0f, 1.0f);
    output.uv = input.uv;
    return output;
};

float4 PS(VSOutput input) : SV_Target
{
    float4 col = ui.Sample(sampler_point, input.uv);
    return col;
}
