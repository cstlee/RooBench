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

#include "Server.h"

#include <iostream>

#include "ControlRpc.h"
#include "WireFormat.h"

namespace RooBench {

Server::Server(std::unique_ptr<Roo::Socket> socket,
               Homa::Driver::Address coordinatorAddress, int numThreads)
    : socket(std::move(socket))
    , serverId(enlist(socket.get(), coordinatorAddress))
    , numThreads(numThreads)
    , threads()
    , run_client(false)
{}

void
Server::run(sig_atomic_t& sig_status)
{
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(&Server::run_thread, this, &sig_status);
    }
    for (auto it = threads.begin(); it != threads.end(); ++it) {
        it->join();
    }
}

uint64_t
Server::enlist(Roo::Socket* socket, Homa::Driver::Address dest)
{
    ControlRpc<WireFormat::EnlistServer> rpc(socket);
    rpc.request.common.opcode = WireFormat::EnlistServer::opcode;
    Homa::Driver::Address serverAddress =
        socket->getDriver()->getLocalAddress();
    socket->getDriver()->addressToWireFormat(serverAddress,
                                             &rpc.request.address);
    rpc.send(dest);
    rpc.wait();
    return rpc.response.serverId;
}

void
Server::run_thread(sig_atomic_t* sig_status)
{
    while (*sig_status == 0) {
        poll();
    }
}

void
Server::poll()
{
    socket->poll();
    for (Roo::unique_ptr<Roo::ServerTask> task = socket->receive(); task;
         task = socket->receive()) {
        dispatch(std::move(task));
    }
    run_benchmark_client();
}

void
Server::run_benchmark_client()
{
    if (!run_client.load()) {
        return;
    }

    int request_count = 1;
    int request_size = 100;
    char buf[100];

    Roo::unique_ptr<Roo::RooPC> rpc = socket->allocRooPC();
    for (int i = 0; i < request_count; ++i) {
        Homa::unique_ptr<Homa::OutMessage> message = rpc->allocRequest();
        WireFormat::Benchmark::Request* request =
            reinterpret_cast<WireFormat::Benchmark::Request*>(buf);
        request->common.opcode = WireFormat::Benchmark::opcode;
        message->append(buf, sizeof(buf));
        // rpc->send()
    }
}

void
Server::dispatch(Roo::unique_ptr<Roo::ServerTask> task)
{
    WireFormat::Common common;
    task->getRequest()->get(0, &common, sizeof(common));

    switch (common.opcode) {
        case WireFormat::Benchmark::opcode:
            handleBenchmarkTask(std::move(task));
            break;
        case WireFormat::BenchmarkClientControl::opcode:
            handleBenchmarkClientControl(std::move(task));
            break;
        case WireFormat::UpdateConfig::opcode:
            handleUpdateConfig(std::move(task));
            break;
        default:
            std::cerr << "Unknown opcode" << std::endl;
    }
}

void
Server::handleBenchmarkTask(Roo::unique_ptr<Roo::ServerTask> task)
{
    WireFormat::BenchmarkClientControl::Request request;
    task->getRequest()->get(0, &request, sizeof(request));
    switch (request.cmd) {
        case WireFormat::BenchmarkClientControl::Command::START:
            run_client.store(true);
            break;
        case WireFormat::BenchmarkClientControl::Command::STOP:
            run_client.store(false);
            break;
        default:
            break;
    }
}

void
Server::handleBenchmarkClientControl(Roo::unique_ptr<Roo::ServerTask> task)
{}

void
Server::handleUpdateConfig(Roo::unique_ptr<Roo::ServerTask> task)
{}

}  // namespace RooBench
