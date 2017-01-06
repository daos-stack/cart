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
 * CaRT (Collective and RPC Transport) basic type definitions.
 */

#ifndef __CRT_TYPES_H__
#define __CRT_TYPES_H__

#include <stdint.h>

typedef uint64_t	crt_size_t;
typedef uint64_t	crt_off_t;

/** iovec for memory buffer */
typedef struct {
	/** Buffer address */
	void	       *iov_buf;
	/** Buffer length */
	crt_size_t	iov_buf_len;
	/** Data length */
	crt_size_t	iov_len;
} crt_iov_t;

static inline void
crt_iov_set(crt_iov_t *iov, void *buf, crt_size_t size)
{
	iov->iov_buf = buf;
	iov->iov_len = iov->iov_buf_len = size;
}

/**
 * hide the dark secret that uuid_t is an array not a structure.
 */
struct crt_uuid {
	uuid_t		uuid;
};

/**
 * Server Identification and Addressing
 *
 * A server is identified by a group and a rank. A name (such as a string) is
 * associated with a group.
 */
typedef uint32_t	crt_rank_t;

typedef struct {
	/** Input number */
	uint32_t	num;
	/** Output/returned number */
	uint32_t	num_out;
} crt_nr_t;

typedef struct {
	/** Number of ranks */
	crt_nr_t	 rl_nr;
	crt_rank_t	*rl_ranks;
} crt_rank_list_t;

typedef char		*crt_string_t;
typedef const char	*crt_const_string_t;

/* CRT uses a string as the group ID */
typedef crt_string_t	crt_group_id_t;
/* Max length of the group ID string including the trailing '\0' */
#define CRT_GROUP_ID_MAX_LEN	(64)

/* Default group ID */
#define CRT_DEFAULT_SRV_GRPID	"crt_default_srv_group"
#define CRT_DEFAULT_CLI_GRPID	"crt_default_cli_group"

typedef struct crt_group {
	/* The group ID of this group */
	crt_group_id_t		cg_grpid;
} crt_group_t;

/* Transport endpoint identifier */
typedef struct {
	/* Group handle, NULL means the primary group */
	crt_group_t	 *ep_grp;
	/* Rank number within the group */
	crt_rank_t	 ep_rank;
	/* Tag, now used as the context ID of the target rank */
	uint32_t	 ep_tag;
} crt_endpoint_t;

/** Scatter/gather list for memory buffers */
typedef struct {
	crt_nr_t	 sg_nr;
	crt_iov_t	*sg_iovs;
} crt_sg_list_t;

/* CaRT context handle */
typedef void *crt_context_t;

/* Physical address string, for example: "bmi+tcp://localhost:3344". */
typedef crt_string_t crt_phy_addr_t;
#define CRT_PHY_ADDR_ENV	"CRT_PHY_ADDR_STR"

/*
 * RPC is identified by opcode. All the opcodes with the highest 16 bits as 1
 * are reserved for internal usage, such as group maintenance and others. If you
 * define its RPC using those reserved opcodes, then an undefined result is
 * expected.
 */
typedef uint32_t crt_opcode_t;
#define CRT_OPC_INTERNAL_BASE	0xFFFF0000UL

/*
 * Check if the opcode is reserved by CRT internally.
 *
 * \param opc [IN]		opcode to be checked.
 *
 * \return			Zero means a legal opcode for user, non-zero means
 *				a CRT internally reserved opcode.
 */
static inline int
crt_opcode_reserved(crt_opcode_t opc)
{
	return (opc & CRT_OPC_INTERNAL_BASE) == CRT_OPC_INTERNAL_BASE;
}

typedef void *crt_rpc_input_t;
typedef void *crt_rpc_output_t;

typedef void *crt_bulk_t; /* Abstract bulk handle */

/**
 * Max size of input/output parameters defined as 64M bytes, for larger length
 * you should transfer by bulk.
 */
#define CRT_MAX_INPUT_SIZE	(0x4000000)
#define CRT_MAX_OUTPUT_SIZE	(0x4000000)

#define CRT_RPC_FLAGS_PUB_BITS	(2)
#define CRT_RPC_FLAGS_PUB_MASK	((1U << CRT_RPC_FLAGS_PUB_BITS) - 1)
enum crt_rpc_flags {
	/*
	 * Ignore timeout. Default behavior (no this flags) is resending
	 * request upon timeout.
	 */
	CRT_RPC_FLAG_IGNORE_TIMEOUT	= (1U << 0),
	/* Destroy group when the bcast RPC finishes, only valid for corpc */
	CRT_RPC_FLAG_GRP_DESTROY	= (1U << 1),
	/* All other bits, are reserved for internal usage. */
};

struct crt_rpc;

typedef int (*crt_req_callback_t)(struct crt_rpc *rpc);

/* Public RPC request/reply, exports to user */
typedef struct crt_rpc {
	crt_context_t		cr_ctx; /* CRT context of the RPC */
	crt_endpoint_t		cr_ep; /* Endpoint ID */
	crt_opcode_t		cr_opc; /* opcode of the RPC */
	/* User passed in flags, \see enum crt_rpc_flags */
	enum crt_rpc_flags	cr_flags;
	crt_rpc_input_t		cr_input; /* Input parameter struct */
	crt_rpc_output_t	cr_output; /* Output parameter struct */
	crt_size_t		cr_input_size; /* Size of input struct */
	crt_size_t		cr_output_size; /* Size of output struct */
	/* Optional bulk handle for collective RPC */
	crt_bulk_t		cr_co_bulk_hdl;
} crt_rpc_t;

/* Abstraction pack/unpack processor */
typedef void *crt_proc_t;
/* Proc callback for pack/unpack parameters */
typedef int (*crt_proc_cb_t)(crt_proc_t proc, void *data);

/* RPC message layout definitions */
enum cmf_flags {
	CMF_ARRAY_FLAG	= 1 << 0,
};

struct crt_msg_field {
	const char		*cmf_name;
	const uint32_t		cmf_flags;
	const uint32_t		cmf_size;
	crt_proc_cb_t		cmf_proc;
};

struct crf_field {
	uint32_t		crf_count;
	struct crt_msg_field	**crf_msg;
};

enum {
	CRT_IN = 0,
	CRT_OUT = 1,
};

struct crt_req_format {
	const char		*crf_name;
	uint32_t		crf_idx;
	struct crf_field	crf_fields[2];
};

struct crt_array {
	crt_size_t	 da_count;
	void		*da_arrays;
};

#define DEFINE_CRT_REQ_FMT_ARRAY(name, crt_in, in_size,			\
				 crt_out, out_size) {			\
	crf_name :	(name),						\
	crf_idx :	0,						\
	crf_fields : {							\
		/* [CRT_IN] = */ {					\
			crf_count :	(in_size),			\
			crf_msg :	(crt_in)			\
		},							\
		/* [CRT_OUT] = */ {					\
			crf_count :	(out_size),			\
			crf_msg :	(crt_out)			\
		}							\
	}								\
}

#define DEFINE_CRT_REQ_FMT(name, crt_in, crt_out)			\
DEFINE_CRT_REQ_FMT_ARRAY((name), (crt_in),				\
			 ((crt_in) == NULL) ? 0 : ARRAY_SIZE(crt_in),	\
			 (crt_out),					\
			 ((crt_out) == NULL) ? 0 : ARRAY_SIZE(crt_out))

#define DEFINE_CRT_MSG(name, flags, size, proc) {			\
	cmf_name :	(name),						\
	cmf_flags :	(flags),					\
	cmf_size :	(size),						\
	cmf_proc :	(crt_proc_cb_t)(proc)				\
}

/* Common request format type */
extern struct crt_msg_field CMF_UUID;
extern struct crt_msg_field CMF_GRP_ID;
extern struct crt_msg_field CMF_INT;
extern struct crt_msg_field CMF_UINT32;
extern struct crt_msg_field CMF_CRT_SIZE;
extern struct crt_msg_field CMF_UINT64;
extern struct crt_msg_field CMF_BULK;
extern struct crt_msg_field CMF_BOOL;
extern struct crt_msg_field CMF_STRING;
extern struct crt_msg_field CMF_PHY_ADDR;
extern struct crt_msg_field CMF_RANK;
extern struct crt_msg_field CMF_RANK_LIST;
extern struct crt_msg_field CMF_BULK_ARRAY;
extern struct crt_msg_field CMF_IOVEC;

extern struct crt_msg_field *crt_single_out_fields[];
struct crt_single_out {
	int	dso_ret;
};

typedef enum {
	CRT_BULK_PUT = 0x68,
	CRT_BULK_GET,
} crt_bulk_op_t;

typedef void *crt_bulk_opid_t;

typedef enum {
	/* Read/write */
	CRT_BULK_RW = 0x88,
	/* Read-only */
	CRT_BULK_RO,
	/* Write-only */
	CRT_BULK_WO,
} crt_bulk_perm_t;

/* Bulk transferring descriptor */
struct crt_bulk_desc {
	crt_rpc_t	*bd_rpc; /* Original RPC request */
	crt_bulk_op_t	bd_bulk_op; /* CRT_BULK_PUT or CRT_BULK_GET */
	crt_bulk_t	bd_remote_hdl; /* Remote bulk handle */
	crt_off_t	bd_remote_off; /* Offset within remote bulk buffer */
	crt_bulk_t	bd_local_hdl; /* Local bulk handle */
	crt_off_t	bd_local_off; /* Offset within local bulk buffer */
	crt_size_t	bd_len; /* Length of the bulk transferring */
};

struct crt_cb_info {
	crt_rpc_t		*cci_rpc; /* RPC struct */
	void			*cci_arg; /* User passed in arg */
	/*
	 * Return code, will be set as:
	 * 0                     For successful RPC request,
	 * -CER_TIMEDOUT         For timed out request,
	 * other negative value  For other possible failures.
	 */
	int			cci_rc;
};

struct crt_bulk_cb_info {
	struct crt_bulk_desc	*bci_bulk_desc; /* Bulk descriptor */
	void			*bci_arg; /* User passed in arg */
	int			bci_rc; /* Return code */
};

/* Server-side RPC handler */
typedef int (*crt_rpc_cb_t)(crt_rpc_t *rpc);

/**
 * Completion callback for crt_req_send
 *
 * \param cb_info [IN]		Pointer to call back info.
 *
 * \return			Zero means success.
 *				In the case of RPC request timed out, user
 *				register complete_cb will be called (with
 *				cb_info->cci_rc set as -CER_TIMEDOUT).
 *				complete_cb returns -CER_AGAIN means resending
 *				the RPC request.
 */
typedef int (*crt_cb_t)(const struct crt_cb_info *cb_info);

/* Completion callback for bulk transferring, such as crt_bulk_transfer() */
typedef int (*crt_bulk_cb_t)(const struct crt_bulk_cb_info *cb_info);

/**
 * Progress condition callback, /see crt_progress().
 *
 * \param arg [IN]              Argument to cond_cb.
 *
 * \return			Zero means continue progressing.
 *				>0 means stopping progress and return success.
 *				<0 means failure.
 */
typedef int (*crt_progress_cond_cb_t)(void *args);

/**
 * Some bit flags for crt_init:
 * CRT_FLAG_BIT_SERVER		False means pure client, true will enable the
 *				server which listens for the incoming request.
 * CRT_FLAG_BIT_SINGLETON	False means it is a multi-process program,
 *				true means with single process.
 */
enum crt_init_flag_bits {
	CRT_FLAG_BIT_SERVER	= 1U << 0,
	CRT_FLAG_BIT_SINGLETON	= 1U << 1
};

#endif /* __CRT_TYPES_H__ */
