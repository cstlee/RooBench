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
    roobench.py run <config> <server_config> <bench_config> <log_dir>

Options:
    -h, --help          Show this screen.
"""

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

def remote_call(host, cmd):
    os.system('ssh {host} "{cmd}"'.format(host=host, cmd=cmd))

def setup_host(remote, log_dir_name):
    log_dir = "~/logs/{}".format(log_dir_name)
    remote_call(remote, 'mkdir -p {log_dir}; ln -sFfn {log_dir} ~/logs/latest'.format(log_dir=log_dir))

def coordinator_setup(log_dir, date_time, server_config, bench_config):
    test_dir = "%s/%s" %(log_dir, date_time)
    os.makedirs(test_dir)
    shutil.copyfile(server_config, test_dir + '/ServerConfig.json')
    shutil.copyfile(bench_config, test_dir + '/BenchConfig.json')

def main(args):
    hosts, bin_dir, src_dir = read_config(args['<config>'])
    remote_config_dir = '/shome/RooConfig'
    date_time = datetime.now().strftime('%F-%H-%M-%S')
    coordinator_setup(args['<log_dir>'],
                      date_time,
                      args['<server_config>'],
                      args['<bench_config>'])
    
    ##### Setup hosts
    SERVER_ID=1
    for host in hosts:
        print "Setup Server %d on %s" %(SERVER_ID, host)
        setup_host(host, date_time)
        SERVER_ID += 1

    ##### Start Servers
    SERVER_ID=1
    for host in hosts:
        print "Start Server %d on %s" %(SERVER_ID, host)
        server_name = "server-{}".format(SERVER_ID)
        cmd = 'sudo nohup {src_dir}/scripts/roobench.py server launch {server_name} {remote_config_dir}/ServerConfig.json'.format(src_dir=src_dir, server_name=server_name, remote_config_dir=remote_config_dir)
        remote_call(host, cmd)
        SERVER_ID += 1
    
    time.sleep(0.5)

    ##### Run Client
    
    for host in hosts[:1]:
        print "Start Client on {}".format(host)
        cmd = 'sudo nohup {src_dir}/scripts/roobench.py server start {remote_config_dir}/ServerConfig.json'.format(src_dir=src_dir, remote_config_dir=remote_config_dir)
        remote_call(host, cmd)
    
    time.sleep(1)
    
    ##### Dump stats
    SERVER_ID=1
    for host in hosts:
        print "Dump Server %d stats on %s" %(SERVER_ID, host)
        server_name = "server-{}".format(SERVER_ID)
        cmd = 'sudo nohup {src_dir}/scripts/roobench.py server stats {remote_config_dir}/ServerConfig.json'.format(src_dir=src_dir, remote_config_dir=remote_config_dir)
        remote_call(host, cmd)
        SERVER_ID += 1
    
    time.sleep(30)

    ##### Dump stats
    SERVER_ID=1
    for host in hosts:
        print "Dump Server %d stats on %s" %(SERVER_ID, host)
        server_name = "server-{}".format(SERVER_ID)
        cmd = 'sudo nohup {src_dir}/scripts/roobench.py server stats {remote_config_dir}/ServerConfig.json'.format(src_dir=src_dir, remote_config_dir=remote_config_dir)
        remote_call(host, cmd)
        SERVER_ID += 1
    
    time.sleep(1)
    
    ##### Stop Servers
    SERVER_ID=1
    for host in hosts:
        print "Stop Server %d on %s" %(SERVER_ID, host)
        cmd = 'sudo {src_dir}/scripts/roobench.py server stop {remote_config_dir}/ServerConfig.json'.format(src_dir=src_dir, remote_config_dir=remote_config_dir)
        remote_call(host, cmd)
        SERVER_ID += 1
    
    ##### Kill Servers
    SERVER_ID=1
    for host in hosts:
        print "Kill Server %d on %s" %(SERVER_ID, host)
        remote_call(host, "sudo pkill -f server")
        SERVER_ID += 1

    ##### Collect Logs
    SERVER_ID=1
    for host in hosts:
        print "Copying Server %d logs from %s" %(SERVER_ID, host)
        os.system('scp "{host}:~/logs/{date_time}/*" "{log_dir}/{date_time}"'.format(host=host, date_time=date_time, log_dir=args['<log_dir>']))


if __name__ == '__main__':
    args = docopt(__doc__)
    main(args)
