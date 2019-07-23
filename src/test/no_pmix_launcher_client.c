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
 * Client utilizing crt_launch generated environment for NO-PMIX mode
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <cart/api.h>

#include "crt_internal.h"
#include "no_pmix_launcher_common.h"
#include "tests_common.h"

#define SCHAN_SRV_CTX 8

struct st_master_endpt {
        crt_endpoint_t endpt;
        struct crt_st_status_req_out reply;
};

static void
start_test_cb(const struct crt_cb_info *cb_info)
{
        /* Result returned to main thread */
        int32_t *return_status = cb_info->cci_arg;

        /* Status retrieved from the RPC result payload */
        int32_t *reply_status;

        /* Check the status of the RPC transport itself */
        if (cb_info->cci_rc != 0) {
                *return_status = cb_info->cci_rc;
                return;
        }

        /* Get the status from the payload */
        reply_status = (int32_t *)crt_reply_get(cb_info->cci_rpc);
        D_ASSERT(reply_status != NULL);

        /* Return whatever result we got to the main thread */
        *return_status = *reply_status;
}

static int wait_for_ranks(crt_context_t ctx, crt_group_t *grp, uint32_t grp_size,
                          d_rank_list_t *rank_list)
{
	struct st_master_endpt  *mep;
	int			i = 0;
	int 			tag;
	d_rank_t		rank;
	int			rc = 0;
	crt_rpc_t		*rpc = NULL;
	uint32_t		done;
	struct crt_st_start_params      *start_args = NULL;

        D_ALLOC_ARRAY(mep, grp_size*SCHAN_SRV_CTX);
        if (mep == NULL)
		return -DER_NOMEM;

	int index = 0;

        for (i = 0; i < rank_list->rl_nr; i++) {
		for (tag = 0; tag < SCHAN_SRV_CTX; tag++){

                	rank = rank_list->rl_ranks[i];

			printf("SCHAN15 - sending rpc to ep %d\n", index);

        	        mep[index].endpt.ep_rank = rank;
                	mep[index].endpt.ep_tag = tag;
                	mep[index].endpt.ep_grp = grp;

			crt_endpoint_t *endpt = &mep[index].endpt;

        	        /* Create and send a new RPC */
                	rc = crt_req_create(ctx, endpt, CRT_OPC_SELF_TEST_START, &rpc);
                	D_ASSERTF(rc == 0, "crt_req_create failed with rc = %d\n", rc);

                	rc = crt_req_set_timeout(rpc, 5);
                	D_ASSERTF(rc == 0, "crt_req_set_timeout failed with rc = %d",
                        	  rc);

			start_args = (struct crt_st_start_params *) crt_req_get(rpc);
			D_ASSERTF(start_args != NULL, "crt_req_get returned NULL\n");

                	/* Set the launch status to a known impossible value */
                	mep[index].reply.status = INT32_MAX;

                	rc = crt_req_send(rpc, start_test_cb, &mep[index].reply.status);
			if(rc != 0){
				D_ERROR("Failed to send start RPC to endpoint %u:%u; "
					"rc = %d\n", endpt->ep_rank, endpt->ep_tag,
					rc);
			}
		index++;
		}
	}

        do {
                /* Flag indicating all test launches have returned a status */
                done = 1;

		index = 0;

                /* Wait a bit for tests to finish launching */
                sched_yield();

                for (i = 0; i < rank_list->rl_nr; i++) {
	        	for (tag = 0; tag < SCHAN_SRV_CTX; tag++){
				rank = rank_list->rl_ranks[i];
	               		printf("SCHAN15 - waiting on ep %d\n", index);

                        	if (mep[index].reply.status == INT32_MAX) {
                                	/* No response yet... */
                                	done = 0;
                                	break;
                        	}
				index++;
			}
			if(done == 0)
				break;
		}
        } while (done != 1);

	return rc;
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
	crt_context_t		crt_ctx;
	crt_group_t		*grp;
	char			*grp_cfg_file;
	int			rc;
	sem_t			sem;
	pthread_t		progress_thread;
	crt_rpc_t		*rpc = NULL;
	struct RPC_PING_in	*input;
	crt_endpoint_t		server_ep;
	int			i;
	uint32_t		grp_size;
	d_rank_list_t		*rank_list;
	d_rank_t		rank;
	int			tag;

	/* Set up for DBG_PRINT */
	opts.self_rank = 0;
	opts.mypid = getpid();
	opts.is_server = 0;

	rc = d_log_init();
	assert(rc == 0);

	DBG_PRINT("Client starting up\n");

	rc = sem_init(&sem, 0, 0);
	if (rc != 0) {
		D_ERROR("sem_init() failed; rc=%d\n", rc);
		assert(0);
	}

	rc = crt_init(NULL, CRT_FLAG_BIT_SINGLETON | CRT_FLAG_BIT_PMIX_DISABLE |
			CRT_FLAG_BIT_LM_DISABLE);
	if (rc != 0) {
		D_ERROR("crt_init() failed; rc=%d\n", rc);
		assert(0);
	}

	rc = crt_proto_register(&my_proto_fmt);
	if (rc != 0) {
		D_ERROR("crt_proto_register() failed; rc=%d\n", rc);
		assert(0);
	}

	rc = crt_group_view_create("server_grp", &grp);
	if (!grp || rc != 0) {
		D_ERROR("Failed to create group view; rc=%d\n", rc);
		assert(0);
	}

	rc = crt_context_create(&crt_ctx);
	if (rc != 0) {
		D_ERROR("crt_context_create() failed; rc=%d\n", rc);
		assert(0);
	}

	rc = pthread_create(&progress_thread, 0,
				progress_function, &crt_ctx);
	assert(rc == 0);

	grp_cfg_file = getenv("CRT_L_GRP_CFG");
	DBG_PRINT("Client starting with cfg_file=%s\n", grp_cfg_file);

	/* load group info from a config file and delete file upon return */
	rc = tc_load_group_from_file(grp_cfg_file, grp, SCHAN_SRV_CTX,
				-1, true);
	if (rc != 0) {
		D_ERROR("tc_load_group_from_file() failed; rc=%d\n", rc);
		assert(0);
	}

	/* Give time for servers to start; need better way later on */
	//sleep(2);

	rc = crt_group_size(grp, &grp_size);
	if (rc != 0) {
		D_ERROR("crt_group_size() failed; rc=%d\n", rc);
		assert(0);
	}

	rc = crt_group_ranks_get(grp, &rank_list);
	if (rc != 0) {
		D_ERROR("crt_group_ranks_get() failed; rc=%d\n", rc);
		assert(0);
	}

	if (rank_list->rl_nr != grp_size) {
		D_ERROR("rank_list differs in size. expected %d got %d\n",
			grp_size, rank_list->rl_nr);
		assert(0);
	}

	rc = crt_group_psr_set(grp, rank_list->rl_ranks[0]);
	if (rc != 0) {
		D_ERROR("crt_group_psr_set() failed; rc=%d\n", rc);
		assert(0);
	}

	printf("\nSCHAN15 - Sync'ing all ep\n\n");
	rc = wait_for_ranks(crt_ctx, grp, grp_size, rank_list);
        if (rc != 0) {
                D_ERROR("wait_for_ranks() failed; rc=%d\n", rc);
                assert(0);
        }

	printf("SCHAN15 - Starting tests...\n");

	/* Cycle through all ranks and 8 tags and send rpc to each */
	for (i = 0; i < rank_list->rl_nr; i++) {

		rank = rank_list->rl_ranks[i];

		for (tag = 0; tag < SCHAN_SRV_CTX; tag++) {
			DBG_PRINT("Sending ping to %d:%d\n", rank, tag);

			server_ep.ep_rank = rank;
			server_ep.ep_tag = tag;
			server_ep.ep_grp = grp;

			rc = crt_req_create(crt_ctx, &server_ep,
					RPC_PING, &rpc);
			if (rc != 0) {
				D_ERROR("crt_req_create() failed; rc=%d\n",
					rc);
				assert(0);
			}

			input = crt_req_get(rpc);
			input->tag = tag;

			rc = crt_req_send(rpc, rpc_handle_reply, &sem);
			tc_sem_timedwait(&sem, 10, __LINE__);
			DBG_PRINT("Ping response from %d:%d\n", rank, tag);
		}
	}

	/* Send shutdown RPC to each server */
	for (i = 0; i < rank_list->rl_nr; i++) {

		rank = rank_list->rl_ranks[i];
		DBG_PRINT("Sending shutdown to rank=%d\n", rank);

		server_ep.ep_rank = rank;
		server_ep.ep_tag = 0;
		server_ep.ep_grp = grp;

		rc = crt_req_create(crt_ctx, &server_ep, RPC_SHUTDOWN,
				&rpc);
		if (rc != 0) {
			D_ERROR("crt_req_create() failed; rc=%d\n", rc);
			assert(0);
		}

		rc = crt_req_send(rpc, rpc_handle_reply, &sem);
		tc_sem_timedwait(&sem, 10, __LINE__);
		DBG_PRINT("RPC response received from rank=%d\n", rank);
	}

	D_FREE(rank_list->rl_ranks);
	D_FREE(rank_list);

	rc = crt_group_view_destroy(grp);
	if (rc != 0) {
		D_ERROR("crt_group_view_destroy() failed; rc=%d\n", rc);
		assert(0);
	}

	g_do_shutdown = true;
	pthread_join(progress_thread, NULL);

	sem_destroy(&sem);

	rc = crt_finalize();
	if (rc != 0) {
		D_ERROR("crt_finalize() failed with rc=%d\n", rc);
		assert(0);
	}

	DBG_PRINT("Client successfully finished\n");
	d_log_fini();
}
