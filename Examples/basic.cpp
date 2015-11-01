#include <cstdio>
#include <aurumecs/World.h>
#include <aurumecs/ComponentId.h>
#include <aurumecs/IProcess.h>
#include <aurumecs/SinglethreadedDispatcher.h>
#include "examples.h"
#include "components.h"

using namespace au;

// Game world type alias, useful for processes
using GameWorld = World<SingleThreadedDispatcher, TransformComponent>;

// We're defining two processes which will be capable of running at the same time
// one will simply print the object positions while the other will update them

// NOTE: This macro's only meant for examples
#define PROCESS_BOILERPLATE(proc_type_id) inline double TimeTaken() const override { return 0.0; } \
	inline size_t GetProcessTypeId() const override { return proc_type_id; } \
	inline size_t GetProcessGroupId() const override { return 0; } \
	static const size_t ProcessTypeId = proc_type_id; \
	static const size_t ProcessGroupId = 0

class TransformUpdateProcess : public IProcess {
private:
	GameWorld* mOwner;
public:
	PROCESS_BOILERPLATE(0);

	TransformUpdateProcess(GameWorld* owner) : mOwner(owner)
	{
	}

	void Execute(double timeSec) override
	{
		auto it = mOwner->GetComponentIterator<AuthoritySet<TransformComponent>, TransformComponent>();

		while (it.Advance())
		{
			auto* transform = it.Edit<TransformComponent>();

			for (int n = 0; n < 3; n++)
				transform->Position[n] += (float) (transform->Velocity[n] * timeSec);
		}
	}
};

class TransformPrintProcess : public IProcess {
private:
	GameWorld* mOwner;
public:
	PROCESS_BOILERPLATE(1);

	TransformPrintProcess(GameWorld* owner) : mOwner(owner)
	{
	}

	void Execute(double timeSec) override
	{
		auto it = mOwner->GetReadComponentIterator<TransformComponent>();

		while (it.Advance())
		{
			const auto& transform = it.Get<TransformComponent>();
			auto entity = it.GetEntityRef();

			printf("Entity %u is at %.2f %.2f %.2f\n", entity.Guid, transform.Position[0], transform.Position[1], transform.Position[2]);
		}
	}
};

void BasicUsageExample()
{
	printf("Basic example ---------------\n");
	GameWorld world;

	// Create entities
	std::vector<EntityRef> entities;
	entities.reserve(10);

	for (int n = 0; n < 10; n++)
		entities.push_back(world.AddEntity());

	// Create and link transform components to each entity
	auto transform = TransformComponent::Create();

	for (auto& entity : entities)
	{
		transform.Velocity[0] = entity.Guid / 10.f;
		transform.Velocity[1] = entity.Guid * 10.f;
		transform.Velocity[2] = (float) entity.Guid;

		world.AddComponent(entity, transform);
	}

	// Create the processes
	world.AddProcess(new TransformPrintProcess(&world), 0);
	world.AddProcess(new TransformUpdateProcess(&world), 0);

	// Actually update the world state
	for (int n = 0; n < 10; n++)
	{
		printf("------ World Tick %d\n", n);
		world.Process(0.016);
	}

	printf("Basic example end ---------------\n");
}