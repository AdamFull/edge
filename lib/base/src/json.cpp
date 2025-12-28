#include "json.hpp"

#include <cctype>

namespace edge {
	struct JsonParser {
		StringView<char8_t> content = {};
		usize pos = 0;
		usize line_pos = 0;
		usize line = 0;

		Result<JsonValue, JsonErrorDesc> parse(NotNull<const Allocator*> alloc) noexcept {
			skip_whitespaces();
			return parse_value(alloc);
		}

		Result<JsonValue, JsonErrorDesc> parse_value(NotNull<const Allocator*> alloc) noexcept {
			skip_whitespaces();

			if (pos >= content.size()) {
				return JsonErrorDesc{
					.error = JsonError::ExpectedValue,
					.line = line,
					.pos = line_pos
				};
			}

			char8_t c = peek();
			switch (c)
			{
			case u8'n':
				return parse_null();
			case u8't':
			case u8'f':
				return parse_boolean();
			case u8'\"': return parse_string(alloc);
			case u8'[': return parse_array(alloc);
			case u8'{': return parse_object(alloc);
			default: {
				if (c == '-' || isdigit(c)) {
					return parse_number();
				}

				return JsonErrorDesc{
					.error = JsonError::UnexpectedToken,
					.line = line,
					.pos = line_pos
				};
			} break;
			}
		}

		Result<JsonValue, JsonErrorDesc> parse_null() noexcept {
			if (match(u8"null")) {
				pos += 4;
				return JsonValue{};
			}

			return JsonErrorDesc{
				.error = JsonError::UnexpectedToken,
				.line = line,
				.pos = line_pos
			};
		}

		Result<JsonValue, JsonErrorDesc> parse_boolean() noexcept {
			if (match(u8"true")) {
				pos += 4;
				return JsonValue::create(true);
			}
			else if (match(u8"false")) {
				pos += 5;
				return JsonValue::create(false);
			}

			return JsonErrorDesc{
				.error = JsonError::UnexpectedToken,
				.line = line,
				.pos = line_pos
			};
		}

		Result<JsonValue, JsonErrorDesc> parse_number() noexcept {
			auto subspan = content.substr(pos, content.size());
			char8_t* parse_end = nullptr;

			f64 value = strtod((const char*)subspan.data(), (char**)(&parse_end));

			if (parse_end == subspan.begin()) {
				return JsonErrorDesc{
				.error = JsonError::InvalidNumber,
				.line = line,
				.pos = line_pos
				};
			}

			if (parse_end > subspan.end()) {
				return JsonErrorDesc{
				.error = JsonError::InvalidNumber,
				.line = line,
				.pos = line_pos
				};
			}

			pos = parse_end - content.data();
			return JsonValue::create(value);
		}

		JsonError parse_unicode_escape(char32_t& codepoint) noexcept {
			for (i32 i = 0; i < 4; ++i) {
				if (pos >= content.size()) {
					return JsonError::InvalidEscape;
				}

				char c = peek();
				pos++;

				u32 digit;
				if (c >= '0' && c <= '9') {
					digit = c - '0';
				}
				else if (c >= 'a' && c <= 'f') {
					digit = 10 + (c - 'a');
				}
				else if (c >= 'A' && c <= 'F') {
					digit = 10 + (c - 'A');
				}
				else {
					return JsonError::InvalidEscape;
				}

				codepoint = (codepoint << 4) | digit;
			}

			return JsonError::None;
		}

		JsonError parse_string_content(NotNull<const Allocator*> alloc, String& str) noexcept {
			while (pos < content.size()) {
				char c = peek();

				if (c == u8'"') {
					return JsonError::None;
				}

				if (c == u8'\\') {
					pos++;
					if (pos >= content.size()) {
						return JsonError::InvalidEscape;
					}

					char8_t escape = peek();
					pos++;

					switch (escape) {
					case u8'"':
					case u8'\\':
					case u8'/':
						if (!str.append(alloc, escape)) {
							return JsonError::OutOfMemory;
						}
						break;
					case u8'b':
						if (!str.append(alloc, u8'\b')) {
							return JsonError::OutOfMemory;
						}
						break;
					case u8'f':
						if (!str.append(alloc, u8'\f')) {
							return JsonError::OutOfMemory;
						}
						break;
					case u8'n':
						if (!str.append(alloc, u8'\n')) {
							return JsonError::OutOfMemory;
						}
						break;
					case u8'r':
						if (!str.append(alloc, u8'\r')) {
							return JsonError::OutOfMemory;
						}
						break;
					case u8't':
						if (!str.append(alloc, u8'\t')) {
							return JsonError::OutOfMemory;
						}
						break;
					case u8'u': {
						char32_t codepoint = 0;
						JsonError res = parse_unicode_escape(codepoint);
						if (res != JsonError::None) {
							return res;
						}
						if (!str.append(alloc, codepoint)) {
							return JsonError::OutOfMemory;
						}
						break;
					}
					default:
						return JsonError::InvalidEscape;
					};
				}
				else if (static_cast<u8>(c) < 0x20) {
					return JsonError::InvalidString;
				}
				else {
					if (!str.append(alloc, (char8_t)c)) {
						return JsonError::OutOfMemory;
					}
					pos++;
				}
			}
			return JsonError::None;
		}

		Result<JsonValue, JsonErrorDesc> parse_string(NotNull<const Allocator*> alloc) noexcept {
			if (!consume(u8'"')) {
				return JsonErrorDesc{
				.error = JsonError::UnexpectedToken,
				.line = line,
				.pos = line_pos
				};
			}

			String string = {};
			JsonError res = parse_string_content(alloc, string);
			if (res != JsonError::None) {
				string.destroy(alloc);
				return JsonErrorDesc{
				.error = res,
				.line = line,
				.pos = line_pos
				};
			}

			if (!consume(u8'"')) {
				string.destroy(alloc);
				return JsonErrorDesc{
				.error = JsonError::UnterminatedString,
				.line = line,
				.pos = line_pos
				};
			}

			return JsonValue::create(string);
		}

		Result<JsonValue, JsonErrorDesc> parse_array(NotNull<const Allocator*> alloc) noexcept {
			if (!consume(u8'[')) {
				return JsonErrorDesc{
				.error = JsonError::UnexpectedToken,
				.line = line,
				.pos = line_pos
				};
			}

			skip_whitespaces();

			if (peek() == u8']') {
				pos++;
				return JsonValue::create(Array<JsonValue>{});
			}

			Array<JsonValue> array = {};

			while (true) {
				Result<JsonValue, JsonErrorDesc> element = parse_value(alloc);
				if (!element) {
					JsonValue::create(array).destroy(alloc);
					return element.error();
				}

				if (!array.push_back(alloc, *element)) {
					element->destroy(alloc);
					JsonValue::create(array).destroy(alloc);
					return JsonErrorDesc{
					.error = JsonError::OutOfMemory,
					.line = line,
					.pos = line_pos
					};
				}

				skip_whitespaces();

				char c = peek();
				if (c == u8']') {
					pos++;
					break;
				}
				else if (c == u8',') {
					pos++;
					skip_whitespaces();

					// Check for trailing comma
					if (peek() == u8']') {
						JsonValue::create(array).destroy(alloc);

						return JsonErrorDesc{
						.error = JsonError::TrailingComma,
						.line = line,
						.pos = line_pos
						};
					}
				}
				else {
					JsonValue::create(array).destroy(alloc);
					
					return JsonErrorDesc{
						.error = JsonError::ExpectedComma,
						.line = line,
						.pos = line_pos
					};
				}
			}

			return JsonValue::create(array);
		}

		Result<JsonValue, JsonErrorDesc> parse_object(NotNull<const Allocator*> alloc) noexcept {
			if (!consume(u8'{')) {
				return JsonErrorDesc{
					.error = JsonError::UnexpectedToken,
					.line = line,
					.pos = line_pos
				};
			}

			skip_whitespaces();

			HashMap<String, JsonValue> object = {};
			if (!object.create(alloc, 4)) {
				return JsonErrorDesc{
						.error = JsonError::OutOfMemory,
						.line = line,
						.pos = line_pos
				};
			}

			if (peek() == u8'}') {
				pos++;
				return JsonValue::create(object);
			}

			while (true) {
				skip_whitespaces();

				if (peek() != u8'"') {
					JsonValue::create(object).destroy(alloc);
					return JsonErrorDesc{
						.error = JsonError::UnexpectedToken,
						.line = line,
						.pos = line_pos
					};
				}

				Result<JsonValue, JsonErrorDesc> key_value = parse_string(alloc);
				if (!key_value) {
					JsonValue::create(object).destroy(alloc);
					return key_value.error();
				}

				skip_whitespaces();

				if (!consume(u8':')) {
					key_value->destroy(alloc);
					JsonValue::create(object).destroy(alloc);
					return JsonErrorDesc{
						.error = JsonError::ExpectedColon,
						.line = line,
						.pos = line_pos
					};
				}

				skip_whitespaces();

				Result<JsonValue, JsonErrorDesc> value = parse_value(alloc);
				if (!value) {
					key_value->destroy(alloc);
					JsonValue::create(object).destroy(alloc);
					return value.error();
				}

				if (!object.insert(alloc, key_value->string, value.value())) {
					key_value->destroy(alloc);
					value->destroy(alloc);
					JsonValue::create(object).destroy(alloc);
					return JsonErrorDesc{
						.error = JsonError::OutOfMemory,
						.line = line,
						.pos = line_pos
					};
				}

				skip_whitespaces();

				char c = peek();
				if (c == u8'}') {
					pos++;
					break;
				}
				else if (c == u8',') {
					pos++;
					skip_whitespaces();

					// Check for trailing comma
					if (peek() == u8'}') {
						JsonValue::create(object).destroy(alloc);

						return JsonErrorDesc{
							.error = JsonError::TrailingComma,
							.line = line,
							.pos = line_pos
						};
					}
				}
				else {
					JsonValue::create(object).destroy(alloc);

					return JsonErrorDesc{
						.error = JsonError::ExpectedComma,
						.line = line,
						.pos = line_pos
					};
				}
			}

			return JsonValue::create(object);
		}

		void skip_whitespaces() noexcept {
			while (pos < content.size()) {
				switch (content[pos])
				{
				case u8'\t':
				case u8'\r':
				case u8' ':
					pos++;
					line_pos++;
					break;
				case u8'\n':
					pos++;
					line++;
					line_pos = 0;
					break;
				default: return;
				}
			}
		}

		char8_t peek() const noexcept {
			if (pos < content.size()) {
				return content[pos];
			}
			return u8'\0';
		}

		char8_t peek_at(usize offset) const noexcept {
			if (pos + offset < content.size()) {
				return content[pos + offset];
			}
			return u8'\0';
		}

		bool consume(char8_t expected) noexcept {
			if (peek() == expected) {
				pos++;
				return true;
			}
			return false;
		}

		bool match(StringView<char8_t> str) noexcept {
			if (pos + str.size() > content.size()) {
				return false;
			}
			auto substr = content.substr(pos, str.size());
			return str.compare(substr) == 0;
		}
	};

	void JsonValue::destroy(NotNull<const Allocator*> alloc) noexcept {
		switch (type)
		{
		case JsonValueType::String: {
			string.destroy(alloc);
			type = JsonValueType::Null;
			break;
		}
		case JsonValueType::Array: {
			for (JsonValue& val : array) {
				val.destroy(alloc);
			}
			array.destroy(alloc);
			break;
		}
		case JsonValueType::Object: {
			for (auto& kv : object) {
				kv.key.destroy(alloc);
				kv.value.destroy(alloc);
			}
			object.destroy(alloc);
			break;
		}
		case JsonValueType::Null:
		case JsonValueType::Boolean:
		case JsonValueType::Number:
		default:
			type = JsonValueType::Null;
			break;
		}
	}

	Result<JsonValue, JsonErrorDesc> json_parse(NotNull<const Allocator*> alloc, StringView<char8_t> content) {
		JsonParser parser = {
			.content = content
		};

		return parser.parse(alloc);
	}

	struct JsonSerializer {
		String output = {};
		bool pretty = true;

		bool append_indent(NotNull<const Allocator*> alloc, i32 indent) noexcept {
			for (i32 i = 0; i < indent; ++i) {
				if (!output.append(alloc, u8"  ")) {
					return false;
				}
			}
			return true;
		}

		// TODO: Return error instead of bool
		bool serialize(NotNull<const Allocator*> alloc, JsonValue value, i32 indent = 1) noexcept {
			switch (value.type)
			{
			case JsonValueType::Null:
				return output.append(alloc, u8"null");
			case JsonValueType::Boolean:
				return output.append(alloc, value.boolean ? u8"true" : u8"false");
			case JsonValueType::Number: {
				char8_t buffer[64];
				i32 len = snprintf((char*)buffer, sizeof(buffer), "%.17g", value.number);
				return output.append(alloc, buffer, len);
			}
			case JsonValueType::String: {
				if (!output.append(alloc, u8'"')) {
					return false;
				}

				for (usize i = 0; i < value.string.m_length; ++i) {
					char8_t c = value.string.m_data[i];
					switch (c) {
					case u8'"':  if (!output.append(alloc, u8"\\\"")) return false; break;
					case u8'\\': if (!output.append(alloc, u8"\\\\")) return false; break;
					case u8'\b': if (!output.append(alloc, u8"\\b")) return false; break;
					case u8'\f': if (!output.append(alloc, u8"\\f")) return false; break;
					case u8'\n': if (!output.append(alloc, u8"\\n")) return false; break;
					case u8'\r': if (!output.append(alloc, u8"\\r")) return false; break;
					case u8'\t': if (!output.append(alloc, u8"\\t")) return false; break;
					default:
						if (static_cast<u8>(c) < 0x20) {
							char8_t escape[7];
							snprintf((char*)escape, sizeof(escape), "\\u%04x", static_cast<u32>(c));
							if (!output.append(alloc, escape)) return false;
						}
						else {
							if (!output.append(alloc, c)) return false;
						}
						break;
					}
				}

				return output.append(alloc, u8'"');
			}
			case JsonValueType::Array: {
				if (!output.append(alloc, u8'[')) {
					return false;
				}

				for (usize i = 0; i < value.array.size(); ++i) {
					if (i > 0) {
						if (!output.append(alloc, u8',')) return false;
					}

					if (pretty) {
						if (!output.append(alloc, u8'\n')) return false;
						if (!append_indent(alloc, indent + 1)) return false;
					}

					if (!serialize(alloc, value.array[i], indent + 1)) {
						return false;
					}
				}

				if (pretty && value.array.size() > 0) {
					if (!output.append(alloc, u8'\n')) return false;
					if (!append_indent(alloc, indent)) return false;
				}

				return output.append(alloc, u8']');
			}
			case JsonValueType::Object: {
				if (!output.append(alloc, u8'{')) {
					return false;
				}

				usize count = 0;

				for (const auto& entry : value.object) {
					if (count > 0) {
						if (!output.append(alloc, u8',')) return false;
					}

					if (pretty) {
						if (!output.append(alloc, u8'\n')) {
							return false;
						}
						if (!append_indent(alloc, indent + 1)) {
							return false;
						}
					}

					// Key
					if (!output.append(alloc, u8'"')) {
						return false;
					}
					if (!output.append(alloc, entry.key.m_data, entry.key.m_length)) {
						return false;
					}
					if (!output.append(alloc, u8'"')) {
						return false;
					}
					if (!output.append(alloc, u8':')) {
						return false;
					}

					if (pretty) {
						if (!output.append(alloc, u8' ')) return false;
					}

					// Value
					if (!serialize(alloc, entry.value, indent + 1)) {
						return false;
					}

					count++;
				}

				if (pretty && count > 0) {
					if (!output.append(alloc, u8'\n')) {
						return false;
					}
					if (!append_indent(alloc, indent)) {
						return false;
					}
				}

				return output.append(alloc, u8'}');
			}
			default:
				break;
			}
		}
	};

	String to_string(NotNull<const Allocator*> alloc, JsonValue json) {
		JsonSerializer serializer = {

		};

		if (serializer.serialize(alloc, json)) {
			return serializer.output;
		}
		return {};
	}
}