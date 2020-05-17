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

#include "RpcImpl.h"

#include "Debug.h"
#include "Perf.h"
#include "SocketImpl.h"

namespace SimpleRpc {

/**
 * RpcImpl constructor.
 */
RpcImpl::RpcImpl(SocketImpl* socket, Proto::RpcId rpcId)
    : socket(socket)
    , rpcId(rpcId)
    , responseArrived(false)
    , request()
    , response()
{}

/**
 * RpcImpl destructor.
 */
RpcImpl::~RpcImpl() = default;

/**
 * @copydoc RpcImpl::allocRequest()
 */
Homa::unique_ptr<Homa::OutMessage>
RpcImpl::allocRequest()
{
    Homa::unique_ptr<Homa::OutMessage> message = socket->transport->alloc();
    message->reserve(sizeof(Proto::RequestHeader));
    return message;
}

/**
 * @copydoc RpcImpl::send()
 */
void
RpcImpl::send(Homa::Driver::Address destination,
              Homa::unique_ptr<Homa::OutMessage> message)
{
    SpinLock::Lock lock(mutex);
    request = std::move(message);
    Perf::counters.tx_message_bytes.add(request->length());
    Homa::Driver::Address replyAddress =
        socket->transport->getDriver()->getLocalAddress();
    Proto::RequestHeader outboundHeader(rpcId);
    socket->transport->getDriver()->addressToWireFormat(
        replyAddress, &outboundHeader.replyAddress);
    request->prepend(&outboundHeader, sizeof(outboundHeader));
    request->send(destination);
}

/**
 * @copydoc RpcImpl::receive()
 */
Homa::unique_ptr<Homa::InMessage>
RpcImpl::receive()
{
    SpinLock::Lock lock(mutex);
    return std::move(response);
}

/**
 * @copydoc RpcImpl::checkStatus()
 */
Rpc::Status
RpcImpl::checkStatus()
{
    SpinLock::Lock lock(mutex);
    if (!request) {
        return Status::NOT_STARTED;
    } else if (responseArrived) {
        return Status::COMPLETED;
    } else if (request->getStatus() == Homa::OutMessage::Status::FAILED) {
        return Status::FAILED;
    } else {
        return Status::IN_PROGRESS;
    }
}

/**
 * @copydoc RpcImpl::wait()
 */
void
RpcImpl::wait()
{
    while (checkStatus() == Status::IN_PROGRESS) {
        socket->poll();
    }
}

/**
 * @copydoc RpcImpl::destroy()
 */
void
RpcImpl::destroy()
{
    // Don't actually free the object yet.  Return contol to the managing
    // socket so it can do some clean up.
    socket->dropRpc(this);
}

/**
 * Add the incoming response message to this Rpc.
 *
 * @param header
 *      Preparsed header for the incoming response.
 * @param message
 *      The incoming response message to add.
 */
void
RpcImpl::handleResponse(Proto::ResponseHeader* header,
                        Homa::unique_ptr<Homa::InMessage> message)
{
    SpinLock::Lock lock(mutex);
    (void)header;
    message->strip(sizeof(Proto::ResponseHeader));

    if (!responseArrived) {
        responseArrived = true;
        response = std::move(message);
        request->cancel();
    } else {
        // Response already received
        NOTICE("Duplicate response received for Rpc (%lu, %lu)", rpcId.socketId,
               rpcId.sequence);
    }
}

}  // namespace SimpleRpc
