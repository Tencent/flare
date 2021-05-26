#!/usr/bin/env python
# -*- coding: gbk -*-
#Copy librarys genereated by blade to source directory
#Author: Manfeng Xu(svdxu@tencent.com)
#Ugly

import re
import sys
import time
import csv
import os
import smtplib
import email
import traceback
import string
import shutil

import subprocess

#设定字符编码为GBK
reload(sys)
sys.setdefaultencoding('utf-8')

_configer_file="release_lib.conf"

def _error_exit(msg, code = 1):
  msg = "(error): " + msg
  msg = '\033[1;31m' + msg + '\033[0m'
  print >>sys.stderr, msg
  sys.exit(code)

def _info(msg):
  msg = "(info): " + msg
  msg = '\033[1;34m' + msg + '\033[0m'
  print >>sys.stderr, msg

def _error(msg):
  msg = "(error): " + msg
  msg = '\033[1;31m' + msg + '\033[0m'
  print >>sys.stderr, msg

def release_lib (src = "", out = "") :
  target = src
  dest = out
  if not os.path.isdir(dest) :
    _error("error %s is not directory" % dest)
    return
  prefixs = ["64_debug", "64_release", "32_debug", "32_release"]
  for prefix_dir in prefixs :
      target_dir = "build" + prefix_dir + "/" + target
      dest_dir = dest + "/lib" + prefix_dir
      if not os.path.isdir(dest_dir) :
        _info( "mkdir %s" % dest_dir)
        os.mkdir(dest_dir)
      if os.path.isdir(target_dir):
        files = os.listdir(target_dir)
        lib_pattern = re.compile("lib.*[.]+.*")
        for lib_file in files :
          if not lib_pattern.match(lib_file) :
            continue
          if os.path.islink(lib_file) :
            lib_file = os.readlink(lib_file)
          target_lib = target_dir + "/" + lib_file
          dest_lib = dest_dir + "/" + lib_file
          _info("Copy %s to %s " %(target_lib, dest_lib))
          shutil.copy(target_lib, dest_lib)

def _load_configer_file(configer_file):
  if os.path.exists(configer_file):
    try:
      execfile(configer_file)
    except SystemExit:
      _error_exit("%s: fatal error, exit..." % configer_file)
    except:
      _error_exit('Parse error in %s, exit...\n%s' % ( configer_file, traceback.format_exc()))
  else:
    _error_exit('%s not exist, exit...' % configer_file)

_load_configer_file(_configer_file)

