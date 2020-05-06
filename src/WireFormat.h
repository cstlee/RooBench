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

#ifndef ROOBENCH_WIREFORMAT_H
#define ROOBENCH_WIREFORMAT_H

namespace RooBench {
namespace WireFormat {

enum Opcode {
    BENCHMARK = 1,
    ILLEGAL_OPCODE,
};

struct Common {
    uint16_t opcode;
} __attribute__((packed));

/**
 * Basic WireFormat for messages sent as part of the benchmark.
 */
struct Benchmark {
    static const Opcode opcode = Opcode::BENCHMARK;

    struct Request {
        Common common;
        uint16_t taskType;

        Request() = default;

        explicit Request(uint16_t taskType)
            : common({opcode})
            , taskType(taskType)
        {}
    } __attribute__((packed));
};

}  // namespace WireFormat
}  // namespace RooBench

#endif  // ROOBENCH_WIREFORMAT_H
