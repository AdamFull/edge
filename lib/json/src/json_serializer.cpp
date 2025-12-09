#include "json.hpp"

#include <cmath>
#include <cstdio>

namespace edge {
	namespace detail {
		static bool stringify_value(String* sb, const JsonValue* value, const char* indent, i32 depth);

		static bool append_escaped_string(String* sb, const char* str, usize length) {
			if (!string_append_char(sb, '"')) {
				return false;
			}

			for (usize i = 0; i < length; i++) {
				char c = str[i];

				switch (c) {
				case '"':  if (!string_append(sb, "\\\"")) return false; break;
				case '\\': if (!string_append(sb, "\\\\")) return false; break;
				case '\b': if (!string_append(sb, "\\b")) return false; break;
				case '\f': if (!string_append(sb, "\\f")) return false; break;
				case '\n': if (!string_append(sb, "\\n")) return false; break;
				case '\r': if (!string_append(sb, "\\r")) return false; break;
				case '\t': if (!string_append(sb, "\\t")) return false; break;
				default:
					if (static_cast<unsigned char>(c) < 32) {
						char buf[7];
						snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
						if (!string_append(sb, buf)) {
							return false;
						}
					}
					else {
						if (!string_append_char(sb, c)) {
							return false;
						}
					}
					break;
				}
			}

			return string_append_char(sb, '"');
		}

		static bool stringify_indent(String* sb, const char* indent, i32 depth) {
			if (!indent) {
				return true;
			}

			for (i32 i = 0; i < depth; i++) {
				if (!string_append(sb, indent)) {
					return false;
				}
			}

			return true;
		}

		static bool stringify_value(String* sb, const JsonValue* value, const char* indent, i32 depth) {
			if (!value) {
				return false;
			}

			switch (value->m_type) {
			case JsonType::Null:
				return string_append(sb, "null");

			case JsonType::Bool:
				return string_append(sb, value->as.bool_value ? "true" : "false");

			case JsonType::Number: {
				char buf[64];
				f64 num = value->as.number_value;

				// Check if it's an integer
				if (floor(num) == num && fabs(num) < 1e15) {
					snprintf(buf, sizeof(buf), "%.0f", num);
				}
				else {
					snprintf(buf, sizeof(buf), "%.17g", num);
				}

				return string_append(sb, buf);
			}

			case JsonType::String:
				return append_escaped_string(sb,
					string_cstr(&value->as.string_value),
					string_length(&value->as.string_value));

			case JsonType::Array: {
				if (!string_append_char(sb, '[')) {
					return false;
				}

				usize size = array_size(&value->as.array_value);

				if (size > 0 && indent) {
					if (!string_append_char(sb, '\n')) {
						return false;
					}
				}

				for (usize i = 0; i < size; i++) {
					if (indent) {
						if (!stringify_indent(sb, indent, depth + 1)) {
							return false;
						}
					}

					JsonValue** elem_ptr = array_at(&value->as.array_value, i);
					if (!stringify_value(sb, *elem_ptr, indent, depth + 1)) {
						return false;
					}

					if (i < size - 1) {
						if (!string_append_char(sb, ',')) {
							return false;
						}
					}

					if (indent) {
						if (!string_append_char(sb, '\n')) {
							return false;
						}
					}
				}

				if (size > 0 && indent) {
					if (!stringify_indent(sb, indent, depth)) {
						return false;
					}
				}

				return string_append_char(sb, ']');
			}

			case JsonType::Object: {
				if (!string_append_char(sb, '{')) {
					return false;
				}

				usize size = hashmap_size(&value->as.object_value);

				if (size > 0 && indent) {
					if (!string_append_char(sb, '\n')) {
						return false;
					}
				}

				usize count = 0;
				for (const auto& entry : value->as.object_value) {
					if (indent) {
						if (!stringify_indent(sb, indent, depth + 1)) {
							return false;
						}
					}

					const char* key = string_cstr(&entry.key);
					usize key_len = string_length(&entry.key);

					if (!append_escaped_string(sb, key, key_len)) {
						return false;
					}

					if (!string_append_char(sb, ':')) {
						return false;
					}

					if (indent) {
						if (!string_append_char(sb, ' ')) {
							return false;
						}
					}

					if (!stringify_value(sb, entry.value, indent, depth + 1)) {
						return false;
					}

					count++;
					if (count < size) {
						if (!string_append_char(sb, ',')) {
							return false;
						}
					}

					if (indent) {
						if (!string_append_char(sb, '\n')) {
							return false;
						}
					}
				}

				if (size > 0 && indent) {
					if (!stringify_indent(sb, indent, depth)) {
						return false;
					}
				}

				return string_append_char(sb, '}');
			}
			}

			return false;
		}
	}

	char* json_stringify(const JsonValue* value) {
		return json_stringify_pretty(value, nullptr);
	}

	char* json_stringify_pretty(const JsonValue* value, const char* indent) {
		if (!value) return nullptr;

		String sb;
		if (!string_create(value->m_allocator, &sb, 256)) {
			return nullptr;
		}

		if (!detail::stringify_value(&sb, value, indent, 0)) {
			string_destroy(&sb);
			return nullptr;
		}

		char* result = allocator_strdup(value->m_allocator, string_cstr(&sb));
		string_destroy(&sb);

		return result;
	}
}