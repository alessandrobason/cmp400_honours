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
    float smooth_amount;
};

#define SMOOTH_OP               (1 << 31)
#define OP_UNION                (1 << 1)
#define OP_SUBTRACTION          (1 << 2)
// #define OP_INTERSECTION         (1 << 3)
#define OP_SMOOTH_UNION         (SMOOTH_OP | OP_UNION)
#define OP_SMOOTH_SUBTRACTION   (SMOOTH_OP | OP_SUBTRACTION)
// #define OP_SMOOTH_INTERSECTION  (SMOOTH_OP | OP_INTERSECTION)

float sampleBrush(float3 position) {
    return brush.Load(int4(int3(position), 0));
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
    return pos - brush_position + brush_size / 2.;
}

#define GROUP_SIZE 8.
#define THREAD_SIZE 8
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
    setVTSkipRead(id, tex[id], new_value);
}

[numthreads(THREAD_SIZE, THREAD_SIZE, THREAD_SIZE)]
void main(uint3 id : SV_DispatchThreadID, uint3 group_id : SV_GroupID) {
    const int MAX_GROUP_COUNT = 5;
    const int MAX_CELL_MOVE = 8;
    int3 cur_group_id = group_id;
    
    const int3 group_start = group_id * THREAD_SIZE;
    const int3 relative_id = id - group_start;

    const float3 group_centre = float3(group_id) * GROUP_SIZE + GROUP_SIZE * 0.5;
    const float3 group_world = group_centre - volume_tex_size * 0.5;
    const float3 group_brush = worldToBrush(group_world);

    // if the group is outside the brush
    if (!all(group_brush >= 0. && group_brush < brush_size)) {
        float distance = length(group_world - brush_position);
        distance -= brush_size.x * 0.5;
        distance = abs(distance);
        // TODO remove this check, at least outside of the first run. otherwise it is 
        // very expensive (5ms -> 45ms!!!)
        // const float old_value = tex[id];
        setVolumeTexture(id, distance);
        //if (old_value >= MAX_DIST) {
        //    tex[id] = distance;
        //}
        //else if (distance < 100.) {
        //    setVTSkipRead(id, old_value, distance);
        //}
        return;
    }
    
    // -- sample brush at current position
    const float3 id_world = float3(id) - volume_tex_size / 2.;
    // move the position relative to where the brush is, then convert it to brush coordinates
    const float3 id_brush = worldToBrush(id_world);
    // if the cell is not inside the brush volume texture, return early
    if (!all(id_brush >= 0 && id_brush < brush_size)) {
        float distance = length(id_world - brush_position);
        distance -= brush_size.x * 0.5;
        distance = abs(distance);
        //if (distance < 10.) {
            setVolumeTexture(id, distance);
        //
        return;
    }
    const float distance = sampleBrush(id_brush);
    setVolumeTexture(id, distance);

#if 0
    for (int i = 0; i < MAX_GROUP_COUNT; ++i) {
        const float3 group_centre = float3(cur_group_id) * GROUP_SIZE + GROUP_SIZE * 0.5;
        const float3 group_world = group_centre - volume_tex_size * 0.5;
        const float3 group_brush = worldToBrush(group_world);
        if (!all(group_brush >= 0. && group_brush < brush_size)) {
            // move along group
            float3 dir = normalize(brush_position - group_world);
            int3 int_dir = round(dir);
            cur_group_id += int_dir;
            continue;
        }

        if (i > 0) {
            return;
            // ------------------------------------------------------
            // int3 cur_id = cur_group_id * GROUP_SIZE + relative_id;
            // for (int c = 0; c < MAX_CELL_MOVE; ++c) {
            //     // -- sample brush at current position
            //     const float3 id_world = float3(cur_id) - volume_tex_size / 2.;
            //     // move the position relative to where the brush is, then convert it to brush coordinates
            //     const float3 id_brush = worldToBrush(id_world);
            //     // if the cell is not inside the brush volume texture, return early
            //     if (!all(id_brush >= 0 && id_brush < brush_size)) {
            //         float3 dir = normalize(brush_position - id_world);
            //         int3 int_dir = round(dir);
            //         cur_id += int_dir;
            //         continue;
            //     }
            //     const float group_distance = length(float3(cur_group_id - group_id)) * GROUP_SIZE;
            //     const float distance = sampleBrush(id_brush) + group_distance;
            //     if (c > 0) {
            //         tex[id] = length(float3(cur_id - id)) + distance;
            //     }
            //     else {
            //         tex[id] = distance;
            //     }
            // }
            // ------------------------------------------------------
        }
        else {
            // this is the same cell as the beginning
            int3 cur_id = id;
            for (int c = 0; c < MAX_CELL_MOVE; ++c) {
                // -- sample brush at current position
                const float3 id_world = float3(cur_id) - volume_tex_size / 2.;
                // move the position relative to where the brush is, then convert it to brush coordinates
                const float3 id_brush = worldToBrush(id_world);
                // if the cell is not inside the brush volume texture, return early
                if (!all(id_brush >= 0 && id_brush < brush_size)) {
                    float3 dir = normalize(brush_position - id_world);
                    int3 int_dir = int3(dir);
                    cur_id += int_dir;
                    continue;
                }
                const float distance = sampleBrush(id_brush);
                if (c > 0) {
                    tex[id] = length(float3(cur_id - id)) + distance;
                }
                else {
                    tex[id] = distance;
                }
            }
        }
    }
#endif

#if 0
    // -- first check if the brush is in the tile, otherwise cull
    const float3 group_centre = float3(group_id) * 8. + 4.;
    const float3 group_world = group_centre - volume_tex_size / 2.;
    // convert group position to be in brush coordinates
    const float3 group_brush = worldToBrush(group_world);
    // if the tile is not inside the brush volume texture, return early
    if (!all(group_brush >= 0 && group_brush < brush_size)) {
        // if (length(group_brush) > 12.);
        return;
    }

    // -- sample brush at current position
    const float3 id_world = float3(id) - volume_tex_size / 2.;
    // move the position relative to where the brush is, then convert it to brush coordinates
    const float3 id_brush = worldToBrush(id_world);
    // if the cell is not inside the brush volume texture, return early
    if (!all(id_brush >= 0 && id_brush < brush_size)) {
        tex[id] = length(id_brush);
        return;
    }
    const float distance = sampleBrush(id_brush);

    // -- do <operation> on the current pixel
    switch (operation) {
        case OP_UNION:               tex[id] = op_union(tex[id], distance);                             break;              
        case OP_SUBTRACTION:         tex[id] = op_subtraction(tex[id], distance);                       break;        
        case OP_SMOOTH_UNION:        tex[id] = op_smooth_union(tex[id], distance, smooth_amount);       break;       
        case OP_SMOOTH_SUBTRACTION:  tex[id] = op_smooth_subtraction(tex[id], distance, smooth_amount); break; 

        // case OP_INTERSECTION:        tex[id] = op_intersection(tex[id], distance);             break;       
        // case OP_SMOOTH_INTERSECTION: tex[id] = op_smooth_intersection(tex[id], distance, 0.5); break;
    }
#endif
}
