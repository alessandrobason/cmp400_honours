RWTexture3D<int> tex : register(u0);

#define WIDTH  1024
#define HEIGHT 1024
#define DEPTH  512
#define PRECISION 32

float sdf_sphere(float3 position, float3 centre, float radius) 
{
	return length(position - centre) - radius;
}

[numthreads(4, 4, 4)]
void main(uint3 id : SV_DispatchThreadID) {
    for (int z = 0; z < DEPTH; ++z) {
        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                int3 tex_coord = int3(x, y, z);
                float3 coords = tex_coord;
                coords /= float3(WIDTH, HEIGHT, DEPTH);
                coords *= 10.0;
                float value = sdf_sphere(coords, float3(0, 0, 0), 1.0);
                value *= PRECISION;
                tex[tex_coord] = (int)value;
            }
        }
    }

}