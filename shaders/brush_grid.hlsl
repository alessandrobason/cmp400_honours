RWTexture3D<float> tex : register(u0);

#define WIDTH  (1024)
#define HEIGHT (1024)
#define DEPTH  (512)
// #define PRECISION 1

float sdf_sphere(float3 position, float3 centre, float radius) 
{
	return length(position - centre) - radius;
}

float sampleBrush(float3 position) {
    return sdf_sphere(position, float3(0, 0, 0), 5.0);
}

void setTex(float3 p) {
    float3 c = (p / float3(WIDTH, HEIGHT, DEPTH) * 2.0 - 1.0) * 10.0;
    tex[p] = sampleBrush(c);
}

[numthreads(8, 8, 8)]
void main(uint3 id : SV_DispatchThreadID, uint3 group_id : SV_GroupID) {
#if 0
    // tex[id] = (float)(id.x + id.y + id.z);
    const float3 centre = float3(group_id.x, group_id.y, group_id.z) * 16. - 8.;

    // const float3 centre = float3(id.x, id.y, id.z);
    const float value_at_centre = sampleBrush(centre);

    // if the sdf value is more than tile bounds (16) + 4
    if (value_at_centre > 20.) {
        // ignore brush, for now it is only one so just return
        return;
    }

#endif

    const float3 pos = float3(id.x, id.y, id.z) * 2.0;

    setTex(pos);
    setTex(pos + float3(0, 0, 1));
    setTex(pos + float3(0, 1, 0));
    setTex(pos + float3(0, 1, 1));
    setTex(pos + float3(1, 0, 0));
    setTex(pos + float3(1, 0, 1));
    setTex(pos + float3(1, 1, 0));
    setTex(pos + float3(1, 1, 1));

    // tex[float3(+id.x, +id.y, +id.z)] = sampleBrush(float3(+id.x, +id.y, +id.z));
    // tex[float3(+id.x, +id.y, -id.z)] = sampleBrush(float3(+id.x, +id.y, -id.z));
    // tex[float3(+id.x, -id.y, +id.z)] = sampleBrush(float3(+id.x, -id.y, +id.z));
    // tex[float3(+id.x, -id.y, -id.z)] = sampleBrush(float3(+id.x, -id.y, -id.z));
    // tex[float3(-id.x, +id.y, +id.z)] = sampleBrush(float3(-id.x, +id.y, +id.z));
    // tex[float3(-id.x, +id.y, -id.z)] = sampleBrush(float3(-id.x, +id.y, -id.z));
    // tex[float3(-id.x, -id.y, +id.z)] = sampleBrush(float3(-id.x, -id.y, +id.z));
    // tex[float3(-id.x, -id.y, -id.z)] = sampleBrush(float3(-id.x, -id.y, -id.z));

#if 0
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
#endif
}