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

#ifndef SIMPLERPC_SIMPLERPCPCIMPL_H
#define SIMPLERPC_SIMPLERPCPCIMPL_H

#include <SimpleRpc/SimpleRpc.h>

#include <deque>
#include <unordered_map>

#include "Proto.h"
#include "SpinLock.h"

namespace SimpleRpc {

// Forward Declaration
class SocketImpl;

/**
 * Implementation of Rpc.
 */
class RpcImpl : public Rpc {
  public:
    explicit RpcImpl(SocketImpl* socket, Proto::RpcId rpcId);
    virtual ~RpcImpl();
    virtual void send(Homa::Driver::Address destination, const void* request,
                      size_t length);
    virtual Homa::InMessage* receive();
    virtual Status checkStatus();
    virtual void wait();

    void handleResponse(Proto::ResponseHeader* header,
                        Homa::unique_ptr<Homa::InMessage> message);

    /**
     * Return this Rpc's identifier.
     */
    Proto::RpcId getId()
    {
        return rpcId;
    }

  protected:
    virtual void destroy();

  private:
    /// Monitor-style lock
    SpinLock mutex;

    /// Socket that manages this Rpc.
    SocketImpl* const socket;

    /// Unique identifier for this Rpc.
    Proto::RpcId rpcId;

    /// True when the response has been received.
    bool responseArrived;

    /// Request being sent for this Rpc.
    Homa::unique_ptr<Homa::OutMessage> request;

    /// Response for this Rpc that have not yet been delievered.
    Homa::unique_ptr<Homa::InMessage> response;
};

}  // namespace SimpleRpc

#endif  // SIMPLERPC_SIMPLERPCPCIMPL_H
