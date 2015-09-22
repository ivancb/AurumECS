#include "Entity.h"
#include "IWorld.h"

namespace au {
	const EntityRef EntityRef::Invalid{ kInvalidEntityGuid, kInvalidEntityIndex, nullptr, 0 };

	bool EntityRef::Destroy()
	{
		if (IsValid())
		{
			if (Owner->RemoveEntity(*this))
			{
				*this = Invalid;
				return true;
			}
		}

		return false;
	}

	bool EntityRef::Acquire()
	{
		if (Guid == kInvalidEntityGuid)
			return false;
		else if(Owner == nullptr)
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

	void* EntityRef::GetRawComponent(size_t componentId, size_t index)
	{
		return Owner->GetRawComponent(*this, componentId, index);
	}

	void* EntityRef::GetRawEditComponent(size_t componentId, size_t index)
	{
		return Owner->GetRawFutureComponent(*this, componentId, index);
	}

	unsigned char EntityRef::CountRawComponent(size_t componentId)
	{
		return Owner->CountRawComponents(*this, componentId);
	}

	unsigned char EntityRef::CountRawEditComponent(size_t componentId)
	{
		return Owner->CountRawFutureComponents(*this, componentId);
	}
}