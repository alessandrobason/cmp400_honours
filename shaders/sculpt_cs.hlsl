cbuffer OperationData : register(b0) {
    uint operation;
    float smooth_amount;
    float brush_scale;
    float depth;
    float3 colour;
    float padding__0;
};

struct BrushData {
	float3 brush_pos;
	float radius;
	float3 brush_norm;
	float padding__1;
};

// input
Texture3D<float> brush : register(t0);
StructuredBuffer<BrushData> brush_data : register(t1);
// StructuredBuffer<uint> material : register(t2);
// output
RWTexture3D<float> vol_tex : register(u0);
// RWTexture3D<float3> material_tex : register(u1);

static float3 brush_size = 0;
static float3 volume_tex_size = 0;

#define MAX_DIST 10000.

// operation is a 32 bit unsigned integer used for flags,
// the left-most 3 bits are used as the operation mask
// 1110 0000 0000 0000 0000 0000 0000 0000
// ^--- is smooth
//  ^-- is union
//   ^- is subtraction
// then, there are 8 bits used for the material index
// 0000 0000 0000 0000 0000 0000 1111 1111

#define OP_MASK                 (3758096384)
#define OP_SUBTRACTION          (1 << 29)
#define OP_UNION                (1 << 30)
#define SMOOTH_OP               (1 << 31)
#define OP_SMOOTH_UNION         (SMOOTH_OP | OP_UNION)
#define OP_SMOOTH_SUBTRACTION   (SMOOTH_OP | OP_SUBTRACTION)

// #define MAT_MASK                (255)

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
    // IF SOMETHING IS WRONG CHECK THIS LINE BROTHER vvvvvvv
    // changed = result < vold;
    // if (changed) {
        vol_tex[id] = result;
        changed = true;
    // }
}

inline void op_smooth_subtraction(float vold, float vnew, float k, uint3 id, out bool changed) {
	const float h = clamp(0.5 - 0.5 * (vold + vnew) / k, 0.0, 1.0);
	const float result = lerp(vold, -vnew, h) + k * h * (1.0 - h);
    // IF SOMETHING IS WRONG CHECK THIS LINE BROTHER vvvvvvv
    // if (result < vold) {
        changed = true;
        vol_tex[id] = result;
    // }
}

// #define FIRST_INDEX_MASK    (255)
// #define FIRST_INDEX_SHIFT   (0)
// #define SECOND_INDEX_MASK   (65280)
// #define SECOND_INDEX_SHIFT  (8)
// #define WEIGHT_MAX          (65536.)

// #define GET_FIRST_INDEX(ind)  ((ind & FIRST_INDEX_MASK)  >> FIRST_INDEX_SHIFT)
// #define GET_SECOND_INDEX(ind) ((ind & SECOND_INDEX_MASK) >> SECOND_INDEX_SHIFT)

// #define SET_MATERIAL(id1, id2, w) uint2((w), (id1) | ((id2) << SECOND_INDEX_SHIFT))

inline void setVTSkipRead(uint3 id, float old_value, float new_value, float3 pos) {
	const float ROUGH_MIN_HIT_DISTANCE = 1;
    bool changed;
    
    switch (operation & OP_MASK) {
        case OP_UNION:               op_union(old_value, new_value, id, changed);                             break;                   
        case OP_SUBTRACTION:         op_subtraction(old_value, new_value, id, changed);                       break;       
        case OP_SMOOTH_UNION:        op_smooth_union(old_value, new_value, smooth_amount, id, changed);       break;       
        case OP_SMOOTH_SUBTRACTION:  op_smooth_subtraction(old_value, new_value, smooth_amount, id, changed); break; 
    }

#if 0
    if (changed && new_value < ROUGH_MIN_HIT_DISTANCE) {
        const float3 old_colour = material_tex[id];
        if (all(old_colour == 0)) {
            material_tex[id] = colour;
        }
        else {
            const float weight = length(pos) / brush_size.x;
            material_tex[id] = lerp(colour, old_colour, weight);
        }
#if 0
        uint2 previous_mat = material_tex[id];
        // uint previous_material = material_tex[id];
        uint id1       = GET_FIRST_INDEX(previous_mat.y);
        uint id2       = GET_SECOND_INDEX(previous_mat.y);
        uint weight    = previous_mat.x;

        uint new_id = operation & MAT_MASK;

        // if one of the id is the same as this new id, set its weight to
        // half of what the other id is taking

        if (new_id == id1) {
            uint weight_rem = weight - WEIGHT_MAX;
            uint weight_delta = weight_rem / 2;
            weight += weight_delta;
        }
        else if (new_id == id2) {
            uint weight_delta = weight / 2;
            weight -= weight_delta;
        }
        // otherwise swap the smaller id for the new one
        else {
            // id1 not set
            if (id1 == 0) {
                weight = WEIGHT_MAX;
                id1 = new_id;
            }
            // id2 not set
            else if (id2 == 0) {
                id2 = new_id;
                weight /= 2;
            }
            // id1 > id2
            else if (weight > (WEIGHT_MAX / 2.)) {
                id2 = new_id;
            }
            // id2 > id1
            else {
                id1 = new_id;
            }
        }
        
        // material_tex[id] = operation & MAT_MASK;
        material_tex[id] = SET_MATERIAL(id1, id2, weight);
#endif
    }
#endif
}

inline void setVolumeTexture(uint3 id, float new_value, float3 pos) {
    const float old_value = sampleWorld(id);
    setVTSkipRead(id, old_value, new_value, pos);
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
    
    bool changed;
    op_union(sampleWorld(id), distance, id, changed);
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

    setVolumeTexture(id, sampleBrush(pos), pos);
}
