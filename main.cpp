#include <benchmark/benchmark.h>

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

using namespace std;
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
    
    int start_index = 0, end_index = 0, max = std::numeric_limits<int>::min();
    for(auto _ : s)
    {
        // UNCOMMENT THIS
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        max = max_sub_array_greedy(data, 0, data.size(), start_index, end_index);
    }
}
BENCHMARK(BM_greedy)->Unit(benchmark::TimeUnit::kMillisecond);

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
    int start_index = 0, end_index = 0, max = std::numeric_limits<int>::min();
    for (auto _ : s) {
        // UNCOMMENT THIS
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        max = max_sub_array_logarithmic(data, 0, data.size() - 1, start_index, end_index);
    }
}
BENCHMARK(BM_logarithmic)->Unit(benchmark::TimeUnit::kMillisecond);

BENCHMARK_MAIN();
