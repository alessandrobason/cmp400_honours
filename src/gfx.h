#pragma once

#include <stdint.h>

#include "d3d11_fwd.h"
#include "slice.h"
#include "vec.h"
#include "utils.h"

enum class ShaderType : uint8_t {
	None     = 0,
	Vertex   = 1 << 0,
	Fragment = 1 << 1,
	Compute  = 1 << 2,
};

inline ShaderType &operator|=(ShaderType &self, ShaderType b) { self = ShaderType((uint8_t)self | (uint8_t)b); return self; }
inline uint8_t operator&(ShaderType a, ShaderType b) { return (uint8_t)a & (uint8_t)b; }

struct Colour;

/* ==========================================
   ================= BUFFER =================
   ========================================== */

struct Buffer {
	enum class Usage {
		Default,   // can read/write from GPU
		Immutable, // can be read from the GPU
		Dynamic,   // can be read from the GPU and written by the CPU
		Staging,   // used to transfer from the GPU to the CPU
		Count,
	};

	Buffer() = default;
	Buffer(const Buffer &buf) = delete;
	Buffer(Buffer &&buf);
	~Buffer();

	Buffer &operator=(Buffer &&buf);

	template<typename T>
	bool create(Usage usage, bool can_write = true, bool can_read = false) {
		return create(sizeof(T), usage, can_write, can_read);
	}

	bool create(size_t type_size, Usage usage, bool can_write = true, bool can_read = false);
	bool create(size_t type_size, Usage usage, const void *initial_data, size_t data_count = 1, bool can_write = true, bool can_read = false);
	void cleanup();

	template<typename T>
	T *map(unsigned int subresource = 0) {
		return (T *)map(subresource);
	}

	void *map(unsigned int subresource = 0);
	void unmap(unsigned int subresource = 0);
	void bind(ShaderType type, unsigned int slot = 0);
	void unbind(ShaderType type, unsigned int slot = 0);

	dxptr<ID3D11Buffer> buffer = nullptr;
};

/* ==========================================
   ================= SHADER =================
   ========================================== */

struct Shader {
	Shader() = default;
	Shader(const Shader &s) = delete;
	Shader(Shader &&s);
	~Shader();

	Shader &operator=(Shader &&s);

	bool loadVertex(const char *filename);
	bool loadVertex(const void *data, size_t len);
	bool loadFragment(const char *filename);
	bool loadFragment(const void *data, size_t len);
	bool loadCompute(const char *filename);
	bool loadCompute(const void *data, size_t len);
	bool load(const char *vertex, const char *fragment, const char *compute);

	int addBuffer(size_t type_size, Buffer::Usage usage = Buffer::Usage::Default, bool can_write = true, bool can_read = false);
	int addBuffer(size_t type_size, Buffer::Usage usage, const void *initial_data, size_t data_count = 1, bool can_write = true, bool can_read = false);
	template<typename T>
	int addBuffer(Buffer::Usage usage = Buffer::Usage::Default, bool can_write = true, bool can_read = false) { return addBuffer(sizeof(T), usage, can_write, can_read); }
	template<typename T>
	int addBuffer(Buffer::Usage usage, const void *initial_data, size_t data_count = 1, bool can_write = true, bool can_read = false) {
		return addBuffer(sizeof(T), usage, initial_data, data_count, can_write, can_read);
	}

	Buffer *getBuffer(int index);

	bool addSampler();
	void setSRV(ShaderType type, Slice<ID3D11ShaderResourceView *> textures);

	void cleanup();

	//bool update(float time);
	void bind();
	//void unbind(int srv_count = 0, int buffer_count = 0);
	void bindBuf(ShaderType type, int buffer, unsigned int slot = 0);
	void bindBuffers(ShaderType type, Slice<int> cbuffers = {});
	void unbindBuf(ShaderType type, int buffer, unsigned int slot = 0);
	void unbindBuffers(ShaderType type, int count = 1);

	void dispatch(const vec3u &threads, Slice<int> cbuffers = {}, Slice<ID3D11ShaderResourceView *> srvs = {}, Slice<ID3D11UnorderedAccessView *> uavs = {});

	dxptr<ID3D11VertexShader> vert_sh = nullptr;
	dxptr<ID3D11PixelShader> pixel_sh = nullptr;
	dxptr<ID3D11ComputeShader> compute_sh = nullptr;
	dxptr<ID3D11InputLayout> layout = nullptr;
	arr<Buffer> buffers;
	dxptr<ID3D11SamplerState> sampler = nullptr;
};

/* ==========================================
   ============= DYNAMIC SHADER =============
   ========================================== */

struct DynamicShader {
	DynamicShader() = default;
	DynamicShader(const DynamicShader &s) = delete;
	DynamicShader(DynamicShader &&s);
	~DynamicShader();

	DynamicShader &operator=(DynamicShader &&s);

	bool init(const char *vertex, const char *fragment, const char *compute);
	void cleanup();

	void poll();
	bool hasUpdated() const { return updated != ShaderType::None; }
	ShaderType getChanged() const { return updated; }

	Shader shader;

private:
	bool addFileWatch(const char *name, ShaderType type);

	file::Watcher watcher = "shaders/";
	ShaderType updated = ShaderType::None;
};

/* ==========================================
   ============= RENDER TEXTURE =============
   ========================================== */

struct RenderTexture {
	RenderTexture() = default;
	RenderTexture(const RenderTexture &rt) = delete;
	RenderTexture(RenderTexture &&rt);
	~RenderTexture();
	RenderTexture &operator=(RenderTexture &&rt);

	bool create(int width, int height);
	bool createFromBackbuffer();
	bool resize(int new_width, int new_height);
	void cleanup();

	void bind(ID3D11DepthStencilView *dsv = nullptr);
	void clear(Colour colour, ID3D11DepthStencilView *dsv = nullptr);

	bool takeScreenshot(const char *base_name = "screenshot");

	vec2i size;
	dxptr<ID3D11Texture2D> texture = nullptr;
	dxptr<ID3D11RenderTargetView> view = nullptr;
	dxptr<ID3D11ShaderResourceView> resource = nullptr;
};

/* ==========================================
   =============== TEXTURE 3D ===============
   ========================================== */

struct Texture3D {
	Texture3D() = default;
	Texture3D(const Texture3D &rt) = delete;
	Texture3D(Texture3D &&rt);
	~Texture3D();
	Texture3D &operator=(Texture3D &&rt);

	inline bool create(const vec3u &texsize) { return create(texsize.x, texsize.y, texsize.z); }
	bool create(int width, int height, int depth);
	void cleanup();

	vec3i size = 0;
	dxptr<ID3D11Texture3D> texture = nullptr;
	dxptr<ID3D11UnorderedAccessView> uav = nullptr;
	dxptr<ID3D11ShaderResourceView> srv = nullptr;
};

/* ==========================================
   ================== MESH ==================
   ========================================== */

struct Mesh {
	struct Vertex {
		vec3 position;
		vec2 uv;
	};

	// check that the Vertex struct does not have any padding
	static_assert(sizeof(Vertex) == (sizeof(float) * 3 + sizeof(float) * 2));

	// we're doing everything in shaders, this is not that important
	using Index = uint16_t;

	Mesh() = default;
	Mesh(const Mesh &m) = delete;
	Mesh(Mesh &&m);
	~Mesh();

	Mesh &operator=(Mesh &&m);

	bool create(Slice<Vertex> vertices, Slice<Index> indices);
	void cleanup();

	void render();

	int vertex_count = 0;
	int index_count = 0;
	dxptr<ID3D11Buffer> vert_buf = nullptr;
	dxptr<ID3D11Buffer> ind_buf = nullptr;
};

/* ==========================================
   ================= COLOUR =================
   ========================================== */

struct Colour : public vec4 {
	using vec4::vec4;
	Colour(const vec4 &vec) : vec4(vec) {}

	static const Colour black;
	static const Colour white;

	static const Colour red;
	static const Colour green;
	static const Colour blue;

	static const Colour cyan;
	static const Colour magenta;
	static const Colour yellow;

	static const Colour crimson;
	static const Colour pink;
	static const Colour coral;
	static const Colour peach;
	static const Colour violet;
	static const Colour indigo;
	static const Colour lime;
	static const Colour olive;
	static const Colour teal;
	static const Colour sky;
	static const Colour navy;
	static const Colour brown;
	static const Colour maroon;
	static const Colour beige;
	static const Colour rose;
	static const Colour orange;
	static const Colour purple;
	static const Colour silver;
	static const Colour dark_grey;
	static const Colour turquoise;
};
