#pragma once

#ifdef __unix__

#include <mh/coroutine/task.hpp>
#include <string>
#include <vector>
#include <memory>

#ifndef MH_STUFF_API
#define MH_STUFF_API
#endif

namespace mh
{
	class process
	{
	public:
		// Type aliases for I/O sources and sinks (to be defined later with async I/O)
		using Source = std::shared_ptr<void>; // Placeholder
		using Sink = std::shared_ptr<void>;	  // Placeholder

		/**
		 * Constructor
		 * @param command The command to execute
		 * @param args Command arguments
		 * @param inputSource Input source for the process (stdin)
		 * @param outputSink Output sink for the process (stdout)
		 * @param errorSink Error sink for the process (stderr)
		 */
		MH_STUFF_API process(
			const std::string &command,
			const std::vector<std::string> &args,
			Source inputSource = nullptr,
			Sink outputSink = nullptr,
			Sink errorSink = nullptr);

		/**
		 * Destructor
		 */
		MH_STUFF_API ~process();

		/**
		 * Start the process
		 * @return True if process started successfully
		 */
		MH_STUFF_API bool start();

		/**
		 * Wait for the process to complete asynchronously
		 * @return Exit code of the process
		 */

		MH_STUFF_API task<int> wait_async();

		/**
		 * Check if the process is running
		 * @return True if process is still running
		 */
		MH_STUFF_API bool is_running() const;

		/**
		 * Get the process ID
		 * @return Process ID (PID)
		 */
		MH_STUFF_API int get_pid() const;

		/**
		 * Terminate the process
		 * @param force If true, use SIGKILL instead of SIGTERM
		 * @return True if termination signal was sent successfully
		 */
		MH_STUFF_API bool terminate(bool force = false);

	private:
		struct impl;
		std::unique_ptr<impl> m_impl;
	};
}

#ifndef MH_COMPILE_LIBRARY
#include "process.inl"
#endif

#endif // __unix__
