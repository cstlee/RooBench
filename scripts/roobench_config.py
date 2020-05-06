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
    roobench.py config bench <server_list> <workload> [--out=<name>]
    roobench.py config server-list <server_config> <hostname>... [--out=<name>]

Options:
    -h, --help          Show this screen.
    -o, --out=<name>    Output to the given file name.
"""

import json
import os
import subprocess

def main(args):
    if args["bench"]:
        with open(args["<server_list>"]) as f:
            server_list = json.load(f)
        with open(args["<workload>"]) as f:
            workload = json.load(f)
        config = {}
        config["server_list"] = server_list
        config["workload"] = workload
        if args["--out"]:
            with open(args["--out"], 'w') as f:
                json.dump(config, f)
        else:
            print(json.dumps(config, indent=4, separators=(',', ': ')))
    elif args["server-list"]:
        with open(os.path.expanduser(args["<server_config>"])) as f:
            config = json.load(f)
        getmac_bin = os.path.expanduser(config["bin_dir"]) + '/getmac'
        server_list = {}
        server_list["servers"] = []
        serverid = 1
        for hostname in args["<hostname>"]:
            p = subprocess.Popen(["ssh",
                                hostname,
                                "sudo %s" % getmac_bin],
                                shell=False,
                                stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE)
            p.wait()
            lines = p.stdout.readlines()
            host_mac = lines[-1].strip('\n')
            entry = {"id": serverid, "address": host_mac}
            server_list["servers"].append(entry)
            serverid += 1
        if args["--out"]:
            with open(args["--out"], 'w') as f:
                json.dump(server_list, f)
        else:
            print(json.dumps(server_list, indent=4, separators=(',', ': ')))



if __name__ == '__main__':
    args = docopt(__doc__)
    main(args)
