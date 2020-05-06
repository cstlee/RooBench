/* Copyright (c) 2020, Stanford University
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef ROOBENCH_FAKEBENCHMARK_H
#define ROOBENCH_FAKEBENCHMARK_H

#include <iostream>
#include <mutex>

#include "Benchmark.h"

namespace RooBench {

using namespace std::chrono_literals;

namespace Test {

struct Temp {
    Temp()
        : x(0)
    {
        std::cout << "New Thread!" << std::endl;
    }
    ~Temp()
    {
        std::cout << "Thread dying..." << std::endl;
    }

    int x;
};

thread_local Temp temp;

}  // namespace Test

/**
 * Base class for all Roobench benchmarks
 *
 * Implementations should be thread-safe.
 */
class FakeBenchmark : public Benchmark {
  public:
    FakeBenchmark(std::string bench_config, std::string server_name,
                  std::string output_dir, size_t num_threads)
        : Benchmark(bench_config, server_name, output_dir, num_threads)
        , mutex()
        , run(true)
    {}

    virtual ~FakeBenchmark() = default;

  protected:
    /**
     * Runs the actual benchmark logic.  Multiple instances may exist.
     */
    virtual void run_benchmark()
    {
        Test::temp.x = 1;
        for (int i = 1; i < 30; ++i) {
            std::this_thread::sleep_for(2s);
            std::lock_guard<std::mutex> lock(mutex);
            if (!run) {
                break;
            }
            std::cout << i * 2 << std::endl;
        }

        std::lock_guard<std::mutex> lock(mutex);
        std::cout << "DONE" << std::endl;
    };

    /**
     * Called when the benchmark should dump the current statistics.
     *
     * Implemented by Benchmark subclasses.  Must be thread-safe.
     */
    virtual void dump_stats()
    {
        std::lock_guard<std::mutex> lock(mutex);
        std::cout << "stats" << std::endl;
    };

    /**
     * Signal that the benchmark client should start running.
     */
    virtual void start_client()
    {
        std::lock_guard<std::mutex> lock(mutex);
        std::cout << "start" << std::endl;
    }

    /**
     * Signal to the subclass that all instacnes of run_benchmark() should
     * return asap.
     */
    virtual void stop()
    {
        std::lock_guard<std::mutex> lock(mutex);
        run = false;
    }

  private:
    std::mutex mutex;
    bool run;
};

}  // namespace RooBench

#endif  // ROOBENCH_FAKEBENCHMARK_H
