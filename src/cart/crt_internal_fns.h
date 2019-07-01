/* Copyright (C) 2016-2019 Intel Corporation
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
 * This file is part of CaRT. It gives out the main CaRT internal function
 * declarations which are not included by other specific header files.
 */

#ifndef __CRT_INTERNAL_FNS_H__
#define __CRT_INTERNAL_FNS_H__

/** crt_init.c */
bool crt_initialized(void);

/** crt_register.c */
int crt_opc_map_create(unsigned int bits);
void crt_opc_map_destroy(struct crt_opc_map *map);
struct crt_opc_info *crt_opc_lookup(struct crt_opc_map *map, crt_opcode_t opc,
				    int locked);

/** crt_hg.c */
int
crt_na_class_get_addr(na_class_t *na_class,
		char *addr_str, na_size_t *str_size);

/** crt_context.c */
/* return values of crt_context_req_track, in addition to standard
 * gurt error values.
 */
enum {
	CRT_REQ_TRACK_IN_INFLIGHQ = 0,
	CRT_REQ_TRACK_IN_WAITQ,
};

int crt_context_req_track(struct crt_rpc_priv *rpc_priv);
bool crt_context_empty(int locked);
void crt_context_req_untrack(struct crt_rpc_priv *rpc_priv);
crt_context_t crt_context_lookup(int na_type, int ctx_idx);
crt_context_t crt_context_lookup_locked(int na_type, int ctx_idx);
void crt_rpc_complete(struct crt_rpc_priv *rpc_priv, int rc);
int crt_req_timeout_track(struct crt_rpc_priv *rpc_priv);
void crt_req_timeout_untrack(struct crt_rpc_priv *rpc_priv);
void crt_req_force_timeout(struct crt_rpc_priv *rpc_priv);
crt_context_t crt_context_lookup_prov(const char *interface, const char *prov,
	bool need_lock);

int crt_parse_na_type(int *na_type, const char *na_str);
int crt_context_na_type(crt_context_t crt_ctx);

/** some simple helper functions */

static inline bool
crt_is_service()
{
	return crt_gdata.cg_server;
}

static inline void
crt_bulk_desc_dup(struct crt_bulk_desc *bulk_desc_new,
		  struct crt_bulk_desc *bulk_desc)
{
	D_ASSERT(bulk_desc_new != NULL && bulk_desc != NULL);
	*bulk_desc_new = *bulk_desc;
}

void
crt_hdlr_proto_query(crt_rpc_t *rpc_req);

/* Internal API to sync timestamp with remote message */
uint64_t crt_hlc_get_msg(uint64_t msg);

static inline struct na_ofi_config *
crt_na_config_lookup(const char *ni_str, const char *na_str, bool need_lock)
{
	struct na_ofi_config	*na_conf = NULL;
	struct na_ofi_config	*na_conf_tmp = NULL;

	if (need_lock)
		D_RWLOCK_RDLOCK(&crt_na_ofi_config_rwlock);

	D_DEBUG(DB_ALL, "looking up interface: %s\n", ni_str);
	d_list_for_each_entry(na_conf_tmp, &crt_na_ofi_config_opt,
			      noc_link) {
		if (!strncmp(na_conf_tmp->noc_interface, ni_str,
			     INET_ADDRSTRLEN) &&
		    !strncmp(na_conf_tmp->noc_na_str, na_str, 64)) {
			na_conf = na_conf_tmp;
			D_DEBUG(DB_ALL, "yes match, found %s on %s (len: %zd) "
					"\n", na_conf_tmp->noc_na_str,
					na_conf_tmp->noc_interface,
					strlen(na_conf_tmp->noc_interface));
			break;
		}
		D_DEBUG(DB_ALL, "not a match, found %s on %s (len: %zd) "
			"want %s on %s (len %zd)\n", na_conf_tmp->noc_na_str,
			na_conf_tmp->noc_interface,
			strlen(na_conf_tmp->noc_interface),
			na_str, ni_str, strlen(ni_str));
	}
	if (need_lock)
		D_RWLOCK_UNLOCK(&crt_na_ofi_config_rwlock);

	return na_conf;
}

#endif /* __CRT_INTERNAL_FNS_H__ */
