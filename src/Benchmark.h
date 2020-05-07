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

#ifndef ROOBENCH_BENCHMARK_H
#define ROOBENCH_BENCHMARK_H

#include <atomic>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <vector>

#include "BenchConfig.h"

namespace RooBench {

/**
 * Base class for all Roobench benchmarks
 *
 * Implementations should be thread-safe.
 */
class Benchmark {
  public:
    Benchmark(nlohmann::json bench_config, std::string server_name,
              std::string output_dir, size_t num_threads);
    virtual ~Benchmark();
    void run();

  protected:
    /**
     * Runs the actual benchmark logic.  Multiple instances may exist.
     */
    virtual void run_benchmark() = 0;

    /**
     * Called when the benchmark should dump the current statistics.
     *
     * Implemented by Benchmark subclasses.  Must be thread-safe.
     */
    virtual void dump_stats() = 0;

    /**
     * Signal that the benchmark client should start running.
     */
    virtual void start_client() = 0;

    /**
     * Signal to the subclass that all instacnes of run_benchmark() should
     * return asap.
     */
    virtual void stop() = 0;

    /// The name assigned to the server running this benchmark instance.  All
    /// output files should be prefixed with this name.
    const std::string server_name;

    /// The directory path under which all output files should be written.
    const std::string output_dir;

    /// Benchmark configuration parameters
    const BenchConfig config;

  private:
    /// Runs the logic of handling async signals
    void handleSignals();

    /// The number of instances of run_benchmark() that should be running.
    const size_t num_threads;

    /// Set of all threads running run_benchmark()
    std::vector<std::thread> benchmark_threads;
};

}  // namespace RooBench

#endif  // ROOBENCH_BENCHMARK_H
