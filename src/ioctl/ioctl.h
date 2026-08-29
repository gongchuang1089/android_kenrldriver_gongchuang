#ifndef GONGCHUANG_IOCTL_H
#define GONGCHUANG_IOCTL_H
#include <linux/completion.h>
#include <linux/bpf.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <net/sock.h>
#include "../memory/memory.h"

#define MAX_CACHE_KERNEL_ADDRESS_COUNT 16

#define GET_PROCESS_PID 0
#define IS_PROCESS_PID_ALIVE 1
#define ATTACH_PROCESS 2
#define GET_MODULE_BASE 3
#define	ACCESS_PROCESS_VM 4

#define PTE_PHYS_READ_MEMORY 5
#define PTE_PHYS_WRITE_MEMORY 6

struct gongchuang_sock {
	pid_t pid;

	atomic_t remap_in_progress;
	unsigned long pfn;

	unsigned long cached_kernel_pages[MAX_CACHE_KERNEL_ADDRESS_COUNT];
	size_t cached_count;
};

struct req_access_process_vm {
	pid_t from;
	void __user* from_addr;
	pid_t to;
	void __user* to_addr;
	size_t size;
};

struct req_pte_rw
{
	uint64_t vaddr;
	void *buffer;
	size_t size;
};


int init_ioctl(void);

void exit_ioctl(void);

#endif/*GONGCHUANG_IOCTL_H*/