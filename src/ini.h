#pragma once

#include "utils.h"

namespace ini {
	struct Value {
		str::view key;
		str::view value;

		Value() = default;
		Value(str::view key, str::view value) : key(key), value(value) {}

		bool isValid() const { return !key.empty(); }
		operator bool() const { return isValid(); }

		unsigned int asUint() const;
		int asInt() const;
		double asNum() const;
		bool asBool() const;
		mem::ptr<char[]> asStr() const;
		size_t toStr(char *buf, size_t buflen) const;
		arr<str::view> asVec(char delim = ' ') const;

		void trySet(int &value) const;
		void trySet(float &value) const;
		void trySet(double &value) const;
		void trySet(bool &value) const;
		void trySet(mem::ptr<char[]> &value) const;
		void trySet(arr<str::view> &value) const;
	};

	struct Table {
		str::view name;
		arr<Value> values;

		Table() = default;
		Table(str::view name) : name(name) {}

		bool isValid() const { return !name.empty(); }
		const Value get(const char *key) const;
		const Value &operator[](const char *key) const;
	};
	
	struct Doc {
		//std::string text;
		file::MemoryBuf text;
		arr<Table> tables;

		Doc() = default;
		Doc(const char *filename) { parse(filename); }
		void parse(const char *filename);
		
		const Table &root() const;
		const Table *get(const char *name) const;
		const Table &operator[](const char *name) const;
	};
} // namespace ini