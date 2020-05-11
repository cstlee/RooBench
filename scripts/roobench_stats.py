#!/usr/bin/env python

# Copyright (c) 2020, Stanford University
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

"""
Usage:
    roobench.py stats <data_dir>

Options:
    -h, --help              Show this screen.
"""

import glob
import os
import json

def stat_diff(stat_name, start_data, end_data):
    return end_data[stat_name] - start_data[stat_name];

def get_transport_stats(data_dir, server_name):
    start_data_file = data_dir + '/' + server_name + '_transport_stats_0.json'
    end_data_file = data_dir + '/' + server_name + '_transport_stats_1.json'
    with open(start_data_file) as f:
        start_data = json.load(f)
    with open(end_data_file) as f:
        end_data = json.load(f)
    cps = end_data["cycles_per_second"]

    data = {}

    data["elapsed_time"] = stat_diff("timestamp", start_data, end_data) / cps
    data["cycles_per_second"] = cps
    data["active_cycles"] = stat_diff("active_cycles", start_data, end_data)
    data["tx_message_bytes"] = stat_diff("tx_message_bytes", start_data, end_data)
    data["rx_message_bytes"] = stat_diff("rx_message_bytes", start_data, end_data)
    data["transport_tx_bytes"] = stat_diff("transport_tx_bytes", start_data, end_data)
    data["transport_rx_bytes"] = stat_diff("transport_rx_bytes", start_data, end_data)
    data["tx_data_pkts"] = stat_diff("tx_data_pkts", start_data, end_data)
    data["rx_data_pkts"] = stat_diff("rx_data_pkts", start_data, end_data)
    data["tx_grant_pkts"] = stat_diff("tx_grant_pkts", start_data, end_data)
    data["rx_grant_pkts"] = stat_diff("rx_grant_pkts", start_data, end_data)
    data["tx_done_pkts"] = stat_diff("tx_done_pkts", start_data, end_data)
    data["rx_done_pkts"] = stat_diff("rx_done_pkts", start_data, end_data)
    data["tx_resend_pkts"] = stat_diff("tx_resend_pkts", start_data, end_data)
    data["rx_resend_pkts"] = stat_diff("rx_resend_pkts", start_data, end_data)
    data["tx_busy_pkts"] = stat_diff("tx_busy_pkts", start_data, end_data)
    data["rx_busy_pkts"] = stat_diff("rx_busy_pkts", start_data, end_data)
    data["tx_ping_pkts"] = stat_diff("tx_ping_pkts", start_data, end_data)
    data["rx_ping_pkts"] = stat_diff("rx_ping_pkts", start_data, end_data)
    data["tx_unknown_pkts"] = stat_diff("tx_unknown_pkts", start_data, end_data)
    data["rx_unknown_pkts"] = stat_diff("rx_unknown_pkts", start_data, end_data)
    data["tx_error_pkts"] = stat_diff("tx_error_pkts", start_data, end_data)
    data["rx_error_pkts"] = stat_diff("rx_error_pkts", start_data, end_data)

    return data

def get_bench_stats(data_dir, server_name):
    start_data_file = data_dir + '/' + server_name + '_bench_stats_0.json'
    end_data_file = data_dir + '/' + server_name + '_bench_stats_1.json'
    with open(start_data_file) as f:
        start_data = json.load(f)
    with open(end_data_file) as f:
        end_data = json.load(f)
    cps = end_data["cycles_per_second"]

    latencies = end_data["client_stats"]["latencies"]
    latencies.sort()
    latency_stats = {}
    latency_stats["count"] = len(latencies) 
    latency_stats["00"] = latencies[0]
    latency_stats["50"] = latencies[int(0.5 * len(latencies))]
    latency_stats["90"] = latencies[int(0.9 * len(latencies))]
    latency_stats["99"] = latencies[int(0.99 * len(latencies))]

    start_task_stats = {task['id']: task['count'] for task in start_data['task_stats']}
    end_task_stats = {task['id']: task['count'] for task in end_data['task_stats']}
    task_stats = {key: end_task_stats[key] - start_task_stats[key] for key in start_task_stats}

    data = {}

    data["elapsed_time"] = stat_diff("timestamp", start_data, end_data) / cps
    data["cycles_per_second"] = cps
    data["client_latency"] = latency_stats
    data["client_count"] = end_data["client_stats"]["count"] - start_data["client_stats"]["count"]
    data["task_stats"] = task_stats

    return data

def print_latency(bench_stats):
    latency_stats = bench_stats["client_latency"]
    latency_min = latency_stats["00"] / 1000.0
    latency_med = latency_stats["50"] / 1000.0
    latency_90 = latency_stats["90"] / 1000.0
    latency_99 = latency_stats["99"] / 1000.0
    print "Client Latency (%9d samples)" % bench_stats["client_latency"]["count"]
    print "----------------------------------"
    print " Med (us)  Min (us)  90% (us)  99% (us) "
    print "%8.3f  %8.3f  %8.3f  %8.3f" % (latency_med, latency_min, latency_90, latency_99)

def print_usage_stats_header():
    print "Usage statistics:"
    print "-----------------"
    print "          " + \
          "   Active (c)" + \
          "   Msg TX (B)" + \
          "   Msg RX (B)" + \
          " Total TX (B)" + \
          " Total RX (B)"

def print_usage_stats(server_name, transport_stats):
    print server_name.rjust(10, ' ') + \
          "%13d" % transport_stats["active_cycles"] + \
          "%13d" % transport_stats["tx_message_bytes"] + \
          "%13d" % transport_stats["rx_message_bytes"] + \
          "%13d" % transport_stats["transport_tx_bytes"] + \
          "%13d" % transport_stats["transport_rx_bytes"]

def print_task_stats_header(bench_stats):
    print "Task statistics:"
    print "----------------"
    header = "         "
    header += "       C"
    for task_id in bench_stats["task_stats"]:
        header += ("T(%d)" % task_id).rjust(8, ' ')
    print header

def print_task_stats(server_name, bench_stats):
    stats_line = server_name.rjust(9, ' ')
    stats_line += "%8d" % bench_stats['client_count'] 
    for task_id in bench_stats["task_stats"]:
        stats_line += "%8d" % bench_stats['task_stats'][task_id] 
    print stats_line

def print_packet_stats_header():
    print "Packet statistics:"
    print "------------------"
    print "                " + \
          "    DATA" + \
          "   GRANT" + \
          "    DONE" + \
          "  RESEND" + \
          "    BUSY" + \
          "    PING" + \
          " UNKNOWN" + \
          "   ERROR"
          
def print_packet_stats(server_name, transport_stats):
    print server_name.rjust(10, ' ') + \
          " (TX) " + \
          "%8d" % transport_stats["tx_data_pkts"] + \
          "%8d" % transport_stats["tx_grant_pkts"] + \
          "%8d" % transport_stats["tx_done_pkts"] + \
          "%8d" % transport_stats["tx_resend_pkts"] + \
          "%8d" % transport_stats["tx_busy_pkts"] + \
          "%8d" % transport_stats["tx_ping_pkts"] + \
          "%8d" % transport_stats["tx_unknown_pkts"] + \
          "%8d" % transport_stats["tx_error_pkts"]
    print "          " + \
          " (RX) " + \
          "%8d" % transport_stats["rx_data_pkts"] + \
          "%8d" % transport_stats["rx_grant_pkts"] + \
          "%8d" % transport_stats["rx_done_pkts"] + \
          "%8d" % transport_stats["rx_resend_pkts"] + \
          "%8d" % transport_stats["rx_busy_pkts"] + \
          "%8d" % transport_stats["rx_ping_pkts"] + \
          "%8d" % transport_stats["rx_unknown_pkts"] + \
          "%8d" % transport_stats["rx_error_pkts"]

def server_id_from_name(server_name):
    return int(server_name[7:])

def main(args):
    server_names =[os.path.basename(file)[:-19] for file in glob.glob(args['<data_dir>'] + '/*_bench_stats_1.json')]
    server_names.sort(key=server_id_from_name)

    transport_stats = {}
    bench_stats = {}

    for server_name in server_names: 
        transport_stats[server_name] = get_transport_stats(args['<data_dir>'], server_name)
        bench_stats[server_name] = get_bench_stats(args['<data_dir>'], server_name)

    print_latency(bench_stats["server-1"])
    print ""
    print_usage_stats_header()
    for server_name in server_names: 
        print_usage_stats(server_name, transport_stats[server_name])
    print ""
    print_packet_stats_header()
    for server_name in server_names: 
        print_packet_stats(server_name, transport_stats[server_name])
    print ""
    print_task_stats_header(bench_stats["server-1"])
    for server_name in server_names: 
        print_task_stats(server_name, bench_stats[server_name])

if __name__ == '__main__':
    args = docopt(__doc__)
    main(args)