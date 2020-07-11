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

#include "RpcBenchmark.h"

#include <Homa/Debug.h>
#include <PerfUtils/Cycles.h>
#include <PerfUtils/TimeTrace.h>
#include <SimpleRpc/Debug.h>
#include <SimpleRpc/Perf.h>

#include <fstream>
#include <functional>
#include <nlohmann/json.hpp>

#include "WireFormat.h"

namespace RooBench {

namespace {
std::unordered_map<int, Homa::Driver::Address>
create_address_book(const BenchConfig::ServerList& server_list,
                    Homa::Driver* driver)
{
    std::unordered_map<int, Homa::Driver::Address> address_book;
    for (auto& elem : server_list) {
        address_book.insert(
            {elem.first, driver->getAddress(&elem.second.address)});
    }
    return address_book;
}

int
get_server_id(
    const std::unordered_map<int, Homa::Driver::Address>& address_book,
    Homa::Driver* driver)
{
    Homa::Driver::Address localAddress = driver->getLocalAddress();
    for (auto& elem : address_book) {
        if (elem.second == localAddress) {
            return elem.first;
        }
    }
    throw;
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
RpcBenchmark::RpcBenchmark(nlohmann::json bench_config, std::string server_name,
                           std::string output_dir, size_t num_threads)
    : Benchmark(bench_config, server_name, output_dir, num_threads)
    , driver(startDriver())
    , transport(Homa::Transport::create(
          driver.get(), std::hash<std::string>{}(driver->addressToString(
                            driver->getLocalAddress()))))
    , socket(SimpleRpc::Socket::create(transport.get()))
    , server_address_book(create_address_book(config.serverList, driver.get()))
    , server_id(get_server_id(server_address_book, driver.get()))
    , run(true)
    , run_client(false)
    , client_running()
    , stats_mutex()
    , client_stats()
    , task_stats(create_task_stats_map(config.tasks))
    , active_cycles(0)
{
    Homa::Debug::setLogPolicy(Homa::Debug::logPolicyFromString("ERROR"));
    SimpleRpc::Debug::setLogPolicy(
        SimpleRpc::Debug::logPolicyFromString("ERROR"));
    client_running.clear();
}

/**
 * Default Benchmark destructor.
 */
RpcBenchmark::~RpcBenchmark() = default;

/**
 * @copydoc Benchmark::run_benchmark()
 */
void
RpcBenchmark::run_benchmark()
{
    while (run) {
        server_poll();
        client_poll();
    }
}

/**
 * @copydoc Benchmark::dump_stats()
 */
void
RpcBenchmark::dump_stats()
{
    static int dump_count = 0;

    // Dump SimpleRpc Stats
    {
        SimpleRpc::Perf::Stats stats;
        SimpleRpc::Perf::getStats(&stats);
        nlohmann::json rpc_stats;
        rpc_stats["timestamp"] = stats.timestamp;
        rpc_stats["cycles_per_second"] = stats.cycles_per_second;
        rpc_stats["active_cycles"] = stats.active_cycles;
        rpc_stats["idle_cycles"] = stats.idle_cycles;
        rpc_stats["tx_message_bytes"] = stats.tx_message_bytes;
        rpc_stats["rx_message_bytes"] = stats.rx_message_bytes;
        rpc_stats["transport_tx_bytes"] = stats.transport_tx_bytes;
        rpc_stats["transport_rx_bytes"] = stats.transport_rx_bytes;
        rpc_stats["tx_data_pkts"] = stats.tx_data_pkts;
        rpc_stats["rx_data_pkts"] = stats.rx_data_pkts;
        rpc_stats["tx_grant_pkts"] = stats.tx_grant_pkts;
        rpc_stats["rx_grant_pkts"] = stats.rx_grant_pkts;
        rpc_stats["tx_done_pkts"] = stats.tx_done_pkts;
        rpc_stats["rx_done_pkts"] = stats.rx_done_pkts;
        rpc_stats["tx_resend_pkts"] = stats.tx_resend_pkts;
        rpc_stats["rx_resend_pkts"] = stats.rx_resend_pkts;
        rpc_stats["tx_busy_pkts"] = stats.tx_busy_pkts;
        rpc_stats["rx_busy_pkts"] = stats.rx_busy_pkts;
        rpc_stats["tx_ping_pkts"] = stats.tx_ping_pkts;
        rpc_stats["rx_ping_pkts"] = stats.rx_ping_pkts;
        rpc_stats["tx_unknown_pkts"] = stats.tx_unknown_pkts;
        rpc_stats["rx_unknown_pkts"] = stats.rx_unknown_pkts;
        rpc_stats["tx_error_pkts"] = stats.tx_error_pkts;
        rpc_stats["rx_error_pkts"] = stats.rx_error_pkts;

        std::string rpc_stats_outfile_name =
            output_dir + "/" + server_name + "_transport_stats_" +
            std::to_string(dump_count) + ".json";
        std::ofstream outfile(rpc_stats_outfile_name);
        outfile << rpc_stats.dump();
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
        uint64_t max_sample_index =
            client_stats.sample_count.load() & SAMPLE_INDEX_MASK;
        std::vector<uint> latencies;
        for (uint64_t i = 0; i <= max_sample_index; ++i) {
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
RpcBenchmark::start_client()
{
    run_client = true;
}

/**
 * @copydoc Benchmark::stop()
 */
void
RpcBenchmark::stop()
{
    run = false;
}

/**
 * Helper static method to initialize the task_stats map.
 */
std::unordered_map<int, const std::unique_ptr<RpcBenchmark::TaskStats>>
RpcBenchmark::create_task_stats_map(const BenchConfig::TaskMap& task_map)
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
RpcBenchmark::server_poll()
{
    socket->poll();
    uint64_t start_cycles = PerfUtils::Cycles::rdtsc();
    uint64_t stop_cycles = start_cycles;
    for (SimpleRpc::unique_ptr<SimpleRpc::ServerTask> task = socket->receive();
         task; task = socket->receive()) {
        dispatch(std::move(task));
        stop_cycles = PerfUtils::Cycles::rdtsc();
    }
    active_cycles.fetch_add(stop_cycles - start_cycles,
                            std::memory_order_relaxed);
}

/**
 * Perform incremental work to process outgoing client SimpleRpc
 */
void
RpcBenchmark::client_poll()
{
    if (!run_client) {
        return;
    }

    if (client_running.test_and_set()) {
        return;
    }

    const int buf_size = 1000000;
    char buf[buf_size];

    bool rpc_failed = false;

    uint64_t poll_start_cycles = 0;
    uint64_t poll_cycles = 0;

    uint64_t start_cycles = PerfUtils::Cycles::rdtsc();
    for (const BenchConfig::Client::Phase& phase : config.client.phases) {
        std::vector<SimpleRpc::unique_ptr<SimpleRpc::Rpc>> rpcs;
        for (const BenchConfig::Request& request_config : phase.requests) {
            for (int i = 0; i < request_config.count; ++i) {
                SimpleRpc::unique_ptr<SimpleRpc::Rpc> rpc = socket->allocRpc();
                WireFormat::Benchmark::Request* request =
                    reinterpret_cast<WireFormat::Benchmark::Request*>(buf);
                request->common.opcode = WireFormat::Benchmark::opcode;
                request->taskType = request_config.taskId;
                Homa::Driver::Address dest =
                    selectServer(request_config.taskId, i);
                assert(request_config.size >=
                       sizeof(WireFormat::Benchmark::Request));
                assert(request_config.size <= sizeof(buf));
                Homa::unique_ptr<Homa::OutMessage> message =
                    rpc->allocRequest();
                int bytesRemaining = request_config.size;
                while (bytesRemaining > 0) {
                    int bytesToCopy = std::min(bytesRemaining, buf_size);
                    message->append(buf, bytesToCopy);
                    bytesRemaining -= bytesToCopy;
                }
                rpc->send(dest, std::move(message));
                rpcs.push_back(std::move(rpc));
                poll_start_cycles = PerfUtils::Cycles::rdtsc();
                socket->poll();
                poll_cycles += (PerfUtils::Cycles::rdtsc() - poll_start_cycles);
            }
        }
        poll_start_cycles = PerfUtils::Cycles::rdtsc();
        for (auto it = rpcs.begin(); it != rpcs.end(); ++it) {
            SimpleRpc::Rpc* rpc = it->get();
            while (rpc->checkStatus() == SimpleRpc::Rpc::Status::IN_PROGRESS) {
                socket->poll();
                if (!run_client) {
                    return;
                }
            }
            rpc->wait();
            poll_cycles += (PerfUtils::Cycles::rdtsc() - poll_start_cycles);
            poll_start_cycles = PerfUtils::Cycles::rdtsc();
            if (rpc->checkStatus() == SimpleRpc::Rpc::Status::FAILED) {
                rpc_failed = true;
                break;
            }
        }
        if (rpc_failed) {
            break;
        }
    }
    uint64_t stop_cycles = PerfUtils::Cycles::rdtsc();
    active_cycles.fetch_add(
        PerfUtils::Cycles::rdtsc() - start_cycles - poll_cycles,
        std::memory_order_relaxed);
    client_running.clear();

    if (!rpc_failed) {
        // Update stats
        std::lock_guard<std::mutex> lock(stats_mutex);
        client_stats.samples.at(client_stats.sample_count & SAMPLE_INDEX_MASK) =
            stop_cycles - start_cycles;
        client_stats.sample_count++;
        client_stats.count++;
    } else {
        client_stats.failures++;
    }
}

Homa::Driver::Address
RpcBenchmark::selectServer(int taskType, int index)
{
    index = (index + server_id) % config.tasks.at(taskType).servers.size();
    int server_id = config.tasks.at(taskType).servers.at(index);
    return server_address_book.at(server_id);
}

void
RpcBenchmark::dispatch(SimpleRpc::unique_ptr<SimpleRpc::ServerTask> task)
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
RpcBenchmark::handleBenchmarkTask(
    SimpleRpc::unique_ptr<SimpleRpc::ServerTask> task)
{
    WireFormat::Benchmark::Request request;
    task->getRequest()->get(0, &request, sizeof(request));
    const BenchConfig::Task& task_config = config.tasks.at(request.taskType);

    const int buf_size = 1000000;
    char buf[buf_size];

    // Only one response is supported.  Take the first one if multiple are
    // configured.
    const BenchConfig::Response& response_config =
        task_config.responses.front();
    assert(response_config.size <= sizeof(buf));
    Homa::unique_ptr<Homa::OutMessage> message = task->allocOutMessage();
    int bytesRemaining = response_config.size;
    while (bytesRemaining > 0) {
        int bytesToCopy = std::min(bytesRemaining, buf_size);
        message->append(buf, bytesToCopy);
        bytesRemaining -= bytesToCopy;
    }
    task->reply(std::move(message));

    // Update stats
    task_stats.at(request.taskType)
        ->count.fetch_add(1, std::memory_order_relaxed);
}

}  // namespace RooBench
