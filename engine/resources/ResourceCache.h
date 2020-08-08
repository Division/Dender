#pragma once

#include <atomic>
#include <unordered_map>
#include <memory>
#include "memory/Allocator.h"
#include <fstream>
#include "system/JobSystem.h"
#include <optick/src/optick.h>

namespace Resources
{
	class ResourceBase
	{
	public:
		template<typename> friend class Handle;

		enum class State : int
		{
			Loading,
			Loaded,
			Unloading,
			Unloaded,
		};

		ResourceBase(const std::wstring& path) 
			: filename(path)
			, resource_memory(nullptr)
			, reference_counter(0)
			, state(State::Unloaded)
		{};

		virtual ~ResourceBase() {}

		uint32_t GetRefCount() const { return reference_counter; }
		bool IsResolved() { return state == State::Loaded; }

	protected:
		
		bool TransitionState(State old_state, State new_state)
		{
			return state.compare_exchange_strong(old_state, new_state);
		}

		void AddRef()
		{
			reference_counter += 1;
		}

		void RemoveRef()
		{
			reference_counter -= 1;
		}

		std::atomic<State> state;
		std::atomic_uint32_t reference_counter;
		std::wstring filename;
		void* resource_memory;
	};

	template <typename T, Memory::Tag Tag = Memory::Tag::UnknownResource>
	class Resource : public ResourceBase
	{
	private:
		using Allocator = Memory::TaggedAllocator<T, Tag>;

		class JobLoadResource : public Thread::Job
		{
		public:
			JobLoadResource(Resource* resource)
				: resource(resource)
			{}

			virtual void Execute() override
			{
				Allocator allocator;
				resource->resource_memory = allocator.allocate(1);
				new (resource->resource_memory) T(resource->filename);

				resource->state = State::Loaded;
			}

		private:
			Resource* resource;
		};

	private:
		template<typename T> friend class Handle;

		void Load()
		{
			if (!TransitionState(State::Unloaded, State::Loading))
				return;

			Thread::Scheduler::Get().SpawnJob<JobLoadResource>(Thread::Job::Priority::Low, this);
		};

		void Unload()
		{
			if (!TransitionState(State::Loaded, State::Unloading))
				return;

			reinterpret_cast<T*>(resource_memory)->~T();

			Allocator allocator;
			allocator.deallocate(reinterpret_cast<T*>(resource_memory), 1);
			resource_memory = nullptr;

			state = State::Unloaded;
		};

		void Wait(State target_state)
		{
			while ((int)state.load() < (int)target_state)
				std::this_thread::yield();
		}

		void WaitUnload()
		{
			if (state == State::Loading)
				Wait(State::Loaded);

			if (state == State::Loaded)
				Unload();

			if (state == State::Unloading)
				Wait(State::Unloaded);
		}

	public:
		Resource(const std::wstring& path) : ResourceBase(path) 
		{
		}

		~Resource()
		{
			WaitUnload();
		}

		void Resolve()
		{
			if (IsResolved()) return;

			OPTICK_EVENT();

			if (state == State::Unloading)
				Wait(State::Unloaded);

			if (state == State::Unloaded)
				Load();

			if (state == State::Loading)
				Wait(State::Loaded);
		}

		T& operator*()
		{
			Resolve();
			return *reinterpret_cast<T*>(resource_memory);
		}

		T* operator->()
		{
			Resolve();
			return reinterpret_cast<T*>(resource_memory);
		}
	};

	template <typename T>
	class Handle
	{
	public:
		Handle() : resource(nullptr) {}

		explicit Handle(const std::wstring& filename)
		{
			resource = static_cast<Resource<T>*>(Cache::Get().GetResource(filename));
			if (!resource)
			{
				auto unique_resource = std::make_unique<Resource<T>>(filename);
				resource = unique_resource.get();
				Cache::Get().SetResource(filename, std::move(unique_resource));
			}

			resource->AddRef();
			if (!resource->IsResolved())
				resource->Load();
		}

		explicit Handle(const Handle& other)
		{
			resource = other.resource;
			if (resource)
				resource->AddRef();
		}

		explicit Handle(Handle&& other)
		{
			resource = other.resource;
			other.resource = nullptr;
		}

		Handle& operator=(const Handle& other)
		{
			if (resource)
				resource->RemoveRef();

			resource = other.resource;
			if (resource)
				resource->AddRef();

			return *this;
		}

		Handle& operator=(Handle&& other)
		{
			if (resource)
				resource->RemoveRef();

			resource = other.resource;
			other.resource = nullptr;
			return *this;
		}

		~Handle()
		{
			if (resource)
				resource->RemoveRef();
		}

		const T& operator*()
		{
			return resource->operator*();
		}

		const T* operator->()
		{
			return resource->operator->();
		}

	private:
		Resource<T>* resource;
	};

	class Cache
	{
	public:
		template<typename> friend class Handle;
		static Cache& Get();

		void GCCollect();
		void Destroy();

	private:
		ResourceBase* GetResource(const std::wstring& filename);
		void SetResource(const std::wstring& filename, std::unique_ptr<ResourceBase> resource);
		std::unordered_map<std::wstring, std::unique_ptr<ResourceBase>> resources;
	};
}