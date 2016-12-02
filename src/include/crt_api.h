/* Copyright (C) 2016 Intel Corporation
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
 * CaRT (Collective and RPC Transport) APIs
 */

#ifndef __CRT_API_H__
#define __CRT_API_H__

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <uuid/uuid.h>

#include <abt.h>

#include <crt_types.h>
#include <crt_errno.h>
#include <crt_iv.h>

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * Initialize CRT transport layer.
 *
 * \param cli_grpid [IN]	Client-side primary group ID, can be ignored for
 *				server-side. User can provide a NULL value in
 *				cases that CRT_DEFAULT_CLI_GRPID will be used.
 * \param srv_grpid [IN]	Server-side primary group ID, the client will attach
 *				to it when initializing. User can provide a NULL
 *				value in cases that CRT_DEFAULT_SRV_GRPID will be
 *				used.
 * \param server [IN]		Zero means pure client, otherwise will enable
 *				the server that listens for incoming connection
 *				requests.

 * \param flags [IN]		Bit flags, /see enum crt_init_flag_bits.
 *
 * \return			Zero on success, negative value if error.
 *
 * Notes: crt_init() is a collective call, which means every caller process
 *	  should make the call collectively, as now it will internally call
 *	  PMIx_Fence.
 */
int
crt_init(crt_group_id_t cli_grpid, crt_group_id_t srv_grpid, uint32_t flags);

/**
 * Create CRT transport context.
 *
 * \param arg [IN]		Input argument, now the only usage is passing the
 *				argobots pool pointer. If you do not use
 *				argobots, it should pass in NULL.
 * \param crt_ctx [OUT]		Created CRT transport context.
 *
 * \return			Zero on success, negative value if error.
 */
int
crt_context_create(void *arg, crt_context_t *crt_ctx);

/**
 * Destroy CRT transport context.
 *
 * \param crt_ctx [IN]          CRT transport context to be destroyed.
 * \param force [IN]            1) force == 0
 *                                 Return as -EBUSY if there is any in-flight
 *                                 RPC request, so the caller can wait its
 *                                 completion or timeout.
 *                              2) force != 0
 *                                 This will cancel all in-flight RPC requests.
 *
 * \return                      Zero on success, negative value if error.
 *
 * Notes: There is currently no in-flight list/queue in Mercury.
 */
int
crt_context_destroy(crt_context_t crt_ctx, int force);

/**
 * Query the index of the transport context, the index value ranges in
 * [0, ctx_num - 1].
 *
 * \param crt_ctx [IN]          CRT transport context.
 * \param ctx_idx [OUT]         Pointer to the returned index.
 *
 * \return                      Zero on success, negative value if error.
 */
int
crt_context_idx(crt_context_t crt_ctx, int *ctx_idx);

/**
 * Query the total number of the transport contexts.
 *
 * \param ctx_num [OUT]         Pointer to the returned number.
 *
 * \return                      Zero on success, negative value if error.
 */
int
crt_context_num(int *ctx_num);

/**
 * Finalize CRT transport layer.
 *
 * \return                      Zero on success, negative value if error.
 *
 * Notes: crt_finalize() is a collective call which means that every caller process
 *	  should make the call collectively, as it will now internally call
 *	  PMIx_Fence.
 */
int
crt_finalize(void);

/**
 * Progress CRT transport layer.
 *
 * \param crt_ctx	[IN]	CRT transport context
 * \param timeout	[IN]	How long the caller is going to wait (in micro-seconds).
 *				If \a timeout > 0 when there is no operation to
 *				progress. Can return when one or more operations
 *				progressed.
 *				Zero means no waiting and -1 waits indefinitely.
 * \param cond_cb	[IN]	Optional progress condition callback.
 *				CRT internally calls this function, when it
 *				returns non-zero then stops the progressing or
 *				waiting and returns.
 * \param arg		[IN]	Argument to cond_cb.
 *
 * \return			Zero on success, negative value if error.
 */
int
crt_progress(crt_context_t crt_ctx, int64_t timeout,
	     crt_progress_cond_cb_t cond_cb, void *arg);

/**
 * Create a RPC request.
 *
 * \param crt_ctx [IN]          CRT transport context.
 * \param tgt_ep [IN]           RPC target endpoint.
 * \param opc [IN]              RPC request opcode.
 * \param req [OUT]             Pointer to the created request.
 *
 * \return                      Zero on success, negative value if error.
 *
 * Notes: The crt_req_create will internally allocate buffers for input and
 *        output parameters (crt_rpc_t::dr_input and crt_rpc_t::dr_output), and
 *        sets the appropriate size (crt_rpc_t::dr_input_size/dr_output_size).
 *        You do not need to allocate extra input/output buffers. After the
 *        request is created, you can directly fill the input parameters into
 *        crt_rpc_t::dr_input and send the RPC request.
 *        When the RPC request finishes executing, CRT internally frees the
 *        RPC request and the input/output buffers, so you don't have to call
 *        crt_req_destroy (no such API exported) or free the input/output
 *        buffers.
 *        Similarly, on the RPC server-side, when an RPC request is received, CRT
 *        internally allocates input/output buffers as well, and internally
 *        frees those buffers when the reply is sent out. Therefore, your RPC
 *        handler does not need to allocate extra input/output buffers, and also
 *        does not need to free input/output buffers in the completion callback of
 *        crt_reply_send.
 */
int
crt_req_create(crt_context_t crt_ctx, crt_endpoint_t tgt_ep, crt_opcode_t opc,
	       crt_rpc_t **req);

/**
 * Add reference of the RPC request.
 *
 * The typical usage is that you need to do some asynchronous operations in
 * the RPC handler and do not want to block the RPC handler. You can call this
 * API to hold a reference and return. When the asynchronous operation is
 * complete, it can release the reference (/see crt_req_decref). CRT internally
 * frees the resource of the RPC request when its reference drops to zero.
 *
 * \param req [IN]              Pointer to RPC request.
 *
 * \return                      Zero on success, negative value if error.
 */
int
crt_req_addref(crt_rpc_t *req);

/**
 * Decrease reference of the RPC request. /see crt_req_addref.
 *
 * \param req [IN]              Pointer to RPC request.
 *
 * \return                      Zero on success, negative value if error.
 */
int
crt_req_decref(crt_rpc_t *req);

/**
 * Send an RPC request. In the case of a sending failure, CRT will internally destroy
 * the request \a req. When successful, the \a req will be internally
 * destroyed when the reply is received. You don't need to call crt_req_decref() to
 * destroy the request in either case.
 *
 * \param req [IN]              Pointer to the RPC request.
 * \param complete_cb [IN]      Completion callback, will be triggered when the
 *                              RPC request's reply arrives, in the context of
 *                              your calling of crt_progress().
 * \param arg [IN]              Arguments for the \a complete_cb.
 *
 * \return                      Zero on success, negative value if error.
 *
 * Notes: The crt_rpc_t is exported to the user, the caller should fill the
 *        crt_rpc_t::dr_input before sending the RPC request.
 *        \see crt_req_create.
 */
int
crt_req_send(crt_rpc_t *req, crt_cb_t complete_cb, void *arg);

/**
 * Send an RPC reply.
 *
 * \param req [IN]              Pointer to the RPC request.
 *
 * \return                      Zero on success, negative value if error.
 *
 * Notes: The crt_rpc_t is exported to the user, the caller should fill the
 *        crt_rpc_t::dr_output before sending the RPC reply.
 *        \see crt_req_create.
 */
int
crt_reply_send(crt_rpc_t *req);

/**
 * Return request buffer.
 *
 * \param req [IN]              Pointer to the RPC request.
 *
 * \return                      Pointer to the request buffer.
 */
static inline void *
crt_req_get(crt_rpc_t *rpc)
{
	return rpc->cr_input;
};

/**
 * Return reply buffer
 *
 * \param req [IN]              Pointer to the RPC request.
 *
 * \return                      Pointer to reply buffer.
 */
static inline void *
crt_reply_get(crt_rpc_t *rpc)
{
	return rpc->cr_output;
}

/**
 * Abort a RPC request.
 *
 * \param req [IN]              Pointer to the RPC request.
 *
 * \return                      Zero on success, negative value if error.
 *                              If the RPC has been sent out by crt_req_send,
 *                              the completion callback will be called with
 *                              CER_CANCELED set to crt_cb_info::dci_rc for
 *                              successful aborting.
 */
int
crt_req_abort(crt_rpc_t *req);

/**
 * Abort all in-flight RPC requests targeting to an endpoint.
 *
 * \param ep [IN]		Endpoint address.
 *
 * \return			Zero on success, negative value if error.
 */
int
crt_ep_abort(crt_endpoint_t ep);

/**
 * Dynamically register an RPC at the client-side.
 *
 * \param opc [IN]              Unique opcode for the RPC
 * \param drf [IN]		Pointer to the request format, which
 *                              describe the request format and provide
 *                              callback to pack/unpack each item in the
 *                              request.
 * \return                      Zero on success, negative value if error.
 */
int
crt_rpc_register(crt_opcode_t opc, struct crt_req_format *drf);

/**
 * Dynamically register an RPC at the server-side.
 *
 * \param opc [IN]              Unique opcode for the RPC.
 * \param drf [IN]		A pointer to the request format, which
 *                              describes the request format and provides

 *                              callback to pack/unpack each item in the
 *                              request.
 * \param rpc_handler [IN]      A pointer to the RPC handler which will be triggered
 *                              when an RPC request opcode associated with rpc_name
 *                              is received. Will return -CER_INVAL if pass in the
 *                              NULL rpc_handler.
 *
 * \return                      Zero on success, negative value if error.
 */
int
crt_rpc_srv_register(crt_opcode_t opc, struct crt_req_format *drf,
		crt_rpc_cb_t rpc_handler);

/******************************************************************************
 * CRT bulk APIs.
 ******************************************************************************/

/**
 * Create a bulk handle
 *
 * \param crt_ctx [IN]          CRT transport context.
 * \param sgl [IN]              A pointer to the buffer segment list.
 * \param bulk_perm [IN]        Bulk permission, \see crt_bulk_perm_t.
 * \param bulk_hdl [OUT]        The created bulk handle.
 *
 * \return                      Zero on success, negative value if error.
 */
int
crt_bulk_create(crt_context_t crt_ctx, crt_sg_list_t *sgl,
		crt_bulk_perm_t bulk_perm, crt_bulk_t *bulk_hdl);

/**
 * Access local bulk handle to retrieve the sgl (segment list) associated
 * with it.
 *
 * \param bulk_hdl [IN]         The bulk handle.
 * \param sgl[IN/OUT]           A pointer to the buffer segment list.
 *                              The caller should provide a valid sgl pointer. If
 *                              sgl->sg_nr.num is too small, -CER_TRUNC will be
 *                              returned and the needed number of iovs will be set at
 *                              sgl->sg_nr.num_out.
 *                              On success, sgl->sg_nr.num_out will be set as
 *                              the actual number of iovs.
 *
 * \return                      Zero on success, negative value if error.
 */
int
crt_bulk_access(crt_bulk_t bulk_hdl, crt_sg_list_t *sgl);

/**
 * Free a bulk handle
 *
 * \param bulk_hdl [IN]         The bulk handle to be freed.
 *
 * \return                      Zero on success, negative value if error.
 */
int
crt_bulk_free(crt_bulk_t bulk_hdl);

/**
 * Start a bulk transferring (inside an RPC handler).
 *
 * \param bulk_desc [IN]        A pointer to bulk transferring descriptor.
 *				it is your responsibility to allocate and free
 *				it. It can be freed after the calling returns.
 * \param complete_cb [IN]      Completion callback.
 * \param arg [IN]              Arguments for the \a complete_cb.
 * \param opid [OUT]            Returned bulk opid which can be used to abort
 *				the bulk. This is optional and can pass in NULL if
 *				it is not needed.

 *
 * \return                      Zero on success, negative value if error.
 */
int
crt_bulk_transfer(struct crt_bulk_desc *bulk_desc, crt_bulk_cb_t complete_cb,
		  void *arg, crt_bulk_opid_t *opid);

/**
 * Get length (number of bytes) of data abstracted by bulk handle.
 *
 * \param bulk_hdl [IN]         Bulk handle.
 * \param bulk_len [OUT]        Length of the data.
 *
 * \return                      Zero on success, negative value if error.
 */
int
crt_bulk_get_len(crt_bulk_t bulk_hdl, crt_size_t *bulk_len);

/**
 * Get the number of segments of data abstracted by bulk handle.
 *
 * \param bulk_hdl [IN]         Bulk handle.
 * \param bulk_sgnum [OUT]      Number of segments.
 *
 * \return                      Zero on success, negative value if error.
 */
int
crt_bulk_get_sgnum(crt_bulk_t bulk_hdl, unsigned int *bulk_sgnum);

/*
 * Abort a bulk transferring.
 *
 * \param crt_ctx [IN]          CRT transport context.
 * \param opid [IN]             Bulk opid.
 *
 * \return                      Zero on success, negative value if error.
 *                              If abort succeeds, the bulk transfer's completion
 *                              callback will be called with CER_CANCELED set to
 *                              crt_bulk_cb_info::bci_rc.
 */
int
crt_bulk_abort(crt_context_t crt_ctx, crt_bulk_opid_t opid);

/******************************************************************************
 * CRT group definition and collective APIs.
 ******************************************************************************/

/* Types for tree topology */
enum crt_tree_type {
	CRT_TREE_INVALID	= 0,
	CRT_TREE_MIN		= 1,
	CRT_TREE_FLAT		= 1,
	CRT_TREE_KARY		= 2,
	CRT_TREE_KNOMIAL	= 3,
	CRT_TREE_MAX		= 3,
};

#define CRT_TREE_TYPE_SHIFT	(16U)
#define CRT_TREE_MAX_RATIO	(64)
#define CRT_TREE_MIN_RATIO	(2)

/*
 * Calculate the tree topology.
 *
 * \param tree_type [IN]	Tree type.
 * \param branch_ratio [IN]	Branch ratio, can be ignored for CRT_TREE_FLAT.
 *				For KNOMIAL tree or KARY tree, the valid value
 *				should be within the range of
 *				[CRT_TREE_MIN_RATIO, CRT_TREE_MAX_RATIO], or
 *				it will be treated as an invalid parameter.
 *
 * \return			Tree topology value on success,
 *				negative value if error.
 */
static inline int
crt_tree_topo(enum crt_tree_type tree_type, uint32_t branch_ratio)
{
	if (tree_type < CRT_TREE_MIN || tree_type > CRT_TREE_MAX)
		return -CER_INVAL;

	return (tree_type << CRT_TREE_TYPE_SHIFT) |
	       (branch_ratio & ((1U << CRT_TREE_TYPE_SHIFT) - 1));
};

struct crt_corpc_ops {
	/*
	 * Collective RPC reply aggregating callback.
	 *
	 * \param source [IN]		The RPC structure of the aggregating source.
	 * \param result[IN]		The RPC structure of the aggregating result.
	 * \param priv [IN]		The private pointer, valid only on
	 *				collective the RPC initiator (same as the
	 *				priv pointer, passed in for
	 *				crt_corpc_req_create).
	 *
	 * \return			Zero on success, negative value if error.
	 */
	int (*co_aggregate)(crt_rpc_t *source, crt_rpc_t *result, void *priv);
};

/*
 * Group create completion callback.
 *
 * \param grp [IN]		Group handle, valid only when the group has been
 *				created successfully.
 * \param priv [IN]		A private pointer associated with the group
 *				(passed in for crt_group_create).
 * \param status [IN]		Status code that indicates whether the group has
 *				been created successfully or not.
 *				Zero for success, negative value otherwise.
 */
typedef int (*crt_grp_create_cb_t)(crt_group_t *grp, void *priv, int status);

/*
 * Group destroy completion callback.
 *
 * \param args [IN]		Arguments pointer passed in for
 *				crt_group_destroy.
 * \param status [IN]		Status code that indicates whether the group
 *				has been destroyed successfully or not.
 *				Zero for success, negative value otherwise.
 *
 */
typedef int (*crt_grp_destroy_cb_t)(void *args, int status);

/*
 * Create CRT sub-group (a subset of the primary group).
 *
 * \param grp_id [IN]		Unique group ID.
 * \param member_ranks [IN]	Rank list of members for the group.
 *				Can-only create the group on the node which is
 *				one member of the group, otherwise -CER_OOG will
 *				be returned.
 * \param populate_now [IN]	True if the group should be populated now;
 *				otherwise, group population will be piggybacked later
 *				on the first broadcast over the
 *				group.
 * \param grp_create_cb [IN]	Callback function to notify completion of the
 *				group creation process,
 *				\see crt_grp_create_cb_t.
 * \param priv [IN]		A private pointer associated with the group.
 *
 * \return			Zero on success, negative value if error.
 */
int
crt_group_create(crt_group_id_t grp_id, crt_rank_list_t *member_ranks,
		 bool populate_now, crt_grp_create_cb_t grp_create_cb,
		 void *priv);

/*
 * Lookup the group handle of one group ID (sub-group or primary group).
 *
 * For sub-group, its creation is initiated by one node, after the group is
 * populated (internally performed inside crt_group_create), you can query the
 * group handle (crt_group_t *) on other nodes.
 *
 * The primary group can be queried using the group ID passed to crt_init.
 * Some special cases:
 * 1) If (grp_id == NULL), the default local primary group ID, such as
 *    CRT_DEFAULT_CLI_GRPID for client and CRT_DEFAULT_SRV_GRPID for server.
 * 2) To query the attached remote service primary group, you can pass in its group ID.
 *    For the client-side, if it passed in NULL as crt_init's srv_grpid
 *    parameter, then use CRT_DEFAULT_SRV_GRPID to look up the attached
 *    service primary group handle.
 *
 * Notes: You can cache the returned group handle to avoid the overhead of
 *	  frequent look up.
 *
 * \param grp_id [IN]		Unique group ID.
 *
 * \return			Group handle on success, NULL if not found.
 */
crt_group_t *
crt_group_lookup(crt_group_id_t grp_id);

/*
 * Destroy a CRT group. You can either call this API or pass a special flag -
 * CRT_RPC_FLAG_GRP_DESTROY to a broadcast RPC to destroy the group.
 *
 * \param grp [IN]		Group handle to be destroyed.
 * \param grp_destroy_cb [IN]	Optional completion callback.
 * \param args [IN]		Optional args for \a grp_destroy_cb.
 *
 * \return			Zero on success, negative value if error.
 */
int
crt_group_destroy(crt_group_t *grp, crt_grp_destroy_cb_t grp_destroy_cb,
		  void *args);

/*
 * Attach to a primary service group.
 *
 * In crt_init(), the client will internally attach to the default service
 * primary group. You can pass crt_endpoint_t::ep_grp pointer as NULL to send
 * RPC to the default service primary group.
 * You can explicitly call this API to attach to another server tier, and set
 * crt_endpoint_t::ep_grp as the returned attached_grp to send RPC to that tier.
 *
 * \param srv_grpid [IN]	Primary service group ID to attach to.
 * \param attached_grp [OUT]	Returned attached group handle pointer.
 *
 * \return			Zero on success, negative value if error.
 */
int
crt_group_attach(crt_group_id_t srv_grpid, crt_group_t **attached_grp);

/*
 * Detach a primary service group which was attached previously.
 *
 * \param attached_grp [IN]	Attached primary service group handle.
 *
 * \return			Zero on success, negative value if error.
 */
int
crt_group_detach(crt_group_t *attached_grp);

/*
 * Create collective RPC request. Can reuse the crt_req_send to broadcast it.
 *
 * \param crt_ctx [IN]		CRT context.
 * \param grp [IN]		CRT group for the collective RPC.
 * \param excluded_ranks [IN]	Optional excluded ranks, the RPC will be
 *				delivered to all members in the group except
 *				those in excluded_ranks.
 *				The ranks in excluded_ranks are numbered in
 *				the primary group.
 * \param opc [IN]		Unique opcode for the RPC.
 * \param co_bulk_hdl [IN]	Collective bulk handle.
 * \param priv [IN]		A private pointer associated with the request
 *				will be passed to crt_corpc_ops::co_aggregate as
 *				2nd parameter.
 * \param flags [IN]		Collective RPC flags. For example, taking
 *				CRT_RPC_FLAG_GRP_DESTROY to destroy the group
 *				when this broadcast RPC has finished.
 * \param tree_topo[IN]		Tree topology for the collective propagation,
 *				can be calculated by crt_tree_topo().
 *				/see enum crt_tree_type, /see crt_tree_topo().
 * \param req [out]		Created collective RPC request.
 *
 * \return			Zero on success, negative value if error.
 */
int
crt_corpc_req_create(crt_context_t crt_ctx, crt_group_t *grp,
		     crt_rank_list_t *excluded_ranks, crt_opcode_t opc,
		     crt_bulk_t co_bulk_hdl, void *priv,  uint32_t flags,
		     int tree_topo, crt_rpc_t **req);

/**
 * Dynamically register a collective RPC.
 *
 * \param opc [IN]		Unique opcode for the RPC.
 * \param drf [IN]		Pointer to the request format, which
 *				describes the request format and provides
 *				callback to pack/unpack each item in the
 *				request.
 * \param rpc_handler [IN]	A pointer to the RPC handler which will be triggered
 *				when an RPC request opcode associated with rpc_name
 *				is received.
 * \param co_ops [IN]		A pointer to the corpc ops table.
 *
 * Notes:
 * 1) You can use crt_rpc_srv_reg to register collective RPC if no reply
 *    aggregation is needed.
 * 2) Can pass in a NULL drf or rpc_handler if it was registered already. This
 *    routine only overwrite if they are non-NULL.
 * 3) A NULL co_ops will be treated as invalid argument.
 *
 * \return			Zero on success, negative value if error.
 */
int
crt_corpc_register(crt_opcode_t opc, struct crt_req_format *drf,
		   crt_rpc_cb_t rpc_handler, struct crt_corpc_ops *co_ops);

/**
 * Query the caller's rank number within the group.
 *
 * \param grp [IN]		CRT group handle, NULL means the primary/global
 *				group.
 * \param rank[OUT]		Result rank number.
 *
 * \return			Zero on success, negative value if error.
 */
int
crt_group_rank(crt_group_t *grp, crt_rank_t *rank);

/**
 * Query number of group members.
 *
 * \param grp [IN]		CRT group handle, NULL means the local
 *				primary/global group.
 * \param size[OUT]		Size (total number of ranks) of the group.
 *
 * \return			Zero on success, negative value if error.
 */
int
crt_group_size(crt_group_t *grp, uint32_t *size);

/******************************************************************************
 * Proc data types, APIs and macros.
 ******************************************************************************/

typedef enum {
	/* causes the type to be encoded into the stream */
	CRT_PROC_ENCODE,
	/* causes the type to be extracted from the stream */
	CRT_PROC_DECODE,
	/* can be used to release the space allocated by CRT_DECODE request */
	CRT_PROC_FREE
} crt_proc_op_t;

/**
 * Get the operation type associated to the proc processor.
 *
 * \param proc [IN]             Abstract processor object.
 * \param proc_op [OUT]         Returned proc operation type.
 *
 * \return                      Zero on success, negative value if error.
 */
int
crt_proc_get_op(crt_proc_t proc, crt_proc_op_t *proc_op);

/**
 * Base proc routine using memcpy().
 * Only uses memcpy() / use crt_proc_raw() for encoding raw buffers.
 *
 * \param proc [IN/OUT]         Abstract processor object.
 * \param data [IN/OUT]         A pointer to data
 * \param data_size [IN]        Data size
 *
 * \return                      Zero on success, negative value if error.
 */
int
crt_proc_memcpy(crt_proc_t proc, void *data, crt_size_t data_size);

/**
 * Generic processing routine.
 *
 * \param proc [IN/OUT]         Abstract processor object.
 * \param data [IN/OUT]         A pointer to data.
 *
 * \return                      Zero on success, negative value if error.
 */
int
crt_proc_int8_t(crt_proc_t proc, int8_t *data);

/**
 * Generic processing routine.
 *
 * \param proc [IN/OUT]         Abstract processor object
 * \param data [IN/OUT]         A pointer to data
 *
 * \return                      Zero on success, negative value if error
 */
int
crt_proc_uint8_t(crt_proc_t proc, uint8_t *data);

/**
 * Generic processing routine.
 *
 * \param proc [IN/OUT]         Abstract processor object.
 * \param data [IN/OUT]         A pointer to data.
 *
 * \return                      Zero on success, negative value if error.
 */
int
crt_proc_int16_t(crt_proc_t proc, int16_t *data);

/**
 * Generic processing routine.
 *
 * \param proc [IN/OUT]         Abstract processor object.
 * \param data [IN/OUT]         A pointer to data
 *
 * \return                      Zero on success, negative value if error.
 */
int
crt_proc_uint16_t(crt_proc_t proc, uint16_t *data);

/**
 * Generic processing routine.
 *
 * \param proc [IN/OUT]         Abstract processor object.
 * \param data [IN/OUT]         A pointer to data.
 *
 * \return                      Zero on success, negative value if error.
 */
int
crt_proc_int32_t(crt_proc_t proc, int32_t *data);

/**
 * Generic processing routine.
 *
 * \param proc [IN/OUT]         Abstract processor object.
 * \param data [IN/OUT]         A pointer to data.
 *
 * \return                      Zero on success, negative value if error.
 */
int
crt_proc_uint32_t(crt_proc_t proc, uint32_t *data);

/**
 * Generic processing routine.
 *
 * \param proc [IN/OUT]         Abstract processor object.
 * \param data [IN/OUT]         A pointer to data.
 *
 * \return                      Zero on success, negative value if error.
 */
int
crt_proc_int64_t(crt_proc_t proc, int64_t *data);

/**
 * Generic processing routine.
 *
 * \param proc [IN/OUT]         Abstract processor object.
 * \param data [IN/OUT]         A pointer to data.
 *
 * \return                      Zero on success, negative value if error.
 */
int
crt_proc_uint64_t(crt_proc_t proc, uint64_t *data);

/**
 * Generic processing routine.
 *
 * \param proc [IN/OUT]         Abstract processor object.
 * \param data [IN/OUT]         A pointer to data.
 *
 * \return                      Zero on success, negative value if error.
 */
int
crt_proc_bool(crt_proc_t proc, bool *data);

/**
 * Generic processing routine.
 *
 * \param proc [IN/OUT]         Abstract processor object.
 * \param buf [IN/OUT]          A pointer to buffer.
 * \param buf_size [IN]         Buffer size.
 *
 * \return                      Zero on success, negative value if error.
 */
int
crt_proc_raw(crt_proc_t proc, void *buf, crt_size_t buf_size);

/**
 * Generic processing routine.
 *
 * \param proc [IN/OUT]         Abstract processor object.
 * \param bulk_hdl [IN/OUT]     A pointer to bulk handle.
 *
 * \return                      Zero on success, negative value if error.
 */
int
crt_proc_crt_bulk_t(crt_proc_t proc, crt_bulk_t *bulk_hdl);

/**
 * Generic processing routine.
 *
 * \param proc [IN/OUT]         Abstract processor object.
 * \param data [IN/OUT]         A pointer to data.
 *
 * \return                      Zero on success, negative value if error.
 */
int
crt_proc_crt_string_t(crt_proc_t proc, crt_string_t *data);

/**
 * Generic processing routine.
 *
 * \param proc [IN/OUT]         Abstract processor object.
 * \param data [IN/OUT]         A pointer to data.
 *
 * \return                      Zero on success, negative value if error.
 */
int
crt_proc_crt_const_string_t(crt_proc_t proc, crt_const_string_t *data);

/**
 * Generic processing routine.
 *
 * \param proc [IN/OUT]         Abstract processor object
 * \param data [IN/OUT]         A pointer to data
 *
 * \return                      Zero on success, negative value if error.
 */
int
crt_proc_uuid_t(crt_proc_t proc, uuid_t *data);

/**
 * Generic processing routine.
 *
 * \param proc [IN/OUT]         Abstract processor object.
 * \param data [IN/OUT]         A second level pointer to data.
 *
 * \return                      Zero on success, negative value if error.
 *
 * Notes:
 * 1) Here you can pass in the 2nd level pointer of crt_rank_list_t, making it
 *    possible to set it to NULL when decoding.
 * 2) If the rank_list is non-NULL, the caller should first duplicate it and then pass
 *    the duplicated rank list's 2nd level pointer as the parameter, because this
 *    function will internally free the memory when freeing the input or output.
 */
int
crt_proc_crt_rank_list_t(crt_proc_t proc, crt_rank_list_t **data);

/**
 * Generic processing routine.
 *
 * \param proc [IN/OUT]         Abstract processor object.
 * \param data [IN/OUT]         A pointer to data.
 *
 * \return                      Zero on success, negative value if error.
 */
int
crt_proc_crt_iov_t(crt_proc_t proc, crt_iov_t *data);

#define crt_proc__Bool			crt_proc_bool
#define crt_proc_crt_size_t		crt_proc_uint64_t
#define crt_proc_crt_off_t		crt_proc_uint64_t
#define crt_proc_crt_rank_t		crt_proc_uint32_t
#define crt_proc_crt_opcode_t		crt_proc_uint32_t
#define crt_proc_int			crt_proc_int32_t
#define crt_proc_crt_group_id_t		crt_proc_crt_string_t
#define crt_proc_crt_phy_addr_t		crt_proc_crt_string_t

#if defined(__cplusplus)
}
#endif

#endif /* __CRT_API_H__ */
