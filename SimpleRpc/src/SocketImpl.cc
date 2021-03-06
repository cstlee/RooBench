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

#include "SocketImpl.h"

#include <PerfUtils/Cycles.h>

#include "Debug.h"
#include "Perf.h"
#include "RpcImpl.h"
#include "ServerTaskImpl.h"

namespace SimpleRpc {

/**
 * Construct a SocketImpl.
 *
 * @param transport
 *      Homa transport to which this socket has exclusive access.
 */
SocketImpl::SocketImpl(Homa::Transport* transport)
    : transport(transport)
    , socketId(transport->getId())
    , nextSequenceNumber(1)
    , mutex()
    , rpcPool()
    , taskPool()
    , rpcs()
    , pendingTasks()
{}

/**
 * SocketImpl destructor.
 */
SocketImpl::~SocketImpl()
{
    for (ServerTaskImpl* task : pendingTasks) {
        taskPool.destroy(task);
    }
}

/**
 * @copydoc SimpleRpc::Socket::allocRpc()
 */
SimpleRpc::unique_ptr<Rpc>
SocketImpl::allocRpc()
{
    Perf::Timer timer;
    SpinLock::Lock lock_socket(mutex);
    Proto::RpcId rpcId = allocRpcId();
    RpcImpl* rpc = rpcPool.construct(this, rpcId);
    rpcs.insert({rpcId, rpc});
    Perf::counters.client_api_cycles.add(timer.split());
    return SimpleRpc::unique_ptr<Rpc>(rpc);
}

/**
 * @copydoc SimpleRpc::Socket::receive()
 */
SimpleRpc::unique_ptr<ServerTask>
SocketImpl::receive()
{
    Perf::Timer timer;
    SpinLock::Lock lock_socket(mutex);
    SimpleRpc::unique_ptr<ServerTask> task;
    if (!pendingTasks.empty()) {
        task = SimpleRpc::unique_ptr<ServerTask>(pendingTasks.front());
        pendingTasks.pop_front();
        Perf::counters.server_api_cycles.add(timer.split());
    }
    return task;
}

/**
 * @copydoc SimpleRpc::Socket::poll()
 */
void
SocketImpl::poll()
{
    // Let the transport make incremental progress.
    transport->poll();

    // Keep track of time spent doing active processing versus idle.
    Perf::Timer timer;
    Perf::Timer activeTimer;

    // Process incoming messages
    for (Homa::unique_ptr<Homa::InMessage> message = transport->receive();
         message; message = std::move(transport->receive())) {
        Proto::HeaderCommon common;
        message->get(0, &common, sizeof(common));
        if (common.opcode == Proto::Opcode::Request) {
            // Incoming message is a request.
            Proto::RequestHeader header;
            message->get(0, &header, sizeof(header));
            Perf::counters.rx_message_bytes.add(message->length() -
                                                sizeof(header));
            ServerTaskImpl* task =
                taskPool.construct(this, &header, std::move(message));
            SpinLock::Lock lock_socket(mutex);
            pendingTasks.push_back(task);
        } else if (common.opcode == Proto::Opcode::Response) {
            // Incoming message is a response
            Proto::ResponseHeader header;
            message->get(0, &header, sizeof(header));
            Perf::counters.rx_message_bytes.add(message->length() -
                                                sizeof(header));
            SpinLock::Lock lock_socket(mutex);
            auto it = rpcs.find(header.rpcId);
            if (it != rpcs.end()) {
                RpcImpl* rpc = it->second;
                rpc->handleResponse(&header, std::move(message));
            } else {
                // There is no Rpc waiting for this message.
            }
        } else {
            WARNING("Unexpected protocol message received.");
        }
        Perf::counters.poll_active_cycles.add(activeTimer.split());
    }
    Perf::counters.poll_total_cycles.add(timer.split());
}

/**
 * Discard a previously allocated Rpc.
 */
void
SocketImpl::dropRpc(RpcImpl* rpc)
{
    SpinLock::Lock lock_socket(mutex);
    rpcs.erase(rpc->getId());
    rpcPool.destroy(rpc);
}

/**
 * Discard a the given ServerTask.
 */
void
SocketImpl::dropTask(ServerTaskImpl* task)
{
    SpinLock::Lock lock_socket(mutex);
    taskPool.destroy(task);
}

/**
 * Return a new unique RpcId.
 */
Proto::RpcId
SocketImpl::allocRpcId()
{
    return Proto::RpcId(
        socketId, nextSequenceNumber.fetch_add(1, std::memory_order_relaxed));
}

}  // namespace SimpleRpc
