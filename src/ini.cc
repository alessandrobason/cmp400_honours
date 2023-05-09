#include "ini.h"

namespace ini {
	unsigned int Value::asUint() const {
		return str::toUInt(value.data);
	}

	int Value::asInt() const {
		return str::toInt(value.data);
	}

	double Value::asNum() const {
		return str::toNum(value.data);
	}

	bool Value::asBool() const {
		return value == "true";
	}

	mem::ptr<char[]> Value::asStr() const {
		return str::dup(value.data, value.len);
	}

	size_t Value::toStr(char *buf, size_t buflen) const {
		if (!buf || buflen == 0) {
			return 0;
		}
		if (value.empty()) {
			buf[0] = '\0';
			return 0;
		}
		str::view view = value.trim();
		size_t to_copy = math::min(view.len, buflen - 1);
		memcpy(buf, value.data, to_copy);
		buf[to_copy] = '\0';
		return to_copy;
	}

	arr<str::view> Value::asVec(char delim) const {
		if (value.empty()) return {};
		arr<str::view> out;

		size_t start = 0;
		for (size_t i = 0; i < value.len; ++i) {
			if (value[i] == delim) {
				str::view arr_val = value.sub(start, i).trim();
				if (!arr_val.empty()) {
					out.push(arr_val);
				}
				start = i + 1;
			}
		}
		str::view last = value.sub(start).trim();
		if (!last.empty()) {
			out.push(last);
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

	void Value::trySet(arr<str::view> &val) const {
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

	static void addValue(Table &table, fs::StreamIn &in) {
		str::view key = in.getView('=').trim();
		in.skip(); // skip divider
		str::view val = in.getView('\n').trim();

		if (key.empty()) return;

		// remove inline comments
		val = val.sub(0, val.findFirstOf("#;"));

		// value might be until EOF, in that case no use in skipping
		if (!in.isFinished()) in.skip(); // ignore \n

		table.values.push(key, val);
	}

	static void addTable(Doc &out, fs::StreamIn &in) {
		in.skip(); // skip [
		str::view name = in.getView(']');
		in.skip(); // skip ]

		if (name.empty()) return;

		Table &tab = out.tables.push(name);
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
		text = fs::read(filename);
		if (!text.data) return;

		tables.clear();
		tables.push("root");
		fs::StreamIn in = text;

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
