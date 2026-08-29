#include "ioctl.h"
#include "../util/util.h"
#include "../memory/memory.h"

static int free_family = AF_DECnet;

static struct proto gongchuang_proto = 
{
	.name = "GONGCHUANG_PROTO",
	.owner = THIS_MODULE,
	.obj_size = sizeof(struct sock) + sizeof(struct gongchuang_sock),
};

static int get_process_pid(int len, char __user *process_name_user) 
{
	int err;
	pid_t pid;
	char* process_name;

	process_name = kmalloc(len, GFP_KERNEL);
	if (!process_name) 
	{
		return -ENOMEM;
	}

	if (copy_from_user(process_name, process_name_user, len)) 
	{
		err = -EFAULT;
		goto out_proc_name;
	}

	pid = FindProcess_ByName(process_name);
	if (pid < 0) 
	{
		err = -ESRCH;
		goto out_proc_name;
	}

	err = put_user((int) pid, (pid_t*) process_name_user);
	if (err)
		goto out_proc_name;

	out_proc_name:
	kfree(process_name);
	return err;
}

static int get_process_module_base(int len, pid_t pid, char __user *module_name_user, int flag) {
	int err;
	char* module_name;

	module_name = kmalloc(len, GFP_KERNEL);
	if (!module_name) {
		return -ENOMEM;
	}

	if (copy_from_user(module_name, module_name_user, len)) {
		err = -EFAULT;
		goto out_module_name;
	}

	uintptr_t base = Get_Module_Base(pid, module_name, flag);
	if (base == 0) {
		err = -ENAVAIL;
		goto out_module_name;
	}

	err = put_user((uintptr_t) base, (uintptr_t*) module_name_user);
	if (err)
		goto out_module_name;

	out_module_name:
	kfree(module_name);
	return err;
}

static int gongchuang_getsockopt(struct socket *sock, int level, int optname, char __user *optval, int __user *optlen)
{
	struct sock* sk;
	struct gongchuang_sock* os;
	int len, alive, ret=0;
	void *kernel_buf = NULL;

	sk = sock->sk;
	if (!sk)
		return -EINVAL;
	os = ((struct gongchuang_sock*)((char *) sock->sk + sizeof(struct sock)));

	pr_debug("getsockopt: %d\n", optname);
	switch (optname) {
		case GET_PROCESS_PID: 
		{
			ret = get_process_pid(level, optval);
			if (ret) 
			{
				pr_err("get_process_pid failed: %d\n", ret);
			}
			break;
		}
		case IS_PROCESS_PID_ALIVE:
		{
			alive=is_pid_alive(level);
			if (put_user(alive,optlen))
			{
				return -EAGAIN;
			}
			ret=0;
			break;			
		}
		case ATTACH_PROCESS: 
		{
			if(is_pid_alive(level) == 0) 
			{
				return -ESRCH;
			}
			os->pid = level;
			pr_info("attached process: %d\n", level);
			ret = 0;
			break;
		}
		case GET_MODULE_BASE:
		{
			if (os->pid <= 0)
			{
				return -ESRCH;
			}
			

			if (get_user(len, optlen))
				return -EFAULT;

			if (len < 0)
				return -EINVAL;

			ret = get_process_module_base(len, os->pid, optval, level);
			break;
		}
		case ACCESS_PROCESS_VM: {
			if (get_user(len, optlen))
				return -EFAULT;

			if (len < sizeof(struct req_access_process_vm))
				return -EINVAL;

			struct req_access_process_vm req;
			if (copy_from_user(&req, optval, sizeof(struct req_access_process_vm)))
				return -EFAULT;
			
			ret = access_process_vm_by_pid(req.from, req.from_addr, req.to, req.to_addr, req.size);
			break;
		}

	if (os->pid <= 0 || is_pid_alive(os->pid) == 0) 
	{
		return -ESRCH;
	}

		return ret;
	}   

	switch (optname)
	{
	case PTE_PHYS_READ_MEMORY:
		if (get_user(len, optlen))
				return -EFAULT;

		if (len < sizeof(struct req_pte_rw))
				return -EINVAL;
		struct req_pte_rw req;
		if (copy_from_user(&req, optval, sizeof(struct req_pte_rw)))
				return -EFAULT;

		kernel_buf=kmalloc(req.size,GFP_KERNEL);
		if (!kernel_buf)
		{
			pr_err("Cna't alloc memroy");
			return -ENOMEM;
		}
		
		enum request_op op;
		op=REQ_PTE_PHYS_READ_MEMORY;
		ret=pte_process_memory_rw_cached(op,os->pid,req.vaddr,kernel_buf,req.size);

		if (ret > 0) {
        if (copy_to_user(req.buffer, kernel_buf, req.size)) {
            ret = -EFAULT;
        } else if (ret == req.size) {
            ret = 0;  // 完全成功，返回 0
        }
		
    	}
		      
        kfree(kernel_buf);
        
		break;

	case PTE_PHYS_WRITE_MEMORY:
		if (get_user(len, optlen))
				return -EFAULT;

		if (len < sizeof(struct req_pte_rw))
				return -EINVAL;
		struct req_pte_rw req1;
		if (copy_from_user(&req1, optval, sizeof(struct req_pte_rw)))
				return -EFAULT;

		kernel_buf=kmalloc(req1.size,GFP_KERNEL);
		if (!kernel_buf)
		{
			pr_err("Cna't alloc memroy");
			return -ENOMEM;
		}
		if (copy_from_user(kernel_buf, req1.buffer, req1.size)) 
		{
            kfree(kernel_buf);
            return -EFAULT;
        }

		enum request_op op1;
		op1=REQ_PTE_PHYS_WRITE_MEMORY;
		ret=pte_process_memory_rw_cached(op1,os->pid,req1.vaddr,kernel_buf,req1.size);

		if (ret==req1.size)
		{
			ret=0;
		}
		
		kfree(kernel_buf);
		break;

	default:
		break;
	}
	if (ret <= 0) 
	{
		if(ret == 0) 
		{
			return 0;
		} else {
			return ret;
		}
	}
	return -EOPNOTSUPP;
}

static int gongchuang_release(struct socket *sock) {
	struct sock *sk = sock->sk;

	if (!sk) 
    {
		return 0;
	}

	struct gongchuang_sock *os = (struct gongchuang_sock *) ((char *) sock->sk + sizeof(struct sock));

	for (int i = 0; i < os->cached_count; i++) 
    {
		if (os->cached_kernel_pages[i]) 
        {
			free_page(os->cached_kernel_pages[i]);
		}
	}

	sock_orphan(sk);
	sock_put(sk);
	return 0;
}

static int gongchuang_setsockopt(struct socket *sock, int level, int optname, sockptr_t optval, unsigned int optlen)
{

    uid_t caller_uid;

    caller_uid = *((uid_t*) &current_cred()->uid);
	if (caller_uid != 0) 
    {
		return -EAFNOSUPPORT;
	}

	switch (optname) 
    {
		default:
			break;
	}

	return -ENOPROTOOPT;
}


static __poll_t gongchuang_poll(struct file *file, struct socket *sock,struct poll_table_struct *wait) 
{
    uid_t caller_uid;

    caller_uid = *((uid_t*) &current_cred()->uid);
	if (caller_uid != 0) 
    {
		return -EAFNOSUPPORT;
	}
	return 0;
}

int gongchuang_sendmsg(struct socket *sock, struct msghdr *m,size_t total_len) 
{
    uid_t caller_uid;

    caller_uid = *((uid_t*) &current_cred()->uid);
	if (caller_uid != 0) 
    {
		return -EAFNOSUPPORT;
	}
	return 0;
}

int gongchuang_ioctl(struct socket * sock, unsigned int cmd, unsigned long arg) 
{
	return 0;
}

int gongchuang_mmap(struct file *file, struct socket *sock, struct vm_area_struct *vma) 
{
	return 0;
}

static struct proto_ops gongchuang_proto_ops = 
{
	.family = PF_DECnet,
	.owner = THIS_MODULE,
	.release = gongchuang_release,
	.bind = sock_no_bind,
	.connect = sock_no_connect,
	.socketpair = sock_no_socketpair,
	.accept = sock_no_accept,
	.getname = sock_no_getname,
	.poll		= gongchuang_poll,
	.ioctl		= gongchuang_ioctl,
	.listen		= sock_no_listen,
	.shutdown	= sock_no_shutdown,
	.setsockopt	= gongchuang_setsockopt,
	.getsockopt	= gongchuang_getsockopt,
	.sendmsg	= gongchuang_sendmsg,
	.recvmsg	= sock_no_recvmsg,
	.mmap		= gongchuang_mmap
};

static int gongchuang_create(struct net *net, struct socket *sock, int protocol, int kern)
{
    uid_t caller_uid;
	struct sock *sk;
	struct gongchuang_sock *os;

	caller_uid = *((uid_t*) &current_cred()->uid);
	if (caller_uid != 0) 
    {
		pr_warn("Only root can create gongchuang socket!\n");
		return -EAFNOSUPPORT;
	}

	if (sock->type != SOCK_RAW) 
    {
		return -ENOKEY;
	}

    if (protocol !=0x090419)
    {
        return -ENOKEY;
    }
    

    sk = sk_alloc(net, PF_INET, GFP_KERNEL, &gongchuang_proto, kern);
	if (!sk) 
    {
		pr_warn("sk_alloc failed!\n");
		return -ENOBUFS;
	}

	os = (struct gongchuang_sock*)((char *) sk + sizeof(struct sock));

	sock->ops = &gongchuang_proto_ops;
	sock_init_data(sock, sk);

	os->pid = 0;
	os->pfn = 0;
	atomic_set(&os->remap_in_progress, 0);
	os->cached_count = 0;

	return 0;
}

static struct net_proto_family gongchuang_family_ops = 
{
	.family = PF_DECnet,
	.create = gongchuang_create,
	.owner	= THIS_MODULE,
};

static int register_free_family(void)
{
    int family;
    int erro;
    for (family=free_family; family<NPROTO; family++)
    {
       gongchuang_family_ops.family=family;
       erro=sock_register(&gongchuang_family_ops);
       if (erro)
       {
            continue;
       }
       	else 
        {
			free_family = family;
			pr_info("Find free proto_family: %d\n", free_family);
			return 0;
		}
    }
    pr_info("Can't find free family\n");
    return erro;

}

int init_ioctl(void)
{
    int erro;

    erro=proto_register(&gongchuang_proto,1);
    if (erro)
    {
        goto result; 
    }

    erro=register_free_family();
    if (erro)
    {
        goto result_proto;
    }
    return 0;

    result_proto:
        proto_unregister(&gongchuang_proto);
    
    result:
        return erro;
}

void exit_ioctl(void) {
	sock_unregister(free_family);
	proto_unregister(&gongchuang_proto);
}