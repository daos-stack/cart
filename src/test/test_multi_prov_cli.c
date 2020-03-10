/* Copyright (C) 2019 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted for any purpose (including commercial purposes)
 * provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the
 *    documentation and/or materials provided with the distribution.
 *
 * 3. In addition, redistributions of modified forms of the source or binary
 *    code must carry prominent notices stating that the original code was
 *    changed and the date of the change.
 *
 *  4. All publications or advertising materials mentioning features or use of
 *     this software are asked, but not required, to acknowledge that it was
 *     developed by Intel Corporation and credit the contributors.
 *
 * 5. Neither the name of Intel Corporation, nor the name of any Contributor
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This is a cart multi provider test based on crt API.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <getopt.h>
#include <semaphore.h>

#include <cart/api.h>
#include "tests_common.h"
#include "test_multi_prov_common.h"

#define TEST_MULTI_PROV_BASE		0x010000000
#define TEST_MULTI_PROV_VER		0
#define TEST_CTX_MAX_NUM		(72)
#define DEFAULT_PROGRESS_CTX_IDX	0

#define NUM_SERVER_CTX 4

static inline void
test_sem_timedwait(sem_t *sem, int sec, int line_number)
{
	struct timespec			deadline;
	int				rc;

	rc = clock_gettime(CLOCK_REALTIME, &deadline);
	D_ASSERTF(rc == 0, "clock_gettime() failed at line %d rc: %d\n",
		  line_number, rc);
	deadline.tv_sec += sec;
	rc = sem_timedwait(sem, &deadline);
	D_ASSERTF(rc == 0, "sem_timedwait() failed at line %d rc: %d\n",
		  line_number, rc);
}

static struct crt_proto_rpc_format my_proto_rpc_fmt_test_multi_prov_cli[] = {
	{
		.prf_req_fmt	= &CQF_test_ping_check,
	}, {
		.prf_flags	= CRT_RPC_FEAT_NO_REPLY,
	}, {
		.prf_flags	= CRT_RPC_FEAT_NO_TIMEOUT,
		.prf_req_fmt	= &CQF_crt_test_ping_delay,
	}
};

static struct crt_proto_format my_proto_fmt_test_multi_prov_cli = {
	.cpf_name = "my-proto-test-cli",
	.cpf_ver = TEST_MULTI_PROV_VER,
	.cpf_count = ARRAY_SIZE(my_proto_rpc_fmt_test_multi_prov_cli),
	.cpf_prf = &my_proto_rpc_fmt_test_multi_prov_cli[0],
	.cpf_base = TEST_MULTI_PROV_BASE,
};

#define NUM_ATTACH_RETRIES 10
void
test_init(void)
{
	sleep(3);
	crt_group_t	*grp = NULL;
	d_rank_list_t	*rank_list;
	int		 rc = 0;

	tc_test_init(0, 40, false, true);

	DBG_PRINT("server group: %s\n", test_g.t_srv_grp_name);
	tc_cli_start_basic(test_g.t_cli_grp_name,
			   test_g.t_srv_grp_name,
			   &grp, &rank_list, &test_g.t_crt_ctx[0],
			   &test_g.t_tid[0], test_g.t_ctx_num,
			   false, NULL);
	test_g.t_srv_grp = grp;
	/* In order to use things like D_ASSERTF, logging needs to be active
	 * even if cart is not
	 */

	D_DEBUG(DB_TEST, "starting multi-prov test\n");
	rc = sem_init(&test_g.t_token_to_proceed, 0, 0);
	D_ASSERTF(rc == 0, "sem_init() failed.\n");

	/* register RPCs */
	rc = crt_proto_register(&my_proto_fmt_test_multi_prov_cli);
	D_ASSERTF(rc == 0, "crt_proto_register() failed. rc: %d\n", rc);

	rc = tc_wait_for_ranks(test_g.t_crt_ctx[0], grp, rank_list,
			    test_g.t_ctx_num - 1, test_g.t_ctx_num,
			    5, 150);
	D_ASSERTF(rc == 0, "wait_for_ranks() failed; rc=%d\n", rc);

	test_g.t_shutdown = 0;
}

void
mycheck_in(crt_group_t *remote_group, int rank)
{
	crt_rpc_t			*rpc_req = NULL;
	struct test_ping_check_in	*rpc_req_input;
	crt_endpoint_t			 server_ep = {0};
	char				*buffer;
	int				 my_rank = 0;
	int				 rc;

	server_ep.ep_grp = remote_group;
	server_ep.ep_rank = rank;
	rc = crt_req_create(test_g.t_crt_ctx[0], &server_ep,
			TEST_OPC_CHECKIN, &rpc_req);
	D_ASSERTF(rc == 0 && rpc_req != NULL, "crt_req_create() failed,"
			" rc: %d rpc_req: %p\n", rc, rpc_req);

	rpc_req_input = crt_req_get(rpc_req);
	D_ASSERTF(rpc_req_input != NULL, "crt_req_get() failed."
			" rpc_req_input: %p\n", rpc_req_input);

	/**
	 * example to inject faults to D_ALLOC. To turn it on, edit the fault
	 * config file: under fault id 1000, change the probability from 0 to
	 * anything in [1, 100]
	 */
	if (D_SHOULD_FAIL(test_g.t_fault_attr_1000)) {
		buffer = NULL;
	} else {
		D_ALLOC(buffer, 256);
		D_INFO("not injecting fault.\n");
	}

	D_ASSERTF(buffer != NULL, "Cannot allocate memory.\n");
	snprintf(buffer,  256, "Guest %d", 0);
	rpc_req_input->name = buffer;
	rpc_req_input->age = 21;
	rpc_req_input->days = 7;
	rpc_req_input->bool_val = true;
	D_DEBUG(DB_TEST, "client(rank %d tag %d) sending checkin rpc to server "
			"rank %d tag %d, "
			"name: %s, age: %d, days: %d, bool_val %d.\n",
		my_rank, 0, rank, server_ep.ep_tag, rpc_req_input->name,
		rpc_req_input->age, rpc_req_input->days,
		rpc_req_input->bool_val);

	/* send an rpc, print out reply */
	rc = crt_req_send(rpc_req, client_cb_common, NULL);
	D_ASSERTF(rc == 0, "crt_req_send() failed. rc: %d\n", rc);
}

void
test_run(void)
{
	int		ii;
	int		rc;

	rc = crt_group_size(test_g.t_srv_grp, &test_g.t_srv_grp_size);
	D_ASSERTF(rc == 0, "crt_group_size() failed; rc=%d\n", rc);
	D_DEBUG(DB_TEST, "srv_grp_size %d\n", test_g.t_srv_grp_size);

	for (ii = 0; ii < test_g.t_srv_grp_size; ii++) {
		mycheck_in(test_g.t_srv_grp, ii);
	}

	for (ii = 0; ii < test_g.t_srv_grp_size; ii++)
		test_sem_timedwait(&test_g.t_token_to_proceed, 61, __LINE__);
}

void
test_fini()
{
	int				 ii;
	crt_endpoint_t			 server_ep = {0};
	crt_rpc_t			*rpc_req = NULL;
	int				 rc = 0;

	/* client rank 0 tells all servers to shut down */
	if (0)
	for (ii = 0; ii < test_g.t_srv_grp_size; ii++) {
		server_ep.ep_grp = test_g.t_srv_grp;
		server_ep.ep_rank = ii;
		rc = crt_req_create(test_g.t_crt_ctx[0], &server_ep,
				TEST_OPC_SHUTDOWN, &rpc_req);
		D_ASSERTF(rc == 0 && rpc_req != NULL,
				"crt_req_create() failed. "
				"rc: %d, rpc_req: %p\n", rc, rpc_req);
		rc = crt_req_send(rpc_req, client_cb_common, NULL);
		D_ASSERTF(rc == 0, "crt_req_send() failed. rc: %d\n",
				rc);

		test_sem_timedwait(&test_g.t_token_to_proceed, 61,
				__LINE__);
	}
	test_g.t_shutdown = 1;

	rc = crt_group_view_destroy(test_g.t_srv_grp);
	if (rc != 0) {
		D_ERROR("crt_group_view_destroy() failed; rc=%d\n", rc);
		assert(0);
	}

	for (ii = 0; ii < test_g.t_ctx_num; ii++) {
		rc = pthread_join(test_g.t_tid[ii], NULL);
		if (rc != 0)
			DBG_PRINT("pthread_join failed. rc: %d\n", rc);
		D_DEBUG(DB_TEST, "joined progress thread.\n");

		/* try to flush indefinitely */
		rc = crt_context_flush(test_g.t_crt_ctx[ii], 0);
		D_ASSERTF(rc == 0 || rc == -DER_TIMEDOUT,
			  "crt_context_flush() failed. rc: %d\n", rc);

		rc = crt_context_destroy(test_g.t_crt_ctx[ii], 0);
		D_ASSERTF(rc == 0, "crt_context_destroy() failed. rc: %d\n",
			  rc);
		D_DEBUG(DB_TEST, "destroyed crt_ctx. id %d\n", ii);
	}

	rc = sem_destroy(&test_g.t_token_to_proceed);
	D_ASSERTF(rc == 0, "sem_destroy() failed.\n");

	rc = crt_finalize();
	D_ASSERTF(rc == 0, "crt_finalize() failed. rc: %d\n", rc);

	d_log_fini();

	D_DEBUG(DB_TEST, "exiting.\n");
}

int main(int argc, char **argv)
{
	DBG_PRINT("XXXX multi_prov client started.\n");
	int	rc;

	rc = test_parse_args(argc, argv);
	if (rc != 0) {
		DBG_PRINT("test_parse_args() failed, rc: %d.\n",
			rc);
		return rc;
	}

	test_init();
	test_run();
	if (test_g.t_hold)
		sleep(test_g.t_hold_time);
	test_fini();

	return rc;
}
