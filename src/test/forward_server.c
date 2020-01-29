/* Copyright (C) 2018-2020 Intel Corporation
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
 */
/**
 * Server utilizing crt_launch generated environment for NO-PMIX mode
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <cart/api.h>

#include "tests_common.h"
#define NUM_SERVER_CTX 8

static d_rank_t		g_rank;
static crt_context_t	crt_ctx[NUM_SERVER_CTX];
static int		g_do_shutdown;

#define MY_BASE 0x010000000
#define MY_VER  0

#define TEST_IOV_SIZE_IN 2048
#define TEST_IOV_SIZE_OUT 2048

#define RPC_DECLARE(name)						\
	CRT_RPC_DECLARE(name, CRT_ISEQ_##name, CRT_OSEQ_##name)		\
	CRT_RPC_DEFINE(name, CRT_ISEQ_##name, CRT_OSEQ_##name)

enum {
	RPC_PING = CRT_PROTO_OPC(MY_BASE, MY_VER, 0),
	RPC_SHUTDOWN
} rpc_id_t;


#define CRT_ISEQ_RPC_PING	/* input fields */		 \
	((uint64_t)		(tag)			CRT_VAR) \
	((d_iov_t)		(test_data)		CRT_VAR)

#define CRT_OSEQ_RPC_PING	/* output fields */		 \
	((uint64_t)		(field)			CRT_VAR) \
	((d_iov_t)		(test_data)		CRT_VAR)

#define CRT_ISEQ_RPC_SHUTDOWN	/* input fields */		 \
	((uint64_t)		(field)			CRT_VAR)

#define CRT_OSEQ_RPC_SHUTDOWN	/* output fields */		 \
	((uint64_t)		(field)			CRT_VAR)

static int
handler_ping(crt_rpc_t *rpc);

static int
handler_shutdown(crt_rpc_t *rpc);

RPC_DECLARE(RPC_PING);
RPC_DECLARE(RPC_SHUTDOWN);

struct crt_proto_rpc_format my_proto_rpc_fmt[] = {
	{
		.prf_flags	= 0,
		.prf_req_fmt	= &CQF_RPC_PING,
		.prf_hdlr	= (void *)handler_ping,
		.prf_co_ops	= NULL,
	}, {
		.prf_flags	= 0,
		.prf_req_fmt	= &CQF_RPC_SHUTDOWN,
		.prf_hdlr	= (void *)handler_shutdown,
		.prf_co_ops	= NULL,
	}
};

struct crt_proto_format my_proto_fmt = {
	.cpf_name = "my-proto",
	.cpf_ver = MY_VER,
	.cpf_count = ARRAY_SIZE(my_proto_rpc_fmt),
	.cpf_prf = &my_proto_rpc_fmt[0],
	.cpf_base = MY_BASE,
};


static void
handle_forward_reply(const struct crt_cb_info *info)
{
	crt_rpc_t		*orig_rpc;
	d_iov_t			iov;
	struct RPC_PING_out	*output;

	orig_rpc = (crt_rpc_t *)info->cci_arg;

	D_ALLOC(iov.iov_buf, TEST_IOV_SIZE_OUT);
	memset(iov.iov_buf, 'b', TEST_IOV_SIZE_OUT);
	D_ASSERTF(iov.iov_buf != NULL, "Failed to allocate iov buf\n");

	iov.iov_buf_len = TEST_IOV_SIZE_OUT;
	iov.iov_len = TEST_IOV_SIZE_OUT;

	output = crt_reply_get(orig_rpc);
	output->test_data = iov;

	DBG_PRINT("Response received from forward with rc=%d\n",
		info->cci_rc);

	crt_reply_send(orig_rpc);
	crt_req_decref(orig_rpc);
}

int forward_rpc(crt_rpc_t *orig_rpc, int rank, int tag)
{
	crt_endpoint_t		ep;
	crt_rpc_t		*rpc = NULL;
	struct RPC_PING_in	*input;
	d_iov_t			iov;
	int			rc;

	ep.ep_grp = NULL;
	ep.ep_rank = rank;
	ep.ep_tag = tag;

	DBG_PRINT("Forwarding RPC to %d:%d\n", rank, tag);

	D_ALLOC(iov.iov_buf, TEST_IOV_SIZE_IN);
	memset(iov.iov_buf, 'a', TEST_IOV_SIZE_IN);

	iov.iov_buf_len = TEST_IOV_SIZE_IN;
	iov.iov_len = TEST_IOV_SIZE_IN;

	rc = crt_req_create(crt_ctx[1], &ep, RPC_PING, &rpc);
	assert(rc == 0);

	input = crt_req_get(rpc);
	input->tag = tag;
	input->test_data = iov;

	rc = crt_req_send(rpc, handle_forward_reply, orig_rpc);
	assert(rc == 0);

	return 0;
}

static int
handler_ping(crt_rpc_t *rpc)
{
	struct RPC_PING_in	*input;
	struct RPC_PING_out	*output;
	d_iov_t			iov;
	int			my_tag;
	d_rank_t		hdr_dst_rank;
	uint32_t		hdr_dst_tag;
	d_rank_t		hdr_src_rank;
	int			rc;

	input = crt_req_get(rpc);
	output = crt_reply_get(rpc);

	rc = crt_req_src_rank_get(rpc, &hdr_src_rank);
	D_ASSERTF(rc == 0, "crt_req_src_rank_get() failed; rc=%d\n", rc);

	rc = crt_req_dst_rank_get(rpc, &hdr_dst_rank);
	D_ASSERTF(rc == 0, "crt_req_dst_rank_get() failed; rc=%d\n", rc);

	rc = crt_req_dst_tag_get(rpc, &hdr_dst_tag);
	D_ASSERTF(rc == 0, "crt_req_dst_tag_get() failed; rc=%d\n", rc);

	crt_context_idx(rpc->cr_ctx, &my_tag);

	DBG_PRINT("Ping handler called on tag %d\n", my_tag);

	if (my_tag != input->tag || my_tag != hdr_dst_tag) {
		D_ERROR("Incorrect tag Expected %lu got %d (hdr=%d)\n",
			input->tag, my_tag, hdr_dst_tag);
		assert(0);
	}

	D_ALLOC(iov.iov_buf, TEST_IOV_SIZE_OUT);
	memset(iov.iov_buf, 'b', TEST_IOV_SIZE_OUT);
	D_ASSERTF(iov.iov_buf != NULL, "Failed to allocate iov buf\n");

	iov.iov_buf_len = TEST_IOV_SIZE_OUT;
	iov.iov_len = TEST_IOV_SIZE_OUT;

	output->test_data = iov;

	/* Rank 1 sends RPC forward request to rank 2 */
	if (g_rank == 1) {
		crt_req_addref(rpc);
		forward_rpc(rpc, 2, (my_tag + 1) % NUM_SERVER_CTX);
	} else {
		crt_reply_send(rpc);
	}

	D_FREE(iov.iov_buf);
	return 0;
}


static int
handler_shutdown(crt_rpc_t *rpc)
{
	DBG_PRINT("Shutdown handler called!\n");
	crt_reply_send(rpc);

	g_do_shutdown = true;
	return 0;
}

static void *
progress_function(void *data)
{
	crt_context_t *p_ctx = (crt_context_t *)data;

	while (g_do_shutdown == 0)
		crt_progress(*p_ctx, 1000, NULL, NULL);

	crt_context_destroy(*p_ctx, 1);

	return NULL;
}

static void
rpc_handle_reply(const struct crt_cb_info *info)
{
	sem_t	*sem;

	D_ASSERTF(info->cci_rc == 0, "rpc response failed. rc: %d\n",
		info->cci_rc);

	sem = (sem_t *)info->cci_arg;
	sem_post(sem);
}



int main(int argc, char **argv)
{
	crt_group_t		*grp;
	pthread_t		progress_thread[NUM_SERVER_CTX];
	int			i;
	char			*my_uri;
	char			*env_self_rank;
	char			*grp_cfg_file;
	uint32_t		grp_size;
	sem_t			sem;
	int			rc;

	env_self_rank = getenv("CRT_L_RANK");
	g_rank = atoi(env_self_rank);

	rc = sem_init(&sem, 0, 0);
	/* rank, num_attach_retries, is_server, assert_on_error */
	tc_test_init(g_rank, 20, true, true);

	rc = d_log_init();
	assert(rc == 0);

	DBG_PRINT("Server starting up\n");
	rc = crt_init("server_grp", CRT_FLAG_BIT_SERVER);
	if (rc != 0) {
		D_ERROR("crt_init() failed; rc=%d\n", rc);
		assert(0);
	}

	grp = crt_group_lookup(NULL);
	if (!grp) {
		D_ERROR("Failed to lookup group\n");
		assert(0);
	}

	rc = crt_rank_self_set(g_rank);
	if (rc != 0) {
		D_ERROR("crt_rank_self_set(%d) failed; rc=%d\n",
			g_rank, rc);
		assert(0);
	}

	rc = crt_context_create(&crt_ctx[0]);
	if (rc != 0) {
		D_ERROR("crt_context_create() failed; rc=%d\n", rc);
		assert(0);
	}

	rc = pthread_create(&progress_thread[0], 0,
			    progress_function, &crt_ctx[0]);
	if (rc != 0) {
		D_ERROR("pthread_create() failed; rc=%d\n", rc);
		assert(0);
	}

	grp_cfg_file = getenv("CRT_L_GRP_CFG");

	rc = crt_rank_uri_get(grp, g_rank, 0, &my_uri);
	if (rc != 0) {
		D_ERROR("crt_rank_uri_get() failed; rc=%d\n", rc);
		assert(0);
	}

	/* load group info from a config file and delete file upon return */
	rc = tc_load_group_from_file(grp_cfg_file, crt_ctx[0], grp, g_rank,
					true);
	if (rc != 0) {
		D_ERROR("tc_load_group_from_file() failed; rc=%d\n", rc);
		assert(0);
	}

	DBG_PRINT("self_rank=%d uri=%s grp_cfg_file=%s\n", g_rank,
		  my_uri, grp_cfg_file);
	D_FREE(my_uri);

	rc = crt_group_size(NULL, &grp_size);
	if (rc != 0) {
		D_ERROR("crt_group_size() failed; rc=%d\n", rc);
		assert(0);
	}

	rc = crt_proto_register(&my_proto_fmt);
	if (rc != 0) {
		D_ERROR("crt_proto_register() failed; rc=%d\n", rc);
		assert(0);
	}

	for (i = 1; i < NUM_SERVER_CTX; i++) {
		rc = crt_context_create(&crt_ctx[i]);
		if (rc != 0) {
			D_ERROR("crt_context_create() failed; rc=%d\n", rc);
			assert(0);
		}
	}

	for (i = 1; i < NUM_SERVER_CTX; i++) {
		rc = pthread_create(&progress_thread[i], 0,
				    progress_function, &crt_ctx[i]);
		if (rc != 0) {
			D_ERROR("pthread_create() failed; rc=%d\n", rc);
			assert(0);
		}
	}

	/* Rank 0 sends PING rpc to rank 1 */
	if (g_rank == 0) {
		crt_endpoint_t		server_ep;
		crt_rpc_t		*rpc = NULL;
		struct RPC_PING_in	*input;
		int			tag;
		d_iov_t			iov;

		DBG_PRINT("Will send rpc to rank=1 in 1 second\n");

		sleep(3);
		DBG_PRINT("Sleep done\n");

		D_ALLOC(iov.iov_buf, TEST_IOV_SIZE_IN);
		assert(iov.iov_buf != NULL);

		memset(iov.iov_buf, 'a', TEST_IOV_SIZE_IN);
		iov.iov_buf_len = TEST_IOV_SIZE_IN;
		iov.iov_len = TEST_IOV_SIZE_IN;


		#define NUM_RUNS 1

		for (i = 0; i < NUM_RUNS; i++) {
			// tag = rand() % NUM_SERVER_CTX;

			tag = 5;

			server_ep.ep_rank = 1;
			server_ep.ep_tag = tag;
			server_ep.ep_grp = grp;
			

			DBG_PRINT("Sending RPC to EP=%d:%d\n", server_ep.ep_rank, tag);
			rc = crt_req_create(crt_ctx[0], &server_ep,
					RPC_PING, &rpc);
			if (rc != 0) {
				D_ERROR("crt_req_create() failed; rc=%d\n",
					rc);
				assert(0);
			}

			input = crt_req_get(rpc);
			input->tag = tag;
			input->test_data = iov;

			rc = crt_req_send(rpc, rpc_handle_reply, &sem);
			tc_sem_timedwait(&sem, 10, __LINE__);
			DBG_PRINT("[%d] Ping response from %d:%d\n", i, server_ep.ep_rank, tag);
		}

	}

	/* Wait until shutdown is issued and progress threads exit */
	for (i = 0; i < NUM_SERVER_CTX; i++)
		pthread_join(progress_thread[i], NULL);

	rc = crt_finalize();
	if (rc != 0) {
		D_ERROR("crt_finalize() failed with rc=%d\n", rc);
		assert(0);
	}

	d_log_fini();

	return 0;
}

