#pragma once

#include <type_traits>

namespace au {
	class IWorld;

	static const size_t kInvalidEntityGuid = 0;
	static const size_t kInvalidEntityIndex = 0xFFFFFFFF;

	template<size_t componentCount>
	struct Entity {
		size_t Guid;
		size_t Index;
		int UserValue;
		unsigned char ComponentCount[componentCount];
		unsigned char InternalComponentCount[componentCount];

		static_assert(sizeof(decltype(InternalComponentCount)) == sizeof(decltype(ComponentCount)), "Component count sizes must match");
	};

	struct EntityRef {
		size_t Guid;
		size_t Index;
		IWorld* Owner;
		int UserValue;

		bool Destroy();

		inline bool IsAcquired() const
		{
			return (Index != kInvalidEntityIndex);
		}

		inline bool IsValid() const
		{
			return (Guid != kInvalidEntityGuid) && (Owner != nullptr);
		}

		template<typename T>
		T* GetComponent()
		{
			return (T*) GetRawComponent(T::Id(), 0);
		}

		template<typename T>
		T* GetComponentByIndex(unsigned char idx)
		{
			return (T*) GetRawComponent(T::Id(), idx);
		}

		template<typename T>
		unsigned char GetComponentCount()
		{
			return CountRawComponent(T::Id());
		}

		template<typename T>
		T* GetEditComponent()
		{
			return (T*) GetRawEditComponent(T::Id(), 0);
		}

		template<typename T>
		T* GetEditComponentByIndex(unsigned char idx)
		{
			return (T*) GetRawEditComponent(T::Id(), idx);
		}

		template<typename T>
		unsigned char GetEditComponentCount()
		{
			return CountRawEditComponent(T::Id());
		}

		bool Acquire();

		static const EntityRef Invalid;

		// Unsafe stuff
		unsigned char CountRawComponent(size_t componentId);
		unsigned char CountRawEditComponent(size_t componentId);
		void* GetRawComponent(size_t componentId, size_t index);
		void* GetRawEditComponent(size_t componentId, size_t index);
	};

#define AUENT_IS_SAME_TYPE(x, y) std::is_same<decltype(x), decltype(y)>::value
	static_assert(AUENT_IS_SAME_TYPE(Entity<1>::Guid, EntityRef::Guid), "Entity and EntityRef Guid must use the same GUID type");
	static_assert(AUENT_IS_SAME_TYPE(Entity<1>::Index, EntityRef::Index), "Entity and EntityRef Guid must use the same Index type");
#undef AUENT_IS_SAME_TYPE

	static_assert(std::is_pod<Entity<1>>::value, "Entity must be POD");
	static_assert(std::is_pod<EntityRef>::value, "EntityRef must be POD");
}