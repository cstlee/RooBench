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

#ifndef SIMPLERPC_SERVERTASKIMPL_H
#define SIMPLERPC_SERVERTASKIMPL_H

#include <Homa/Homa.h>
#include <SimpleRpc/SimpleRpc.h>

#include <deque>

#include "Proto.h"

namespace SimpleRpc {

// Forward declaration
class SocketImpl;

/**
 * Implementation of SimpleRpc::ServerTask.
 *
 * This class is NOT thread-safe.
 */
class ServerTaskImpl : public ServerTask {
  public:
    explicit ServerTaskImpl(SocketImpl* socket,
                            Proto::RequestHeader const* requestHeader,
                            Homa::unique_ptr<Homa::InMessage> request);
    virtual ~ServerTaskImpl();
    virtual Homa::InMessage* getRequest();
    virtual void reply(const void* response, size_t length);
    bool poll();

  protected:
    virtual void destroy();

  private:
    /// True if the ServerTask is no longer held by the application and is being
    /// processed by the Socket.
    std::atomic<bool> detached;

    // The socket that manages this ServerTask.
    SocketImpl* const socket;

    /// Identifier the Rpc that triggered this ServerTask.
    Proto::RpcId const rpcId;

    /// Message containing a task request; may come directly from the
    /// Rpc client, or from another server that has delegated a request
    /// to us.
    Homa::unique_ptr<Homa::InMessage> const request;

    /// Address of the client that sent the original request; the reply should
    /// be sent back to this address.
    Homa::Driver::Address const replyAddress;

    /// Response message.
    Homa::unique_ptr<Homa::OutMessage> response;
};

}  // namespace SimpleRpc

#endif  // SIMPLERPC_SERVERTASKIMPL_H
