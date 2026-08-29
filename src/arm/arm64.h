#ifndef GONGCHUANG_ARM64_H
#define GONGCHUANG_ARM64_H
#include <asm/pgtable-prot.h>
#include <asm/sysreg.h>
#include <asm/pgtable.h>//操作页表 （Page Global Directory）
#include <linux/types.h>        // 基本类型定义
#include <asm/barrier.h>        // dsb, isb, dmb 等屏障
#include <asm/tlbflush.h>       // TLBI 相关宏和函数
#include <asm/cache.h>          // 缓存操作
#include <asm/memory.h>         // 内存地址转换

// 如果使用 u64 替代 uint64_t：
// #include <linux/types.h>      // 已经包含

// 读写调试寄存器
#ifndef AARCH64_DBG_READ
#define AARCH64_DBG_READ(N, REG, VAL)         \
    do                                        \
    {                                         \
        VAL = read_sysreg(dbg##REG##N##_el1); \
    } while (0)
#endif
/*// arch/arm64/include/asm/sysreg.h
#define read_sysreg(r) ({                                      \
    u64 __val;                                                 \
    asm volatile("mrs %0, " __stringify(r) : "=r" (__val));   \
    __val;                                                     \
})linxu定义宏读取寄存器*/
// 无条件按 PA52 布局解码 TTBR.BADDR；TTBR[5:2] 为 0 时自然退化为 PA48 布局。
static inline phys_addr_t ttbr_to_phys(uint64_t ttbr)
{
    // GENMASK_ULL(47, PAGE_SHIFT) 生成仅 [47:PAGE_SHIFT] 为 1 的掩码；
    // 按位与后只保留 TTBR 中的低 48 位页表基址，清除 ASID、CnP 和对齐低位。
    phys_addr_t phys = ttbr & GENMASK_ULL(47, PAGE_SHIFT);

    // 取出 TTBR[5:2] 中编码的 PA[51:48]，左移恢复后拼回物理地址。
    phys |= (ttbr & GENMASK_ULL(5, 2)) << 46;

    return phys;
}

//直接从寄存器ttbr1读取内核页表基础地址
static inline pgd_t *get_kernel_pgd_base(void)
{
    uint64_t ttbr1=read_sysreg(ttbr1_el1);

    return (pgd_t*)phys_to_virt(ttbr_to_phys(ttbr1));
}

// 刷新同一缓存一致性域（Inner Shareable 域）内全部 CPU 中，与指定 VA 对应的所有 ASID TLB 项。
// Android SMP SoC 中屏障范围必须与 TLBI 广播范围匹配：VAALE1IS 广播到 Inner Shareable 域，因此前后必须使用 ISHST/ISH，不能使用仅覆盖本地范围的 NSHST/NSH。
static inline void flush_tlb_addr_all_asid_all_cpus(uint64_t addr)
{
    // TLBI 操作数不是原始 VA；__TLBI_VADDR() 会去掉页内偏移并转换为架构要求的 VA[55:12] 格式。
    uint64_t tlbi_addr = __TLBI_VADDR(addr, 0);

    asm volatile( // DSB ISHST：范围与后面的 VAALE1IS 广播范围匹配，等待此前 PTE 写入对域内其他 CPU 可见。
        "dsb ishst\n\t"
        // TLBI VAALE1IS 字段：VA=按虚拟地址，A=所有 ASID，L=仅最后一级页表项，E1=EL1 Stage-1，IS=广播到一致性域。
        "tlbi vaale1is, %[tlbi_addr]\n\t"
        // DSB ISH：范围同样与 VAALE1IS 匹配，等待该域内所有目标 CPU 完成失效后才能使用新映射。
        "dsb ish\n\t"
        // ISB：清空并重新同步当前 CPU 的取指/执行流水线，使后续指令使用更新后的地址翻译环境。
        "isb\n\t"
        :
        : [tlbi_addr] "r"(tlbi_addr)
        : "memory");
}

#endif/*GONGCHUANG_ARM64_H*/