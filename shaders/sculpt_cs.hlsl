#include "shaders/common.hlsl"

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
Texture3D<snorm float> brush : register(t0);
StructuredBuffer<BrushData> brush_data : register(t1);
// output
RWTexture3D<snorm float> vol_tex : register(u0);

static float3 brush_size = 0;
static float3 volume_tex_size = 0;

// operation is a 32 bit unsigned integer used for flags,
// the left-most bit is used to flag if the operation is smooth
// the other bits are used as flag for the operation
// 1000 0000 0000 0000 0000 0000 0000 0011
// ^--- is smooth            is union ---^
//                    is subtraction ---^

#define OP_UNION                (1)
#define OP_SUBTRACTION          (2)
#define SMOOTH_OP               (1 << 31)
#define OP_SMOOTH_UNION         (SMOOTH_OP | OP_UNION)
#define OP_SMOOTH_SUBTRACTION   (SMOOTH_OP | OP_SUBTRACTION)

inline float sampleBrush(float3 position) {
    return trilinearInterpolation(position / brush_scale, brush_size, brush);
}

inline float sampleWorld(float3 position) {
    return vol_tex[position];
}

inline void op_union(float vold, float vnew, uint3 id, out bool changed) {
    changed = vnew < vold;
    if (changed) {
        vol_tex[id] = vnew;
    }
}

inline void op_subtraction(float vold, float vnew, uint3 id, out bool changed) {
    changed = (-vnew) > vold;
    if (changed) {
        vol_tex[id] = -vnew;
    }
}

inline void op_smooth_union(float vold, float vnew, float k, uint3 id, out bool changed) {
	const float h = clamp(0.5 + 0.5 * (vnew - vold) / k, 0.0, 1.0);
	const float result = lerp(vnew, vold, h) - k * h * (1.0 - h);
    vol_tex[id] = result;
    changed = true;
}

inline void op_smooth_subtraction(float vold, float vnew, float k, uint3 id, out bool changed) {
	const float h = clamp(0.5 - 0.5 * (vold + vnew) / k, 0.0, 1.0);
	const float result = lerp(vold, -vnew, h) + k * h * (1.0 - h);
    changed = true;
    vol_tex[id] = result;
}

inline void setVTSkipRead(uint3 id, float old_value, float new_value, float3 pos) {
    bool changed;
    
    switch (operation) {
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

inline void writeApproximateDistance(float3 pos, uint3 id) {
    // clamp the position to the bounds, this way we get a point inside the rect 
    // in the same rough direction as the point
    const float3 edge_pos = clamp(pos, 0, brush_size * brush_scale);
    // get the brush value at this edge
    float distance = sampleBrush(edge_pos) * MAX_STEP;
    // then add the distance from this edge to the actual point
    distance += length(pos - edge_pos);
    // make the distance slightly smaller, this is to avoid the distance from being
    // way too large and skipping the actual shape. it will slightly lower performance
    // but not by much (hopefully lol)
    distance *= 0.9;
    
    bool changed;
    op_union(sampleWorld(id), distance / MAX_STEP, id, changed);
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
    float dist_from_tex = texBoundarySDF(pos);
    pos = worldToBrush(pos);

    if (dist_from_tex > 0) {
        if (dist_from_tex < MAX_STEP && operation & OP_UNION) {
            writeApproximateDistance(pos, id);
        }
        return;
    }

    setVolumeTexture(id, sampleBrush(pos), pos);
}
