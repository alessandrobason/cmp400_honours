struct PixelInput {
	float4 pos : SV_POSITION;
	float4 col : COLOR;
};

float4 main(PixelInput input) : SV_TARGET{
	//return abs(input.pos);
	return float4(1, 0, 0, 1);
}