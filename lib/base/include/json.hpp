#ifndef EDGE_JSON_H
#define EDGE_JSON_H

#include "array.hpp"
#include "hashmap.hpp"
#include "string.hpp"
#include "string_view.hpp"
#include "span.hpp"

namespace edge {
	enum class JsonValueType {
		Null,
		Boolean,
		Number,
		String,
		Array,
		Object
	};

	enum class JsonError {
		None = 0,
		OutOfMemory,
		UnexpectedToken,
		InvalidNumber,
		InvalidString,
		InvalidEscape,
		UnterminatedString,
		ExpectedColon,
		ExpectedComma,
		ExpectedValue,
		TrailingComma,
		InvalidUtf8
	};

	struct JsonErrorDesc {
		JsonError error;
		usize line;
		usize pos;
	};

	struct JsonValue;

	struct JsonValue {
		JsonValueType type = JsonValueType::Null;
		union {
			bool boolean;
			double number;
			String string;
			Array<JsonValue> array;
			HashMap<String, JsonValue> object = {};
		};

		JsonValue() noexcept
			: type{ JsonValueType::Null } {

		}

		[[nodiscard]] static JsonValue create() noexcept {
			return {};
		}

		[[nodiscard]] static JsonValue create(bool val) noexcept {
			JsonValue json = {};
			json.type = JsonValueType::Boolean;
			json.boolean = val;
			return json;
		}

		[[nodiscard]] static JsonValue create(double val) noexcept {
			JsonValue json = {};
			json.type = JsonValueType::Number;
			json.number = val;
			return json;
		}

		[[nodiscard]] static JsonValue create(String val) noexcept {
			JsonValue json = {};
			json.type = JsonValueType::String;
			json.string = val;
			return json;
		}

		[[nodiscard]] static JsonValue create(Array<JsonValue> val) noexcept {
			JsonValue json = {};
			json.type = JsonValueType::Array;
			json.array = val;
			return json;
		}

		[[nodiscard]] static JsonValue create(HashMap<String, JsonValue> val) noexcept {
			JsonValue json = {};
			json.type = JsonValueType::Object;
			json.object = val;
			return json;
		}

		void destroy(NotNull<const Allocator*> alloc) noexcept;
	};

	Result<JsonValue, JsonErrorDesc> json_parse(NotNull<const Allocator*> alloc, StringView<char8_t> content);
	String to_string(NotNull<const Allocator*> alloc, JsonValue json);
}

#endif