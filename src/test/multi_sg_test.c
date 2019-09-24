/* Copyright (C) 2018-2019 Intel Corporation
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
 * Dynamic group testing for primary and secondary groups
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <cart/api.h>

#include <libpmemobj.h>


#include "tests_common.h"
#define LEN1 (1024*1024)
#define LEN2 (2*1024*1024)
#define GAP (1024*4096)

#define NUM_IOVS 2
#define TOTAL_BUFF (15*1024 * 1024)
#define POOL_SIZE (1024*1024*1024)

#define DATA_BUFF1 'a'
#define DATA_BUFF2 'e'


struct test_options {
	int		self_rank;
	int		mypid;
};

static struct test_options opts;
static int g_do_shutdown;

#define DBG_PRINT(x...)							\
	do {								\
		fprintf(stderr, "SRV [rank=%d pid=%d]\t",		\
			opts.self_rank,					\
			opts.mypid);					\
		fprintf(stderr, x);					\
	} while (0)

#define MY_BASE 0x010000000
#define MY_VER  0

#define NUM_SERVER_CTX 8

#define RPC_DECLARE(name)						\
	CRT_RPC_DECLARE(name, CRT_ISEQ_##name, CRT_OSEQ_##name)		\
	CRT_RPC_DEFINE(name, CRT_ISEQ_##name, CRT_OSEQ_##name)

enum {
	RPC_PING = CRT_PROTO_OPC(MY_BASE, MY_VER, 0),
	CORPC_PING,
	RPC_SHUTDOWN
} rpc_id_t;


#define CRT_ISEQ_RPC_PING	/* input fields */		 \
	((crt_bulk_t)		(bulk_hdl)		CRT_VAR)

#define CRT_OSEQ_RPC_PING	/* output fields */		 \
	((uint64_t)		(field)			CRT_VAR)

#define CRT_ISEQ_RPC_SHUTDOWN	/* input fields */		 \
	((uint64_t)		(field)			CRT_VAR)

#define CRT_OSEQ_RPC_SHUTDOWN	/* output fields */		 \
	((uint64_t)		(field)			CRT_VAR)

#define CRT_ISEQ_CORPC_PING \
	((uint64_t)		(field)			CRT_VAR)

#define CRT_OSEQ_CORPC_PING \
	((uint64_t)		(field)			CRT_VAR)


RPC_DECLARE(RPC_PING);
RPC_DECLARE(RPC_SHUTDOWN);
RPC_DECLARE(CORPC_PING);

static int
handler_corpc_ping(crt_rpc_t *rpc)
{
	DBG_PRINT("CORPC_HANDLER called\n");
	crt_reply_send(rpc);

	return 0;
}

static char output_buff1[LEN1];
static char output_buff2[LEN2];

static int
bulk_done_cb(const struct crt_bulk_cb_info *info)
{

	int i;

	DBG_PRINT("bulk transfer is done\n");

	for (i = 0; i < LEN1; i++) {
		if (output_buff1[i] != DATA_BUFF1) {	
			D_ERROR("Wrong character data in output; i=%d\n", i);
			D_ERROR("Expected %x got %x\n", DATA_BUFF1, output_buff1[i]);
			assert(0);
		}
	}
	DBG_PRINT("Data of buff1 verified\n");

	for (i = 0; i < LEN2; i++) {
		if (output_buff2[i] != DATA_BUFF2) {
			D_ERROR("Wrong character data in output; i=%d\n", i);
			D_ERROR("Expected %x got %x\n", DATA_BUFF2, output_buff2[i]);
			assert(0);
		}

	}

	DBG_PRINT("Data of buff2 verified\n");
	crt_reply_send(info->bci_bulk_desc->bd_rpc);
	RPC_PUB_DECREF(info->bci_bulk_desc->bd_rpc);


	return 0;
}

static int
handler_ping(crt_rpc_t *rpc)
{
	struct RPC_PING_in	*input;
	crt_bulk_t		remote_bulk_hdl;
	crt_bulk_t		local_bulk_hdl;
	crt_bulk_opid_t		op_id;
	struct crt_bulk_desc	bulk_desc;
	d_sg_list_t		sgl;
	d_iov_t			iovs[2];
	int rc;
	unsigned int		bulk_sgnum;

	input = crt_req_get(rpc);
	remote_bulk_hdl = input->bulk_hdl;

	iovs[0].iov_buf = output_buff1;
	iovs[0].iov_buf_len = LEN1;
	iovs[0].iov_len = LEN1;

	iovs[1].iov_buf = output_buff2;
	iovs[1].iov_buf_len = LEN2;
	iovs[1].iov_len = LEN2;

	sgl.sg_nr = NUM_IOVS;
	sgl.sg_iovs = iovs;

	memset(output_buff1, 0x0, LEN1);
	memset(output_buff2, 0x0, LEN2);
	
	rc = crt_bulk_get_sgnum(remote_bulk_hdl, &bulk_sgnum);
	if (rc != 0) {
		D_ERROR("crt_bulk_get_sgnum() failed; rc = %d\n", rc);
		assert(0);
	}

	DBG_PRINT("Recevied bulk with sgnum=%d\n", bulk_sgnum);

	rc = crt_bulk_create(rpc->cr_ctx, &sgl, CRT_BULK_RW, &local_bulk_hdl);
	if (rc != 0) {
		D_ERROR("crt_bulk_create() failed; rc = %d\n", rc);
		assert(0);
	}

	RPC_PUB_ADDREF(rpc);
	bulk_desc.bd_rpc = rpc;
	bulk_desc.bd_bulk_op = CRT_BULK_GET;
	bulk_desc.bd_remote_hdl = remote_bulk_hdl;
	bulk_desc.bd_remote_off = 0;
	bulk_desc.bd_local_hdl = local_bulk_hdl;
	bulk_desc.bd_local_off = 0;
	bulk_desc.bd_len = LEN1 + LEN2;

	rc = crt_bulk_transfer(&bulk_desc, bulk_done_cb, NULL,  &op_id);
	if (rc != 0) {
		D_ERROR("crt_bulk_transfer() failed; rc=%d\n", rc);
		assert(0);
	}
	
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
static int
corpc_aggregate(crt_rpc_t *src, crt_rpc_t *result, void *priv)
{
	struct CORPC_PING_out	*output_src;
	struct CORPC_PING_out	*output_result;


	output_src = crt_reply_get(src);
	output_result = crt_reply_get(result);

	output_result->field = output_src->field;
	return 0;
}

struct crt_corpc_ops corpc_ping_ops = {
	.co_aggregate = corpc_aggregate,
	.co_pre_forward = NULL,
};

struct crt_proto_rpc_format my_proto_rpc_fmt[] = {
	{
		.prf_flags	= 0,
		.prf_req_fmt	= &CQF_RPC_PING,
		.prf_hdlr	= (void *)handler_ping,
		.prf_co_ops	= NULL,
	}, {
		.prf_flags	= 0,
		.prf_req_fmt	= &CQF_CORPC_PING,
		.prf_hdlr	= (void *)handler_corpc_ping,
		.prf_co_ops	= &corpc_ping_ops,
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


static void *
progress_function(void *data)
{
	int i;
	crt_context_t *p_ctx = (crt_context_t *)data;

	while (g_do_shutdown == 0)
		crt_progress(*p_ctx, 1000, NULL, NULL);

	/* Progress contexts for a while after shutdown to send response */
	for (i = 0; i < 1000; i++)
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
	crt_context_t		crt_ctx[NUM_SERVER_CTX];
	pthread_t		progress_thread[NUM_SERVER_CTX];
	int			i;
	char			*my_uri;
	char			*env_self_rank;
	d_rank_t		my_rank;
	char			*grp_cfg_file;
	uint32_t		grp_size;
	crt_endpoint_t		server_ep;
	crt_rpc_t		*rpc;
	struct RPC_PING_in	*input;
	sem_t			sem;
	int			rc;

	env_self_rank = getenv("CRT_L_RANK");
	my_rank = atoi(env_self_rank);

	/* Set up for DBG_PRINT */
	opts.self_rank = my_rank;
	opts.mypid = getpid();

	rc = d_log_init();
	assert(rc == 0);

	DBG_PRINT("Server starting up\n");
	rc = crt_init(NULL, CRT_FLAG_BIT_SERVER |
		CRT_FLAG_BIT_PMIX_DISABLE | CRT_FLAG_BIT_LM_DISABLE);
	if (rc != 0) {
		D_ERROR("crt_init() failed; rc=%d\n", rc);
		assert(0);
	}

	rc = crt_proto_register(&my_proto_fmt);
	if (rc != 0) {
		D_ERROR("crt_proto_register() failed; rc=%d\n", rc);
		assert(0);
	}

	grp = crt_group_lookup(NULL);
	if (!grp) {
		D_ERROR("Failed to lookup group\n");
		assert(0);
	}

	for (i = 0; i < NUM_SERVER_CTX; i++) {
		rc = crt_context_create(&crt_ctx[i]);
		if (rc != 0) {
			D_ERROR("crt_context_create() failed; rc=%d\n", rc);
			assert(0);
		}

		rc = pthread_create(&progress_thread[i], 0,
				progress_function, &crt_ctx[i]);
		assert(rc == 0);
	}

	grp_cfg_file = getenv("CRT_L_GRP_CFG");

	rc = crt_rank_self_set(my_rank);
	if (rc != 0) {
		D_ERROR("crt_rank_self_set(%d) failed; rc=%d\n",
			my_rank, rc);
		assert(0);
	}

	rc = crt_rank_uri_get(grp, my_rank, 0, &my_uri);
	if (rc != 0) {
		D_ERROR("crt_rank_uri_get() failed; rc=%d\n", rc);
		assert(0);
	}

	/* load group info from a config file and delete file upon return */
	rc = tc_load_group_from_file(grp_cfg_file, crt_ctx[0], grp, my_rank,
					true);
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

	DBG_PRINT("Sleeping 5 secons to let every server come up\n");
	/* TODO: Give time for all servers to start up */
	sleep(5);

	rc = sem_init(&sem, 0, 0);
	if (rc != 0) {
		D_ERROR("sem_init() failed; rc=%d\n", rc);
		assert(0);
	}
	if (my_rank == 0) {

		char *input_buff;
		crt_bulk_t bulk_hdl;
		d_sg_list_t	sgl;
		d_iov_t		iovs[2];
		char	*buff1;
		char	*buff2;
		PMEMobjpool *my_pool;
		PMEMoid root_oid;


		my_pool = pmemobj_create("/mnt/daos0/alexpool", "test_pool", POOL_SIZE, 0666);
		DBG_PRINT("pmemobj_create returned %p ptr for pool \n", my_pool);
		root_oid = pmemobj_root(my_pool, TOTAL_BUFF);
		DBG_PRINT("After pmemboj_root\n");
		input_buff = pmemobj_direct(root_oid);

//		input_buff = malloc(TOTAL_BUFF);

		buff1 = input_buff;
		buff2  = input_buff + LEN1 + GAP;

		iovs[0].iov_buf = buff1;
		iovs[0].iov_buf_len = LEN1;
		iovs[0].iov_len = LEN1;

		iovs[1].iov_buf = buff2;
		iovs[1].iov_buf_len = LEN2;
		iovs[1].iov_len = LEN2;

		sgl.sg_nr = NUM_IOVS;
		sgl.sg_iovs = iovs;

		memset(buff1, DATA_BUFF1, LEN1);
		memset(buff2, DATA_BUFF2, LEN2);

		DBG_PRINT("Buff1[10] = %x\n", buff1[10]);
		DBG_PRINT("Buff2[10] = %x\n", buff2[10]);

		rc = crt_bulk_create(crt_ctx[0], &sgl, CRT_BULK_RW,
				&bulk_hdl);
		if (rc != 0) {
			D_ERROR("crt_bulk_create() failed; rc=%d\n", rc);
			assert(0);
		}

		server_ep.ep_rank = 1;
		server_ep.ep_tag = 0;
		server_ep.ep_grp = NULL;

		rc = crt_req_create(crt_ctx[0], &server_ep, RPC_PING, &rpc);
		if (rc != 0) {
			D_ERROR("crt_req_create() failed; rc=%d\n", rc);
			assert(0);
		}

		input = crt_req_get(rpc);


		input->bulk_hdl = bulk_hdl;
		
		rc = crt_req_send(rpc, rpc_handle_reply, &sem);
		tc_sem_timedwait(&sem, 10, __LINE__);
		DBG_PRINT("Ping response from %d:%d\n", 1, 0);

	//	free(input_buff);
	}

	for (i = 0; i < NUM_SERVER_CTX; i++)
		pthread_join(progress_thread[i], NULL);

	DBG_PRINT("Finished waiting for contexts\n");

	rc = crt_finalize();
	if (rc != 0) {
		D_ERROR("crt_finalize() failed with rc=%d\n", rc);
		assert(0);
	}

	DBG_PRINT("Finalized\n");
	d_log_fini();

	return 0;
}

