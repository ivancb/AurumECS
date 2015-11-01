#pragma once

#include "iprocess.h"

namespace au {
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