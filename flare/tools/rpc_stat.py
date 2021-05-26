#!/usr/bin/env python3

# Copyright (C) 2020 THL A29 Limited, a Tencent company. All rights reserved.
#
# Licensed under the BSD 3-Clause License (the "License"); you may not use this
# file except in compliance with the License. You may obtain a copy of the
# License at
#
# https://opensource.org/licenses/BSD-3-Clause
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

# This script periodically pulls RPC statistics from the given server and dump
# it in a more readable way.
#
# Adapted from `marvel/tools/rpc_stat`.

import os
import http.client
import json
import time
import sys
import getopt


def usage():
    ''' Display program usage. '''
    progname = os.path.split(sys.argv[0])[1]
    if os.path.splitext(progname)[1] in ['.py', '.pyc']:
        progname = 'python ' + progname
    return 'Usage: %s [--host=...] <--port=...> [--method=...]' % progname


def get_rpc_stat(host, port):
    conn = http.client.HTTPConnection(host, port)
    conn.request('GET', '/inspect/rpc_stats')
    data = conn.getresponse().read()
    conn.close()
    return json.loads(data.decode('utf-8'))


def dump_stat_periodically(host, port, method):
    i = 0
    while i < 60:
        if i % 15 == 0:
            detail = '|reqs succ fail min/max/avg(ms)| in each cloumn'
            print('stating %s:%s:%s\n%s' % (host, port, method, detail))
            print('%s%s%s%s' %
                  ('|--in last second----|', '---in last minute----|',
                   '-------in last hour-------|',
                   '-----------since start--------|'))
        data = get_rpc_stat(host, port)[method]
        ct_total = data['counter']['total']
        ct_succ = data['counter']['success']
        ct_fail = data['counter']['failure']
        line = ''
        for k in ['last_second', 'last_minute', 'last_hour', 'total']:
            latency = data['latency'][k]
            line += '%4s %4s %4s %s/%s/%s' % (
                ct_total[k], ct_succ[k], ct_fail[k], latency['min'],
                latency['max'], latency['average']) + '|'
        line = line[:-1]
        print(line)
        time.sleep(1)
        i = i + 1



def main():
    opts, args = getopt.getopt(sys.argv[1:], '',
                               ['host=', 'port=', 'method=', 'help'])
    if '--help' in opts:
        print(usage())
        sys.exit(0)

    host = '127.0.0.1'
    port = 0
    method = 'global'
    for o, a in opts:
        if o in '--host':
            host = a
        elif o in '--port':
            port = a
        elif o in '--method':
            method = a

    if port == 0:
        print(usage())
        sys.exit(-1)

    dump_stat_periodically(host, port, method)


if __name__ == '__main__':
    main()
