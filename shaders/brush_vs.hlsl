struct VertexInput {
	float4 pos : POSITION;
	float2 uv : TEXCOORDS0;
};

struct PixelInput {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORDS0;
};

StructuredBuffer<matrix> model_matrix;

PixelInput main(VertexInput input) {
	PixelInput output;
	output.pos = mul(input.pos, model_matrix[0]);
	output.pos /= output.pos.w;
	output.uv  = input.uv;
	return output;
}
