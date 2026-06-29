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
struct Payload
{
    uint depth;
    int flags;
    uint rndState;
    float3 colour;
    float3 pathThroughput;
};

// Constant buffer holding camera matrices, number of area lights, Samples Per Pixel (SPP)
// and a flag for whether to use an environment map
cbuffer CBuffer : register(b0)
{
    float4x4 inverseView;
    float4x4 inverseProjection;
    uint nLights;
    float SPP;
    uint useEnvironmentMap;
};

// Acceleration structure for raytracing the scene
RaytracingAccelerationStructure scene : register(t0);

// UAV for storing the final rendered image (output texture)
RWTexture2D<float4> uav : register(u0);

// Array of textures and sampler state for texture sampling
Texture2D<float4> textures[] : register(t0, space1);
SamplerState samplerState : register(s0);

#define PI 3.1415926535

// Structure representing a vertex with position, normal, tangent, and texture coordinates
struct Vertex
{
    float3 position;
    float3 normal;
    float3 tangent;
    float2 uv;
};

// Structure holding instance-specific data including index offsets
// BSDF/albedo texture ID, and additional material parameters
struct InstanceData
{
    uint startIndex;
    unsigned int bsdfAlbedoID;
    float bsdfData[7];
    float coatingData[6];
};

// Structure for area light information including three vertices (defining a triangle)
// the light's normal, and its emitted radiance
struct AreaLightData
{
    float3 v1;
    float3 v2;
    float3 v3;
    float3 normal;
    float3 Le;
};

// Buffers for vertex data, index data, instance data, and area light data
StructuredBuffer<Vertex> vertices   : register(t1);
StructuredBuffer<uint>   indices    : register(t2);
StructuredBuffer<InstanceData> instanceData : register(t3);
StructuredBuffer<AreaLightData> areaLightData : register(t4);

// Environment map texture for background lighting
Texture2D<float4> environmentMap : register(t5);

// Structure holding hit data computed at a ray intersection
struct HitData
{
    float3 pos;       // World-space hit position
    float3 normal;    // Surface normal at the hit
    float3x3 tbn;     // Tangent, bitangent, and normal matrix
    float2 uv;        // Interpolated texture coordinates
    uint bsdf;        // BSDF type identifier
    float3 albedo;    // Surface albedo colour
    InstanceData instance; // Instance data for the hit geometry
};

// Function to compute the inverse of a 3x3 matrix
float3x3 inverse(float3x3 m)
{
    float a = m[0][0], b = m[0][1], c = m[0][2];
    float d = m[1][0], e = m[1][1], f = m[1][2];
    float g = m[2][0], h = m[2][1], i = m[2][2];

    float det = a * (e * i - f * h)
        - b * (d * i - f * g)
        + c * (d * h - e * g);
    float invDet = 1.0f / det;

    float3x3 inv;
    inv[0][0] = (e * i - f * h) * invDet;
    inv[0][1] = (c * h - b * i) * invDet;
    inv[0][2] = (b * f - c * e) * invDet;

    inv[1][0] = (f * g - d * i) * invDet;
    inv[1][1] = (a * i - c * g) * invDet;
    inv[1][2] = (c * d - a * f) * invDet;

    inv[2][0] = (d * h - e * g) * invDet;
    inv[2][1] = (b * g - a * h) * invDet;
    inv[2][2] = (a * e - b * d) * invDet;

    return inv;
}

// Function to determine if the BSDF is two-sided; returns false for specific BSDF types (4 and 6), otherwise true
bool isBSDFTwoSided(uint bsdf)
{
    if (bsdf == 4 || bsdf == 6)
    {
        return false;
    }
    return true;
}

// Evaluates the environment map by converting a direction vector into spherical coordinates; the direction is mapped to texture UV coordinates
float3 evaluateEnvironmentMap(float3 wi)
{
    float u = atan2(wi.z, wi.x);
    u = (u < 0.0f) ? u + (2.0f * PI) : u;
    u = u / (2.0f * PI);
    float v = acos(wi.y) / PI;
    return environmentMap.SampleLevel(samplerState, float2(u, v), 0);
}

// Encodes the specular flag into the given flags integer
uint encodeIsSpecular(uint flags)
{
    return flags | 2;
}

// Clear the specular flag
uint clearSpecular(uint flags)
{
    return flags & 0xFD;
}

// Decodes and checks if the specular flag is set
bool decodeIsSpecular(uint flags)
{
    return ((flags & 2) > 0);
}

// Encodes the shadow flag into the given flags integer
uint encodeIsShadow(uint flags)
{
    return flags | 4;
}

// Decodes and checks if the shadow flag is set
bool decodeIsShadow(uint flags)
{
    return ((flags & 4) > 0);
}

// Computes hit data at the intersection point by interpolating vertex attributes and applying necessary transforms; uses built-in triangle intersection attributes
HitData calculateHitData(BuiltInTriangleIntersectionAttributes attrib)
{
    HitData hitData;
    // Retrieve instance data for the current hit
    hitData.instance = instanceData[InstanceID()];
    uint startIndex = hitData.instance.startIndex + (PrimitiveIndex() * 3);

    // Fetch the three vertices of the hit triangle
    Vertex vertex[3];
    vertex[0] = vertices[indices[startIndex]];
    vertex[1] = vertices[indices[startIndex + 1]];
    vertex[2] = vertices[indices[startIndex + 2]];

    // Barycentric coordinates for interpolation
    float u = attrib.barycentrics.x;
    float v = attrib.barycentrics.y;
    float w = 1.0 - attrib.barycentrics.x - attrib.barycentrics.y;

    // Compute hit position along the ray
    hitData.pos = WorldRayOrigin() + (WorldRayDirection() * RayTCurrent());

    // Interpolate the surface normal from the vertex normals
    hitData.normal = normalize((w * vertex[0].normal) + (u * vertex[1].normal) + (v * vertex[2].normal));

    // Interpolate the texture coordinates
    hitData.uv = (w * vertex[0].uv) + (u * vertex[1].uv) + (v * vertex[2].uv);

    // Transform the normal to world space using the instance's object-to-world matrix
    float3x4 instanceToWorld = ObjectToWorld3x4();
    float3x3 rotation = (float3x3)instanceToWorld;
    float3x3 transform = transpose(inverse(rotation));
    hitData.normal = normalize(mul(transform, hitData.normal));

    // Retrieve BSDF type from instance data
    hitData.bsdf = hitData.instance.bsdfAlbedoID >> 16;

    // If the material is two-sided, flip the normal if necessary
    if (isBSDFTwoSided(hitData.bsdf))
    {
        if (dot(hitData.normal, WorldRayDirection()) > 0)
        {
            hitData.normal = -hitData.normal;
        }
    }

    // Calculate tangent and binormal to form the TBN matrix
    float3 tangent;
    if (abs(hitData.normal.x) > abs(hitData.normal.y))
    {
        float l = 1.0f / sqrt(hitData.normal.x * hitData.normal.x + hitData.normal.z * hitData.normal.z);
        tangent = float3(hitData.normal.z * l, 0.0f, -hitData.normal.x * l);
    } else
    {
        float l = 1.0f / sqrt(hitData.normal.y * hitData.normal.y + hitData.normal.z * hitData.normal.z);
        tangent = float3(0, hitData.normal.z * l, -hitData.normal.y * l);
    }
    float3 binormal = normalize(cross(hitData.normal, tangent));
    hitData.tbn = float3x3(tangent, binormal, hitData.normal);

    // Retrieve texture ID and sample the albedo texture
    uint albedoTexID = hitData.instance.bsdfAlbedoID & 0xFFFF;
    hitData.albedo = textures[albedoTexID].SampleLevel(samplerState, hitData.uv, 0);

    return hitData;
}

// Pseudo-random number generator using bitwise operations; it updates the rndState and returns a random float in [0, 1)
float rnd(inout uint rndState)
{
    rndState = rndState * 747796405u + 2891336453u;
    uint word = ((rndState >> ((rndState >> 28u) + 4u)) ^ rndState) * 277803737u;
    return (float)((word >> 22u) ^ word) / 4294967296.0;
}

// Tone mapping operator: applies gamma correction (gamma = 2.2)
float3 tmo(float3 p)
{
    return pow(p, 1.0 / 2.2);
}

// Inverse tone mapping (linearizes the colour) by applying inverse gamma
float3 itmo(float3 p)
{
    return pow(p, 2.2);
}

// Ray generation shader that computes primary rays, traces them, and accumulates results
[shader("raygeneration")]
void RayGeneration()
{
    // Get the pixel index and dimensions of the dispatch
    uint2 idx = DispatchRaysIndex().xy;
    float2 size = DispatchRaysDimensions().xy;

    // Initialize the payload with default values
    Payload payload;
    payload.colour = float3(0.0, 0.0, 0.0);
    payload.pathThroughput = float3(1.0, 1.0, 1.0);
    payload.depth = 0;
    payload.flags = 0;
    payload.rndState = DispatchRaysIndex().x ^ (DispatchRaysIndex().y * 0x9e3779b9u) ^ (asuint(SPP) * 0x85ebca6bu);

    // Generate jittered UV coordinates for anti-aliasing
    float2 uv = (idx + float2(rnd(payload.rndState), rnd(payload.rndState))) / size;
    uv.y = 1.0 - uv.y;
    uv = (uv * 2.0) - 1.0;

    // Compute the camera position and the ray direction using inverse matrices
    float3 cameraPosition = mul(inverseView, float4(0, 0, 0, 1)).xyz;
    float4 p = mul(inverseProjection, float4(uv, 0.0, 1.0));
    p.xyz = normalize(p.xyz / p.w);
    float3 rayDirection = mul(inverseView, float4(p.xyz, 0)).xyz;

    // Set up the ray description
    RayDesc ray;
    ray.Origin = cameraPosition;
    ray.Direction = rayDirection;
    ray.TMin = 0.001;
    ray.TMax = 1000;

    // Trace the primary ray
    TraceRay(scene, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

    uav[idx] = float4(tmo(payload.colour), 1.0);
}

// Miss shader: executed when a ray misses all geometry
[shader("miss")]
void Miss(inout Payload payload)
{
    // Only add environment contribution if not a shadow ray
    if (decodeIsShadow(payload.flags) == 0)
    {
        if (payload.depth == 0 || decodeIsSpecular(payload.flags))
        {
            payload.colour = payload.colour + (payload.pathThroughput * evaluateEnvironmentMap(WorldRayDirection()));
        }
    } else
    {
        // If it's a shadow ray, set the throughput to 1 in the red channel
        payload.pathThroughput.r = 1.0f;
    }
}

// Converts spherical coordinates (theta, phi) to a 3D world-space direction
float3 sphericalToWorld(float theta, float phi)
{
    return float3(cos(phi) * sin(theta), sin(phi) * sin(theta), cos(theta));
}

// Returns the polar angle theta from a given direction
float sphericalTheta(float3 wi)
{
    return acos(wi.z);
}

// Returns the azimuthal angle phi from a given direction
float sphericalPhi(float3 wi)
{
    float p = atan2(wi.y, wi.x);
    return (p < 0.0f) ? p + (2.0f * PI) : p;
}

// Samples a cosine-weighted hemisphere using two random numbers
float3 cosineSampleHemisphere(float r1, float r2)
{
    float theta = acos(sqrt(r1));
    float phi = 2.0f * PI * r2;
    return sphericalToWorld(theta, phi);
}

// Computes the probability density function for a cosine-weighted hemisphere sample
float cosineHemispherePDF(float3 wi)
{
    return wi.z / PI;
}

// Uniformly samples a point on the surface of a sphere
float3 uniformSampleSphere(float r1, float r2)
{
    float z = 1.0f - (2.0f * r1);
    float r = sqrt(max(0.0f, 1.0f - z * z));
    float phi = 2.0f * PI * r2;
    float x = r * cos(phi);
    float y = r * sin(phi);
    return float3(x, y, z);
}

// Returns the PDF value for a uniform sphere sample
float uniformSpherePDF()
{
    return 1.0 / (4.0 * PI);
}

// Uniformly samples barycentric coordinates for a triangle
void uniformSampleTriangle(float r1, float r2, out float alpha, out float beta, out float gamma)
{
    alpha = 1.0 - sqrt(r1);
    beta = sqrt(r1) * r2;
    gamma = 1.0f - (alpha + beta);
}

// Randomly selects an area light from the available lights; returns the selected light data and sets the probability mass function (pmf)
AreaLightData sampleLight(inout uint rndState, out float pmf)
{
    uint lightIndex = rnd(rndState) * nLights;
    pmf = 1.0 / (float)nLights;
    return areaLightData[lightIndex];
}

// Samples a point on an area light's triangle and calculates the PDF for that sample
float3 sampleAreaLight(AreaLightData lightData, inout uint rndState, out float pdf)
{
    float u;
    float v;
    float w;
    uniformSampleTriangle(rnd(rndState), rnd(rndState), u, v, w);
    float3 p = lightData.v1 * u + lightData.v2 * v + lightData.v3 * w;
    pdf = 1.0 / (length(cross(lightData.v3 - lightData.v2, lightData.v1 - lightData.v3)) * 0.5f);
    return p;
}

// Samples a new direction based on the material's BSDF; it returns the new world-space direction along with the reflected colour, PDF value, and a flag indicating if the reflection is specular
float3 sampleBSDF(HitData hitData, inout uint rndState, out float3 reflectedColour, out float pdf, out bool isSpecular)
{
    // Convert the outgoing ray direction to local space
    float3 woLocal = mul(-WorldRayDirection(), transpose(hitData.tbn));
    float3 wiLocal;
    isSpecular = false;

    // Different sampling based on the BSDF type
    if (hitData.bsdf == 0) // Diffuse
    {
        wiLocal = cosineSampleHemisphere(rnd(rndState), rnd(rndState));
        reflectedColour = hitData.albedo / PI;
        pdf = cosineHemispherePDF(wiLocal);
    }
    if (hitData.bsdf == 1) // Emission
    {
        wiLocal = woLocal;
        reflectedColour = float3(0.0, 0.0, 0.0);
        pdf = 0;
    }
    if (hitData.bsdf == 2) // Oren-Nayar
    {
        wiLocal = cosineSampleHemisphere(rnd(rndState), rnd(rndState));
        reflectedColour = hitData.albedo / PI;
        pdf = cosineHemispherePDF(wiLocal);
    }
    if (hitData.bsdf == 3) // Mirror
    {
        wiLocal = float3(-woLocal.x, -woLocal.y, woLocal.z);
        reflectedColour = hitData.albedo / wiLocal.z;
        pdf = 1.0f;
        isSpecular = true;
    }
    if (hitData.bsdf == 4) // Glass
    {
        wiLocal = cosineSampleHemisphere(rnd(rndState), rnd(rndState));
        reflectedColour = hitData.albedo / PI;
        pdf = cosineHemispherePDF(wiLocal);
    }
    if (hitData.bsdf == 5) // Plastic
    {
        wiLocal = cosineSampleHemisphere(rnd(rndState), rnd(rndState));
        reflectedColour = hitData.albedo / PI;
        pdf = cosineHemispherePDF(wiLocal);
    }
    if (hitData.bsdf == 6) // Dielectric
    {
        wiLocal = cosineSampleHemisphere(rnd(rndState), rnd(rndState));
        reflectedColour = hitData.albedo / PI;
        pdf = cosineHemispherePDF(wiLocal);
    }
    if (hitData.bsdf == 7) // Conductor
    {
        wiLocal = cosineSampleHemisphere(rnd(rndState), rnd(rndState));
        reflectedColour = hitData.albedo / PI;
        pdf = cosineHemispherePDF(wiLocal);
    }
    // Convert the local direction back to world space
    return normalize(mul(wiLocal, hitData.tbn));
}

// Evaluates the BSDF function for the given hit and incoming light direction
float3 evaluateBSDF(HitData hitData, float3 wi)
{
    float3 woLocal = mul(WorldRayDirection(), hitData.tbn);
    float3 wiLocal = mul(wi, hitData.tbn);
    if (hitData.bsdf == 0) // Diffuse
    {
        return hitData.albedo / PI;
    }
    if (hitData.bsdf == 1) // Emission
    {
        return float3(0.0, 0.0, 0.0);
    }
    if (hitData.bsdf == 2) // Oren-Nayar
    {
        return hitData.albedo / PI;
    }
    if (hitData.bsdf == 3) // Mirror
    {
        return float3(0.0, 0.0, 0.0);
    }
    if (hitData.bsdf == 4) // Glass
    {
        return float3(0.0, 0.0, 0.0);
    }
    if (hitData.bsdf == 5) // Plastic
    {
        return hitData.albedo / PI;
    }
    if (hitData.bsdf == 6) // Dielectric
    {
        return hitData.albedo / PI;
    }
    if (hitData.bsdf == 7) // Conductor
    {
        return hitData.albedo / PI;
    }
    return hitData.albedo / PI;
}

// Checks if the hit corresponds to a light source
bool isLight(HitData hitData)
{
    if (hitData.bsdf == 1)
    {
        return true;
    }
    return false;
}

// Determines visibility between two points by casting a shadow ray; returns true if there is no occluder between the points
bool visible(float3 p1, float3 p2)
{
    float3 dir = p2 - p1;
    float l = length(dir);
    dir = normalize(dir);
    RayDesc shadowRay;
    shadowRay.Origin = p1 + dir * 0.0001;
    shadowRay.Direction = dir;
    shadowRay.TMin = 0.0001;
    shadowRay.TMax = l - 0.0002;

    Payload shadowPayload;
    // Mark this payload as a shadow ray
    shadowPayload.flags = encodeIsShadow(shadowPayload.flags);
    shadowPayload.pathThroughput.r = 0;
    TraceRay(scene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, 0xFF, 0, 0, 0, shadowRay, shadowPayload);
    return shadowPayload.pathThroughput.r > 0;
}

// Calculates the direct lighting contribution at a hit point
float3 calculateDirect(HitData hitData, inout uint rndState)
{
    // If using an environment map and either randomly sampling the environment or if no lights exist
    if (useEnvironmentMap == 1 && (rnd(rndState) < (float)nLights + 1 || nLights == 0))
    {
        // Sample a point on the sphere (environment)
        float pmf = (1.0f / (float)(nLights + 1));
        float3 wi = uniformSampleSphere(rnd(rndState), rnd(rndState));
        float pdf = uniformSpherePDF();
        if (dot(hitData.normal, wi) > 0)
        {
            if (visible(hitData.pos + (wi * 1000), hitData.pos))
            {
                return evaluateEnvironmentMap(wi) * evaluateBSDF(hitData, wi) * dot(hitData.normal, wi) / (pmf * pdf);
            }
        }
    } else
    {
        // Otherwise, sample an area light
        float pmf;
        AreaLightData light = sampleLight(rndState, pmf);
        pmf = (1.0f / (float)(nLights + 1));
        // Sample a point on the light's surface
        float pdf;
        float3 p = sampleAreaLight(light, rndState, pdf);
        // Compute the geometry term
        float3 wi = p - hitData.pos;
        float l = length(wi);
        wi = normalize(wi);
        float GTerm = max(dot(hitData.normal, wi), 0.0) * max(dot(light.normal, -wi), 0.0) / (l * l);
        if (GTerm > 0)
        {
            // Check if the light is visible from the hit point
            if (visible(p, hitData.pos))
            {
                return light.Le * evaluateBSDF(hitData, wi) * GTerm / (pmf * pdf);
            }
        }
    }
    // Return zero contribution if no valid light sample was found
    return float3(0, 0, 0);
}

// Closest hit shader executed when a ray hits geometry
[shader("closesthit")]
void ClosestHit(inout Payload payload, BuiltInTriangleIntersectionAttributes attrib)
{
    // Compute hit data using the intersection attributes
    HitData hitData = calculateHitData(attrib);

    // If the hit object is a light and it's the first bounce, return its emitted light
    if (isLight(hitData) && (payload.depth == 0 || decodeIsSpecular(payload.flags)))
    {
        payload.colour = float3(hitData.instance.bsdfData[0], hitData.instance.bsdfData[1], hitData.instance.bsdfData[2]);
        return;
    }

    // Accumulate direct lighting contribution
    payload.colour = payload.colour + (payload.pathThroughput * calculateDirect(hitData, payload.rndState));

    // Terminate recursion if maximum depth reached
    if (payload.depth == 6)
    {
        return;
    }

    // Apply Russian Roulette termination for deeper bounces
    if (payload.depth > 3)
    {
        float q = min(dot(payload.pathThroughput, float3(0.2126, 0.7152, 0.0722)), 0.7);
        if (rnd(payload.rndState) < q || payload.depth == 7)
        {
            return;
        }
        payload.pathThroughput = payload.pathThroughput / (1.0f - q);
    }

    // Sample indirect illumination from the BSDF
    float3 wi;
    float pdf;
    float3 indirect;
    bool isSpecular;
    wi = sampleBSDF(hitData, payload.rndState, indirect, pdf, isSpecular);

    // Check if pdf is valid
    if (pdf <= 0)
    {
        return;
    }

    // Update the path throughput
    payload.pathThroughput = payload.pathThroughput * indirect * abs(dot(wi, hitData.normal)) / pdf;
    payload.depth = payload.depth + 1;
    if (isSpecular)
    {
        payload.flags = encodeIsSpecular(payload.flags);
    } else
    {
        payload.flags = clearSpecular(payload.flags);
    }

    // Set up the ray for the indirect bounce
    RayDesc ray;
    ray.Origin = hitData.pos + (dot(wi, hitData.normal) > 0 ? hitData.normal : -hitData.normal) * 0.001;
    ray.Direction = wi;
    ray.TMin = 0.001;
    ray.TMax = 1000;

    // Trace the indirect ray
    TraceRay(scene, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);
}
