/*	$OpenBSD: $ */
/*
 * Copyright (c) 2026 Doongar Singh <doonbsd@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/percpu.h>

#include <uvm/uvm.h>
#include <uvm/uvm_extern.h>

#include <machine/sbi.h>
#include <machine/sse.h>

#include <dev/cons.h>

static unsigned long supp_local_events[] = {
	SBI_SSE_LOCAL_SW_INJECTED,
};

static unsigned long supp_global_events[] = {
	SBI_SSE_GLOBAL_SW_INJECTED,
};

#define	NR_LOCAL_EVENTS		1
#define	NR_GLOBAL_EVENTS	1

enum sse_event_state {
	SSE_EVENT_UNUSED,	/* Event is unused */
	SSE_EVENT_REGISTERED,	/* Event is registered */
	SSE_EVENT_ENABLED,	/* Event is ready to run */
	SSE_EVENT_RUNNING,	/* Event is currently being handled */

	SSE_EVENT_MAX
};

struct sse_event {
	unsigned long evt_id;	/* Event ID of the event */
	vaddr_t stack;		/* Stack to handle the event */
	vaddr_t va;		/* Global event memory */
	paddr_t pa;		/* Physical addr */
	sse_evt_cb_t cb;	/* Event callback */
	void *context;		/* Context for callback */
	unsigned int state;	/* Current state of the event */
	TAILQ_ENTRY(sse_event) lentry;
};

TAILQ_HEAD(, sse_event) sse_local_evt_list;
TAILQ_HEAD(, sse_event) sse_global_evt_list;

extern void sse_entry(unsigned long, unsigned long);

static void free_global_evt_buffers(void)
{
}

static void free_local_evt_buffers(void)
{
}

#define USE_MALLOC 1

#if USE_MALLOC
#define ALLOC_MEM(sz) malloc(sz, M_DEVBUF, M_NOWAIT | M_ZERO)
#else
#define ALLOC_MEM(sz) km_alloc(sz, &kv_any, &kp_zero, &kd_nowait)
#endif

static vaddr_t allocate_evt_buffer(paddr_t *pa)
{
	vaddr_t evt_va;

	evt_va = (vaddr_t)ALLOC_MEM(2 * PAGE_SIZE);
	if (!evt_va)
		goto out;

	*pa = 0;
	if (!pmap_extract(pmap_kernel(), evt_va, pa)) {
		km_free((void *)evt_va, 2 * PAGE_SIZE, &kv_any, &kp_zero);
		evt_va = 0;
	}

 out:
	return evt_va;
}

static int sbi_sse_register(uint32_t event_id, unsigned long pc, unsigned long arg)
{
	struct sbi_ret ret;

	ret = SBI_CALL3(SBI_EXT_ID_SSE, SBI_SSE_REGISTER_EVENT, event_id, pc, arg);

	return (ret.error != 0 ? (int)ret.error : 0);
}

static int sse_init_global_events(void)
{
	int i;
	struct sse_event  *evt;
	vaddr_t evt_va;
	paddr_t evt_pa;
	int err = 0;

	for (i = 0; i < NR_GLOBAL_EVENTS; i++) {
		if ((evt_va = allocate_evt_buffer(&evt_pa)) == 0) {
			err = ENOMEM;
			goto err_out;
		}

		err = sbi_sse_register(supp_global_events[i],
		    (unsigned long)sse_entry, evt_va);

		if (err != 0) {
			printf("Unable to register event 0x%lx\n", supp_global_events[i]);
			return err;
		}

		evt = (struct sse_event *)evt_va;
		evt->va = evt_va;
		evt->pa = evt_pa;
		evt->evt_id = supp_global_events[i];
		evt->state = SSE_EVENT_UNUSED;
		evt->stack = evt->va + (2 * PAGE_SIZE);
		evt->cb = NULL;
		evt->context = NULL;
		TAILQ_INSERT_HEAD(&sse_global_evt_list, evt, lentry);
		printf("SSE: Registered event 0x%lx\n", evt->evt_id);
	}

	return 0;

 err_out:
	free_global_evt_buffers();

	return err;
}

static int sse_init_local_events(void)
{
	int i;
	struct sse_event *evt;
	vaddr_t evt_va;
	paddr_t evt_pa;
	int err = 0;

	for (i = 0; i < NR_LOCAL_EVENTS; i++) {
		if ((evt_va = allocate_evt_buffer(&evt_pa)) == 0) {
			err = ENOMEM;
			goto err_out;
		}

		err = sbi_sse_register(supp_local_events[i],
		    (unsigned long)sse_entry, evt_va);

		if (err != 0) {
			printf("Unable to register event 0x%lx\n", supp_local_events[i]);
			return err;
		}

		evt = (struct sse_event *)evt_va;
		evt->va = evt_va;
		evt->pa = evt_pa;
		evt->evt_id = supp_local_events[i];
		evt->state = SSE_EVENT_UNUSED;
		evt->stack = evt->va + (2 * PAGE_SIZE);
		evt->cb = NULL;
		evt->context = NULL;
		TAILQ_INSERT_HEAD(&sse_local_evt_list, evt, lentry);
		printf("SSE: Registered event 0x%lx\n", evt->evt_id);
	}

	return 0;

 err_out:
	free_local_evt_buffers();

	return err;
}

void handle_double_trap(struct trapframe *frame)
{
	panic("!!! Double trap!!!\n");
}

void
do_handle_sse(struct trapframe *frame, uint32_t hart_id, void *entry_arg)
{
	struct sse_event *ev = (struct sse_event *)entry_arg;

	if (ev->evt_id == SBI_SSE_LOCAL_DBL_TRAP) {
		handle_double_trap(frame);
		/* NOT REACHED */
	}

	if (ev && ev->cb) {
		ev->cb(ev->context);
		return;
	}
}

int sse_init(void)
{
	struct cpu_info *ci = curcpu();
	int rc;

	if (sbi_probe_extension(SBI_EXT_ID_SSE) == 0) {
		printf("SSE: Extension not detected\n");
		return -1;
	}

	if (!CPU_IS_PRIMARY(ci)) {
		if ((rc = sse_init_local_events()) != 0) {
			printf("SSE: Failed to initialize local event on hart %llu\n",
			    ci->ci_hartid);
			return rc;
		}
		return 0;
	}

	if ((rc = sse_init_global_events()) != 0) {
		printf("SSE: Failed to initialize global events for boot CPU\n");
		return rc;
	}

	if ((rc = sse_init_local_events()) != 0) {
		printf("SSE: Failed to initialize local events for boot CPU\n");
		return rc;
	}

	printf("SSE: Initialized successfully\n");

	return 0;
}

static struct sse_event *
get_sse_local_event(unsigned long evt_id)
{
	struct sse_event *evt = NULL;

	TAILQ_FOREACH(evt, &sse_local_evt_list, lentry) {
		if (evt->evt_id == evt_id)
			return evt;
	}

	return NULL;
}

static struct sse_event *
get_sse_global_event(unsigned long evt_id)
{
	struct sse_event *evt = NULL;

	TAILQ_FOREACH(evt, &sse_global_evt_list, lentry) {
		if (evt->evt_id == evt_id)
			return evt;
	}

	return NULL;
}

static struct sse_event *
get_sse_event(unsigned long evt_id)
{
	struct sse_event *evt;

	if ((evt = get_sse_local_event(evt_id)) != NULL)
		return evt;

	if ((evt = get_sse_global_event(evt_id)) != NULL)
		return evt;

	return NULL;
}

int sse_register_event(unsigned long evt_id, sse_evt_cb_t cb, void *context)
{
	struct sse_event *evt;

	evt = get_sse_event(evt_id);

	if (evt == NULL)
		return EINVAL;


	return 0;
}

int sse_enable_event(unsigned long evt)
{
	return 0;
}

int sse_disable_event(unsigned long evt)
{
	return 0;
}

int sse_unregister_event(unsigned long evt)
{
	return 0;
}

int sse_complete_event(unsigned long evt)
{
	return 0;
}

int sse_set_event_priority(unsigned long evt, unsigned long priority)
{
	return 0;
}

unsigned long sse_get_event_priority(unsigned long evt)
{
	return 0;
}

void do_sse_test(void)
{
	printf("%s: enter\n", __func__);
}
