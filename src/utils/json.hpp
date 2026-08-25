#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdio>
#include <cstring>

// 极简 JSON 值类：支持链式下标赋值与 dump 序列化（替代 nlohmann-json）
class json_t
{
public:
	enum class type : uint8_t
	{
		null,
		boolean,
		number,
		string,
		array,
		object
	};

	json_t() : m_type(type::null), m_num(0), m_bool(false) {}
	json_t(std::nullptr_t) : json_t() {}
	json_t(bool v) : m_type(type::boolean), m_num(0), m_bool(v) {}
	json_t(int v) : m_type(type::number), m_num(static_cast<double>(v)), m_bool(false) {}
	json_t(float v) : m_type(type::number), m_num(v), m_bool(false) {}
	json_t(double v) : m_type(type::number), m_num(v), m_bool(false) {}
	json_t(const char* v) : m_type(type::string), m_num(0), m_bool(false), m_string(v ? v : "") {}
	json_t(const std::string& v) : m_type(type::string), m_num(0), m_bool(false), m_string(v) {}
	json_t(const std::vector<std::string>& v) : m_type(type::array), m_num(0), m_bool(false)
	{
		for (const auto& s : v)
			m_array.emplace_back(s);
	}

	static json_t array() { json_t j; j.m_type = type::array; return j; }
	static json_t object() { json_t j; j.m_type = type::object; return j; }

	json_t& operator[](const std::string& key)
	{
		if (m_type != type::object)
		{
			m_type = type::object;
			m_children.clear();
		}
		for (auto& [k, v] : m_children)
		{
			if (k == key)
				return v;
		}
		m_children.emplace_back(key, json_t());
		return m_children.back().second;
	}

	json_t& operator[](const char* key) { return (*this)[std::string(key)]; }

	json_t& operator[](size_t idx)
	{
		if (m_type != type::array)
		{
			m_type = type::array;
			m_array.clear();
		}
		while (m_array.size() <= idx)
			m_array.emplace_back();
		return m_array[idx];
	}

	json_t& operator=(const json_t& other)
	{
		m_type = other.m_type;
		m_num = other.m_num;
		m_bool = other.m_bool;
		m_string = other.m_string;
		m_array = other.m_array;
		m_children = other.m_children;
		return *this;
	}

	json_t& push_back(const json_t& v)
	{
		if (m_type != type::array)
		{
			m_type = type::array;
			m_array.clear();
		}
		m_array.push_back(v);
		return *this;
	}

	void clear()
	{
		m_type = type::null;
		m_num = 0;
		m_bool = false;
		m_string.clear();
		m_array.clear();
		m_children.clear();
	}

	std::string dump() const
	{
		std::string out;
		append(out);
		return out;
	}

private:
	static void escape(std::string& out, const std::string& s)
	{
		out += '"';
		for (unsigned char ch : s)
		{
			switch (ch)
			{
				case '"': out += "\\\""; break;
				case '\\': out += "\\\\"; break;
				case '\n': out += "\\n"; break;
				case '\r': out += "\\r"; break;
				case '\t': out += "\\t"; break;
				default:
					if (ch < 0x20)
					{
						char buf[8];
						snprintf(buf, sizeof(buf), "\\u%04x", ch);
						out += buf;
					}
					else
						out += static_cast<char>(ch);
			}
		}
		out += '"';
	}

	void append(std::string& out) const
	{
		switch (m_type)
		{
			case type::null: out += "null"; break;
			case type::boolean: out += m_bool ? "true" : "false"; break;
			case type::number:
			{
				char buf[32];
				if (m_num == static_cast<int64_t>(m_num) && m_num >= -2e9 && m_num <= 2e9)
					snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(m_num));
				else
					snprintf(buf, sizeof(buf), "%.4f", m_num);
				out += buf;
				break;
			}
			case type::string: escape(out, m_string); break;
			case type::array:
			{
				out += '[';
				for (size_t i = 0; i < m_array.size(); i++)
				{
					if (i) out += ',';
					m_array[i].append(out);
				}
				out += ']';
				break;
			}
			case type::object:
			{
				out += '{';
				for (size_t i = 0; i < m_children.size(); i++)
				{
					if (i) out += ',';
					escape(out, m_children[i].first);
					out += ':';
					m_children[i].second.append(out);
				}
				out += '}';
				break;
			}
		}
	}

	type m_type;
	double m_num;
	bool m_bool;
	std::string m_string;
	std::vector<json_t> m_array;
	std::vector<std::pair<std::string, json_t>> m_children;
};
