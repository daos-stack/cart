#!/usr/bin/python
'''
  (C) Copyright 2018-2019 Intel Corporation.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  GOVERNMENT LICENSE RIGHTS-OPEN SOURCE SOFTWARE
  The Government's rights to use, modify, reproduce, release, perform, display,
  or disclose this software are subject to the terms of the Apache License as
  provided in Contract No. B609815.
  Any reproduction of computer software, computer software documentation, or
  portions thereof marked with this legend must also reproduce the markings.
'''
from __future__ import print_function

import time
import os
import random
import json
import shlex
import subprocess

class CartUtils():
    """CartUtils Class"""

    def write_host_file(self, hostlist, slots=1):
        """ write out a hostfile suitable for orterun """

        unique = random.randint(1, 100000)

        path = "./hostfile"

        if not os.path.exists(path):
            os.makedirs(path)
        hostfile = path + '/hostfile' + str(unique)

        if hostlist is None:
            raise ValueError("host list parameter must be provided.")
        hostfile_handle = open(hostfile, 'w')

        for host in hostlist:
            if slots is None:
                print("<<{}>>".format(slots))
                hostfile_handle.write("{0}\n".format(host))
            else:
                print("<<{}>>".format(slots))
                hostfile_handle.write("{0} slots={1}\n".format(host, slots))
        hostfile_handle.close()
        return hostfile

    def create_uri_file(self):
        """ create uri file suitable for orterun """

        path = "./uri"
        unique = random.randint(1, 100000)

        if not os.path.exists(path):
            os.makedirs(path)
        urifile = path + "/urifile" + str(unique)

        return urifile

    def check_process(self, proc):
        """ check if a process is still running"""
        proc.poll()
        procrtn = proc.returncode
        if procrtn is None:
            return True
        return False

<<<<<<< HEAD
    def wait_process(self, proc, wait_time):
        """ wait for process to terminate """
        i = wait_time
        procrtn = None
        while i:
            proc.poll()
            procrtn = proc.returncode
            if procrtn is not None:
                break
            else:
                time.sleep(1)
                i = i - 1

        return procrtn

=======
>>>>>>> ef7f7da6df1f8d7948288564e7aac7056c63dd88
    def stop_process(self, proc):
        """ wait for process to terminate """
        i = 60
        procrtn = None
        while i:
            proc.poll()
            procrtn = proc.returncode
            if procrtn is not None:
                break
            else:
                time.sleep(1)
                i = i - 1

        if procrtn is None:
            procrtn = -1
            try:
                proc.terminate()
<<<<<<< HEAD
=======
                proc.wait(2)
>>>>>>> ef7f7da6df1f8d7948288564e7aac7056c63dd88
            except Exception:
                proc.kill()

        return procrtn

    def get_env(self, cartobj):
        """ return basic env setting in yaml """
        env_CCSA = cartobj.params.get("env", '/run/env_CRT_CTX_SHARE_ADDR/*/')
        test_name = cartobj.params.get("name", '/run/tests/*/')
        host_cfg = cartobj.params.get("config", '/run/hosts/*/')

        log_dir = "{}-{}-{}-{}".format(env_CCSA, test_name, host_cfg, cartobj.id())
        log_path = os.path.join("testLogs", log_dir)
        log_file = os.path.join(log_path, "output.log")

        log_mask = cartobj.params.get("D_LOG_MASK", '/run/defaultENV/')
        crt_phy_addr = cartobj.params.get("CRT_PHY_ADDR_STR", '/run/defaultENV/')
        ofi_interface = cartobj.params.get("OFI_INTERFACE", '/run/defaultENV/')
        ofi_share_addr = cartobj.params.get("CRT_CTX_SHARE_ADDR", '/run/env_CRT_CTX_SHARE_ADDR/*/')

        env = " --output-filename {!s}".format(log_path)
        env += ' -x D_LOG_MASK={!s} -x D_LOG_FILE={!s}' \
                        ' -x CRT_PHY_ADDR_STR={!s}' \
                        ' -x OFI_INTERFACE={!s}' \
                        ' -x CRT_CTX_SHARE_ADDR={!s}' \
                            .format(log_mask, log_file, crt_phy_addr, \
                                    ofi_interface, ofi_share_addr)

        if not os.path.exists(log_path):
            os.makedirs(log_path)

        return env

    def get_srv_cnt(self, cartobj, host):
        """ get server count """
        hostlist = cartobj.params.get("{}".format(host), '/run/hosts/*/')

        srvcnt = 0

        for host in hostlist:
            srvcnt += 1

        return srvcnt

    def build_cmd(self, cartobj, env, host, report_uri=True, urifile=None):
        """ build command """
        tst_cmd = ""

        # get paths from the build_vars generated by build
        with open('.build_vars.json') as build_file:
            build_paths = json.load(build_file)

        orterun_bin = os.path.join(build_paths["OMPI_PREFIX"], "bin", "orterun")

        tst_bin = " " + cartobj.params.get("{}_bin".format(host), '/run/tests/*/')
        tst_arg = " " + cartobj.params.get("{}_arg".format(host), '/run/tests/*/')
        tst_env = " " + cartobj.params.get("{}_env".format(host), '/run/tests/*/')
        tst_ctx = " -x CRT_CTX_NUM=" + cartobj.params.get("{}_CRT_CTX_NUM".format(host), '/run/defaultENV/')

        tst_host = cartobj.params.get("{}".format(host), '/run/hosts/*/')
        tst_ppn = cartobj.params.get("{}_ppn".format(host), '/run/tests/*/')
        hostfile = self.write_host_file(tst_host,tst_ppn)

        tst_cmd = "{} --mca btl self,tcp -N {} --hostfile {} ".format(orterun_bin, tst_ppn, hostfile)
        if urifile is not None:
            if report_uri == True:
                tst_cmd += "--report-uri {} ".format(urifile)
            else:
                tst_cmd += "--ompi-server file:{} ".format(urifile)
        tst_cmd += env
        tst_cmd += tst_ctx
        tst_cmd += tst_env
        tst_cmd += tst_bin
        tst_cmd += tst_arg

        return tst_cmd

    def launch_srv_cli(self, cartobj, srvcmd, clicmd):
        """ launches sever in the background and client in the foreground """

        srv_rtn = self.launch_cmd_bg(cartobj, srvcmd)

        # Verify the server is still running.
        if not self.check_process(srv_rtn):
            procrtn = self.stop_process(srv_rtn)
            cartobj.fail("Server did not launch, return code %s" \
                       % procrtn)

        cli_rtn = self.launch_cmd(cartobj, clicmd)

        srv_rtn = self.stop_process(srv_rtn)

        if cli_rtn or srv_rtn:
            cartobj.fail("Failed, return codes client %d " % cli_rtn + \
                      "server %d" % srv_rtn)

        return 0

    def launch_cmd(self, cartobj, cmd):
        """ launches the given cmd """

        cmd = shlex.split(cmd)
        rtn = subprocess.call(cmd)

        if rtn:
            cartobj.fail("Failed, return codes %d " % rtn)

<<<<<<< HEAD
        return rtn 
=======
        return 0
>>>>>>> ef7f7da6df1f8d7948288564e7aac7056c63dd88

    def launch_cmd_bg(self, cartobj, cmd):
        """ launches the given cmd in background """

        cmd = shlex.split(cmd)
        rtn = subprocess.Popen(cmd)

        if rtn is None:
            cartobj.fail("Background process launch failed, \
                          return codes %d " % rtn.returncode)

        return rtn
