/*
 *  Copyright (C) 2003-2005 Pontus Fuchs, Giridhar Pemmasani
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 */
#include <linux/proc_fs.h>
#include <linux/module.h>
#include <asm/uaccess.h>

#include "ndis.h"
#include "iw_ndis.h"
#include "wrapper.h"

#define MAX_PROC_STR_LEN 32

static struct proc_dir_entry *ndiswrapper_procfs_entry;
extern int proc_uid, proc_gid;

static int procfs_read_stats(char *page, char **start, off_t off,
			     int count, int *eof, void *data)
{
	char *p = page;
	struct ndis_handle *handle = (struct ndis_handle *) data;
	struct ndis_wireless_stats stats;
	NDIS_STATUS res;
	ndis_rssi rssi;

	if (off != 0) {
		*eof = 1;
		return 0;
	}

	res = miniport_query_info(handle, OID_802_11_RSSI, &rssi,
				  sizeof(rssi));
	if (!res)
		p += sprintf(p, "signal_level=%d dBm\n", (s32)rssi);

	res = miniport_query_info(handle, OID_802_11_STATISTICS,
				  &stats, sizeof(stats));
	if (!res) {

		p += sprintf(p, "tx_frames=%Lu\n", stats.tx_frag);
		p += sprintf(p, "tx_multicast_frames=%Lu\n",
			     stats.tx_multi_frag);
		p += sprintf(p, "tx_failed=%Lu\n", stats.failed);
		p += sprintf(p, "tx_retry=%Lu\n", stats.retry);
		p += sprintf(p, "tx_multi_rerty=%Lu\n", stats.multi_retry);
		p += sprintf(p, "tx_rtss_success=%Lu\n", stats.rtss_succ);
		p += sprintf(p, "tx_rtss_fail=%Lu\n", stats.rtss_fail);
		p += sprintf(p, "ack_fail=%Lu\n", stats.ack_fail);
		p += sprintf(p, "frame_duplicates=%Lu\n", stats.frame_dup);
		p += sprintf(p, "rx_frames=%Lu\n", stats.rx_frag);
		p += sprintf(p, "rx_multicast_frames=%Lu\n",
			     stats.rx_multi_frag);
		p += sprintf(p, "fcs_errors=%Lu\n", stats.fcs_err);
	}

	if (p - page > count) {
		ERROR("wrote %lu bytes (limit is %u)\n",
		      (unsigned long)(p - page), count);
		*eof = 1;
	}

	return (p - page);
}

static int procfs_read_encr(char *page, char **start, off_t off,
			    int count, int *eof, void *data)
{
	char *p = page;
	struct ndis_handle *handle = (struct ndis_handle *) data;
	int i, encr_status, auth_mode, infra_mode;
	NDIS_STATUS res;
	struct ndis_essid essid;
	mac_address ap_address;

	if (off != 0) {
		*eof = 1;
		return 0;
	}

	res = miniport_query_info(handle, OID_802_11_BSSID,
				  &ap_address, sizeof(ap_address));
	if (res)
		memset(ap_address, 0, ETH_ALEN);
	p += sprintf(p, "ap_address=%2.2X", ap_address[0]);
	for (i = 1 ; i < ETH_ALEN ; i++)
		p += sprintf(p, ":%2.2X", ap_address[i]);
	p += sprintf(p, "\n");

	res = miniport_query_info(handle, OID_802_11_SSID, &essid,
				  sizeof(essid));
	if (!res) {
		essid.essid[essid.length] = '\0';
		p += sprintf(p, "essid=%s\n", essid.essid);
	}

	res = miniport_query_int(handle, OID_802_11_ENCRYPTION_STATUS,
				 &encr_status);
	res |= miniport_query_int(handle, OID_802_11_AUTHENTICATION_MODE,
				  &auth_mode);

	if (!res) {
		int t = handle->encr_info.tx_key_index;
		p += sprintf(p, "tx_key=%u\n", handle->encr_info.tx_key_index);
		p += sprintf(p, "key=");
		if (handle->encr_info.keys[t].length > 0)
			for (i = 0; i < NDIS_ENCODING_TOKEN_MAX &&
				     i < handle->encr_info.keys[t].length;
			     i++)
				p += sprintf(p, "%2.2X",
					     handle->encr_info.keys[t].key[i]);
		else
			p += sprintf(p, "off");
		p += sprintf(p, "\n");

		p += sprintf(p, "status=%d\n", encr_status);
		p += sprintf(p, "auth_mode=%d\n", auth_mode);
	}

	res = miniport_query_int(handle, OID_802_11_INFRASTRUCTURE_MODE,
				 &infra_mode);
	p += sprintf(p, "mode=%s\n", (infra_mode == Ndis802_11IBSS) ?
		     "adhoc" : (infra_mode == Ndis802_11Infrastructure) ?
		     "managed" : "auto");
	if (p - page > count) {
		WARNING("wrote %lu bytes (limit is %u)",
			(unsigned long)(p - page), count);
		*eof = 1;
	}

	return (p - page);
}

static int procfs_read_hw(char *page, char **start, off_t off,
			  int count, int *eof, void *data)
{
	char *p = page;
	struct ndis_handle *handle = (struct ndis_handle *)data;
	struct ndis_configuration config;
	unsigned int power_mode;
	NDIS_STATUS res;
	ndis_tx_power_level tx_power;
	ULONG bit_rate;
	ndis_rts_threshold rts_threshold;
	ndis_fragmentation_threshold frag_threshold;
	ndis_antenna antenna;

	if (off != 0) {
		*eof = 1;
		return 0;
	}

	res = miniport_query_info(handle, OID_802_11_CONFIGURATION,
				  &config, sizeof(config));
	if (!res) {
		p += sprintf(p, "beacon_period=%u msec\n",
			     config.beacon_period);
		p += sprintf(p, "atim_window=%u msec\n", config.atim_window);
		p += sprintf(p, "frequency=%u kHZ\n", config.ds_config);
		p += sprintf(p, "hop_pattern=%u\n",
			     config.fh_config.hop_pattern);
		p += sprintf(p, "hop_set=%u\n",
			     config.fh_config.hop_set);
		p += sprintf(p, "dwell_time=%u msec\n",
			     config.fh_config.dwell_time);
	}

	res = miniport_query_info(handle, OID_802_11_TX_POWER_LEVEL,
				  &tx_power, sizeof(tx_power));
	if (!res)
		p += sprintf(p, "tx_power=%u mW\n", tx_power);

	res = miniport_query_info(handle, OID_GEN_LINK_SPEED,
				  &bit_rate, sizeof(bit_rate));
	if (!res)
		p += sprintf(p, "bit_rate=%u kBps\n", (u32)bit_rate / 10);

	res = miniport_query_info(handle, OID_802_11_RTS_THRESHOLD,
				  &rts_threshold, sizeof(rts_threshold));
	if (!res)
		p += sprintf(p, "rts_threshold=%u bytes\n", rts_threshold);

	res = miniport_query_info(handle, OID_802_11_FRAGMENTATION_THRESHOLD,
				  &frag_threshold, sizeof(frag_threshold));
	if (!res)
		p += sprintf(p, "frag_threshold=%u bytes\n", frag_threshold);

	res = miniport_query_int(handle, OID_802_11_POWER_MODE, &power_mode);
	if (!res)
		p += sprintf(p, "power_mode=%s\n",
			     (power_mode == NDIS_POWER_OFF) ?
			     "always_on" :
			     (power_mode == NDIS_POWER_MAX) ?
			     "max_savings" : "min_savings");

	res = miniport_query_info(handle, OID_802_11_NUMBER_OF_ANTENNAS,
				  &antenna, sizeof(antenna));
	if (!res)
		p += sprintf(p, "num_antennas=%u\n", antenna);

	res = miniport_query_info(handle, OID_802_11_TX_ANTENNA_SELECTED,
				  &antenna, sizeof(antenna));
	if (!res)
		p += sprintf(p, "tx_antenna=%u\n", antenna);

	res = miniport_query_info(handle, OID_802_11_RX_ANTENNA_SELECTED,
				  &antenna, sizeof(antenna));
	if (!res)
		p += sprintf(p, "rx_antenna=%u\n", antenna);

	if (p - page > count) {
		WARNING("wrote %lu bytes (limit is %u)",
			(unsigned long)(p - page), count);
		*eof = 1;
	}

	return (p - page);
}

static int procfs_read_settings(char *page, char **start, off_t off,
				int count, int *eof, void *data)
{
	char *p = page;
	struct ndis_handle *handle = (struct ndis_handle *)data;
	struct device_setting *setting;

	if (off != 0) {
		*eof = 1;
		return 0;
	}

	p += sprintf(p, "hangcheck_interval=%d\n",
		     (int)(handle->hangcheck_interval / HZ));

	list_for_each_entry(setting, &handle->device->settings, list) {
		p += sprintf(p, "%s=%s\n", setting->name, setting->value);
	}

	return (p - page);
}

static int procfs_write_settings(struct file *file, const char *buf,
				 unsigned long count, void *data)
{
	struct ndis_handle *handle = (struct ndis_handle *)data;
	char setting[MAX_PROC_STR_LEN], *p;

	if (count > MAX_PROC_STR_LEN)
		return -EINVAL;

	memset(setting, 0, sizeof(setting));
	if (copy_from_user(setting, buf, count))
		return -EFAULT;

	if ((p = strchr(setting, '\n')))
		*p = 0;

	if ((p = strchr(setting, '=')))
		*p = 0;

	if (!strcmp(setting, "hangcheck_interval")) {
		int i;

		if (!p)
			return -EINVAL;
		p++;
		i = simple_strtol(p, NULL, 10);
		hangcheck_del(handle);
		handle->hangcheck_interval = i * HZ;
		hangcheck_add(handle);
	} else if (!strcmp(setting, "suspend")) {
		int i;

		if (!p)
			return -EINVAL;
		p++;
		i = simple_strtol(p, NULL, 10);
		if (i <= 0 || i > 3)
			return -EINVAL;
		if (handle->device->bustype == NDIS_PCI_BUS)
			ndiswrapper_suspend_pci(handle->dev.pci, i);
	} else if (!strcmp(setting, "resume")) {
		if (handle->device->bustype == NDIS_PCI_BUS)
			ndiswrapper_resume_pci(handle->dev.pci);
	} else if (!strcmp(setting, "reinit")) {
		if (ndis_reinit(handle))
			return -EINVAL;
	} else if (!strcmp(setting, "power_profile")) {
		int i;
		struct miniport_char *miniport;
		ULONG profile_inf;

		if (!p)
			return -EINVAL;
		p++;
		i = simple_strtol(p, NULL, 10);
		if (i < 0 || i > 1)
			return -EINVAL;

		miniport = &handle->driver->miniport_char;
		if (!miniport->pnp_event_notify)
			return -EFAULT;

		/* 1 for AC and 0 for Battery */
		if (i)
			profile_inf = NdisPowerProfileAcOnLine;
		else
			profile_inf = NdisPowerProfileBattery;
		
		miniport->pnp_event_notify(handle->adapter_ctx,
					   NdisDevicePnPEventPowerProfileChanged,
					   &profile_inf, sizeof(profile_inf));
	} else if (!strcmp(setting, "auth_mode")) {
		int i;

		if (!p)
			return -EINVAL;
		p++;
		i = simple_strtol(p, NULL, 10);
		if (i <= 0 || i > 5)
			return -EINVAL;

		if (set_auth_mode(handle, i))
			return -EINVAL;
	} else if (!strcmp(setting, "encr_mode")) {
		int i;

		if (!p)
			return -EINVAL;
		p++;
		i = simple_strtol(p, NULL, 10);
		if (i <= 0 || i > 7)
			return -EINVAL;

		if (set_encr_mode(handle, i))
			return -EINVAL;
	} else {
		int res = -1;
		struct device_setting *dev_setting;

		if (!p)
			TRACEEXIT1(return -EINVAL);
		p++;
		DBGTRACE1("name='%s', value='%s'\n", setting, p);
		list_for_each_entry(dev_setting, &handle->device->settings,
				    list) {
			struct ndis_config_param *param;

			param = &dev_setting->config_param;
			if (!stricmp(dev_setting->name, setting)) {
				if (strlen(p) > MAX_NDIS_SETTING_VALUE_LEN)
					TRACEEXIT1(return -EINVAL);
				memset(dev_setting->value, 0,
				       MAX_NDIS_SETTING_VALUE_LEN);
				memcpy(dev_setting->value, p, strlen(p));
				if (param->type == NDIS_CONFIG_PARAM_STRING)
					RtlFreeUnicodeString(&param->data.ustring);
				param->type = NDIS_CONFIG_PARAM_NONE;
				res = 0;
			}
		}
		if (res)
			return -EINVAL;
	}

	return count;

}

int ndiswrapper_procfs_add_iface(struct ndis_handle *handle)
{
	struct net_device *dev = handle->net_dev;
	struct proc_dir_entry *proc_iface, *procfs_entry;

	handle->procfs_iface = NULL;
	if (ndiswrapper_procfs_entry == NULL)
		return -ENOMEM;

	proc_iface = proc_mkdir(dev->name, ndiswrapper_procfs_entry);

	handle->procfs_iface = proc_iface;

	if (proc_iface == NULL) {
		ERROR("%s", "Couldn't create proc directory");
		return -ENOMEM;
	}
	proc_iface->uid = proc_uid;
	proc_iface->gid = proc_gid;

	procfs_entry = create_proc_entry("hw", S_IFREG | S_IRUSR | S_IRGRP,
					 proc_iface);
	if (procfs_entry == NULL) {
		ERROR("%s", "Couldn't create proc entry for 'hw'");
		return -ENOMEM;
	} else {
		procfs_entry->uid = proc_uid;
		procfs_entry->gid = proc_gid;
		procfs_entry->data = handle;
		procfs_entry->read_proc = procfs_read_hw;
	}

	procfs_entry = create_proc_entry("stats", S_IFREG | S_IRUSR | S_IRGRP,
					 proc_iface);
	if (procfs_entry == NULL) {
		ERROR("%s", "Couldn't create proc entry for 'stats'");
		return -ENOMEM;
	} else {
		procfs_entry->uid = proc_uid;
		procfs_entry->gid = proc_gid;
		procfs_entry->data = handle;
		procfs_entry->read_proc = procfs_read_stats;
	}

	procfs_entry = create_proc_entry("encr", S_IFREG | S_IRUSR | S_IRGRP,
					 proc_iface);
	if (procfs_entry == NULL) {
		ERROR("%s", "Couldn't create proc entry for 'encr'");
		return -ENOMEM;
	} else {
		procfs_entry->uid = proc_uid;
		procfs_entry->gid = proc_gid;
		procfs_entry->data = handle;
		procfs_entry->read_proc = procfs_read_encr;
	}

	procfs_entry = create_proc_entry("settings", S_IFREG |
					 S_IRUSR | S_IRGRP |
					 S_IWUSR | S_IWGRP, proc_iface);
	if (procfs_entry == NULL) {
		ERROR("%s", "Couldn't create proc entry for 'settings'");
		return -ENOMEM;
	} else {
		procfs_entry->uid = proc_uid;
		procfs_entry->gid = proc_gid;
		procfs_entry->data = handle;
		procfs_entry->read_proc = procfs_read_settings;
		procfs_entry->write_proc = procfs_write_settings;
	}
	return 0;
}

void ndiswrapper_procfs_remove_iface(struct ndis_handle *handle)
{
	struct net_device *dev = handle->net_dev;
	struct proc_dir_entry *procfs_iface = handle->procfs_iface;

	if (procfs_iface == NULL)
		return;
	remove_proc_entry("hw", procfs_iface);
	remove_proc_entry("stats", procfs_iface);
	remove_proc_entry("encr", procfs_iface);
	remove_proc_entry("settings", procfs_iface);
	if (ndiswrapper_procfs_entry != NULL)
		remove_proc_entry(dev->name, ndiswrapper_procfs_entry);
	handle->procfs_iface = NULL;
}

#if defined DEBUG
int debug = DEBUG;

NW_MODULE_PARM_INT(debug, 0600);
MODULE_PARM_DESC(debug, "Debug level");

static int procfs_read_debug(char *page, char **start, off_t off,
			     int count, int *eof, void *data)
{
	char *p = page;

	if (off != 0) {
		*eof = 1;
		return 0;
	}

	p += sprintf(p, "%d\n", debug);

	return (p - page);
}

static int procfs_write_debug(struct file *file, const char *buf,
			      unsigned long count, void *data)
{
	char debug_level[MAX_PROC_STR_LEN];
	int i;

	if (count > MAX_PROC_STR_LEN)
		return -EINVAL;

	memset(debug_level, 0, sizeof(debug_level));
	if (copy_from_user(debug_level, buf, count))
		return -EFAULT;

	i = simple_strtol(debug_level, NULL, 10);
	if (i < 0 || i > DEBUG)
		return -EINVAL;

	debug = i;

	return count;
}
#endif

int ndiswrapper_procfs_init(void)
{
	ndiswrapper_procfs_entry = proc_mkdir(DRIVER_NAME, proc_net);
	if (ndiswrapper_procfs_entry == NULL) {
		ERROR("%s", "Couldn't create procfs directory");
		return -ENOMEM;
	}
	ndiswrapper_procfs_entry->uid = proc_uid;
	ndiswrapper_procfs_entry->gid = proc_gid;

#if defined DEBUG
	{
		struct proc_dir_entry *procfs_entry;

		procfs_entry = create_proc_entry("debug", S_IFREG | S_IRUSR | S_IRGRP,
						 ndiswrapper_procfs_entry);
		if (procfs_entry == NULL) {
			ERROR("%s", "Couldn't create proc entry for 'debug'");
			return -ENOMEM;
		} else {
			procfs_entry->uid = proc_uid;
			procfs_entry->gid = proc_gid;
			procfs_entry->read_proc  = procfs_read_debug;
			procfs_entry->write_proc = procfs_write_debug;
		}
	}
#endif

	return 0;
}

void ndiswrapper_procfs_remove(void)
{
	if (ndiswrapper_procfs_entry == NULL)
		return;
#if defined DEBUG
	remove_proc_entry("debug", ndiswrapper_procfs_entry);
#endif
	remove_proc_entry(DRIVER_NAME, proc_net);
}
