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
 * 4. All publications or advertising materials mentioning features or use of
 *    this software are asked, but not required, to acknowledge that it was
 *    developed by Intel Corporation and credit the contributors.
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
 * This file is part of CaRT. Hybrid Logical Clock (HLC) implementation.
 */
#include "crt_hlc.h"
#include <mercury_atomic.h>
#include <time.h>

#define CRT_HLC_MASK 0xFFFF

static hg_atomic_int64_t crt_hlc = HG_ATOMIC_VAR_INIT(0);

uint64_t crt_hlc_get_last(void)
{
	return hg_atomic_get64(&crt_hlc);
}

static inline int crt_hlc_gettime(uint64_t *time)
{
	struct timespec now;
	int		rc;

	rc = clock_gettime(CLOCK_REALTIME, &now);
	if (rc)
		return rc;

	*time = (uint64_t)(now.tv_sec * 1e9 + now.tv_nsec) & ~CRT_HLC_MASK;

	return 0;
}

int crt_hlc_send(uint64_t *time)
{
	uint64_t pt, hlc, new;
	int	 rc;

	rc = crt_hlc_gettime(&pt);
	if (rc)
		return rc;

	do {
		hlc = hg_atomic_get64(&crt_hlc);
		new = (hlc & ~CRT_HLC_MASK) < pt ? pt : hlc + 1;
	} while (!hg_atomic_cas64(&crt_hlc, hlc, new));

	if (time != NULL)
		*time = new;

	return 0;
}

int crt_hlc_receive(uint64_t *time, uint64_t msg)
{
	uint64_t pt, hlc, new, ml = msg & ~CRT_HLC_MASK;
	int	 rc;

	rc = crt_hlc_gettime(&pt);
	if (rc)
		return rc;

	do {
		hlc = hg_atomic_get64(&crt_hlc);
		if ((hlc & ~CRT_HLC_MASK) < ml)
			new = ml < pt ? pt : msg + 1;
		else if ((hlc & ~CRT_HLC_MASK) < pt)
			new = pt;
		else if (pt <= ml)
			new = (hlc < msg ? msg : hlc) + 1;
		else
			new = hlc + 1;
	} while (!hg_atomic_cas64(&crt_hlc, hlc, new));

	if (time != NULL)
		*time = new;

	return 0;
}
