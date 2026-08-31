#ifndef GONGCHUANG_MEMORY_H
#define GONGCHUANG_MEMORY_H

#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/mm.h>
#include <linux/sched/task.h>
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0))
#include <linux/mmap_lock.h>
#define MM_READ_LOCK(mm) mmap_read_lock(mm);
#define MM_READ_UNLOCK(mm) mmap_read_unlock(mm);
#else
#include <linux/rwsem.h>
#define MM_READ_LOCK(mm) down_read(&(mm)->mmap_sem);
#define MM_READ_UNLOCK(mm) up_read(&(mm)->mmap_sem);
#endif


struct pte_phys_page
{
    void *base_addr;
    size_t size;
    pte_t *pte_addr;
};
static struct pte_phys_page pte_page;
enum request_op
{
   REQ_PTE_PHYS_READ_MEMORY,
   REQ_PTE_PHYS_WRITE_MEMORY,
};

uintptr_t Get_Module_Base(pid_t pid, char *name, int vm_flag);
int access_process_vm_by_pid(pid_t from, void __user *from_addr, pid_t to, void __user*to_addr, size_t size);

int allocate_physical_page_info(void);
void free_phys_page(void);

int pte_process_memory_rw_cached(enum request_op op, pid_t pid, uint64_t vaddr, void *buffer, size_t size);

#endif /*GONGCHUANG_MEMORY_H*/
