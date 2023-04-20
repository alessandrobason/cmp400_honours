cbuffer OperationData : register(b0) {
    uint operation;
    float smooth_amount;
    float brush_scale;
    float depth;
};

struct BrushData {
	float3 brush_pos;
	float padding__0;
	float3 brush_norm;
	float padding__1;
};

// input
Texture3D<float> brush : register(t0);
StructuredBuffer<BrushData> brush_data : register(t1);
// output
RWTexture3D<float> vol_tex : register(u0);

static float3 brush_size = 0;
static float3 volume_tex_size = 0;

#define MAX_DIST 10000.

#define SMOOTH_OP               (1 << 31)
#define OP_UNION                (1 << 1)
#define OP_SUBTRACTION          (1 << 2)
#define OP_SMOOTH_UNION         (SMOOTH_OP | OP_UNION)
#define OP_SMOOTH_SUBTRACTION   (SMOOTH_OP | OP_SUBTRACTION)

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

inline float sampleBrush(float3 position) {
    return trilinearInterpolation(position / brush_scale, brush_size, brush);
}

inline float sampleWorld(float3 position) {
    return vol_tex[position];
}

inline void op_union(float vold, float vnew, uint3 id) {
    if (vnew < vold) {
        vol_tex[id] = vnew;
    }
}

inline void op_subtraction(float vold, float vnew, uint3 id) {
    if ((-vnew) > vold) {
        vol_tex[id] = -vnew;
    }
}

inline void op_smooth_union(float vold, float vnew, float k, uint3 id) {
    op_subtraction(vold, vnew, id);
    return;
    k = 500.;
	const float h = clamp(0.5 + 0.5 * (vnew - vold) / k, 0.0, 1.0);
	const float result = lerp(vnew, vold, h) - k * h * (1.0 - h);
    // IF SOMETHING IS WRONG CHECK THIS LINE BROTHER vvvvvvv
    if (result < vold) {
        vol_tex[id] = result;
    }
}

inline void op_smooth_subtraction(float vold, float vnew, float k, uint3 id) {
	const float h = clamp(0.5 - 0.5 * (vold + vnew) / k, 0.0, 1.0);
	const float result = lerp(vold, -vnew, h) + k * h * (1.0 - h);
    // IF SOMETHING IS WRONG CHECK THIS LINE BROTHER vvvvvvv
    if (result < vold) {
        vol_tex[id] = result;
    }
}

inline void setVTSkipRead(uint3 id, float old_value, float new_value) {
    switch (operation) {
        case OP_UNION:               op_union(old_value, new_value, id);                             break;                   
        case OP_SUBTRACTION:         op_subtraction(old_value, new_value, id);                       break;       
        case OP_SMOOTH_UNION:        op_smooth_union(old_value, new_value, smooth_amount, id);       break;       
        case OP_SMOOTH_SUBTRACTION:  op_smooth_subtraction(old_value, new_value, smooth_amount, id); break; 
    }
}

inline void setVolumeTexture(uint3 id, float new_value) {
    const float old_value = sampleWorld(id);
    setVTSkipRead(id, old_value, new_value);
}

inline bool isOutsideTexture(float3 pos) {
    return any(pos < 0) || any(pos >= brush_size * brush_scale);
}

inline float3 idToWorld(uint3 id) {
    return float3(id) - volume_tex_size * 0.5;
}

inline float3 worldToBrush(float3 pos) {
    return pos - brush_data[0].brush_pos + brush_size * brush_scale * 0.5;
}

inline void writeApproximateDistance(float3 pos, uint3 id) {
    // clamp the position to the bounds, this way we get a point inside the rect 
    // in the same rough direction as the point
    const float3 edge_pos = clamp(pos, 0, brush_size * brush_scale);
    // get the brush value at this edge
    float distance = sampleBrush(edge_pos);
    // then add the distance from this edge to the actual point
    distance += length(pos - edge_pos);
    // make the distance slightly smaller, this is to avoid the distance from being
    // way too large and skipping the actual shape. it will slightly lower performance
    // but not by much (hopefully lol)
    distance *= 0.7;
    
    op_union(sampleWorld(id), distance, id);
}

[numthreads(8, 8, 8)]
void main(uint3 id : SV_DispatchThreadID) {
    vol_tex.GetDimensions(volume_tex_size.x, volume_tex_size.y, volume_tex_size.z);
    brush.GetDimensions(brush_size.x, brush_size.y, brush_size.z);
    
    const float3 pos = worldToBrush(idToWorld(id));

    // if the cell is outside the brush
    if (isOutsideTexture(pos)) {
        writeApproximateDistance(pos, id);
        return;
    }

    setVolumeTexture(id, sampleBrush(pos));
}
