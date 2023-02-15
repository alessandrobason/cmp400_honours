#pragma once

#include <vector>
#include <string>
#include <memory>
#include <stddef.h>

#include "d3d11_fwd.h"
#include "matrix.h"
#include "buffer.h"
#include "timer.h"
#include "slice.h"

enum class ShaderType {
	None, Vertex, Fragment, Compute
};

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

	int addBuffer(size_t type_size, Usage usage = Usage::Default, bool can_write = true, bool can_read = false);
	template<typename T>
	int addBuffer(Usage usage = Usage::Default, bool can_write = true, bool can_read = false) { return addBuffer(sizeof(T), usage, can_write, can_read); }
	Buffer *getBuffer(int index);

	bool addSampler();
	void setSRV(Slice<ID3D11ShaderResourceView*> textures);

	void cleanup();

	//bool update(float time);
	void bind();
	void unbind();

	ID3D11VertexShader *vert_sh = nullptr;
	ID3D11PixelShader *pixel_sh = nullptr;
	ID3D11ComputeShader *compute_sh = nullptr;
	ID3D11InputLayout *layout = nullptr;
	std::vector<Buffer> buffers;
	ID3D11SamplerState *sampler = nullptr;
};

struct DynamicShader {
	DynamicShader() = default;
	DynamicShader(const DynamicShader &s) = delete;
	DynamicShader(DynamicShader &&s);
	~DynamicShader();

	DynamicShader &operator=(DynamicShader &&s);

	bool init(const char *vertex, const char *fragment, const char *compute);
	void cleanup();

	void poll();
	bool hasUpdated() const { return is_dirty; }

	Shader shader;

private:
	struct WatchedFile {
		WatchedFile() = default;
		WatchedFile(const std::string &name, ShaderType type) : name(name), type(type) {}
		std::string name;
		ShaderType type;
	};

	struct ChangedData {
		ChangedData(size_t i) : index(i) {}
		OnceClock clock;
		size_t index;
	};

	bool addFileWatch(const char *name, ShaderType type);
	void addChanged(size_t index);
	void tryUpdate();

	std::vector<WatchedFile> watched_files;
	std::vector<ChangedData> changed_files;
	win32_overlapped_t overlapped;
	uint8_t change_buf[1024];
	win32_handle_t handle = nullptr;
	bool is_dirty = true;
};