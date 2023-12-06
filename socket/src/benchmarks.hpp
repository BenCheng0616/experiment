#ifndef BENCHMARKS_H
#define BENCHMARKS_H

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <list>

#include "parseargs.hpp"
typedef unsigned long long bench_t;

typedef struct Benchmarks
{
    bench_t total_start;
    bench_t single_start;
    bench_t minimum;
    bench_t maximum;
    bench_t sum;
    bench_t squared_sum;

} Benchmarks;

class Benchmark
{
public:
    Benchmark(Arguments *args)
    {
        _bench.maximum = 0;
        _bench.minimum = INT32_MAX;
        _bench.sum = 0;
        _bench.squared_sum = 0;
        _bench.total_start = this->now();
    }

    ~Benchmark()
    {
    }

    void singleStart()
    {
        _bench.single_start = now();
    }

    void benchmark()
    {
        _timeList.push_back(this->now() - _bench.single_start);
    }

    void evaluate(Arguments *args)
    {
        for (auto t = _timeList.begin(); t != _timeList.end(); t++)
        {

            bench_t time = *t;
            if (time < _bench.minimum)
                _bench.minimum = time;
            if (time > _bench.maximum)
                _bench.maximum = time;
            _bench.sum += time;
            _bench.squared_sum += (time * time);
        }

        const bench_t total_time = now() - _bench.total_start;
        const double average = ((double)_bench.sum) / args->count;
        double sigma = _bench.squared_sum / args->count;
        sigma = sqrt(sigma - (average * average));

        int packetRate = (int)(args->count / (total_time / 1e9));
        //======================================================
        printf("\n===============RESULTS===============\n");
        printf("Message Size:       %d\n", args->size);
        printf("Message count:      %d\n", args->count);
        printf("Total duration:     %.3f\tms\n", total_time / 1e6);
        printf("Average duration:   %.3f\tus\n", average / 1000.0);
        printf("Minimum duration:   %.3f\tns\n", _bench.minimum / 1000.0);
        printf("Maximum duration:   %.3f\tus\n", _bench.maximum / 1000.0);
        printf("Standard deviation: %.3f\tus\n", sigma / 1000.0);
        printf("Packet Rate:        %d\tpkt/s\n", packetRate);
        printf("\n=====================================\n");
    }

    bench_t now()
    {
        struct timespec ts;
        timespec_get(&ts, TIME_UTC);

        return ts.tv_sec * 1e9 + ts.tv_nsec;
    }

    int getsize()
    {
        return _timeList.size();
    }

private:
    Benchmarks _bench;
    std::list<bench_t> _timeList;
};

#endif
