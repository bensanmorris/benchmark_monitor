#pragma once

#if defined( _WIN32 )
    #define NOMINMAX
    #include "windows.h"
    #include "psapi.h"
#elif defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
    #include <unistd.h>
    #include <sys/resource.h>
#endif

namespace benchmark_memory {

struct MemInfo {
    uint64_t process_pmem;
    uint64_t process_vmem;
    MemInfo() :
        process_pmem(0), process_vmem(0) {}
};

#if defined( _WIN32 )
void getMemoryInfo(MemInfo& meminfo) {
    PROCESS_MEMORY_COUNTERS_EX pmc;
    GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
    meminfo.process_pmem = pmc.WorkingSetSize;
    meminfo.process_vmem = pmc.PrivateUsage;
}
#elif defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
void getMemoryInfo(MemInfo& meminfo) {
    struct rusage rusage;
    getrusage(RUSAGE_SELF, &rusage);
    meminfo.process_pmem = rusage.ru_maxrss * 1024L;
    meminfo.process_vmem = 0; // TODO
}
#endif

#define MEMORY_MONITOR_BEGIN \
    std::atomic_bool memory_monitor_stop = false; \
    benchmark_memory::MemInfo meminfo; \
    std::thread t([&] () { \
        while (!memory_monitor_stop) { \
            benchmark_memory::MemInfo tmp; \
            getMemoryInfo(tmp); \
            if(tmp.process_pmem > meminfo.process_pmem) \
                meminfo.process_pmem = tmp.process_pmem; \
            if(tmp.process_vmem > meminfo.process_vmem) \
                meminfo.process_vmem = tmp.process_vmem; \
        } \
    });

#define MEMORY_MONITOR_END \
    memory_monitor_stop = true; \
    t.join(); \
    s.counters["MaxProcPhysMem"] = meminfo.process_pmem; \
    s.counters["MaxProcVirtMem"] = meminfo.process_vmem;
}