#ifndef GONGCHUANG_UTIL_H
#define GONGCHUANG_UTIL_H
#define PAGE_MASK  (~(PAGE_SIZE - 1))
#define PAGE_SHIFT 12

#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/version.h>
#include <linux/kprobes.h>
#include <linux/mm.h>           // 内存管理总头文件 
#include <linux/sched.h>        // 进程调度相关
#include <asm/pgtable.h>        // 页表操作函数 
#include <linux/kernel.h>       // 内核基础函数

#include "../arm/arm64.h"

unsigned long gongchuang_kallsyms_lookup_name(const char *symbol_name);

pid_t FindProcess_ByName(const char *name);

int is_pid_alive(pid_t pid);

bool is_file_exist(const char *filename) ;

pte_t *get_kernel_pte(uint64_t vaddr);
static inline struct task_struct *get_task_by_pid(pid_t pid);
struct mm_struct *get_mm_by_pid(pid_t pid);
#endif /*GONGCHUANG_UTIL_H*/
