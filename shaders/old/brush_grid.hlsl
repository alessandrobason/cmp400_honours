#define MAX_SDF_DISTANCE 10000000000.0

RWTexture3D<float> tex : register(u0);

cbuffer SDFData : register(b0)  {
    float3 tex_size;
    float padding__0;
};

float sdf_sphere(float3 position, float3 centre, float radius) 
{
	return length(position - centre) - radius;
}

float sampleBrush(float3 position) {
    return sdf_sphere(position, float3(0, 0, 0), 6.0 * 32.);
}

void setTex(float3 p) {
    // float3 c = (p / tex_size * 2.0 - 1.0) * 10.0;
    float3 c = p;
    tex[p] = sampleBrush(c);
}

int3 voxelToWorld(int3 id) {
    // move it half the volume size back, this way it is in the range (-half, +half)
    const int3 tex_half = (uint3)tex_size / 2;
    id = id - tex_half;
    return id;
}

// group is the id of each kernel, each kernel has a range of (-4, +4) voxels
// and every voxel has a precision of 32 (256 / 8)
// this function doesn't return subvoxel precision
int3 groupToWorld(int3 group) {
    // convert from group to voxel id
    group = group * 8;
    return voxelToWorld(group);
}

/*
1 ## BRUSH GRID
  1. sample brush at tile centre
    - if sdf > grid bounds + 4
      -> discard
    - else
      -> add + store in GSM
  2. loop through brushes in GSM
    - sample at cell centre
    - if accepted
      -> store to grid (linear)
*/

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

    int3 world = groupToWorld((int3)group_id);
    float distance = sampleBrush((float3)world);
    
    // // ignore if the sdf is farther than the kernel boundaries (8 voxels) + 4 voxels
    // if (distance > (8 + 4)) {
    //     tex[id] = distance;
    //     return;
    // }

    int3 voxel_world = voxelToWorld((int3)id);
    distance = sampleBrush((float3)voxel_world);
    tex[id] = distance;
    return;

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