/*
 * This program is part of the Clear Linux Project
 *
 * Copyright 2015 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms and conditions of the GNU Lesser General Public License, as
 * published by the Free Software Foundation; either version 2.1 of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 */

#include <check.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include "nica/nc-string.h"
#include "log.h"
#include "read_oopsfile.h"
#include "src/probes/klog_scanner.h"
#include "src/probes/oops_parser.h"

static char reason[1024];
static nc_string *pl;

void callback_func(struct oops_log_msg *msg)
{
        size_t len = strlen(msg->lines[0]) + 1;
        if (len > sizeof(reason)) {
                len = sizeof(reason);
        }
        memcpy(reason, msg->lines[0], len);
        reason[sizeof(reason) - 1] = 0;
        pl = parse_payload(msg);
}

void setup_payload(char *oopsfile)
{
        char *buf = NULL;
        int buflen = 0;

        oops_parser_cleanup();

        buf = readfile(oopsfile);
        buflen = (int)getbuflen();

        if (buflen) {
                oops_parser_init(callback_func);
                split_buf_by_line(buf, buflen);
        }

        if (buf) {
                free(buf);
        }
}

// Tests for checking backtrace
START_TEST(watchdog_payload)
{
        char *oopsfile = NULL;

        oopsfile = TESTOOPSDIR "/watchdog.txt";
        setup_payload(oopsfile);

        telem_log(LOG_ERR, "Watchdog backtrace: %s\n", pl->str);

        ck_assert(pl->len > 0);
        ck_assert_str_eq(reason, "WARNING: at net/sched/sch_generic.c:255 dev_watchdog+0x238/0x250");

        ck_assert(strstr(pl->str, "Kernel Version : 3.10.4-100.fc18.x86_64"));

        ck_assert(strstr(pl->str, "Tainted : Not tainted"));
        ck_assert(strstr(pl->str, "Modules : ebtable_nat fuse ipt_MASQUERADE bnep bluetooth nf_conntrack_netbios_ns"));
        ck_assert(strstr(pl->str, "wire_ohci sdhci_pci mii sdhci firewire_core mmc_core crc_itu_t i2c_core sunrpc"));

        // Check 'Call Trace'
        ck_assert(strstr(pl->str, "#1 dump_stack"));
        ck_assert(strstr(pl->str, "#2 warn_slowpath_common"));
        ck_assert(strstr(pl->str, "#3 warn_slowpath_fmt"));
        ck_assert(strstr(pl->str, "#4 ? __queue_work"));
        ck_assert(strstr(pl->str, "#5 dev_watchdog"));
        ck_assert(strstr(pl->str, "#6 ? dev_deactivate_queue.constprop.30"));
        ck_assert(strstr(pl->str, "#7 call_timer_fn"));
        ck_assert(strstr(pl->str, "#8 ? dev_deactivate_queue.constprop.30"));
        ck_assert(strstr(pl->str, "#9 run_timer_softirq"));
        ck_assert(strstr(pl->str, "#10 __do_softirq"));
        ck_assert(strstr(pl->str, "#11 irq_exit"));
        ck_assert(strstr(pl->str, "#12 smp_apic_timer_interrupt"));
        ck_assert(strstr(pl->str, "#13 apic_timer_interrupt"));
        ck_assert(strstr(pl->str, "#14 ? __hrtimer_start_range_ns"));
        ck_assert(strstr(pl->str, "#15 ? cpuidle_enter_state"));
        ck_assert(strstr(pl->str, "#16 ? cpuidle_enter_state"));
        ck_assert(strstr(pl->str, "#17 cpuidle_idle_call"));
        ck_assert(strstr(pl->str, "#18 arch_cpu_idle"));
        ck_assert(strstr(pl->str, "#19 cpu_startup_entry"));
        ck_assert(strstr(pl->str, "#20 ? clockevents_register_device"));
        ck_assert(strstr(pl->str, "#21 start_secondary"));

        nc_string_free(pl);

}
END_TEST

START_TEST(alsa_bug_payload)
{
        char *oopsfile = NULL;

        oopsfile = TESTOOPSDIR "/ALSA.txt";
        setup_payload(oopsfile);

        printf("ALSA payload: %s\n", pl->str);
        telem_log(LOG_ERR, "AlSA bug backtrace: %s\n", pl->str);

        ck_assert(pl->len > 0);
        ck_assert_str_eq(reason, "ALSA sound/core/pcm_lib.c:154: BUG: stream = 1, pos = 0x1138, buffer size = 0x1138, period size = 0x44e");
        ck_assert(strstr(pl->str, "Kernel Version : 2.6.25-14.fc9.i686 #1"));

        ck_assert(strstr(pl->str, "Tainted : Not tainted"));

        ck_assert(strstr(pl->str, "#1 warn_on_slowpath"));
        ck_assert(strstr(pl->str, "#2 ? vsnprintf"));
        ck_assert(strstr(pl->str, "#3 ? _spin_unlock_irqrestore"));
        ck_assert(strstr(pl->str, "#10 start_kernel"));
        nc_string_free(pl);
}
END_TEST

START_TEST(warning_payload)
{
        char *oopsfile = NULL;

        oopsfile = TESTOOPSDIR "/warning.txt";
        setup_payload(oopsfile);

        telem_log(LOG_ERR, "Warning backtrace: %s\n", pl->str);

        ck_assert(pl->len > 0);
        ck_assert_str_eq(reason, "WARNING: CPU: 1 PID: 796 at kernel/sched/core.c:2342 preempt_notifier_register+0x30/0x70");
        ck_assert(strstr(pl->str, "Kernel Version : 4.2.0-rc2-satori #1"));

        ck_assert(strstr(pl->str, "Tainted : G        W  OE"));

        ck_assert(strstr(pl->str, \
                         "Modules : vboxpci(OE) vboxnetadp(OE) vboxnetflt(OE) vboxdrv(OE) joydev pci_stub"));
        ck_assert(strstr(pl->str, \
                         "drm_kms_helper e1000e 8139too drm r8169 8139cp ptp mii pps_core wmi video"));

        ck_assert(strstr(pl->str, "#1 dump_stack"));
        ck_assert(strstr(pl->str, "#2 warn_slowpath_common"));
        ck_assert(strstr(pl->str, "#3 warn_slowpath_fmt"));
        ck_assert(strstr(pl->str, "#4 preempt_notifier_register"));
        ck_assert(strstr(pl->str, "#5 VBoxHost_RTThreadCtxHooksRegister"));
        ck_assert(strstr(pl->str, "#6 ? update_curr"));
        ck_assert(strstr(pl->str, "#7 ? supdrvIOCtlFast"));
        ck_assert(strstr(pl->str, "#8 ? VBoxDrvLinuxIOCtl_4_3_28"));
        ck_assert(strstr(pl->str, "#9 ? put_prev_entity"));
        ck_assert(strstr(pl->str, "#10 ? do_vfs_ioctl"));
        ck_assert(strstr(pl->str, "#11 ? pick_next_task_rt"));
        ck_assert(strstr(pl->str, "#12 ? SyS_ioctl"));
        ck_assert(strstr(pl->str, "#13 ? entry_SYSCALL_64_fastpath"));

        nc_string_free(pl);
}
END_TEST

START_TEST(warn_on_payload)
{
        char *oopsfile = NULL;

        oopsfile = TESTOOPSDIR "/warn_on.txt";
        setup_payload(oopsfile);

        telem_log(LOG_ERR, "Warn_on backtrace: %s\n", pl->str);
        ck_assert(pl->len > 0);
        ck_assert_str_eq(reason, "WARNING: CPU: 3 PID: 0 at net/sched/sch_generic.c:303 dev_watchdog+0x254/0x260");

        ck_assert(strstr(pl->str, "Kernel Version : 4.0.5-300.fc22.x86_64 #1"));

        ck_assert(strstr(pl->str, "Tainted : Not tainted"));

        ck_assert(strstr(pl->str, "Modules : rfcomm xt_multiport xt_CHECKSUM tun ipt_MASQUERADE nf_nat_masquerade_ipv4"));
        ck_assert(strstr(pl->str, "snd_hda_codec_generic snd_hda_intel snd_hda_controller snd_hda_codec k10temp snd_hwdep"));

        ck_assert(strstr(pl->str, "#1 dump_stack"));
        ck_assert(strstr(pl->str, "#2 warn_slowpath_common"));
        ck_assert(strstr(pl->str, "#3 warn_slowpath_fmt"));
        ck_assert(strstr(pl->str, "#4 dev_watchdog"));
        ck_assert(strstr(pl->str, "#5 ? qdisc_rcu_free"));
        ck_assert(strstr(pl->str, "#6 call_timer_fn"));
        ck_assert(strstr(pl->str, "#7 ? qdisc_rcu_free"));
        ck_assert(strstr(pl->str, "#8 run_timer_softirq"));
        ck_assert(strstr(pl->str, "#9 __do_softirq"));
        ck_assert(strstr(pl->str, "#10 irq_exit"));
        ck_assert(strstr(pl->str, "#11 smp_apic_timer_interrupt"));
        ck_assert(strstr(pl->str, "#12 apic_timer_interrupt"));
        ck_assert(strstr(pl->str, "#13 ? cpuidle_enter_state"));
        ck_assert(strstr(pl->str, "#14 ? cpuidle_enter_state"));
        ck_assert(strstr(pl->str, "#15 cpuidle_enter"));
        ck_assert(strstr(pl->str, "#16 cpu_startup_entry"));
        ck_assert(strstr(pl->str, "#17 start_secondary"));

        nc_string_free(pl);
}
END_TEST

START_TEST(sysctl1_payload)
{
        char *oopsfile = NULL;

        oopsfile = TESTOOPSDIR "/sysctl_check1.txt";
        setup_payload(oopsfile);

        telem_log(LOG_ERR, "sysctl1 backtrace: %s\n", pl->str);

        ck_assert(pl->len > 0);
        ck_assert_str_eq(reason, "sysctl table check failed: net/ath1/%parent  Sysctl already exists");

        ck_assert(strstr(pl->str, "Kernel Version : 2.6.26-2-686 #1"));
        ck_assert(strstr(pl->str, "Tainted : P"));

        ck_assert(strstr(pl->str, "#1 set_fail"));
        ck_assert(strstr(pl->str, "#2 sysctl_check_table"));
        ck_assert(strstr(pl->str, "#3 sysctl_check_table"));
        ck_assert(strstr(pl->str, "#4 sysctl_check_table"));
        ck_assert(strstr(pl->str, "#5 __register_sysctl_paths"));
        ck_assert(strstr(pl->str, "#6 register_sysctl_paths"));
        ck_assert(strstr(pl->str, "#7 ieee80211_sysctl_vattach"));
        ck_assert(strstr(pl->str, "#8 ieee80211_vap_setup"));
        ck_assert(strstr(pl->str, "#9 ath_vap_create"));
        ck_assert(strstr(pl->str, "#10 ieee80211_ioctl_create_vap"));
        ck_assert(strstr(pl->str, "#11 ath_ioctl"));
        ck_assert(strstr(pl->str, "#12 __dev_get_by_name"));
        ck_assert(strstr(pl->str, "#13 ath_ioctl"));
        ck_assert(strstr(pl->str, "#14 dev_ifsioc"));
        ck_assert(strstr(pl->str, "#15 dev_ioctl"));
        ck_assert(strstr(pl->str, "#16 sk_alloc"));
        ck_assert(strstr(pl->str, "#17 d_alloc"));
        ck_assert(strstr(pl->str, "#18 inotify_d_instantiate"));
        ck_assert(strstr(pl->str, "#19 fd_install"));
        ck_assert(strstr(pl->str, "#20 sock_ioctl"));
        ck_assert(strstr(pl->str, "#21 vfs_ioctl"));
        ck_assert(strstr(pl->str, "#22 do_vfs_ioctl"));
        ck_assert(strstr(pl->str, "#23 sys_socketcall"));
        ck_assert(strstr(pl->str, "#24 sys_ioctl"));
        ck_assert(strstr(pl->str, "#25 sysenter_past_esp"));
        ck_assert(strstr(pl->str, "#26 quirk_ali7101_acpi"));

        nc_string_free(pl);
}
END_TEST

START_TEST(sysctl2_payload)
{
        char *oopsfile = NULL;

        oopsfile = TESTOOPSDIR "/sysctl_check2.txt";
        setup_payload(oopsfile);

        telem_log(LOG_ERR, "sysctl2 backtrace: %s\n", pl->str);

        ck_assert(pl->len > 0);
        ck_assert_str_eq(reason, "sysctl table check failed: net/netfilter/nf_conntrack_max .3.19.1 Missing strategy");

        ck_assert(strstr(pl->str, "Kernel Version : 2.6.32.53-bigmem-5-openvz #28"));
        ck_assert(strstr(pl->str, "Tainted : P"));

        ck_assert(strstr(pl->str, "#1 ? set_fail"));
        ck_assert(strstr(pl->str, "#2 ? sysctl_check_table"));
        ck_assert(strstr(pl->str, "#3 ? sysctl_check_lookup"));
        ck_assert(strstr(pl->str, "#4 ? sysctl_check_table"));
        ck_assert(strstr(pl->str, "#5 ? sysctl_check_lookup"));
        ck_assert(strstr(pl->str, "#6 ? sysctl_check_table"));
        ck_assert(strstr(pl->str, "#7 ? __kmalloc"));
        ck_assert(strstr(pl->str, "#8 ? __register_sysctl_paths"));
        ck_assert(strstr(pl->str, "#9 ? proc_register"));
        ck_assert(strstr(pl->str, "#10 ? register_net_sysctl_table"));
        ck_assert(strstr(pl->str, "#11 ? nf_conntrack_net_init"));
        ck_assert(strstr(pl->str, "#12 ? setup_net"));
        ck_assert(strstr(pl->str, "#13 ? copy_net_ns"));
        ck_assert(strstr(pl->str, "#14 ? create_new_namespaces"));

        nc_string_free(pl);
}
END_TEST

START_TEST(softlockup_payload)
{
        char *oopsfile = NULL;

        oopsfile = TESTOOPSDIR "/soft_lockup.txt";
        setup_payload(oopsfile);

        telem_log(LOG_ERR, "Softlockup backtrace: %s\n", pl->str);

        ck_assert(pl->len > 0);
        ck_assert_str_eq(reason, "BUG: soft lockup - CPU#8 stuck for 22s! [ceph-osd:12347]");

        ck_assert(strstr(pl->str, "Kernel Version : 3.16.6-200.el7.x86_64 #1"));

        ck_assert(strstr(pl->str, "Tainted : Not tainted"));

        ck_assert(strstr(pl->str, "Modules : vhost_net vhost macvtap macvlan tun nf_conntrack_ipv6 nf_defrag_ipv6 ip6table_filter"));
        ck_assert(strstr(pl->str, "it drm_kms_helper ttm uas tg3 drm ptp usb_storage pps_core i2c_core hpsa"));

        ck_assert(strstr(pl->str, "#1 _raw_read_lock"));
        ck_assert(strstr(pl->str, "#2 btrfs_tree_read_lock"));
        ck_assert(strstr(pl->str, "#3 btrfs_read_lock_root_node"));
        ck_assert(strstr(pl->str, "#4 btrfs_search_slot"));
        ck_assert(strstr(pl->str, "#5 ? crypto_shash_update"));
        ck_assert(strstr(pl->str, "#6 btrfs_lookup_xattr"));
        ck_assert(strstr(pl->str, "#7 __btrfs_getxattr"));
        ck_assert(strstr(pl->str, "#8 btrfs_getxattr"));
        ck_assert(strstr(pl->str, "#9 get_vfs_caps_from_disk"));
        ck_assert(strstr(pl->str, "#10 audit_copy_inode"));
        ck_assert(strstr(pl->str, "#11 __audit_inode"));
        ck_assert(strstr(pl->str, "#12 SyS_fgetxattr"));
        ck_assert(strstr(pl->str, "#13 system_call_fastpath"));

        nc_string_free(pl);
}
END_TEST

START_TEST(rtnl_payload)
{
        char *oopsfile = NULL;
        oopsfile = TESTOOPSDIR "/RTNL.txt";
        setup_payload(oopsfile);

        telem_log(LOG_ERR, "rtnl backtrace: %s\n", pl->str);

        ck_assert(pl->len > 0);
        ck_assert_str_eq(reason, "RTNL: assertion failed at drivers/net/bonding/bond_main.c");

        ck_assert(strstr(pl->str, "Kernel Version : 3.12-trunk-amd64 #1 Debian 3.12.3-1~exp1"));

        ck_assert(strstr(pl->str, "Tainted : Not tainted"));

        ck_assert(strstr(pl->str, "#1 ? dump_stack"));
        ck_assert(strstr(pl->str, "#2 ? bond_set_rx_mode"));
        ck_assert(strstr(pl->str, "#3 ? __dev_mc_add"));
        ck_assert(strstr(pl->str, "#4 ? igmp6_group_added"));
        ck_assert(strstr(pl->str, "#5 ? sock_def_readable"));
        ck_assert(strstr(pl->str, "#6 ? ipv6_dev_mc_inc"));
        ck_assert(strstr(pl->str, "#7 ? ipv6_sock_mc_join"));
        ck_assert(strstr(pl->str, "#8 ? do_ipv6_setsockopt.isra.6"));
        ck_assert(strstr(pl->str, "#9 ? pipe_write"));
        ck_assert(strstr(pl->str, "#10 ? SYSC_sendto"));
        ck_assert(strstr(pl->str, "#11 ? ipv6_setsockopt"));
        ck_assert(strstr(pl->str, "#12 ? SyS_setsockopt"));
        ck_assert(strstr(pl->str, "#13 ? system_call_fastpath"));

        nc_string_free(pl);
}
END_TEST

START_TEST(kernel_null_pointer_payload)
{
        char *oopsfile = NULL;

        oopsfile = TESTOOPSDIR "/kernel_null_pointer.txt";
        setup_payload(oopsfile);

        telem_log(LOG_ERR, "Kernel Null Pointer backtrace: %s\n", pl->str);

        ck_assert(pl->len > 0);
        ck_assert_str_eq(reason, "BUG: unable to handle kernel NULL pointer dereference at 0000000000000118");

        ck_assert(strstr(pl->str, "Kernel Version : 3.7.4-204.fc18.x86_64 #1 LENOVO 27672LG/27672LG"));
        ck_assert(strstr(pl->str, "Tainted : Not tainted"));

        ck_assert(strstr(pl->str, "Modules : vfat fat usb_storage ipt_MASQUERADE nf_conntrack_netbios_ns nf_conntrack_broadcast"));
        ck_assert(strstr(pl->str, "crc_itu_t yenta_socket i2c_algo_bit drm_kms_helper drm i2c_core wmi video"));

        ck_assert(strstr(pl->str, "#1 fat_detach"));
        ck_assert(strstr(pl->str, "#2 fat_evict_inode"));
        ck_assert(strstr(pl->str, "#3 evict"));
        ck_assert(strstr(pl->str, "#4 iput"));
        ck_assert(strstr(pl->str, "#5 fsnotify_destroy_mark"));
        ck_assert(strstr(pl->str, "#6 sys_inotify_rm_watch"));
        ck_assert(strstr(pl->str, "#7 system_call_fastpath"));

        nc_string_free(pl);
}
END_TEST

START_TEST(kernel_bug_payload)
{
        char *oopsfile = NULL;

        oopsfile = TESTOOPSDIR "/kernel_bug_at_DONE.txt";
        setup_payload(oopsfile);

        telem_log(LOG_ERR, "Kernel bug at backtrace: %s\n", pl->str);

        ck_assert(pl->len > 0);
        ck_assert_str_eq(reason, "kernel BUG at mm/mmap.c:2156!");

        ck_assert(strstr(pl->str, "Kernel Version : 2.6.27.56-0.1-default #1"));
        ck_assert(strstr(pl->str, "Tainted : G"));

        ck_assert(strstr(pl->str, "Modules : ip6t_LOG xt_tcpudp xt_pkttype ipt_LOG xt_limit adi joydev snd_pcm_oss snd_mixer_oss"));
        ck_assert(strstr(pl->str, "ata_generic sata_sis pata_sis libata scsi_mod dock thermal processor thermal_sys hwmon"));

        ck_assert(strstr(pl->str, "#1 mmput"));
        ck_assert(strstr(pl->str, "#2 exit_mm"));
        ck_assert(strstr(pl->str, "#3 do_exit"));
        ck_assert(strstr(pl->str, "#4 do_group_exit"));
        ck_assert(strstr(pl->str, "#5 get_signal_to_deliver"));
        ck_assert(strstr(pl->str, "#6 do_signal"));
        ck_assert(strstr(pl->str, "#7 do_notify_resume"));
        ck_assert(strstr(pl->str, "#8 ptregscall_common"));
        ck_assert(strstr(pl->str, "#9 sysret_signal"));

        nc_string_free(pl);
}
END_TEST

START_TEST(irq_payload)
{
        char *oopsfile = NULL;

        oopsfile = TESTOOPSDIR "/irq.txt";
        setup_payload(oopsfile);

        telem_log(LOG_ERR, "IRQ backtrace: %s\n", pl->str);

        ck_assert(pl->len > 0);
        ck_assert_str_eq(reason, "irq 11: nobody cared");

        ck_assert(strstr(pl->str, "Kernel Version : 3.10.7-100.fc18.i686.PAE #1"));
        ck_assert(strstr(pl->str, "Tainted : Not tainted"));

        ck_assert(strstr(pl->str, "#1 dump_stack"));
        ck_assert(strstr(pl->str, "#2 __report_bad_irq"));
        ck_assert(strstr(pl->str, "#3 note_interrupt"));
        ck_assert(strstr(pl->str, "#4 ? __do_softirq"));
        ck_assert(strstr(pl->str, "#5 handle_irq_event_percpu"));
        ck_assert(strstr(pl->str, "#6 handle_irq_event"));
        ck_assert(strstr(pl->str, "#7 ? handle_nested_irq"));
        ck_assert(strstr(pl->str, "#8 handle_level_irq"));
        ck_assert(strstr(pl->str, "#9 ? do_IRQ"));
        ck_assert(strstr(pl->str, "#10 ? common_interrupt"));
        ck_assert(strstr(pl->str, "#11 ? __do_softirq"));
        ck_assert(strstr(pl->str, "#12 ? handle_irq"));
        ck_assert(strstr(pl->str, "#13 ? irq_exit"));
        ck_assert(strstr(pl->str, "#14 ? do_IRQ"));
        ck_assert(strstr(pl->str, "#15 ? pci_write"));
        ck_assert(strstr(pl->str, "#16 ? common_interrupt"));
        ck_assert(strstr(pl->str, "#17 ? audit_make_tree"));
        ck_assert(strstr(pl->str, "#18 ? usbdev_ioctl"));
        ck_assert(strstr(pl->str, "#19 ? _raw_spin_unlock_irqrestore"));
        ck_assert(strstr(pl->str, "#20 ? __setup_irq"));
        ck_assert(strstr(pl->str, "#21 ? kmem_cache_alloc_trace"));
        ck_assert(strstr(pl->str, "#22 ? vsnprintf"));
        ck_assert(strstr(pl->str, "#23 ? request_threaded_irq"));
        ck_assert(strstr(pl->str, "#24 ? usb_hcd_platform_shutdown"));
        ck_assert(strstr(pl->str, "#25 ? request_threaded_irq"));
        ck_assert(strstr(pl->str, "#26 ? usb_add_hcd"));
        ck_assert(strstr(pl->str, "#27 ? usb_hcd_pci_probe"));
        ck_assert(strstr(pl->str, "#28 ? rpm_resume"));
        ck_assert(strstr(pl->str, "#29 ? pci_device_probe"));
        ck_assert(strstr(pl->str, "#30 ? driver_probe_device"));
        ck_assert(strstr(pl->str, "#31 ? pci_match_device"));
        ck_assert(strstr(pl->str, "#32 ? __driver_attach"));
        ck_assert(strstr(pl->str, "#33 ? driver_probe_device"));
        ck_assert(strstr(pl->str, "#34 ? bus_for_each_dev"));
        ck_assert(strstr(pl->str, "#35 ? driver_attach"));
        ck_assert(strstr(pl->str, "#36 ? driver_probe_device"));
        ck_assert(strstr(pl->str, "#37 ? bus_add_driver"));
        ck_assert(strstr(pl->str, "#38 ? pci_match_id"));
        ck_assert(strstr(pl->str, "#39 ? driver_register"));
        ck_assert(strstr(pl->str, "#40 ? ohci_hcd_mod_init"));
        ck_assert(strstr(pl->str, "#41 ? ohci_hcd_mod_init"));
        ck_assert(strstr(pl->str, "#42 ? __pci_register_driver"));
        ck_assert(strstr(pl->str, "#43 ? uhci_hcd_init"));
        ck_assert(strstr(pl->str, "#44 ? do_one_initcall"));
        ck_assert(strstr(pl->str, "#45 ? kernel_init_freeable"));
        ck_assert(strstr(pl->str, "#46 ? do_early_param"));

        nc_string_free(pl);

}
END_TEST

START_TEST(general_protection_fault_payload)
{
        char *oopsfile = NULL;

        oopsfile = TESTOOPSDIR "/general_protection_fault.txt";
        setup_payload(oopsfile);

        ck_assert(pl->len > 0);
        ck_assert_str_eq(reason, "general protection fault: 0000 [#1] SMP ");

        telem_log(LOG_ERR, "General protection fault backtrace: %s\n", pl->str);

        ck_assert(strstr(pl->str, "Kernel Version : 3.10.0-210.el7.x86_64"));
        ck_assert(strstr(pl->str, "Tainted : Not tainted"));

        ck_assert(strstr(pl->str, "Modules : tun 8021q garp mrp bridge stp llc bonding ipt_REJECT xt_comment xt_multiport"));
        ck_assert(strstr(pl->str, "lpc_ich iw_cm mfd_core nfsd auth_rpcgss wmi nfs_acl lockd ipmi_si ipmi_devintf"));

        ck_assert(strstr(pl->str, "#1 ? alloc_pipe_info"));
        ck_assert(strstr(pl->str, "#2 alloc_pipe_info"));
        ck_assert(strstr(pl->str, "#3 create_pipe_files"));
        ck_assert(strstr(pl->str, "#4 __do_pipe_flags"));
        ck_assert(strstr(pl->str, "#5 SyS_pipe2"));
        ck_assert(strstr(pl->str, "#6 ? __audit_syscall_exit"));
        ck_assert(strstr(pl->str, "#7 SyS_pipe"));
        ck_assert(strstr(pl->str, "#8 system_call_fastpath"));

        nc_string_free(pl);
}
END_TEST

START_TEST(double_fault_payload)
{
        char *oopsfile = NULL;

        oopsfile = TESTOOPSDIR "/double_fault.txt";
        setup_payload(oopsfile);

        telem_log(LOG_ERR, "Double fault backtrace: %s\n", pl->str);

        ck_assert(pl->len > 0);
        ck_assert_str_eq(reason, "double fault: 0000 [#3] SMP ");

        ck_assert(strstr(pl->str, "Kernel Version : 2.6.31.5-127.fc12.x86_64 #1 Latitude E6430"));
        ck_assert(strstr(pl->str, "Tainted : P      D  "));

        ck_assert(strstr(pl->str, "Modules : nls_utf8 vfat fat sit tunnel4 ext2 fuse ip6table_filter ip6_tables ebtable_nat"));
        ck_assert(strstr(pl->str, "joydev sdhci_pci sdhci usb_storage mmc_core video output [last unloaded: speedstep_lib]"));

        nc_string_free(pl);
}
END_TEST

START_TEST(bad_page_map_payload)
{
        char *oopsfile = NULL;

        oopsfile = TESTOOPSDIR "/bad_page_map.txt";
        setup_payload(oopsfile);

        telem_log(LOG_ERR, "Bad page map backtrace: %s\n", pl->str);

        ck_assert(pl->len > 0);
        ck_assert_str_eq(reason, "BUG: Bad page map in process nemo  pte:00010000 pmd:6ade7067");

        ck_assert(strstr(pl->str, "Kernel Version : 3.12-1-amd64 #1 Debian 3.12.9-1"));
        ck_assert(strstr(pl->str, "Tainted : Not tainted"));

        nc_string_free(pl);

}
END_TEST

START_TEST(bug_kernel_handle_payload)
{
        char *oopsfile = NULL;

        oopsfile = TESTOOPSDIR "/bug_kernel_handle.txt";
        setup_payload(oopsfile);

        telem_log(LOG_ERR, "Bug kernel handle backtrace: %s\n", pl->str);

        ck_assert(pl->len > 0);
        ck_assert_str_eq(reason, "BUG: unable to handle kernel paging request at 0000000000002658");

        ck_assert(strstr(pl->str, "Kernel Version : 3.7.9-201.fc18.x86_64 #1 Apple Inc. MacBookPro6,2/Mac-F22586C8"));
        ck_assert(strstr(pl->str, "Tainted : PF"));

        ck_assert(strstr(pl->str, "Modules : nf_conntrack_netbios_ns nf_conntrack_broadcast ipt_MASQUERADE be2iscsi"));
        ck_assert(strstr(pl->str, "drm_kms_helper firewire_ohci drm firewire_core tg3 crc_itu_t i2c_core video usb_storage sunrpc"));

        ck_assert(strstr(pl->str, "#1 ? _nv007312rm"));
        ck_assert(strstr(pl->str, "#2 ? _nv007847rm"));
        ck_assert(strstr(pl->str, "#3 ? _nv004049rm"));
        ck_assert(strstr(pl->str, "#4 ? _nv004049rm"));
        ck_assert(strstr(pl->str, "#5 ? _nv010019rm"));
        ck_assert(strstr(pl->str, "#6 ? _nv014983rm"));
        ck_assert(strstr(pl->str, "#7 ? _nv001097rm"));
        ck_assert(strstr(pl->str, "#8 ? rm_init_adapter"));
        ck_assert(strstr(pl->str, "#9 ? nv_kern_open"));
        ck_assert(strstr(pl->str, "#10 ? chrdev_open"));
        ck_assert(strstr(pl->str, "#11 ? do_dentry_open"));
        ck_assert(strstr(pl->str, "#12 ? cdev_put"));
        ck_assert(strstr(pl->str, "#13 ? finish_open"));
        ck_assert(strstr(pl->str, "#14 ? do_last"));
        ck_assert(strstr(pl->str, "#15 ? inode_permission"));
        ck_assert(strstr(pl->str, "#16 ? link_path_walk"));
        ck_assert(strstr(pl->str, "#17 ? path_openat"));
        ck_assert(strstr(pl->str, "#18 ? do_filp_open"));
        ck_assert(strstr(pl->str, "#19 ? __alloc_fd"));
        ck_assert(strstr(pl->str, "#20 ? do_sys_open"));
        ck_assert(strstr(pl->str, "#21 ? __audit_syscall_entry"));
        ck_assert(strstr(pl->str, "#22 ? sys_open"));
        ck_assert(strstr(pl->str, "#23 ? system_call_fastpath"));

        nc_string_free(pl);

}
END_TEST

START_TEST(bug_kernel_handle_payload_new_format)
{
        char *oopsfile = NULL;

        oopsfile = TESTOOPSDIR "/bug_kernel_handle_new.txt";
        setup_payload(oopsfile);

        telem_log(LOG_ERR, "Bug kernel handle backtrace: %s\n", pl->str);

        ck_assert(pl->len > 0);
        ck_assert_str_eq(reason, "BUG: unable to handle kernel paging request at 0000000000002658");

        ck_assert(strstr(pl->str, "Kernel Version : 3.7.9-201.fc18.x86_64 #1 Apple Inc. MacBookPro6,2/Mac-F22586C8"));
        ck_assert(strstr(pl->str, "Tainted : PF"));

        ck_assert(strstr(pl->str, "Modules : nf_conntrack_netbios_ns nf_conntrack_broadcast ipt_MASQUERADE be2iscsi"));
        ck_assert(strstr(pl->str, "drm_kms_helper firewire_ohci drm firewire_core tg3 crc_itu_t i2c_core video usb_storage sunrpc"));

        ck_assert(strstr(pl->str, "#1 ? _nv007312rm"));
        ck_assert(strstr(pl->str, "#2 ? _nv007847rm"));
        ck_assert(strstr(pl->str, "#3 ? _nv004049rm"));
        ck_assert(strstr(pl->str, "#4 ? _nv004049rm"));
        ck_assert(strstr(pl->str, "#5 ? _nv010019rm"));
        ck_assert(strstr(pl->str, "#6 ? _nv014983rm"));
        ck_assert(strstr(pl->str, "#7 ? _nv001097rm"));
        ck_assert(strstr(pl->str, "#8 ? rm_init_adapter"));
        ck_assert(strstr(pl->str, "#9 nv_kern_open"));
        ck_assert(strstr(pl->str, "#10 ? chrdev_open"));
        ck_assert(strstr(pl->str, "#11 ? do_dentry_open"));
        ck_assert(strstr(pl->str, "#12 ? cdev_put"));
        ck_assert(strstr(pl->str, "#13 ? finish_open"));
        ck_assert(strstr(pl->str, "#14 ? do_last"));
        ck_assert(strstr(pl->str, "#15 ? inode_permission"));
        ck_assert(strstr(pl->str, "#16 link_path_walk"));
        ck_assert(strstr(pl->str, "#17 path_openat"));
        ck_assert(strstr(pl->str, "#18 ? do_filp_open"));
        ck_assert(strstr(pl->str, "#19 ? __alloc_fd"));
        ck_assert(strstr(pl->str, "#20 ? do_sys_open"));
        ck_assert(strstr(pl->str, "#21 ? __audit_syscall_entry"));
        ck_assert(strstr(pl->str, "#22 ? sys_open"));
        ck_assert(strstr(pl->str, "#23 ? system_call_fastpath"));

        nc_string_free(pl);

}
END_TEST

Suite *config_suite(void)
{
        // A suite is comprised of test cases, defined below
        Suite *s = suite_create("probes");

        // Individual unit tests are added to "test cases"
        TCase *t = tcase_create("probes");
        tcase_add_test(t, watchdog_payload);
        tcase_add_test(t, alsa_bug_payload);
        tcase_add_test(t, warning_payload);
        tcase_add_test(t, warn_on_payload);

        tcase_add_test(t, sysctl1_payload);
        tcase_add_test(t, sysctl2_payload);
        tcase_add_test(t, softlockup_payload);
        tcase_add_test(t, rtnl_payload);
        tcase_add_test(t, kernel_null_pointer_payload);
        tcase_add_test(t, kernel_bug_payload);

        tcase_add_test(t, irq_payload);

        tcase_add_test(t, general_protection_fault_payload);
        tcase_add_test(t, double_fault_payload);
        tcase_add_test(t, bad_page_map_payload);
        tcase_add_test(t, bug_kernel_handle_payload);
        tcase_add_test(t, bug_kernel_handle_payload_new_format);

        suite_add_tcase(s, t);

        return s;
}

int main(void)
{
        Suite *s;
        SRunner *sr;
        int failed;

        s = config_suite();
        sr = srunner_create(s);

        srunner_set_fork_status (sr, CK_NOFORK);

        // Use the TAP driver for now, so that each
        // unit test will PASS/FAIL in the log output.
        srunner_set_log(sr, NULL);
        srunner_set_tap(sr, "-");

        // set CK_NOFORK to attach gdb
        // srunner_set_fork_status(sr, CK_NOFORK);
        srunner_run_all(sr, CK_SILENT);
        failed = srunner_ntests_failed(sr);
        srunner_free(sr);

        // if you want the TAP driver to report a hard error based
        // on certain conditions (e.g. number of failed tests, etc.),
        // return non-zero here instead.
        return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
