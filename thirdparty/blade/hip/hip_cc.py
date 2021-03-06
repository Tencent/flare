# Copyright (c) 2016 Tencent Inc.
# All rights reserved.
#
# Author: Li Wenting <wentingli@tencent.com>
# Date:   August 11, 2016

"""
This file is a thin wrapper around the underlying hip cc executable and
parses the gcc compilation command using argparse.
The command line generated by blade is divided into the following information:

    1. Input source file
    2. Output object file specified by -o
    3. Include directories used by searching for headers
    4. Compilation options

"""

from __future__ import print_function

import os
import re
import subprocess
import sys
import argparse
import errno


def hip(**kwargs):
    return kwargs


def _get_hip_info(dir):
    """Search for HIP file upwards starting from dir. """
    while dir:
        f = os.path.join(dir, 'HIP')
        if os.path.isfile(f):
            return eval(open(f).read(), globals(), None)
        dir = os.path.dirname(dir)

    return { }


def parse():
    # Local compiler may be prefixed by ccache/distcc
    for i, arg in enumerate(sys.argv):
        if arg.startswith('-'):
            break
    compiler = sys.argv[i - 1]
    args = sys.argv[i:]

    parser = argparse.ArgumentParser(prog = 'hipcc')
    parser.add_argument('input',
                        help = 'the input source file')
    parser.add_argument('-o', dest = 'output',
                        help = 'the output object file')
    parser.add_argument('-I', dest = 'include_directories', action = 'append',
                        help = 'include directories used for header search')
    parser.add_argument('-MMD', dest = 'MMD', action='store_true', default=False,
                        help = 'The gcc -MMD')
    parser.add_argument('-MF', dest = 'MF', type=str,
                        help = 'the output dependency file')
    parser.add_argument('-H', dest = 'H', action='store_true', default=False,
                        help = 'output inclusion stack')
    compile_info, options = parser.parse_known_args(args)

    source = compile_info.input
    target = compile_info.output
    # Here we determine whether to invoke hipcc according
    # to the path of source:
    #
    #     1. If source is located in the source directory, then
    #        compile directly with the compiler and args passed in
    #
    #     2. If source is located in the build directory, which
    #        means that it's a placeholder generated by blade
    parts = os.path.normpath(target).split('/')
    if not source.startswith(parts[0]):
        return False, sys.argv[1:]

    hip_source = source
    parts = os.path.normpath(source).split('/')
    source = '/'.join(parts[1:])
    # Search for HIP in source directory
    hip_info = _get_hip_info(os.path.dirname(source))
    version = hip_info.get('version')
    if not version:
        print(source, 'version not found')
        sys.exit(1)
    # For now ignore absolute directories
    include_directories = ':'.join([d for d in compile_info.include_directories
                                    if not os.path.isabs(d)])
    options = ' '.join(options)
    return (True,
            (compile_info, source, hip_source, version, target, include_directories, options, compiler))


def makedirs(dir):
    """Make directory tree, ignore existance errors."""
    try:
        if not os.path.isdir(dir):
            os.makedirs(dir)
    except OSError as e:
        if e.errno != errno.EEXIST:
            raise


def remove_file(filename):
    try:
        os.remove(filename)
    except OSError:
        pass


def compile(compile_info,
            source,
            hip_source,
            version,
            target,
            include_directories,
            options,
            local_compiler):
    hipcc = os.path.join(os.path.dirname(__file__), 'hipcc')
    hipcc_conf = os.path.join(os.path.dirname(__file__), 'hip_cc.conf')
    log_dir = os.path.join('blade-bin', 'hipcc')
    makedirs(log_dir)

    # The hipcc executable need this directory to obtain complete inclusion information.
    makedirs(os.path.dirname(hip_source))

    output = subprocess.check_output('%s -dumpversion' % local_compiler,
                                     stderr = subprocess.STDOUT,
                                     shell = True)
    compiler_version = output.strip()
    hip_rpc_timeout = 240000  # ms
    cmd = ('%s --source=%s --hip_source=%s --source_version=%s --target=%s '
           '--include_paths="%s" --compile_options="%s" '
           '--compiler=%s --compiler_version="%s" --hip_rpc_timeout=%s '
           '--hip_cc_conf=%s --log_dir=%s --stderrthreshold=3' % (
           hipcc, source, hip_source, version, target,
           include_directories, rewrite_options(compile_info, options),
           local_compiler, compiler_version, hip_rpc_timeout,
           hipcc_conf, log_dir))
    p = subprocess.Popen(cmd, shell = True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = p.communicate()
    process_stderr(stderr, compile_info, target, hip_source)
    return p.returncode


def rewrite_options(compile_info, options):
    # Force add a `-H` option to obtain inclusion stack output
    if compile_info.H or compile_info.MF:
        return options + ' -H'
    return options


_INCLUSION_STACK_RE = re.compile(r'\.+ ')


def process_stderr(stderr, compile_info, target, hip_source):
    headers = []
    new_stderr = []
    for raw_line in stderr.splitlines():
        line = raw_line.rstrip()
        if _INCLUSION_STACK_RE.match(line):
            pos = line.find(' ')
            header = line[line.find(' ') + 1:]
            if not os.path.exists(header):  # Remote only, ignore
                continue
            headers.append(header)
            if compile_info.H:
                new_stderr.append(line)
            continue
        if line.startswith('Multiple include guards may be useful for:') and not compile_info.H:
            break
        new_stderr.append(line)

    # Reprint the filtered stderr
    sys.stderr.write('\n'.join(new_stderr))

    if not compile_info.MF:
        return
    # Generate a `xxx.o.d` file for the `-MF` option
    with open(compile_info.MF, 'w') as mf:
        first_line = "%s: %s" % (target, hip_source)
        if headers:
            first_line += ' \\'
        print(first_line, file=mf)
        for index, header in enumerate(headers):
            if compile_info.MMD and header.startswith('/'):
                continue
            if index == len(headers) - 1:
                print(' %s' % header, file=mf)
            else:
                print(' %s \\' % header, file=mf)


if __name__ == "__main__":
    hip, args = parse()
    if hip:
        result = compile(*args)
    else:
        result = subprocess.call(' '.join(args), shell = True)
    sys.exit(result)

