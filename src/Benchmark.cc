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

#include "Benchmark.h"

#include <signal.h>

#include <fstream>
#include <iostream>

namespace RooBench {

/**
 * Benchmark constructor
 *
 * @param bench_config
 *      Structure containing the benchmark configuration.
 * @param server_name
 *      Name of the server node running this benchmark instance.
 * @param output_dir
 *      Directory for log and stats output.
 * @param num_threads
 *      The number of threads that should be running run_benchmark().
 */
Benchmark::Benchmark(nlohmann::json bench_config, std::string server_name,
                     std::string output_dir, size_t num_threads)
    : server_name(server_name)
    , output_dir(output_dir)
    , config(bench_config)
    , num_threads(num_threads)
    , benchmark_threads()
{}

/**
 * Default Benchmark destructor.
 */
Benchmark::~Benchmark() = default;

/**
 *
 */
void
Benchmark::run()
{
    // Start all benchmark threads
    for (size_t i = 0; i < num_threads; ++i) {
        benchmark_threads.emplace_back(&Benchmark::run_benchmark, this);
    }
    handleSignals();
    // Wait for benchmark threads to complete.
    for (auto thread = benchmark_threads.begin();
         thread != benchmark_threads.end(); ++thread) {
        thread->join();
    }
}

void
Benchmark::handleSignals()
{
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGUSR1);
    sigaddset(&sigset, SIGUSR2);
    sigprocmask(SIG_BLOCK, &sigset, NULL);

    while (true) {
        int sig, error;
        error = sigwait(&sigset, &sig);
        if (sig == SIGINT) {
            stop();
            break;
        } else if (sig == SIGUSR1) {
            start_client();
        } else if (sig == SIGUSR2) {
            dump_stats();
        } else {
            exit(1);
        }
    }
}

}  // namespace RooBench
