#pragma once

#include <mh/coroutine/task.hpp>
#include <coroutine>
#include <unordered_map>
#include <mutex>

#ifndef MH_STUFF_API
#define MH_STUFF_API
#endif

namespace mh
{
    // Internal process manager - singleton that handles SIGCHLD globally
    class process_manager
    {
    public:
        MH_STUFF_API static process_manager& instance();
        MH_STUFF_API bool register_process(int pid, std::coroutine_handle<> handle);
        MH_STUFF_API void unregister_process(int pid);
        MH_STUFF_API int get_exit_status(int pid);

    private:
        struct process_info
        {
            std::coroutine_handle<> handle;
            int exit_status = -1;
        };

        std::mutex mutex_;
        std::unordered_map<int, process_info> waiting_processes_;
        std::unordered_map<int, int> exit_statuses_;
        bool monitoring_started_ = false;
        static bool signal_handler_installed_;
        static int signal_pipe_[2];

        process_manager();
        void install_signal_handler();
        static void signal_handler(int);
        void start_monitoring_task();
        void check_processes();
    };
}

#ifndef MH_COMPILE_LIBRARY
#include "process_manager.inl"
#endif