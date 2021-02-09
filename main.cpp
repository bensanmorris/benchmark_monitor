#include <benchmark/benchmark.h>
#include <time.h>

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <thread>
#include <vector>

#if defined( __WIN32__ ) || defined( _WIN32 ) || defined( WIN32 ) || defined( _WINDOWS )
#define NOMINMAX
#include "windows.h"
#include "psapi.h"
#define MEMORY_MONITOR_BEGIN \
    bool memory_monitor_stop = false; \
    std::vector<double> memory_samples; \
    std::thread t([&] () { \
        while (!memory_monitor_stop) { \
            PROCESS_MEMORY_COUNTERS_EX pmc; \
            GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)); \
            double physicalMem = (float)pmc.WorkingSetSize / 1024.f / 1024.f; \
            memory_samples.push_back(physicalMem); \
        } \
    });

#define MEMORY_MONITOR_END \
    memory_monitor_stop = true; \
    t.join(); \
    s.counters["MaxPhysicalMem"] = *std::max_element(memory_samples.begin(), memory_samples.end()); \
    s.counters["MinPhysicalMem"] = *std::min_element(memory_samples.begin(), memory_samples.end()); \
    s.counters["AvgPhysicalMem"] = std::accumulate(memory_samples.begin(), memory_samples.end(), 0.f) / memory_samples.size();

#else
// TODO
#define MEMORY_MONITOR_BEGIN
#define MEMORY_MONITOR_END
#endif

typedef std::vector<int> VALS;

int max_sub_array_greedy(const VALS& vals, int start_at, int end_at, int& start_index, int& end_index)
{
    int max = vals[0];
    start_index = end_index = start_at;
    for(int i = start_at; i < end_at; ++i)
    {
        int s = vals[i];
        for(int j = i; j < end_at; ++j)
        {
            if(j != i)
                s+=vals[j];
            if(s > max)
            {
                max         = s;
                start_index = i;
                end_index   = j;
            }
        }
    }
    return max;
}

static const int DATA_LEN = 100;

static void BM_greedy(benchmark::State& s)
{
    VALS data;
    for (int i = 0; i < DATA_LEN; i++) {
        data.push_back(rand());
    }
    
    MEMORY_MONITOR_BEGIN

    int start_index = 0, end_index = 0, max = std::numeric_limits<int>::min();
    for(auto _ : s)
    {
        // UNCOMMENT THIS (introduces slow down by performing same code twice)
        //max = max_sub_array_greedy(data, 0, data.size(), start_index, end_index);
        max = max_sub_array_greedy(data, 0, data.size(), start_index, end_index);
    }

    MEMORY_MONITOR_END
}
BENCHMARK(BM_greedy);

inline int max_crossing_sub_array(const VALS& vals, int start_at, int mid, int end_at, int& start_index, int& end_index)
{
    int left_max         = vals[mid];
    int left_start_index = mid;
    int s                = 0;
    for(int l = mid; l >= start_at; --l)
    {
        s+=vals[l];
        if(s > left_max)
        {
            left_max         = s;
            left_start_index = l;
        }
    }

    int right_end_index = -1;
    int right_max       = vals[mid+1];
    s                   = 0;
    for(int r = mid+1; r <= end_at; ++r)
    {
        s+=vals[r];
        if(s > right_max)
        {
            right_max       = s;
            right_end_index = r;
        }
    }

    start_index = left_start_index;
    end_index   = right_end_index;

    return left_max + right_max;
}

int max_sub_array_logarithmic(const VALS& vals, int start_at, int end_at, int& start_index, int& end_index)
{
    if(start_at == end_at)
    {
        start_index = end_index = start_at;
        return vals[start_at];
    }
    int sze         = end_at - start_at;
    int mid         = start_at + (sze >> 1);

    int left_start  = 0, left_end = 0;
    int left_max    = max_sub_array_logarithmic(vals, start_at, mid, left_start, left_end);

    int right_start = 0, right_end = 0;
    int right_max   = max_sub_array_logarithmic(vals, mid+1, end_at, right_start, right_end);

    int cross_start = 0, cross_end = 0;
    int cross_max   = max_crossing_sub_array(vals, start_at, mid, end_at, cross_start, cross_end);

    int max = 0;
    if(left_max > right_max && left_max > cross_max)
    {
        start_index = left_start;
        end_index   = left_end;
        max         = left_max;
    }
    else if(right_max > left_max && right_max > cross_max)
    {
        start_index = right_start;
        end_index   = right_end;
        max         = right_max;
    }
    else
    {
        start_index = cross_start;
        end_index   = cross_end;
        max         = cross_max;
    }
    return max;
}

static void BM_logarithmic(benchmark::State& s)
{
    VALS data;
    for (int i = 0; i < DATA_LEN; i++) {
        data.push_back(rand());
    }

    MEMORY_MONITOR_BEGIN

    int start_index = 0, end_index = 0, max = std::numeric_limits<int>::min();
    for (auto _ : s) {
        max = max_sub_array_logarithmic(data, 0, data.size() - 1, start_index, end_index);
    }

    MEMORY_MONITOR_END
}
BENCHMARK(BM_logarithmic);

BENCHMARK_MAIN();
