# Copyright (c) 2016 Tencent Inc.
# All rights reserved.
#
# Author: Li Wenting <wentingli@tencent.com>
# Date:   November 11, 2016

"""
This file is a thin wrapper around the underlying cxx executable.

"""

import os
import sys
import subprocess
import argparse


BUILD_SERVERS = [
    '10.12.67.13:17777',
    '10.12.72.76:17777',
]


class Cxx(object):
    def __init__(self):
        self.cxx = sys.argv[1]
        self.compile = '-c' in sys.argv
        self.cmdline = ' '.join(sys.argv[1:])
        self.parser = argparse.ArgumentParser(prog = 'cxx')
        self.parser.add_argument('-o', dest = 'output',
                                 help = 'the build output')

    def _execute(self, cmd):
        # print '[COMMAND]:', cmd
        return subprocess.call(cmd, shell = True)

    def _fallback(self):
        return self._execute(self.cmdline)

    def _handle_escape_characters(self, options):
        cmdline = self.cmdline
        # Handle special characters escaped by shell
        for option in options:
            escaped_option = ''
            for c in option:
                if c == '"':
                    escaped_option += '\\"'
                elif c == "'":
                    escaped_option += "\\'"
                elif c == '<':
                    escaped_option += "'<"
                elif c == '>':
                    escaped_option += ">'"
                else:
                    escaped_option += c
            if escaped_option != option:
                cmdline = cmdline.replace(option, escaped_option, 1)

        self.cmdline = cmdline

    def _preprocess_suffix(self, source):
        if source.endswith('.c'):
            return 'i'
        else:
            return 'ii'

    def _version(self):
        if 'GCCVERSION' in os.environ:
            return os.environ['GCCVERSION']
        output = subprocess.check_output('%s -dumpversion' % self.cxx,
                                         stderr = subprocess.STDOUT,
                                         shell = True)
        return output.strip()

    def _support_compile(self, source, target):
        # Only support gcc/g++ without path suffix
        if self.cxx not in ['g++', 'gcc']:
            return False
        if not target.endswith('.o'):
            return False
        if source.endswith('.s') or source.endswith('.S'):
            return False
        return True

    def _compile(self):
        parser = self.parser
        parser.add_argument('input', help = 'the input source file')
        parser.add_argument('-I', dest = 'include_directories',
                            action = 'append',
                            help = 'include directories used for header search')
        compile_info, options = parser.parse_known_args(sys.argv[2:])
        self._handle_escape_characters(options)
        if sys.argv[-1] == '-':
            return self._fallback()

        cmdline = self.cmdline
        source = compile_info.input
        object_output = compile_info.output
        if not self._support_compile(source, object_output):
            return self._fallback()

        preprocessed_output = object_output[:-1] + self._preprocess_suffix(source)
        preprocessed_cmd = cmdline.replace(' -c ', ' -E -fno-working-directory ').replace(object_output, preprocessed_output, 1)
        if self._execute(preprocessed_cmd):
            return 1

        object_cmd = cmdline[:cmdline.rindex(source)] + preprocessed_output
        # Filter out absolute include directories
        for d in compile_info.include_directories:
            if os.path.isabs(d):
                object_cmd = object_cmd.replace(' -I%s ' % d, ' ', 1)

        cxx = os.path.join(os.path.dirname(__file__), 'cxx')
        cmd = ('%s --target=%s --source=%s --cmdline="%s" '
               '--stage=compile --build_servers="%s" '
               '--compiler=%s --compiler_version="%s" --stderrthreshold=3 ' % (
               cxx, object_output, preprocessed_output, object_cmd,
               ','.join(BUILD_SERVERS), self.cxx, self._version()))
        return self._execute(cmd)

    def _link(self):
        return self._fallback()

    def run(self):
        if self.compile:
            return self._compile()
        else:
            return self._link()


if __name__ == "__main__":
    sys.exit(Cxx().run())

