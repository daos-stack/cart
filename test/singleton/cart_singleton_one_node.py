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

import sys
import time
import tempfile
import os
import shutil

from avocado       import Test
from avocado       import main

sys.path.append('./util')

from cart_utils import CartUtils

class CartSingletonOneNodeTest(Test):
    """
    Runs basic CaRT tests on one-node

    :avocado: tags=all,singleton,one_node
    """
    def setUp(self):
        print("Running setup\n")
        self.utils = CartUtils()
        self.env = self.utils.get_env(self)
        self.tempdir = tempfile.mkdtemp()

    def tearDown(self):
        print("Run TearDown\n")
        shutil.rmtree(self.tempdir)

    def test_cart_singleton(self):
        """
        Test CaRT Singleton

        :avocado: tags=all,singleton,one_node
        """

        srvcmd = self.utils.build_cmd(self, self.env, "srv")

        srvcmd += " -p {} -s".format(self.tempdir)

        print("\nServer cmd : %s\n" % srvcmd)

        try:
            srv_rtn = self.utils.launch_cmd_bg(self, srvcmd)
        except Exception as e:
            print("Exception in launching server : {}".format(e))
            self.fail("Test failed.\n")

        # Verify the server is still running.
        if not self.utils.check_process(srv_rtn):
            procrtn = self.utils.stop_process(srv_rtn)
            self.fail("Server did not launch, return code %s" \
                       % procrtn)

        os.environ["CRT_PHY_ADDR_STR"] = self.params.get("CRT_PHY_ADDR_STR",
                                                         '/run/defaultENV/')
        os.environ["OFI_INTERFACE"] = self.params.get("OFI_INTERFACE",
                                                      '/run/defaultENV/')
        os.environ["CRT_CTX_NUM"] = self.params.get("cli_CRT_CTX_NUM", 
                                                    '/run/defaultENV/')
        os.environ["CRT_CTX_SHARE_ADDR"] = self.params.get("CRT_CTX_SHARE_ADDR",
                                               '/run/env_CRT_CTX_SHARE_ADDR/*/')

        clicmd = self.params.get("cli_bin", '/run/tests/*/')

        clicmd += " -p {} -s".format(self.tempdir)

        print("\nClient cmd : %s\n" % clicmd)

        cli_rtn = self.utils.launch_cmd(self, clicmd)

        # Stop the server
        print("Stopping server process {}".format(srv_rtn))        
        procrtn = self.utils.stop_process(srv_rtn)

        if cli_rtn or procrtn:
            self.fail("Test failed. Client ret code {}, \
                       server ret code {}".format(cli_rtn, procrtn))

if __name__ == "__main__":
    main()
