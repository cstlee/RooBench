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
    roobench.py stats [options] <data_dir>

Options:
    -h, --help              Show this screen.
    -l, --latency           Output Latency Stats
    -n, --network           Output Network Usage Stats
    -c, --cpu               Output CPU Usage Stats
    -p, --packet            Output Packet Stats
    -t, --task              Output Task Stats
"""

import glob
import os
import json
import numpy as np

np.seterr(divide = 'ignore') 

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

def print_net_usage(server_names, bench_stats, transport_stats):
    count = bench_stats['server-1']['client_count']
    print "Network Usage Statistics:"
    print "-------------------------"
    h1 = "               Application                " + \
         "                Transport                 "
    h1_ = " -----------------------------------------"
    h2 = "   per iter   " + \
         "     Tput     " + \
         "     Total    "
    h2_2 = "    (Byte)    " + \
          "    (Mbps)    " + \
          "     (MB)     "
    h2_ = " -------------" + \
          " -------------" + \
          " -------------"
    h3 = '   TX     RX  '
    s = '          '
    print s + h1
    print s + h1_ + h1_
    print s + h2 + h2
    print s + h2_2 + h2_2
    print s + h2_ + h2_
    print s + h3 + h3 + h3 + h3 + h3 + h3
    for server_name in server_names:
        duration = transport_stats[server_name]['elapsed_time']
        tx_msg = transport_stats[server_name]['tx_message_bytes']
        rx_msg = transport_stats[server_name]['rx_message_bytes']
        tx_tran = transport_stats[server_name]['transport_tx_bytes']
        rx_tran = transport_stats[server_name]['transport_rx_bytes']
        app_iter = " %6d %6d" % (np.divide(tx_msg, count), np.divide(rx_msg, count))
        tran_iter = " %6d %6d" % (np.divide(tx_tran, count), np.divide(rx_tran, count))
        app_Tput = " %6.1f %6.1f" % (8 * tx_msg / (1000000.0 * duration), 8 * rx_msg / (1000000.0 * duration))
        tran_Tput = " %6.1f %6.1f" % (8 * tx_tran / (1000000.0 * duration), 8 * rx_tran / (1000000.0 * duration))
        app_Tot = " %6.1f %6.1f" % (tx_msg / (1000000.0), rx_msg / (1000000.0))
        tran_Tot = " %6.1f %6.1f" % (tx_tran / (1000000.0), rx_tran / (1000000.0))
        print server_name.rjust(10, ' ') + app_iter + app_Tput + app_Tot + tran_iter + tran_Tput + tran_Tot

def print_cpu_usage_stats(server_names, bench_stats, transport_stats):
    count = bench_stats['server-1']['client_count']
    print "CPU Usage Statistics:"
    print "---------------------"
    h1 = "              Active Cycles               "
    h1_ = " -----------------------------------------"
    h2 = "   per iter   " + \
         "   CPU Load   " + \
         "     Total    "
    h2_2 = "   (cycles)   " + \
          "   (1 core)   " + \
          "   (cycles)   "
    h2_ = " -------------" + \
          " -------------" + \
          " -------------"
    s = '          '
    print s + h1
    print s + h1_
    print s + h2
    print s + h2_2
    print s + h2_
    for server_name in server_names:
        cycles = transport_stats[server_name]['active_cycles']
        cps = transport_stats[server_name]['cycles_per_second']
        duration = transport_stats[server_name]['elapsed_time']
        print server_name.rjust(10, ' ') + \
            " %12d " % np.divide(cycles, count) + \
            " %12.3f " % np.divide(cycles, cps * duration) + \
            " %12d " % (cycles)

def print_task_stats(server_names, bench_stats):
    print "Task statistics:"
    print "----------------"
    header = "         "
    header_ = header
    header += "       C"
    header_ += " -------"
    total = [0,]
    for task_id in bench_stats['server-1']["task_stats"]:
        header += ("T(%d)" % task_id).rjust(8, ' ')
        header_ += " -------"
        total.append(0)
    total = np.array(total) 
    print header
    print header_
    for server_name in server_names: 
        stats_line = server_name.rjust(9, ' ')
        client_count = bench_stats[server_name]['client_count']
        stats_line += " %7d" % client_count
        server_stat = [client_count, ] 
        for task_id in bench_stats[server_name]["task_stats"]:
            task_count = bench_stats[server_name]['task_stats'][task_id]
            stats_line += " %7d" % task_count 
            server_stat.append(task_count)
        total += np.array(server_stat)
        print stats_line
    print header_
    norm = np.divide(total, total[0])
    stats_line = "    Total"
    for stat in total:
        stats_line += " %7d" % stat 
    print stats_line
    stats_line = "Normalize"
    for stat in norm:
        stats_line += " %7d" % stat 
    print stats_line

def print_packet_stats(server_names, transport_stats):
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
    hline = "                " + \
            " -------" + \
            " -------" + \
            " -------" + \
            " -------" + \
            " -------" + \
            " -------" + \
            " -------" + \
            " -------"
    print hline
    pkt_types = ("data",
                 "grant",
                 "done",
                 "resend",
                 "busy",
                 "ping",
                 "unknown",
                 "error")
    totals = np.zeros(2 * len(pkt_types))
    for server_name in server_names:
        stats = transport_stats[server_name]
        data = []
        for direction in ("tx", "rx"):
            for pkt_type in pkt_types:
                key = direction + '_' + pkt_type + '_pkts'
                data.append(stats[key])
        data = np.array(data)
        totals += data
        print server_name.rjust(10, ' ') + \
            " (TX)  %7d %7d %7d %7d %7d %7d %7d %7d" % tuple(data[0:len(pkt_types)]) 
        print "          " + \
            " (RX)  %7d %7d %7d %7d %7d %7d %7d %7d" % tuple(data[len(pkt_types):]) 
    print hline
    print 'Totals'.rjust(10, ' ') + \
        " (TX)  %7d %7d %7d %7d %7d %7d %7d %7d" % tuple(totals[0:len(pkt_types)]) 
    print "          " + \
        " (RX)  %7d %7d %7d %7d %7d %7d %7d %7d" % tuple(totals[len(pkt_types):]) 

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

    flags_set = 0
    for flag in ('--cpu', '--latency', '--network', '--packet', '--task'):
        if args[flag]:
            flags_set += 1
    if flags_set > 0:
        print_all = False
    else:
        print_all = True

    if (print_all or args['--latency']):
        print_latency(bench_stats["server-1"])
        print ""
    
    if (print_all or args['--cpu']):
        print_cpu_usage_stats(server_names, bench_stats, transport_stats)
        print ""
    
    if (print_all or args['--network']):
        print_net_usage(server_names, bench_stats, transport_stats)
        print ""
    if (print_all or args['--packet']):
        print_packet_stats(server_names, transport_stats)
        print ""
    
    if (print_all or args['--task']):
        print_task_stats(server_names, bench_stats)
        print ""

if __name__ == '__main__':
    args = docopt(__doc__)
    main(args)