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

#ifndef ROOBENCH_BENCHMARKFACTORY_H
#define ROOBENCH_BENCHMARKFACTORY_H

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#include "Benchmark.h"
#include "DpcBenchmark.h"
#include "FakeBenchmark.h"
#include "RpcBenchmark.h"

namespace RooBench {
namespace BenchmarkFactory {

/**
 * Create and return a new benchmark instances based on the given configuration.
 *
 * @param bench_config
 *      Path to benchmark configuration file.
 */
Benchmark*
createBenchmark(std::string bench_config, std::string server_name,
                std::string output_dir_path, size_t num_threads)
{
    std::ifstream i(bench_config);
    nlohmann::json config;
    i >> config;
    std::string bench_type = config.at("workload").at("bench_type");

    if (bench_type == "Fake") {
        return new RooBench::FakeBenchmark(config, server_name, output_dir_path,
                                           num_threads);
    } else if (bench_type == "DPC") {
        return new RooBench::DpcBenchmark(config, server_name, output_dir_path,
                                          num_threads);
    } else if (bench_type == "RPC") {
        return new RooBench::RpcBenchmark(config, server_name, output_dir_path,
                                          num_threads);
    } else {
        std::cerr << "Unknown Benchmark type '" << bench_type << "'"
                  << std::endl;
        return nullptr;
    }
}

}  // namespace BenchmarkFactory
}  // namespace RooBench

#endif  // ROOBENCH_BENCHMARKFACTORY_H
