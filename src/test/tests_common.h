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
#include <gurt/common.h>

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

int
psr_start_basic()
{
	char		*env_self_rank;
	d_rank_t	 my_rank;

	env_self_rank = getenv("CRT_L_RANK");
	my_rank = atoi(env_self_rank);

        /* Set up for DBG_PRINT */
	opts.self_rank = my_rank;
	opts.mypid = getpid();
	opts.is_server = 1;

	rc = d_log_init();
	assert(rc == 0);

	DBG_PRINT("Server starting up\n");
	rc = crt_init("server_grp", CRT_FLAG_BIT_SERVER |
		CRT_FLAG_BIT_PMIX_DISABLE | CRT_FLAG_BIT_LM_DISABLE);
	if (rc != 0) {
		D_ERROR("crt_init() failed; rc=%d\n", rc);
		assert(0);
	}

	grp = crt_group_lookup(NULL);
	if (!grp) {
		D_ERROR("Failed to lookup group\n");
		assert(0);
	}

	rc = crt_rank_self_set(my_rank);
	if (rc != 0) {
		D_ERROR("crt_rank_self_set(%d) failed; rc=%d\n",
			my_rank, rc);
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
}

#endif /* __TESTS_COMMON_H__ */
