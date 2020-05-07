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

#ifndef SIMPLERPC_INCLUDE_SIMPLERPC_SIMPLERPC_H
#define SIMPLERPC_INCLUDE_SIMPLERPC_SIMPLERPC_H

#include <Homa/Driver.h>
#include <Homa/Homa.h>

#include <atomic>
#include <bitset>
#include <cstdint>
#include <memory>

namespace SimpleRpc {

// Forward declarations
class Socket;

/**
 * Shorthand for an std::unique_ptr with a customized deleter.
 */
template <typename T>
using unique_ptr = std::unique_ptr<T, typename T::Deleter>;

/**
 * A simple RPC with a request message and a response message sent to and
 * received from a SimpleRpc Socket.
 *
 * This class is NOT thread-safe.
 */
class Rpc {
  public:
    /**
     * Custom deleter for use with std::unique_ptr.
     */
    struct Deleter {
        void operator()(Rpc* rpc)
        {
            rpc->destroy();
        }
    };

    /**
     * Encodes the status of a Rpc.
     */
    enum class Status {
        NOT_STARTED,  //< Initial state before any request has been sent.
        IN_PROGRESS,  //< One or more requests have been sent but not all
                      //< expected responses have been received.
        COMPLETED,    //< All expected responses have been received.
        FAILED,       // The Rpc has failed to send.
    };

    /**
     * Return a new OutMessage that can be sent as a request for this
     * Rpc.
     *
     * @return
     *      A newly allocated message object.  Ownership of the message object
     *      is transferred to the caller.
     */
    virtual Homa::unique_ptr<Homa::OutMessage> allocRequest() = 0;

    /**
     * Send a new request for this Rpc asynchronously.
     *
     * @param destination
     *      The network address to which the request will be sent.
     * @param request
     *      The request that should be sent.  Ownership of the request message
     *      is transferred to this Rpc.
     */
    virtual void send(Homa::Driver::Address destination,
                      Homa::unique_ptr<Homa::OutMessage> request) = 0;

    /**
     * Return a received response for this Rpc.
     *
     * @return
     *      Returns a received response message, if available; otherwise, a
     *      nullptr is returned.  Ownership of returned response message objects
     *      are transferred to the caller.
     */
    virtual Homa::unique_ptr<Homa::InMessage> receive() = 0;

    /**
     * Check and return the current Status of this Rpc.
     */
    virtual Status checkStatus() = 0;

    /**
     * Wait until all expected responses have been received or the Rpc
     * encountered some kind of failure.
     */
    virtual void wait() = 0;

  protected:
    /**
     * Destruct this ServerTask and free any associated memory.
     */
    virtual void destroy() = 0;
};

/**
 * A handle for an incoming request providing access to the request message and
 * an interface for sending a response.
 *
 * This class is NOT thread-safe.
 */
class ServerTask {
  public:
    /**
     * Custom deleter for use with std::unique_ptr.
     */
    struct Deleter {
        void operator()(ServerTask* task)
        {
            task->destroy();
        }
    };

    /**
     * Return the incoming request message.
     *
     * @return
     *      Pointer to the incoming request message.  Ownership of the message
     *      is not transferred to the caller; the message's lifetime is tied to
     *      this ServerTask.
     */
    virtual Homa::InMessage* getRequest() = 0;

    /**
     * Return a message that can be populated use as a rely message or an
     * additional request message.
     *
     * @return
     *      Pointer to an OutMessage object associated with this ServerTask; the
     *      message should only be used with this ServerTask. Ownership is
     *      transferred to the caller.
     */
    virtual Homa::unique_ptr<Homa::OutMessage> allocOutMessage() = 0;

    /**
     * Send a message back to the initial Rpc requestor.
     *
     * @param message
     *      Response message to return to Rpc initiator.  Ownership of
     * the message object is transferred to this ServerTask.
     */
    virtual void reply(Homa::unique_ptr<Homa::OutMessage> message) = 0;

  protected:
    /**
     * Destruct this ServerTask and free any associated memory.
     */
    virtual void destroy() = 0;
};

/**
 * Manages the Rpcs sent and received through a single transport.
 *
 * This class is thread-safe.
 */
class Socket {
  public:
    /**
     * Create a new Socket.
     *
     * @param transport
     *      The transport through which message can be sent and received.  The
     *      created socket assumes exclusive access to this transport.
     */
    static std::unique_ptr<Socket> create(Homa::Transport* transport);

    /**
     * Allocate a new Rpc that is managed by this socket.
     */
    virtual SimpleRpc::unique_ptr<Rpc> allocRpc() = 0;

    /**
     * Check for and return an incoming request.
     *
     * @return
     *      A request context for an incoming request, if available.  Otherwise,
     *      an empty pointer returned.
     */
    virtual SimpleRpc::unique_ptr<ServerTask> receive() = 0;

    /**
     * Make incremental progress performing Socket management.
     *
     * This method MUST be called for the Socket to make progress and should
     * be called frequently to ensure timely progress.
     */
    virtual void poll() = 0;

    /**
     * Return the driver used to send and received packets for this Socket.
     */
    virtual Homa::Driver* getDriver() = 0;
};

}  // namespace SimpleRpc

#endif  // SIMPLERPC_INCLUDE_SIMPLERPC_SIMPLERPC_H
