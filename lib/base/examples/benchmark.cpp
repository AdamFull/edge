#include <buffer.hpp>
#include <hashmap.hpp>
#include <random.hpp>

#include <chrono>
#include <unordered_map>

namespace edge {
	struct string {
		char* data;
		usize len;
	};

	bool operator==(const string& lhs, const string& rhs) {
		if (lhs.len != rhs.len) {
			return false;
		}
		return !strncmp(lhs.data, rhs.data, lhs.len);
	}

	struct DefaultStringHash {
		usize operator()(const string& str) const noexcept {
			return hash_xxh64(str.data, str.len);
		}
	};

	struct DefaultStringEqual {
		bool operator()(const string& s1, const string& s2) const noexcept {
			if (s1.len != s2.len) {
				return false;
			}
			return !strncmp(s1.data, s2.data, s1.len);
		}
	};
}

namespace std {
	template<>
	struct hash<edge::string> {
		size_t operator()(const edge::string& s) const noexcept {
			return edge::hash_xxh32(s.data, s.len);
		}
	};
}

using HashMap1 = edge::HashMap<const edge::string, i32, edge::DefaultStringHash, edge::DefaultStringEqual>;

constexpr usize DATASET_SIZE = 2000;

using DatasetStorage = edge::StackStorage<edge::string, DATASET_SIZE>;

static void generate_dataset1(edge::NotNull<const edge::Allocator*> alloc, DatasetStorage& output, usize count, edge::RngPCGGen& rng) {
	static const char* prefixes[] = { "pre", "post", "un", "re", "anti", "de", "dis", "en", "in", "inter", "over", "sub", "trans", "under", "co", "mis", "non", "out" };
	static const char* roots[] = { "act", "form", "port", "dict", "scribe", "ject", "tract", "mit", "fer", "duc", "pose", "pone", "sta", "vert", "cede", "cess", "struct", "spect", "gress", "press" };
	static const char* suffixes[] = { "tion", "ness", "ment", "able", "ible", "ful", "less", "ive", "ous", "al", "er", "or", "ing", "ed", "ly", "ity", "ism", "ist", "ence", "ance" };
	
	char buffer[64] = {};
	for (usize i = 0; i < count; ++i) {
		usize prefix_idx = rng.gen_u32_bounded(edge::array_size(prefixes));
		usize root_idx = rng.gen_u32_bounded(edge::array_size(roots));
		usize suffix_idx = rng.gen_u32_bounded(edge::array_size(suffixes));

		i32 word_size = 0;
		if (i % 4 == 0) {
			word_size = snprintf(buffer, 64, "%s%s%s", prefixes[prefix_idx], roots[root_idx], suffixes[suffix_idx]);
		}
		else if (i % 4 == 1) {
			word_size = snprintf(buffer, 64, "%s%s", roots[root_idx], suffixes[suffix_idx]);
		}
		else if (i % 4 == 2) {
			word_size = snprintf(buffer, 64, "%s%s%zu", prefixes[prefix_idx], roots[root_idx], i);
		}
		else {
			word_size = snprintf(buffer, 64, "word_%s_%s_%zu", roots[root_idx], suffixes[suffix_idx], i);
		}

		edge::string& str = output[i];
		str.data = (char*)alloc->malloc(word_size + 1);
		str.len = word_size;
		memcpy(str.data, buffer, word_size);
		str.data[word_size] = '\0';
	}
}

static void run_bench(edge::NotNull<const edge::Allocator*> alloc, DatasetStorage& dataset, usize word_count) {
	HashMap1 map = {};
	if (!map.create(alloc, word_count * 2)) {
		printf("Failed to create hash map\n");
		return;
	}

	for (usize i = 0; i < word_count; ++i) {
		map.insert(alloc, dataset[i], static_cast<i32>(i));
	}

	printf("\n==============================================================");
	printf("\n===================== Dataset Benchmark ======================");
	printf("\n==============================================================\n");

	printf("Total entries: %zu\n", map.size());
	printf("Load factor: %.2f\n", map.load_factor());
	printf("Bucket count: %zu\n", map.m_bucket_count);

	printf("\nWarming up (100,000 lookups)...\n");
	for (usize warmup = 0; warmup < 100000; ++warmup) {
		usize idx = warmup % word_count;
		volatile i32* value = map.get(dataset[idx]);
		(void)value;
	}

	printf("Running sequential lookup benchmark...\n");
	constexpr usize NUM_ITERATIONS = 10000000;
	usize successful = 0;

	auto start = std::chrono::high_resolution_clock::now();

	for (usize i = 0; i < NUM_ITERATIONS; ++i) {
		usize idx = i % word_count;
		i32* value = map.get(dataset[idx]);
		if (value) {
			successful++;
		}
	}

	auto end = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

	f64 total_time_seconds = duration.count() / 1e9;
	f64 lookups_per_second = NUM_ITERATIONS / total_time_seconds;
	f64 avg_lookup_time_ns = duration.count() / static_cast<f64>(NUM_ITERATIONS);

	printf("\n------ Sequential Access Results ------\n");
	printf("Total lookups:         %zu\n", NUM_ITERATIONS);
	printf("Total time:            %.4f seconds\n", total_time_seconds);
	printf("Lookups per second:    %.2f M/s\n", lookups_per_second / 1e6);
	printf("Average lookup time:   %.2f ns\n", avg_lookup_time_ns);
	printf("Successful lookups:    %zu (%.1f%%)\n", successful, (successful * 100.0) / NUM_ITERATIONS);

	printf("\n------ Random Access Pattern ------\n");
	edge::RngPCGGen rng;
	rng.set_seed(0x12345678);
	successful = 0;

	start = std::chrono::high_resolution_clock::now();

	for (usize i = 0; i < NUM_ITERATIONS; ++i) {
		usize idx = rng.gen_u32_bounded(word_count);
		i32* value = map.get(dataset[idx]);
		if (value) successful++;
	}

	end = std::chrono::high_resolution_clock::now();
	duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

	total_time_seconds = duration.count() / 1e9;
	lookups_per_second = NUM_ITERATIONS / total_time_seconds;
	avg_lookup_time_ns = duration.count() / static_cast<f64>(NUM_ITERATIONS);

	printf("Total time:            %.4f seconds\n", total_time_seconds);
	printf("Lookups per second:    %.2f M/s\n", lookups_per_second / 1e6);
	printf("Average lookup time:   %.2f ns\n", avg_lookup_time_ns);
	printf("Successful lookups:    %zu (%.1f%%)\n", successful, (successful * 100.0) / NUM_ITERATIONS);

	printf("\n------ 50%% Hit Rate (with misses) ------\n");
	rng.set_seed(0xDEADBEEF);
	usize hits = 0;

	start = std::chrono::high_resolution_clock::now();

	for (usize i = 0; i < NUM_ITERATIONS; ++i) {
		char temp_key[64];
		if (rng.gen_bool(0.5f)) {
			usize idx = rng.gen_u32_bounded(word_count);
			i32* value = map.get(dataset[idx]);
			if (value) hits++;
		}
		else {
			usize len = snprintf(temp_key, 64, "nonexistent_%zu_%x", i, rng.gen_u32());
			edge::string tmp_str = { .data = temp_key, .len = len };
			i32* value = map.get(tmp_str);
			(void)value;
		}
	}

	end = std::chrono::high_resolution_clock::now();
	duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

	total_time_seconds = duration.count() / 1e9;
	lookups_per_second = NUM_ITERATIONS / total_time_seconds;
	avg_lookup_time_ns = duration.count() / static_cast<f64>(NUM_ITERATIONS);

	printf("Total time:            %.4f seconds\n", total_time_seconds);
	printf("Lookups per second:    %.2f M/s\n", lookups_per_second / 1e6);
	printf("Average lookup time:   %.2f ns\n", avg_lookup_time_ns);
	printf("Hit rate:              %.1f%%\n", (hits * 100.0) / NUM_ITERATIONS);

	map.destroy(alloc);
}

static void run_bench_std(DatasetStorage& dataset, usize word_count) {
	std::unordered_map<edge::string, i32> map = {};
	map.reserve(word_count * 2);

	for (usize i = 0; i < word_count; ++i) {
		map[dataset[i]] = static_cast<i32>(i);
	}

	printf("\n==============================================================");
	printf("\n=================== STL Dataset Benchmark =====================");
	printf("\n==============================================================\n");

	printf("Total entries: %zu\n", map.size());
	printf("Load factor: %.2f\n", map.load_factor());
	printf("Bucket count: %zu\n", map.bucket_count());

	printf("\nWarming up (100,000 lookups)...\n");
	for (usize warmup = 0; warmup < 100000; ++warmup) {
		usize idx = warmup % word_count;
		auto value = map.find(dataset[idx]);
		if (value != map.end()) {
			(void)value;
		}
	}

	printf("Running sequential lookup benchmark...\n");
	constexpr usize NUM_ITERATIONS = 10000000;
	usize successful = 0;

	auto start = std::chrono::high_resolution_clock::now();

	for (usize i = 0; i < NUM_ITERATIONS; ++i) {
		usize idx = i % word_count;
		auto value = map.find(dataset[idx]);
		if (value != map.end()) {
			successful++;
		}
	}

	auto end = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

	f64 total_time_seconds = duration.count() / 1e9;
	f64 lookups_per_second = NUM_ITERATIONS / total_time_seconds;
	f64 avg_lookup_time_ns = duration.count() / static_cast<f64>(NUM_ITERATIONS);

	printf("\n------ Sequential Access Results ------\n");
	printf("Total lookups:         %zu\n", NUM_ITERATIONS);
	printf("Total time:            %.4f seconds\n", total_time_seconds);
	printf("Lookups per second:    %.2f M/s\n", lookups_per_second / 1e6);
	printf("Average lookup time:   %.2f ns\n", avg_lookup_time_ns);
	printf("Successful lookups:    %zu (%.1f%%)\n", successful, (successful * 100.0) / NUM_ITERATIONS);

	printf("\n------ Random Access Pattern ------\n");
	edge::RngPCGGen rng;
	rng.set_seed(0x12345678);
	successful = 0;

	start = std::chrono::high_resolution_clock::now();

	for (usize i = 0; i < NUM_ITERATIONS; ++i) {
		usize idx = rng.gen_u32_bounded(word_count);
		auto value = map.find(dataset[idx]);
		if (value != map.end()) {
			successful++;
		}
	}

	end = std::chrono::high_resolution_clock::now();
	duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

	total_time_seconds = duration.count() / 1e9;
	lookups_per_second = NUM_ITERATIONS / total_time_seconds;
	avg_lookup_time_ns = duration.count() / static_cast<f64>(NUM_ITERATIONS);

	printf("Total time:            %.4f seconds\n", total_time_seconds);
	printf("Lookups per second:    %.2f M/s\n", lookups_per_second / 1e6);
	printf("Average lookup time:   %.2f ns\n", avg_lookup_time_ns);
	printf("Successful lookups:    %zu (%.1f%%)\n", successful, (successful * 100.0) / NUM_ITERATIONS);

	printf("\n------ 50%% Hit Rate (with misses) ------\n");
	rng.set_seed(0xDEADBEEF);
	usize hits = 0;

	start = std::chrono::high_resolution_clock::now();

	for (usize i = 0; i < NUM_ITERATIONS; ++i) {
		char temp_key[64];
		if (rng.gen_bool(0.5f)) {
			usize idx = rng.gen_u32_bounded(word_count);
			auto value = map.find(dataset[idx]);
			if (value != map.end()) {
				hits++;
			}
		}
		else {
			usize len = snprintf(temp_key, 64, "nonexistent_%zu_%x", i, rng.gen_u32());
			edge::string tmp_str = { .data = temp_key, .len = len };
			volatile auto value = map.find(tmp_str);
			(void)value;
		}
	}

	end = std::chrono::high_resolution_clock::now();
	duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

	total_time_seconds = duration.count() / 1e9;
	lookups_per_second = NUM_ITERATIONS / total_time_seconds;
	avg_lookup_time_ns = duration.count() / static_cast<f64>(NUM_ITERATIONS);

	printf("Total time:            %.4f seconds\n", total_time_seconds);
	printf("Lookups per second:    %.2f M/s\n", lookups_per_second / 1e6);
	printf("Average lookup time:   %.2f ns\n", avg_lookup_time_ns);
	printf("Hit rate:              %.1f%%\n", (hits * 100.0) / NUM_ITERATIONS);
}

int main(void) {
	edge::Allocator alloc = edge::Allocator::create_tracking();

	edge::RngPCGGen rng = {};
	rng.set_seed(42);

	DatasetStorage words_dataset = {};
	generate_dataset1(&alloc, words_dataset, DATASET_SIZE, rng);

	run_bench(&alloc, words_dataset, DATASET_SIZE);
	run_bench_std(words_dataset, DATASET_SIZE);
	
	for (edge::string str : words_dataset) {
		alloc.deallocate(str.data);
	}

	return 0;
}