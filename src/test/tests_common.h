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
 */
/**
 * Common functions to be shared among tests
 */
#ifndef __TESTS_COMMON_H__
#define __TESTS_COMMON_H__
#include <semaphore.h>
#include <cart/api.h>

#include "crt_internal.h"

struct wfr_status {
	sem_t	sem;
	int	rc;
	int	num_ctx;
};

static inline void
sync_timedwait(struct wfr_status *wfrs, int sec, int line_number)
{
	struct timespec	deadline;
	int		rc;

	rc = clock_gettime(CLOCK_REALTIME, &deadline);
	D_ASSERTF(rc == 0, "clock_gettime() failed at line %d rc: %d\n",
		  line_number, rc);

	deadline.tv_sec += sec;

	rc = sem_timedwait(&wfrs->sem, &deadline);
	D_ASSERTF(rc == 0, "Sync timed out at line %d rc: %d\n",
		  line_number, rc);
}

static void
ctl_client_cb(const struct crt_cb_info *info)
{
	struct wfr_status		*wfrs;
	struct crt_ctl_ep_ls_out	*out_ls_args;
	char				*addr_str;
	int				 i;

	wfrs = (struct wfr_status *)info->cci_arg;

	if (info->cci_rc == 0) {
		out_ls_args = crt_reply_get(info->cci_rpc);
		wfrs->num_ctx = out_ls_args->cel_ctx_num;
		wfrs->rc = out_ls_args->cel_rc;

		D_DEBUG(DB_TEST, "ctx_num: %d\n",
			out_ls_args->cel_ctx_num);
		addr_str = out_ls_args->cel_addr_str.iov_buf;
		for (i = 0; i < out_ls_args->cel_ctx_num; i++) {
			D_DEBUG(DB_TEST, "    %s\n", addr_str);
				addr_str += (strlen(addr_str) + 1);
		}
	} else {
		wfrs->rc = info->cci_rc;
	}

	sem_post(&wfrs->sem);
}

int
wait_for_ranks(crt_context_t ctx, crt_group_t *grp, d_rank_list_t *rank_list,
		int tag, int total_ctx, double ping_timeout,
		double total_timeout)
{
	struct wfr_status		ws;
	struct timespec			t1, t2;
	struct crt_ctl_ep_ls_in		*in_args;
	d_rank_t			rank;
	crt_rpc_t			*rpc = NULL;
	crt_endpoint_t			server_ep;
	double				time_s = 0;
	int				i = 0;
	int				rc = 0;

	rc = d_gettime(&t1);
	D_ASSERTF(rc == 0, "d_gettime() failed; rc=%d\n", rc);

	rc = sem_init(&ws.sem, 0, 0);
	D_ASSERTF(rc == 0, "sem_init() failed; rc=%d\n", rc);

	server_ep.ep_tag = tag;
	server_ep.ep_grp = grp;

	for (i = 0; i < rank_list->rl_nr; i++) {

		rank = rank_list->rl_ranks[i];

		server_ep.ep_rank = rank;

		rc = crt_req_create(ctx, &server_ep, CRT_OPC_CTL_LS, &rpc);
		D_ASSERTF(rc == 0, "crt_req_create failed; rc=%d\n", rc);

		in_args = crt_req_get(rpc);
		in_args->cel_grp_id = grp->cg_grpid;
		in_args->cel_rank = rank;

		rc = crt_req_set_timeout(rpc, ping_timeout);
		D_ASSERTF(rc == 0, "crt_req_set_timeout failed; rc=%d\n", rc);

		ws.rc = 0;
		ws.num_ctx = 0;

		rc = crt_req_send(rpc, ctl_client_cb, &ws);

		if (rc == 0)
			sync_timedwait(&ws, 120, __LINE__);
		else
			ws.rc = rc;

		while (ws.rc != 0 && time_s < total_timeout) {
			rc = crt_req_create(ctx, &server_ep,
					    CRT_OPC_CTL_LS, &rpc);
			D_ASSERTF(rc == 0,
				   "crt_req_create failed; rc=%d\n", rc);

			in_args = crt_req_get(rpc);
			in_args->cel_grp_id = grp->cg_grpid;
			in_args->cel_rank = rank;

			rc = crt_req_set_timeout(rpc, ping_timeout);
			D_ASSERTF(rc == 0,
				   "crt_req_set_timeout failed; rc=%d\n", rc);

			ws.rc = 0;
			ws.num_ctx = 0;

			rc = crt_req_send(rpc, ctl_client_cb, &ws);

			if (rc == 0)
				sync_timedwait(&ws, 120, __LINE__);
			else
				ws.rc = rc;

			rc = d_gettime(&t2);
			D_ASSERTF(rc == 0, "d_gettime() failed; rc=%d\n", rc);
			time_s = d_time2s(d_timediff(t1, t2));
		}

		if (ws.rc != 0) {
			rc = ws.rc;
			break;
		}

		if (ws.num_ctx < total_ctx) {
			rc = -1;
			break;
		}
	}

	sem_destroy(&ws.sem);

	return rc;
}

int
tc_load_group_from_file(const char *grp_cfg_file,
		crt_context_t ctx, crt_group_t *grp,
		d_rank_t my_rank, bool delete_file)
{
	FILE		*f;
	int		parsed_rank;
	char		parsed_addr[255];
	int		rc = 0;

	f = fopen(grp_cfg_file, "r");
	if (!f) {
		D_ERROR("Failed to open %s for reading\n", grp_cfg_file);
		D_GOTO(out, rc = DER_NONEXIST);
	}

	while (1) {
		rc = fscanf(f, "%d %s", &parsed_rank, parsed_addr);
		if (rc == EOF) {
			rc = 0;
			break;
		}

		if (parsed_rank == my_rank)
			continue;

		rc = crt_group_primary_rank_add(ctx, grp,
					parsed_rank, parsed_addr);

		if (rc != 0) {
			D_ERROR("Failed to add %d %s; rc=%d\n",
				parsed_rank, parsed_addr, rc);
			break;
		}
	}

	fclose(f);

	if (delete_file)
		unlink(grp_cfg_file);

out:
	return rc;
}

static inline void
tc_sem_timedwait(sem_t *sem, int sec, int line_number)
{
	struct timespec	deadline;
	int		rc;

	rc = clock_gettime(CLOCK_REALTIME, &deadline);
	D_ASSERTF(rc == 0, "clock_gettime() failed at line %d rc: %d\n",
		  line_number, rc);

	deadline.tv_sec += sec;
	rc = sem_timedwait(sem, &deadline);
	D_ASSERTF(rc == 0, "sem_timedwait() failed at line %d rc: %d\n",
		  line_number, rc);
}
#endif /* __TESTS_COMMON_H__ */
