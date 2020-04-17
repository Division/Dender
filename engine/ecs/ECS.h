#pragma once

#include <set>
#include <unordered_map>
#include <functional>
#include "stdint.h"
#include "utils/Math.h"
#include "EntityChunks.h"
#include "components/Entity.h"

namespace core { namespace ECS {

	class EntityManager
	{
	public:
		typedef std::function<void(EntityID)> EntityCallback;
		
		struct CallbackHandle
		{
			CallbackHandle() : manager(nullptr), id(0) {}
			CallbackHandle(EntityManager* manager, uint64_t id)
				: manager(manager), id(id) {}
			CallbackHandle(CallbackHandle&&) = default;
			CallbackHandle& operator=(CallbackHandle&& other)
			{
				if (id)
					manager->RemoveEntityDestroyCallback(id);
				id = std::move(other.id);
				manager = other.manager;
				other.id = 0;
				other.manager = nullptr;
				return *this;
			}
			CallbackHandle& operator=(const CallbackHandle&) = delete;

			~CallbackHandle()
			{
				if (id)
					manager->RemoveEntityDestroyCallback(id);
			}

			EntityManager* manager;
			uint64_t id;
		};

		struct EntityCallbackData
		{
			EntityCallback callback;
			uint64_t id;
			bool operator==(uint64_t compare_id) { return id == compare_id; }
		};

		~EntityManager()
		{
			while (!entity_address.empty())
				DestroyEntity((*entity_address.begin()).first);
		}

		CallbackHandle AddEntityDestroyCallback(EntityCallback callback)
		{
			entity_destroy_callbacks.push_back({ callback, ++callback_id });

			return CallbackHandle(this, callback_id);
		}

		void RemoveEntityDestroyCallback(uint64_t id)
		{
			auto found = std::find(entity_destroy_callbacks.begin(), entity_destroy_callbacks.end(), id);
			if (found != entity_destroy_callbacks.end())
			{
				entity_destroy_callbacks.erase(found);
			}
		}

		EntityID CreateEntity(ComponentSetHash set_hash = 0)
		{
			auto entity_id = ++id_counter;
			auto address = EntityAddress{ nullptr, (uint32_t)-1 };

			if (set_hash)
			{
				assert(false); // not ready yet
			}

			entity_address[entity_id] = address;

			auto* entity = AddComponent<EntityData>(entity_id); // EntityData component exists for all entities
			entity->id = entity_id;
			return entity_id;
		}

		void DestroyEntity(EntityID entity)
		{
			auto address_it = entity_address.find(entity);
			assert(address_it != entity_address.end());
			TriggerDestroyCallbacks(entity);

			auto& address = address_it->second;
			address.chunk->RemoveEntity(address.index);
			entity_address.erase(address_it);
		}

		template<typename T, typename ...Args>
		T* AddComponent(EntityID entity, Args&& ...args)
		{
			auto entity_address_it = entity_address.find(entity);
			assert(entity_address_it != entity_address.end());

			auto old_address = entity_address_it->second;
			auto layout = old_address.chunk ? old_address.chunk->GetComponentLayout() : ComponentLayout(CHUNK_SIZE);

			if (old_address.chunk)
			{
				if (!layout.AddComponent<T>())
					return nullptr;
			}
			else
				layout.AddComponent<T>();

			auto* chunk_list = GetOrCreateChunkList(layout);
			auto* chunk = chunk_list->GetFirstChunk();
			auto new_address = chunk->AddEntity(entity, old_address.chunk ? &old_address : nullptr);

			if (old_address.chunk)
				old_address.chunk->RemoveEntity(old_address.index);

			auto* pointer = (T*)new_address.chunk->GetComponentPointer(new_address.index, GetComponentHash<T>());
			new (pointer) T(std::forward<Args>(args)...); // Apply constructor

			return pointer;
		}

		template<typename T>
		void RemoveComponent(EntityID entity)
		{
			auto entity_address_it = entity_address.find(entity);
			assert(entity_address_it != entity_address.end());

			auto old_address = entity_address_it->second;
			assert(old_address.chunk);
			auto layout = old_address.chunk->GetComponentLayout();
			auto* component_data = layout.GetComponentData(GetComponentHash<T>());

			if (!component_data)
				throw std::runtime_error("Component doesn't exist");
			
			auto* component_pointer = old_address.chunk->GetComponentPointer(old_address.index, component_data);
			component_data->destructor(component_pointer);

			layout.RemoveComponent<T>();

			auto* chunk_list = GetOrCreateChunkList(layout);
			auto* chunk = chunk_list->GetFirstChunk();
			auto new_address = chunk->AddEntity(entity, &old_address);
			old_address.chunk->RemoveEntity(old_address.index);
		}

		ChunkList* GetOrCreateChunkList(const ComponentLayout& layout)
		{
			auto chunk_list_it = chunks.find(layout.GetHash());
			if (chunk_list_it == chunks.end())
			{
				auto it = chunks.insert(std::make_pair(layout.GetHash(), std::make_unique<ChunkList>(layout, std::bind(&EntityManager::OnEntityAddressChanged, this, std::placeholders::_1, std::placeholders::_2))));
				chunk_list_it = it.first;
			}

			return chunk_list_it->second.get();
		}

		template<typename T>
		T* GetComponent(EntityID entity)
		{
			auto address = entity_address.at(entity);
			return (T*)address.chunk->GetComponentPointer(address.index, GetComponentHash<T>());
		}

		void OnEntityAddressChanged(uint64_t entity_id, EntityAddress new_address)
		{
			auto& address = entity_address.at(entity_id);
			address = new_address;
		}

		const auto& GetChunkMap() const { return chunks; }
		const auto& GetEntityAddressMap() const { return entity_address; }

		void ForEachChunkList(std::function<void(ChunkList*)> callback, std::function<bool(ChunkList*)> predicate)
		{
			for (auto& chunk : chunks)
				if (predicate(chunk.second.get()))
					callback(chunk.second.get());
		}

		ChunkList::List GetChunkLists(std::function<bool(ChunkList*)> predicate)
		{
			ChunkList::List list;

			ForEachChunkList([&list](ChunkList* chunk_list) {
				list.push_back(chunk_list);
			}, predicate);

			return list;
		}

		template <typename T>
		ChunkList::List GetChunkListsWithComponent()
		{
			auto hash = GetComponentHash<T>();
			return GetChunkLists([=](ChunkList* chunk_list) {
				return chunk_list->HasComponent(hash);
			});
		}

		template <typename T1, typename T2>
		ChunkList::List GetChunkListsWithComponents()
		{
			auto hash1 = GetComponentHash<T1>();
			auto hash2 = GetComponentHash<T2>();
			return GetChunkLists([=](ChunkList* chunk_list) {
				return chunk_list->HasComponent(hash1) && chunk_list->HasComponent(hash2);
			});
		}

		template <typename T>
		ChunkList::List GetChunkListsWithoutComponent()
		{
			auto hash = GetComponentHash<T>();
			return GetChunkLists([=](ChunkList* chunk_list) {
				return !chunk_list->HasComponent(hash);
			});
		}

		template <typename T1, typename T2>
		ChunkList::List GetChunkListsWithoutComponents()
		{
			auto hash1 = GetComponentHash<T1>();
			auto hash2 = GetComponentHash<T2>();
			return GetChunkLists([=](ChunkList* chunk_list) {
				return !(chunk_list->HasComponent(hash1) && chunk_list->HasComponent(hash2));
			});
		}


	private:
		void TriggerDestroyCallbacks(EntityID id)
		{
			for (auto& callback : entity_destroy_callbacks)
				callback.callback(id);
		}

	private:
		uint64_t callback_id = 0;
		EntityID id_counter = 0;
		std::unordered_map<ComponentSetHash, std::unique_ptr<ChunkList>> chunks;
		std::unordered_map<EntityID, EntityAddress> entity_address;
		std::vector<EntityCallbackData> entity_destroy_callbacks;
	};

} }