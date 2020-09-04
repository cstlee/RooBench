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

#include "ServerTaskImpl.h"

#include "Debug.h"
#include "Perf.h"
#include "SocketImpl.h"

namespace SimpleRpc {

/**
 * ServerTaskImpl constructor.
 *
 * @param socket
 *      Socket managing this ServerTask.
 * @param requestHeader
 *      Contents of the request header.  This constructor does not take
 *      ownership of this pointer.
 * @param request
 *      The request message associated with this ServerTask.
 */
ServerTaskImpl::ServerTaskImpl(SocketImpl* socket,
                               Proto::RequestHeader const* requestHeader,
                               Homa::unique_ptr<Homa::InMessage> request)
    : socket(socket)
    , rpcId(requestHeader->rpcId)
    , request(std::move(request))
    , replyAddress(socket->transport->getDriver()->getAddress(
          &requestHeader->replyAddress))
    , response()
{
    this->request->strip(sizeof(Proto::RequestHeader));
}

/**
 * ServerTaskImpl destructor.
 */
ServerTaskImpl::~ServerTaskImpl() = default;

/**
 * @copydoc ServerTask::getRequest()
 */
Homa::InMessage*
ServerTaskImpl::getRequest()
{
    return request.get();
}

/**
 * @copydoc ServerTask::reply()
 */
void
ServerTaskImpl::reply(const void* response, size_t length)
{
    Perf::Timer timer;
    Homa::unique_ptr<Homa::OutMessage> message = socket->transport->alloc();
    Proto::ResponseHeader header(rpcId);
    message->append(&header, sizeof(header));
    message->append(response, length);
    Perf::counters.tx_message_bytes.add(sizeof(Proto::ResponseHeader) + length);
    message->send(replyAddress, Homa::OutMessage::Options::NO_RETRY);
    this->response = std::move(message);
    Perf::counters.server_api_cycles.add(timer.split());
}

/**
 * @copydoc ServerTask::destroy()
 */
void
ServerTaskImpl::destroy()
{
    Perf::Timer timer;
    socket->dropTask(this);  // Do not access _this_ after this point as the
                             // destructor will have been called and the memory
                             // may be deallocated.
    Perf::counters.server_api_cycles.add(timer.split());
}

}  // namespace SimpleRpc
