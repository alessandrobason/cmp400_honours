#include "ini.h"

#include "utils.h"

struct StrStream {
	const char *start;
	const char *cur;
	size_t len;

	StrStream(const char *str) : start(str), cur(str), len(strlen(str)) {}
	bool isFinished() { return (size_t)(cur - start) >= len; }
	void skipWhitespace();
	void skipUntil(char until);
	void skip();
	std::string_view getView(char delim);
};

static std::string_view trim(std::string_view view);

namespace ini {
	unsigned int Value::asUint() const {
		if (value.empty()) return 0;
		unsigned int out = strtoul(value.data(), NULL, 0);
		if (out == ULONG_MAX) {
			out = 0;
		}
		return out;
	}

	int Value::asInt() const {
		if (value.empty()) return 0;
		int out = strtol(value.data(), NULL, 0);
		if (out == LONG_MAX || out == LONG_MIN) {
			out = 0;
		}
		return out;
	}

	double Value::asNum() const {
		if (value.empty()) return 0;
		double out = strtod(value.data(), NULL);
		if (out == HUGE_VAL || out == -HUGE_VAL) {
			out = 0;
		}
		return out;
	}

	bool Value::asBool() const {
		return value == "true";
	}

	std::string Value::asStr() const {
		return std::string{ trim(value) };
	}

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

	std::vector<std::string_view> Value::asVec(char delim) const {
		if (value.empty()) return {};
		std::vector<std::string_view> out;

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

	const Value *Table::get(const char *key) const {
		for (const Value &v : values) {
			if (v.key == key) {
				return &v;
			}
		}
		return nullptr;
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
				break;
			default:
				addValue(tab, in);
				break;
			}
		}
	}

	void Doc::parse(const char *filename) {
		text = file::readString(filename);
		if (text.empty()) return;

		tables.clear();
		tables.emplace_back("root");
		StrStream in{ text.data()};

		while (!in.isFinished()) {
			switch (*in.cur) {
			case '[':
				addTable(*this, in);
				break;
			case '#': case ';':
				in.skipUntil('\n');
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