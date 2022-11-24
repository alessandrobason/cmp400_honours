#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace ini {
	struct Value {
		std::string_view key;
		std::string_view value;

		Value() = default;
		Value(std::string_view key, std::string_view value) : key(key), value(value) {}

		bool isValid() const { return !key.empty(); }

		unsigned int asUint() const;
		int asInt() const;
		double asNum() const;
		bool asBool() const;
		std::string asStr() const;
		size_t toStr(char *buf, size_t buflen) const;
		std::vector<std::string_view> asVec(char delim = ' ') const;
	};

	struct Table {
		std::string_view name;
		std::vector<Value> values;

		Table() = default;
		Table(std::string_view name) : name(name) {}

		bool isValid() const { return !name.empty(); }
		const Value *get(const char *key) const;
		const Value &operator[](const char *key) const;
	};
		
	struct Doc {
		std::string text;
		std::vector<Table> tables;

		Doc() = default;
		Doc(const char *filename) { parse(filename); }
		void parse(const char *filename);
		
		const Table &root() const;
		const Table *get(const char *name) const;
		const Table &operator[](const char *name) const;
	};
} // namespace ini