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
#define NUM_SERVER_CTX_PSM2		(1)

#define NUM_SERVER_CTX 4

#define YULU_DEBUG \
	DBG_PRINT("on line %s:%s:%d\n", __FILE__, __func__, __LINE__);

static int mypause = 1;

static void *
progress_func(void *arg)
{
	crt_context_t	ctx;
	pthread_t	current_thread = pthread_self();
	int		num_cores = sysconf(_SC_NPROCESSORS_ONLN);
	cpu_set_t	cpuset;
	int		t_idx;
	int		rc;

	t_idx = *(int *)arg;
	CPU_ZERO(&cpuset);
	CPU_SET(t_idx % num_cores, &cpuset);
	pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);

	DBG_PRINT("progress thread %d running on core %d...\n",
		t_idx, sched_getcpu());

	ctx = (crt_context_t)test_g.t_crt_ctx[t_idx];
	/* progress loop */
	while (1) {
		rc = crt_progress(ctx, 0, NULL, NULL);
		if (rc != 0 && rc != -DER_TIMEDOUT)
			D_ERROR("crt_progress failed rc: %d.\n", rc);
		if (test_g.t_shutdown == 1)
			break;
	}

	DBG_PRINT("%s: rc: %d, test_srv.do_shutdown: %d.\n", __func__, rc,
	       test_g.t_shutdown);
	DBG_PRINT("%s: progress thread exit ...\n", __func__);

	return NULL;
}


static struct crt_proto_rpc_format my_proto_rpc_fmt_test_multi_prov[] = {
	{
		.prf_flags	= 0,
		.prf_req_fmt	= &CQF_test_ping_check,
		.prf_hdlr	= test_checkin_handler,
		.prf_co_ops	= NULL,
	}, {
		.prf_flags	= CRT_RPC_FEAT_NO_REPLY,
		.prf_req_fmt	= NULL,
		.prf_hdlr	= test_shutdown_handler,
		.prf_co_ops	= NULL,
	}, {
		.prf_flags	= CRT_RPC_FEAT_NO_TIMEOUT,
		.prf_req_fmt	= &CQF_crt_test_ping_delay,
		.prf_hdlr	= test_ping_delay_handler,
		.prf_co_ops	= NULL,
	}
};

static struct crt_proto_format my_proto_fmt_test_multi_prov = {
	.cpf_name = "my-proto-test-multi-prov",
	.cpf_ver = TEST_MULTI_PROV_VER,
	.cpf_count = ARRAY_SIZE(my_proto_rpc_fmt_test_multi_prov),
	.cpf_prf = &my_proto_rpc_fmt_test_multi_prov[0],
	.cpf_base = TEST_MULTI_PROV_BASE,
};

void
test_init(void)
{
	int			 i;
	char			*env_self_rank;
	crt_ctx_init_opt_t	 ctx_opt;
	char			*my_uri;
	crt_group_t		*grp;
	uint32_t		 grp_size;
	char			*grp_cfg_file;
	d_rank_t		 my_rank;
	int			 rc = 0;

	DBG_PRINT("local group: %s\n",
		test_g.t_srv_grp_name);

	env_self_rank = getenv("CRT_L_RANK");
	my_rank = atoi(env_self_rank);
	/* rank, num_attach_retries, is_server, assert_on_error */
	tc_test_init(my_rank, 20, true, true);

	/* In order to use things like D_ASSERTF, logging needs to be active
	 * even if cart is not
	 */
	rc = d_log_init();
	D_ASSERT(rc == 0);

	D_DEBUG(DB_TEST, "starting multi-prov test\n");
	rc = sem_init(&test_g.t_token_to_proceed, 0, 0);
	D_ASSERTF(rc == 0, "sem_init() failed.\n");

	/**
	 * crt_init() reads OIF_INTERFACE, PHY_ADDR, and OFI_PORT. Currently
	 * these ENVs are the only way to control crt_init()
	 */
	rc = crt_init(test_g.t_srv_grp_name, CRT_FLAG_BIT_SERVER);

	D_ASSERTF(rc == 0, "crt_init() failed, rc: %d\n", rc);


	/* register RPCs */
	rc = crt_proto_register(&my_proto_fmt_test_multi_prov);
	D_ASSERTF(rc == 0, "crt_proto_register() failed. rc: %d\n", rc);
	YULU_DEBUG;

	///////////////////////////////
	//load configs from file

	// step 1, retrieve local group handle
	grp = crt_group_lookup(NULL);
	if (!grp) {
		D_ERROR("Failed to lookup group\n");
		assert(0);
	}

	// create sockets contexts
	// step 2, create context 0
	for (i = 0; i < test_g.t_ctx_num; i++) {
		rc = crt_context_create(&test_g.t_crt_ctx[i]);
		D_ASSERTF(rc == 0, "crt_context_create() failed. rc: %d\n", rc);
		test_g.t_thread_id[i] = i;
		rc = pthread_create(&test_g.t_tid[i], NULL, progress_func,
				    &test_g.t_thread_id[i]);
		D_ASSERTF(rc == 0, "pthread_create() failed. rc: %d\n", rc);
	}
	// assign local rank
	// step 3,
	rc = crt_rank_self_set(my_rank);
	if (rc != 0) {
		D_ERROR("crt_rank_self_set(%d) failed; rc=%d\n",
			my_rank, rc);
		assert(0);
	}

	// retrive self rank as a sanity check
	// setp 4
	rc = crt_rank_uri_get(grp, my_rank,
			CRT_NA_OFI_SOCKETS, 0,
			&my_uri);
	if (rc != 0) {
		D_ERROR("crt_rank_uri_get() failed; rc=%d\n", rc);
		assert(0);
	}

	/**
	 * load group info from a config file, create contexts, populate peer
	 * rank URIs based on config file.  and delete file upon return if so
	 * requested
	 */
	grp_cfg_file = getenv("CRT_L_GRP_CFG");
	rc = tc_load_group_from_file(grp_cfg_file, test_g.t_crt_ctx[0],
			grp, my_rank,
			false);
	if (rc != 0) {
		D_ERROR("tc_load_group_from_file() failed; rc=%d\n", rc);
		assert(0);
	}

	DBG_PRINT("self_rank=%d uri=%s grp_cfg_file=%s\n", my_rank,
			my_uri, grp_cfg_file);
	D_FREE(my_uri);

	rc = crt_group_size(NULL, &grp_size);
	if (rc != 0) {
		D_ERROR("crt_group_size() failed; rc=%d\n", rc);
		assert(0);
	}

	/* create psm2 contexts */
	for (i = 0; i < test_g.t_ctx_num_psm2; i++) {
		int j = i + test_g.t_ctx_num;
		ctx_opt.ccio_interface = "ib0";
		ctx_opt.ccio_prov = "ofi+psm2";
		ctx_opt.ccio_port = 25125;
		ctx_opt.ccio_share_na = 0;
		ctx_opt.ccio_ctx_max_num = 1;
		test_g.t_thread_id[j] = j;
		rc = crt_context_create_opt(&test_g.t_crt_ctx[j], &ctx_opt);
		D_ASSERTF(rc == 0, "crt_context_create_opt() failed. rc: %d\n",
			  rc);
		rc = pthread_create(&test_g.t_tid[j], NULL, progress_func,
				    &test_g.t_thread_id[j]);
		D_ASSERTF(rc == 0, "pthread_create() failed. rc: %d\n", rc);
	}
	if (my_rank == 0)
		while (mypause) {
			sched_yield();
		}

	crt_group_config_save_v2(grp, "eth0", "ofi+sockets");
	crt_group_config_save_v2(grp, "ib0", "ofi+psm2");
}

void
test_run(void)
{
	return;
}

void
test_fini()
{
	int				 ii;
	int				 rc = 0;

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
	// do the same for psm2
	for (ii = 0; ii < test_g.t_ctx_num_psm2; ii++) {
		int jj = ii + test_g.t_ctx_num;
		rc = pthread_join(test_g.t_tid[jj], NULL);
		if (rc != 0)
			DBG_PRINT("pthread_join failed. rc: %d\n", rc);
		D_DEBUG(DB_TEST, "joined progress thread.\n");

		/* try to flush indefinitely */
		rc = crt_context_flush(test_g.t_crt_ctx[jj], 0);
		D_ASSERTF(rc == 0 || rc == -DER_TIMEDOUT,
			  "crt_context_flush() failed. rc: %d\n", rc);

		rc = crt_context_destroy(test_g.t_crt_ctx[jj], 0);
		D_ASSERTF(rc == 0, "crt_context_destroy() failed. rc: %d\n",
			  rc);
		D_DEBUG(DB_TEST, "destroyed crt_ctx. id %d\n", jj);
	}

	rc = sem_destroy(&test_g.t_token_to_proceed);
	D_ASSERTF(rc == 0, "sem_destroy() failed.\n");
	/*
	if (test_g.t_is_service && test_g.t_my_rank == 0) {
		rc = crt_group_config_remove(NULL);
		assert(rc == 0);
	}
	*/

	/*
	if (test_g.t_is_service) {
		crt_swim_fini();
	}
	*/

	rc = crt_finalize();
	D_ASSERTF(rc == 0, "crt_finalize() failed. rc: %d\n", rc);

	d_log_fini();

	D_DEBUG(DB_TEST, "exiting.\n");
}

int main(int argc, char **argv)
{

	DBG_PRINT("XXXX multi_prov server started.\n");
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

	test_g.t_shutdown = 1;
	test_fini();

	return rc;
}
