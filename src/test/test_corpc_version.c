/* Copyright (C) 2017-2019 Intel Corporation
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
 * This is a test for the CORPC error case in which the group signatures between
 * participant ranks don't match.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <getopt.h>
#include <semaphore.h>

#include <gurt/atomic.h>
#include <cart/api.h>

#include "tests_common.h"

#define TEST_CORPC_BASE1 0x010000000
#define TEST_CORPC_BASE2 0x020000000
#define TEST_CORPC_VER 0

#define TEST_OPC_SHUTDOWN CRT_PROTO_OPC(TEST_CORPC_BASE2,		\
						TEST_CORPC_VER, 0)
#define TEST_OPC_CORPC_VER_MISMATCH CRT_PROTO_OPC(TEST_CORPC_BASE1,	\
						TEST_CORPC_VER, 0)
#define TEST_OPC_RANK_EVICT CRT_PROTO_OPC(TEST_CORPC_BASE2,		\
						TEST_CORPC_VER, 1)
#define TEST_OPC_SUBGRP_PING CRT_PROTO_OPC(TEST_CORPC_BASE2,		\
						TEST_CORPC_VER, 2)

struct test_t {
	crt_group_t		*t_primary_group;
	crt_group_t		*t_sub_group;
	char			*t_primary_group_name;
	ATOMIC uint32_t		 t_shutdown;
	uint32_t		 t_my_rank;
	uint32_t		 t_my_pid;
	uint32_t		 t_my_group_size;
	crt_context_t		 t_crt_ctx;
	pthread_t		 t_tid;
	sem_t			 t_token;
};

struct test_t test;


#define CRT_ISEQ_CORPC_VER_MISMATCH /* input fields */		 \
	((uint32_t)		(magic)			CRT_VAR)

#define CRT_OSEQ_CORPC_VER_MISMATCH /* output fields */		 \
	((uint32_t)		(magic)			CRT_VAR) \
	((uint32_t)		(result)		CRT_VAR)

CRT_RPC_DECLARE(corpc_ver_mismatch,
		CRT_ISEQ_CORPC_VER_MISMATCH, CRT_OSEQ_CORPC_VER_MISMATCH)
CRT_RPC_DEFINE(corpc_ver_mismatch,
		CRT_ISEQ_CORPC_VER_MISMATCH, CRT_OSEQ_CORPC_VER_MISMATCH)

#define CRT_ISEQ_RANK_EVICT	/* input fields */		 \
	((uint32_t)		(rank)			CRT_VAR)

#define CRT_OSEQ_RANK_EVICT	/* output fields */		 \
	((uint32_t)		(rc)			CRT_VAR)

CRT_RPC_DECLARE(rank_evict, CRT_ISEQ_RANK_EVICT, CRT_OSEQ_RANK_EVICT)
CRT_RPC_DEFINE(rank_evict, CRT_ISEQ_RANK_EVICT, CRT_OSEQ_RANK_EVICT)

#define CRT_ISEQ_SUBGRP_PING	/* input fields */		 \
	((uint32_t)		(magic)			CRT_VAR)

#define CRT_OSEQ_SUBGRP_PING	/* output fields */		 \
	((uint32_t)		(magic)			CRT_VAR)

CRT_RPC_DECLARE(subgrp_ping, CRT_ISEQ_SUBGRP_PING, CRT_OSEQ_SUBGRP_PING)
CRT_RPC_DEFINE(subgrp_ping, CRT_ISEQ_SUBGRP_PING, CRT_OSEQ_SUBGRP_PING)

static void client_cb(const struct crt_cb_info *cb_info);

d_rank_t real_ranks[] = {4, 3, 1, 2};
d_rank_t sec_ranks[] = {0, 1, 2, 3};

int
test_parse_args(int argc, char **argv)
{
	int			 option_index = 0;
	int			 rc = 0;

	struct option		 long_options[] = {
		{"name", required_argument, 0, 'n'},
		{0, 0, 0, 0}
	};

	while (1) {
		rc = getopt_long(argc, argv, "n:a:", long_options,
				 &option_index);
		if (rc == -1)
			break;
		switch (rc) {
		case 0:
			if (long_options[option_index].flag != 0)
				break;
		case 'n':
			test.t_primary_group_name = optarg;
			break;
		case '?':
			return 1;
		default:
			return 1;
		}
	}
	if (optind < argc) {
		DBG_PRINT("non-option argv elements encountered");
		return 1;
	}

	return 0;
}

static void *progress_thread(void *arg)
{
	crt_context_t	crt_ctx;
	int		rc;

	crt_ctx = (crt_context_t) arg;
	do {
		rc = crt_progress(crt_ctx, 1, NULL, NULL);
		if (rc != 0 && rc != -DER_TIMEDOUT) {
			D_ERROR("crt_progress failed rc: %d.\n", rc);
			/* continue calling progress on error */
		}

		if (atomic_load_consume(&test.t_shutdown) == 1)
			break;
		sched_yield();
	} while (1);

	D_ASSERTF(rc == 0 || rc == -DER_TIMEDOUT,
		  "Failure exiting progress loop: rc: %d\n", rc);
	DBG_PRINT("progress_thread: progress thread exit ...\n");

	pthread_exit(NULL);
}

static int
test_rank_evict(crt_group_t *grp, crt_context_t *crt_ctx, d_rank_t rank)
{
	d_rank_list_t		*mod_ranks;
	int			rc = 0;

	mod_ranks = d_rank_list_alloc(1);
	if (!mod_ranks) {
		D_ERROR("rank list allocation failed\n");
		assert(0);
	}
	mod_ranks->rl_ranks[0] = rank;
	mod_ranks->rl_nr = 1;

	rc = crt_group_primary_modify(grp, crt_ctx, 1,
				mod_ranks, NULL,
				CRT_GROUP_MOD_OP_REMOVE);
	if (rc != 0) {
		D_ERROR("crt_group_primary_modify() failed; rc=%d\n", rc);
		assert(0);
	}
	crt_group_version_set(grp, 1);
	DBG_PRINT("group %p version changed to %d\n", grp, 1);

	return rc;
}

static void
corpc_ver_mismatch_hdlr(crt_rpc_t *rpc_req)
{
	struct corpc_ver_mismatch_in	*rpc_req_input;
	struct corpc_ver_mismatch_out	*rpc_req_output;
	int				 rc = 0;

	rpc_req_input = crt_req_get(rpc_req);
	rpc_req_output = crt_reply_get(rpc_req);
	D_ASSERT(rpc_req_input != NULL && rpc_req_output != NULL);
	DBG_PRINT("server received request, opc: 0x%x.\n",
		rpc_req->cr_opc);
	rpc_req_output->magic = rpc_req_input->magic;
	rpc_req_output->result = 1;
	rc = crt_reply_send(rpc_req);
	D_ASSERT(rc == 0);
	DBG_PRINT("received magic number %d, reply %d\n",
		rpc_req_input->magic, rpc_req_output->result);

	/* now everybody evicts rank 2 so group destroy can succeed */

	if (0 && test.t_my_rank != 2) {
		rc = test_rank_evict(test.t_primary_group, test.t_crt_ctx,
				2);
		if (rc != DER_SUCCESS)
			D_ERROR("crt_rank_evcit(grp=%p, rank=2) failed, "
				"rc %d\n", test.t_primary_group, rc);
	}
}

static void
test_shutdown_hdlr(crt_rpc_t *rpc_req)
{
	int rc = 0;

	D_DEBUG(DB_ALL, "rpc err server received shutdown request, "
		"opc: 0x%x.\n", rpc_req->cr_opc);
	D_ASSERTF(rpc_req->cr_input == NULL, "RPC request has invalid input\n");
	D_ASSERTF(rpc_req->cr_output == NULL, "RPC request output is NULL\n");

	atomic_store_release(&test.t_shutdown, 1);
	D_DEBUG(DB_ALL, "server set shutdown flag.\n");
	rc = crt_reply_send(rpc_req);
	D_ASSERTF(rc == 0, "crt_reply_send(opc=0x%x) failed, rc %d\n",
		  rpc_req->cr_opc, rc);
}

static void
subgrp_ping_hdlr(crt_rpc_t *rpc_req)
{
	struct subgrp_ping_in	*rpc_req_input;
	struct subgrp_ping_out	*rpc_req_output;
	int			 rc = 0;

	rpc_req_input = crt_req_get(rpc_req);
	D_ASSERT(rpc_req_input != NULL);
	rpc_req_output = crt_reply_get(rpc_req);
	D_ASSERT(rpc_req_output != NULL);

	D_DEBUG(DB_TEST, "Recieved magic number %d\n", rpc_req_input->magic);
	rpc_req_output->magic = rpc_req_input->magic + 1;
	rc = crt_reply_send(rpc_req);
	D_ASSERT(rc == 0);
}

static void
test_rank_evict_hdlr(crt_rpc_t *rpc_req)
{
	struct rank_evict_in	*rpc_req_input;
	struct rank_evict_out	*rpc_req_output;
	int			 rc = 0;

	rpc_req_input = crt_req_get(rpc_req);
	rpc_req_output = crt_reply_get(rpc_req);
	D_ASSERT(rpc_req_input != NULL && rpc_req_output != NULL);

	DBG_PRINT("server received eviction request, opc: 0x%x.\n",
		rpc_req->cr_opc);

	if (test.t_sub_group == NULL)
		DBG_PRINT("test.t_sub_group shuold not be NULL\n");
	D_ASSERTF(test.t_sub_group != NULL, "should not be NULL\n");
	rc = test_rank_evict(test.t_primary_group, test.t_crt_ctx,
			     rpc_req_input->rank);
	D_ASSERT(rc == 0);
	D_DEBUG(DB_TEST, "rank %d evicted rank %d.\n", test.t_my_rank,
		rpc_req_input->rank);

	rpc_req_output->rc = rc;
	rc = crt_reply_send(rpc_req);
	D_ASSERT(rc == 0);
}

static int
corpc_ver_mismatch_aggregate(crt_rpc_t *source, crt_rpc_t *result, void *priv)
{
	struct corpc_ver_mismatch_out *reply_source;
	struct corpc_ver_mismatch_out *reply_result;

	D_ASSERT(source != NULL && result != NULL);
	reply_source = crt_reply_get(source);
	reply_result = crt_reply_get(result);
	reply_result->result += reply_source->result;

	printf("corpc_ver_mismatch_aggregate, rank %d, result %d, "
	       "aggregate result %d.\n",
	       test.t_my_rank, reply_source->result, reply_result->result);

	return 0;
}

struct crt_corpc_ops corpc_ver_mismatch_ops = {
	.co_aggregate = corpc_ver_mismatch_aggregate,
	.co_pre_forward = NULL,
};

void
primary_shutdown_cmd_issue()
{
	crt_endpoint_t			 server_ep;
	crt_rpc_t			*rpc_req = NULL;
	int				 i;
	int				 rc = 0;

	for (i = 0; i < test.t_my_group_size; i++) {
		if (i == test.t_my_rank)
			continue;
		server_ep.ep_grp = test.t_primary_group;
		server_ep.ep_rank = i;
		server_ep.ep_tag = 0;
		rc = crt_req_create(test.t_crt_ctx, &server_ep,
				    TEST_OPC_SHUTDOWN,
				    &rpc_req);
		D_ASSERTF(rc == 0 && rpc_req != NULL,
			  "crt_req_create() failed, rc: %d rpc_req: %p\n",
			  rc, rpc_req);
		rc = crt_req_send(rpc_req, client_cb, NULL);
		D_ASSERTF(rc == 0, "crt_req_send() failed. rc: %d\n", rc);

	}
}


static int
rank_evict_cb(crt_rpc_t *rpc_req)
{
	d_rank_t			 excluded_ranks[1] = {2};
	d_rank_list_t			 excluded_membs;
	crt_rpc_t			*corpc_req;
	struct corpc_ver_mismatch_in	*corpc_in;
	struct rank_evict_out		*rpc_req_output;
	int				 rc = 0;


	rpc_req_output = crt_reply_get(rpc_req);
	if (rpc_req_output == NULL)
		return -DER_INVAL;

	/* excluded ranks  contains secondary logical ranks */
	excluded_membs.rl_nr = 1;
	excluded_membs.rl_ranks = excluded_ranks;
	rc = crt_corpc_req_create(test.t_crt_ctx, test.t_sub_group,
			&excluded_membs, TEST_OPC_CORPC_VER_MISMATCH,
			NULL, NULL, 0,
			crt_tree_topo(CRT_TREE_KNOMIAL, 4),
			&corpc_req);
	DBG_PRINT("crt_corpc_req_create()  rc: %d, my_rank %d.\n", rc,
		test.t_my_rank);
	D_ASSERT(rc == 0 && corpc_req != NULL);
	corpc_in = crt_req_get(corpc_req);
	D_ASSERT(corpc_in != NULL);
	corpc_in->magic = random()%100;

	rc = crt_req_send(corpc_req, client_cb, NULL);
	D_ASSERT(rc == 0);


	return 0;
}
static int
corpc_ver_mismatch_cb(crt_rpc_t *rpc_req)
{
	struct corpc_ver_mismatch_in		*rpc_req_input;
	struct corpc_ver_mismatch_out		*rpc_req_output;
	int					 rc = 0;

	rpc_req_input = crt_req_get(rpc_req);
	if (rpc_req_input == NULL)
		return -DER_INVAL;
	rpc_req_output = crt_reply_get(rpc_req);
	if (rpc_req_output == NULL)
		return -DER_INVAL;
	DBG_PRINT("%s, bounced back magic number: %d, %s\n",
		test.t_primary_group_name,
		rpc_req_output->magic,
		rpc_req_output->magic == rpc_req_input->magic ?
		"MATCH" : "MISMATCH");
	primary_shutdown_cmd_issue();

	return rc;
}

static int
eviction_rpc_issue(void)
{
	crt_rpc_t		*rpc_req;
	crt_endpoint_t		 server_ep;
	struct rank_evict_in	*rpc_req_input;
	int			 rc = 0;

	/* tell rank 4 to evict rank 2 */
	server_ep.ep_grp = test.t_primary_group;
	server_ep.ep_rank = 4;
	server_ep.ep_tag = 0;
	rc = crt_req_create(test.t_crt_ctx, &server_ep, TEST_OPC_RANK_EVICT,
			    &rpc_req);
	D_ASSERTF(rc == 0 && rpc_req != NULL,
		  "crt_req_create() failed, rc: %d rpc_req: %p\n",
		  rc, rpc_req);
	rpc_req_input = crt_req_get(rpc_req);
	D_ASSERTF(rpc_req_input != NULL, "crt_req_get() failed. "
		  "rpc_req_input: %p\n", rpc_req_input);
	rpc_req_input->rank = 2;
	rc = crt_req_send(rpc_req, client_cb, NULL);
	D_ASSERTF(rc == 0, "crt_req_send() failed, rc %d\n", rc);

	return rc;
}

static int
subgrp_ping_cb(crt_rpc_t *rpc_req)
{
	struct subgrp_ping_in	*rpc_req_input;
	struct subgrp_ping_out	*rpc_req_output;
	int			 rc = 0;

	rpc_req_input = crt_req_get(rpc_req);
	D_ASSERT(rpc_req_input != NULL);
	rpc_req_output = crt_reply_get(rpc_req);
	D_ASSERT(rpc_req_output != NULL);

	D_DEBUG(DB_TEST, "Received magic number %d\n", rpc_req_output->magic);
	D_ASSERT(rpc_req_output->magic == rpc_req_input->magic + 1);

	eviction_rpc_issue();

	return rc;
}

static void
client_cb(const struct crt_cb_info *cb_info)
{
	crt_rpc_t		*rpc_req;

	rpc_req = cb_info->cci_rpc;

	switch (cb_info->cci_rpc->cr_opc) {
	case TEST_OPC_SUBGRP_PING:
		D_DEBUG(DB_TEST, "subgrp_ping got reply\n");
		subgrp_ping_cb(rpc_req);
		break;
	case TEST_OPC_CORPC_VER_MISMATCH:
		DBG_PRINT("RPC failed, return code: %d.\n",
			cb_info->cci_rc);
/*
 * TODO: This needs to be investigated in a test. Depending on which
 * rank is hit first, we might bet back -DER_NONEXIST instead
 * if rank updated membership list but group version hasnt changed yet
 */
		D_ASSERTF((cb_info->cci_rc == -DER_MISMATCH),
			  "cb_info->cci_rc %d\n", cb_info->cci_rc);
		corpc_ver_mismatch_cb(rpc_req);
		break;
	case TEST_OPC_RANK_EVICT:
		rank_evict_cb(rpc_req);
		break;
	case TEST_OPC_SHUTDOWN:
		sem_post(&test.t_token);
		break;
	default:
		break;
	}
}

static void
test_rank_conversion(void)
{
	d_rank_t	rank_out;
	int		rc = 0;

	rc = crt_group_rank_p2s(test.t_sub_group, 2, &rank_out);
	D_ASSERT(rc == 0);
	D_ASSERT(rank_out == 3);

	rc = crt_group_rank_s2p(test.t_sub_group, 0, &rank_out);
	D_ASSERT(rc == 0);
	D_ASSERT(rank_out == 4);
}

void
dummy_barrier_cb(struct crt_barrier_cb_info *cb_info)
{
}

void
sub_grp_ping_issue(struct crt_barrier_cb_info *cb_info)
{
	crt_endpoint_t			 server_ep;
	crt_rpc_t			*rpc_req = NULL;
	struct subgrp_ping_in		*rpc_req_input;
	int				 rc = 0;

	/* send an RPC to a subgroup rank */
	server_ep.ep_grp = test.t_sub_group;
	server_ep.ep_rank = 1;
	server_ep.ep_tag = 0;
	rc = crt_req_create(test.t_crt_ctx, &server_ep, TEST_OPC_SUBGRP_PING,
			    &rpc_req);
	D_ASSERTF(rc == 0 && rpc_req != NULL,
		  "crt_req_create() failed, rc: %d rpc_req: %p\n",
		  rc, rpc_req);
	rpc_req_input = crt_req_get(rpc_req);
	D_ASSERTF(rpc_req_input != NULL, "crt_req_get() failed. "
		  "rpc_req_input: %p\n", rpc_req_input);
	rpc_req_input->magic = 1234;
	rc = crt_req_send(rpc_req, client_cb, NULL);
	D_ASSERTF(rc == 0, "crt_req_send() failed, rc %d\n", rc);

	D_DEBUG(DB_TEST, "exiting\n");
}

static void
test_run(void)
{
	crt_group_id_t		sub_grp_id = "example_grpid";
	bool			is_member = false;
	int			i;
	int			rc = 0;

	if (test.t_my_group_size < 5) {
		DBG_PRINT("need at least 5 ranks, exiting\n");
		D_GOTO(out, rc);
	}

	for (i = 0; i < 4; i++)
		if (test.t_my_rank == real_ranks[i])
			is_member = true;

	if (!is_member) {
		DBG_PRINT("I am not a sub-group member\n");
		for (;;) {
			rc = crt_barrier(NULL, dummy_barrier_cb, NULL);
			if (rc != -DER_BUSY)
				break;
			sched_yield();
		}
		DBG_PRINT("created barrier.\n");
		D_GOTO(out, rc);
	}
	/* only subgroup members execute the following code */

	/* root: rank 3, participants: rank 1, rank 2, rank 4 */
	rc = crt_group_secondary_create(sub_grp_id, test.t_primary_group, NULL,
					&test.t_sub_group);
	if (rc != 0) {
		D_ERROR("crt_group_secondary_create() failed; rc=%d\n", rc);
		D_ASSERT(0);
	}
	D_ASSERT(test.t_sub_group != NULL);
	test.t_sub_group = crt_group_lookup("example_grpid");
	D_ASSERT(test.t_sub_group != NULL);

	for (i = 0 ; i < 4; i++) {
		rc = crt_group_secondary_rank_add(test.t_sub_group,
					sec_ranks[i], real_ranks[i]);
		if (rc != 0) {
			D_ERROR("Rank addition failed; rc=%d\n", rc);
			D_ASSERT(0);
		}
	}

	/* test rank conversion */
	test_rank_conversion();

	if (test.t_my_rank == 3) {
		for (;;) {
			crt_barrier(NULL, sub_grp_ping_issue, NULL);
			if (rc != -DER_BUSY)
				break;
			sched_yield();
		}
	} else {
		for (;;) {
			rc = crt_barrier(NULL, dummy_barrier_cb, NULL);
			if (rc != -DER_BUSY)
				break;
			sched_yield();
		}
	}
	DBG_PRINT("created barrier.\n");

out:

	if (test.t_my_rank == 3) {
		for (i = 0; i < test.t_my_group_size - 1; i++)
			sem_wait(&test.t_token);
		atomic_store_release(&test.t_shutdown, 1);
	}
	D_ASSERT(rc == 0);
}

struct crt_proto_rpc_format my_proto_rpc_fmt_corpc[] = {
	{
		.prf_flags      = 0,
		.prf_req_fmt    = &CQF_corpc_ver_mismatch,
		.prf_hdlr       = corpc_ver_mismatch_hdlr,
		.prf_co_ops     = &corpc_ver_mismatch_ops,
	}
};

struct crt_proto_format my_proto_fmt_corpc = {
	.cpf_name = "my-proto-corpc",
	.cpf_ver = TEST_CORPC_VER,
	.cpf_count = ARRAY_SIZE(my_proto_rpc_fmt_corpc),
	.cpf_prf = &my_proto_rpc_fmt_corpc[0],
	.cpf_base = TEST_CORPC_BASE1,
};

static struct crt_proto_rpc_format my_proto_rpc_fmt_srv[] = {
	{
		.prf_flags	= 0,
		.prf_req_fmt    = NULL,
		.prf_hdlr       = test_shutdown_hdlr,
		.prf_co_ops     = NULL,
	}, {
		.prf_flags      = 0,
		.prf_req_fmt    = &CQF_rank_evict,
		.prf_hdlr       = test_rank_evict_hdlr,
		.prf_co_ops     = NULL,
	}, {
		.prf_flags      = 0,
		.prf_req_fmt    = &CQF_subgrp_ping,
		.prf_hdlr       = subgrp_ping_hdlr,
		.prf_co_ops     = NULL,
	}
};

static struct crt_proto_format my_proto_fmt_srv = {
	.cpf_name = "my-proto-srv",
	.cpf_ver = TEST_CORPC_VER,
	.cpf_count = ARRAY_SIZE(my_proto_rpc_fmt_srv),
	.cpf_prf = &my_proto_rpc_fmt_srv[0],
	.cpf_base = TEST_CORPC_BASE2,
};


void
test_init(void)
{
	uint32_t	 flag;
	char		*env_self_rank;
	d_rank_t	 my_rank;
	d_rank_list_t	*rank_list;
	char		*grp_cfg_file;
	int		 rc = 0;

	env_self_rank = getenv("CRT_L_RANK");
	if (env_self_rank == NULL) {
		printf("CRT_L_RANK was not set\n");
		return;
	}

	my_rank = atoi(env_self_rank);

	/* rank, num_attach_retries, is_server, assert_on_error */
	tc_test_init(my_rank, 20, true, true);
	rc = d_log_init();
	D_ASSERT(rc == 0);

	D_DEBUG(DB_TEST, "primary group: %s\n", test.t_primary_group_name);

	flag = CRT_FLAG_BIT_SERVER | CRT_FLAG_BIT_PMIX_DISABLE;
	rc = crt_init(test.t_primary_group_name, flag);
	D_ASSERTF(rc == 0, "crt_init() failed, rc: %d\n", rc);

	rc = crt_rank_self_set(my_rank);
	D_ASSERT(rc == 0);

	test.t_primary_group = crt_group_lookup(test.t_primary_group_name);
	D_ASSERTF(test.t_primary_group != NULL, "crt_group_lookup() failed. "
		  "primary_group = %p\n", test.t_primary_group);

	rc = crt_proto_register(&my_proto_fmt_corpc);
	D_ASSERTF(rc == 0, "crt_proto_register() for corpc failed, rc: %d\n",
		rc);

	rc = crt_proto_register(&my_proto_fmt_srv);
	D_ASSERTF(rc == 0, "crt_rpc_srv_register() failed, rc: %d\n", rc);

	rc = crt_context_create(&test.t_crt_ctx);
	D_ASSERTF(rc == 0, "crt_context_create() failed. rc: %d\n", rc);

	rc = pthread_create(&test.t_tid, NULL, progress_thread,
			    test.t_crt_ctx);
	D_ASSERTF(rc == 0, "pthread_create() failed. rc: %d\n", rc);


	grp_cfg_file = getenv("CRT_L_GRP_CFG");
	if (grp_cfg_file == NULL) {
		D_ERROR("CRT_L_GRP_CFG was not set\n");
		D_ASSERT(0);
	}

	D_DEBUG(DB_TEST, "grp_cfg_file is %s\n", grp_cfg_file);
	rc = tc_load_group_from_file(grp_cfg_file, test.t_crt_ctx,
			test.t_primary_group, my_rank, false);
	if (rc != 0) {
		D_ERROR("Failed to load group file %s\n", grp_cfg_file);
		D_ASSERT(0);
	}

	rc = crt_group_rank(NULL, &test.t_my_rank);
	D_ASSERTF(rc == 0, "crt_group_rank() failed, rc: %d\n", rc);
	D_DEBUG(DB_TEST, "primary rank is %d\n", test.t_my_rank);

	test.t_my_pid = getpid();

	rc = crt_group_size(NULL, &test.t_my_group_size);
	D_ASSERTF(rc == 0, "crt_group_size() failed. rc: %d\n", rc);
	D_DEBUG(DB_TEST, "primary group size is %d\n", test.t_my_group_size);

	rc = crt_group_ranks_get(test.t_primary_group, &rank_list);
	D_ASSERT(rc == 0);
	rc = tc_wait_for_ranks(test.t_crt_ctx, test.t_primary_group, rank_list,
			       0, 1, 5, 120);
	D_ASSERT(rc == 0);

	rc = sem_init(&test.t_token, 0, 0);
	D_ASSERTF(rc == 0, "Could not initialize semaphore\n");

}

void
test_fini()
{
	int		i;
	int		rc;

	rc = pthread_join(test.t_tid, NULL);
	D_ASSERTF(rc == 0, "pthread_join() failed, rc: %d\n", rc);

	for (i = 0; i < 4; i++) {
		if (test.t_my_rank != real_ranks[i])
			continue;
		rc = crt_group_secondary_destroy(test.t_sub_group);
		DBG_PRINT("crt_group_secondary_destroy rc: %d, arg %p.\n",
			  rc, &test.t_my_rank);
	}

	crt_swim_fini();
	rc = crt_context_flush(test.t_crt_ctx, 60);
	D_ASSERTF(rc == DER_SUCCESS || rc == -DER_TIMEDOUT,
		  "crt_context_destroy() failed. rc: %d\n", rc);

	rc = crt_context_destroy(test.t_crt_ctx, 0);
	D_ASSERTF(rc == 0, "crt_context_destroy() failed. rc: %d\n", rc);

	rc = crt_finalize();
	D_ASSERTF(rc == 0, "crt_finalize() failed. rc: %d\n", rc);

	d_log_fini();
}

int main(int argc, char **argv)
{
	int		 rc;

	opts.is_server = 1;
	DBG_PRINT("here\n");
	rc = test_parse_args(argc, argv);
	if (rc != 0) {
		DBG_PRINT("test_parse_args() failed, rc: %d.\n", rc);
		return rc;
	}

	test_init();
	test_run();
	test_fini();

	return rc;
}
