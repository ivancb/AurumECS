#pragma once

#include <thread>
#include <atomic>
#include <vector>
#include "iprocess.h"

namespace au {
	// A dispatcher that executes processes over (NumThreads + 1) threads.
	// Note that using this with very small worlds and short processes may
	// lead to slowdowns since there's a slight overhead.
	template<int NumThreads>
	class MultiThreadedDispatcher {
		static_assert(NumThreads > 0, "Must use more at least two threads (including the spawning thread) for a MultiThreadedDispatcher");
		static_assert(NumThreads < 32, "Probably a bad idea to use more than 32 threads in a MultiThreadedDispatcher");
	private:
		// Note: This is a slight hack since atomics are not copy-constructible/copy-assignable (and for good reason).
		// The wrapper just lets us contain atomics in an std::vector. This usage is safe since in this situation
		// the copy constructor is only being called when the secondary threads are yielding.
		template <typename T>
		struct AtomicCopyWrapper {
			T a;

			AtomicCopyWrapper() : a() { }
			AtomicCopyWrapper(const T &a) : a(a.load()) { }
			AtomicCopyWrapper(const AtomicCopyWrapper &rhs) : a(rhs.a.load()) { }
			AtomicCopyWrapper &operator=(const AtomicCopyWrapper &rhs) { a.store(rhs.a.load()); }
		};

		struct ScheduledProcess {
			IProcess* process;
			AtomicCopyWrapper<std::atomic_bool> taken;
			AtomicCopyWrapper<std::atomic_bool> done;
		};

		double mTimeSec = 0.0;
		std::thread mThreads[NumThreads];
		std::atomic_bool mThreadActive[NumThreads];
		std::vector<ScheduledProcess> mScheduledProcesses;
		std::atomic_bool mExecuting = false;
		std::atomic_bool mStopRequested = false;
	public:
		MultiThreadedDispatcher()
		{
			mScheduledProcesses.reserve(10);

			for (int n = 0; n < NumThreads; n++)
				mThreads[n] = std::thread(ThreadExecutionCallback, this, n);
		}

		~MultiThreadedDispatcher()
		{
			mStopRequested = true;

			for (int n = 0; n < NumThreads; n++)
				mThreads[n].join();
		}

		MultiThreadedDispatcher(const MultiThreadedDispatcher&) = delete;
		MultiThreadedDispatcher(MultiThreadedDispatcher&&) = delete;
		MultiThreadedDispatcher& operator=(const MultiThreadedDispatcher&) = delete;

		inline void Schedule(IProcess* process)
		{
			mScheduledProcesses.push_back(ScheduledProcess{
				process,
				false,
				false
			});
		}

		void Execute()
		{
			auto size = mScheduledProcesses.size();
			mExecuting = true;

			bool any_not_done = true;
			while (any_not_done)
			{
				any_not_done = false;

				for (ScheduledProcess& schedule : mScheduledProcesses)
				{
					bool expected = false;
					if (schedule.taken.a.compare_exchange_strong(expected, true))
					{
						schedule.process->Execute(mTimeSec);
						schedule.done.a = true;
					}
					else if (!schedule.done.a)
					{
						any_not_done = true;
					}
				}
			}

			mExecuting = false;

			while (true)
			{
				bool any_executing = false;

				for (int n = 0; n < NumThreads; n++)
				{
					if (mThreadActive[n])
					{
						any_executing = true;
						break;
					}
				}

				if (!any_executing)
					break;
			}

			mScheduledProcesses.clear();
		}

		inline void SetTime(double timeSec)
		{
			mTimeSec = timeSec;
		}
	private:
		static void ThreadExecutionCallback(MultiThreadedDispatcher<NumThreads>* owner, int tindex)
		{
			while (!owner->mStopRequested)
			{
				if (owner->mExecuting)
				{
					owner->mThreadActive[tindex] = true;

					for (ScheduledProcess& schedule : owner->mScheduledProcesses)
					{
						bool expected = false;
						if (schedule.taken.a.compare_exchange_strong(expected, true))
						{
							schedule.process->Execute(owner->mTimeSec);
							schedule.done.a = true;
						}
					}
				}
				else
				{
					owner->mThreadActive[tindex] = false;
					std::this_thread::yield();
				}
			}
		}
	};
}