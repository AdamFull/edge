#include "json.hpp"

#include <cctype>
#include <cstring>
#include <cstdlib>

namespace edge {
    struct JsonParser {
        const char* m_json;
        usize m_pos;
        usize m_length;
        const Allocator* m_allocator;
    };

	namespace detail {
		static JsonValue* parse_value(JsonParser* parser);

		static void skip_whitespace(JsonParser* parser) {
			while (parser->m_pos < parser->m_length) {
				char c = parser->m_json[parser->m_pos];
				if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
					parser->m_pos++;
				}
				else {
					break;
				}
			}
		}

		static char* parse_string_content(JsonParser* parser, usize* out_length) {
			if (parser->m_pos >= parser->m_length || parser->m_json[parser->m_pos] != '"') {
				return nullptr;
			}

			parser->m_pos++;

			usize capacity = 32;
			usize length = 0;
			char* result = parser->m_allocator->allocate<char>(capacity);
			if (!result) return nullptr;

			while (parser->m_pos < parser->m_length) {
				char c = parser->m_json[parser->m_pos];

				if (c == '"') {
					parser->m_pos++;
					result[length] = '\0';
					if (out_length) *out_length = length;
					return result;
				}

				if (c == '\\') {
					parser->m_pos++;
					if (parser->m_pos >= parser->m_length) {
						parser->m_allocator->deallocate(result);
						return nullptr;
					}

					char escaped = parser->m_json[parser->m_pos];
					char unescaped;

					switch (escaped) {
					case '"':  unescaped = '"';  break;
					case '\\': unescaped = '\\'; break;
					case '/':  unescaped = '/';  break;
					case 'b':  unescaped = '\b'; break;
					case 'f':  unescaped = '\f'; break;
					case 'n':  unescaped = '\n'; break;
					case 'r':  unescaped = '\r'; break;
					case 't':  unescaped = '\t'; break;
					case 'u': {
						// TODO: Unicode support
						parser->m_pos++;
						if (parser->m_pos + 3 >= parser->m_length) {
							parser->m_allocator->deallocate(result);
							return nullptr;
						}
						unescaped = '?';
						parser->m_pos += 3;
						break;
					}
					default:
						parser->m_allocator->deallocate(result);
						return nullptr;
					}

					c = unescaped;
				}

				// Resize if needed
				if (length >= capacity - 1) {
					capacity *= 2;
					char* new_result = (char*)parser->m_allocator->realloc(result, capacity);
					if (!new_result) {
						parser->m_allocator->deallocate(result);
						return nullptr;
					}
					result = new_result;
				}

				result[length++] = c;
				parser->m_pos++;
			}

			parser->m_allocator->deallocate(result);
			return nullptr;
		}

		// Parse number
		static JsonValue* parse_number(JsonParser* parser) {
			usize start = parser->m_pos;

			// Sign
			if (parser->m_json[parser->m_pos] == '-') {
				parser->m_pos++;
			}

			// Integer part
			if (parser->m_pos >= parser->m_length || !isdigit(parser->m_json[parser->m_pos])) {
				return nullptr;
			}

			if (parser->m_json[parser->m_pos] == '0') {
				parser->m_pos++;
			}
			else {
				while (parser->m_pos < parser->m_length && isdigit(parser->m_json[parser->m_pos])) {
					parser->m_pos++;
				}
			}

			// Decimal part
			if (parser->m_pos < parser->m_length && parser->m_json[parser->m_pos] == '.') {
				parser->m_pos++;
				if (parser->m_pos >= parser->m_length || !isdigit(parser->m_json[parser->m_pos])) {
					return nullptr;
				}
				while (parser->m_pos < parser->m_length && isdigit(parser->m_json[parser->m_pos])) {
					parser->m_pos++;
				}
			}

			// Exponent
			if (parser->m_pos < parser->m_length &&
				(parser->m_json[parser->m_pos] == 'e' || parser->m_json[parser->m_pos] == 'E')) {
				parser->m_pos++;
				if (parser->m_pos < parser->m_length &&
					(parser->m_json[parser->m_pos] == '+' || parser->m_json[parser->m_pos] == '-')) {
					parser->m_pos++;
				}
				if (parser->m_pos >= parser->m_length || !isdigit(parser->m_json[parser->m_pos])) {
					return nullptr;
				}
				while (parser->m_pos < parser->m_length && isdigit(parser->m_json[parser->m_pos])) {
					parser->m_pos++;
				}
			}

			char* num_str = parser->m_allocator->strndup(parser->m_json + start, parser->m_pos - start);
			if (!num_str) return nullptr;

			f64 value = strtod(num_str, nullptr);
			parser->m_allocator->deallocate(num_str);

			return json_number(parser->m_allocator, value);
		}

		// Parse array
		static JsonValue* parse_array(JsonParser* parser) {
			if (parser->m_json[parser->m_pos] != '[') {
				return nullptr;
			}

			parser->m_pos++; // Skip '['
			skip_whitespace(parser);

			JsonValue* array = json_array(parser->m_allocator);
			if (!array) return nullptr;

			// Empty array
			if (parser->m_pos < parser->m_length && parser->m_json[parser->m_pos] == ']') {
				parser->m_pos++;
				return array;
			}

			while (parser->m_pos < parser->m_length) {
				JsonValue* element = parse_value(parser);
				if (!element) {
					json_free_value(array);
					return nullptr;
				}

				if (!json_array_append(array, element)) {
					json_free_value(element);
					json_free_value(array);
					return nullptr;
				}

				skip_whitespace(parser);

				if (parser->m_pos >= parser->m_length) {
					json_free_value(array);
					return nullptr;
				}

				if (parser->m_json[parser->m_pos] == ']') {
					parser->m_pos++;
					return array;
				}

				if (parser->m_json[parser->m_pos] == ',') {
					parser->m_pos++;
					skip_whitespace(parser);
				}
				else {
					json_free_value(array);
					return nullptr;
				}
			}

			json_free_value(array);
			return nullptr;
		}

		// Parse object
		static JsonValue* parse_object(JsonParser* parser) {
			if (parser->m_json[parser->m_pos] != '{') {
				return nullptr;
			}

			parser->m_pos++; // Skip '{'
			skip_whitespace(parser);

			JsonValue* object = json_object(parser->m_allocator);
			if (!object) return nullptr;

			// Empty object
			if (parser->m_pos < parser->m_length && parser->m_json[parser->m_pos] == '}') {
				parser->m_pos++;
				return object;
			}

			while (parser->m_pos < parser->m_length) {
				// Parse key
				if (parser->m_json[parser->m_pos] != '"') {
					json_free_value(object);
					return nullptr;
				}

				usize key_length;
				char* key = parse_string_content(parser, &key_length);
				if (!key) {
					json_free_value(object);
					return nullptr;
				}

				skip_whitespace(parser);

				// Expect ':'
				if (parser->m_pos >= parser->m_length || parser->m_json[parser->m_pos] != ':') {
					parser->m_allocator->deallocate(key);
					json_free_value(object);
					return nullptr;
				}

				parser->m_pos++;
				skip_whitespace(parser);

				// Parse value
				JsonValue* value = parse_value(parser);
				if (!value) {
					parser->m_allocator->deallocate(key);
					json_free_value(object);
					return nullptr;
				}

				if (!json_object_set(object, key, value)) {
					parser->m_allocator->deallocate(key);
					json_free_value(value);
					json_free_value(object);
					return nullptr;
				}

				parser->m_allocator->deallocate(key);
				skip_whitespace(parser);

				if (parser->m_pos >= parser->m_length) {
					json_free_value(object);
					return nullptr;
				}

				if (parser->m_json[parser->m_pos] == '}') {
					parser->m_pos++;
					return object;
				}

				if (parser->m_json[parser->m_pos] == ',') {
					parser->m_pos++;
					skip_whitespace(parser);
				}
				else {
					json_free_value(object);
					return nullptr;
				}
			}

			json_free_value(object);
			return nullptr;
		}

		// Parse value
		static JsonValue* parse_value(JsonParser* parser) {
			skip_whitespace(parser);

			if (parser->m_pos >= parser->m_length) {
				return nullptr;
			}

			char c = parser->m_json[parser->m_pos];

			// null
			if (c == 'n') {
				if (parser->m_pos + 4 <= parser->m_length &&
					memcmp(parser->m_json + parser->m_pos, "null", 4) == 0) {
					parser->m_pos += 4;
					return json_null(parser->m_allocator);
				}
				return nullptr;
			}

			// true
			if (c == 't') {
				if (parser->m_pos + 4 <= parser->m_length &&
					memcmp(parser->m_json + parser->m_pos, "true", 4) == 0) {
					parser->m_pos += 4;
					return json_bool(parser->m_allocator, true);
				}
				return nullptr;
			}

			// false
			if (c == 'f') {
				if (parser->m_pos + 5 <= parser->m_length &&
					memcmp(parser->m_json + parser->m_pos, "false", 5) == 0) {
					parser->m_pos += 5;
					return json_bool(parser->m_allocator, false);
				}
				return nullptr;
			}

			// string
			if (c == '"') {
				usize length;
				char* str = parse_string_content(parser, &length);
				if (!str) return nullptr;
				JsonValue* value = json_string_len(parser->m_allocator, str, length);
				parser->m_allocator->deallocate(str);
				return value;
			}

			// number
			if (c == '-' || isdigit(c)) {
				return parse_number(parser);
			}

			// array
			if (c == '[') {
				return parse_array(parser);
			}

			// object
			if (c == '{') {
				return parse_object(parser);
			}

			return nullptr;
		}
	}

	JsonValue* json_parse(const Allocator* alloc, const char* json) {
        if (!alloc || !json) {
            return nullptr;
        }
		return json_parse_len(alloc, json, strlen(json));
	}

	JsonValue* json_parse_len(const Allocator* alloc, const char* json, usize length) {
		if (!alloc || !json || length == 0) {
			return nullptr;
		}

		JsonParser parser = {
			.m_json = json,
			.m_pos = 0,
			.m_length = length,
			.m_allocator = alloc
		};

		JsonValue* value = detail::parse_value(&parser);

		if (value) {
			detail::skip_whitespace(&parser);
			if (parser.m_pos < parser.m_length) {
				json_free_value(value);
				return nullptr;
			}
		}

		return value;
	}
}