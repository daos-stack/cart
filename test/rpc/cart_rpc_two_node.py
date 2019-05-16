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

from avocado       import Test
from avocado       import main

sys.path.append('./util')

from cart_utils import CartUtils

class CartRpcOneNodeTest(Test):
    """
    Runs basic CaRT tests on one-node and two-node

    :avocado: tags=all,rpc,two_node
    """
    def setUp(self):
        print("Running setup\n")
        self.utils = CartUtils()
        self.env = self.utils.get_env(self)

    def tearDown(self):
        print("Run TearDown\n")

    def test_cart_rpc(self):
        """
        Test CaRT

        :avocado: tags=all,rpc,two_node
        """

        urifile = self.utils.create_uri_file()

        srvcmd = self.utils.build_srv_cmd(self, urifile, self.env)
        clicmd = self.utils.build_cli_cmd(self, urifile, self.env)

        print("\nServer cmd : %s\n" % srvcmd)
        print("\nClient cmd : %s\n" % clicmd)

        ret = -1

        try:
            ret = self.utils.launch_srv_cli(self, srvcmd, clicmd)
        except Exception as e:
            print("Exception in launching test : {}".format(e))

        expected_result = self.params.get("status", '/run/tests/*/')

        if (expected_result == 'FAIL' and ret == 0):
            self.fail("Test was expected to fail but it passed.\n")

        if (expected_result == 'PASS' and ret != 0):
            self.fail("Test was expected to pass but it failed.\n")

if __name__ == "__main__":
    main()
