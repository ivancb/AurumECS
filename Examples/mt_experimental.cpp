// Shows an example of how to use experimental multithreading support for World.
// Note that at the moment, only process execution is parallelized and the processes
// included in this example aren't exactly demanding in terms of processing time.

#include <cstdio>
#include <random>
#include <aurumecs/World.h>
#include <aurumecs/ComponentId.h>
#include <aurumecs/IProcess.h>
#include <aurumecs/SinglethreadedDispatcher.h>
#include <aurumecs/MultithreadedDispatcher.h>
#include "examples.h"
#include "components.h"

using namespace au;

// IMPORTANT: The number after MultiThreadedDispatcher is the number of threads to create.
// Note however that the number of effective processing threads is NumThreads + 1, since it
// includes the spawning thread.
using STGameWorld = World<SingleThreadedDispatcher, TransformComponent, RandomThingComponent>;
using MTGameWorld = World<MultiThreadedDispatcher<1>, TransformComponent, RandomThingComponent>;

// NOTE: This macro's only meant for examples
#define PROCESS_BOILERPLATE(proc_type_id) inline double TimeTaken() const override { return 0.0; } \
	inline size_t GetProcessTypeId() const override { return proc_type_id; } \
	inline size_t GetProcessGroupId() const override { return 0; } \
	static const size_t ProcessTypeId = proc_type_id; \
	static const size_t ProcessGroupId = 0

template<typename WorldType>
class TransformUpdateProcess : public IProcess {
private:
	WorldType* mOwner;
public:
	PROCESS_BOILERPLATE(0);

	TransformUpdateProcess(WorldType* owner) : mOwner(owner)
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

template<typename WorldType>
class RandomThingProcess : public IProcess {
private:
	WorldType* mOwner;
	std::default_random_engine mRng;
	std::uniform_int_distribution<int> mDistribution{ 0, 100 };
public:
	PROCESS_BOILERPLATE(1);

	RandomThingProcess(WorldType* owner) : mOwner(owner)
	{
	}

	void Execute(double timeSec) override
	{
		auto it = mOwner->GetComponentIterator<AuthoritySet<RandomThingComponent>, RandomThingComponent>();

		while (it.Advance())
		{
			auto* comp = it.Edit<RandomThingComponent>();
			comp->RandomThing = mDistribution(mRng);
		}
	}
};

template<typename WorldType>
double RunExample(int num_entities, int iterations)
{
	WorldType world;

	// Create entities
	std::vector<EntityRef> entities;
	entities.reserve(num_entities);

	for (int n = 0; n < num_entities; n++)
		entities.push_back(world.AddEntity());

	// Create and link transform components to each entity
	auto transform = TransformComponent::Create();

	for (auto& entity : entities)
	{
		transform.Velocity[0] = -0.2f;
		transform.Velocity[1] = 0.5f;
		transform.Velocity[2] = 1.f;

		world.AddComponent(entity, transform);
	}

	// Create the processes
	world.AddProcess(new RandomThingProcess<WorldType>(&world), 0);
	world.AddProcess(new TransformUpdateProcess<WorldType>(&world), 0);

	// Actually update the world state
	double acc = 0.0;

	for (int n = 0; n < iterations; n++)
	{
		world.Process(0.016);
		acc += world.GetMetrics().ProcessExecutionTime;
	}

	return acc;
}

void MultithreadedWorldProcessingExample()
{
	printf("Multithreaded world example ---------------\n");

	const int k_entity_count = 1000000;
	const int k_iteration_count = 100;

	for (int n = 0; n < 3; n++)
	{
		printf("----- Loop %d\n", n);
		auto st_time = RunExample<STGameWorld>(k_entity_count, k_iteration_count);
		auto mt_time = RunExample<MTGameWorld>(k_entity_count, k_iteration_count);

		printf("Singlethreaded took %.2f ms\nMultithreaded took %.2f ms\nMT = %.2f %% of ST\n", st_time, mt_time, 100.f * ((float) mt_time) / st_time);
		printf("-------------\n\n");
	}

	printf("Multithreaded world example end ---------------\n");
}