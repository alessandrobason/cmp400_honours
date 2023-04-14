cbuffer VolumeTexData : register(b0) {
    float3 volume_tex_size;
    int padding__0;
};

cbuffer BrushData : register(b1) {
    float3 brush_size;
    uint operation;
    //float3 brush_position;
    float smooth_amount;
    //float3 padding__1;
    float scale;
    float2 padding__1;
};

struct BrushPosData {
	float3 brush_pos;
	float padding__0;
	float3 brush_norm;
	float padding__1;
};

// input
Texture3D<float> brush : register(t0);
StructuredBuffer<BrushPosData> brush_data : register(t1);
// output
RWTexture3D<float> tex : register(u0);

#define SMOOTH_OP               (1 << 31)
#define OP_UNION                (1 << 1)
#define OP_SUBTRACTION          (1 << 2)
// #define OP_INTERSECTION         (1 << 3)
#define OP_SMOOTH_UNION         (SMOOTH_OP | OP_UNION)
#define OP_SMOOTH_SUBTRACTION   (SMOOTH_OP | OP_SUBTRACTION)
// #define OP_SMOOTH_INTERSECTION  (SMOOTH_OP | OP_INTERSECTION)

float trilinearInterpolation(float3 pos, float3 size, Texture3D<float> tex_to_sample) {
    int3 start = max(min(int3(pos), int3(size) - 2), 0);
    int3 end = start + 1;

    float3 delta = pos - start;
    float3 rem = 1 - delta;

#define map(x, y, z) tex_to_sample.Load(int4((x), (y), (z), 0))

    float4 c = float4(
        map(start.x, start.y, start.z) * rem.x + map(end.x, start.y, start.z) * delta.x,
        map(start.x, end.y,   start.z) * rem.x + map(end.x, end.y,   start.z) * delta.x,
        map(start.x, start.y, end.z)   * rem.x + map(end.x, start.y, end.z)   * delta.x,
        map(start.x, end.y,   end.z)   * rem.x + map(end.x, end.y,   end.z)   * delta.x
    );

#undef map

    float c0 = c.x * rem.y + c.y * delta.y;
    float c1 = c.z * rem.y + c.w * delta.y;

    return c0 * rem.z + c1 * delta.z;
}

#if 0
float trilinearInterpolationRW(float3 pos, float3 size, RWTexture3D<float> tex_to_sample) {
    int3 start = max(min(int3(pos), int3(size) - 2), 0);
    int3 end = start + 1;

    float3 delta = pos - start;
    float3 rem = 1 - delta;

#define map(x, y, z) \
    tex_to_sample.Load(int4(\
        clamp(int3((x), (y), (z)), 0, size), \
        0))

    float4 c = float4(
        map(start.x, start.y, start.z) * rem.x + map(end.x, start.y, start.z) * delta.x,
        map(start.x, end.y,   start.z) * rem.x + map(end.x, end.y,   start.z) * delta.x,
        map(start.x, start.y, end.z)   * rem.x + map(end.x, start.y, end.z)   * delta.x,
        map(start.x, end.y,   end.z)   * rem.x + map(end.x, end.y,   end.z)   * delta.x
    );

#undef map

    float c0 = c.x * rem.y + c.y * delta.y;
    float c1 = c.z * rem.y + c.w * delta.y;

    return c0 * rem.z + c1 * delta.z;
}
#endif

float sampleBrush(float3 position) {
    return trilinearInterpolation(position / scale, brush_size, brush);
}

float sampleWorld(float3 position) {
    //return trilinearInterpolationRW(position, volume_tex_size, tex);
    return tex[position];
}

float op_union(float d1, float d2) {
    return min(d1, d2);
}

float op_subtraction(float d1, float d2) {
    return max(d1, -d2);
}

// float op_intersection(float d1, float d2) {
//     return max(d1, d2);
// }

float op_smooth_union(float d1, float d2, float k) {
	float h = clamp(0.5 + 0.5 * (d2 - d1) / k, 0.0, 1.0);
	return lerp(d2, d1, h) - k * h * (1.0 - h);
}

float op_smooth_subtraction(float d1, float d2, float k) {
	float h = clamp(0.5 - 0.5 * (d1 + d2) / k, 0.0, 1.0);
	return lerp(d1, -d2, h) + k * h * (1.0 - h);
}

// float op_smooth_intersection(float d1, float d2, float k) {
// 	float h = clamp(0.5 - 0.5 * (d2 - d1) / k, 0.0, 1.0);
// 	return lerp(d2, d1, h) + k * h * (1.0 - h);
// }

float3 worldToBrush(float3 pos) {
    return pos - brush_data[0].brush_pos + brush_size * scale * 0.5;
}

#define GROUP_SIZE 8
#define MAX_DIST 10000.

void setVTSkipRead(uint3 id, float old_value, float new_value) {
    if (old_value >= MAX_DIST) {
        tex[id] = new_value;
    }
    else {
        switch (operation) {
            case OP_UNION:               tex[id] = op_union(old_value, new_value);                             break;              
            case OP_SUBTRACTION:         tex[id] = op_subtraction(old_value, new_value);                       break;        
            case OP_SMOOTH_UNION:        tex[id] = op_smooth_union(old_value, new_value, smooth_amount);       break;       
            case OP_SMOOTH_SUBTRACTION:  tex[id] = op_smooth_subtraction(old_value, new_value, smooth_amount); break; 
        }
    }
}

void setVolumeTexture(uint3 id, float new_value) {
    const float old_value = sampleWorld(id);
    setVTSkipRead(id, old_value, new_value);
}

void writeApproximateDistance(float3 id_brush, uint3 id) {
    // clamp the position to the bounds, this way we get a point inside the rect 
    // in the same rough direction as the point
    const float3 sample_pos = clamp(id_brush, 0, brush_size * scale);
    // get the brush value at this edge
    float distance = sampleBrush(sample_pos);
    // then add the distance from this edge to the actual point
    distance += length(id_brush - sample_pos);
    // make the distance slightly smaller, this is to avoid the distance from being
    // way too large and skipping the actual shape. it will slightly lower performance
    // but not by much (hopefully lol)
    distance *= 0.7;
    
    setVolumeTexture(id, distance);
}

//#define NO_EXTRA

[numthreads(8, 8, 8)]
void main(uint3 id : SV_DispatchThreadID, uint3 group_id : SV_GroupID) {
    const float3 group_centre = float3(group_id) * 8. + 4.;
    const float3 group_world = group_centre - volume_tex_size * 0.5;
    const float3 group_brush = worldToBrush(group_world);

    // -- sample brush at current position
    const float3 id_world = float3(id) - volume_tex_size * 0.5;
    // move the position relative to where the brush is, then convert it to brush coordinates
    const float3 id_brush = worldToBrush(id_world);

    // if the group is outside the brush
    if (!all(group_brush >= 0. && group_brush < brush_size * scale)) {
#ifndef NO_EXTRA
        writeApproximateDistance(id_brush, id);
#endif
        return;
    }

    // if the cell is not inside the brush volume texture, return early
    if (!all(id_brush >= 0 && id_brush < brush_size * scale)) {
#ifndef NO_EXTRA
        writeApproximateDistance(id_brush, id);
#endif
        return;
    }

    const float distance = sampleBrush(id_brush);
    setVolumeTexture(id, distance);
}
