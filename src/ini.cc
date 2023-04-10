#include "ini.h"

#include <ctype.h>

struct StrStream {
	const char *start;
	const char *cur;
	size_t len;

	StrStream(const file::MemoryBuf &buf) : start((const char *)buf.data.get()), cur(start), len(buf.size) {}
	bool isFinished() { return (size_t)(cur - start) >= len; }
	void skipWhitespace();
	void skipUntil(char until);
	void skip();
	std::string_view getView(char delim);
};

static std::string_view trim(std::string_view view);

namespace ini {
	unsigned int Value::asUint() const {
		return str::toUInt(value.data());
	}

	int Value::asInt() const {
		return str::toInt(value.data());
	}

	double Value::asNum() const {
		return str::toNum(value.data());
	}

	bool Value::asBool() const {
		return value == "true";
	}

	mem::ptr<char[]> Value::asStr() const {
		return str::dup(value.data(), value.size());
	}

	//std::string Value::asStr() const {
	//	return std::string{ trim(value) };
	//}

	size_t Value::toStr(char *buf, size_t buflen) const {
		if (!buf || buflen == 0) {
			return 0;
		}
		if (value.empty()) {
			buf[0] = '\0';
			return 0;
		}
		std::string_view view = trim(value);
		size_t to_copy = std::min(view.size(), buflen - 1);
		memcpy(buf, value.data(), to_copy);
		buf[to_copy] = '\0';
		return to_copy;
	}

	arr<std::string_view> Value::asVec(char delim) const {
		if (value.empty()) return {};
		arr<std::string_view> out;

		size_t start = 0;
		for (size_t i = 0; i < value.size(); ++i) {
			if (value[i] == delim) {
				std::string_view arr_val = trim(value.substr(start, i - start));
				if (!arr_val.empty()) {
					out.emplace_back(arr_val);
				}
				start = i + 1;
			}
		}
		std::string_view last = trim(value.substr(start));
		if (!last.empty()) {
			out.emplace_back(last);
		}
		return out;
	}

	void Value::trySet(int &val) const {
		if (isValid()) {
			val = asInt();
		}
	}

	void Value::trySet(float &val) const {
		if (isValid()) {
			val = (float)asNum();
		}
	}

	void Value::trySet(double &val) const {
		if (isValid()) {
			val = asNum();
		}
	}

	void Value::trySet(bool &val) const {
		if (isValid()) {
			val = asBool();
		}
	}

	void Value::trySet(mem::ptr<char[]> &val) const {
		if (isValid()) {
			val = asStr();
		}
	}

	//void Value::trySet(std::string &val) const {
	//	if (isValid()) {
	//		val = asStr();
	//	}
	//}

	void Value::trySet(arr<std::string_view> &val) const {
		if (isValid()) {
			val = asVec();
		}
	}

	const Value Table::get(const char *key) const {
		for (const Value &v : values) {
			if (v.key == key) {
				return v;
			}
		}
		return {};
	}

	const Value &Table::operator[](const char *key) const {
		static Value null_value{};
		for (const Value &v : values) {
			if (v.key == key) {
				return v;
			}
		}
		return null_value;
	}

	const Table &Doc::root() const {
		static Table null_table{};
		if (tables.empty()) return null_table;
		return tables[0];
	}

	const Table *Doc::get(const char *name) const {
		for (const Table &t : tables) {
			if (t.name == name) {
				return &t;
			}
		}
		return nullptr;
	}

	const Table &Doc::operator[](const char *name) const {
		static Table null_table{};
		for (const Table &t : tables) {
			if (t.name == name) {
				return t;
			}
		}
		return null_table;
	}

	static void addValue(Table &table, StrStream &in) {
		std::string_view key = trim(in.getView('='));
		in.skip(); // skip divider
		std::string_view val = trim(in.getView('\n'));

		if (key.empty()) return;

		// remove inline comments
		val = val.substr(0, val.find_first_of("#;"));

		// value might be until EOF, in that case no use in skipping
		if (!in.isFinished()) in.skip(); // ignore \n

		table.values.emplace_back(key, val);
	}

	static void addTable(Doc &out, StrStream &in) {
		in.skip(); // skip [
		std::string_view name = in.getView(']');
		in.skip(); // skip ]

		if (name.empty()) return;

		Table &tab = out.tables.emplace_back(name);
		in.skipUntil('\n');
		in.skip(); // skip \n

		while (!in.isFinished()) {
			switch (*in.cur) {
			case '\n': case '\r':
				return;
			case '#': case ';':
				in.skipUntil('\n');
				in.skip(); // skip \n
				break;
			default:
				addValue(tab, in);
				break;
			}
		}
	}

	void Doc::parse(const char *filename) {
		text = file::read(filename);
		if (!text.data) return;

		tables.clear();
		tables.emplace_back("root");
		StrStream in = text;

		while (!in.isFinished()) {
			switch (*in.cur) {
			case '[':
				addTable(*this, in);
				break;
			case '#': case ';':
				in.skipUntil('\n');
				in.skip(); // skip \n
				break;
			default:
				addValue(tables[0], in);
				break;
			}
			in.skipWhitespace();
		}
	}

} // namespace ini

static std::string_view trim(std::string_view view) {
	if (view.empty()) return view;
	std::string_view out = view;
	// trim left
	for (size_t i = 0; i < view.size() && isspace(view[i]); ++i) {
		out.remove_prefix(1);
	}
	if (out.empty()) return out;
	// trim right
	for (long long i = out.size() - 1; i >= 0 && isspace(out[i]); --i) {
		out.remove_suffix(1);
	}
	return out;
}

void StrStream::skipWhitespace() {
	const char *end = start + len;
	for (; cur < end && isspace(*cur); ++cur);
}

void StrStream::skipUntil(char until) {
	const char *end = start + len;
	for (; cur < end && *cur != until; ++cur);
}

void StrStream::skip() {
	if (!isFinished()) ++cur;
}

std::string_view StrStream::getView(char delim) {
	const char *from = cur;
	skipUntil(delim);
	size_t view_len = cur - from;
	return { from, view_len };
}