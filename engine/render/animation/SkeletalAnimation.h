#pragma once

#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/blending_job.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/base/log.h"
#include "ozz/base/maths/math_ex.h"
#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/maths/vec_float.h"
#include "ozz/options/options.h"
#include "ozz/base/containers/vector.h"
#include "ozz/base/span.h"
#include "resources/SkeletalAnimationResource.h"
#include "resources/SkeletonResource.h"
#include "utils/DataStructures.h"
#include <magic_enum/magic_enum.hpp>
#include "utils/EventDispatcher.h"

namespace SkeletalAnimation
{
	enum class PlaybackMode
	{
		Once,
		Loop,
		Clamp
	};

	enum class BlendingMode
	{
		Normal,
		Additive
	};

	enum class EventType
	{
		Start,
		Complete,
		Loop
	};

	struct EventParam
	{
		uint64_t id = 0;
	};

	typedef utils::EventDispatcher<EventType, EventParam> Dispatcher;

	constexpr float DEFAULT_FADE_TIME = 0.3f;

	struct PlaybackParams
	{
		PlaybackMode playback_mode = PlaybackMode::Once;
		BlendingMode blending_mode = BlendingMode::Normal;
		float fade_time = DEFAULT_FADE_TIME;
		uint32_t layer = 0;

		PlaybackParams& Playback(PlaybackMode value) { playback_mode = value; return *this; }
		PlaybackParams& Loop() { return Playback(PlaybackMode::Loop); }
		PlaybackParams& Once() { return Playback(PlaybackMode::Once); }
		PlaybackParams& Clamp() { return Playback(PlaybackMode::Clamp); }
		PlaybackParams& Blending(BlendingMode value) { blending_mode = value; return *this; }
		PlaybackParams& FadeTime(float value) { fade_time = value; return *this; }
		PlaybackParams& Layer(uint32_t value) { layer = value; return *this; }
	};

	class AnimationInstance
	{
	public:
		class Handle
		{
		public:
			Handle(std::shared_ptr<AnimationInstance> instance) : weak_reference(instance) {}
			Handle() = default;
			Handle(const Handle&) = default;
			Handle(Handle&&) = default;
			Handle& operator=(Handle&&) = default;
			Handle& operator=(const Handle&) = default;
			Handle& operator=(std::nullptr_t) { weak_reference.reset(); return *this; };

			operator bool() const { return !weak_reference.expired(); }

			bool Play()
			{
				if (auto instance = weak_reference.lock())
					instance->Play();
				else
					return false;

				return true;
			}

			bool Pause()
			{
				if (auto instance = weak_reference.lock())
					instance->Pause();
				else
					return false;

				return true;
			}

			bool SetProgress(float progress)
			{
				if (auto instance = weak_reference.lock())
					instance->SetProgress(progress);
				else
					return false;

				return true;
			}

			bool SetWeight(float weight) 
			{
				if (auto instance = weak_reference.lock())
					instance->SetWeight(weight);
				else
					return false;

				return true;
			}

			float GetWeight()
			{
				if (auto instance = weak_reference.lock())
					return instance->GetWeight();
				else
					return 0;
			}

			bool SetSpeed(float speed)
			{
				if (auto instance = weak_reference.lock())
					instance->SetSpeed(speed);
				else
					return false;

				return true;
			}

			bool FadeOut(float fade_duration_seconds, bool finish = true)
			{
				if (auto instance = weak_reference.lock())
					instance->FadeOut(fade_duration_seconds, finish);
				else
					return false;

				return true;
			}

		private:
			std::weak_ptr<AnimationInstance> weak_reference;
		};

		struct BlendEvent
		{
			enum class Type { Blend, Finish };
			float start_progress;
			float duration_progress;
			float start_value;
			float target_value;
			Type type = Type::Blend;
		};

		AnimationInstance(uint64_t id, Resources::SkeletonResource::Handle skeleton, Resources::SkeletalAnimationResource::Handle animation, const PlaybackParams& params);

		void Play()
		{
			is_playing = true;
		}

		void Pause()
		{
			is_playing = false;
		}

		bool GetIsPlaying() const { return is_playing; }

		void SetProgress(float progress)
		{
			this->progress = std::min(std::max(0.0f, progress), 1.0f);
			if (root_motion_enabled)
				should_skip_root_update = true;
		}

		float GetProgress() const { return progress; }

		void SetWeight(float weight)
		{
			this->weight = weight;
		}

		float GetWeight() const { return weight; }

		void SetSpeed(float speed)
		{
			this->speed = speed;
		}

		float GetSpeed() const { return speed; }

		bool IsFinished() const { return finished; }

		uint32_t GetLayer() const { return layer; }

		const auto& GetAnimation() { return animation; }
		void Update(float dt, Dispatcher& dispatcher);
		void RemoveBlendEvents();

		void FadeIn(float fade_duration_seconds);
		void FadeOut(float fade_duration_seconds, bool finish = true);
		void AddBlendEvent(BlendEvent::Type type, float start, float duration = 0, float start_value = 0, float target_value = 0);
		BlendingMode GetBlendingMode() const { return blend_mode; }

		auto& GetCache() { return cache; }
		auto& GetLocals() { return locals; }
		vec3 GetLastRootPosition() const { return last_root_position; }
		void SetLastRootPosition(vec3 value) { last_root_position = value; }
		bool GetShouldSkipRootUpdate() const { return should_skip_root_update; }
		void SetShouldSkipRootUpdate(bool value) { should_skip_root_update = value; }
		uint64_t GetID() const { return id; }

		bool GetRootMotionEnabled() const { return root_motion_enabled; }
		void SetRootMotionEnabled(bool value) { root_motion_enabled = value; }

	private:
		void UpdateBlendEvents(float progress);

	private:
		uint64_t id = 0;
		Resources::SkeletonResource::Handle skeleton;
		Resources::SkeletalAnimationResource::Handle animation_resource;
		const ozz::animation::Animation& animation;
		const float duration;
		const PlaybackMode playback_mode;

		// Sampling cache.
		ozz::animation::SamplingCache cache;

		// Buffer of local transforms as sampled from animation.
		ozz::vector<ozz::math::SoaTransform> locals;

		BlendingMode blend_mode = BlendingMode::Normal;
		bool is_playing = false;
		float progress = 0; // Progress of animation in range 0..1
		float weight = 1.0f;
		float speed = 1.0f;
		bool finished = false;
		uint32_t layer = 0;

		bool should_skip_root_update = true;
		bool root_motion_enabled = false;
		uint32_t root_motion_bone_index = 0;
		vec3 last_root_position = vec3(0);
		utils::SmallVector<BlendEvent, 4> blend_events;
	};

	class AnimationMixer
	{
	public:
		static constexpr uint32_t SOCKET_OFFSET = 10000; // Bone indices that greater or equal than this are sockets

		class Socket
		{
			friend AnimationMixer;

			uint32_t name_hash = 0;
			uint32_t parent_bone_index = 0;
			vec3 position = vec3(0);
			quat rotation;
			vec3 scale = vec3(1);
			mutable mat4 matrix = mat4(1); // local TRS combined
			mutable bool dirty = true;
			mat4 local_to_model = mat4(1); // not worldspace yet (Transform component not included here)

		public:
			Socket(const char* name, const uint32_t parent_bone_index)
				: name_hash(FastHash(name))
				, parent_bone_index(parent_bone_index)
			{
			}

			uint32_t GetParentBone() const { return parent_bone_index; }
			uint32_t GetNameHash() const { return name_hash; }
			void SetPosition(const vec3& value) { if (position != value) dirty = true; position = value; }
			const vec3 GetPosition() const { return position; }
			void SetRotation(const quat& value) { if (rotation != value) dirty = true; rotation = value; }
			const quat GetRotation() const { return rotation; }
			void SetScale(const vec3& value) { if (scale != value) dirty = true; scale = value;  }
			const vec3 GetScale() const { return scale; }

			vec3 GetModelPosition() const { return vec3(local_to_model[3]); }
			const vec3 Left() const { return -Right(); }
			const vec3 Up() const { return vec3(local_to_model[1]); }
			const vec3 Forward() const { return -Backward(); }
			const vec3 Right() const { return vec3(local_to_model[0]); }
			const vec3 Down() const { return -Up(); }
			const vec3 Backward() const { return vec3(local_to_model[2]); }

			const mat4& GetLocalMatrix() const;
			const mat4& GetModelMatrix() const { return local_to_model; };

		};

		AnimationMixer(Resources::SkeletonResource::Handle skeleton);
		~AnimationMixer();

		void Update(float dt);
		void ProcessBlending();
		void FadeOutAllAnimations(float fade_duration, uint32_t layer);
		AnimationInstance::Handle PlayAnimation(Resources::SkeletalAnimationResource::Handle animation, const PlaybackParams& params = {});
		AnimationInstance::Handle BlendAnimation(Resources::SkeletalAnimationResource::Handle animation, const PlaybackParams& params = {});
		Resources::SkeletonResource::Handle GetSkeleton() const { return skeleton; }

		const ozz::span<const ozz::math::Float4x4> GetModelMatrices() const { return ozz::make_span(model_matrices); } // Doesn't include sockets
		const mat4* GetModelMatrix(uint32_t index) const; // includes sockets

		const vec3& GetRootOffset() const { return root_offset; };
		void SetRootMotionEnabled(bool value) { root_motion = value; }
		bool GetRootMotionEnabled() const { return root_motion; }
		Dispatcher::Handle AddCallback(Dispatcher::Callback callback) { return dispatcher.AddCallback(callback); };
		Socket* AddSocket(const char* name, uint32_t parent_index);
		void RemoveSocket(const char* name);
		Socket* GetSocket(const char* name);
		const Socket* GetSocket(const char* name) const;
		std::optional<uint32_t> GetSocketIndex(const char* name) const;

	private:
		void UpdateSockets();

		uint64_t instance_counter = 0;
		Resources::SkeletonResource::Handle skeleton;
		std::vector<std::shared_ptr<AnimationInstance>> instances;
		std::array<std::vector<ozz::animation::BlendingJob::Layer>, magic_enum::enum_count<BlendingMode>()> blend_layers;
		Dispatcher dispatcher;

		std::vector<Socket> sockets;

		// Buffer of local transforms which stores the blending result.
		ozz::vector<ozz::math::SoaTransform> blended_locals;

		// Buffer of model space matrices. These are computed by the local-to-model
		// job after the blending stage.
		ozz::vector<ozz::math::Float4x4> model_matrices;

		bool root_motion = false;
		uint32_t root_motion_bone = 0;
		vec3 root_offset = vec3(0);
	};
}