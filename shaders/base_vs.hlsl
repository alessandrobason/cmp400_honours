cbuffer MatrixBuffer {
	matrix world_mat;
	matrix view_mat;
	matrix proj_mat;
};

struct VertexInput {
	float4 pos : POSITION;
	float4 col : COLOR;
};

struct PixelInput {
	float4 pos : SV_POSITION;
	float4 col : COLOR;
};

PixelInput main(VertexInput input) {
	PixelInput output;

	input.pos.w = 1.f;	
	
	output.pos = mul(input.pos, world_mat);
	output.pos = mul(output.pos, view_mat);
	output.pos = mul(output.pos, proj_mat);

	output.col = input.col;

	return output;
}
