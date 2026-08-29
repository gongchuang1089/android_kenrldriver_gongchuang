#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kallsyms.h>
#include <linux/vmalloc.h>
#include <linux/rbtree.h>
#include <linux/list.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/proc_fs.h>

#include "ioctl/ioctl.h"
#include "util/util.h"
#include "memory/memory.h"

static void hide_module(void)
{
    if (is_file_exist("/proc/sched_debug")) {
        remove_proc_entry("sched_debug", NULL);
    }

    if (is_file_exist("/proc/uevents_records")) {
        remove_proc_entry("uevents_records", NULL);
    }
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 12, 0)
    struct vmap_area *va, *vtmp;
    struct list_head *_vmap_area_list;
    struct rb_root *_vmap_area_root;

    _vmap_area_list = (struct list_head *)gongchuang_kallsyms_lookup_name("vmap_area_list");
    _vmap_area_root = (struct rb_root *)gongchuang_kallsyms_lookup_name("vmap_area_root");

    // Remove vmalloc call chain, invisible in /proc/vmallocinfo
    list_for_each_entry_safe(va, vtmp, _vmap_area_list, list)
    {
        if ((uint64_t)THIS_MODULE > va->va_start && (uint64_t)THIS_MODULE < va->va_end)
        {
            list_del(&va->list);
            // Remove from rbtree, cannot be located via rbtree traversal
            rb_erase(&va->rb_node, _vmap_area_root);
        }
    }

#endif

    // Remove from linked‑list, invisible in /proc/modules
    list_del_init(&THIS_MODULE->list);
    // Remove kobj, invisible under /sys/modules/
    kobject_del(&THIS_MODULE->mkobj.kobj);

}

static int __init gongchuang_init(void)
{
    int ret=0;
    hide_module();//hide mydriver

    ret=init_ioctl();
    if (ret)
    {
        return ret;  /* code */
    }
    allocate_physical_page_info();//初始化pte
    return ret;
}

static void __exit gongchuang_exit(void)
{
    exit_ioctl();
    free_phys_page();
}

module_init(gongchuang_init);
module_exit(gongchuang_exit);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))

MODULE_IMPORT_NS(is_really_a_systemfile_gongchuang);

#endif


MODULE_AUTHOR("gongchuang");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("gongchuang");
MODULE_VERSION("1.0.0");