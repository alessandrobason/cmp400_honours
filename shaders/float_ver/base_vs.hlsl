struct VertexInput {
	float4 pos : POSITION;
	float2 uv : TEXCOORDS0;
};

struct PixelInput {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORDS0;
};

PixelInput main(VertexInput input) {
	PixelInput output;
	output.pos = input.pos;
	output.uv  = input.uv;
	return output;
}
