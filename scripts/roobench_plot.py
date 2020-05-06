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
    roobench.py plot latency <dataset_name> <data_dir>... [--out=<name>] [--dataout=<name>]
    roobench.py plot cpu-usage <dataset_name> <num_servers> <data_dir>...
    roobench.py plot merge <data_file>...

Options:
    -h, --help              Show this screen.
    -o, --out=<name>        Output to the given file name.
    -d, --dataout=<name>    Output data to a data file.
"""

import json
import matplotlib.pyplot as plt

def load_latency(data_dir):
    data_file = data_dir + '/server-1_bench_stats_1.json'
    with open(data_file) as f:
        data = json.load(f)
    return data["client_stats"]["latencies"]

def load_server_cpu_usage(data_dir, server_id):
    start_data_file = data_dir + '/server-%d_transport_stats_0.json' % server_id
    end_data_file = data_dir + '/server-%d_transport_stats_1.json' %server_id
    with open(start_data_file) as f:
        start_data = json.load(f)
    with open(end_data_file) as f:
        end_data = json.load(f)
    cpu_usage = end_data["active_cycles"] - start_data["active_cycles"]
    return cpu_usage

def load_cpu_usage(data_dir, num_servers):
    client_usage = load_server_cpu_usage(data_dir, 1)
    server_usage = 0
    for i in xrange(2,num_servers+1,1):
        server_usage += load_server_cpu_usage(data_dir, i)
    return (client_usage, server_usage)

def get_median(data):
    data.sort()
    return data[len(data)/2]

def generate_latency_plot(x, y, data_name):
    plt.plot(x, y, '-o', label=data_name)
    plt.ylabel('End-to-end latency (us)')

def plot_latency(args):
    y = []
    x = []
    hops = 1
    for data_dir in args["<data_dir>"]:
        latencies = load_latency(data_dir)
        median = get_median(latencies)
        median = median / 1000.0
        y.append(median)
        x.append(hops)
        hops += 1
    if args['--dataout']:
        data = {}
        data["y"] = y
        data["type"] = "latency"
        data["x"] = x
        data["name"] = args['<dataset_name>']
        with open(args['--dataout'], 'w') as f:
            json.dump(data, f)
    else:
        generate_latency_plot(x, y, args['<dataset_name>'])
        plt.legend()
        plt.ylim(bottom=0)
        plt.show()

def plot_cpu_usage(args):
    for data_dir in args["<data_dir>"]:
        start_client_data_file = data_dir + '/server-1_bench_stats_0.json'
        end_client_data_file = data_dir + '/server-1_bench_stats_1.json'
        with open(start_client_data_file) as f:
            start_client_data = json.load(f)
        with open(end_client_data_file) as f:
            end_client_data = json.load(f)
        client_count = end_client_data["client_stats"]["count"] - start_client_data["client_stats"]["count"]
        
        (client_cycles, server_cycles) = load_cpu_usage(data_dir, int(args['<num_servers>']))
        client_cycles = client_cycles / client_count
        server_cycles = server_cycles / client_count
        print "C: %d S: %d T: %d" % (client_cycles, server_cycles, client_cycles + server_cycles)

def plot_merge(args):
    data_type = None
    for data_file in args['<data_file>']:
        with open(data_file) as f:
            data = json.load(f)
        if data_type is None:
            data_type = data["type"]
        elif data_type != data["type"]:
            print "Data types do not match"
            return
        if data["type"] == "latency":
            generate_latency_plot(data['x'], data['y'], data['name'])
        else:
            print "Unknown type %s" % data["type"]
    plt.legend()
    plt.ylim(bottom=0)
    plt.show()
    

def main(args):
    if args["latency"]:
        plot_latency(args)
    elif args["cpu-usage"]:
        plot_cpu_usage(args)
    elif args["merge"]:
        plot_merge(args)

if __name__ == '__main__':
    args = docopt(__doc__)
    main(args)
