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

#include <SimpleRpc/Debug.h>
#include <gtest/gtest.h>

#include "Mock/MockHoma.h"
#include "RpcImpl.h"
#include "SocketImpl.h"

namespace SimpleRpc {
namespace {

using ::testing::An;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::Return;

class RpcImplTest : public ::testing::Test {
  public:
    RpcImplTest()
        : transport()
        , driver()
        , inMessage()
        , outMessage()
        , rpcId(42, 1)
        , replyAddress(0xDEADBEEF)
        , socket(nullptr)
        , rpc(nullptr)
    {
        ON_CALL(transport, getId()).WillByDefault(Return(42));
        ON_CALL(transport, getDriver()).WillByDefault(Return(&driver));

        // Setup default socket
        EXPECT_CALL(transport, getId());
        socket = new SocketImpl(&transport);

        // Setup default Rpc
        rpc = new RpcImpl(socket, rpcId);
    }

    ~RpcImplTest()
    {
        delete rpc;
        delete socket;
    }

    Mock::Homa::MockTransport transport;
    Mock::Homa::MockDriver driver;
    Mock::Homa::MockInMessage inMessage;
    Mock::Homa::MockOutMessage outMessage;
    Proto::RpcId rpcId;
    Homa::Driver::Address replyAddress;
    SocketImpl* socket;
    RpcImpl* rpc;
};

struct VectorHandler {
    VectorHandler()
        : messages()
    {}
    void operator()(Debug::DebugMessage message)
    {
        messages.push_back(message);
    }
    std::vector<Debug::DebugMessage> messages;
};

TEST_F(RpcImplTest, constructor)
{
    RpcImpl rpc(socket, rpcId);
    EXPECT_EQ(socket, rpc.socket);
    EXPECT_EQ(rpcId, rpc.rpcId);
}

TEST_F(RpcImplTest, send)
{
    char buffer[1024];

    EXPECT_FALSE(rpc->request);

    EXPECT_CALL(transport, alloc())
        .WillOnce(
            Return(ByMove(Homa::unique_ptr<Homa::OutMessage>(&outMessage))));
    EXPECT_CALL(transport, getDriver()).Times(2);
    EXPECT_CALL(driver, getLocalAddress()).WillOnce(Return(replyAddress));
    EXPECT_CALL(driver,
                addressToWireFormat(Eq(replyAddress),
                                    An<Homa::Driver::WireFormatAddress*>()));
    EXPECT_CALL(outMessage,
                append(An<const void*>(), Eq(sizeof(Proto::RequestHeader))));
    EXPECT_CALL(outMessage, append(Eq(buffer), Eq(sizeof(buffer))));
    EXPECT_CALL(outMessage, send(Eq(0xFEED)));

    rpc->send(0xFEED, buffer, sizeof(buffer));

    EXPECT_TRUE(rpc->request);

    EXPECT_CALL(outMessage, release());
}

TEST_F(RpcImplTest, receive)
{
    Homa::InMessage* message = rpc->receive();
    EXPECT_EQ(nullptr, message);

    rpc->response = std::move(Homa::unique_ptr<Homa::InMessage>(&inMessage));

    message = rpc->receive();
    EXPECT_EQ(&inMessage, message);
}

TEST_F(RpcImplTest, checkStatus)
{
    EXPECT_EQ(Rpc::Status::NOT_STARTED, rpc->checkStatus());

    rpc->request = std::move(Homa::unique_ptr<Homa::OutMessage>(&outMessage));

    EXPECT_CALL(outMessage, getStatus())
        .WillOnce(Return(Homa::OutMessage::Status::IN_PROGRESS));
    EXPECT_EQ(Rpc::Status::IN_PROGRESS, rpc->checkStatus());

    EXPECT_CALL(outMessage, getStatus())
        .WillOnce(Return(Homa::OutMessage::Status::FAILED));
    EXPECT_EQ(Rpc::Status::FAILED, rpc->checkStatus());

    rpc->responseArrived = true;
    EXPECT_EQ(Rpc::Status::COMPLETED, rpc->checkStatus());

    EXPECT_CALL(outMessage, release());
}

TEST_F(RpcImplTest, wait)
{
    // nothing to test
    rpc->wait();
}

TEST_F(RpcImplTest, destroy)
{
    // nothing to test
}

TEST_F(RpcImplTest, handleResponse_basic)
{
    Proto::ResponseHeader header;
    Homa::unique_ptr<Homa::InMessage> message(&inMessage);
    EXPECT_CALL(inMessage, acknowledge());
    EXPECT_CALL(inMessage, strip(Eq(sizeof(Proto::ResponseHeader))));

    EXPECT_FALSE(rpc->responseArrived);
    EXPECT_FALSE(rpc->response);

    rpc->handleResponse(&header, std::move(message));

    EXPECT_TRUE(rpc->responseArrived);
    ASSERT_TRUE(rpc->response);
    EXPECT_EQ(&inMessage, rpc->response.get());

    EXPECT_CALL(inMessage, release());
}

TEST_F(RpcImplTest, handleResponse_duplicate)
{
    Proto::ResponseHeader header;
    Homa::unique_ptr<Homa::InMessage> message(&inMessage);
    EXPECT_CALL(inMessage, acknowledge());
    EXPECT_CALL(inMessage, strip(Eq(sizeof(Proto::ResponseHeader))));

    rpc->responseArrived = true;
    EXPECT_FALSE(rpc->response);

    EXPECT_CALL(inMessage, release());

    VectorHandler handler;
    Debug::setLogHandler(std::ref(handler));

    rpc->handleResponse(&header, std::move(message));

    EXPECT_EQ(1U, handler.messages.size());
    const Debug::DebugMessage& m = handler.messages.at(0);
    EXPECT_STREQ("src/RpcImpl.cc", m.filename);
    EXPECT_STREQ("handleResponse", m.function);
    EXPECT_EQ(int(Debug::LogLevel::NOTICE), m.logLevel);
    EXPECT_EQ("Duplicate response received for Rpc (42, 1)", m.message);
    Debug::setLogHandler(std::function<void(Debug::DebugMessage)>());

    EXPECT_TRUE(rpc->responseArrived);
    EXPECT_FALSE(rpc->response);
}

}  // namespace
}  // namespace SimpleRpc
