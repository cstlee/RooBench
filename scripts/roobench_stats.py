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
    -s, --summary           Output a stats summary
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
    data["api_cycles"] = stat_diff("api_cycles", start_data, end_data)
    data["active_cycles"] = stat_diff("active_cycles", start_data, end_data)
    data["idle_cycles"] = stat_diff("idle_cycles", start_data, end_data)
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

    latencies = []
    if end_data["client_stats"]["count"] > len(end_data["client_stats"]["latencies"]):
        end_count = end_data["client_stats"]["count"]
        start_count = start_data["client_stats"]["count"]
        max_count = len(end_data["client_stats"]["latencies"])
        if end_count - start_count >= max_count:
            latencies = end_data["client_stats"]["latencies"]
        else:
            end_index = end_count % max_count
            start_index = start_count % max_count 
            if start_index <= end_index:
                latencies = end_data["client_stats"]["latencies"][start_index:end_index]
            else:
                latencies = end_data["client_stats"]["latencies"][start_index:] + end_data["client_stats"]["latencies"][:end_index]
    else:
        warmup_count = len(start_data["client_stats"]["latencies"])
        latencies = end_data["client_stats"]["latencies"][warmup_count:]

    start_task_stats = {task['id']: task['count'] for task in start_data['task_stats']}
    end_task_stats = {task['id']: task['count'] for task in end_data['task_stats']}
    task_stats = {key: end_task_stats[key] - start_task_stats[key] for key in start_task_stats}

    data = {}

    data["elapsed_time"] = stat_diff("timestamp", start_data, end_data) / cps
    data["cycles_per_second"] = cps
    data["active_cycles"] = stat_diff("active_cycles", start_data, end_data)
    data["client_latencies"] = latencies
    data["client_count"] = end_data["client_stats"]["count"] - start_data["client_stats"]["count"]
    data["client_failures"] = end_data["client_stats"]["failures"] - start_data["client_stats"]["failures"]
    data["task_stats"] = task_stats

    return data

def get_latencies(client_names, bench_stats):
    latencies = []
    for name in client_names:
        latencies += bench_stats[name]["client_latencies"]
    latencies.sort()
    return latencies

def print_summary(client_names, server_names, bench_stats, transport_stats):
    client_count = 0
    client_failures = 0
    throughput = 0.0
    cpu_util_bench = 0.0
    cpu_util_fg = 0.0
    cpu_util_bg = 0.0
    for name in client_names + server_names:
        client_count += bench_stats[name]["client_count"]
        client_failures += bench_stats[name]["client_failures"]
        throughput += (bench_stats[name]["client_count"] / bench_stats[name]["elapsed_time"]) / 1000.0
        _cps = bench_stats[name]["cycles_per_second"]
        _duration = bench_stats[name]["elapsed_time"]
        cpu_util_bench += bench_stats[name]["active_cycles"] / (_cps * _duration)
        _cps = transport_stats[name]["cycles_per_second"]
        _duration = transport_stats[name]["elapsed_time"]
        cpu_util_fg += transport_stats[name]["api_cycles"] / (_cps * _duration)
        cpu_util_bg += transport_stats[name]["active_cycles"] / (_cps * _duration)
    latency = 0
    latencies = get_latencies(client_names, bench_stats)
    if len(latencies) > 0:
        latency = latencies[int(0.5 * len(latencies))] / 1000.0

    print "Summary Statistics"
    print "------------------"
    print "Num Completed: %8d" % client_count
    print "   Num Failed: %8d" % client_failures
    print "Latency [med]: %8.3f us" % latency
    print "   Throughput: %8.3f kops" % throughput
    print "     CPU Util: %8.3f cores [%6.2f / %6.2f / %6.2f](bench, api, poll)" % (cpu_util_bench + cpu_util_bg, cpu_util_bench - cpu_util_fg, cpu_util_fg, cpu_util_bg)
    pass

def print_latency(bench_stats, client_names):
    latencies = get_latencies(client_names, bench_stats)
    count = len(latencies)
    print "Client Latency (%9d samples)" % count
    print "------------------------------------------------------------"
    if (count < 1):
        print "No data"
    else:
        latency_min = latencies[0] / 1000.0
        latency_25 = latencies[int(0.25 * len(latencies))] / 1000.0
        latency_med = latencies[int(0.5 * len(latencies))] / 1000.0
        latency_75 = latencies[int(0.75 * len(latencies))] / 1000.0
        latency_90 = latencies[int(0.9 * len(latencies))] / 1000.0
        latency_99 = latencies[int(0.99 * len(latencies))] / 1000.0
        print " Med (us)  Min (us)  25% (us)  75% (us)  90% (us)  99% (us) "
        print "%8.3f  %8.3f  %8.3f  %8.3f  %8.3f  %8.3f" % (latency_med, latency_min, latency_25, latency_75, latency_90, latency_99)

def print_net_usage(client_names, server_names, bench_stats, transport_stats):
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

    def print_net_entry(host_name, count):
        duration = transport_stats[host_name]['elapsed_time']
        tx_msg = transport_stats[host_name]['tx_message_bytes']
        rx_msg = transport_stats[host_name]['rx_message_bytes']
        tx_tran = transport_stats[host_name]['transport_tx_bytes']
        rx_tran = transport_stats[host_name]['transport_rx_bytes']
        app_iter = " %6d %6d" % (np.divide(tx_msg, count), np.divide(rx_msg, count))
        tran_iter = " %6d %6d" % (np.divide(tx_tran, count), np.divide(rx_tran, count))
        app_Tput = " %6.1f %6.1f" % (8 * tx_msg / (1000000.0 * duration), 8 * rx_msg / (1000000.0 * duration))
        tran_Tput = " %6.1f %6.1f" % (8 * tx_tran / (1000000.0 * duration), 8 * rx_tran / (1000000.0 * duration))
        app_Tot = " %6.1f %6.1f" % (tx_msg / (1000000.0), rx_msg / (1000000.0))
        tran_Tot = " %6.1f %6.1f" % (tx_tran / (1000000.0), rx_tran / (1000000.0))
        print host_name.rjust(10, ' ') + app_iter + app_Tput + app_Tot + tran_iter + tran_Tput + tran_Tot

    total_count = 0

    for client_name in client_names:
        count = bench_stats[client_name]['client_count']
        print_net_entry(client_name, count)
        total_count += count
    for server_name in server_names:
        print_net_entry(server_name, total_count)


def print_cpu_usage_stats(client_names, server_names, bench_stats, transport_stats):
    print "CPU Usage Statistics:"
    print "---------------------"
    h1 = "                     Active Cycles                      "
    h1_ = " -------------------------------------------------------"
    h2 = "   per iter   " + \
         "   per iter   " + \
         "   CPU Load   " + \
         "     Total    "
    h2_2 = "   (cycles)   " + \
          "     (us)     " + \
          "   (1 core)   " + \
          "   (cycles)   "
    h2_ = " -------------" + \
          " -------------" + \
          " -------------" + \
          " -------------"
    s = '          '
    print s + h1
    print s + h1_
    print s + h2
    print s + h2_2
    print s + h2_
    total_fg_cpu = 0.0
    total_bg_cpu = 0.0
    total_iter_per_second = 0.0

    def print_cpu_entry(host_name, iter_per_second):
        cps = transport_stats[host_name]['cycles_per_second']
        duration = transport_stats[host_name]['elapsed_time']
        cycles = transport_stats[host_name]['api_cycles'] + transport_stats[host_name]['active_cycles']
        active_cycles_per_second = np.divide(cycles, duration);
        active_cycles_per_iter = np.divide(active_cycles_per_second, iter_per_second)
        print host_name.rjust(10, ' ') + \
            " %12d " % active_cycles_per_iter + \
            " %12.3f " % np.divide(1000000 * active_cycles_per_iter, cps) + \
            " %12.3f " % np.divide(active_cycles_per_second, cps) + \
            " %12d " % (cycles)
        cycles = transport_stats[host_name]['api_cycles']
        active_cycles_per_second = np.divide(cycles, duration);
        active_cycles_per_iter = np.divide(active_cycles_per_second, iter_per_second)
        fg_cpu = np.divide(active_cycles_per_second, cps)
        print "(fg)".rjust(10, ' ') + \
            " %12d " % active_cycles_per_iter + \
            " %12.3f " % np.divide(1000000 * active_cycles_per_iter, cps) + \
            " %12.3f " % np.divide(active_cycles_per_second, cps) + \
            " %12d " % (cycles)
        cycles = transport_stats[host_name]['active_cycles']
        active_cycles_per_second = np.divide(cycles, duration);
        active_cycles_per_iter = np.divide(active_cycles_per_second, iter_per_second)
        bg_cpu = np.divide(active_cycles_per_second, cps)
        print "(bg)".rjust(10, ' ') + \
            " %12d " % active_cycles_per_iter + \
            " %12.3f " % np.divide(1000000 * active_cycles_per_iter, cps) + \
            " %12.3f " % np.divide(active_cycles_per_second, cps) + \
            " %12d " % (cycles)
        return (fg_cpu, bg_cpu)

    for client_name in client_names:
        count = float(bench_stats[client_name]['client_count']) 
        duration = float(bench_stats[client_name]['elapsed_time'])
        iter_per_second = (count / duration)
        total_iter_per_second += iter_per_second
        fg, bg = print_cpu_entry(client_name, iter_per_second)
        total_fg_cpu += fg
        total_bg_cpu += bg
    for server_name in server_names:
        fg, bg = print_cpu_entry(server_name, total_iter_per_second)
        total_fg_cpu += fg
        total_bg_cpu += bg

    print s + h2_
    active_cpu = total_fg_cpu + total_bg_cpu
    print "Total".rjust(10, ' ') + \
        "      --      " + \
        " %12.3f " % np.divide(1000000 * active_cpu, total_iter_per_second) + \
        " %12.3f " % active_cpu + \
        "      --      "
    active_cpu = total_fg_cpu
    print "(fg)".rjust(10, ' ') + \
        "      --      " + \
        " %12.3f " % np.divide(1000000 * active_cpu, total_iter_per_second) + \
        " %12.3f " % active_cpu + \
        "      --      "
    active_cpu = total_bg_cpu
    print "(bg)".rjust(10, ' ') + \
        "      --      " + \
        " %12.3f " % np.divide(1000000 * active_cpu, total_iter_per_second) + \
        " %12.3f " % active_cpu + \
        "      --      "

def print_task_stats(host_names, bench_stats):
    print "Task statistics:"
    print "----------------"
    header = "         "
    header_ = header
    header += "       C"
    header_ += " -------"
    total = [0.0,]
    for task_id in bench_stats[host_names[0]]["task_stats"]:
        header += ("T(%d)" % task_id).rjust(8, ' ')
        header_ += " -------"
        total.append(0.0)
    total = np.array(total) 
    print header
    print header_
    for host_name in host_names: 
        stats_line = host_name.rjust(9, ' ')
        client_count = bench_stats[host_name]['client_count']
        stats_line += " %7d" % client_count
        server_stat = [client_count, ] 
        for task_id in bench_stats[host_name]["task_stats"]:
            task_count = bench_stats[host_name]['task_stats'][task_id]
            stats_line += " %7d" % task_count 
            server_stat.append(task_count)
        total += np.array(server_stat)
        print stats_line
    print header_
    norm = np.rint(np.divide(total, total[0]))
    stats_line = "    Total"
    for stat in total:
        stats_line += " %7d" % stat 
    print stats_line
    stats_line = "Normalize"
    for stat in norm:
        stats_line += " %7d" % stat 
    print stats_line

def print_packet_stats(client_names, server_names, bench_stats, transport_stats):
    count = 0
    for client_name in client_names:
        count += bench_stats[client_name]['client_count']
    print "Packet statistics:"
    print "------------------"
    print "                " + \
          "     DATA" + \
          "    GRANT" + \
          "     DONE" + \
          "   RESEND" + \
          "     BUSY" + \
          "     PING" + \
          "  UNKNOWN" + \
          "    ERROR" + \
          " |" + \
          "    TOTAL"
    hline = "                " + \
            " --------" + \
            " --------" + \
            " --------" + \
            " --------" + \
            " --------" + \
            " --------" + \
            " --------" + \
            " --------" + \
            "  " + \
            " --------"
    print hline
    pkt_types = ("data",
                 "grant",
                 "done",
                 "resend",
                 "busy",
                 "ping",
                 "unknown",
                 "error")
    tx_totals = np.zeros(len(pkt_types))
    rx_totals = np.zeros(len(pkt_types))
    for host_name in client_names + server_names:
        stats = transport_stats[host_name]
        tx_data = []
        rx_data = []
        for pkt_type in pkt_types:
            tx_data.append(stats['tx_' + pkt_type + '_pkts'])
            rx_data.append(stats['rx_' + pkt_type + '_pkts'])
        tx_data = np.array(tx_data)
        rx_data = np.array(rx_data)
        tx_totals += tx_data
        rx_totals += rx_data
        print host_name.rjust(10, ' ') + \
            " (TX)  %8d %8d %8d %8d %8d %8d %8d %8d | %8d" % tuple(np.append(tx_data, np.sum(tx_data))) 
        print "          " + \
            " (RX)  %8d %8d %8d %8d %8d %8d %8d %8d | %8d" % tuple(np.append(rx_data, np.sum(rx_data))) 
    print hline
    print 'Totals'.rjust(10, ' ') + \
        " (TX)  %8d %8d %8d %8d %8d %8d %8d %8d | %8d" % tuple(np.append(tx_totals, np.sum(tx_totals))) 
    print "          " + \
        " (RX)  %8d %8d %8d %8d %8d %8d %8d %8d | %8d" % tuple(np.append(rx_totals, np.sum(rx_totals))) 
    print 'Normalize'.rjust(10, ' ') + \
        " (TX)  %8d %8d %8d %8d %8d %8d %8d %8d | %8d" % tuple(np.rint(np.append(tx_totals / count, np.sum(tx_totals) / count))) 
    print "          " + \
        " (RX)  %8d %8d %8d %8d %8d %8d %8d %8d | %8d" % tuple(np.rint(np.append(rx_totals / count, np.sum(rx_totals)/ count))) 

def id_from_name(host_name):
    return int(host_name[7:])

def main(args):
    client_names =[os.path.basename(file)[:-19] for file in glob.glob(args['<data_dir>'] + '/client*_bench_stats_1.json')]
    client_names.sort(key=id_from_name)
    server_names =[os.path.basename(file)[:-19] for file in glob.glob(args['<data_dir>'] + '/server*_bench_stats_1.json')]
    server_names.sort(key=id_from_name)
    host_names = client_names + server_names

    transport_stats = {}
    bench_stats = {}

    for host_name in host_names: 
        transport_stats[host_name] = get_transport_stats(args['<data_dir>'], host_name)
        bench_stats[host_name] = get_bench_stats(args['<data_dir>'], host_name)

    flags_set = 0
    for flag in ('--cpu', '--latency', '--network', '--packet', '--task', '--summary'):
        if args[flag]:
            flags_set += 1
    if flags_set > 0:
        print_all = False
    else:
        print_all = True

    if (print_all or args['--summary']):
        print_summary(client_names, server_names, bench_stats, transport_stats)
        print ""

    if (print_all or args['--latency']):
        print_latency(bench_stats, client_names)
        print ""
    
    if (print_all or args['--cpu']):
        print_cpu_usage_stats(client_names, server_names, bench_stats, transport_stats)
        print ""
    
    if (print_all or args['--network']):
        print_net_usage(client_names, server_names, bench_stats, transport_stats)
        print ""
    if (print_all or args['--packet']):
        print_packet_stats(client_names, server_names, bench_stats, transport_stats)
        print ""
    
    if (print_all or args['--task']):
        print_task_stats(host_names, bench_stats)
        print ""

if __name__ == '__main__':
    args = docopt(__doc__)
    main(args)