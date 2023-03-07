// input
Texture3D<float> brush : register(t0);
// output
RWTexture3D<float> tex : register(u0);

cbuffer VolumeTexData : register(b0) {
    float3 volume_tex_size;
    int padding__0;
};

cbuffer BrushData : register(b1) {
    float3 brush_size;
    uint operation;
    float3 brush_position;
    // TODO maybe scale? maybe K for smooth operations?
    int padding__1;
};

#define SMOOTH_OP               (1 << 31)
#define OP_UNION                (1 << 1)
#define OP_SUBTRACTION          (1 << 2)
#define OP_INTERSECTION         (1 << 3)
#define OP_SMOOTH_UNION         (SMOOTH_OP | OP_UNION)
#define OP_SMOOTH_SUBTRACTION   (SMOOTH_OP | OP_SUBTRACTION)
#define OP_SMOOTH_INTERSECTION  (SMOOTH_OP | OP_INTERSECTION)

float sampleBrush(float3 position) {
    return brush.Load(int4(int3(position), 0));
}

float op_smooth_union(float d1, float d2, float k) {
	float h = clamp(0.5 + 0.5 * (d2 - d1) / k, 0.0, 1.0);
	return lerp(d2, d1, h) - k * h * (1.0 - h);
}

float op_smooth_subtraction(float d1, float d2, float k) {
	float h = clamp(0.5 - 0.5 * (d2 + d1) / k, 0.0, 1.0);
	return lerp(d2, -d1, h) + k * h * (1.0 - h);
}

float op_smooth_intersection(float d1, float d2, float k) {
	float h = clamp(0.5 - 0.5 * (d2 - d1) / k, 0.0, 1.0);
	return lerp(d2, d1, h) + k * h * (1.0 - h);
}

[numthreads(8, 8, 8)]
void main(uint3 id : SV_DispatchThreadID, uint3 group_id : SV_GroupID) {
    // -- first check if the brush is in the tile, otherwise cull
    const float3 group_centre = float3(group_id) * 8. + 4.;
    const float3 group_world = group_centre - volume_tex_size / 2.;
    // convert group position to be in brush coordinates
    const float3 group_brush = group_world - brush_position;
    // if the tile is not inside the brush volume texture, return early
    if (!all(group_brush >= 0 && group_brush < brush_size)) {
        return;
    }

    // -- sample brush at current position
    const float3 id_world = float3(id) - volume_tex_size / 2.;
    const float3 id_brush = id_world - brush_position;
    const float distance = sampleBrush(id_brush);

    // -- do <operation> on the current pixel
    switch (operation) {
        case OP_UNION:               tex[id] = min(tex[id], distance);                         break;              
        case OP_SUBTRACTION:         tex[id] = min(-tex[id], distance);                        break;        
        case OP_INTERSECTION:        tex[id] = max(tex[id], distance);                         break;       
        case OP_SMOOTH_UNION:        tex[id] = op_smooth_union(tex[id], distance, 0.5);        break;       
        case OP_SMOOTH_SUBTRACTION:  tex[id] = op_smooth_subtraction(tex[id], distance, 0.5);  break; 
        case OP_SMOOTH_INTERSECTION: tex[id] = op_smooth_intersection(tex[id], distance, 0.5); break;
    }
}