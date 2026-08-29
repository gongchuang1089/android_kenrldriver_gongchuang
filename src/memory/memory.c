#include "memory.h"

#include <linux/tty.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/memory.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/string.h>
#include <asm/page.h>
#include <linux/pgtable.h>
#include <linux/vmalloc.h>
#include <linux/mman.h>

#include "../util/util.h"

uintptr_t Get_Module_Base(pid_t pid, char *name, int vm_flag)
{
    struct pid *pid_struct;
    struct task_struct *task;
    struct mm_struct *mm;
    struct vm_area_struct *vma;

#if (LINUX_VERSION_CODE>=KERNEL_VERSION(6,1,0))
    struct vma_iterator vmi;
#endif
    uintptr_t result=0;
    struct dentry *dentry;
    size_t name_len,dname_len;

    name_len=strlen(name);
    if (name_len==0)
    {
        pr_err("module name is empty");
        return -1;
    }
    
    pid_struct=find_get_pid(pid);
    if (!pid_struct)
    {
        pr_err("failed find pid_struct");
        return -1;
    }
    
    task=get_pid_task(pid_struct,PIDTYPE_PID);
    put_pid(pid_struct);
    if (!task)
    {
        pr_err("failed to get task from pid_struct");
        return -1;
    }
    
    mm=get_task_mm(task);
    put_task_struct(task);
    if (!mm)
    {
       	pr_err("failed to get mm from task\n");
        return -1;
    }


    MM_READ_LOCK(mm)

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
    vma_iter_init(&vmi, mm, 0);
    for_each_vma(vmi, vma)
#else
        for (vma = mm->mmap; vma; vma = vma->vm_next)
#endif
    {
        if (vma->vm_file) {
			if (vm_flag && !(vma->vm_flags & vm_flag)) {
				continue;
			}
			dentry = vma->vm_file->f_path.dentry;
			dname_len = dentry->d_name.len;
			if (!memcmp(dentry->d_name.name, name, min(name_len, dname_len))) {
				result = vma->vm_start;
				goto ret;
			}
        }
    }

    ret:
    MM_READ_UNLOCK(mm)

    mmput(mm);
    return result;
}

int access_process_vm_by_pid(pid_t from, void __user *from_addr, pid_t to, void __user*to_addr, size_t size)
{
    char __kernel *buf;
    int ret;
    struct task_struct *task;
    struct pid *pid_struct;
     
    pid_struct=find_get_pid(from);
    if (!pid_struct)
    {
        pr_err("failed to find pid_struct");
		return -1;
    }

    task=get_pid_task(pid_struct,PIDTYPE_PID);
    put_pid(pid_struct);
    if (!task)
    {
        pr_err("failed to get task from pid_struct");
		return -1;
    }
    
    buf=vmalloc(size);
    if (!buf)
    {
        pr_err("can't allocte memory");
        return -1;
    }
    
    ret=access_process_vm(task,(unsigned long)from_addr,buf,(int)size,0);
    if (ret!=size)
    {
        vfree(buf);
        put_task_struct(task);
        return -1;
    }
    put_task_struct(task);

    pid_struct=find_get_pid(to);
	if (!pid_struct) {
		pr_err("failed to find pid_struct");
		vfree(buf);
		return -1;
	}

	task=get_pid_task(pid_struct, PIDTYPE_PID);
	put_pid(pid_struct);
	if(!task) {
		pr_err("failed to get task from pid_struct");
		vfree(buf);
		return -1;
	}

    ret = access_process_vm(task, (unsigned long) to_addr, buf, (int) size, FOLL_WRITE);
    if (ret != size) {
        vfree(buf);
		put_task_struct(task);
        return -1;
    }

    vfree(buf);
	put_task_struct(task);
    return 0;
}

// 初始化
int allocate_physical_page_info(void)
{
    uint64_t vaddr;
    pte_t *ptep;

    if (in_atomic())
    {
        ls_log_tag("vmem", "原子上下文禁止调用 vmalloc\n");
        return -EPERM;
    }

    __builtin_memset(&pte_page, 0, sizeof(pte_page));

    // 分配内存
    vaddr = (uint64_t)vmalloc(PAGE_SIZE);
    if (!vaddr)
    {
        ls_log_tag("vmem", "vmalloc 失败\n");
        return -ENOMEM;
    }

    // 必须 memset 触发缺页，让内核填充 TTBR1 指向的页表
    __builtin_memset((void *)vaddr, 0xAA, PAGE_SIZE);

    // 获取内核地址对应的 PTE 指针
    ptep = get_kernel_pte(vaddr);
    if (!ptep)
    {
        ls_log_tag("vmem", "获取 PTE 失败\n");
        goto err_out;
    }

    pte_page.base_addr = (void *)vaddr;
    pte_page.size = PAGE_SIZE;
    pte_page.pte_addr = ptep;
    return 0;

err_out:
    vfree((void *)vaddr);
    __builtin_memset(&pte_page, 0, sizeof(pte_page));
    return -EFAULT;
}
// 释放
static inline void free_phys_page(void)
{
    if (pte_page.base_addr)
    {
        // 释放之前通过 vmalloc 分配的虚拟内存
        vfree(pte_page.base_addr);
        __builtin_memset(&pte_page, 0, sizeof(pte_page));
    }
}

// 验证参数并直接操作PTE建立物理页映射
static inline void *pte_map_page(phys_addr_t paddr, size_t size, const void *buffer)
{
    // 普通内存页表配置
    /*
    我建议使用MT_NORMAL(有缓存)，RAM是口语化表达广泛含义表内存，DRAM是内存硬件具体的硬件存储介质
    原因如下:
        一块普通的DRAM物理地址同时被2个或以上的虚拟地址进行了不同属性的映射
        类如:用户态虚拟地址映射这个物理页为有缓存,内核线性区映射这个物理页有缓存，这里却映射为无缓存
        虽然说3种都能访问，但是会出现数据不一致的情况
    1.映射为有缓存的用户态和线性:对地址写入很多时候还存在CPU cache(多级缓存中,常见的如L1~L3级缓存)
                                这时候进行绕过缓存读DRAM中数据，肯定是错乱的，应为cpu未把缓存写回DRAM
    2.映射为无缓存的内核态:你对这个物理页的读写都是直达DRAM,此时cpu拿缓存进行计算，修改DRMA不会实时反映到虚拟地址

    这里使用无缓存读原因是：目标进程分配一个内存页，用dc civac直接清除这个内存页的缓存，
                        随后把坐标指针重定向到这个内存页，内核读取了这个内存页用了缓存，那么下次这个页的读取就会变快，进行缓存检测
    无缓存读会带来非常严重的性能下降和数据不一致情况
    */
    static const uint64_t FLAGS = PTE_TYPE_PAGE | PTE_VALID | PTE_AF | PTE_SHARED | PTE_PXN | PTE_UXN | PTE_ATTRINDX(MT_NORMAL_NC);
    /*
    Device memory 不允许普通 RAM 那种随意访问方式
    代码使用__builtin_memcpy
    但 mapped 如果被标成 MT_DEVICE_nGnRnE，编译器生成的访问序列可能是：
    ldr/str 8 字节
    ldp/stp 成对访问
    更宽的块访问
    非自然对齐访问
    只要 mapped 地址不是对应宽度自然对齐，或者指令形式不适合 Device memory，就可能直接死
    尤其这里返回的是return (uint8_t *)pte_info.base_address + (paddr & ~PAGE_MASK);
    如果 paddr 页内偏移不是 4/8/16 对齐，而 memcpy 刚好生成宽访问，Device 就很容易炸。Normal_NC 映射下 CPU 可以处理很多非对齐访问；Device mapping 下不行。

    // 硬件设备寄存器专用页表配置（不要使用硬件寄存器页表配置去读取普通物理页，原因不过多解释，太复杂了问AI去）
     static const uint64_t FLAGS = PTE_TYPE_PAGE | PTE_VALID | PTE_AF |
                                   PTE_SHARED | PTE_PXN | PTE_UXN |
                                   PTE_ATTRINDX(MT_DEVICE_nGnRnE);
    */

    uint64_t pfn = __phys_to_pfn(paddr);

    // 参数检查
    if (!size || !buffer) return ERR_PTR(-EINVAL);
    // PFN 有效性检查：确保物理页帧在系统内存管理范围内
    if (!pfn_valid(pfn)) return ERR_PTR(-EFAULT);
    // 跨页检查：读写可能跨越页边界，访问到未映射的下一页
    if (((paddr & ~PAGE_MASK) + size) > PAGE_SIZE) return ERR_PTR(-EINVAL);

    // 修改 PTE 指向目标物理页
    set_pte(pte_page.pte_addr, pfn_pte(pfn, __pgprot(FLAGS)));

    // 可能跨 CPU 使用，广播刷新对应 VA 的 TLB。
    flush_tlb_addr_all_asid_all_cpus((uint64_t)pte_page.base_addr);

    // 刷新该页的 TLB, 内部含：dsb(ish) + TLBI + dsb(ish)+isb(),手写刷新需取消dsbisb注释
    // flush_tlb_kernel_range((uint64_t)pte_info.base_address, (uint64_t)pte_info.base_address + PAGE_SIZE);
    // 刷新全部cpu核心TLB
    // flush_tlb_all();

    return (uint8_t *)pte_page.base_addr + (paddr & ~PAGE_MASK);
}

// 读取
static inline int pte_read_physical(phys_addr_t paddr, void *buffer, size_t size)
{
    void *mapped = pte_map_page(paddr, size, buffer);
    if (IS_ERR(mapped))
    {
        return PTR_ERR(mapped);
    }

    // 极限性能且安全的内存拷贝 (防未对齐崩溃)
    switch (size)
    {
    case 1:
        __builtin_memcpy(buffer, mapped, 1);
        break;
    case 2:
        __builtin_memcpy(buffer, mapped, 2);
        break;
    case 4:
        __builtin_memcpy(buffer, mapped, 4);
        break;
    case 8:
        __builtin_memcpy(buffer, mapped, 8);
        break;
    case 16:
        __builtin_memcpy(buffer, mapped, 16);
        break;
    default:
        __builtin_memcpy(buffer, mapped, size);
        break;
    }

    return 0;
}

// 写入
static inline int pte_write_physical(phys_addr_t paddr, const void *buffer, size_t size)
{
    void *mapped = pte_map_page(paddr, size, (void *)buffer);
    if (IS_ERR(mapped))
    {
        return PTR_ERR(mapped);
    }

    switch (size)
    {
    case 1:
        __builtin_memcpy(mapped, buffer, 1);
        break;
    case 2:
        __builtin_memcpy(mapped, buffer, 2);
        break;
    case 4:
        __builtin_memcpy(mapped, buffer, 4);
        break;
    case 8:
        __builtin_memcpy(mapped, buffer, 8);
        break;
    case 16:
        __builtin_memcpy(mapped, buffer, 16);
        break;
    default:
        __builtin_memcpy(mapped, buffer, size);
        break;
    }

    return 0;
}

// 手动走页表翻译，遇到PUD:1G大页/PMD:2MB大页，可以直接返回物理地址了
static inline int walk_translate_va_to_pa(struct mm_struct *mm, uint64_t vaddr, phys_addr_t *paddr)
{
    if (!mm || !paddr) return -EINVAL;

    // PGD Level
    pgd_t *pgd = pgd_offset(mm, vaddr);
    if (pgd_none(*pgd) || pgd_bad(*pgd)) return -EFAULT;

    // P4D Level
    p4d_t *p4d = p4d_offset(pgd, vaddr);
    if (p4d_none(*p4d) || p4d_bad(*p4d)) return -EFAULT;

    // PUD Level (可能遇到 1GB 大页)
    pud_t *pud = pud_offset(p4d, vaddr);
    if (pud_none(*pud)) return -EFAULT;

    // 检查是否是 1G 大页
    if (pud_leaf(*pud))
    {
        // 检查pfn
        unsigned long pfn = pud_pfn(*pud);
        if (!pfn_valid(pfn)) return -EFAULT;

        *paddr = (pud_pfn(*pud) << PAGE_SHIFT) + (vaddr & ~PUD_MASK);
        return 0;
    }
    if (pud_bad(*pud)) return -EFAULT;

    //  PMD Level (可能遇到 2MB 大页)
    pmd_t *pmd = pmd_offset(pud, vaddr);
    if (pmd_none(*pmd)) return -EFAULT;

    // 检查是否是 2M 大页
    if (pmd_leaf(*pmd))
    {
        // 检查pfn
        unsigned long pfn = pmd_pfn(*pmd);
        if (!pfn_valid(pfn)) return -EFAULT;

        *paddr = (pmd_pfn(*pmd) << PAGE_SHIFT) + (vaddr & ~PMD_MASK);
        return 0;
    }
    if (pmd_bad(*pmd)) return -EFAULT;

    //  PTE Level (普通的 4KB 页)
    // 较新内核中 __pte_offset_map 不导出，对于 64位 系统直接使用 pte_offset_kernel 即可
    pte_t *ptep = pte_offset_kernel(pmd, vaddr);
    if (!ptep) return -EFAULT;

    pte_t pte = *ptep;

    // 必须检查 pte_present，因为页可能被换出到 Swap 分区
    // 如果 present 为 false，pfn 字段是无效的（存的是 swap offset）
    if (pte_present(pte))
    {
        // 检查pfn
        unsigned long pfn = pte_pfn(pte);
        if (!pfn_valid(pfn)) return -EFAULT;

        *paddr = (pte_pfn(pte) << PAGE_SHIFT) + (vaddr & ~PAGE_MASK);
        return 0;
    }

    return -EFAULT;
}

int pte_process_memory_rw_cached(enum request_op op, pid_t pid, uint64_t vaddr, void *buffer, size_t size)
{
    // 软件 TLB 缓存
    static pid_t cached_pid = 0;
    static uint64_t cached_vpage = -1ULL;
    static phys_addr_t cached_ppage = 0;
    static struct mm_struct *cached_mm = NULL;

    struct mm_struct *mm;
    phys_addr_t paddr_of_page;
    uint64_t current_vaddr = untagged_addr(vaddr);
    size_t bytes_remaining = size;
    size_t bytes_copied = 0;
    size_t bytes_done = 0;
    int status = 0;

    if (!buffer || size == 0)
        return -EINVAL;

    // 检查缓存
    if (pid != cached_pid || cached_mm == NULL) {
        // 进程切换，清空缓存
        if (cached_mm) {
            mmput(cached_mm);
            cached_mm = NULL;
        }
        
        mm = get_mm_by_pid(pid);
        if (!mm)
            return -ESRCH;
        
        cached_pid = pid;
        cached_mm = mm;
        cached_vpage = -1ULL;
    } else {
        mm = cached_mm;
    }

    // 逐页循环
    while (bytes_remaining > 0) {
        size_t page_offset = current_vaddr & (PAGE_SIZE - 1);
        size_t bytes_this_page = PAGE_SIZE - page_offset;
        uint64_t current_vpn = current_vaddr & PAGE_MASK;

        if (bytes_this_page > bytes_remaining)
            bytes_this_page = bytes_remaining;

        // 软件 TLB 命中
        if (current_vpn == cached_vpage) {
            paddr_of_page = cached_ppage;
        } else {
            // 地址合法性检查
            uint64_t task_size = READ_ONCE(mm->task_size);
            if (current_vaddr >= task_size || bytes_this_page > task_size - current_vaddr) {
                status = -EFAULT;
                cached_vpage = -1ULL;
                if (op == request_op_vmem_read && size > 8)
                    memset((uint8_t *)buffer + bytes_copied, 0, bytes_this_page);
                goto next_chunk;
            }

            // 手动页表翻译（不用硬件 MMU）
            status = walk_translate_va_to_pa(mm, current_vpn, &paddr_of_page);
            if (status != 0) {
                cached_vpage = -1ULL;
                if (op == request_op_vmem_read && size > 8)
                    memset((uint8_t *)buffer + bytes_copied, 0, bytes_this_page);
                goto next_chunk;
            }

            // 更新缓存
            cached_vpage = current_vpn;
            cached_ppage = paddr_of_page;
        }

        // PTE 读写
        if (op == PTE_PHYS_READ_MEMORY) {
            status = pte_read_physical(paddr_of_page + page_offset,
                                       (uint8_t *)buffer + bytes_copied,
                                       bytes_this_page);
        } else {
            status = pte_write_physical(paddr_of_page + page_offset,
                                        (const uint8_t *)buffer + bytes_copied,
                                        bytes_this_page);
        }

        if (status != 0) {
            cached_vpage = -1ULL;
            if (op == PTE_PHYS_READ_MEMORY && size > 8)
                memset((uint8_t *)buffer + bytes_copied, 0, bytes_this_page);
            goto next_chunk;
        }

        bytes_done += bytes_this_page;

next_chunk:
        bytes_remaining -= bytes_this_page;
        bytes_copied += bytes_this_page;
        current_vaddr += bytes_this_page;
    }

    return (bytes_done == 0) ? status : (int)bytes_done;
}
