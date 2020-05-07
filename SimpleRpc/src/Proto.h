/* Copyright (c) 2018-2020, Stanford University
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

/**
 * @file Proto.h
 *
 * This file contains wire protocol definitions for Rpc messages.
 */

#ifndef SIMPLERPC_PROTO_H
#define SIMPLERPC_PROTO_H

#include <Homa/Driver.h>

#include <cstdint>
#include <functional>

namespace SimpleRpc {
namespace Proto {

/**
 * A unique identifier for a Task.
 */
struct RpcId {
    uint64_t socketId;  ///< Unique id for socket that owns this task.
    uint64_t sequence;  ///< Sequence number for this task (unique for
                        ///< socketId, monotonically increasing).

    /// RpcId default constructor.
    RpcId()
        : socketId(0)
        , sequence(0)
    {}

    /// RpcId constructor.
    RpcId(uint64_t socketId, uint64_t sequence)
        : socketId(socketId)
        , sequence(sequence)
    {}

    /**
     * Comparison function for RpcId, for use in std::maps etc.
     */
    bool operator<(RpcId other) const
    {
        return (socketId < other.socketId) ||
               ((socketId == other.socketId) && (sequence < other.sequence));
    }

    /**
     * Equality function for RpcId, for use in std::unordered_maps etc.
     */
    bool operator==(RpcId other) const
    {
        return ((socketId == other.socketId) && (sequence == other.sequence));
    }

    /**
     * This class computes a hash of an RpcId, so that Id can be used
     * as keys in unordered_maps.
     */
    struct Hasher {
        /// Return a "hash" of the given RpcId.
        std::size_t operator()(const RpcId& taskId) const
        {
            std::size_t h1 = std::hash<uint64_t>()(taskId.socketId);
            std::size_t h2 = std::hash<uint64_t>()(taskId.sequence);
            return h1 ^ (h2 << 1);
        }
    };
} __attribute__((packed));

/**
 * A unique identifier for a response.
 */
struct ResponseId {
    RpcId taskId;       ///< Id of the task that sent this response.
    uint64_t sequence;  ///< Uniquely identifies the response within the task.

    /// ResponseId default constructor.
    ResponseId()
        : taskId(0, 0)
        , sequence(0)
    {}

    /// ResponseId constructor.
    ResponseId(RpcId taskId, uint64_t sequence)
        : taskId(taskId)
        , sequence(sequence)
    {}

    /**
     * Comparison function for ResponseId, for use in std::maps etc.
     */
    bool operator<(ResponseId other) const
    {
        return (taskId < other.taskId) ||
               ((taskId == other.taskId) && (sequence < other.sequence));
    }

    /**
     * Equality function for ResponseId, for use in std::unordered_maps etc.
     */
    bool operator==(ResponseId other) const
    {
        return ((taskId == other.taskId) && (sequence == other.sequence));
    }

    /**
     * This class computes a hash of an ResponseId, so that Id can be used as
     * keys in unordered_maps.
     */
    struct Hasher {
        /// Return a "hash" of the given ResponseId.
        std::size_t operator()(const ResponseId& responseId) const
        {
            std::size_t h1 = RpcId::Hasher()(responseId.taskId);
            std::size_t h2 = std::hash<uint64_t>()(responseId.sequence);
            return h1 ^ (h2 << 1);
        }
    };
} __attribute__((packed));

/**
 * This is the first part of the Homa packet header and is common to all
 * versions of the protocol. The struct contains version information about the
 * protocol used in the encompassing Message. The Transport should always send
 * this prefix and can always expect it when receiving a Homa Message. The
 * prefix is separated into its own struct because the Transport may need to
 * know the protocol version before interpreting the rest of the packet.
 */
struct HeaderPrefix {
    uint8_t version;  ///< The version of the protocol being used by this
                      ///< Message.

    /// HeaderPrefix constructor.
    HeaderPrefix(uint8_t version)
        : version(version)
    {}
} __attribute__((packed));

/**
 * Distinguishes between different protocol messages types.  See the xxx
 * namespace and xxx::Header for more information.
 */
enum class Opcode : uint8_t {
    Request = 1,
    Response,
    Manifest,
    Invalid,
};

/**
 * Contains information needed for all protocol message types.
 */
struct HeaderCommon {
    HeaderPrefix prefix;  ///< Common to all versions of the protocol
    Opcode opcode;        ///< Distinguishes between different protocol messages

    /// HeaderCommon default constructor.
    HeaderCommon()
        : prefix(1)
        , opcode(Opcode::Invalid)
    {}

    /// HeaderCommon constructor.
    HeaderCommon(Opcode opcode)
        : prefix(1)
        , opcode(opcode)
    {}
} __attribute__((packed));

/**
 * Contains the wire format header definitions for sending and receiving a
 * Rpc request message.
 */
struct RequestHeader {
    HeaderCommon common;  ///< Common header information.
    RpcId rpcId;          ///< Id of the Rpc to which this request belongs.
    Homa::Driver::WireFormatAddress replyAddress;  ///< Replies to this request
                                                   ///< should be sent to this
                                                   ///< address.
    // TODO(cstlee): remove pad.
    char pad[4];  // Hack to make Request and Response headers the same length.

    /// RequestHeader default constructor.
    RequestHeader()
        : common(Opcode::Request)
        , rpcId()
        , replyAddress()
    {}

    /// RequestHeader constructor.
    explicit RequestHeader(RpcId rpcId)
        : common(Opcode::Request)
        , rpcId(rpcId)
        , replyAddress()
    {}
} __attribute__((packed));

/**
 * Contains the wire format header definitions for sending and receiving a
 * Rpc response message.
 */
struct ResponseHeader {
    HeaderCommon common;  ///< Common header information.
    RpcId rpcId;          ///< Id of the Rpc to which this request belongs.

    /// ResponseHeader default constructor.
    ResponseHeader()
        : common(Opcode::Response)
        , rpcId()
    {}

    /// ResponseHeader constructor.
    explicit ResponseHeader(RpcId rpcId)
        : common(Opcode::Response)
        , rpcId(rpcId)
    {}
} __attribute__((packed));

}  // namespace Proto
}  // namespace SimpleRpc

#endif  // SIMPLERPC_PROTO_H
