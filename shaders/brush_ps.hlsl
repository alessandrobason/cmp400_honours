struct PixelInput {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORDS0;
};

Texture2D brush_image;
sampler sampler_0;

float4 main(PixelInput input) : SV_TARGET {
    float4 colour = brush_image.Sample(sampler_0, input.uv);
    // if (colour.a < 0.5) discard;
    return colour;
}