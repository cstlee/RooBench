/* Copyright (c) 2019-2020, Stanford University
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

#include <Homa/Drivers/Fake/FakeDriver.h>
#include <SimpleRpc/Debug.h>
#include <SimpleRpc/SimpleRpc.h>
#include <unistd.h>

#include <atomic>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "StringUtil.h"
#include "docopt.h"

static const char USAGE[] = R"(Homa System Test.

    Usage:
        system_test <count> [-v | -vv | -vvv | -vvvv] [options]
        system_test (-h | --help)
        system_test --version

    Options:
        -h --help       Show this screen.
        --version       Show version.
        -v --verbose    Show verbose output.
        --servers=<n>   Number of virtual servers [default: 1].
        --size=<n>      Number of bytes to send as a payload [default: 10].
        --lossRate=<f>  Rate at which packets are lost [default: 0.0].
)";

bool _PRINT_CLIENT_ = false;
bool _PRINT_SERVER_ = false;

struct MessageHeader {
    uint64_t id;
    uint64_t length;
} __attribute__((packed));

struct Node {
    explicit Node(uint64_t id)
        : id(id)
        , driver()
        , transport(Homa::Transport::create(&driver, id))
        , socket(SimpleRpc::Socket::create(transport))
        , thread()
        , run(false)
    {}

    ~Node()
    {
        socket.reset(nullptr);
        delete transport;
    }

    const uint64_t id;
    Homa::Drivers::Fake::FakeDriver driver;
    Homa::Transport* transport;
    std::unique_ptr<SimpleRpc::Socket> socket;
    std::thread thread;
    std::atomic<bool> run;
};

void
serverMain(Node* server, std::vector<std::string> addresses)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, addresses.size() - 1);

    while (true) {
        if (server->run.load() == false) {
            break;
        }
        SimpleRpc::unique_ptr<SimpleRpc::ServerTask> task(
            server->socket->receive());
        if (task) {
            MessageHeader header;
            task->getRequest()->get(0, &header, sizeof(MessageHeader));

            char response[sizeof(MessageHeader) + header.length];
            task->getRequest()->get(0, &response, sizeof(response));

            if (_PRINT_SERVER_) {
                std::cout << "  -> Server " << server->id
                          << " (rpcId: " << header.id << ")" << std::endl;
            }

            task->reply(response, sizeof(response));
            if (_PRINT_SERVER_) {
                std::cout << "  <- Server " << server->id
                          << " (rpcId: " << header.id << ")" << std::endl;
            }
        }
        server->socket->poll();
    }
}

/**
 * @return
 *      Number of Rpcs that failed.
 */
int
clientMain(int count, int size, std::vector<std::string> addresses)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> randAddr(0, addresses.size() - 1);
    std::uniform_int_distribution<char> randData(0);

    uint64_t nextId = 0;
    int numFailed = 0;

    Node client(1);
    for (int i = 0; i < count; ++i) {
        uint64_t id = nextId++;
        char request[sizeof(MessageHeader) + size];
        MessageHeader* header = reinterpret_cast<MessageHeader*>(request);
        char* payload = &(request[sizeof(MessageHeader)]);
        for (int i = 0; i < size; ++i) {
            payload[i] = randData(gen);
        }

        SimpleRpc::unique_ptr<SimpleRpc::Rpc> rpc(client.socket->allocRpc());
        header->id = id;
        header->length = size;

        std::string destAddress = addresses[randAddr(gen)];
        if (_PRINT_CLIENT_) {
            std::cout << "Client -> (rpcId: " << header->id << ")" << std::endl;
        }
        rpc->send(client.driver.getAddress(&destAddress), request,
                  sizeof(request));

        rpc->wait();

        if (rpc->checkStatus() == SimpleRpc::Rpc::Status::FAILED) {
            numFailed++;
            std::cout << "Rpc FAILED" << std::endl;
            continue;
        }

        Homa::InMessage* response = rpc->receive();
        if (response != nullptr) {
            MessageHeader header;
            char buf[size];
            response->get(0, &header, sizeof(MessageHeader));
            response->get(sizeof(MessageHeader), &buf, header.length);
            if (header.id != id || header.length != size ||
                memcmp(payload, buf, size) != 0) {
                std::cout << "Failed sanity check (" << (header.id != id)
                          << ", " << (header.length != size) << ")"
                          << std::endl;
                std::cout << "Client <" << header.id << ", " << header.length
                          << ">" << std::endl;
                numFailed++;
            }
            if (_PRINT_CLIENT_) {
                std::cout << "Client <- (rpcId: " << header.id << ")"
                          << std::endl;
            }
        }
    }
    return numFailed;
}

int
main(int argc, char* argv[])
{
    std::map<std::string, docopt::value> args =
        docopt::docopt(USAGE, {argv + 1, argv + argc},
                       true,                      // show help if requested
                       "SimpleRpc System Test");  // version string

    // Read in args.
    int numTests = args["<count>"].asLong();
    int numServers = args["--servers"].asLong();
    int numBytes = args["--size"].asLong();
    int verboseLevel = args["--verbose"].asLong();
    double packetLossRate = atof(args["--lossRate"].asString().c_str());

    // level of verboseness
    bool printSummary = false;
    if (verboseLevel > 0) {
        printSummary = true;
        SimpleRpc::Debug::setLogPolicy(
            SimpleRpc::Debug::logPolicyFromString("ERROR"));
    }
    if (verboseLevel > 1) {
        SimpleRpc::Debug::setLogPolicy(
            SimpleRpc::Debug::logPolicyFromString("WARNING"));
    }
    if (verboseLevel > 2) {
        _PRINT_CLIENT_ = true;
        SimpleRpc::Debug::setLogPolicy(
            SimpleRpc::Debug::logPolicyFromString("NOTICE"));
    }
    if (verboseLevel > 3) {
        _PRINT_SERVER_ = true;
        SimpleRpc::Debug::setLogPolicy(
            SimpleRpc::Debug::logPolicyFromString("VERBOSE"));
    }

    Homa::Drivers::Fake::FakeNetworkConfig::setPacketLossRate(packetLossRate);

    uint64_t nextServerId = 101;
    std::vector<std::string> addresses;
    std::vector<Node*> servers;
    for (int i = 0; i < numServers; ++i) {
        Node* server = new Node(nextServerId++);
        addresses.emplace_back(std::string(
            server->driver.addressToString(server->driver.getLocalAddress())));
        servers.push_back(server);
    }

    for (auto it = servers.begin(); it != servers.end(); ++it) {
        Node* server = *it;
        server->run = true;
        server->thread = std::move(std::thread(&serverMain, server, addresses));
    }

    int numFails = clientMain(numTests, numBytes, addresses);

    for (auto it = servers.begin(); it != servers.end(); ++it) {
        Node* server = *it;
        server->run = false;
        server->thread.join();
        delete server;
    }

    if (printSummary) {
        std::cout << numTests << " Rpcs tested: " << numTests - numFails
                  << " completed, " << numFails << " failed" << std::endl;
    }

    return numFails;
}
