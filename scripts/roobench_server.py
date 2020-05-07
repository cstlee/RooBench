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
    roobench.py server launch [options] <server_name> <config_file>
    roobench.py server start [options] <config_file>
    roobench.py server stats [options] <config_file>
    roobench.py server stop [options] <config_file>
    roobench.py server kill [options] <config_file>

Options:
    -h, --help          Show this screen.

Available commands are:
    launch      Setup the benchmark server
    start       Start the benchmark run
    stats       Dump the benchmark statistics
    stop        Stop the benchmark server
    kill        Kill the benchmark server
"""

from docopt import docopt
import subprocess
import json
import os
import signal

def main(args):
    with open(os.path.expanduser(args["<config_file>"])) as f:
        config = json.load(f)
    server_bin = os.path.expanduser(config["bin_dir"]) + '/server'
    num_threads = config["num_threads"]
    bench_config = os.path.expanduser(config["bench_config"])

    output_dir = os.path.expanduser(config["log_dir"])
    server_info_path = output_dir + '/serverinfo.json'
    try:
        with open(server_info_path, 'r') as f:
            server_info = json.load(f)
    except IOError as error:
        pass

    if args['launch']:
        try:
            server_info
        except NameError:
            server_info = {}
        server_name = args["<server_name>"]
        outlog = open(output_dir + '/' + server_name + '.out.log', 'w')
        errlog = open(output_dir + '/' + server_name + '.err.log', 'w')
        p = subprocess.Popen([server_bin,
                              server_name,
                              num_threads,
                              bench_config,
                              output_dir],
                             stdout = outlog,
                             stderr = errlog)
        server_info['pid'] = p.pid
        with open(server_info_path, 'w') as f:
            json.dump(server_info, f)
    elif args['start']:
        try:
            server_info
        except NameError:
            print("No server info available (server not loaded?)")
        else:
            pid = server_info["pid"]
            try:
                os.kill(pid, signal.SIGUSR1)
            except OSError as error:
                print(error)
    elif args['stats']:
        try:
            server_info
        except NameError:
            print("No server info available (server not loaded?)")
        else:
            pid = server_info["pid"]
            try:
                os.kill(pid, signal.SIGUSR2)
            except OSError as error:
                print(error)
    elif args['stop']:
        try:
            server_info
        except NameError:
            print("No server info available (server not loaded?)")
        else:
            pid = server_info["pid"]
            try:
                os.kill(pid, signal.SIGINT)
            except OSError as error:
                print(error)
    elif args['kill']:
        try:
            server_info
        except NameError:
            print("No server info available (server not loaded?)")
        else:
            pid = server_info["pid"]
            try:
                os.kill(pid, signal.SIGTERM)
            except OSError as error:
                print(error)

if __name__ == '__main__':
    args = docopt(__doc__)
    main(args)

