cbuffer CommonApp
{
	float4x4 g_WVP;
	float4 g_constantColour;
	float4x4 g_InvXposeW;
	float4x4 g_W;
	float4 g_lightDirections[MAX_NUM_LIGHTS];//(x,y,z,has-direction flag)
	float4 g_lightPositions[MAX_NUM_LIGHTS];//(x,y,z,has-position flag)
	float3 g_lightColours[MAX_NUM_LIGHTS];
	float4 g_lightAttenuations[MAX_NUM_LIGHTS];//(a0,a1,a2,range^2)
	float4 g_lightSpots[MAX_NUM_LIGHTS];//(cos(phi/2),cos(theta/2),1/(cos(theta/2)-cos(phi/2)),falloff)
	int g_numLights;
}

float4 GetLightingColour(float3 worldPos, float3 N)
{
    float4 lightingColour = float4(0, 0, 0, 1);

    for (int i = 0; i < g_numLights; ++i)
    {
        float3 D = g_lightPositions[i].w * (g_lightPositions[i].xyz - worldPos);
        float dotDD = dot(D, D);

        if (dotDD > g_lightAttenuations[i].w)
            continue;

        float atten = 1.0 / (g_lightAttenuations[i].x + g_lightAttenuations[i].y * length(D) + g_lightAttenuations[i].z * dot(D, D));

        float3 L = g_lightDirections[i].xyz;
        float dotNL = g_lightDirections[i].w * saturate(dot(N, L));

        float rho = 0.0;
        if (dotDD > 0.0)
            rho = dot(L, normalize(D));//rho will be zero for point lights

        float spot;
        if (rho > g_lightSpots[i].y)
            spot = 1.0;
        else if(rho < g_lightSpots[i].x)
            spot = 0.0;
        else
            spot = pow((rho - g_lightSpots[i].x) * g_lightSpots[i].z, g_lightSpots[i].w);

        float3 light = atten * spot * g_lightColours[i];
        if (g_lightDirections[i].w > 0.f)
            light *= dotNL;
        else
            light *= saturate(dot(N, normalize(D)));

        lightingColour.xyz += light;
    }

    return lightingColour;
}


cbuffer DrawHeightMap
{
	float4x4 g_shadowMatrix;
	float4 g_shadowColour;
}

Texture2D g_shadowTexture;
SamplerState g_shadowSampler;

struct VSInput
{
	float4 pos:POSITION;
	float4 colour:COLOUR0;
	float3 normal:NORMAL;
};

struct PSInput
{
	float4 pos:SV_Position;
	float4 colour:COLOUR0;
	float4 worldPos:TEXCOORD0;

};

struct PSOutput
{
	float4 colour:SV_Target;
};

// This gets called for every vertex which needs to be transformed
void VSMain(const VSInput input, out PSInput output)
{
	output.pos = mul(input.pos, g_WVP);

	output.worldPos = input.pos;

	float3 worldNormal = mul(input.normal, g_InvXposeW);

	output.colour = input.colour * GetLightingColour(output.worldPos, normalize(worldNormal));
}

// This gets called for every pixel which needs to be drawn
void PSMain(const PSInput input, out PSOutput output)
{
	output.colour = input.colour;

	// Transform the pixel into light space

	// Perform perspective correction

	// Scale and offset uvs into 0-1 range.

	// Sample render target to see if this pixel is in shadow

	// If it is then alpha blend between final colour and shadow colour
}


