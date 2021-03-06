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

#include <gtest/gtest.h>

#include "Mock/MockHoma.h"
#include "ServerTaskImpl.h"
#include "SocketImpl.h"

namespace SimpleRpc {
namespace {

using ::testing::_;
using ::testing::An;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::Return;
using ::testing::TypedEq;

class ServerTaskImplTest : public ::testing::Test {
  public:
    ServerTaskImplTest()
        : transport()
        , driver()
        , inMessage()
        , outMessage()
        , replyAddress(0xDEADBEEF)
        , socket(nullptr)
        , task(nullptr)
    {
        ON_CALL(transport, getId()).WillByDefault(Return(42));
        ON_CALL(transport, getDriver()).WillByDefault(Return(&driver));

        // Setup default socket
        EXPECT_CALL(transport, getId());
        socket = new SocketImpl(&transport);
    }

    ~ServerTaskImplTest()
    {
        if (task != nullptr) {
            EXPECT_CALL(inMessage, release());
            socket->taskPool.destroy(task);
        }
        delete socket;
    }

    void initDefaultTask()
    {
        EXPECT_CALL(transport, getDriver());
        EXPECT_CALL(driver,
                    getAddress(An<const Homa::Driver::WireFormatAddress*>()))
            .WillOnce(Return(replyAddress));
        EXPECT_CALL(inMessage, strip(An<size_t>()));
        Proto::RequestHeader header;
        header.rpcId = Proto::RpcId(1, 1);
        Homa::unique_ptr<Homa::InMessage> request(&inMessage);
        task = socket->taskPool.construct(socket, &header, std::move(request));
    }

    Mock::Homa::MockTransport transport;
    Mock::Homa::MockDriver driver;
    Mock::Homa::MockInMessage inMessage;
    Mock::Homa::MockOutMessage outMessage;
    Homa::Driver::Address replyAddress;
    SocketImpl* socket;
    ServerTaskImpl* task;
};

TEST_F(ServerTaskImplTest, constructor)
{
    Proto::RequestHeader header;
    header.rpcId = Proto::RpcId(1, 1);
    Homa::unique_ptr<Homa::InMessage> request(&inMessage);

    EXPECT_CALL(transport, getDriver());
    EXPECT_CALL(driver,
                getAddress(TypedEq<const Homa::Driver::WireFormatAddress*>(
                    &header.replyAddress)))
        .WillOnce(Return(0xDEADBEEF));
    EXPECT_CALL(inMessage, strip(Eq(sizeof(Proto::RequestHeader))));

    ServerTaskImpl task(socket, &header, std::move(request));
    EXPECT_EQ(socket, task.socket);
    EXPECT_EQ(header.rpcId, task.rpcId);
    EXPECT_EQ(&inMessage, task.request.get());
    EXPECT_EQ(0xDEADBEEF, task.replyAddress);

    EXPECT_CALL(inMessage, release());
}

TEST_F(ServerTaskImplTest, getRequest)
{
    initDefaultTask();
    EXPECT_EQ(&inMessage, task->getRequest());
}

TEST_F(ServerTaskImplTest, reply)
{
    initDefaultTask();

    char buffer[1024];

    EXPECT_CALL(transport, alloc())
        .WillOnce(
            Return(ByMove(Homa::unique_ptr<Homa::OutMessage>(&outMessage))));
    EXPECT_CALL(outMessage,
                append(An<const void*>(), Eq(sizeof(Proto::RequestHeader))));
    EXPECT_CALL(outMessage, append(Eq(buffer), Eq(sizeof(buffer))));
    EXPECT_CALL(outMessage, send(Eq(replyAddress)));
    EXPECT_CALL(outMessage, release());

    task->reply(buffer, sizeof(buffer));
}

TEST_F(ServerTaskImplTest, destroy)
{
    initDefaultTask();
    EXPECT_EQ(1, socket->taskPool.outstandingObjects);
    EXPECT_CALL(inMessage, release());
    task->destroy();
    EXPECT_EQ(0, socket->taskPool.outstandingObjects);
    task = nullptr;
}

}  // namespace
}  // namespace SimpleRpc
