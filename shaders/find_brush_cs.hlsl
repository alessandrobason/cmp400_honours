Texture3D<float> vol_tex : register(t0);

cbuffer FindData : register(b0) {
    // float3 cam_pos;
    // float padding__0;
    // float3 mouse_dir;
    // float padding__1;
    float3 pos;
    float mouse_dir_x;
    float3 right;
    float mouse_dir_y;
    float3 up;
    float mouse_dir_z;
    float3 fwd;
    float padding__0;
};

cbuffer TexData : register(b1) {
    float3 tex_size;
    float padding__2;
	float3 tex_position;
	float padding__3;
};

struct BrushPositionData {
    float3 position;
    float padding__4;
    float3 normal;
    float padding__5;
};

// RWStructuredBuffer<BrushPositionData> brush_data;
RWStructuredBuffer<matrix> model_matrix;

float3 worldToTex(float3 world) {
	return world - tex_position + tex_size / 2.;
}

float trilinearInterpolation(float3 pos, float3 size) {
    const int3 start = max(min(int3(pos), int3(size) - 2), 0);
    const int3 end = start + 1;

    const float3 delta = pos - start;
    const float3 rem = 1 - delta;

#define map(x, y, z) vol_tex.Load(int4((x), (y), (z), 0))

	const float4 map_start = float4(
		map(start.x, start.y, start.z),
		map(start.x, end.y,   start.z),
		map(start.x, start.y, end.z),
		map(start.x, end.y,   end.z)
	);

	const float4 map_end = float4(
		map(end.x, start.y, start.z),
		map(end.x, end.y,   start.z),
		map(end.x, start.y, end.z),  
		map(end.x, end.y,   end.z)  
	);

	const float4 c = map_start * rem.x + map_end * delta.x;

#undef map

    const float c0 = c.x * rem.y + c.y * delta.y;
    const float c1 = c.z * rem.y + c.w * delta.y;

    return c0 * rem.z + c1 * delta.z;
}

float map(float3 coords) {
	return trilinearInterpolation(coords, tex_size);
}

float3 calcNormal(float3 pos) {
	const float step = 1;
	const float2 k = float2(1, -1);

	return normalize(
		k.xyy * map(pos + k.xyy * step) +
		k.yyx * map(pos + k.yyx * step) +
		k.yxy * map(pos + k.yxy * step) +
		k.xxx * map(pos + k.xxx * step)
	);
}

void rayMarch(float3 ro, float3 rd, out float3 normal, out float3 pos) {
    float t = 0;
    const int MAX_STEPS = 300;
    const int MIN_DISTANCE = 0.005;
    const int MAX_DISTANCE = 0.005;

    for (int i = 0; i < MAX_STEPS; ++i) {
        float3 p = ro + rd * t;
        float closest = 0;

        float3 tex_pos = worldToTex(p);
		if (!all(tex_pos >= 0 && tex_pos < tex_size)) {
			float distance = length(p - tex_position) - tex_size.x * 0.5;
			closest = max(abs(distance), 1);
		}
		else {
			closest = map(tex_pos);
		}

        if (closest < MIN_DISTANCE) {
            normal = calcNormal(tex_pos);
            pos = p;
            return;
        }

        if (t > MAX_DISTANCE) {
            break;
        }

        t += closest;
    }

    normal = 0;
    pos = 0;
}

matrix perspective(float fovy, float aspect, float near, float far) {
    float f = 1 / tan(fovy * 0.5);
    float fn = 1 / (near - far);

    matrix dest = 0;
    dest[0][0] = f / aspect;
    dest[1][1] = f;
    dest[2][2] = (near + far) * fn;
    dest[2][3] = -1.0;
    dest[3][2] = 2.0 * near * far * fn;
    dest[3][3] = 1;
    return dest;
}

matrix ortho(float left, float right, float bottom, float top, float near, float far) {
    matrix dest = 0;
   
	dest[0][0] = 2.f / (right - left);
	dest[1][1] = 2.0f / (top - bottom);
	dest[2][2] = 2.0f / (near - far);
	dest[3][3] = 1.0f;

	dest[3][0] = (left + right) / (left - right);
	dest[3][1] = (bottom + top) / (bottom - top);
	dest[3][2] = (far + near) / (near - far); 

    return dest;
}

[numthreads(1, 1, 1)]
void main() {
    float3 position, normal;

    float3 mouse_dir = float3(mouse_dir_x, mouse_dir_y, mouse_dir_z);
    
    // raymarch in the direction until something is found
    rayMarch(pos, mouse_dir, normal, position);

    position = float3(1920, 1080, 0) / 2.;

    matrix result = matrix(
        1.0, 0.0, 0.0, 0.0, // position.x,
        0.0, 1.0, 0.0, 0.0, // position.y,
        0.0, 0.0, 1.0, 0.0, // position.z,
        // 0.0, 0.0, 0.0, 1.0
        position.x, position.y, position.z, 1.0
    );

    // matrix persp = perspective(3.14 / 2.5, 16/9, 0.1, 1000.0);
    matrix persp = ortho(0, 1920, 0, 1080, 0.01, 1000.0);

    model_matrix[0] = mul(persp, result);
}