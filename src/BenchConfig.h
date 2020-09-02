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

#ifndef ROOBENCH_BENCHCONFIG_H
#define ROOBENCH_BENCHCONFIG_H

#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace RooBench {

struct BenchConfig {
    struct Request {
        int taskId;
        int size;
        int count;
    };

    struct Response {
        int size;
        int count;
    };

    struct Server {
        std::string address;
    };
    using ServerList = std::unordered_map<int, Server>;

    /**
     * Client configuration parameters
     */
    struct Client {
        struct Phase {
            std::vector<Request> requests;
        };
        std::vector<Phase> phases;
        std::vector<int> servers;
    };

    /**
     * Task configuration parameters
     */
    struct Task {
        std::vector<Request> requests;
        std::vector<Response> responses;
        std::vector<int> servers;
    };
    using TaskMap = std::unordered_map<int, Task>;

    Client client;
    TaskMap tasks;
    ServerList serverList;
    int client_count;
    bool unified;
    double load;

    explicit BenchConfig(const nlohmann::json& config)
        : serverList()
        , client()
        , tasks()
        , client_count()
        , load()
        , unified(false)
    {
        // Load workload
        auto& workload_config = config.at("workload");
        // Load client
        auto& client_config = workload_config.at("client");
        for (auto& phase : client_config.at("phases")) {
            client.phases.push_back({});
            // load requests
            for (auto& request_config : phase.at("requests")) {
                Request request;
                request.taskId = request_config.at("task_id").get<int>();
                request.size = request_config.at("size").get<int>();
                request.count = request_config.at("count").get<int>();
                client.phases.back().requests.push_back(request);
            }
        }
        for (auto& server_id : client_config.at("servers")) {
            client.servers.push_back(server_id.get<int>());
        }
        // Load tasks
        auto& tasks_config = workload_config.at("tasks");
        for (auto& task_config : tasks_config) {
            int task_id = task_config.at("id").get<int>();
            tasks.insert({task_id, {}});
            // load requests
            for (auto& request_config : task_config.at("requests")) {
                Request request;
                request.taskId = request_config.at("task_id").get<int>();
                request.size = request_config.at("size").get<int>();
                request.count = request_config.at("count").get<int>();
                tasks.at(task_id).requests.push_back(request);
            }
            // load responses
            for (auto& response_config : task_config.at("responses")) {
                Response response;
                response.size = response_config.at("size").get<int>();
                response.count = response_config.at("count").get<int>();
                tasks.at(task_id).responses.push_back(response);
            }
            // load servers
            for (auto& server_id : task_config.at("servers")) {
                tasks.at(task_id).servers.push_back(server_id.get<int>());
            }
        }

        // Load server list
        for (auto& server : config.at("server_list").at("servers")) {
            serverList.insert({server.at("id").get<int>(),
                               {server.at("address").get<std::string>()}});
        }

        // Load other configurations
        client_count = config.at("client_count");
        load = config.at("load");
        unified = config.at("unified");
    }

    void dumps() const
    {
        std::cout << "Workload:" << std::endl;
        std::cout << "  Client:" << std::endl;
        for (auto& phase : client.phases) {
            std::cout << "    [" << std::endl;
            for (auto& request : phase.requests) {
                std::cout << "      -> {id: " << request.taskId
                          << ", size: " << request.size
                          << ", count: " << request.count << "}" << std::endl;
            }
            std::cout << "    ]" << std::endl;
        }
        std::cout << "  Tasks:" << std::endl;
        for (auto& elem : tasks) {
            std::cout << "    id: " << elem.first << " [" << std::endl;
            std::cout << "      servers:";
            for (auto& server : elem.second.servers) {
                std::cout << " " << server;
            }
            std::cout << std::endl;
            for (auto& request : elem.second.requests) {
                std::cout << "      -> {id: " << request.taskId
                          << ", size: " << request.size
                          << ", count: " << request.count << "}" << std::endl;
            }
            for (auto& response : elem.second.responses) {
                std::cout << "      <- {size: " << response.size
                          << ", count: " << response.count << "}" << std::endl;
            }
            std::cout << "    ]" << std::endl;
        }
        std::cout << "Server List" << std::endl;
        for (auto elem : serverList) {
            std::cout << elem.first << " : " << elem.second.address
                      << std::endl;
        }
        std::cout << "client_count: " << client_count << std::endl;
        std::cout << "load: " << load << std::endl;
        std::cout << "unified: " << unified << std::endl;
    }
};

}  // namespace RooBench

#endif  // ROOBENCH_BENCHCONFIG_H
