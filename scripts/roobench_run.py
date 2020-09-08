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
    roobench.py run <config> <server_config> <bench_config> <log_dir> [--out=<outdir> --pause --verbose]

Options:
    -h, --help          Show this screen.
    -o, --out=<outdir>  Name of the output directory; defaults to a date string.
    -p, --pause         Wait for user before starting clients.
    -v, --verbose       Print out the commands before they are run.
"""

import json
import os
import shutil
import subprocess
import time
from datetime import datetime

def read_config(config):
    BASH_CMD = 'source %s; echo ${HOSTS[@]}; echo $BIN_DIR; echo $SRC_DIR' % config
    p = subprocess.Popen(BASH_CMD, shell=True, stdout=subprocess.PIPE, executable='/bin/bash') 
    lines = p.stdout.readlines()
    hosts = lines[0].strip('\n').split(' ')
    bin_dir = lines[1].strip('\n')
    src_dir = lines[2].strip('\n')
    return (hosts, bin_dir, src_dir)

def wait(tasks):
    for p in tasks:
        p.wait()
    tasks = []

def remote_call(host, cmd):
    # os.system('ssh {host} "{cmd}"'.format(host=host, cmd=cmd))
    p = subprocess.Popen('ssh {host} "{cmd}"'.format(host=host, cmd=cmd), shell=True)
    return p

def setup_host(remote, log_dir_name):
    log_dir = "~/logs/{}".format(log_dir_name)
    p = remote_call(remote, 'mkdir -p {log_dir}; ln -sFfn {log_dir} ~/logs/latest'.format(log_dir=log_dir))
    return p

def coordinator_setup(log_dir, out_name, server_config, bench_config):
    test_dir = "%s/%s/" %(log_dir.rstrip('/'), out_name.rstrip('/'))
    os.makedirs(test_dir)
    shutil.copyfile(server_config, test_dir + '/ServerConfig.json')
    shutil.copyfile(bench_config, test_dir + '/BenchConfig.json')
    return test_dir

def main(args):
    hosts, bin_dir, src_dir = read_config(args['<config>'])
    remote_config_dir = '/shome/RooConfig'
    date_time = datetime.now().strftime('%F-%H-%M-%S')
    with open(os.path.expanduser(args["<bench_config>"])) as f:
        bench_config = json.load(f)
    client_count = bench_config['client_count']
    if (args['--out'] is not None):
        out_name = args['--out']
    else:
        out_name = date_time
    out_dir = coordinator_setup(args['<log_dir>'],
                      out_name,
                      args['<server_config>'],
                      args['<bench_config>'])
    hosts = hosts[:bench_config['node_count']]

    tasks = []

    print "Setup Hosts..."
    ##### Setup hosts
    SERVER_ID=1
    for host in hosts:
        p = setup_host(host, date_time)
        tasks.append(p)
        SERVER_ID += 1
    wait(tasks)
    print "          ... Done."

    ##### Start Servers
    SERVER_ID=1
    print "Start Servers..."
    for host in hosts:
        if SERVER_ID <= client_count:
            server_name = "client-{}".format(SERVER_ID)
        else:
            server_name = "server-{}".format(SERVER_ID - client_count)
        cmd = 'sudo nohup {src_dir}/scripts/roobench.py server launch {server_name} {remote_config_dir}/ServerConfig.json'.format(src_dir=src_dir, server_name=server_name, remote_config_dir=remote_config_dir)
        if args['--verbose']:
            print cmd
        p = remote_call(host, cmd)
        tasks.append(p)
        SERVER_ID += 1
    wait(tasks)
    print "          ... Done."
    
    time.sleep(1)
    
    if args['--pause']:
        raw_input("Press Enter to continue...")

    ##### Run Client
    
    print "Start Client Workload..."
    for host in hosts[:client_count]:
        cmd = 'sudo nohup {src_dir}/scripts/roobench.py server start {remote_config_dir}/ServerConfig.json'.format(src_dir=src_dir, remote_config_dir=remote_config_dir)
        if args['--verbose']:
            print cmd
        p = remote_call(host, cmd)
        tasks.append(p)
    wait(tasks)
    print "          ... Done."
    
    time.sleep(4)
    
    ##### Dump stats
    SERVER_ID=1
    print "Dump stats [begining]..."
    for host in hosts:
        cmd = 'sudo nohup {src_dir}/scripts/roobench.py server stats {remote_config_dir}/ServerConfig.json'.format(src_dir=src_dir, remote_config_dir=remote_config_dir)
        if args['--verbose']:
            print cmd
        p = remote_call(host, cmd)
        tasks.append(p)
        SERVER_ID += 1
    wait(tasks)
    print "          ... Done."
    
    time.sleep(10)

    ##### Dump stats
    SERVER_ID=1
    print "Dump stats [end]..."
    for host in hosts:
        cmd = 'sudo nohup {src_dir}/scripts/roobench.py server stats {remote_config_dir}/ServerConfig.json'.format(src_dir=src_dir, remote_config_dir=remote_config_dir)
        if args['--verbose']:
            print cmd
        p = remote_call(host, cmd)
        tasks.append(p)
        SERVER_ID += 1
    wait(tasks)
    print "          ... Done."
    
    time.sleep(1)
    
    ##### Stop Servers
    SERVER_ID=1
    print "Stop Hosts..."
    for host in hosts:
        cmd = 'sudo {src_dir}/scripts/roobench.py server stop {remote_config_dir}/ServerConfig.json'.format(src_dir=src_dir, remote_config_dir=remote_config_dir)
        if args['--verbose']:
            print cmd
        p = remote_call(host, cmd)
        tasks.append(p)
        SERVER_ID += 1
    wait(tasks)
    print "          ... Done."
    
    ##### Kill Servers
    SERVER_ID=1
    print "Kill Hosts..."
    for host in hosts:
        p = remote_call(host, "sudo pkill -f server")
        if args['--verbose']:
            print cmd
        tasks.append(p)
        SERVER_ID += 1
    wait(tasks)
    print "          ... Done."

    ##### Collect Logs
    SERVER_ID=1
    print "Collect logs..."
    for host in hosts:
        p = subprocess.Popen('scp "{host}:~/logs/{date_time}/*" "{out_dir}"'.format(host=host, date_time=date_time, out_dir=out_dir), shell=True)
        tasks.append(p)
        SERVER_ID=1
    wait(tasks)
    print "          ... Done."


if __name__ == '__main__':
    args = docopt(__doc__)
    main(args)
