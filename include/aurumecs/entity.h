#pragma once
#include <type_traits>

namespace au {
	class IWorld;

	static const size_t kInvalidEntityGuid = 0;
	static const size_t kInvalidEntityIndex = 0xFFFFFFFF;

	template<size_t componentCount>
	struct EntityBase {
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

		bool Acquire()
		{
			if ((Guid == kInvalidEntityGuid) || !Owner)
				return false;
			else if (!Owner->IsValid(*this))
			{
				EntityRef foundRef = Owner->FindEntity(this->Guid);

				if (!foundRef.IsValid())
					return false;
				else
				{
					*this = foundRef;
					return true;
				}
			}

			return true;
		}

		bool Destroy()
		{
			if (IsValid())
			{
				if (Owner->RemoveEntity(*this))
				{
					*this = InvalidRef();
					return true;
				}
			}

			return false;
		}

		inline unsigned char CountRawComponent(size_t componentId) const { return Owner->CountRawComponents(*this, componentId); }
		inline unsigned char CountRawEditComponent(size_t componentId) const { return Owner->CountRawFutureComponents(*this, componentId); }
		inline void* GetRawComponent(size_t componentId, unsigned char index) { return Owner->GetRawComponent(*this, componentId, index); }
		inline void* GetRawEditComponent(size_t componentId, unsigned char index) { return Owner->GetRawFutureComponent(*this, componentId, index); }

		inline bool IsAcquired() const { return (Index != kInvalidEntityIndex); }
		inline bool IsValid() const { return (Guid != kInvalidEntityGuid) && Owner; }

		template<typename T> inline T* GetComponent() { return (T*) GetRawComponent(T::Id(), 0); }
		template<typename T> inline T* GetEditComponent() { return (T*) GetRawEditComponent(T::Id(), 0); }
		template<typename T> inline T* GetComponentByIndex(unsigned char idx) { return (T*) GetRawComponent(T::Id(), idx); }
		template<typename T> inline T* GetEditComponentByIndex(unsigned char idx) { return (T*) GetRawEditComponent(T::Id(), idx); }

		template<typename T> inline unsigned char GetComponentCount() const { return CountRawComponent(T::Id()); }
		template<typename T> inline unsigned char GetEditComponentCount() const { return CountRawEditComponent(T::Id()); }

		static inline EntityRef InvalidRef() { return{ kInvalidEntityGuid, kInvalidEntityIndex, nullptr, 0 }; }
	};

#define AUENT_IS_SAME_TYPE(x, y) std::is_same<decltype(x), decltype(y)>::value
	static_assert(AUENT_IS_SAME_TYPE(EntityBase<1>::Guid, EntityRef::Guid), "EntityBase and EntityRef Guid must use the same GUID type");
	static_assert(AUENT_IS_SAME_TYPE(EntityBase<1>::Index, EntityRef::Index), "EntityBase and EntityRef Guid must use the same Index type");
#undef AUENT_IS_SAME_TYPE

	static_assert(std::is_pod<EntityBase<1>>::value, "EntityBase must be POD");
	static_assert(std::is_pod<EntityRef>::value, "EntityRef must be POD");
}