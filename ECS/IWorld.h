#pragma once
#include "Entity.h"

namespace au {
	class IProcess;

	class IWorld {
	public:
		virtual ~IWorld()
		{
		}

		// Entities
		virtual EntityRef AddEntity() = 0;
		virtual EntityRef AddEntity(int userValue) = 0;
		virtual bool RemoveEntity(EntityRef eref) = 0;
		virtual void ReserveEntities(size_t count) = 0;

		virtual size_t CountEntities() const = 0;
		virtual EntityRef GetEntity(size_t idx) const = 0;
		virtual EntityRef FindEntity(size_t guid) const = 0;
		virtual EntityRef FindEntityExt(size_t guid) const = 0;
		virtual bool IsValid(EntityRef entity) const = 0;

		// Components
		virtual void* GetRawComponent(EntityRef ent, size_t componentId, unsigned char idx) = 0;
		virtual unsigned char CountRawComponents(EntityRef ent, size_t componentId) const = 0;
		virtual void* GetRawFutureComponent(EntityRef ent, size_t componentId, unsigned char idx) = 0;
		virtual unsigned char CountRawFutureComponents(EntityRef ent, size_t componentId) const = 0;

		// Processes
		virtual void AddProcess(IProcess* proc, size_t procGroup) = 0;
		virtual void RemoveProcess(IProcess* proc) = 0;
		virtual IProcess* GetProcessById(size_t id) const = 0;
		virtual void SetProcessEnabled(size_t processTypeId, bool enabled) = 0;
		virtual void SetProcessGroupEnabled(size_t group_id, bool enabled) = 0;
		virtual bool GetProcessEnabled(size_t processTypeId) const = 0;
		virtual bool GetProcessGroupEnabled(size_t group_id) const = 0;

		virtual void Process(double timeSec) = 0;

		virtual void* GetUserPointer() const = 0;
		virtual void SetUserPointer(void*) = 0;
	};
}