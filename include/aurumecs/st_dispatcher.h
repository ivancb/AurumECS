#pragma once
#include "iprocess.h"

namespace au {
	// A dispatcher that just executes processes in the calling thread
	class SingleThreadedDispatcher {
	private:
		double mTimeSec = 0.0;
	public:
		inline void Schedule(IProcess* process)
		{
			process->Execute(mTimeSec);
		}

		inline void Execute()
		{

		}

		inline void SetTime(double timeSec)
		{
			mTimeSec = timeSec;
		}
	};
}