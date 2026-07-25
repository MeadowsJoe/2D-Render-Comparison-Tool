/*
MIT License

Copyright (c) 2024 MSc Games Engineering Team

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

// Structure that holds the payload data for each ray
// This includes the current recursion depth, flags for state, a random seed
// the accumulated colour, and the current path throughput
cbuffer CBuffer : register(b0)
{
    float4x4 viewProj;
};

struct RasterInstanceData
{
    row_major float3x4 transform;
    uint textureID;
    uint pad[3];
};

StructuredBuffer<RasterInstanceData> instances : register(t0, space0);

Texture2D<float4> textures[] : register(t0, space1);
SamplerState samplerState : register(s0);

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 uv : TEXCOORD0;
    
    uint instanceID : SV_InstanceID;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
    
    uint textureID : TEXCOORD1;
};

VSOutput VSMain(VSInput input)
{
    RasterInstanceData inst = instances[input.instanceID];
    
    float4 localPos = float4(input.position, 1.0f);
    float3 worldPos = mul(inst.transform, localPos);
    
    float4 clipPos = float4(worldPos, 1.0f);
    float4 clipSpace = mul(viewProj, clipPos);
    
    VSOutput output;
    output.pos = clipSpace;
    output.uv = input.uv;
    output.textureID = inst.textureID;
    
    return output;
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    float4 s = textures[input.textureID].Sample(samplerState, input.uv);
    clip(s.a - 0.5f);
    return s;
}