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

static const char USAGE[] = R"(RooBench server

Usage:
    server <server_name> <num_threads> <bench_config> <output_dir>

Options:
    -h --help           Show this screen.
    --version           Show version.
)";

#include <docopt.h>

#include "Benchmark.h"
#include "BenchmarkFactory.h"

int
main(int argc, char* argv[])
{
    std::map<std::string, docopt::value> args =
        docopt::docopt(USAGE, {argv + 1, argv + argc},
                       true,                    // show help if requested
                       "RooBench server 0.1");  // version string
    std::string server_name = args["<server_name>"].asString();
    int num_threads = args["<num_threads>"].asLong();
    std::string bench_config = args["<bench_config>"].asString();
    std::string output_dir_path = args["<output_dir>"].asString();

    RooBench::Benchmark* benchmark =
        RooBench::BenchmarkFactory::createBenchmark(
            bench_config, server_name, output_dir_path, num_threads);

    if (benchmark != nullptr) {
        benchmark->run();
        delete benchmark;
    }

    return 0;
}
