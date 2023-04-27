cbuffer OperationData : register(b0) {
    uint operation;
    float smooth_amount;
    float brush_scale;
    float depth;
};

struct BrushData {
	float3 brush_pos;
	float radius;
	float3 brush_norm;
	float padding__1;
};

// input
Texture3D<int> brush : register(t0);
StructuredBuffer<BrushData> brush_data : register(t1);
// output
RWTexture3D<int> vol_tex : register(u0);

static float3 brush_size = 0;
static float3 volume_tex_size = 0;

#define PRECISION 8.

// operation is a 32 bit unsigned integer used for flags,
// the left-most 3 bits are used as the operation mask
// 1110 0000 0000 0000 0000 0000 0000 0000
// ^--- is smooth
//  ^-- is union
//   ^- is subtraction

#define OP_MASK                 (3758096384)
#define OP_SUBTRACTION          (1 << 29)
#define OP_UNION                (1 << 30)
#define SMOOTH_OP               (1 << 31)
#define OP_SMOOTH_UNION         (SMOOTH_OP | OP_UNION)
#define OP_SMOOTH_SUBTRACTION   (SMOOTH_OP | OP_SUBTRACTION)

float trilinearInterpolation(float3 pos, float3 size) {
    int3 start = max(min(int3(pos), int3(size) - 2), 0);
    int3 end = start + 1;

    float3 delta = pos - start;
    float3 rem = 1 - delta;

#define map(x, y, z) brush.Load(int4((x), (y), (z), 0))

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
    return trilinearInterpolation(position / brush_scale, brush_size);
}

inline float sampleWorld(float3 position) {
    return vol_tex[position];
}

inline void op_union(float vold, float vnew, uint3 id, out bool changed) {
    changed = vnew < vold;
    if (changed) {
        vol_tex[id] = round(vnew);
    }
}

inline void op_subtraction(float vold, float vnew, uint3 id, out bool changed) {
    changed = (-vnew) > vold;
    if (changed) {
        vol_tex[id] = round(-vnew);
    }
}

inline void op_smooth_union(float vold, float vnew, float k, uint3 id, out bool changed) {
	const float h = clamp(0.5 + 0.5 * (vnew - vold) / k, 0.0, 1.0);
	const float result = lerp(vnew, vold, h) - k * h * (1.0 - h);
    // IF SOMETHING IS WRONG CHECK THIS LINE BROTHER vvvvvvv
    // changed = result < vold;
    // if (changed) {
        vol_tex[id] = round(result);
        changed = true;
    // }
}

inline void op_smooth_subtraction(float vold, float vnew, float k, uint3 id, out bool changed) {
	const float h = clamp(0.5 - 0.5 * (vold + vnew) / k, 0.0, 1.0);
	const float result = lerp(vold, -vnew, h) + k * h * (1.0 - h);
    // IF SOMETHING IS WRONG CHECK THIS LINE BROTHER vvvvvvv
    // if (result < vold) {
        changed = true;
        vol_tex[id] = round(result);
    // }
}

inline void setVTSkipRead(uint3 id, float old_value, float new_value, float3 pos) {
	const float ROUGH_MIN_HIT_DISTANCE = 1;
    bool changed;
    
    switch (operation & OP_MASK) {
        case OP_UNION:               op_union(old_value, new_value, id, changed);                             break;                   
        case OP_SUBTRACTION:         op_subtraction(old_value, new_value, id, changed);                       break;       
        case OP_SMOOTH_UNION:        op_smooth_union(old_value, new_value, smooth_amount, id, changed);       break;       
        case OP_SMOOTH_SUBTRACTION:  op_smooth_subtraction(old_value, new_value, smooth_amount, id, changed); break; 
    }
}

inline void setVolumeTexture(uint3 id, float new_value, float3 pos) {
    const float old_value = sampleWorld(id);
    setVTSkipRead(id, old_value, new_value, pos);
}

inline float3 idToWorld(uint3 id) {
    return float3(id) - volume_tex_size * 0.5;
}

inline float3 worldToBrush(float3 pos) {
    return pos - brush_data[0].brush_pos + brush_size * brush_scale * 0.5;
}

inline float texBoundarySDF(float3 pos) {
	float3 q = abs(pos - brush_data[0].brush_pos) - brush_size * brush_scale * 0.5;
	return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

[numthreads(8, 8, 8)]
void main(uint3 id : SV_DispatchThreadID) {
    vol_tex.GetDimensions(volume_tex_size.x, volume_tex_size.y, volume_tex_size.z);
    brush.GetDimensions(brush_size.x, brush_size.y, brush_size.z);
    
    float3 pos = idToWorld(id);
    float dist = texBoundarySDF(pos);

    if (dist > 0) {
        //if (dist < 127) {
        //    bool changed;
        //    op_union(sampleWorld(id), dist, id, changed);
        //}
        return;
    }

    pos = worldToBrush(pos);
    setVolumeTexture(id, sampleBrush(pos), pos);
    return;
}
