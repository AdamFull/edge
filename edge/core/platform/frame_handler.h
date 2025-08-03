#pragma once

#include <chrono>
#include <numeric>

#ifdef EDGE_PLATFORM_APPLE
#include <dispatch/dispatch.h>
#include <mach/mach_time.h>
#endif

namespace edge {
	class FrameHandlerBase {
	public:
		using handler_t = int32_t(*)(float delta_time, void* user_data);
		using duration_t = std::chrono::high_resolution_clock::duration;
		using timepoint_t = std::chrono::high_resolution_clock::time_point;

		auto set_limit(this auto&& self, int32_t fps) -> void {
			using namespace std::chrono;
			self.target_frame_time_ = duration_cast<high_resolution_clock::duration>(duration<double>(1.0 / static_cast<double>(fps)));
			self.last_frame_time_ = std::chrono::high_resolution_clock::now();
		}

		auto setup_callback(this auto&& self, handler_t handler, void* user_data = nullptr) -> void {
			self.callback_.handler = std::move(handler);
			self.callback_.user_data = user_data;
		}

		auto sleep(this auto&& self, double seconds) -> void {
			// Adaptive algorithm (same across all platforms)
			while (seconds - self.estimate_ > 1e-7) {
				double to_wait = seconds - self.estimate_;

				auto start = std::chrono::high_resolution_clock::now();
				self.sleep_(to_wait);
				auto end = std::chrono::high_resolution_clock::now();

				double observed = std::chrono::duration<double>(end - start).count();
				seconds -= observed;

				// Update statistics using Welford's online algorithm
				++self.count_;
				double error = observed - to_wait;
				double delta = error - self.mean_;
				self.mean_ += delta / self.count_;
				self.m2_ += delta * (error - self.mean_);
				double stddev = self.count_ > 1 ? std::sqrt(self.m2_ / (self.count_ - 1)) : 0.0;
				self.estimate_ = self.mean_ + stddev;
			}

			// Spin lock for remaining time (cross-platform)
			auto start = std::chrono::high_resolution_clock::now();
			auto spin_duration = std::chrono::duration<double>(seconds);
			while (std::chrono::high_resolution_clock::now() - start < spin_duration) {
				// Tight spin loop for maximum precision
			}
		}

		auto process(this auto&& self) -> int32_t {
			if (!self.callback_.handler) {
				return 0;
			}

			auto target_time = self.last_frame_time_ + self.target_frame_time_;
			auto current_time = std::chrono::high_resolution_clock::now();

			float delta_time = std::chrono::duration<float>(current_time - self.prev_time_).count();

			if (self.frame_time_accumulator_ > 1.0f) {
				self.mean_fps_ = self.frame_counter_;
				self.mean_frame_time_ = self.frame_time_accumulator_ / static_cast<float>(self.frame_counter_);

				self.frame_time_accumulator_ = 0.0f;
				self.frame_counter_ = 0;
			}
			else {
				self.frame_time_accumulator_ += delta_time;
				self.frame_counter_ += 1;
			}

			int32_t result = self.callback_.handler(delta_time, self.callback_.user_data);

			// Handle fixed frame time
			if (self.first_frame_) {
				self.prev_time_ = self.last_frame_time_ = std::chrono::high_resolution_clock::now();
				self.first_frame_ = false;
				return 0;
			}

			if (current_time < target_time) {
				auto remaining_duration = target_time - current_time;
				double remaining_seconds = std::chrono::duration<double>(remaining_duration).count();
				self.sleep(remaining_seconds);
			}

			self.last_frame_time_ = target_time;
			self.prev_time_ = current_time;

			return result;
		}

		auto get_fps(this auto&& self) -> uint32_t {
			return self.mean_fps_;
		}

		auto get_mean_frame_time(this auto&& self) -> float {
			return self.mean_frame_time_;
		}
	protected:
		std::chrono::high_resolution_clock::duration target_frame_time_;
		timepoint_t last_frame_time_;
		timepoint_t prev_time_;
		bool first_frame_{ true };

		double estimate_ = 5e-3;
		double mean_ = 5e-3;
		double m2_ = 0.0;
		int64_t count_ = 1;

		float frame_time_accumulator_{ 0.0f };
		uint32_t frame_counter_{ 0 };
		uint32_t mean_fps_{ 0 };
		float mean_frame_time_{ 0.0f };

		struct {
			handler_t handler;
			void* user_data;
		} callback_;
	};
}

#ifdef EDGE_PLATFORM_WINDOWS
#include "windows/frame_handler.h"
#elif defined(EDGE_PLATFORM_LINUX)
#include "linux/frame_handler.h"
#elif defined(EDGE_PLATFORM_ANDROID)
#include "android/frame_handler.h"
#elif defined(EDGE_PLATFORM_APPLE)
#include "apple/frame_handler.h"
#else
#include "generic/frame_handler.h"
#endif