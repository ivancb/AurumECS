// This example is very similar to the basic example, however it shows how multiple processes
// can modify the same component at the same time. This operation is inherently unsafe and it's
// up to the process author's to ensure that each process is writing to different fields.
//
// To do this, when requesting a component iterator from a World, the process also provides the
// address to one or multiple keys, which allows processes with the same keys to operate over
// the requested components simultaneously, without triggering the "borrow checker" in World.

#include <cstdio>
#include <aurumecs/world.h>
#include <aurumecs/component.h>
#include <aurumecs/iprocess.h>
#include <aurumecs/st_dispatcher.h>
#include "examples.h"
#include "components.h"

using namespace au;

// Game world type alias, useful for processes
using GameWorld = World<SingleThreadedDispatcher, TransformComponent>;

// We're defining three processes which will be capable of running at the same time.
// One will print the transform data while the other two will operate on the same transform
// without requiring any sort of synchronization primitives. One updates the rotation
// while the other updates the position.
// The downside to this system is that the user must ensure that he knows what he's doing.

// Note: This macro's only meant for examples
#define PROCESS_BOILERPLATE(proc_type_id) inline double TimeTaken() const override { return 0.0; } \
	inline size_t GetProcessTypeId() const override { return proc_type_id; } \
	inline size_t GetProcessGroupId() const override { return 0; } \
	static const size_t ProcessTypeId = proc_type_id; \
	static const size_t ProcessGroupId = 0

// This key is shared between both of the update processes, indicating to the ECS world that it's safe
// to allow processes using this key to run in the same process set concurrently.
static int Key = 15123931;

class TransformUpdateProcess1 : public IProcess {
private:
	GameWorld* mOwner;
public:
	PROCESS_BOILERPLATE(0);

	TransformUpdateProcess1(GameWorld* owner) : mOwner(owner)
	{
	}

	void Execute(double timeSec) override
	{
		auto it = mOwner->GetComponentIterator<AuthoritySet<TransformComponent>, TransformComponent>(&Key);

		while (it.Advance())
		{
			auto* transform = it.Edit<TransformComponent>();

			for (int n = 0; n < 3; n++)
				transform->Position[n] += (float) (transform->Velocity[n] * timeSec);
		}
	}
};

class TransformRotationUpdateProcess : public IProcess {
private:
	GameWorld* mOwner;
public:
	PROCESS_BOILERPLATE(1);

	TransformRotationUpdateProcess(GameWorld* owner) : mOwner(owner)
	{
	}

	void Execute(double timeSec) override
	{
		auto it = mOwner->GetComponentIterator<AuthoritySet<TransformComponent>, TransformComponent>(&Key);

		while (it.Advance())
		{
			auto* transform = it.Edit<TransformComponent>();

			for (int n = 0; n < 3; n++)
				transform->Rotation[n] += (float) (transform->AngularVelocity[n] * timeSec);
		}
	}
};

class TransformPrintProcess : public IProcess {
private:
	GameWorld* mOwner;
public:
	PROCESS_BOILERPLATE(2);

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

			printf("Entity %u is at %.2f %.2f %.2f with rotation %.2f %.2f %.2f\n", entity.Guid, 
				transform.Position[0], transform.Position[1], transform.Position[2],
				transform.Rotation[0], transform.Rotation[1], transform.Rotation[2]);
		}
	}
};

void BasicSharedAuthorityExample()
{
	printf("Basic shared authority example ---------------\n");

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

		transform.AngularVelocity[0] = -(entity.Guid / 3.f);
		transform.AngularVelocity[1] = entity.Guid / 18.f;
		transform.AngularVelocity[2] = entity.Guid / 3.f;

		world.AddComponent(entity, transform);
	}

	// Create the processes
	world.AddProcess(new TransformPrintProcess(&world), 0);
	world.AddProcess(new TransformUpdateProcess1(&world), 0);
	world.AddProcess(new TransformRotationUpdateProcess(&world), 0);

	// Actually update the world state
	for (int n = 0; n < 10; n++)
	{
		printf("------ World Tick %d\n", n);
		world.Process(0.016);
	}

	printf("Basic shared authority example end ---------------\n");
}