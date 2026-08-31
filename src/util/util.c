#include "util.h"

unsigned long gongchuang_kallsyms_lookup_name(const char *symbol_name) 
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)//在内核版本5.7以后kallsyms_lookup_name就不导出了
    typedef unsigned long (*kallsyms_lookup_name_t)(const char *name);

    struct kprobe kp={0};
    static kallsyms_lookup_name_t lookup_name = NULL;
    if (lookup_name == NULL) 
    {
        kp.symbol_name = "kallsyms_lookup_name";
        if(register_kprobe(&kp) < 0) 
        {
            return 0;
        }

        lookup_name = (kallsyms_lookup_name_t) kp.addr;
        unregister_kprobe(&kp);
    }
    return lookup_name(symbol_name);
#else
    return kallsyms_lookup_name(symbol_name);
#endif
}

static int (*my_get_cmdline)(struct task_struct *task, char *buffer, int buflen) = NULL;


pid_t FindProcess_ByName(const char *name) 
{
    struct task_struct *task;
    char cmdline[256];
	size_t name_len;
    int ret;

	name_len = strlen(name);
	if (name_len == 0) 
    {
		pr_err("process name is empty\n");
		return -2;
	}

    if (my_get_cmdline == NULL)
    {
        my_get_cmdline = (void *) gongchuang_kallsyms_lookup_name("get_cmdline");
    }
    
    rcu_read_lock();
    for_each_process(task) 
    {
        //jumpkernel
        if (task->mm == NULL) 
        {
            continue;
        }

        cmdline[0] = '\0';
        if (my_get_cmdline != NULL) 
        {
            ret = my_get_cmdline(task, cmdline, sizeof(cmdline));
        } else {
            ret = -1;
        }

        if (ret < 0) 
        {
            pr_warn("Failed to get cmdline for pid %d\n", task->pid);
            if (strncmp(task->comm, name, min(strlen(task->comm), name_len)) == 0) {
                rcu_read_unlock();
                return task->pid;
            }
        } else {
            if (strncmp(cmdline, name, min(name_len, strlen(cmdline))) == 0) {
                rcu_read_unlock();
                return task->pid;
            }
        }
    }

    rcu_read_unlock();
    return 0;
}

int is_pid_alive(pid_t pid) 
{
    struct pid * pid_struct;
    struct task_struct *task;

    pid_struct = find_get_pid(pid);
    if (!pid_struct)
    {
        return false;
    }

    task = pid_task(pid_struct, PIDTYPE_PID);
    if (!task)
    {
        return false;
    }
        
    put_pid(pid_struct);
    return pid_alive(task);
}

int flip_open(const char *filename, int flags, umode_t mode, struct file **f) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0))
    *f = filp_open(filename, flags, mode);
    return *f == NULL ? -2 : 0;
#else
    static struct file* (*reserve_flip_open)(const char *filename, int flags, umode_t mode) = NULL;

    if (reserve_flip_open == NULL) {
        reserve_flip_open = (struct file* (*)(const char *filename, int flags, umode_t mode))gongchuang_kallsyms_lookup_name("filp_open");
        if (reserve_flip_open == NULL) {
            return -1;
        }
    }

    *f = reserve_flip_open(filename, flags, mode);
    return *f == NULL ? -2 : 0;
#endif
}

int flip_close(struct file **f, fl_owner_t id) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0))
    filp_close(*f, id);
    return 0;
#else
    static struct file* (*reserve_flip_close)(struct file **f, fl_owner_t id) = NULL;

    if (reserve_flip_close == NULL) {
        reserve_flip_close = (struct file* (*)(struct file **f, fl_owner_t id))gongchuang_kallsyms_lookup_name("filp_close");
        if (reserve_flip_close == NULL) {
            return -1;
        }
    }

    reserve_flip_close(f, id);
    return 0;
#endif
}

bool is_file_exist(const char *filename) {
    struct file* fp;

    if(flip_open(filename, O_RDONLY, 0, &fp) == 0) {
        if (!IS_ERR(fp)) {
            flip_close(&fp, NULL);
            return true;
        }
        return false;
    }
    return false;
}
//pgd->p4d(为了兼容性加的)->pud->pmd->pte
pte_t *get_kernel_pte(uint64_t vaddr)
{
    pgd_t *pgd=get_kernel_pgd_base()+pgd_index(vaddr);
    if (pgd_bad(*pgd)||pgd_none(*pgd))
    {
        return NULL;
    }
    
    p4d_t *p4d=p4d_offset(pgd,vaddr);
    if (p4d_bad(*p4d)||p4d_none(*p4d))
    {
        return NULL;
    }
    
    pud_t *pud=pud_offset(p4d,vaddr);
    if (pud_none(*pud))
    {
        return NULL;
    }
    
    if (pud_leaf(*pud))
    {
        return NULL;
    }

    if (pud_bad(*pud))
    {
        return NULL;
    }
    
    pmd_t *pmd=pmd_offset(pud,vaddr);
    if (pmd_none(*pmd))
    {
        return NULL;
    }
    
    if (pmd_leaf(*pmd))
    {
        return NULL;
    }
    
    if (pmd_bad(*pmd))
    {
        return NULL;
    }
    
    pte_t *ptep=pte_offset_kernel(pmd,vaddr);
    if (!ptep)
    {
        return NULL;
    }
    if (!pte_present(*ptep))
    {
        return NULL;
    }
    
    
    return ptep;
}

static inline struct task_struct *get_task_by_pid(pid_t pid)
{
    struct pid *pid_struct = find_get_pid(pid);
    if (!pid_struct) return NULL;

    struct task_struct *task = get_pid_task(pid_struct, PIDTYPE_PID);
    put_pid(pid_struct);
    return task;
}

// 根据 pid 获取 mm_struct，调用方负责 mmput。
struct mm_struct *get_mm_by_pid(pid_t pid)
{
    struct task_struct *task = get_task_by_pid(pid);
    if (!task) return NULL;

    struct mm_struct *mm = get_task_mm(task);
    put_task_struct(task);
    return mm;
}
