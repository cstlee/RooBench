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

#ifndef ROOBENCH_DPCBENCHMARK_H
#define ROOBENCH_DPCBENCHMARK_H

#include <Homa/Drivers/DPDK/DpdkDriver.h>
#include <Homa/Homa.h>
#include <Roo/Roo.h>

#include <array>
#include <atomic>
#include <mutex>
#include <unordered_map>

#include "Benchmark.h"

// Forward Declarations
namespace Homa {
class Driver;
class Transport;
}  // namespace Homa
namespace Roo {
class Socket;
}  // namespace Roo

namespace RooBench {

/**
 * Benchmark for DPC style workloads.
 *
 * Implementations should be thread-safe.
 */
class DpcBenchmark : public Benchmark {
  public:
    DpcBenchmark(nlohmann::json bench_config, std::string server_name,
                 std::string output_dir, size_t num_threads);
    virtual ~DpcBenchmark();

  protected:
    /**
     * Runs the actual benchmark logic.  Multiple instances may exist.
     */
    virtual void run_benchmark();

    /**
     * Called when the benchmark should dump the current statistics.
     *
     * Implemented by Benchmark subclasses.  Must be thread-safe.
     */
    virtual void dump_stats();

    /**
     * Signal that the benchmark client should start running.
     */
    virtual void start_client();

    /**
     * Signal to the subclass that all instacnes of run_benchmark() should
     * return asap.
     */
    virtual void stop();

  private:
    static const uint64_t SAMPLE_INDEX_MASK = 0x0FFFFF;
    static const uint64_t MAX_SAMPLES = SAMPLE_INDEX_MASK + 1;

    struct ClientStats {
        std::atomic<int> count;
        std::atomic<int> failures;
        std::atomic<uint64_t> sample_count;
        std::array<std::atomic<uint64_t>, MAX_SAMPLES> samples;
    };
    struct TaskStats {
        std::atomic<int> count;
    };

    static std::unordered_map<int, const std::unique_ptr<TaskStats>>
    create_task_stats_map(const BenchConfig::TaskMap& task_map);

    void server_poll();
    void client_poll();
    Homa::Driver::Address selectServer(int taskType, int index);
    void dispatch(Roo::unique_ptr<Roo::ServerTask> task);
    void handleBenchmarkTask(Roo::unique_ptr<Roo::ServerTask> task);

    const std::unique_ptr<Homa::Driver> driver;
    const std::unique_ptr<Homa::Transport> transport;
    const std::unique_ptr<Roo::Socket> socket;
    const std::unordered_map<int, Homa::Driver::Address> server_address_book;
    const int server_id;
    std::atomic<bool> run;
    std::atomic<bool> run_client;
    std::atomic_flag client_running;

    std::mutex stats_mutex;
    ClientStats client_stats;
    const std::unordered_map<int, const std::unique_ptr<TaskStats>> task_stats;

    /// Cycles spent in the benchmark application logic; excludes time spent
    /// polling.
    std::atomic<uint64_t> active_cycles;
};

}  // namespace RooBench

#endif  // ROOBENCH_DPCBENCHMARK_H
