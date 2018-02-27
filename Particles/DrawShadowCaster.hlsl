cbuffer CommonApp
{
	float4x4 g_WVP;
}

struct VSInput
{
	float4 pos:POSITION;
};

struct PSInput
{
	float4 pos:SV_Position;
};

struct PSOutput
{
	float4 colour:SV_Target;
};

// This gets called for every vertex which needs to be transformed
void VSMain(const VSInput input, out PSInput output)
{
	// This needs to transform each world position into light space rather than just passing it through
	output.pos = input.pos;
}

// This gets called for every pixel which needs to be drawn
void PSMain(const PSInput input, out PSOutput output)
{
	//This needs to set the alpha channel of each drawn pixel to 1 rather than setting everything to 0
	output.colour = 0;
}
