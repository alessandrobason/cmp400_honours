RWTexture3D<float> tex : register(u0);

#define WIDTH  (64 * 3)
#define HEIGHT (64 * 3)
#define DEPTH  (32 * 3)
#define PRECISION 1

float sdf_sphere(float3 position, float3 centre, float radius) 
{
	return length(position - centre) - radius;
}

[numthreads(4, 4, 4)]
void main(uint3 id : SV_DispatchThreadID) {
    const int z_tile = DEPTH / 4;
    const int y_tile = WIDTH / 4;
    const int x_tile = HEIGHT / 4;

    const int z_start = id.z * z_tile;
    const int z_end = z_start + z_tile;

    const int y_start = id.y * y_tile;
    const int y_end = y_start + y_tile;

    const int x_start = id.x * x_tile;
    const int x_end = x_start + x_tile;

    for (int z = z_start; z < z_end; ++z) {
        for (int y = y_start; y < y_end; ++y) {
            for (int x = x_start; x < x_end; ++x) {
                int3 tex_coord = int3(x, y, z);
                float3 coords = tex_coord;
                coords /= float3(WIDTH, HEIGHT, DEPTH);
                coords = coords * 2.0 - 1.0;
                coords *= 10.0;
                float value = sdf_sphere(coords, float3(0, 0, 0), 5.0);
                // value *= PRECISION;
                tex[tex_coord] = value;
            }
        }
    }
}