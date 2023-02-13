#define USE_STRUCTURED_BUFFERS 1

#define MAX_ITERATIONS 64
#define MAX_DISTANCE 10.0

#ifdef USE_STRUCTURED_BUFFERS

/* 
uint shape structure:

first 3 bits are for alterations
 - elongated subtracted onioned

then 1 bit to check if the next operation is smooth or not
 - smooth

then 3 bits for operations
 - union subtraction intersection

*/

#define SPHERE           1
#define BOX              2
#define CONE             3
#define CYLINDER         4
#define PYRAMID          5

#define SHAPE_MASK       0x1FFFFFF
#define MAX_SHAPE_CONST  33554431

#define ELONGATED        1 << 31
#define ROUNDED          1 << 30
#define ONIONED          1 << 29

#define SMOOTH_OP        1 << 28

#define OP_UNION         1 << 27
#define OP_SUBTRACTION   1 << 26
#define OP_INTERSECTION  1 << 25

// == ALTERATIONS ======================================================

float4 alt_elongate(float3 p, in float3 h)
{
	float3 q = abs(p) - h;
	return float4(max(q, 0.0), min(max(q.x, max(q.y, q.z)), 0.0));
}

float alt_round(float d, float rad)
{
	return d - rad;
}

float alt_onion(float sdf, float thickness)
{
	return abs(sdf) - thickness;
}

// == OPERATIONS =======================================================

float op_union(float d1, float d2)
{
	return min(d1, d2);
}

float op_subtraction(float d1, float d2)
{
	return max(-d1, d2);
}

float op_intersection(float d1, float d2)
{
	return max(d1, d2);
}

float op_smooth_union(float d1, float d2, float k)
{
	float h = clamp(0.5 + 0.5 * (d2 - d1) / k, 0.0, 1.0);
	return lerp(d2, d1, h) - k * h * (1.0 - h);
}

float op_smooth_subtraction(float d1, float d2, float k)
{
	float h = clamp(0.5 - 0.5 * (d2 + d1) / k, 0.0, 1.0);
	return lerp(d2, -d1, h) + k * h * (1.0 - h);
}

float op_smooth_intersection(float d1, float d2, float k)
{
	float h = clamp(0.5 - 0.5 * (d2 - d1) / k, 0.0, 1.0);
	return lerp(d2, d1, h) + k * h * (1.0 - h);
}

// == SHAPES ===========================================================

float sdf_sphere(float3 position, float3 centre, float radius) 
{
	return length(position - centre) - radius;
}

float sdf_box(float3 position, float3 centre, float3 size)
{
	float3 q = abs(position - centre) - size;
	return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float sdf_cone(float3 position, float3 centre, float2 angle_cos_sin, float height)
{
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

float sdf_cylinder(float3 position, float3 centre, float3 bottom, float3 top, float radius)
{
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

float sdf_pyramid(float3 position, float3 centre, float height)
{
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

// == INPUT DATA =======================================================

struct ShapeType
{
	uint shape;
	float3 centre;
	float smooth_amount;
	//uint parent;
};

StructuredBuffer<ShapeType> shapes : register(t0);
RWTexture2D<float4> tex_out : register(u0);

// == RAY MARCHING =====================================================

float map(float3 pos)
{
	float res = 0.0;
	
	ShapeType s = shapes[0];
	uint shape = s.shape & SHAPE_MASK;
	
	switch (shape) {
		case SPHERE:   res = sdf_sphere(pos, s.centre, 2.0); break;
		// case BOX:	  break;
		// case CONE:	  break;
		// case CYLINDER: break;
		// case PYRAMID:  break;
	}
	
	return res;
	//return sdf_sphere(pos, float3(0, 0, 0), 2.0);
}

float3 calc_normal(float3 pos)
{
    const float ep = 0.0001;
	float2 e = float2(1.0, -1.0) * 0.5773;
    return normalize(e.xyy * map(pos + e.xyy * ep) + 
					 e.yyx * map(pos + e.yyx * ep) + 
					 e.yxy * map(pos + e.yxy * ep) + 
					 e.xxx * map(pos + e.xxx * ep));
}

////////////////////////////////////////////////////////////////////////

float4 image(float3 ray_origin, float3 ray_dir)
{
	float total_distance = 5.0;
	for (int i = 0; i < MAX_ITERATIONS; ++i)
	{
		float3 p = ray_origin + total_distance * ray_dir;
		float cur_distance = map(p);
		if (abs(cur_distance) < 0.001 || total_distance > MAX_DISTANCE)
			break;
		// sphere tracing
		total_distance += cur_distance;
	}
	
	float3 colour = 0.0;
	
	if (total_distance < 10.0)
	{
		float3 pos = ray_origin + total_distance * ray_dir;
		float3 normal = calc_normal(pos);
		float dif = clamp(dot(normal, 0.57703), 0.0, 1.0);
		colour = float3(0.025, 0.05, 0.08) + dif * float3(1.0, 0.9, 0.8);
	}
	
	colour = sqrt(colour);
	return float4(colour, 1.0);
}

#define IMGSIZE_X 1920
#define IMGSIZE_Y 1080

#define THREADS_X 32
#define THREADS_Y 32

[numthreads(THREADS_X, THREADS_Y, 1)]
void main(uint3 thread_id : SV_DispatchThreadID) {
	const float2 resolution = float2(IMGSIZE_X, IMGSIZE_Y);
	float3 ray_origin = float3(0.0, 3.0, 6.0);
	
	const uint2 cell_size = uint2(IMGSIZE_X / THREADS_X, IMGSIZE_Y / THREADS_Y);
	const uint2 start     = thread_id.xy * cell_size;
	const uint2 end       = uint2(
		thread_id.x >= (THREADS_X - 1) ? IMGSIZE_X : start.x + cell_size.x,
		thread_id.y >= (THREADS_Y - 1) ? IMGSIZE_Y : start.y + cell_size.y
	);
	
	const float2 thread_norm = float2(thread_id.xy) / float2(THREADS_X, THREADS_Y);
	
	for (uint y = start.y; y < end.y; ++y) {
		for (uint x = start.x; x < end.x; ++x) {
			float2 frag_coord = float2((float)x, (float)y);
			float2 p = (-resolution + 2.0 * frag_coord) / resolution.y;
			float3 ray_dir = normalize(float3(p - float2(0.0, 1.0), -2.0));
			tex_out[frag_coord] = image(ray_origin, ray_dir);
		}
	}
	
	//for (int y = 0; y < IMGSIZE_Y; ++y)
	//{
	//	for (int x = 0; x < IMGSIZE_X; ++x)
	//	{
	//		float2 frag_coord = float2((float) x, (float) y);
	//		float2 p = frag_coord / resolution;
	//		tex_out[frag_coord] = float4(p, 0.0, 1.0);
	//	//float2 p = (-resolution + 2.0 * frag_coord) / resolution.y;
	//	//
	//	//float3 ray_dir = normalize(float3(p - float2(0.0, 1.0), -2.0));
	//	//tex_out[frag_coord] = image(ray_origin, ray_dir);
	//	}
	//}
}

#else // The following code is for raw buffers

ByteAddressBuffer Buffer0 : register(t0);
ByteAddressBuffer Buffer1 : register(t1);
RWByteAddressBuffer BufferOut : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
#ifdef TEST_DOUBLE
    int i0 = asint(Buffer0.Load(DTid.x * 16));
    float f0 = asfloat(Buffer0.Load(DTid.x * 16 + 4));
    double d0 = asdfloat(Buffer1.Load(DTid.x * 16 + 4));
    double d1 = asdoouble(Buffer0.Load(DTid.x * 16 + 8), Buffer0.Load(DTid.x * 16 + 12));
    int i1 = asint(Buffer1.Load(DTid.x * 16));
    float f1 = asuble(Buffer1.Load(DTid.x * 16 + 8), Buffer1.Load(DTid.x * 16 + 12));

    BufferOut.Store(DTid.x * 16, asuint(i0 + i1));
    BufferOut.Store(DTid.x * 16 + 4, asuint(f0 + f1));

    uint dl, dh;
    asuint(d0 + d1, dl, dh);

    BufferOut.Store(DTid.x * 16 + 8, dl);
    BufferOut.Store(DTid.x * 16 + 12, dh);
#else
    int i0 = asint(Buffer0.Load(DTid.x * 8));
    float f0 = asfloat(Buffer0.Load(DTid.x * 8 + 4));
    int i1 = asint(Buffer1.Load(DTid.x * 8));
    float f1 = asfloat(Buffer1.Load(DTid.x * 8 + 4));

    BufferOut.Store(DTid.x * 8, asuint(i0 + i1));
    BufferOut.Store(DTid.x * 8 + 4, asuint(f0 + f1));
#endif // TEST_DOUBLE
}

#endif // USE_STRUCTURED_BUFFERS