/*	$OpenBSD: $ */

/*
 * Copyright (c) 2026 Himanshu Chauhan <himanshu@thechauhan.dev>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MACHINE_SSE_H_
#define	_MACHINE_SSE_H_

enum {
	SSE_EVT_ATTR_ID_STATUS,
	SSE_EVT_ATTR_ID_PRIORITY,
	SSE_EVT_ATTR_ID_CONFIG,
	SSE_EVT_ATTR_ID_PREFERRED_HART,
	SSE_EVT_ATTR_ID_ENTRY_PC,
	SSE_EVT_ATTR_ID_ENTRY_ARG,
	SSE_EVT_ATTR_ID_INT_SEPC,
	SSE_EVT_ATTR_ID_INT_FLAGS,
	SSE_EVT_ATTR_ID_INT_A6,
	SSE_EVT_ATTR_ID_INT_A7,

	SSE_EVT_ATTR_ID_RESVD,
};

/* Event call back */
typedef int (*sse_evt_cb_t)(void *context);

int sse_init(void);
int sse_register_event(unsigned long evt, sse_evt_cb_t cb, void *context);
int sse_enable_event(unsigned long evt);
int sse_disable_event(unsigned long evt);
int sse_unregister_event(unsigned long evt);
int sse_complete_event(unsigned long evt);
int sse_set_event_priority(unsigned long evt, unsigned long priority);
unsigned long sse_get_event_priority(unsigned long evt);

#endif
