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

#include "DpcBenchmark.h"

#include <Homa/Debug.h>
#include <PerfUtils/Cycles.h>
#include <PerfUtils/TimeTrace.h>
#include <Roo/Debug.h>
#include <Roo/Perf.h>

#include <deque>
#include <fstream>
#include <functional>
#include <nlohmann/json.hpp>
#include <random>

#include "WireFormat.h"

namespace RooBench {

namespace {

std::vector<Homa::Driver::Address>
create_peer_list(const BenchConfig::ServerList& server_list,
                 Homa::Driver* driver)
{
    Homa::Driver::Address localAddress = driver->getLocalAddress();
    std::vector<Homa::Driver::Address> peer_list;
    for (auto& elem : server_list) {
        Homa::Driver::Address serverAddress =
            driver->getAddress(&elem.second.address);
        if (serverAddress != localAddress) {
            peer_list.push_back(serverAddress);
        }
    }
    return peer_list;
}

Homa::Driver*
startDriver()
{
    Homa::Drivers::DPDK::DpdkDriver::Config driverConfig;
    driverConfig.HIGHEST_PACKET_PRIORITY_OVERRIDE = 0;
    int port = 1;
    return new Homa::Drivers::DPDK::DpdkDriver(port, &driverConfig);
}

}  // namespace

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
DpcBenchmark::DpcBenchmark(nlohmann::json bench_config, std::string server_name,
                           std::string output_dir, size_t num_threads)
    : Benchmark(bench_config, server_name, output_dir, num_threads)
    , driver(startDriver())
    , transport(Homa::Transport::create(
          driver.get(), std::hash<std::string>{}(driver->addressToString(
                            driver->getLocalAddress()))))
    , socket(Roo::Socket::create(transport.get()))
    , peer_list(create_peer_list(config.serverList, driver.get()))
    , unified(config.unified)
    , queueDepth(std::lround((config.load * 0.1) / config.client_count) + 1)
    , cyclesPerOp(PerfUtils::Cycles::fromSeconds(
          static_cast<double>(config.client_count) / config.load))
    , nextOpTimeout(0)
    , run(true)
    , run_client(false)
    , client_running()
    , stats_mutex()
    , client_stats()
    , task_stats(create_task_stats_map(config.tasks))
    , active_cycles(0)
{
    Homa::Debug::setLogPolicy(Homa::Debug::logPolicyFromString("ERROR"));
    Roo::Debug::setLogPolicy(Roo::Debug::logPolicyFromString("ERROR"));
    client_running.clear();
}

/**
 * Default Benchmark destructor.
 */
DpcBenchmark::~DpcBenchmark() = default;

/**
 * @copydoc Benchmark::run_benchmark()
 */
void
DpcBenchmark::run_benchmark()
{
    while (run) {
        if (run_client) {
            socket->poll();
            client_poll();
        }
        if (!run_client || unified) {
            socket->poll();
            server_poll();
        }
    }
}

/**
 * @copydoc Benchmark::dump_stats()
 */
void
DpcBenchmark::dump_stats()
{
    static int dump_count = 0;

    // Dump Roo Stats
    {
        Roo::Perf::Stats stats;
        Roo::Perf::getStats(&stats);
        nlohmann::json roo_stats;
        roo_stats["timestamp"] = stats.timestamp;
        roo_stats["cycles_per_second"] = stats.cycles_per_second;
        roo_stats["api_cycles"] = stats.api_cycles;
        roo_stats["active_cycles"] = stats.active_cycles;
        roo_stats["idle_cycles"] = stats.idle_cycles;
        roo_stats["tx_message_bytes"] = stats.tx_message_bytes;
        roo_stats["rx_message_bytes"] = stats.rx_message_bytes;
        roo_stats["transport_tx_bytes"] = stats.transport_tx_bytes;
        roo_stats["transport_rx_bytes"] = stats.transport_rx_bytes;
        roo_stats["tx_data_pkts"] = stats.tx_data_pkts;
        roo_stats["rx_data_pkts"] = stats.rx_data_pkts;
        roo_stats["tx_grant_pkts"] = stats.tx_grant_pkts;
        roo_stats["rx_grant_pkts"] = stats.rx_grant_pkts;
        roo_stats["tx_done_pkts"] = stats.tx_done_pkts;
        roo_stats["rx_done_pkts"] = stats.rx_done_pkts;
        roo_stats["tx_resend_pkts"] = stats.tx_resend_pkts;
        roo_stats["rx_resend_pkts"] = stats.rx_resend_pkts;
        roo_stats["tx_busy_pkts"] = stats.tx_busy_pkts;
        roo_stats["rx_busy_pkts"] = stats.rx_busy_pkts;
        roo_stats["tx_ping_pkts"] = stats.tx_ping_pkts;
        roo_stats["rx_ping_pkts"] = stats.rx_ping_pkts;
        roo_stats["tx_unknown_pkts"] = stats.tx_unknown_pkts;
        roo_stats["rx_unknown_pkts"] = stats.rx_unknown_pkts;
        roo_stats["tx_error_pkts"] = stats.tx_error_pkts;
        roo_stats["rx_error_pkts"] = stats.rx_error_pkts;

        std::string roo_stats_outfile_name =
            output_dir + "/" + server_name + "_transport_stats_" +
            std::to_string(dump_count) + ".json";
        std::ofstream outfile(roo_stats_outfile_name);
        outfile << roo_stats.dump();
    }

    // Dump Bench Stats
    {
        uint64_t timestamp = PerfUtils::Cycles::rdtsc();
        nlohmann::json bench_stats_json;
        bench_stats_json["timestamp"] = timestamp;
        bench_stats_json["cycles_per_second"] = PerfUtils::Cycles::perSecond();

        bench_stats_json["active_cycles"] = active_cycles.load();

        // Task stats
        std::vector<nlohmann::json> task_stats_json_list;
        for (auto& elem : task_stats) {
            nlohmann::json task_stats_json;
            task_stats_json["id"] = elem.first;
            task_stats_json["count"] = elem.second->count.load();
            task_stats_json_list.push_back(task_stats_json);
        }

        // Client stats
        nlohmann::json client_stats_json;
        client_stats_json["count"] = client_stats.count.load();
        client_stats_json["failures"] = client_stats.failures.load();
        client_stats_json["drops"] = client_stats.drops.load();
        uint64_t const sample_count =
            std::min(static_cast<uint64_t>(client_stats_json["count"]),
                     client_stats.samples.max_size());
        std::vector<uint> latencies;
        for (uint64_t i = 0; i < sample_count; ++i) {
            latencies.push_back(
                PerfUtils::Cycles::toNanoseconds(client_stats.samples.at(i)));
        }
        client_stats_json["unit"] = "ns";
        client_stats_json["latencies"] = nlohmann::json(latencies);

        bench_stats_json["task_stats"] = nlohmann::json(task_stats_json_list);
        bench_stats_json["client_stats"] = client_stats_json;

        // Dump stats
        std::string bench_stats_outfile_name =
            output_dir + "/" + server_name + "_bench_stats_" +
            std::to_string(dump_count) + ".json";
        std::ofstream outfile(bench_stats_outfile_name);
        outfile << bench_stats_json.dump();
    }

    // Dump time trace
    std::string ttlogname = output_dir + "/" + server_name + "_tt_" +
                            std::to_string(dump_count) + ".log";
    PerfUtils::TimeTrace::setOutputFileName(ttlogname.c_str());
    PerfUtils::TimeTrace::print();

    dump_count++;
}

/**
 * @copydoc Benchmark::start_client()
 */
void
DpcBenchmark::start_client()
{
    nextOpTimeout = PerfUtils::Cycles::rdtsc();
    run_client = true;
}

/**
 * @copydoc Benchmark::stop()
 */
void
DpcBenchmark::stop()
{
    run = false;
}

/**
 * Helper static method to initialize the task_stats map.
 */
std::unordered_map<int, const std::unique_ptr<DpcBenchmark::TaskStats>>
DpcBenchmark::create_task_stats_map(const BenchConfig::TaskMap& task_map)
{
    std::unordered_map<int, const std::unique_ptr<TaskStats>> task_stats;
    for (auto& elem : task_map) {
        task_stats.emplace(elem.first, new TaskStats());
        task_stats.at(elem.first)->count.store(0);
    }
    return task_stats;
}

/**
 * Perform increment work to process incoming ServerTasks
 */
void
DpcBenchmark::server_poll()
{
    uint64_t const start_tsc = PerfUtils::Cycles::rdtsc();
    Roo::unique_ptr<Roo::ServerTask> task = socket->receive();
    if (task) {
        dispatch(std::move(task));
        uint64_t const stop_tsc = PerfUtils::Cycles::rdtsc();
        active_cycles.fetch_add(stop_tsc - start_tsc,
                                std::memory_order_relaxed);
    }
}

/**
 * Perform incremental work to process outgoing client RooPCs
 */
void
DpcBenchmark::client_poll()
{
    bool idle = true;
    uint64_t const start_tsc = PerfUtils::Cycles::rdtsc();
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    static thread_local std::poisson_distribution<uint64_t> dis(cyclesPerOp);
    static thread_local std::deque<Op*> ops;

    if (client_running.test_and_set()) {
        return;
    }

    // Check if it is time for another execution
    uint64_t timeout = nextOpTimeout.load();
    if (ops.size() < 16 && timeout <= PerfUtils::Cycles::rdtsc() &&
        nextOpTimeout.compare_exchange_strong(timeout, timeout + dis(gen))) {
        Op* op = new Op;
        op->start_cycles = timeout;
        ops.push_back(op);
    }

    const int buf_size = 1000000;
    char buf[buf_size];

    if (!ops.empty()) {
        Op* op = ops.front();
        ops.pop_front();

        if (!op->rpc) {
            idle = false;
            op->rpc = socket->allocRooPC();
            op->nextPhase = config.client.phases.cbegin();
        }
        if (op->rpc->checkStatus() == Roo::RooPC::Status::IN_PROGRESS) {
            // nothing to do
        } else if (op->nextPhase != config.client.phases.cend()) {
            idle = false;
            const BenchConfig::Client::Phase& phase = *op->nextPhase;
            for (const BenchConfig::Request& request_config : phase.requests) {
                for (int i = 0; i < request_config.count; ++i) {
                    WireFormat::Benchmark::Request* request =
                        reinterpret_cast<WireFormat::Benchmark::Request*>(buf);
                    request->common.opcode = WireFormat::Benchmark::opcode;
                    request->taskType = request_config.taskId;
                    Homa::Driver::Address dest = selectServer();
                    assert(request_config.size >=
                           sizeof(WireFormat::Benchmark::Request));
                    assert(request_config.size <= sizeof(buf));
                    op->rpc->send(dest, buf, request_config.size);
                }
            }
            ++op->nextPhase;
        }
        if (op->nextPhase == config.client.phases.cend() &&
            op->rpc->checkStatus() != Roo::RooPC::Status::IN_PROGRESS) {
            op->stop_cycles = PerfUtils::Cycles::rdtsc();
            idle = false;
            Roo::RooPC::Status status = op->rpc->checkStatus();
            op->rpc.reset();
            if (status == Roo::RooPC::Status::COMPLETED) {
                // Update stats
                std::lock_guard<std::mutex> lock(stats_mutex);
                uint64_t sample = op->stop_cycles - op->start_cycles;
                client_stats.samples.at(client_stats.sample_count &
                                        SAMPLE_INDEX_MASK) = sample;
                client_stats.sample_count++;
                client_stats.count++;
            } else {
                client_stats.failures++;
            }
            delete op;
        } else {
            ops.push_back(op);
        }
    }
    client_running.clear();
    uint64_t const stop_tsc = PerfUtils::Cycles::rdtsc();
    if (!idle) {
        active_cycles.fetch_add(stop_tsc - start_tsc,
                                std::memory_order_relaxed);
    }
}

Homa::Driver::Address
DpcBenchmark::selectServer()
{
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    auto start = peer_list.begin();
    auto end = peer_list.end();
    std::uniform_int_distribution<> dis(0, std::distance(start, end) - 1);
    std::advance(start, dis(gen));
    return *start;
}

void
DpcBenchmark::dispatch(Roo::unique_ptr<Roo::ServerTask> task)
{
    WireFormat::Common common;
    task->getRequest()->get(0, &common, sizeof(common));

    switch (common.opcode) {
        case WireFormat::Benchmark::opcode:
            handleBenchmarkTask(std::move(task));
            break;
        default:
            std::cerr << "Unknown opcode" << std::endl;
    }
}

void
DpcBenchmark::handleBenchmarkTask(Roo::unique_ptr<Roo::ServerTask> task)
{
    WireFormat::Benchmark::Request request;
    task->getRequest()->get(0, &request, sizeof(request));
    const int taskId = request.taskType;
    const BenchConfig::Task& task_config = config.tasks.at(taskId);

    const int buf_size = 1000000;
    char buf[buf_size];

    for (const BenchConfig::Request& request_config : task_config.requests) {
        for (int i = 0; i < request_config.count; ++i) {
            WireFormat::Benchmark::Request* request =
                reinterpret_cast<WireFormat::Benchmark::Request*>(buf);
            request->common.opcode = WireFormat::Benchmark::opcode;
            request->taskType = request_config.taskId;
            Homa::Driver::Address dest = selectServer();
            assert(request_config.size >=
                   sizeof(WireFormat::Benchmark::Request));
            assert(request_config.size <= sizeof(buf));
            task->delegate(dest, buf, request_config.size);
        }
    }

    for (const BenchConfig::Response& response_config : task_config.responses) {
        for (int i = 0; i < response_config.count; ++i) {
            assert(response_config.size <= sizeof(buf));
            task->reply(buf, response_config.size);
        }
    }

    // Done with the task;
    task.reset();

    // Update stats
    task_stats.at(taskId)->count.fetch_add(1, std::memory_order_relaxed);
}

}  // namespace RooBench
