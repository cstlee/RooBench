/* Copyright (c) 2011-2020, Stanford University
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

#include <string>
#include <thread>
#include <unordered_map>

#include "ThreadId.h"

namespace SimpleRpc {

namespace ThreadId {
namespace Internal {
extern __thread uint64_t id;
extern uint64_t nextId;
extern std::unordered_map<uint64_t, std::string> threadNames;
}  // namespace Internal
}  // namespace ThreadId

namespace {

class ThreadIdTest : public ::testing::Test {
  public:
    ThreadIdTest()
    {
        ThreadId::Internal::id = 0;
        ThreadId::Internal::nextId = 1;
        ThreadId::Internal::threadNames.clear();
    }
};

// Helper function that runs in a separate thread.  It reads its id and
// saves it in the variable pointed to by its argument.
static void
readThreadId(uint64_t* p)
{
    *p = ThreadId::getId();
}

TEST_F(ThreadIdTest, basics)
{
    uint64_t value;
    EXPECT_EQ(1U, ThreadId::getId());
    EXPECT_EQ(1U, ThreadId::getId());
    std::thread thread1(readThreadId, &value);
    thread1.join();
    EXPECT_EQ(2U, value);
    std::thread thread2(readThreadId, &value);
    thread2.join();
    EXPECT_EQ(3U, value);
}

TEST_F(ThreadIdTest, names)
{
    EXPECT_EQ("thread 1", ThreadId::getName());
    ThreadId::setName("foo");
    EXPECT_EQ("foo", ThreadId::getName());
    ThreadId::setName("bar");
    EXPECT_EQ("bar", ThreadId::getName());
    ThreadId::setName("");
    EXPECT_EQ("thread 1", ThreadId::getName());
}

}  // namespace
}  // namespace SimpleRpc
