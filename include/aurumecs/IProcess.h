#pragma once

namespace au {
	class IProcess {
	public:
		IProcess() = default;

		// Processes shouldn't really be copied or moved around
		IProcess(const IProcess&) = delete;
		IProcess(IProcess&&) = delete;
		IProcess& operator = (const IProcess&) = delete;

		virtual ~IProcess()
		{
		}

		virtual void Execute(double timeSec) = 0;

		virtual inline double TimeTaken() const = 0;
		virtual inline size_t GetProcessTypeId() const = 0;
		virtual inline size_t GetProcessGroupId() const = 0;
	};
}