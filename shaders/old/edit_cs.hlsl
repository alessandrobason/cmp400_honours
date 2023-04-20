struct ShapeData {
	uint type;
	float3 centre;
	float4 data_0;
	float3 data_1;
	uint parent;
};

// output texture
RWTexture3D<float> tex : register(u0);
// brush data
StructuredBuffer<ShapeData> shape_data : register(t0);
// sdf volume texture data
cbuffer SDFinfo : register(b0) {
	int3 half_tex_size;
	uint padding__0;
};

/* 
uint shape structure:

first 3 bits are for alterations
 - elongated subtracted onioned

then 1 bit to check if the next operation is smooth or not
 - smooth

then 3 bits for operations
 - union subtraction intersection

*/

#define SPHERE           (1)
#define BOX              (2)
#define CONE             (3)
#define CYLINDER         (4)
#define PYRAMID          (5)

#define ALTERATION_MASK  (0xE0000000)
#define SMOOTH_MASK      (0x10000000)
#define OPERATION_MASK   (0x0E000000)
#define SHAPE_MASK       (0x01FFFFFF)
#define MAX_SHAPE_CONST  (33554431)

#define ELONGATED        (1 << 31)
#define ROUNDED          (1 << 30)
#define ONIONED          (1 << 29)

#define SMOOTH_OP        (1 << 28)

#define OP_UNION         (1 << 27)
#define OP_SUBTRACTION   (1 << 26)
#define OP_INTERSECTION  (1 << 25)
#define OP_SMOOTH_UNION         (SMOOTH_MASK | OP_UNION)
#define OP_SMOOTH_SUBTRACTION   (SMOOTH_MASK | OP_SUBTRACTION)
#define OP_SMOOTH_INTERSECTION  (SMOOTH_MASK | OP_INTERSECTION)

// == ALTERATIONS ======================================================

// TODO
float4 alt_elongate(float3 p, float3 h) {
	float3 q = abs(p) - h;
	return float4(max(q, 0.0), min(max(q.x, max(q.y, q.z)), 0.0));
}

float alt_round(float sdf, float rad) {
	return sdf - rad;
}

float alt_onion(float sdf, float thickness) {
	return abs(sdf) - thickness;
}

// == OPERATIONS =======================================================

float op_union(float d1, float d2) {
	return min(d1, d2);
}

float op_subtraction(float d1, float d2) {
	return max(-d1, d2);
}

float op_intersection(float d1, float d2) {
	return max(d1, d2);
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

// == SHAPES ===========================================================

float sdf_sphere(float3 position, float3 centre, float radius) {
	return length(position - centre) - radius;
}

float sdf_box(float3 position, float3 centre, float3 size) {
	float3 q = abs(position - centre) - size;
	return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float sdf_cone(float3 position, float3 centre, float2 angle_cos_sin, float height) {
	float2 q = height * float2(angle_cos_sin.x / angle_cos_sin.y, -1.0);
    
	position -= centre;
	float2 w = float2(length(position.xz), position.y);
	float2 a = w - q * clamp(dot(w, q) / dot(q, q), 0.0, 1.0);
	float2 b = w - q * float2(clamp(w.x / q.x, 0.0, 1.0), 1.0);
	float k = sign(q.y);
	float d = min(dot(a, a), dot(b, b));
	float s = max(k * (w.x * q.y - w.y * q.x), k * (w.y - q.y));
	return sqrt(d) * sign(s);
}

float sdf_cylinder(float3 position, float3 centre, float3 bottom, float3 top, float radius){
	position -= centre;
	float3 ba = top - bottom;
	float3 pa = position - bottom;
	float baba = dot(ba, ba);
	float paba = dot(pa, ba);
	float x = length(pa * baba - ba * paba) - radius * baba;
	float y = abs(paba - baba * 0.5) - baba * 0.5;
	float x2 = x * x;
	float y2 = y * y * baba;
	float d = (max(x, y) < 0.0) ? -min(x2, y2) : (((x > 0.0) ? x2 : 0.0) + ((y > 0.0) ? y2 : 0.0));
	return sign(d) * sqrt(abs(d)) / baba;
}

float sdf_pyramid(float3 position, float3 centre, float height) {
	float m2 = height * height + 0.25;
    
	position -= centre;
	position.xz = abs(position.xz);
	position.xz = (position.z > position.x) ? position.zx : position.xz;
	position.xz -= 0.5;

	float3 q = float3(
		position.z, 
		height * position.y - 0.5 * position.x, 
		height * position.x + 0.5 * position.y
	);
   
	float s = max(-q.x, 0.0);
	float t = clamp((q.y - 0.5 * position.z) / (m2 + 0.25), 0.0, 1.0);
    
	float a = m2 * (q.x + s) * (q.x + s) + q.y * q.y;
	float b = m2 * (q.x + 0.5 * t) * (q.x + 0.5 * t) + (q.y - m2 * t) * (q.y - m2 * t);
    
	float d2 = min(q.y, -q.x * m2 - q.y * 0.5) > 0.0 ? 0.0 : min(a, b);
    
	return sqrt((d2 + q.z * q.z) / m2) * sign(max(q.z, -position.y));
}

// == RAY MARCHING =====================================================

float mapShape(float3 position, int id) {
	uint type = shape_data[id].type;

	uint alteration = type & ALTERATION_MASK;
	uint operation  = type & (SMOOTH_MASK | OPERATION_MASK);
	uint shape      = type & SHAPE_MASK;

	float result = 0;

	switch (shape) {
		case SPHERE:
			result = sdf_sphere(position, shape_data[id].centre, shape_data[id].data_0.x);
			break;
		case BOX:
			result = sdf_box(position, shape_data[id].centre, shape_data[id].data_0.xyz);
			break;
		case CONE:
			result = sdf_cone(position, shape_data[id].centre, shape_data[id].data_0.xy, shape_data[id].data_0.z);
			break;
		case CYLINDER:
			result = sdf_cylinder(position, shape_data[id].centre, shape_data[id].data_0.xyz, shape_data[id].data_1.xyz, shape_data[id].data_0.w);
			break;
		case PYRAMID:
			result = sdf_pyramid(position, shape_data[id].centre, shape_data[id].data_0.x);
			break;
	}

// TODO
#if 0
	switch (alteration) {
		case ELONGATED:
			break;
		case ROUNDED:
			break;
		case ONIONED:
			break;
	}

	switch (operation) {
		case OP_UNION:
			op_union(parent_value, result);
			break;
		case OP_SUBTRACTION: 
			break;
		case OP_INTERSECTION: 
			break;
		case OP_SMOOTH_UNION: 
			break;
		case OP_SMOOTH_SUBTRACTION: 
			break;
		case OP_SMOOTH_INTERSECTION: 
			break;
	}
#endif

	return result;
}

float map(int3 id) {
	float3 position = float3(id - half_tex_size);

	uint len, stride;
	shape_data.GetDimensions(len, stride);

	const float MAX_DIST = 100000.0;
	float cell_distance = MAX_DIST;

	for (uint i = 0; i < len; ++i) {
		float distance = mapShape(position, i);
		cell_distance = min(cell_distance, distance);
	}

	return cell_distance;
}

////////////////////////////////////////////////////////////////////////

[numthreads(8, 8, 8)]
void main(uint3 id : SV_DispatchThreadID, uint3 group_id : SV_GroupID) {
	tex[id] = map(id);
}
