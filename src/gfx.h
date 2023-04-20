#pragma once

#include <stdint.h>

#include "d3d11_fwd.h"
#include "vec.h"
#include "utils.h"
#include "handle.h"

enum class ShaderType : uint8_t {
	None,
	Vertex,
	Fragment,
	Compute,
};

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

	// Buffer() = default;
	Buffer(const Buffer &buf) = delete;
	Buffer(Buffer &&buf);
	~Buffer();
	Buffer &operator=(Buffer &&buf);

	static Handle<Buffer> make();
	static Handle<Buffer> makeConstant(size_t type_size, Usage usage, bool can_write = true, bool can_read = false, const void *initial_data = nullptr, size_t data_count = 1);
	static Handle<Buffer> makeStructured(size_t type_size, size_t count = 1, const void *initial_data = nullptr);
	static Buffer *get(Handle<Buffer> handle);
	static bool isHandleValid(Handle<Buffer> handle);
	static void cleanAll();

	template<typename T>
	static Handle<Buffer> makeConstant(Usage usage, bool can_write = true, bool can_read = false, const void *initial_data = nullptr, size_t data_count = 1) {
		return makeConstant(sizeof(T), usage, can_write, can_read, initial_data, data_count);
	}

	template<typename T>
	static Handle<Buffer> makeStructured(size_t count = 1, const void* initial_data = nullptr) {
		return makeStructured(sizeof(T), count, initial_data);
	}

	// template<typename T>
	// bool create(Usage usage, bool can_write = true, bool can_read = false) {
	// 	return create(sizeof(T), usage, can_write, can_read);
	// }

	// template<typename T>
	// bool createStructured(size_t count = 1, const void* initial_data = nullptr) {
	// 	return createStructured(sizeof(T), count, initial_data);
	// }

	// bool create(size_t type_size, Usage usage, bool can_write = true, bool can_read = false);
	// bool create(size_t type_size, Usage usage, bool can_write = true, bool can_read = false, const void *initial_data = nullptr, size_t data_count = 1);
	// bool createStructured(size_t type_size, size_t count = 1, const void *initial_data = nullptr);
	void cleanup();

	template<typename T>
	T *map(unsigned int subresource = 0) {
		return (T *)map(subresource);
	}

	void *map(unsigned int subresource = 0);
	void unmap(unsigned int subresource = 0);

	void bindCBuffer(ShaderType type, unsigned int slot = 0) { bindCBuffer(*this, type, slot); }
	void bindSRV(ShaderType type, unsigned int slot = 0) { bindSRV(*this, type, slot); }
	void bindUAV(unsigned int slot = 0) { bindUAV(*this, slot); }

	static void bindCBuffer(Buffer &buf, ShaderType type, unsigned int slot = 0);
	static void bindSRV(Buffer &buf, ShaderType type, unsigned int slot = 0);
	static void bindUAV(Buffer &buf, unsigned int slot = 0);
	static void unbindCBuffer(ShaderType type, unsigned int slot = 0, size_t count = 1);
	static void unbindSRV(ShaderType type, unsigned int slot = 0, size_t count = 1);
	static void unbindUAV(unsigned int slot = 0, size_t count = 1);

	dxptr<ID3D11Buffer> buffer = nullptr;
	dxptr<ID3D11UnorderedAccessView> uav = nullptr;
	dxptr<ID3D11ShaderResourceView> srv = nullptr;

private:
	Buffer() = default;
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

	bool load(const char *filename, ShaderType type);

	bool loadVertex(const void *data, size_t len);
	bool loadFragment(const void *data, size_t len);
	bool loadCompute(const void *data, size_t len);

	// int addBuffer(size_t type_size, Buffer::Usage usage = Buffer::Usage::Default, bool can_write = true, bool can_read = false);
	// int addBuffer(size_t type_size, Buffer::Usage usage, const void *initial_data, size_t data_count = 1, bool can_write = true, bool can_read = false);
	// template<typename T>
	// int addBuffer(Buffer::Usage usage = Buffer::Usage::Default, bool can_write = true, bool can_read = false) { return addBuffer(sizeof(T), usage, can_write, can_read); }
	// template<typename T>
	// int addBuffer(Buffer::Usage usage, const void *initial_data, size_t data_count = 1, bool can_write = true, bool can_read = false) {
	// 	static_assert(sizeof(T) % 16 == 0);
	// 	return addBuffer(sizeof(T), usage, initial_data, data_count, can_write, can_read);
	// }

	// int addStructuredBuf(size_t type_size, size_t count = 1, const void* initial_data = nullptr);
	// template<typename T>
	// int addStructuredBuf(size_t count = 1, const void* initial_data = nullptr) {
	// 	static_assert(sizeof(T) % 16 == 0);
	// 	return addStructuredBuf(sizeof(T), count, initial_data);
	// }

	// Buffer *getBuffer(int index);

	bool addSampler();
	void setSRV(Slice<ID3D11ShaderResourceView *> textures);

	void cleanup();

	void bind();
	void bindCBuf(Handle<Buffer> handle, unsigned int slot = 0);
	void bindCBuffers(Slice<Handle<Buffer>> handles = {});
	void unbindCBuf(unsigned int slot = 0);
	void unbindCBuffers(int count = 1, unsigned int slot = 0);
	// void bindCBuf(int buffer, unsigned int slot = 0);
	// void bindCBuffers(Slice<int> cbuffers = {});
	// void unbindCBuf(unsigned int slot = 0);
	// void unbindCBuffers(int count = 1, unsigned int slot = 0);

	void dispatch(const vec3u &threads, Slice<Handle<Buffer>> cbuffers = {}, Slice<ID3D11ShaderResourceView *> srvs = {}, Slice<ID3D11UnorderedAccessView *> uavs = {});
	//void dispatch(const vec3u &threads, Slice<int> cbuffers = {}, Slice<ID3D11ShaderResourceView *> srvs = {}, Slice<ID3D11UnorderedAccessView *> uavs = {});

	dxptr<ID3D11DeviceChild> shader = nullptr;
	// either input layout (if VS) or sampler state (if PS)
	dxptr<ID3D11DeviceChild> extra = nullptr;
	// arr<Buffer> buffers;
	ShaderType shader_type = ShaderType::None;
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

	//bool init(const char *vertex, const char *fragment, const char *compute);
	int add(const char *name, ShaderType type);
	void cleanup();

	void poll();
	// bool hasUpdated() const { return updated != ShaderType::None; }
	bool hasUpdated() const { return has_updated; }
	bool hasUpdated(int index) const;
	// ShaderType getChanged() const { return updated; }
	//const Shader &getChanged() const { return shaders[updated]; }

	Shader *get(int index);

	//Shader shader;

private:
	bool addFileWatch(const char *name, ShaderType type);

	arr<Shader> shaders;
	arr<bool> update_list;
	file::Watcher watcher = "shaders/";
	//ShaderType updated = ShaderType::None;
	bool has_updated = false;
	//int updated = -1;
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
   =============== TEXTURE 2D ===============
   ========================================== */

struct Texture2D {
	Texture2D() = default;
	Texture2D(const Texture2D& rt) = delete;
	Texture2D(Texture2D&& rt);
	~Texture2D();
	Texture2D& operator=(Texture2D&& rt);

	bool load(const char *filename);
	void cleanup();

	vec2i size = 0;
	dxptr<ID3D11Texture2D> texture = nullptr;
	dxptr<ID3D11ShaderResourceView> srv = nullptr;
};

/* ==========================================
   =============== TEXTURE 3D ===============
   ========================================== */

struct Texture3D {
	enum class Type {
		uint8,
		uint16,
		uint32,
		sint8,
		sint16,
		sint32,
		float16, 
		float32,
		count,
	};

	Texture3D() = default;
	Texture3D(const Texture3D &rt) = delete;
	Texture3D(Texture3D &&rt);
	~Texture3D();
	Texture3D &operator=(Texture3D &&rt);

	inline bool create(const vec3u &texsize, Type type) { return create(texsize.x, texsize.y, texsize.z, type); }
	bool create(int width, int height, int depth, Type type);
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
