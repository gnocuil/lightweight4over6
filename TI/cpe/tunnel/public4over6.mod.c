#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
 .name = KBUILD_MODNAME,
 .init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
 .exit = cleanup_module,
#endif
 .arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xad12b0d5, "module_layout" },
	{ 0x8c1dbb52, "inet6_add_protocol" },
	{ 0xa1bf72e, "dst_release" },
	{ 0xccf39896, "dev_get_by_name" },
	{ 0x3e383385, "nf_hooks" },
	{ 0x7b75527e, "skb_realloc_headroom" },
	{ 0xf2142bde, "nf_hook_slow" },
	{ 0x2fbd3bb, "netif_rx" },
	{ 0x55f2bf3e, "__pskb_pull_tail" },
	{ 0xb72397d5, "printk" },
	{ 0x1e2669d7, "free_netdev" },
	{ 0xa7591d5e, "register_netdev" },
	{ 0xb4390f9a, "mcount" },
	{ 0x9756c141, "skb_push" },
	{ 0xe1048212, "init_net" },
	{ 0x6261ecfd, "ip6_route_output" },
	{ 0x7dceceac, "capable" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0xa9b4fdb, "alloc_netdev_mq" },
	{ 0xaeefc015, "kfree_skb" },
	{ 0x81a86b8d, "inet6_del_protocol" },
	{ 0x4fd481d1, "__dev_get_by_name" },
	{ 0xd83791bc, "nf_conntrack_destroy" },
	{ 0x37a0cba, "kfree" },
	{ 0x1f07d4a6, "unregister_netdev" },
	{ 0xf3149e88, "consume_skb" },
	{ 0xfd430c52, "sock_wfree" },
	{ 0xcba6ea37, "skb_copy_bits" },
	{ 0xd6c963c, "copy_from_user" },
	{ 0xe914e41e, "strcpy" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "CBC6909949F7017CD303A76");
