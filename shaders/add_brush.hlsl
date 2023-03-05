// input
Texture3D<int> brush : register(t0);
// output
RWTexture3D<float> tex : register(u0);

cbuffer BrushData : register(b0) {
    /*
    brush size is either:
    - float3 brush_size
    - uint brush_type + uint2 padding
    */
    int3 brush_size;
    uint operation;
    int3 brush_pos;
    int padding__1;
};

#define BRUSH_SIZE (5 * 32)

float sdf_sphere(float3 position, float3 centre, float radius) 
{
	return length(position - centre) - radius;
}

float smin(float a, float b, float k) {
	float h = max(k - abs(a - b), 0.0) / k;
	return min(a, b) - h * h * k * (1.0 / 4.0);
}

// polynomial smooth min
float sminCubic(float a, float b, float k) {
	float h = max(k - abs(a - b), 0.0) / k;
	return min(a, b) - h * h * h * k * (1.0 / 6.0);
}

float sampleBrush(int3 position) {
    // return brush.Load(int4(position - brush_pos, 0));
    float3 pos = position - float3(512, 512, 256);
    return sdf_sphere(pos, brush_pos, BRUSH_SIZE);
    // return brush.Load(int4(position - brush_pos, 0));
}

int3 groupToWorld(int3 group) {
    // convert from group to voxel id
    group = group * 8;
    return group - float3(512, 512, 256);
}

int3 worldToBrush(int3 world) {
    return world - brush_pos;
}

bool isCoordInBrush(int3 coord) {
    return all(coord > 0 && coord < brush_size);
}

[numthreads(8, 8, 8)]
void main(uint3 id : SV_DispatchThreadID, uint3 group_id : SV_GroupID) {
    int3 world = groupToWorld((int3)group_id);
    // int3 pos = worldToBrush(world);
    // int distance = 0;

    int3 centre = world + 4;
    int3 dif = centre - brush_pos;
    float len = length(dif);
    if (len > (BRUSH_SIZE + 12 * 32)) {
        return;
    }

    float distance = sampleBrush(id);
    tex[id] = min(tex[id], distance);
    // switch(operation)
}