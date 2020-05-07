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

#include <SimpleRpc/SimpleRpc.h>
#include <gtest/gtest.h>

#include "Mock/MockHoma.h"

namespace SimpleRpc {
namespace {

using ::testing::Return;

class SimpleRpcTest : public ::testing::Test {
  public:
    SimpleRpcTest()
        : transport()
    {}
    ~SimpleRpcTest() {}

    Mock::Homa::MockTransport transport;
};

TEST_F(SimpleRpcTest, Socket_create)
{
    EXPECT_CALL(transport, getId()).WillOnce(Return(42));
    std::unique_ptr<SimpleRpc::Socket> socket =
        SimpleRpc::Socket::create(&transport);
}

}  // namespace
}  // namespace SimpleRpc
