/* Copyright (C) 2018 Intel Corporation
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
 * This file is part of CaRT. It implements the server side of the cart_ctl
 * command line utility.
 */

#include "crt_internal.h"

#define MAX_HOSTNAME_SIZE 1024

static int verify_ctl_in_args(struct crt_ctl_ep_ls_in *in_args)
{
	struct crt_grp_priv	*grp_priv;
	int			rc = 0;

	if (in_args->cel_grp_id == NULL) {
		D_ERROR("invalid parameter, NULL input grp_id.\n");
		D_GOTO(out, rc = -DER_INVAL);
	}
	if (crt_validate_grpid(in_args->cel_grp_id) != 0) {
		D_ERROR("srv_grpid contains invalid characters "
			"or is too long\n");
		D_GOTO(out, rc = -DER_INVAL);
	}
	grp_priv = crt_gdata.cg_grp->gg_srv_pri_grp;

	if (!crt_grp_id_identical(in_args->cel_grp_id,
				  grp_priv->gp_pub.cg_grpid)) {
		D_ERROR("RPC request has wrong grp_id: %s\n",
			in_args->cel_grp_id);
		D_GOTO(out, rc = -DER_INVAL);
	}
	if (in_args->cel_rank != grp_priv->gp_self) {
		D_ERROR("RPC request has wrong rank: %d\n", in_args->cel_rank);
		D_GOTO(out, rc = -DER_INVAL);
	}

out:
	return rc;
}

int
crt_ctl_fill_buffer_cb(d_list_t *rlink, void *arg)
{
	struct crt_uri_item	*ui;
	int			*idx;
	char			*buf;
	struct crt_uri_cache	*uri_cache = arg;
	int			 i;
	int			 uri_size;
	int			 rc = 0;

	D_ASSERT(rlink != NULL);
	D_ASSERT(arg != NULL);

	ui = crt_ui_link2ptr(rlink);

	D_MUTEX_LOCK(&ui->ui_mutex);
	for (i = 0; i < CRT_SRV_CONTEXT_NUM; i++) {
		if (ui->ui_uri[i] != NULL) {
			char *tmp_uri;

			tmp_uri = ui->ui_uri[i];

			idx = &uri_cache->info_idx;

			buf = uri_cache->info_buf;

			/* Pack rank */
			*((d_rank_t *)(buf + *idx)) = ui->ui_rank;
			*idx += sizeof(d_rank_t);

			/* Pack tag */
			*((uint32_t *)(buf + *idx)) = i;
			*idx += sizeof(uint32_t);

			/* Pack uri size */
			uri_size = strlen(tmp_uri) + 1;
			*((uint32_t *)(buf + *idx)) = uri_size;
			*idx += sizeof(uint32_t);

			/* Pack uri */
			snprintf(buf + *idx, uri_size, "%s", tmp_uri);
			*idx += uri_size;
		}
	}
	D_MUTEX_UNLOCK(&ui->ui_mutex);

	return rc;
}

int
crt_ctl_get_uri_cache_size_cb(d_list_t *rlink, void *arg)
{
	struct crt_uri_item	*ui;
	uint32_t		*total_size = arg;
	int			 i;
	int			 rc = 0;

	D_ASSERT(rlink != NULL);
	D_ASSERT(arg != NULL);

	ui = crt_ui_link2ptr(rlink);

	D_MUTEX_LOCK(&ui->ui_mutex);
	for (i = 0; i < CRT_SRV_CONTEXT_NUM; i++) {
		if (ui->ui_uri[i] != NULL) {
			char *tmp_uri;

			tmp_uri = ui->ui_uri[i];

			/* Allocate space for rank:tag:uri_size:uri */
			*total_size += sizeof(d_rank_t);
			*total_size += sizeof(uint32_t);
			*total_size += sizeof(uint32_t);
			*total_size += strlen(tmp_uri) + 1;
		}
	}
	D_MUTEX_UNLOCK(&ui->ui_mutex);

	return rc;
}

void
crt_hdlr_ctl_get_uri_cache(crt_rpc_t *rpc_req)
{
	struct crt_ctl_get_uri_cache_in		*in_args;
	struct crt_ctl_get_uri_cache_out	*out_args;
	int					 rc = 0;
	uint32_t				 total_size = 0;
	struct crt_grp_priv			*grp_priv;
	struct crt_uri_cache			 uri_cache;

	D_ASSERTF(crt_is_service(), "Must be called in a service process\n");
	in_args = crt_req_get(rpc_req);
	D_ASSERTF(in_args != NULL, "NULL input args\n");
	out_args = crt_reply_get(rpc_req);
	D_ASSERTF(out_args != NULL, "NULL output args\n");

	rc = verify_ctl_in_args(crt_req_get(rpc_req));
	if (rc != 0)
		D_GOTO(out, rc);

	grp_priv = crt_gdata.cg_grp->gg_srv_pri_grp;

	rc = d_hash_table_traverse(&grp_priv->gp_uri_lookup_cache,
				   crt_ctl_get_uri_cache_size_cb, &total_size);
	if (rc != 0)
		D_ERROR("Get uri cache size failed, rc: %d.\n", rc);

	D_ALLOC(uri_cache.info_buf, total_size);
	if (uri_cache.info_buf == NULL)
		D_GOTO(out, rc = -DER_NOMEM);

	uri_cache.info_idx = 0;

	rc = d_hash_table_traverse(&grp_priv->gp_uri_lookup_cache,
				   crt_ctl_fill_buffer_cb, &uri_cache);
	if (rc != 0)
		D_ERROR("Fill uri cache buffer failed, rc: %d.\n", rc);

	d_iov_set(&out_args->cguc_grp_info, uri_cache.info_buf, total_size);

out:
	rc = crt_reply_send(rpc_req);
	D_ASSERTF(rc == 0, "crt_reply_send() failed. rc: %d\n", rc);
	D_DEBUG(DB_TRACE, "sent reply to get uri cache request\n");
	D_FREE(uri_cache.info_buf);
}

void
crt_hdlr_ctl_get_hostname(crt_rpc_t *rpc_req)
{
	struct crt_ctl_get_host_out	*out_args;
	char				hostname[MAX_HOSTNAME_SIZE];
	int				rc;

	out_args = crt_reply_get(rpc_req);
	rc = verify_ctl_in_args(crt_req_get(rpc_req));
	if (rc != 0)
		D_GOTO(out, rc);

	if (gethostname(hostname, MAX_HOSTNAME_SIZE) != 0) {
		D_ERROR("gethostname() failed with errno %d\n", errno);
		D_GOTO(out, rc = -DER_INVAL);
	}

	d_iov_set(&out_args->cgh_hostname, hostname, strlen(hostname));

out:
	out_args->cgh_rc = rc;
	rc = crt_reply_send(rpc_req);
	if (rc != 0)
		D_ERROR("crt_reply_send() failed with rc %d\n", rc);
}

void
crt_hdlr_ctl_get_pid(crt_rpc_t *rpc_req)
{
	struct crt_ctl_get_pid_out	*out_args;
	int				rc;

	out_args = crt_reply_get(rpc_req);
	rc = verify_ctl_in_args(crt_req_get(rpc_req));
	if (rc != 0)
		D_GOTO(out, rc);

	out_args->cgp_pid = getpid();

out:
	out_args->cgp_rc = rc;
	rc = crt_reply_send(rpc_req);
	if (rc != 0)
		D_ERROR("crt_reply_send() failed with rc %d\n", rc);
}

void
crt_hdlr_ctl_ls(crt_rpc_t *rpc_req)
{
	struct crt_ctl_ep_ls_in		*in_args;
	struct crt_ctl_ep_ls_out	*out_args;
	char				 addr_str[CRT_ADDR_STR_MAX_LEN]
						= {'\0'};
	size_t				 str_size;
	char				*addr_buf = NULL;
	uint32_t			 addr_buf_len;
	int				 count;
	struct crt_context		*ctx = NULL;
	int				 rc = 0;

	D_ASSERTF(crt_is_service(), "Must be called in a service process\n");
	in_args = crt_req_get(rpc_req);
	D_ASSERTF(in_args != NULL, "NULL input args\n");
	out_args = crt_reply_get(rpc_req);
	D_ASSERTF(out_args != NULL, "NULL output args\n");

	rc = verify_ctl_in_args(in_args);
	if (rc != 0)
		D_GOTO(out, rc);


	out_args->cel_ctx_num = crt_gdata.cg_ctx_num;
	D_DEBUG(DB_TRACE, "out_args->cel_ctx_num %d\n", crt_gdata.cg_ctx_num);
	addr_buf_len = 0;

	D_RWLOCK_RDLOCK(&crt_gdata.cg_rwlock);

	d_list_for_each_entry(ctx, &crt_gdata.cg_ctx_list, cc_link) {
		str_size = CRT_ADDR_STR_MAX_LEN;

		D_MUTEX_LOCK(&ctx->cc_mutex);
		rc = crt_hg_get_addr(ctx->cc_hg_ctx.chc_hgcla, NULL, &str_size);
		D_MUTEX_UNLOCK(&ctx->cc_mutex);
		if (rc != 0) {
			D_RWLOCK_UNLOCK(&crt_gdata.cg_rwlock);
			D_ERROR("context (idx %d), crt_hg_get_addr failed rc: "
				"%d.\n", ctx->cc_idx, rc);
			D_GOTO(out, rc);
		}
		addr_buf_len += str_size;
	}

	D_ALLOC(addr_buf, addr_buf_len);
	if (addr_buf == NULL) {
		D_RWLOCK_UNLOCK(&crt_gdata.cg_rwlock);
		D_GOTO(out, rc = -DER_NOMEM);
	}

	count = 0;

	d_list_for_each_entry(ctx, &crt_gdata.cg_ctx_list, cc_link) {
		str_size = CRT_ADDR_STR_MAX_LEN;
		rc = 0;

		pthread_mutex_lock(&ctx->cc_mutex);
		rc = crt_hg_get_addr(ctx->cc_hg_ctx.chc_hgcla, addr_str,
				     &str_size);
		pthread_mutex_unlock(&ctx->cc_mutex);

		if (rc != 0) {
			D_ERROR("context (idx %d), crt_hg_get_addr failed rc: "
				"%d.\n", ctx->cc_idx, rc);
			break;
		}

		count += snprintf(addr_buf + count, addr_buf_len - count,
				  "%s", addr_str);
		count += 1;
	}

	D_RWLOCK_UNLOCK(&crt_gdata.cg_rwlock);
	D_ASSERT(count <= addr_buf_len);

	d_iov_set(&out_args->cel_addr_str, addr_buf, count);

out:
	out_args->cel_rc = rc;
	rc = crt_reply_send(rpc_req);
	D_ASSERTF(rc == 0, "crt_reply_send() failed. rc: %d\n", rc);
	D_DEBUG(DB_TRACE, "sent reply to endpoint list request\n");
	D_FREE(addr_buf);
}
