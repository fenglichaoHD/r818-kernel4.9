/*
 * Firmware debug interface for XRadio drivers
 *
 * Copyright (c) 2013
 * Xradio Technology Co., Ltd. <www.xradiotech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef SUPPORT_FW_DBG_INF

#include <linux/debugfs.h>

#include "xradio.h"
#include "hwio.h"
#include "fw_dbg_inf.h"

/* import function */
int wsm_fw_dbg(struct xradio_common *hw_priv, void *arg, size_t arg_size);

/* internally visible function */
static int fw_dbg_generic_open(struct inode *inode, struct file *file);

static ssize_t fw_dbg_sys_show_config(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos);
static int fw_dbg_sys_indicate_config(struct fwd_msg *p_ind);
static ssize_t fw_dbg_sys_show_cpu_load(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos);
static ssize_t fw_dbg_sys_show_cpu_load_enhance(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos);
static int fw_dbg_sys_indicate_cpu_load(struct fwd_msg *p_ind);
static int fw_dbg_sys_change_to_direct_mode(struct xradio_common *hw_priv);
static int fw_dbg_sys_change_to_queue_mode(struct xradio_common *hw_priv);
static int fw_dbg_sys_read(struct xradio_common *hw_priv, u32 addr,
						void *buf, size_t buf_len);
static int fw_dbg_sys_read_32(struct xradio_common *hw_priv, u32 addr, u32 *buf);
static int fw_dbg_sys_write_32(struct xradio_common *hw_priv, u32 addr, u32 val);
static int fw_dbg_sys_fw_dump_cross_read_32(struct xradio_common *hw_priv,
					u32 read_addr, u32 *val);
static int fw_dbg_sys_fw_dump(struct xradio_common *hw_priv,
					u32 read_addr, size_t read_len);
static int fw_dbg_sys_process_fw_dump(struct xradio_common *hw_priv);

static int fw_dbg_sys_parse_frame_trace(struct fwd_sys_frame_trace *p_trace);
static int fw_dbg_sys_parse_cmd_trace(struct fwd_sys_cmd_trace *p_trace);
static int fw_dbg_sys_parse_func_trace(struct fwd_sys_func_trace *p_trace);

static ssize_t fw_dbg_soc_show_lpclk_stat(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos);
static int fw_dbg_soc_parse_lpclk_stat(char *buf,
					struct fwd_soc_lpclk_stat *p_stat);

static ssize_t fw_dbg_pas_config_fiq_dump(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos);
static int fw_dbg_pas_indicate_fiq_dump(struct fwd_msg *p_ind);
static int fw_dbg_pas_parse_fiq_capture(struct fwd_pas_fiq_capture *p_cap);
static int fw_dbg_pas_parse_fiq_regs(u8 idx, u32 *regs, u32 *p_last_dump_time);

static ssize_t fw_dbg_pas_config_fiq_trace(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos);
static int fw_dbg_pas_indicate_fiq_trace(struct fwd_msg *p_ind);
static int fw_dbg_pas_parse_fiq_trace(struct fwd_pas_fiq_trace *p_fiq_trace);

static ssize_t fw_dbg_pas_config_ptcs_dump(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos);
static int fw_dbg_pas_indicate_ptcs_dump(struct fwd_msg *p_ind);
static int fw_dbg_pas_parse_ttcs(u32 *p_ttcs);
static int fw_dbg_pas_parse_recipe(u32 recipe);

static ssize_t fw_dbg_pas_config_hw_status(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos);
static int fw_dbg_pas_parse_hw_reg_ntd(u32 ntd_control);
static int fw_dbg_pas_parse_hw_reg_txc(u32 txc_status);
static int fw_dbg_pas_parse_hw_reg_rxc(struct fwd_pas_hw_reg *p_reg);
static int fw_dbg_pas_parse_hw_reg_ebm(struct fwd_pas_hw_reg *p_reg);

static ssize_t fw_dbg_pas_show_hw_stat(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos);
static int fw_dbg_pas_parse_hw_stat(char *buf, struct fwd_pas_hw_stat *p_stat);

static ssize_t fw_dbg_pas_config_force_mode(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos);

static ssize_t fw_dbg_pas_show_dur_entry(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos);
static int fw_dbg_pas_parse_dur_entry(struct fwd_pas_dur_entry *p_dur);

static ssize_t fw_dbg_pas_config_force_tx(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos);

static ssize_t fw_dbg_epta_show_time_line_ctrl(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos);
static ssize_t fw_dbg_epta_config_time_line_ctrl(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos);
static int fw_dbg_epta_indicate_time_line(struct fwd_msg *p_ind);
static ssize_t fw_dbg_epta_show_rf_stat_ctrl(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos);
static ssize_t fw_dbg_epta_config_rf_stat_ctrl(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos);
static int fw_dbg_epta_indicate_rf_stat(struct fwd_msg *p_ind);

static int xradio_fw_dbg_request(struct xradio_common *hw_priv,
	struct fwd_msg *p_req, u16 req_len, u16 buf_size, u16 req_id);

/* internally visible data */
static struct fwd_local fw_dbg = {0};

static const struct file_operations fw_dbg_sys_config = {
	.open = fw_dbg_generic_open,
	.read = fw_dbg_sys_show_config,
	.llseek = default_llseek,
};

static const struct file_operations fw_dbg_sys_cpu_load = {
	.open = fw_dbg_generic_open,
	.read = fw_dbg_sys_show_cpu_load,
	.write = fw_dbg_sys_show_cpu_load_enhance,
	.llseek = default_llseek,
};

static const struct file_operations fw_dbg_soc_lpclk_stat = {
	.open = fw_dbg_generic_open,
	.read = fw_dbg_soc_show_lpclk_stat,
	.llseek = default_llseek,
};

static const struct file_operations fw_dbg_pas_fiq_dump = {
	.open = fw_dbg_generic_open,
	.write = fw_dbg_pas_config_fiq_dump,
	.llseek = default_llseek,
};

static const struct file_operations fw_dbg_pas_fiq_trace = {
	.open = fw_dbg_generic_open,
	.write = fw_dbg_pas_config_fiq_trace,
	.llseek = default_llseek,
};

static const struct file_operations fw_dbg_pas_ptcs_dump = {
	.open = fw_dbg_generic_open,
	.write = fw_dbg_pas_config_ptcs_dump,
	.llseek = default_llseek,
};

static const struct file_operations fw_dbg_pas_hw_status = {
	.open = fw_dbg_generic_open,
	.write = fw_dbg_pas_config_hw_status,
	.llseek = default_llseek,
};

static const struct file_operations fw_dbg_pas_hw_stat = {
	.open = fw_dbg_generic_open,
	.read = fw_dbg_pas_show_hw_stat,
	.llseek = default_llseek,
};

static const struct file_operations fw_dbg_pas_force_mode = {
	.open = fw_dbg_generic_open,
	.write = fw_dbg_pas_config_force_mode,
	.llseek = default_llseek,
};

static const struct file_operations fw_dbg_pas_dur_entry = {
	.open = fw_dbg_generic_open,
	.write = fw_dbg_pas_show_dur_entry,
	.llseek = default_llseek,
};

static const struct file_operations fw_dbg_pas_force_tx = {
	.open = fw_dbg_generic_open,
	.write = fw_dbg_pas_config_force_tx,
	.llseek = default_llseek,
};

static const struct file_operations fw_dbg_epta_time_line_ctrl = {
	.open = fw_dbg_generic_open,
	.read = fw_dbg_epta_show_time_line_ctrl,
	.write = fw_dbg_epta_config_time_line_ctrl,
	.llseek = default_llseek,
};

static const struct file_operations fw_dbg_epta_rf_stat_ctrl = {
	.open = fw_dbg_generic_open,
	.read = fw_dbg_epta_show_rf_stat_ctrl,
	.write = fw_dbg_epta_config_rf_stat_ctrl,
	.llseek = default_llseek,
};

struct fwd_common fwd_priv;

static int fw_dbg_sys_init(struct xradio_common *hw_priv,
				struct dentry *fw_node)
{
	fw_dbg.dbgfs_sys = debugfs_create_dir("sys", fw_node);
	if (!fw_dbg.dbgfs_sys) {
		xradio_dbg(XRADIO_DBG_ERROR, "sys is not created.\n");
		goto err;
	}

	if (!debugfs_create_file("dbg_config", S_IRUSR | S_IWUSR,
				fw_dbg.dbgfs_sys, hw_priv, &fw_dbg_sys_config))
		goto err;

	if (!debugfs_create_file("cpu_load", S_IRUSR | S_IWUSR,
				fw_dbg.dbgfs_sys, hw_priv, &fw_dbg_sys_cpu_load))
		goto err;

	fwd_priv.sys_cpu_load_store.ind_write_idx = 0x0;
	fwd_priv.sys_cpu_load_store.cat_read_idx = 0x0;

	return 0;

err:
	xradio_dbg(XRADIO_DBG_ERROR, "%s faild\n", __func__);
	if (fw_dbg.dbgfs_sys)
		debugfs_remove_recursive(fw_dbg.dbgfs_sys);
	fw_dbg.dbgfs_sys = NULL;
	return 1;
}

static void fw_dbg_sys_deinit(void)
{
	if (fw_dbg.dbgfs_sys)
		debugfs_remove_recursive(fw_dbg.dbgfs_sys);
	fw_dbg.dbgfs_sys = NULL;
}

static int fw_dbg_sys_indicate(struct fwd_msg *p_ind)
{
	switch (p_ind->dbg_id & FWD_CMD_MINOR_ID_MASK) {
	case FWD_CMD_MINOR_ID_SYS_CONFIG:
		fw_dbg_sys_indicate_config(p_ind);
		break;
	case FWD_CMD_MINOR_ID_SYS_CPU_LOAD:
		fw_dbg_sys_indicate_cpu_load(p_ind);
		break;
	default:
		xradio_dbg(XRADIO_DBG_ALWY, "undefined sys minor id:0x%04x \n",
			(p_ind->dbg_id & FWD_CMD_MINOR_ID_MASK));
		break;
	}
	return 0;
}


static ssize_t fw_dbg_sys_show_config(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{
	struct xradio_common *hw_priv = file->private_data;
	char buf[1024];
	size_t size = 0;

	struct fwd_msg *p_msg;
	struct fwd_sys_config *p_sys_cfg;

	ssize_t msg_buf_size;
	ssize_t msg_req_size;

	sprintf(buf, "\nshow fw dbg sys config\n\n");

	msg_buf_size = sizeof(struct fwd_msg) + sizeof(struct fwd_sys_config);

	p_msg = xr_kzalloc(msg_buf_size, false);
	if (p_msg == NULL) {
		sprintf(buf, "%s" "but not enough memory to show.\n", buf);
		goto err;
	}

	p_sys_cfg = (struct fwd_sys_config *)(p_msg + 1);
	msg_req_size = sizeof(struct fwd_msg);

	if (xradio_fw_dbg_request(hw_priv, p_msg, msg_req_size, msg_buf_size,
		FWD_CMD_MAJOR_ID_SYS | FWD_CMD_MINOR_ID_SYS_CONFIG)) {
		sprintf(buf, "%s" "but cfm msg status error.\n", buf);
		goto err;
	}

	fwd_priv.sys_config = *p_sys_cfg;

	sprintf(buf, "%s" "frame trace: %08x, cmd trace: %08x, func trace: %08x\n"
		"fw dump: %08x, fiq dump: %08x, fiq trace: %08x\n",
		buf, p_sys_cfg->frm_trace_addr, p_sys_cfg->cmd_trace_addr,
		p_sys_cfg->func_trace_addr, p_sys_cfg->fw_dump_addr,
		p_sys_cfg->fiq_dump_addr, p_sys_cfg->fiq_trace_addr);

err:
	if (p_msg)
		kfree(p_msg);

	sprintf(buf, "%s\n", buf);
	size = strlen(buf);
	return simple_read_from_buffer(user_buf, count, ppos, buf, size);
}

static int fw_dbg_sys_indicate_config(struct fwd_msg *p_ind)
{
	struct fwd_sys_config *p_sys_cfg;

	if (p_ind->dbg_len !=
		(sizeof(struct fwd_msg) + sizeof(struct fwd_sys_config))) {

		xradio_dbg(XRADIO_DBG_ALWY,
			"sys config: error msg len %d\n",
			p_ind->dbg_len);

		goto err;
	}

	p_sys_cfg = (struct fwd_sys_config *)(p_ind + 1);

	fwd_priv.sys_config = *p_sys_cfg;
	fwd_priv.sys_dump_flag = FWD_SYS_FW_DUMP_FLAG_FRAME
					| FWD_SYS_FW_DUMP_FLAG_CMD
					| FWD_SYS_FW_DUMP_FLAG_FUNC
					| FWD_SYS_FW_DUMP_FLAG_FIQ_DUMP
					| FWD_SYS_FW_DUMP_FLAG_FIQ_TRACE;

	xradio_dbg(XRADIO_DBG_NIY,
		"frame trace: %08x, cmd trace: %08x, func trace: %08x\n"
		"fw dump: %08x, fiq dump : %08x, fiq trace: %08x\n",
		p_sys_cfg->frm_trace_addr, p_sys_cfg->cmd_trace_addr,
		p_sys_cfg->func_trace_addr, p_sys_cfg->fw_dump_addr,
		p_sys_cfg->fiq_dump_addr, p_sys_cfg->fiq_trace_addr);

err:
	return 0;
}

static ssize_t fw_dbg_sys_show_cpu_load(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[2000];
	size_t size = 0;

	int ret = 0;

	struct fwd_sys_cpu_load_store *p_store = &(fwd_priv.sys_cpu_load_store);
	u32 idx = 0;
	u32 num = 0;
	s32 cpu_proc_time;
	s32 total_time;
	s32 cpu_load;

	u32 max_num;
	u32 stored_num;

	sprintf(buf, "\nshow fw dbg sys cpu load\n\n");

	stored_num = p_store->ind_write_idx - p_store->cat_read_idx;

	if (stored_num == 0x0) {
		sprintf(buf, "%s" "catch none.\n", buf);
		goto end;
	}

	sprintf(buf, "%s" "idx|load|stat_time|proc_time|idle_time|"
			"evt_time|irq_time|fiq_time\n", buf);

	max_num = (stored_num > FWD_SYS_CPU_LOAD_CAT_MAX_NUM) ?
			FWD_SYS_CPU_LOAD_CAT_MAX_NUM : stored_num;

	for (num = 0; num < max_num; num++) {
		idx = num + p_store->cat_read_idx;
		cpu_proc_time = p_store->cap[idx].cpu_active_time
				- p_store->cap[idx].cpu_sleep_time;

		total_time = p_store->cap[idx].stat_total_time;

		if ((cpu_proc_time > 0x0) && (total_time != 0x0)) {
			cpu_load = (cpu_proc_time * 100 / total_time);
		} else {
			cpu_load = 0;
		}

		sprintf(buf, "%s" "%3d|%3d%%|%9d|%9d|%9d|%8d|%8d|%8d\n", buf,
			idx, cpu_load,
			p_store->cap[idx].stat_total_time,
			p_store->cap[idx].cpu_active_time,
			p_store->cap[idx].cpu_sleep_time,
			p_store->cap[idx].event_proc_time,
			p_store->cap[idx].irq_proc_time,
			p_store->cap[idx].fiq_proc_time);
	}

	p_store->cat_read_idx += num;

	if (p_store->cat_read_idx == p_store->ind_write_idx) {
		p_store->ind_write_idx = FWD_SYS_CPU_LOAD_CAP_MAX_NUM + 1;

		memset(&(fwd_priv.sys_cpu_load_store.cap[0]), 0,
			sizeof(fwd_priv.sys_cpu_load_store.cap));

		p_store->cat_read_idx = 0x0;
		p_store->ind_write_idx = 0x0;

		sprintf(buf, "%s" "\nshow end\n", buf);

	} else {
		sprintf(buf, "%s" "\nshow left: %d\n", buf,
			p_store->ind_write_idx - p_store->cat_read_idx);
	}

end:
	sprintf(buf, "%s\n", buf);
	size = strlen(buf);
	ret = simple_read_from_buffer(user_buf, count, ppos, buf, size);

	return ret;
}

static ssize_t fw_dbg_sys_show_cpu_load_enhance(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct xradio_common *hw_priv = file->private_data;
	char  buf[256] = {0};
	u16   buf_size = (count > 255 ? 255 : count);
	char *startptr = &buf[0];
	char *endptr   = NULL;

	u32 cmd;
	u32 idx;
	s32 cpu_proc_time;
	s32 total_time;
	s32 cpu_load;
	u32 *p_sub;
	char print_buf[512];
	u32 print_idx;

	struct fwd_sys_cpu_load_store *p_store = &(fwd_priv.sys_cpu_load_store);

	if (!count)
		return -EINVAL;
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	cmd = simple_strtoul(startptr, &endptr, 16);
	startptr = endptr + 1;

	xradio_dbg(XRADIO_DBG_ALWY, "show fw dbg sys cpu load: %d.\n\n", cmd);

	switch (cmd) {
	case 0x0:

		xradio_dbg(XRADIO_DBG_ALWY, "reset capture buffer.\n\n");

		p_store->ind_write_idx = FWD_SYS_CPU_LOAD_CAP_MAX_NUM + 1;

		memset(&(fwd_priv.sys_cpu_load_store), 0,
			sizeof(struct fwd_sys_cpu_load_store));

		p_store->cat_read_idx = 0x0;
		p_store->ind_write_idx = 0x0;
		break;

	case 0x1:

		xradio_dbg(XRADIO_DBG_ALWY, "show load and time.\n\n");

		xradio_dbg(XRADIO_DBG_ALWY,
				"idx|load|stat_time|proc_time|idle_time|"
				"evt_time|irq_time|fiq_time\n");

		for (idx = 0; idx < p_store->ind_write_idx; idx++) {
			cpu_proc_time = p_store->cap[idx].cpu_active_time
					- p_store->cap[idx].cpu_sleep_time;

			total_time = p_store->cap[idx].stat_total_time;

			if ((cpu_proc_time > 0x0) && (total_time != 0x0)) {
				cpu_load = (cpu_proc_time * 100 / total_time);
			} else {
				cpu_load = 0;
			}

			xradio_dbg(XRADIO_DBG_ALWY,
				"%3d|%3d%%|%9d|%9d|%9d|%8d|%8d|%8d\n",
				idx, cpu_load,
				p_store->cap[idx].stat_total_time,
				p_store->cap[idx].cpu_active_time,
				p_store->cap[idx].cpu_sleep_time,
				p_store->cap[idx].event_proc_time,
				p_store->cap[idx].irq_proc_time,
				p_store->cap[idx].fiq_proc_time);
		}
		break;

	case 0x2:

		xradio_dbg(XRADIO_DBG_ALWY, "show sub event time.\n\n");

		sprintf(print_buf, "idx");

		for (print_idx = 0x0; print_idx < 32; print_idx++) {
			sprintf(print_buf, "%s" "|%s%02d",
				print_buf, "evt", print_idx);
		}

		xradio_dbg(XRADIO_DBG_ALWY, "%s\n", print_buf);

		for (idx = 0; idx < p_store->ind_write_idx; idx++) {

			p_sub = &p_store->cap[idx].sub_event_proc_time[0];

			sprintf(print_buf, "%3d", idx);

			for (print_idx = 0x0; print_idx < 32; print_idx++) {
				sprintf(print_buf, "%s" "|%5d",
					print_buf, p_sub[print_idx]);
			}
			xradio_dbg(XRADIO_DBG_ALWY, "%s\n", print_buf);

		}
		break;

	case 0x3:

		xradio_dbg(XRADIO_DBG_ALWY, "show sub irq time.\n\n");

		sprintf(print_buf, "idx");

		for (print_idx = 0x0; print_idx < 32; print_idx++) {
			sprintf(print_buf, "%s" "|%s%02d",
				print_buf, "irq", print_idx);
		}

		xradio_dbg(XRADIO_DBG_ALWY, "%s\n", print_buf);

		for (idx = 0; idx < p_store->ind_write_idx; idx++) {

			p_sub = &p_store->cap[idx].sub_irq_proc_time[0];

			sprintf(print_buf, "%3d", idx);

			for (print_idx = 0x0; print_idx < 32; print_idx++) {
				sprintf(print_buf, "%s" "|%5d",
					print_buf, p_sub[print_idx]);
			}
			xradio_dbg(XRADIO_DBG_ALWY, "%s\n", print_buf);

		}
		break;

	case 0x4:

		xradio_dbg(XRADIO_DBG_ALWY, "show sub event count.\n\n");

		sprintf(print_buf, "idx");

		for (print_idx = 0x0; print_idx < 32; print_idx++) {
			sprintf(print_buf, "%s" "|%s%02d",
				print_buf, "evt", print_idx);
		}

		xradio_dbg(XRADIO_DBG_ALWY, "%s\n", print_buf);

		for (idx = 0; idx < p_store->ind_write_idx; idx++) {

			p_sub = &p_store->cap[idx].sub_event_proc_count[0];

			sprintf(print_buf, "%3d", idx);

			for (print_idx = 0x0; print_idx < 32; print_idx++) {
				sprintf(print_buf, "%s" "|%5d",
					print_buf, p_sub[print_idx]);
			}
			xradio_dbg(XRADIO_DBG_ALWY, "%s\n", print_buf);

		}
		break;

	case 0x5:

		xradio_dbg(XRADIO_DBG_ALWY, "show sub irq count.\n\n");

		sprintf(print_buf, "idx");

		for (print_idx = 0x0; print_idx < 32; print_idx++) {
			sprintf(print_buf, "%s" "|%s%02d",
				print_buf, "irq", print_idx);
		}

		xradio_dbg(XRADIO_DBG_ALWY, "%s\n", print_buf);

		for (idx = 0; idx < p_store->ind_write_idx; idx++) {

			p_sub = &p_store->cap[idx].sub_irq_proc_count[0];

			sprintf(print_buf, "%3d", idx);

			for (print_idx = 0x0; print_idx < 32; print_idx++) {
				sprintf(print_buf, "%s" "|%5d",
					print_buf, p_sub[print_idx]);
			}
			xradio_dbg(XRADIO_DBG_ALWY, "%s\n", print_buf);

		}
		break;

	case 0x6:
	{
		struct fwd_msg *p_msg;
		struct fwd_sys_cpu_load_config *p_cfg;

		ssize_t msg_buf_size;
		ssize_t msg_req_size;
		ssize_t msg_cfm_size;

		msg_req_size = sizeof(struct fwd_msg)
				+ sizeof(struct fwd_sys_cpu_load_config);
		msg_cfm_size = sizeof(struct fwd_msg);
		msg_buf_size = msg_req_size;

		p_msg = xr_kzalloc(msg_buf_size, false);
		if (p_msg == NULL) {
			xradio_dbg(XRADIO_DBG_ALWY, "but not enough memory to show.\n");
			goto err;
		}

		p_cfg = (struct fwd_sys_cpu_load_config *)(p_msg + 1);

		p_cfg->enable = simple_strtoul(startptr, &endptr, 0);
		startptr = endptr + 1;

		xradio_dbg(XRADIO_DBG_ALWY,
			"config fw dbg sys cpu load: %d.\n", p_cfg->enable);

		if (xradio_fw_dbg_request(hw_priv, p_msg, msg_req_size, msg_cfm_size,
			FWD_CMD_MAJOR_ID_SYS | FWD_CMD_MINOR_ID_SYS_CPU_LOAD)) {
			xradio_dbg(XRADIO_DBG_ALWY, "but cfm msg status error.\n");
			goto err;
		}

err:
		if (p_msg)
			kfree(p_msg);

		break;
	}
	default:
		xradio_dbg(XRADIO_DBG_ALWY, "not support this cmd.\n\n");
		break;
	}

	return count;
}

static int fw_dbg_sys_indicate_cpu_load(struct fwd_msg *p_ind)
{
	struct fwd_sys_cpu_load *p_load;
	u32 idx;

	if (p_ind->dbg_len !=
		(sizeof(struct fwd_msg) + sizeof(struct fwd_sys_cpu_load))) {

		xradio_dbg(XRADIO_DBG_ALWY,
			"sys cpu load: error msg len %d\n",
			p_ind->dbg_len);

		goto err;
	}

	p_load = (struct fwd_sys_cpu_load *)(p_ind + 1);

	idx = fwd_priv.sys_cpu_load_store.ind_write_idx;

	if (idx == 0x0) {
		xradio_dbg(XRADIO_DBG_ALWY, "sys cpu load: catch start.\n");
	}

	if (idx < FWD_SYS_CPU_LOAD_CAP_MAX_NUM) {
		fwd_priv.sys_cpu_load_store.cap[idx] = *p_load;
		fwd_priv.sys_cpu_load_store.ind_write_idx++;
	}

err:
	return 0;
}

static int fw_dbg_sys_change_to_direct_mode(struct xradio_common *hw_priv)
{
	int ret = 0;
	int i;
	u32 val32;

	/* Set wakeup bit in device */
	ret = xradio_reg_write_16(hw_priv, HIF_CONTROL_REG_ID,
			HIF_CTRL_WUP_BIT);
	if (SYS_WARN(ret)) {
		goto exit;
	}

	/* Wait for wakeup */
	for (i = 0; i < 300; i += 1 + i / 2) {
		ret = xradio_reg_read_32(hw_priv, HIF_CONTROL_REG_ID, &val32);
		if (ret < 0) {
			xradio_dbg(XRADIO_DBG_ERROR, "%s: Wait_for_wakeup: " \
				"can't read control register.\n", __func__);
			goto exit;
		}
		if (val32 & HIF_CTRL_RDY_BIT) {
			break;
		}
		msleep(i);
	}

	if ((val32 & HIF_CTRL_RDY_BIT) == 0) {
		xradio_dbg(XRADIO_DBG_ERROR, "%s: Wait for wakeup:" \
			"device is not responding.\n", __func__);
	}

	/* change to direct mode */
	ret = xradio_reg_read_32(hw_priv, HIF_CONFIG_REG_ID, &val32);
	if (ret < 0) {
		xradio_dbg(XRADIO_DBG_ERROR, "%s:HIF R/W -- " \
			   "reading CONFIG err, ret is %d!\n",
			   __func__, ret);
		goto exit;
	}
	ret = xradio_reg_write_32(hw_priv, HIF_CONFIG_REG_ID,
				   val32 | HIF_CONFIG_ACCESS_MODE_BIT);
	if (ret < 0) {
		xradio_dbg(XRADIO_DBG_ERROR, "%s:HIF R/W -- " \
			   "setting direct mode err, ret is %d!\n",
				__func__, ret);
		goto exit;
	}

exit:
	return ret;
}

static int fw_dbg_sys_change_to_queue_mode(struct xradio_common *hw_priv)
{
	int ret = 0;
	u32 val32;

	ret = xradio_reg_read_32(hw_priv, HIF_CONFIG_REG_ID, &val32);
	if (ret < 0) {
		xradio_dbg(XRADIO_DBG_ERROR, "%s:HIF R/W -- " \
			   "reading CONFIG err, ret is %d!\n",
			   __func__, ret);
		goto exit;
	}

	/* return to queue mode */
	ret = xradio_reg_write_32(hw_priv, HIF_CONFIG_REG_ID,
				  val32 & ~HIF_CONFIG_ACCESS_MODE_BIT);
	if (ret < 0) {
		xradio_dbg(XRADIO_DBG_ERROR, "%s:HIF R/W -- " \
			   "setting queue mode err, ret is %d!\n",
			   __func__, ret);
	}
exit:
	return ret;
}

static int fw_dbg_sys_read(struct xradio_common *hw_priv, u32 addr,
						void *buf, size_t buf_len)
{
	int ret = 0;
	int i;

	if (0xfff00000 == (addr & 0xfff00000)) {

		u32 *val = (u32 *)buf;
		addr = (addr & (~0xfff00000)) | 0x08000000;

		for (i = 0; i < buf_len>>2; i++) {
			ret = xradio_ahb_read_32(hw_priv, addr, &(val[i]));
			if (ret < 0) {
				xradio_dbg(XRADIO_DBG_ERROR, "%s:AHB read err, " \
					   "addr 0x%08x, ret=%d\n",
					   __func__, addr, ret);
				goto exit;
			}
			addr += 4;
		}
	} else if (0x09000000 == (addr & 0xfff00000)) {

		addr = (addr & (~0xfff00000)) | 0x09000000;

		ret = xradio_apb_read(hw_priv, addr, buf, buf_len);
		if (ret < 0) {
			xradio_dbg(XRADIO_DBG_ERROR, "%s:APB read err, " \
				   "addr 0x%08x, ret=%d\n",
				   __func__, addr, ret);
			goto exit;
		}
	} else {

		xradio_dbg(XRADIO_DBG_ERROR, "%s:unkown addr=0x%08x\n",
				__func__, addr);
		ret = 1;
	}

exit:
	return ret;
}

static int fw_dbg_sys_read_32(struct xradio_common *hw_priv, u32 addr, u32 *buf)
{
	int ret = 0;

	if (0xfff00000 == (addr & 0xfff00000)) {
		addr = (addr & (~0xfff00000)) | 0x08000000;
		ret = xradio_ahb_read_32(hw_priv, addr, buf);
		if (ret < 0) {
			xradio_dbg(XRADIO_DBG_ERROR, "%s:AHB read err, " \
				   "addr 0x%08x, ret=%d\n",
				   __func__, addr, ret);
			goto exit;
		}
	} else if (0x09000000 == (addr & 0xfff00000)) {
		addr = (addr & (~0xfff00000)) | 0x09000000;
		ret = xradio_apb_read_32(hw_priv, addr, buf);
		if (ret < 0) {
			xradio_dbg(XRADIO_DBG_ERROR, "%s:APB read err, " \
				   "addr 0x%08x, ret=%d\n",
				   __func__, addr, ret);
			goto exit;
		}
	} else {

		xradio_dbg(XRADIO_DBG_ERROR, "%s:unkown addr=0x%08x\n",
				__func__, addr);
		ret = 1;
	}

exit:
	return ret;
}

static int fw_dbg_sys_write_32(struct xradio_common *hw_priv, u32 addr, u32 val)
{
	int ret = 0;

	if (0xfff00000 == (addr & 0xfff00000)) {
		addr = (addr & (~0xfff00000)) | 0x08000000;
		ret = xradio_ahb_write_32(hw_priv, addr, val);
		if (ret < 0) {
			xradio_dbg(XRADIO_DBG_ERROR, "%s:AHB write err, " \
				   "addr 0x%08x, ret=%d\n",
				   __func__, addr, ret);
			goto exit;
		}
	} else if (0x09000000 == (addr & 0xfff00000)) {
		addr = (addr & (~0xfff00000)) | 0x09000000;
		ret = xradio_apb_write_32(hw_priv, addr, val);
		if (ret < 0) {
			xradio_dbg(XRADIO_DBG_ERROR, "%s:APB write err, " \
				   "addr 0x%08x, ret=%d\n",
				   __func__, addr, ret);
			goto exit;
		}
	} else {

		xradio_dbg(XRADIO_DBG_ERROR, "%s:unkown addr=0x%08x\n",
				__func__, addr);
		ret = 1;
	}

exit:
	return ret;
}

static int fw_dbg_sys_fw_dump_cross_read_32(struct xradio_common *hw_priv,
					u32 read_addr, u32 *val)
{
	u32 state_addr;
	u32 addr_addr;
	u32 data_addr;

	u32 fw_dump_state = 0x0;
	int ret = 0;
	u32 time_out = 0;

	state_addr = fwd_priv.sys_config.fw_dump_addr;
	addr_addr = fwd_priv.sys_config.fw_dump_addr + sizeof(u32);
	data_addr = fwd_priv.sys_config.fw_dump_addr + sizeof(u32) + sizeof(u32);

	/* check fw is wait */
	time_out = 0x0;
	while (time_out < 10) {
		if (fw_dbg_sys_read_32(hw_priv, state_addr, &fw_dump_state)) {
			ret = 1;
			goto exit;
		}

		if (fw_dump_state == FWD_SYS_FW_DUMP_STATE_WAIT)
			break;
		time_out++;
	}

	if (fw_dump_state != FWD_SYS_FW_DUMP_STATE_WAIT) {
		ret = 1;
		xradio_dbg(XRADIO_DBG_ALWY, "cross read: check wait error.\n");
		goto exit;
	}

	/* send request to fw */
	if (fw_dbg_sys_write_32(hw_priv, addr_addr, read_addr)) {
			ret = 1;
			xradio_dbg(XRADIO_DBG_ALWY, "cross read: write addr error.\n");
			goto exit;
	}

	if (fw_dbg_sys_write_32(hw_priv, state_addr,
				FWD_SYS_FW_DUMP_STATE_REQUEST)) {
			ret = 1;
			xradio_dbg(XRADIO_DBG_ALWY, "cross read: write request error.\n");
			goto exit;
	}

	/* check fw has finished read. */
	time_out = 0x0;
	while (time_out < 10) {
		if (fw_dbg_sys_read_32(hw_priv, state_addr, &fw_dump_state)) {
			ret = 1;
			goto exit;
		}

		if (fw_dump_state == FWD_SYS_FW_DUMP_STATE_DONE)
			break;
		time_out++;
	}

	if (fw_dump_state != FWD_SYS_FW_DUMP_STATE_DONE) {
		ret = 1;
		xradio_dbg(XRADIO_DBG_ALWY, "cross read: check done error.\n");
		goto exit;
	}

	/* read data from fw */
	if (fw_dbg_sys_read_32(hw_priv, data_addr, val)) {
		ret = 1;
		xradio_dbg(XRADIO_DBG_ALWY, "cross read: read data error.\n");
		goto exit;
	}

	/* reset fw state to wait */
	if (fw_dbg_sys_write_32(hw_priv, state_addr,
				FWD_SYS_FW_DUMP_STATE_WAIT)) {
		ret = 1;
		xradio_dbg(XRADIO_DBG_ALWY, "cross read: reset wait error.\n");
		goto exit;
	}

exit:
	return ret;

}

static int fw_dbg_sys_fw_dump(struct xradio_common *hw_priv,
					u32 read_addr, size_t read_len)
{
	u32 addr;
	u32 value;
	int ret = 0x0;
	for (addr = read_addr; addr < read_addr + read_len; addr += 4) {
		if (fw_dbg_sys_fw_dump_cross_read_32(hw_priv, addr, &value)) {
			ret = 1;
			xradio_dbg(XRADIO_DBG_ALWY, "fw dump: cross read error.\n");
			goto exit;
		}
		xradio_dbg(XRADIO_DBG_ALWY, "fw dump(%08x): %08x\n",
			addr, value);
	}

exit:
	return ret;
}

static int fw_dbg_sys_process_fw_dump(struct xradio_common *hw_priv)
{
	int ret = 0x0;

	xradio_dbg(XRADIO_DBG_ALWY, "fw dump: dump itcm.\n");
	if (fw_dbg_sys_fw_dump(hw_priv, 0x0, 0x20000)) {
		ret = 0x1;
		xradio_dbg(XRADIO_DBG_ALWY, "fw dump: dump itcm error.\n");
		goto exit;
	}

	xradio_dbg(XRADIO_DBG_ALWY, "fw dump: dump ahb ram.\n");
	if (fw_dbg_sys_fw_dump(hw_priv, 0xFFF00000, 0x8000)) {
		ret = 0x1;
		xradio_dbg(XRADIO_DBG_ALWY, "fw dump: dump ahb ram error.\n");
		goto exit;
	}

	xradio_dbg(XRADIO_DBG_ALWY, "fw dump: dump dtcm stack.\n");
	if (fw_dbg_sys_fw_dump(hw_priv, 0x0400B000, 0x1000)) {
		ret = 0x1;
		xradio_dbg(XRADIO_DBG_ALWY, "fw dump: dump dtcm stack error.\n");
		goto exit;
	}

exit:
	return ret;
}

static int fw_dbg_sys_parse_frame_trace(struct fwd_sys_frame_trace *p_trace)
{
	u32 idx = 0x0;
	u32 first_idx = p_trace->frames_index;

	xradio_dbg(XRADIO_DBG_ALWY, "frame dump, the oldest idx %2d:\n",
		p_trace->frames_index & 0x1f);

	for (idx = 0; idx < 32; idx++) {
		xradio_dbg(XRADIO_DBG_ALWY, "%02d: %08x\n",
			((idx + first_idx) & 0x1f),
			p_trace->frames_record[(idx + first_idx) & 0x1f]);
	}

	return 0;
}

static int fw_dbg_sys_parse_cmd_trace(struct fwd_sys_cmd_trace *p_trace)
{
	u32 idx = 0x0;
	u32 first_idx = p_trace->cmd_index;

	xradio_dbg(XRADIO_DBG_ALWY, "current cmd %04x, the oldest idx %2d: \n",
		p_trace->cmd_current, p_trace->cmd_index & 0xf);


	for (idx = 0; idx < 16; idx++) {
		xradio_dbg(XRADIO_DBG_ALWY, "%02d: %04x\n",
			((idx + first_idx) & 0xf),
			p_trace->cmds_record[(idx + first_idx) & 0xf]);
	}

	return 0;
}

static int fw_dbg_sys_parse_func_trace(struct fwd_sys_func_trace *p_trace)
{
	u16 i = 0x0;
	u16 idx;
	u32 handler;
	u32 info;
	u32 stamp;
	char buf[512];
	char place_name[8][10] = {
		"EVENT",
		"TIMER",
		"IRQ",
		"FIQ",
		"CPU_IDLE",
		"FIRST_UP",
		"GOTO_DOWN",
		"WAKE_UP",
	};

	xradio_dbg(XRADIO_DBG_ALWY, "func trace============================\n");

	sprintf(buf, "stage %02x:", p_trace->stage);

	for (i = 0; i < 8; i++) {
		if ((1 << i) & (p_trace->stage)) {
			sprintf(buf, "%s%s ", buf, place_name[i]);
		}
	}

	xradio_dbg(XRADIO_DBG_ALWY, "%s\n", buf);

	xradio_dbg(XRADIO_DBG_ALWY, "lr in irq: %08x\n", p_trace->lr);

	xradio_dbg(XRADIO_DBG_ALWY, " %-2s %-8s   %-8s  %-8s \n",
		"idx", "time", "func", "info");
	xradio_dbg(XRADIO_DBG_ALWY, "--------------------------------------\n");

	for (i = 0; i < 32; i++) {
		idx = ((p_trace->idx + i) & 0x1F);
		handler = p_trace->handler[idx];
		info = p_trace->info[idx];
		stamp = p_trace->stamp[idx];

		if ((info & FWD_SYS_FUNC_TRACE_MAKE_INFO_MASK)
			== FWD_SYS_FUNC_TRACE_MAKE_INFO_MASK) {

			xradio_dbg(XRADIO_DBG_ALWY,
				"[%03d %08x]: %08x [%08x - %5s, %2d]\n",
				idx, stamp, handler, info,
				place_name[FWD_SYS_FUNC_TRACE_GET_PLACE(info)],
				FWD_SYS_FUNC_TRACE_GET_PROIRITY(info));

		} else {
			xradio_dbg(XRADIO_DBG_ALWY,
				"[%02d %08x]: %08x [%08x]\n",
				idx, stamp, handler, info);
		}
	}

	return 0;
}

static int fw_dbg_soc_init(struct xradio_common *hw_priv,
				struct dentry *fw_node)
{
	fw_dbg.dbgfs_soc = debugfs_create_dir("soc", fw_node);
	if (!fw_dbg.dbgfs_soc) {
		xradio_dbg(XRADIO_DBG_ERROR, "soc is not created.\n");
		goto err;
	}

	if (!debugfs_create_file("lpclk_stat", S_IRUSR | S_IWUSR,
			fw_dbg.dbgfs_soc, hw_priv, &fw_dbg_soc_lpclk_stat))
		goto err;

	return 0;

err:
	xradio_dbg(XRADIO_DBG_ERROR, "%s faild\n", __func__);
	if (fw_dbg.dbgfs_soc)
		debugfs_remove_recursive(fw_dbg.dbgfs_soc);
	fw_dbg.dbgfs_soc = NULL;
	return 1;
}

static void fw_dbg_soc_deinit(void)
{
	if (fw_dbg.dbgfs_soc)
		debugfs_remove_recursive(fw_dbg.dbgfs_soc);
	fw_dbg.dbgfs_soc = NULL;
}


static ssize_t fw_dbg_soc_show_lpclk_stat(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{
	struct xradio_common *hw_priv = file->private_data;
	char buf[1024];
	size_t size = 0;

	struct fwd_msg *p_msg;
	struct fwd_soc_lpclk_stat *p_lpclk_stat;

	ssize_t msg_buf_size;
	ssize_t msg_req_size;

	sprintf(buf, "\nshow fw dbg soc lpclk stat\n\n");

	msg_buf_size = sizeof(struct fwd_msg)
			+ sizeof(struct fwd_soc_lpclk_stat);

	p_msg = xr_kzalloc(msg_buf_size, false);
	if (p_msg == NULL) {
		sprintf(buf, "%s" "but not enough memory to show.\n", buf);
		goto err;
	}

	p_lpclk_stat = (struct fwd_soc_lpclk_stat *)(p_msg + 1);
	msg_req_size = sizeof(struct fwd_msg);

	if (xradio_fw_dbg_request(hw_priv, p_msg, msg_req_size, msg_buf_size,
		FWD_CMD_MAJOR_ID_SOC | FWD_CMD_MINOR_ID_SOC_LPCLK_STAT)) {
		sprintf(buf, "%s" "but cfm msg status error.\n", buf);
		goto err;
	}

	fw_dbg_soc_parse_lpclk_stat(buf, p_lpclk_stat);

err:
	if (p_msg)
		kfree(p_msg);

	sprintf(buf, "%s\n", buf);
	size = strlen(buf);
	return simple_read_from_buffer(user_buf, count, ppos, buf, size);
}

static int fw_dbg_soc_parse_lpclk_stat(char *buf,
					struct fwd_soc_lpclk_stat *p_stat)
{
	s32 tsf_diff_avg;
	s32 sleep_time;
	s32 lp_clk_ative;
	s32 lp_clk_compensation;
	s32 lp_clk_sleep;
	s32 lp_clk_diff;

	tsf_diff_avg = p_stat->tsf_diff_cnt ?
			(p_stat->tsf_diff_sum/(s32)(p_stat->tsf_diff_cnt)) : 0;

	sleep_time = (p_stat->resume_time_cur - p_stat->suspend_time_cur);

	lp_clk_ative = p_stat->dpll_cnt_per_cal ?
			((p_stat->lpclk_cal_period * 160 * 1000000)
				/ p_stat->dpll_cnt_per_cal)
			: 0;

	lp_clk_compensation = sleep_time ?
			(s32)(p_stat->dpll_cnt_per_cal) * p_stat->tsf_diff_cur / sleep_time
			: 0;

	lp_clk_sleep = ((s32)(p_stat->dpll_cnt_per_cal) + lp_clk_compensation) ?
			((p_stat->lpclk_cal_period * 160 * 1000000)
				/ ((s32)(p_stat->dpll_cnt_per_cal) + lp_clk_compensation))
			: 0;

	lp_clk_diff = lp_clk_sleep - lp_clk_ative;

	sprintf(buf, "%s" "---lp clk statitics:\n"
		"tsf_diff_sum: %d \n"
		"tsf_diff_max: %d \n"
		"tsf_diff_cur: %d \n"
		"tsf_diff_cnt: %d \n"
		"dpll_cnt_per_cal: %d \n"
		"lpclk_cal_period: %d \n"
		"suspend_time_cur: %d \n"
		"resume_time_cur: %d \n"
		"---\n",
		buf,
		p_stat->tsf_diff_sum,
		p_stat->tsf_diff_max,
		p_stat->tsf_diff_cur,
		p_stat->tsf_diff_cnt,
		p_stat->dpll_cnt_per_cal,
		p_stat->lpclk_cal_period,
		p_stat->suspend_time_cur,
		p_stat->resume_time_cur);

	sprintf(buf, "%s" "---lp clk calculate:\n"
		"tsf_diff_avg(us): %d \n"
		"sleep time: %d\n"
		"lp clk freq(active) : %d \n"
		"compenstion : %d\n"
		"lp clk freq(sleep)  : %d \n"
		"lp clk diff : %d \n"
		"---\n",
		buf,
		tsf_diff_avg,
		sleep_time,
		lp_clk_ative,
		lp_clk_compensation,
		lp_clk_sleep,
		lp_clk_diff);

	return 0;
}

static int fw_dbg_lmc_init(struct xradio_common *hw_priv,
				struct dentry *fw_node)
{
	fw_dbg.dbgfs_lmc = debugfs_create_dir("lmc", fw_node);
	if (!fw_dbg.dbgfs_lmc) {
		xradio_dbg(XRADIO_DBG_ERROR, "lmc is not created.\n");
		goto err;
	}

	return 0;

err:
	xradio_dbg(XRADIO_DBG_ERROR, "%s faild\n", __func__);
	if (fw_dbg.dbgfs_lmc)
		debugfs_remove_recursive(fw_dbg.dbgfs_lmc);
	fw_dbg.dbgfs_lmc = NULL;
	return 1;
}

static void fw_dbg_lmc_deinit(void)
{
	if (fw_dbg.dbgfs_lmc)
		debugfs_remove_recursive(fw_dbg.dbgfs_lmc);
	fw_dbg.dbgfs_lmc = NULL;
}

static int fw_dbg_pas_init(struct xradio_common *hw_priv,
				struct dentry *fw_node)
{
	fw_dbg.dbgfs_pas = debugfs_create_dir("pas", fw_node);
	if (!fw_dbg.dbgfs_pas) {
		xradio_dbg(XRADIO_DBG_ERROR, "pas is not created.\n");
		goto err;
	}

	if (!debugfs_create_file("fiq_dump", S_IRUSR | S_IWUSR,
			fw_dbg.dbgfs_pas, hw_priv, &fw_dbg_pas_fiq_dump))
		goto err;

	if (!debugfs_create_file("fiq_trace", S_IRUSR | S_IWUSR,
			fw_dbg.dbgfs_pas, hw_priv, &fw_dbg_pas_fiq_trace))
		goto err;

	if (!debugfs_create_file("ptcs_dump", S_IRUSR | S_IWUSR,
			fw_dbg.dbgfs_pas, hw_priv, &fw_dbg_pas_ptcs_dump))
		goto err;

	if (!debugfs_create_file("hw_status", S_IRUSR | S_IWUSR,
			fw_dbg.dbgfs_pas, hw_priv, &fw_dbg_pas_hw_status))
		goto err;

	if (!debugfs_create_file("hw_stat", S_IRUSR | S_IWUSR,
			fw_dbg.dbgfs_pas, hw_priv, &fw_dbg_pas_hw_stat))
		goto err;

	if (!debugfs_create_file("force_mode", S_IRUSR | S_IWUSR,
			fw_dbg.dbgfs_pas, hw_priv, &fw_dbg_pas_force_mode))
		goto err;

	if (!debugfs_create_file("dur_entry", S_IRUSR | S_IWUSR,
			fw_dbg.dbgfs_pas, hw_priv, &fw_dbg_pas_dur_entry))
		goto err;

	if (!debugfs_create_file("force_tx", S_IRUSR | S_IWUSR,
			fw_dbg.dbgfs_pas, hw_priv, &fw_dbg_pas_force_tx))
		goto err;

	return 0;

err:
	xradio_dbg(XRADIO_DBG_ERROR, "%s faild\n", __func__);
	if (fw_dbg.dbgfs_pas)
		debugfs_remove_recursive(fw_dbg.dbgfs_pas);
	fw_dbg.dbgfs_pas = NULL;
	return 1;
}

static void fw_dbg_pas_deinit(void)
{
	if (fw_dbg.dbgfs_pas)
		debugfs_remove_recursive(fw_dbg.dbgfs_pas);
	fw_dbg.dbgfs_pas = NULL;
}

static int fw_dbg_pas_indicate(struct fwd_msg *p_ind)
{
	switch (p_ind->dbg_id & FWD_CMD_MINOR_ID_MASK) {
	case FWD_CMD_MINOR_ID_PAS_FIQ_DUMP:
		fw_dbg_pas_indicate_fiq_dump(p_ind);
		break;
	case FWD_CMD_MINOR_ID_PAS_FIQ_TRACE:
		fw_dbg_pas_indicate_fiq_trace(p_ind);
		break;
	case FWD_CMD_MINOR_ID_PAS_PTCS_DUMP:
		fw_dbg_pas_indicate_ptcs_dump(p_ind);
		break;
	default:
		xradio_dbg(XRADIO_DBG_ALWY, "fw dbg pas indicate: "
			"undefined sys minor id:0x%04x \n",
			(p_ind->dbg_id & FWD_CMD_MINOR_ID_MASK));
		break;
	}
	return 0;
}

static ssize_t fw_dbg_pas_config_fiq_dump(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct xradio_common *hw_priv = file->private_data;
	char  buf[256] = {0};
	u16   buf_size = (count > 255 ? 255 : count);
	char *startptr = &buf[0];
	char *endptr   = NULL;

	struct fwd_msg *p_msg;
	struct fwd_pas_fiq_dump_cfg *p_dump_cfg;

	ssize_t msg_buf_size;
	ssize_t msg_req_size;
	ssize_t msg_cfg_size;

	if (!count)
		return -EINVAL;
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	xradio_dbg(XRADIO_DBG_ALWY, "config fw dbg pas fiq dump. \n\n");

	msg_buf_size = sizeof(struct fwd_msg)
			+ sizeof(struct fwd_pas_fiq_dump_cfg);

	p_msg = xr_kzalloc(msg_buf_size, false);
	if (p_msg == NULL) {
		xradio_dbg(XRADIO_DBG_ALWY, "but not enough memory to show.\n");
		goto err;
	}

	p_dump_cfg = (struct fwd_pas_fiq_dump_cfg *)(p_msg + 1);
	msg_req_size = msg_buf_size;
	msg_cfg_size = sizeof(struct fwd_msg);

	p_dump_cfg->dump_type = simple_strtoul(startptr, &endptr, 0);
	startptr = endptr + 1;

	if (p_dump_cfg->dump_type == FWD_PAS_FIQ_DUMP_TYPE_POSITION) {
		p_dump_cfg->dump_operation =
			simple_strtoul(startptr, &endptr, 0);
		startptr = endptr + 1;

		p_dump_cfg->dump_position =
			simple_strtoul(startptr, &endptr, 16);
		startptr = endptr + 1;

		if (p_dump_cfg->dump_operation ==
			FWD_PAS_FIQ_DUMP_OPERATION_COUNTER) {
			p_dump_cfg->dump_max_cnt =
				simple_strtoul(startptr, &endptr, 0);
		} else {
			p_dump_cfg->dump_max_cnt = 0x0;
		}
	}

	if (xradio_fw_dbg_request(hw_priv, p_msg, msg_req_size, msg_cfg_size,
		FWD_CMD_MAJOR_ID_PAS | FWD_CMD_MINOR_ID_PAS_FIQ_DUMP)) {
		xradio_dbg(XRADIO_DBG_ALWY, "but cfm msg status error.\n");
		goto err;
	}

	xradio_dbg(XRADIO_DBG_ALWY,
		"type: %d, operation: %d, position: %d, max count: %d\n\n",
		p_dump_cfg->dump_type, p_dump_cfg->dump_operation,
		p_dump_cfg->dump_position, p_dump_cfg->dump_max_cnt);

err:
	if (p_msg)
		kfree(p_msg);

	return count;
}

static int fw_dbg_pas_indicate_fiq_dump(struct fwd_msg *p_ind)
{
	struct fwd_pas_fiq_dump *p_dump;
	struct fwd_pas_fiq_capture *p_cap;

	if (p_ind->dbg_len !=
		(sizeof(struct fwd_msg) + sizeof(struct fwd_pas_fiq_dump))) {

		xradio_dbg(XRADIO_DBG_ALWY,
			"pas fiq dump: error msg len %d\n",
			p_ind->dbg_len);

		goto end;
	}

	p_dump = (struct fwd_pas_fiq_dump *)(p_ind + 1);

	xradio_dbg(XRADIO_DBG_ALWY, "pas fiq dump position %08x\n",
						p_dump->dump_position);

	p_cap = ((struct fwd_pas_fiq_capture *)&(p_dump->cap));

	fw_dbg_pas_parse_fiq_capture(p_cap);

end:
	return 0;
}

static int fw_dbg_pas_parse_fiq_capture(struct fwd_pas_fiq_capture *p_cap)
{
	u8 parse_idx;
	u32 *regs;
	u32 last_dump_time;
	if (p_cap->dump_status != FWD_PAS_FIQ_DUMP_STATUS__TO_HOST) {

		xradio_dbg(XRADIO_DBG_ALWY,
			"pas fiq dump: dump status error %04x\n",
			p_cap->dump_status);

		goto end;
	}

	if ((p_cap->idx_status == FWD_PAS_FIQ_DUMP_IDX_STATUS__NOT_FULL)
			&& (p_cap->next_write_idx == 0x0)) {
		xradio_dbg(XRADIO_DBG_ALWY, "pas fiq dump:: no fiq.\n");
		goto end;
	}

	if (p_cap->idx_status == FWD_PAS_FIQ_DUMP_IDX_STATUS__FULL) {
		parse_idx = p_cap->next_write_idx;
		xradio_dbg(XRADIO_DBG_ALWY, "dump full: idx = %d.\n", parse_idx);
	} else {
		parse_idx = 0x0;
		xradio_dbg(XRADIO_DBG_ALWY, "dump not full: idx = %d.\n", parse_idx);
	}

	last_dump_time = p_cap->last_dump_time;

	do {
		regs = &(p_cap->regs[parse_idx][0]);

		fw_dbg_pas_parse_fiq_regs(parse_idx, regs, &last_dump_time);

		parse_idx++;

		if (parse_idx == FWD_PAS_FIQ_DUMP_MAX_NUM)
			parse_idx = 0x0;

	} while (parse_idx != p_cap->next_write_idx);

end:
	return 0;
}

static int fw_dbg_pas_parse_fiq_regs(u8 idx, u32 *regs, u32 *p_last_dump_time)
{
	u32 ntd_status = regs[0];
	u32 ebm_int_conten = regs[1];
	u32 *queue_control = &regs[2];
	u32 time_stamp = regs[6];

	xradio_dbg(XRADIO_DBG_ALWY, "========\n");

	xradio_dbg(XRADIO_DBG_ALWY,
		"[idx:%02d][ntd_status:0x%08x]\n", idx, ntd_status);

	xradio_dbg(XRADIO_DBG_ALWY, "[time: %08x][wait:%d us]\n",
			time_stamp, time_stamp - *p_last_dump_time);

	*p_last_dump_time = time_stamp;

	if (ntd_status & FWD_PAS_REG_NTD_STATUS_OVERFLOW)
		xradio_dbg(XRADIO_DBG_ALWY, "-- overflow-----------.\n");

	if (ntd_status & FWD_PAS_REG_NTD_STATUS_EMPTY)
		xradio_dbg(XRADIO_DBG_ALWY, "-- empty-----------.\n");

	if (ntd_status & FWD_PAS_REG_NTD_STATUS_TSF_REQ_SWITCH)
		xradio_dbg(XRADIO_DBG_ALWY, "-- tsf request switch (%d)--.\n",
			!!(ntd_status & FWD_PAS_REG_NTD_STATUS_TSF_REQ_SWITCH_STATUS));

	if (ntd_status & FWD_PAS_REG_NTD_STATUS_TSF_HW_EVENT_1)
		xradio_dbg(XRADIO_DBG_ALWY, "-- tsf hw event 1----------.\n");

	if (ntd_status & FWD_PAS_REG_NTD_STATUS_TSF_HW_EVENT_0)
		xradio_dbg(XRADIO_DBG_ALWY, "-- tsf hw event 0----------.\n");

	if ((ntd_status & FWD_PAS_REG_NTD_STATUS_PTCS_EVENT)
		|| (ntd_status & FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_BT_ABORT)) {

		u8 tx_request =
			(ntd_status & FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_REQUEST_MASK)
			>> FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_REQUEST_SHIFT;

		u8 tx_ac =
			(ntd_status & FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_EBM_SEQ_REQ_WIN_AC_MASK)
			>> FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_EBM_SEQ_REQ_WIN_AC_SHIFT;

		u8 in_txop =
			(ntd_status & FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_EBM_SEQ_REQ_IN_TXOP);

		u8 abort_status =
			(ntd_status & FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_ABORT_MASK)
			>> FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_ABORT_SHIFT;

		u8 ptcs_event =
			(ntd_status & FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_MASK)
			>> FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_SHIFT;

		u8 tx_bandwidth =
			(ntd_status & FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_TX_BANDWIDTH_MASK)
			>> FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_TX_BANDWIDTH_SHIFT;

		xradio_dbg(XRADIO_DBG_ALWY, "-- ptcs event---------.\n");


		if ((ptcs_event == FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_TTCS_END)
			|| (ptcs_event == FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_RECIPE_END)) {

			char buf_ptcs_event[30];
			char buf_tx_event[100];

			if (ptcs_event == FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_TTCS_END) {
				sprintf(buf_ptcs_event, "[log]ttcs end");
			} else {
				sprintf(buf_ptcs_event, "[log]recipe end");
			}

			switch (tx_request) {
			case FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_REQUEST_EBM_SEQ_REQ:
			{
				sprintf(buf_tx_event, "(ebm): ac = %d, in txop = %d.\n",
					tx_ac, in_txop);

				if (queue_control[tx_ac]
					& FWD_PAS_REG_SEQ_QUEUE_CONTROL_NOT_EMPTY) {

					u8 queue_full = queue_control[tx_ac]
								& FWD_PAS_REG_SEQ_QUEUE_CONTROL_FULL;

					u8 read_pointer = (queue_control[tx_ac]
								& FWD_PAS_REG_SEQ_QUEUE_CONTROL_READ_POINTER_MASK)
								>> FWD_PAS_REG_SEQ_QUEUE_CONTROL_READ_POINTER_SHIFT;

					u8 write_pointer = (queue_control[tx_ac]
								& FWD_PAS_REG_SEQ_QUEUE_CONTROL_WRITE_POINTER_MASK)
								>> FWD_PAS_REG_SEQ_QUEUE_CONTROL_WRITE_POINTER_SHIFT;

					u8 ack_expect = (queue_control[tx_ac]
								& FWD_PAS_REG_SEQ_QUEUE_CONTROL_ACK_EXPECT_MASK)
								>> FWD_PAS_REG_SEQ_QUEUE_CONTROL_ACK_EXPECT_SHIFT;

					u8 ack_rxed = (queue_control[tx_ac]
								& FWD_PAS_REG_SEQ_QUEUE_CONTROL_ACK_RXED_MASK)
								>> FWD_PAS_REG_SEQ_QUEUE_CONTROL_ACK_RXED_SHIFT;

					u8 t_flag = (queue_control[tx_ac]
								& FWD_PAS_REG_SEQ_QUEUE_CONTROL_T_FLAG_MASK)
								>> FWD_PAS_REG_SEQ_QUEUE_CONTROL_T_FLAG_SHIFT;

					u8 entry_idx = 0;

					xradio_dbg(XRADIO_DBG_ALWY,
						"[log]queue[%d][full:%d][read:%d][write:%d]\n",
						tx_ac, queue_full, read_pointer, write_pointer);

					for (entry_idx = 0; entry_idx < 4; entry_idx++) {
						u8 entry_ack_expect = !!(ack_expect & (1 << entry_idx));
						u8 entry_ack_rxed = !!(ack_rxed & (1 << entry_idx));
						u8 entry_t_flag = !!(t_flag & (1 << entry_idx));
						xradio_dbg(XRADIO_DBG_ALWY,
							"[log]entry[%d][AE:%d][AR:%d][T:%d]\n",
							entry_idx, entry_ack_expect,
							entry_ack_rxed, entry_t_flag);
					}

				}

				break;
			}

			case FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_REQUEST_HIGH_TSF_HW_EVENT_0:
				sprintf(buf_tx_event,
					"(gbm): tx_request = %d, tsf hw event 0.\n",
					tx_request);
				break;

			case FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_REQUEST_HIGH_TSF_HW_EVENT_1:
				sprintf(buf_tx_event,
					"(gbm): tx_request = %d, tsf hw event 1.\n",
					tx_request);
				break;

			case FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_REQUEST_LOW_BAR_IMM_0:
				sprintf(buf_tx_event,
					"(gbm): tx_request = %d, low BAR IMM inf 0.\n",
					tx_request);
				break;

			case FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_REQUEST_LOW_BAR_IMM_1:
				sprintf(buf_tx_event,
					"(gbm): tx_request = %d, low BAR IMM inf 1.\n",
					tx_request);
				break;

			case FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_REQUEST_HIGH_UNI_RXED_TXACK_0:
				sprintf(buf_tx_event,
					"(gbm): tx_request = %d, high Rxed TxAck inf 0.\n",
					tx_request);
				break;

			case FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_REQUEST_HIGH_UNI_RXED_TXACK_1:
				sprintf(buf_tx_event,
					"(gbm): tx_request = %d, high Rxed TxAck inf 1.\n",
					tx_request);
				break;

			case FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_REQUEST_HIGH_AGGREND_RSPBA_0:
				sprintf(buf_tx_event,
					"(gbm): tx_request = %d, high AGGR End Rsp BA inf 0.\n",
					tx_request);
				break;

			case FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_REQUEST_HIGH_AGGREND_RSPBA_1:
				sprintf(buf_tx_event,
					"(gbm): tx_request = %d, high AGGR End Rsp BA inf 1.\n",
					tx_request);
				break;

			case FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_REQUEST_HIGH_RTS_0:
				sprintf(buf_tx_event,
					"(gbm): tx_request = %d, high RTS inf 0.\n",
					tx_request);
				break;

			case FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_REQUEST_HIGH_RTS_1:
				sprintf(buf_tx_event,
					"(gbm): tx_request = %d, high RTS inf 1.\n",
					tx_request);
				break;

			default:
				sprintf(buf_tx_event, "(gbm): tx_request = %d.\n",
					tx_request);
				break;
			}
			xradio_dbg(XRADIO_DBG_ALWY, "%s%s", buf_ptcs_event, buf_tx_event);

			if (tx_bandwidth
				== FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_TX_BANDWIDTH_20M) {
				xradio_dbg(XRADIO_DBG_ALWY, "[log]tx bandwidth: 20M PPDU.\n");
			} else {
				xradio_dbg(XRADIO_DBG_ALWY, "[log]tx bandwidth: 40M PPDU.\n");
			}

		}

		if (ntd_status & FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_BT_ABORT) {
			if (tx_request == FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_REQUEST_EBM_SEQ_REQ)
				xradio_dbg(XRADIO_DBG_ALWY,
					"[log]bt abort(ebm): ac = %d, in txop = %d.\n",
					tx_ac, in_txop);
			else
				xradio_dbg(XRADIO_DBG_ALWY,
					"[log]bt abort(gbm): tx_request = %d.\n",
					tx_request);
		}

		if ((ptcs_event == FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_ABORT)
			|| (ntd_status & FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_BT_ABORT))
			switch (abort_status) {
			case FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_ABORT_READING_TTCS:
				xradio_dbg(XRADIO_DBG_ALWY,
					"[log]ptcs tx abort: reading ttcs.\n");
				break;
			case FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_ABORT_IFS_RUNNING:
				xradio_dbg(XRADIO_DBG_ALWY,
					"[log]ptcs tx abort: ifs running.\n");
				break;
			case FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_ABORT_IFS_COMPLETE:
				xradio_dbg(XRADIO_DBG_ALWY,
					"[log]ptcs tx abort: ifs complete.\n");
				break;
			case FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_ABORT_MEDIUM_BUSY:
				xradio_dbg(XRADIO_DBG_ALWY,
					"[log]ptcs tx abort: medium busy.\n");
				break;
			case FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_ABORT_BACKOFF_COMPLETE:
				xradio_dbg(XRADIO_DBG_ALWY,
					"[log]ptcs tx abort: back off complete.\n");
				break;
			case FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_ABORT_PHY_ERROR:
				xradio_dbg(XRADIO_DBG_ALWY,
					"[log]ptcs tx abort: phy error.\n");
				break;
			default:
				xradio_dbg(XRADIO_DBG_ALWY,
					"[log]ptcs tx abort: undefined.\n");
				break;
			}
	}

	if ((ntd_status & FWD_PAS_REG_NTD_STATUS_EBM_STATUS)
		|| (ntd_status & FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_BT_ABORT)) {

		u8 int_contention =
			(ebm_int_conten & FWD_PAS_REG_EBM_INT_CONTEN_INT_CONTENTION_MASK)
			>> FWD_PAS_REG_EBM_INT_CONTEN_INT_CONTENTION_SHIFT;
		u8 first_frame_error =
			(ebm_int_conten & FWD_PAS_REG_EBM_INT_FIRST_FRAME_ERROR_MASK)
			>> FWD_PAS_REG_EBM_INT_FIRST_FRAME_ERROR_SHIFT;
		u8 ack_fail =
			(ebm_int_conten & FWD_PAS_REG_EBM_INT_ACK_FAIL_MASK)
			>> FWD_PAS_REG_EBM_INT_ACK_FAIL_SHIFT;
		u8 tx_err =
			(ebm_int_conten & FWD_PAS_REG_EBM_INT_TX_ERROR_MASK)
			>> FWD_PAS_REG_EBM_INT_TX_ERROR_SHIFT;

		xradio_dbg(XRADIO_DBG_ALWY, "-- ebm status-----------.\n");

		if (int_contention) {
			u8 ac_idx;
			for (ac_idx = 0; ac_idx < 4; ac_idx++) {
				if (int_contention & (0x1 << ac_idx))
					break;
			}

			xradio_dbg(XRADIO_DBG_ALWY, "[log]int_contention: 0x%04x=%d\n",
					int_contention, ac_idx);

		}

		if (first_frame_error) {
			u8 ac_idx;
			for (ac_idx = 0; ac_idx < 4; ac_idx++) {
				if (first_frame_error & (0x1 << ac_idx))
					break;
			}

			xradio_dbg(XRADIO_DBG_ALWY, "[log]first_frame_error: 0x%04x=%d\n",
				first_frame_error, ac_idx);
		}

		if (ack_fail) {
			u8 ac_idx;
			for (ac_idx = 0; ac_idx < 4; ac_idx++) {
				if (ack_fail & (0x1 << ac_idx))
					break;
			}

			xradio_dbg(XRADIO_DBG_ALWY, "[log]ack_fail: 0x%04x=%d\n",
				ack_fail, ac_idx);
		}

		if (tx_err) {
			u8 ac_idx;
			for (ac_idx = 0; ac_idx < 4; ac_idx++) {
				if (tx_err & (0x1 << ac_idx))
					break;
			}

			xradio_dbg(XRADIO_DBG_ALWY, "[log]tx_err: 0x%04x=%d\n",
				tx_err, ac_idx);
		}
	}

	if (ntd_status & FWD_PAS_REG_NTD_STATUS_RX_EVENT) {
		u8 ack_fail =
			(ebm_int_conten & FWD_PAS_REG_EBM_INT_ACK_FAIL_MASK)
			>> FWD_PAS_REG_EBM_INT_ACK_FAIL_SHIFT;

		u8 tx_ac =
			(ntd_status & FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_EBM_SEQ_REQ_WIN_AC_MASK)
			>> FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_EBM_SEQ_REQ_WIN_AC_SHIFT;

		u16 rx_event =
			(ntd_status & FWD_PAS_REG_NTD_STATUS_RX_EVENT_MASK)
			>> FWD_PAS_REG_NTD_STATUS_RX_EVENT_SHIFT;

		xradio_dbg(XRADIO_DBG_ALWY, "-- rx event=%d-----------.\n",
				rx_event);

		if ((ack_fail)
				&& ((rx_event == FWD_PAS_REG_NTD_STATUS_RX_EVENT_RESP_TIMEOUT)
				|| (rx_event == FWD_PAS_REG_NTD_STATUS_RX_EVENT_CRC_ERROR))) {
			u8 ack_fail_ac;
			for (ack_fail_ac = 0; ack_fail_ac < 4; ack_fail_ac++) {
				if (ack_fail & (0x1 << ack_fail_ac))
					break;
			}

			if (queue_control[ack_fail_ac]
					& FWD_PAS_REG_SEQ_QUEUE_CONTROL_NOT_EMPTY) {

				u8 queue_full = queue_control[ack_fail_ac]
							& FWD_PAS_REG_SEQ_QUEUE_CONTROL_FULL;

				u8 read_pointer = (queue_control[ack_fail_ac]
							& FWD_PAS_REG_SEQ_QUEUE_CONTROL_READ_POINTER_MASK)
							>> FWD_PAS_REG_SEQ_QUEUE_CONTROL_READ_POINTER_SHIFT;

				u8 write_pointer = (queue_control[ack_fail_ac]
							& FWD_PAS_REG_SEQ_QUEUE_CONTROL_WRITE_POINTER_MASK)
							>> FWD_PAS_REG_SEQ_QUEUE_CONTROL_WRITE_POINTER_SHIFT;

				u8 ack_expect = (queue_control[ack_fail_ac]
							& FWD_PAS_REG_SEQ_QUEUE_CONTROL_ACK_EXPECT_MASK)
							>> FWD_PAS_REG_SEQ_QUEUE_CONTROL_ACK_EXPECT_SHIFT;

				u8 ack_rxed = (queue_control[ack_fail_ac]
							& FWD_PAS_REG_SEQ_QUEUE_CONTROL_ACK_RXED_MASK)
							>> FWD_PAS_REG_SEQ_QUEUE_CONTROL_ACK_RXED_SHIFT;

				u8 t_flag = (queue_control[ack_fail_ac]
							& FWD_PAS_REG_SEQ_QUEUE_CONTROL_T_FLAG_MASK)
							>> FWD_PAS_REG_SEQ_QUEUE_CONTROL_T_FLAG_SHIFT;

				u8 entry_idx = 0;

				xradio_dbg(XRADIO_DBG_ALWY,
					"[log]queue[%d][full:%d][read:%d][write:%d]\n",
					ack_fail_ac, queue_full, read_pointer, write_pointer);

				for (entry_idx = 0; entry_idx < 4; entry_idx++) {
					u8 entry_ack_expect = !!(ack_expect & (1 << entry_idx));
					u8 entry_ack_rxed = !!(ack_rxed & (1 << entry_idx));
					u8 entry_t_flag = !!(t_flag & (1 << entry_idx));
					xradio_dbg(XRADIO_DBG_ALWY,
						"[log]entry[%d][AE:%d][AR:%d][T:%d]\n",
						entry_idx, entry_ack_expect,
						entry_ack_rxed, entry_t_flag);
				}

			}

			xradio_dbg(XRADIO_DBG_ALWY,
				"[log]ack fail: ebm ac 0x%04x=%d, ntd ac = %d\n",
				ack_fail, ack_fail_ac, tx_ac);
		} else {
			if (rx_event < FWD_PAS_REG_NTD_STATUS_RX_EVENT_FRAME_ABORT)
				xradio_dbg(XRADIO_DBG_ALWY,
					"[log]aggr-end case(%d).\n", rx_event);
			else if (rx_event == FWD_PAS_REG_NTD_STATUS_RX_EVENT_FRAME_ABORT)
				xradio_dbg(XRADIO_DBG_ALWY, "[log]rx frame abort.\n");
			else if (rx_event == FWD_PAS_REG_NTD_STATUS_RX_EVENT_CRC_ERROR)
				xradio_dbg(XRADIO_DBG_ALWY, "[log]rx frame crc error.\n");
			else if (rx_event == FWD_PAS_REG_NTD_STATUS_RX_EVENT_ACK_RXED) {

					u8 queue_full = queue_control[tx_ac]
								& FWD_PAS_REG_SEQ_QUEUE_CONTROL_FULL;

					u8 read_pointer = (queue_control[tx_ac]
								& FWD_PAS_REG_SEQ_QUEUE_CONTROL_READ_POINTER_MASK)
								>> FWD_PAS_REG_SEQ_QUEUE_CONTROL_READ_POINTER_SHIFT;

					u8 write_pointer = (queue_control[tx_ac]
								& FWD_PAS_REG_SEQ_QUEUE_CONTROL_WRITE_POINTER_MASK)
								>> FWD_PAS_REG_SEQ_QUEUE_CONTROL_WRITE_POINTER_SHIFT;

					u8 ack_expect = (queue_control[tx_ac]
								& FWD_PAS_REG_SEQ_QUEUE_CONTROL_ACK_EXPECT_MASK)
								>> FWD_PAS_REG_SEQ_QUEUE_CONTROL_ACK_EXPECT_SHIFT;

					u8 ack_rxed = (queue_control[tx_ac]
								& FWD_PAS_REG_SEQ_QUEUE_CONTROL_ACK_RXED_MASK)
								>> FWD_PAS_REG_SEQ_QUEUE_CONTROL_ACK_RXED_SHIFT;

					u8 t_flag = (queue_control[tx_ac]
								& FWD_PAS_REG_SEQ_QUEUE_CONTROL_T_FLAG_MASK)
								>> FWD_PAS_REG_SEQ_QUEUE_CONTROL_T_FLAG_SHIFT;

					u8 entry_idx = 0;

					xradio_dbg(XRADIO_DBG_ALWY,
						"[log]queue[%d][full:%d][read:%d][write:%d]\n",
						tx_ac, queue_full, read_pointer, write_pointer);

					for (entry_idx = 0; entry_idx < 4; entry_idx++) {
						u8 entry_ack_expect = !!(ack_expect & (1 << entry_idx));
						u8 entry_ack_rxed = !!(ack_rxed & (1 << entry_idx));
						u8 entry_t_flag = !!(t_flag & (1 << entry_idx));
						xradio_dbg(XRADIO_DBG_ALWY,
							"[log]entry[%d][AE:%d][AR:%d][T:%d]\n",
							entry_idx, entry_ack_expect,
							entry_ack_rxed, entry_t_flag);
					}

				xradio_dbg(XRADIO_DBG_ALWY,
					"[log]rx ack success: ntd ac = %d\n", tx_ac);
			} else if (rx_event == FWD_PAS_REG_NTD_STATUS_RX_EVENT_MULTICAST_RXED)
				xradio_dbg(XRADIO_DBG_ALWY, "[log]rx multicase pkt.\n");
			else if (rx_event == FWD_PAS_REG_NTD_STATUS_RX_EVENT_BEACON_RXED)
				xradio_dbg(XRADIO_DBG_ALWY, "[log]rx beacon.\n");
			else if (rx_event == FWD_PAS_REG_NTD_STATUS_RX_EVENT_BA_IMM) {

				if (queue_control[tx_ac]
						& FWD_PAS_REG_SEQ_QUEUE_CONTROL_NOT_EMPTY) {

					u8 queue_full = queue_control[tx_ac]
								& FWD_PAS_REG_SEQ_QUEUE_CONTROL_FULL;

					u8 read_pointer = (queue_control[tx_ac]
								& FWD_PAS_REG_SEQ_QUEUE_CONTROL_READ_POINTER_MASK)
								>> FWD_PAS_REG_SEQ_QUEUE_CONTROL_READ_POINTER_SHIFT;

					u8 write_pointer = (queue_control[tx_ac]
								& FWD_PAS_REG_SEQ_QUEUE_CONTROL_WRITE_POINTER_MASK)
								>> FWD_PAS_REG_SEQ_QUEUE_CONTROL_WRITE_POINTER_SHIFT;

					u8 ack_expect = (queue_control[tx_ac]
								& FWD_PAS_REG_SEQ_QUEUE_CONTROL_ACK_EXPECT_MASK)
								>> FWD_PAS_REG_SEQ_QUEUE_CONTROL_ACK_EXPECT_SHIFT;

					u8 ack_rxed = (queue_control[tx_ac]
								& FWD_PAS_REG_SEQ_QUEUE_CONTROL_ACK_RXED_MASK)
								>> FWD_PAS_REG_SEQ_QUEUE_CONTROL_ACK_RXED_SHIFT;

					u8 t_flag = (queue_control[tx_ac]
								& FWD_PAS_REG_SEQ_QUEUE_CONTROL_T_FLAG_MASK)
								>> FWD_PAS_REG_SEQ_QUEUE_CONTROL_T_FLAG_SHIFT;

					u8 entry_idx = 0;

					xradio_dbg(XRADIO_DBG_ALWY,
						"[log]queue[%d][full:%d][read:%d][write:%d]\n",
						tx_ac, queue_full, read_pointer, write_pointer);

					for (entry_idx = 0; entry_idx < 4; entry_idx++) {
						u8 entry_ack_expect = !!(ack_expect & (1 << entry_idx));
						u8 entry_ack_rxed = !!(ack_rxed & (1 << entry_idx));
						u8 entry_t_flag = !!(t_flag & (1 << entry_idx));
						xradio_dbg(XRADIO_DBG_ALWY,
							"[log]entry[%d][AE:%d][AR:%d][T:%d]\n",
							entry_idx, entry_ack_expect,
							entry_ack_rxed, entry_t_flag);
					}

				}

				xradio_dbg(XRADIO_DBG_ALWY,
					"[log]rx BA success: ntd ac = %d\n", tx_ac);
			} else if (rx_event == FWD_PAS_REG_NTD_STATUS_RX_EVENT_RTS_0)
				xradio_dbg(XRADIO_DBG_ALWY, "[log]rx rts\n");
			else if (rx_event == FWD_PAS_REG_NTD_STATUS_RX_EVENT_CTS)
				xradio_dbg(XRADIO_DBG_ALWY, "[log]rx cts\n");
			else if (rx_event == FWD_PAS_REG_NTD_STATUS_RX_EVENT_RESP_TIMEOUT)
				xradio_dbg(XRADIO_DBG_ALWY, "[log]response timeout.\n");
			else if ((rx_event > FWD_PAS_REG_NTD_STATUS_RX_EVENT_RESERVED_1)
					&& (rx_event < FWD_PAS_REG_NTD_STATUS_RX_EVENT_RESP_TIMEOUT))
				xradio_dbg(XRADIO_DBG_ALWY,
					"[log]rx frame success(%d).\n", rx_event);
			else
				xradio_dbg(XRADIO_DBG_ALWY,
					"[log]undefined case(%d).\n", rx_event);
		}

		if (ntd_status & FWD_PAS_REG_NTD_STATUS_RX_EVENT_RESP_RX_STROED)
			xradio_dbg(XRADIO_DBG_ALWY, "[log]rx frame stored.\n");
	}

	if (ntd_status & FWD_PAS_REG_NTD_STATUS_PTCS_EVENT_BT_ABORT) {

		u8 queue_idx = 0;

		xradio_dbg(XRADIO_DBG_ALWY, "-- queue_control: bt abort-----------.\n");

		for (queue_idx = 0; queue_idx < 4; queue_idx++) {
			if (queue_control[queue_idx]
				& FWD_PAS_REG_SEQ_QUEUE_CONTROL_NOT_EMPTY) {

				u8 queue_full = queue_control[queue_idx]
							& FWD_PAS_REG_SEQ_QUEUE_CONTROL_FULL;

				u8 read_pointer = (queue_control[queue_idx]
							& FWD_PAS_REG_SEQ_QUEUE_CONTROL_READ_POINTER_MASK)
							>> FWD_PAS_REG_SEQ_QUEUE_CONTROL_READ_POINTER_SHIFT;

				u8 write_pointer = (queue_control[queue_idx]
							& FWD_PAS_REG_SEQ_QUEUE_CONTROL_WRITE_POINTER_MASK)
							>> FWD_PAS_REG_SEQ_QUEUE_CONTROL_WRITE_POINTER_SHIFT;

				u8 ack_expect = (queue_control[queue_idx]
							& FWD_PAS_REG_SEQ_QUEUE_CONTROL_ACK_EXPECT_MASK)
							>> FWD_PAS_REG_SEQ_QUEUE_CONTROL_ACK_EXPECT_SHIFT;

				u8 ack_rxed = (queue_control[queue_idx]
							& FWD_PAS_REG_SEQ_QUEUE_CONTROL_ACK_RXED_MASK)
							>> FWD_PAS_REG_SEQ_QUEUE_CONTROL_ACK_RXED_SHIFT;

				u8 t_flag = (queue_control[queue_idx]
							& FWD_PAS_REG_SEQ_QUEUE_CONTROL_T_FLAG_MASK)
							>> FWD_PAS_REG_SEQ_QUEUE_CONTROL_T_FLAG_SHIFT;

				u8 entry_idx = 0;

				xradio_dbg(XRADIO_DBG_ALWY,
					"[log]queue[%d][full:%d][read:%d][write:%d]\n",
					queue_idx, queue_full, read_pointer, write_pointer);

				for (entry_idx = 0; entry_idx < 4; entry_idx++) {
					u8 entry_ack_expect = !!(ack_expect & (1 << entry_idx));
					u8 entry_ack_rxed = !!(ack_rxed & (1 << entry_idx));
					u8 entry_t_flag = !!(t_flag & (1 << entry_idx));
					xradio_dbg(XRADIO_DBG_ALWY,
						"[log]entry[%d][AE:%d][AR:%d][T:%d]\n",
						entry_idx, entry_ack_expect,
						entry_ack_rxed, entry_t_flag);
				}

			}
		}
	}

	return 0;
}

static ssize_t fw_dbg_pas_config_fiq_trace(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct xradio_common *hw_priv = file->private_data;

	struct fwd_msg *p_msg;

	ssize_t msg_req_size;
	ssize_t msg_cfm_size;
	ssize_t msg_buf_size;

	xradio_dbg(XRADIO_DBG_ALWY, "config fw dbg pas fiq trace. \n\n");

	msg_req_size = sizeof(struct fwd_msg);
	msg_cfm_size = msg_req_size;
	msg_buf_size = msg_req_size;

	p_msg = xr_kzalloc(msg_buf_size, false);
	if (p_msg == NULL) {
		xradio_dbg(XRADIO_DBG_ALWY, "but not enough memory to show.\n");
		goto err;
	}

	if (xradio_fw_dbg_request(hw_priv, p_msg, msg_req_size, msg_cfm_size,
		FWD_CMD_MAJOR_ID_PAS | FWD_CMD_MINOR_ID_PAS_FIQ_TRACE)) {
		xradio_dbg(XRADIO_DBG_ALWY, "but cfm msg status error.\n");
		goto err;
	}

err:
	if (p_msg)
		kfree(p_msg);

	return count;
}

static int fw_dbg_pas_indicate_fiq_trace(struct fwd_msg *p_ind)
{
	struct fwd_pas_fiq_trace *p_fiq_trace;

	if (p_ind->dbg_len !=
		(sizeof(struct fwd_msg) + sizeof(struct fwd_pas_fiq_trace))) {

		xradio_dbg(XRADIO_DBG_ALWY,
			"pas fiq trace: error msg len %d\n",
			p_ind->dbg_len);

		goto end;
	}

	p_fiq_trace = (struct fwd_pas_fiq_trace *)(p_ind + 1);

	fw_dbg_pas_parse_fiq_trace(p_fiq_trace);

end:
	return 0;
}

static int fw_dbg_pas_parse_fiq_trace(struct fwd_pas_fiq_trace *p_fiq_trace)
{
	u8 parse_idx;
	u32 *regs;

	if (p_fiq_trace->dump_status != FWD_PAS_FIQ_TRACE_STATUS__TO_HOST) {
		xradio_dbg(XRADIO_DBG_ALWY,
			"pas fiq trace: dump status error %04x\n",
			p_fiq_trace->dump_status);
		goto end;
	}

	if ((p_fiq_trace->idx_status == FWD_PAS_FIQ_TRACE_IDX_STATUS__NOT_FULL)
			&& (p_fiq_trace->next_write_idx == 0x0)) {
		xradio_dbg(XRADIO_DBG_ALWY, "pas fiq trace: no fiq trace.\n");
		goto end;
	}

	if (p_fiq_trace->idx_status == FWD_PAS_FIQ_TRACE_IDX_STATUS__FULL) {
		parse_idx = p_fiq_trace->next_write_idx;
		xradio_dbg(XRADIO_DBG_ALWY,
			"trace full: idx = %d. last time = 0x%08x, unit:us\n",
			parse_idx, p_fiq_trace->fiq_last_end_time);
	} else {
		parse_idx = 0x0;
		xradio_dbg(XRADIO_DBG_ALWY,
			"trace not full: idx = %d. last time = 0x%08x, unit:us\n",
			parse_idx, p_fiq_trace->fiq_last_end_time);
	}

	do {
		regs = &(p_fiq_trace->regs[parse_idx][0]);

		xradio_dbg(XRADIO_DBG_ALWY,
			"fiq trace[idx:%02d]================\n", parse_idx);

		xradio_dbg(XRADIO_DBG_ALWY,
			"time: [last: %08x][enter: %08x][leave: %08x][wait:%2d]\n",
			regs[0], regs[1], regs[2], (regs[1] - regs[0]));

		xradio_dbg(XRADIO_DBG_ALWY,
			"stat: [time:%2d][cnt:%2d][0x%08x - 0x%08x]\n",
			(regs[2] - regs[1]), regs[3], regs[4], regs[5]);

		parse_idx++;

		if (parse_idx == FWD_PAS_FIQ_TRACE_MAX_NUM)
			parse_idx = 0x0;

	} while (parse_idx != p_fiq_trace->next_write_idx);

end:
	return 0;
}

static ssize_t fw_dbg_pas_config_ptcs_dump(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct xradio_common *hw_priv = file->private_data;
	char  buf[256] = {0};
	u16   buf_size = (count > 255 ? 255 : count);
	char *startptr = &buf[0];
	char *endptr   = NULL;

	struct fwd_msg *p_msg;
	struct fwd_pas_ptcs_dump_cfg *p_dump_cfg;

	ssize_t msg_buf_size;
	ssize_t msg_req_size;
	ssize_t msg_cfm_size;

	if (!count)
		return -EINVAL;
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	xradio_dbg(XRADIO_DBG_ALWY, "config fw dbg pas ptcs dump. \n\n");

	msg_req_size = sizeof(struct fwd_msg)
			+ sizeof(struct fwd_pas_fiq_dump_cfg);
	msg_cfm_size = sizeof(struct fwd_msg);
	msg_buf_size = msg_req_size;

	p_msg = xr_kzalloc(msg_buf_size, false);
	if (p_msg == NULL) {
		xradio_dbg(XRADIO_DBG_ALWY, "but not enough memory to show.\n");
		goto err;
	}

	p_dump_cfg = (struct fwd_pas_ptcs_dump_cfg *)(p_msg + 1);

	p_dump_cfg->dump_position = simple_strtoul(startptr, &endptr, 16);
	startptr = endptr + 1;

	p_dump_cfg->dump_operation = simple_strtoul(startptr, &endptr, 0);
	startptr = endptr + 1;

	if (p_dump_cfg->dump_operation == FWD_PAS_FIQ_DUMP_OPERATION_COUNTER) {
		p_dump_cfg->dump_max_cnt = simple_strtoul(startptr, &endptr, 0);
	} else {
		p_dump_cfg->dump_max_cnt = 0x0;
	}

	if (xradio_fw_dbg_request(hw_priv, p_msg, msg_req_size, msg_cfm_size,
		FWD_CMD_MAJOR_ID_PAS | FWD_CMD_MINOR_ID_PAS_PTCS_DUMP)) {
		xradio_dbg(XRADIO_DBG_ALWY, "but cfm msg status error.\n");
		goto err;
	}

	xradio_dbg(XRADIO_DBG_ALWY,
		"position: 0x%x, operation: %d, max count: %d\n\n",
		p_dump_cfg->dump_position, p_dump_cfg->dump_operation,
		p_dump_cfg->dump_max_cnt);

err:
	if (p_msg)
		kfree(p_msg);

	return count;
}

static int fw_dbg_pas_indicate_ptcs_dump(struct fwd_msg *p_ind)
{
	struct fwd_pas_ptcs_dump *p_dump;

	u32 *p_ptcs;
	u32 *p_recipe;
	u32 idx;

	if (p_ind->dbg_len !=
		(sizeof(struct fwd_msg) + sizeof(struct fwd_pas_ptcs_dump))) {

		xradio_dbg(XRADIO_DBG_ALWY,
			"pas ptcs dump: error msg len %d\n",
			p_ind->dbg_len);

		goto end;
	}

	p_dump = (struct fwd_pas_ptcs_dump *)(p_ind + 1);

	xradio_dbg(XRADIO_DBG_ALWY, "ptcs dump (0x%x) ===========.\n",
				p_dump->dump_position);

	switch (p_dump->dump_position) {
	case FWD_PAS_PTCS_DUMP_POSITION_PREWRITE:

		xradio_dbg(XRADIO_DBG_ALWY, "dump recipe only.\n");

		p_recipe = (u32 *)&(p_dump->dump_data[0]);

		for (idx = 0; idx < 256; idx++) {
			fw_dbg_pas_parse_recipe(p_recipe[idx]);

			if (p_recipe[idx] == 0xf0000000)
				break;
		}

		break;

	case FWD_PAS_PTCS_DUMP_POSITION_AGGR_BUF:

		xradio_dbg(XRADIO_DBG_ALWY, "dump aggr buf.\n");

		p_recipe = (u32 *)&(p_dump->dump_data[0]);

		for (idx = 0; idx < 256; idx++) {
			fw_dbg_pas_parse_recipe(p_recipe[idx]);
			if (p_recipe[idx] == 0xe4000000)
				break;
		}

		break;

	default:

		xradio_dbg(XRADIO_DBG_ALWY, "dump ptcs.\n");

		p_ptcs = (u32 *)&(p_dump->dump_data[0]);
		p_recipe = (u32 *)&(p_dump->dump_data[3]);

		fw_dbg_pas_parse_ttcs(p_ptcs);

		for (idx = 0; idx < 253; idx++) {
			fw_dbg_pas_parse_recipe(p_recipe[idx]);

		if ((p_recipe[idx] & 0xff000000) == 0x65000000)
			break;

		if ((p_recipe[idx] & 0xff000000) == 0x71000000)
			break;

		if (p_recipe[idx] == 0xf0000000)
			break;
		}

		break;
	}

end:
	return 0;
}

static int fw_dbg_pas_parse_ttcs(u32 *p_ttcs)
{
	u32 ifsa = p_ttcs[0];
	u32 backoff = p_ttcs[1];
	u32 resp_timeout = p_ttcs[2];

	u32 rx_event_select;
	u32 tx_bandwidth;
	u32 tx_scheme;
	u32 enable_rx_event;
	u32 backoff_slots;

	u32 resp_timeout_in_1_8us;
	u32 wait_for_resp;
	u32 set_ifs_complete_flags;
	u32 reset_resp_timeout_on_cca_busy;
	u32 resp_timeout_adj_ht40;
	u32 resp_timeout_event_disable;
	u32 enable_recipe_end_event;
	u32 enable_recipe;
	u32 requeue_ptcs_if_aborted;
	u32 enable_ttcs_end_event;
	u32 abort_recipe_on_new_ntd_req;

	rx_event_select = FWD_GET_REG_SECTION(backoff, 0x1F, 0);
	tx_bandwidth = FWD_GET_REG_SECTION(backoff, 0x1, 5);
	tx_scheme = FWD_GET_REG_SECTION(backoff, 0x1, 6);
	enable_rx_event = FWD_GET_REG_SECTION(backoff, 0x1, 7);
	backoff_slots = FWD_GET_REG_SECTION(backoff, 0xFFF, 10);

	resp_timeout_in_1_8us = FWD_GET_REG_SECTION(resp_timeout, 0x1FFF, 0);
	wait_for_resp = FWD_GET_REG_SECTION(resp_timeout, 0x1, 13);
	set_ifs_complete_flags = FWD_GET_REG_SECTION(resp_timeout, 0x1, 14);
	reset_resp_timeout_on_cca_busy = FWD_GET_REG_SECTION(resp_timeout, 0x1, 15);
	resp_timeout_adj_ht40 = FWD_GET_REG_SECTION(resp_timeout, 0x3FF, 16);
	resp_timeout_event_disable = FWD_GET_REG_SECTION(resp_timeout, 0x1, 26);
	enable_recipe_end_event = FWD_GET_REG_SECTION(resp_timeout, 0x1, 27);
	enable_recipe = FWD_GET_REG_SECTION(resp_timeout, 0x1, 28);
	requeue_ptcs_if_aborted = FWD_GET_REG_SECTION(resp_timeout, 0x1, 29);
	enable_ttcs_end_event = FWD_GET_REG_SECTION(resp_timeout, 0x1, 30);
	abort_recipe_on_new_ntd_req = FWD_GET_REG_SECTION(resp_timeout, 0x1, 31);

	xradio_dbg(XRADIO_DBG_ALWY,
			"ifsa            : %08x--------------------\n", ifsa);
	xradio_dbg(XRADIO_DBG_ALWY,
			"backoff         : %08x--------------------\n", backoff);

	xradio_dbg(XRADIO_DBG_ALWY, "rx event: %d \n", rx_event_select);

	if (tx_bandwidth)
		xradio_dbg(XRADIO_DBG_ALWY, "tx bw: 40M, tx scheme: %d \n", tx_scheme);
	else
		xradio_dbg(XRADIO_DBG_ALWY, "tx bw: 20M, tx scheme: %d \n", tx_scheme);

	if (enable_rx_event)
		xradio_dbg(XRADIO_DBG_ALWY,
			"enable rx event select: override PAC_TIM_EBM_EVENT_SEL\n");

	xradio_dbg(XRADIO_DBG_ALWY, "backoff (slots): %d \n", backoff_slots);

	xradio_dbg(XRADIO_DBG_ALWY,
			"response_timeout: %08x--------------------\n", resp_timeout);

	xradio_dbg(XRADIO_DBG_ALWY,
		"reponse timeout (1/8us) : %d\n", resp_timeout_in_1_8us);

	xradio_dbg(XRADIO_DBG_ALWY,
		"reponse timeout ht40 adjust(1/8us) : %d\n", resp_timeout_adj_ht40);

	if (wait_for_resp)
		xradio_dbg(XRADIO_DBG_ALWY, "wait for response \n");

	if (set_ifs_complete_flags)
		xradio_dbg(XRADIO_DBG_ALWY,
			"set ifs complete flag on reponse timeout \n");

	if (reset_resp_timeout_on_cca_busy)
		xradio_dbg(XRADIO_DBG_ALWY,
			"reset response timeout on CCA busy \n");

	if (resp_timeout_event_disable)
		xradio_dbg(XRADIO_DBG_ALWY, "response timeout event disable \n");

	if (enable_recipe_end_event)
		xradio_dbg(XRADIO_DBG_ALWY, "enable recipe end event \n");

	if (enable_recipe)
		xradio_dbg(XRADIO_DBG_ALWY, "enable recipe \n");

	if (requeue_ptcs_if_aborted)
		xradio_dbg(XRADIO_DBG_ALWY, "re-queue ptcs if aborted \n");

	if (enable_ttcs_end_event)
		xradio_dbg(XRADIO_DBG_ALWY, "enable ttcs end event \n");

	if (abort_recipe_on_new_ntd_req)
		xradio_dbg(XRADIO_DBG_ALWY, "abort recipe on new ntd request \n");

	return 0;
}

static int fw_dbg_pas_parse_recipe(u32 recipe)
{
	xradio_dbg(XRADIO_DBG_ALWY, "%08x\n", recipe);
	return 0;
}

static ssize_t fw_dbg_pas_config_hw_status(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct xradio_common *hw_priv = file->private_data;
	char  buf[256] = {0};
	u16   buf_size = (count > 255 ? 255 : count);
	char *startptr = &buf[0];
	char *endptr   = NULL;

	u32 cmd;

	struct fwd_msg *p_msg;
	struct fwd_pas_hw_reg *p_hw_reg;

	ssize_t msg_buf_size;
	ssize_t msg_req_size;
	ssize_t msg_cfm_size;

	if (!count)
		return -EINVAL;
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	cmd = simple_strtoul(startptr, &endptr, 0);
	startptr = endptr + 1;

	xradio_dbg(XRADIO_DBG_ALWY, "show fw dbg pas hw status. \n\n");

	xradio_dbg(XRADIO_DBG_ALWY, "cmd: %d\n", cmd);

	msg_buf_size = sizeof(struct fwd_msg)
			+ sizeof(struct fwd_pas_hw_reg);

	p_msg = xr_kzalloc(msg_buf_size, false);
	if (p_msg == NULL) {
		xradio_dbg(XRADIO_DBG_ALWY, "but not enough memory to show.\n");
		goto err;
	}

	p_hw_reg = (struct fwd_pas_hw_reg *)(p_msg + 1);
	msg_req_size = sizeof(struct fwd_msg);
	msg_cfm_size = msg_buf_size;

	if (xradio_fw_dbg_request(hw_priv, p_msg, msg_req_size, msg_cfm_size,
		FWD_CMD_MAJOR_ID_PAS | FWD_CMD_MINOR_ID_PAS_HW_STATUS)) {
		xradio_dbg(XRADIO_DBG_ALWY, "but cfm msg status error.\n");
		goto err;
	}

	fw_dbg_pas_parse_hw_reg_ntd(p_hw_reg->ntd_control);
	fw_dbg_pas_parse_hw_reg_txc(p_hw_reg->txc_status);
	fw_dbg_pas_parse_hw_reg_rxc(p_hw_reg);
	fw_dbg_pas_parse_hw_reg_ebm(p_hw_reg);

err:
	if (p_msg)
		kfree(p_msg);

	return count;
}

static int fw_dbg_pas_parse_hw_reg_ntd(u32 ntd_control)
{
	u32 ntd_state = (ntd_control & FWD_PAS_REG_NTD_CONTROL_STATE_MASK)
				>> FWD_PAS_REG_NTD_CONTROL_STATE_SHIFT;

	u32 low_rx_rsp_req = (ntd_control
				& FWD_PAS_REG_NTD_CONTROL_LOW_RX_RESP_REQ_MASK)
				>> FWD_PAS_REG_NTD_CONTROL_LOW_RX_RESP_REQ_SHIFT;

	u32 high_rx_rsp_req = (ntd_control
				& FWD_PAS_REG_NTD_CONTROL_HIGH_RX_RESP_REQ_MASK)
				>> FWD_PAS_REG_NTD_CONTROL_HIGH_RX_RESP_REQ_SHIFT;

	u32 rsp_timeout = ntd_control & FWD_PAS_REG_NTD_CONTROL_RESP_TIMEOUT_REQ;
	u32 tsf_hw_event0_req = ntd_control & FWD_PAS_REG_NTD_CONTROL_TSF_HW_EVENT0_REQ;
	u32 tsf_hw_event1_req = ntd_control & FWD_PAS_REG_NTD_CONTROL_TSF_HW_EVENT1_REQ;

	xradio_dbg(XRADIO_DBG_ALWY,
		"ntd control:0x%08x-----------\n", ntd_control);

	if (ntd_state == FWD_PAS_REG_NTD_CONTROL_STATE_IDLE)
		xradio_dbg(XRADIO_DBG_ALWY, "ntd state:idle\n");
	else
		xradio_dbg(XRADIO_DBG_ALWY, "ntd state:%d\n", ntd_state);

	if (low_rx_rsp_req == FWD_PAS_REG_NTD_CONTROL_RX_RESP_REQ_NO_PENDING)
		xradio_dbg(XRADIO_DBG_ALWY, "Rsp Rxed Request low : no pending\n");
	else
		xradio_dbg(XRADIO_DBG_ALWY,
			"Rsp Rxed Request low pending or active  :%d\n", low_rx_rsp_req);

	if (high_rx_rsp_req == FWD_PAS_REG_NTD_CONTROL_RX_RESP_REQ_NO_PENDING)
		xradio_dbg(XRADIO_DBG_ALWY, "Rsp Rxed Request high: no pending\n");
	else
		xradio_dbg(XRADIO_DBG_ALWY,
			"Rsp Rxed Request high pending or active :%d\n", high_rx_rsp_req);

	if (rsp_timeout)
		xradio_dbg(XRADIO_DBG_ALWY, "Rsp timout request pending or active.\n");

	if (tsf_hw_event0_req)
		xradio_dbg(XRADIO_DBG_ALWY, "tsf hw event0 request pending or active.\n");

	if (tsf_hw_event1_req)
		xradio_dbg(XRADIO_DBG_ALWY, "tsf hw event0 request pending or active.\n");

	xradio_dbg(XRADIO_DBG_ALWY, "\n");

	return 0;
}

static int fw_dbg_pas_parse_hw_reg_txc(u32 txc_status)
{
	u32 txc_state = txc_status & FWD_PAS_REG_TXC_STATUS_STATE;

	u32 txc_ifs_complete_flag =
			txc_status & FWD_PAS_REG_TXC_STATUS_IFS_COMPLETE_FLAG;

	u32 txc_backoff_complete_flag =
			txc_status & FWD_PAS_REG_TXC_STATUS_BACKOFF_COMPLETE_FLAG;

	u32 txc_ptcs_pointer =
			(txc_status & FWD_PAS_REG_TXC_STATUS_PTCS_POINTER_INDEX_MASK)
			>> FWD_PAS_REG_TXC_STATUS_PTCS_POINTER_INDEX_SHIFT;

	u32 txc_last_rx_crc_error =
			txc_status & FWD_PAS_REG_TXC_STATUS_LAST_RX_CRC_ERROR;

	u32 txc_mac_cca = txc_status & FWD_PAS_REG_TXC_STATUS_MAC_CCA;

	xradio_dbg(XRADIO_DBG_ALWY,
		"txc status:0x%08x-----------\n", txc_status);

	if (txc_state == FWD_PAS_REG_TXC_STATUS_STATE_IDLE)
		xradio_dbg(XRADIO_DBG_ALWY, "txc state:idle\n");
	else
		xradio_dbg(XRADIO_DBG_ALWY, "txc state:%d\n", txc_state);

	xradio_dbg(XRADIO_DBG_ALWY, "txc ptcs pointer index:%d\n", txc_ptcs_pointer);

	if (txc_ifs_complete_flag)
		xradio_dbg(XRADIO_DBG_ALWY, "txc ifs complete.\n");

	if (txc_backoff_complete_flag)
		xradio_dbg(XRADIO_DBG_ALWY, "txc backoff complete.\n");

	if (txc_last_rx_crc_error)
		xradio_dbg(XRADIO_DBG_ALWY, "txc last rx crc error.\n");

	if (txc_mac_cca)
		xradio_dbg(XRADIO_DBG_ALWY, "txc mac clear channel assessment.\n");

	xradio_dbg(XRADIO_DBG_ALWY, "\n");

	return 0;
}

static int fw_dbg_pas_parse_hw_reg_rxc(struct fwd_pas_hw_reg *p_reg)
{
	/* rx state */
	xradio_dbg(XRADIO_DBG_ALWY, "rxc rx state:0x%02x(%d)-----------\n",
		p_reg->rxc_rx_state, p_reg->rxc_rx_state);

	/* rx control */
	xradio_dbg(XRADIO_DBG_ALWY,
		"rxc rx control:0x%08x-----------\n", p_reg->rxc_rx_control);

	if (p_reg->rxc_rx_control & FWD_PAS_REG_RXC_RX_CONTROL_RX_ENABLE)
		xradio_dbg(XRADIO_DBG_ALWY, "setting: rxc enable.\n");
	else
		xradio_dbg(XRADIO_DBG_ALWY, "setting: rxc disable.\n");

	if (p_reg->rxc_rx_control & FWD_PAS_REG_RXC_RX_CONTROL_APPEND_ERROR)
		xradio_dbg(XRADIO_DBG_ALWY,
			"setting: rx frame append error message.\n");

	if (p_reg->rxc_rx_control & FWD_PAS_REG_RXC_RX_CONTROL_APPEND_BAP_STATUS)
		xradio_dbg(XRADIO_DBG_ALWY, "setting: rx frame append bap status.\n");

	if (p_reg->rxc_rx_control & FWD_PAS_REG_RXC_RX_CONTROL_APPEND_STATUS)
		xradio_dbg(XRADIO_DBG_ALWY, "setting: rx frame append phy status.\n");

	if (p_reg->rxc_rx_control & FWD_PAS_REG_RXC_RX_CONTROL_APPEND_TSF)
		xradio_dbg(XRADIO_DBG_ALWY, "setting: rx frame append tsf message.\n");

	if (p_reg->rxc_rx_control & FWD_PAS_REG_RXC_RX_CONTROL_STORE_CSI_DATA)
		xradio_dbg(XRADIO_DBG_ALWY, "setting: rx frame append csi data.\n");

	if (p_reg->rxc_rx_control & FWD_PAS_REG_RXC_RX_CONTROL_BT_DIS_RX)
		xradio_dbg(XRADIO_DBG_ALWY, "setting: active bt will disable rx phy.\n");

	if (p_reg->rxc_rx_control & FWD_PAS_REG_RXC_RX_CONTROL_KEEP_BAD_FRAMES)
		xradio_dbg(XRADIO_DBG_ALWY, "setting: keep error frame.\n");

	if (p_reg->rxc_rx_control & FWD_PAS_REG_RXC_RX_CONTROL_AUTO_DISCARD)
		xradio_dbg(XRADIO_DBG_ALWY, "setting: auto discard.\n");

	if (p_reg->rxc_rx_control & FWD_PAS_REG_RXC_RX_CONTROL_AUTO_KEEP)
		xradio_dbg(XRADIO_DBG_ALWY, "setting: auto keep.\n");

	if (p_reg->rxc_rx_control & FWD_PAS_REG_RXC_RX_CONTROL_DISABLE_DEAGGR)
		xradio_dbg(XRADIO_DBG_ALWY, "setting: disable de-aggregation.\n");

	if (p_reg->rxc_rx_control & FWD_PAS_REG_RXC_RX_CONTROL_BAP_DIS_PHY)
		xradio_dbg(XRADIO_DBG_ALWY, "status: bap disable phy.\n");

	if (p_reg->rxc_rx_control & FWD_PAS_REG_RXC_RX_CONTROL_PHY_CCA_VALID) {
		if (p_reg->rxc_rx_control & FWD_PAS_REG_RXC_RX_CONTROL_PHY_CCA)
			xradio_dbg(XRADIO_DBG_ALWY, "status: phy set cca.\n");
		else
			xradio_dbg(XRADIO_DBG_ALWY, "status: no cca.\n");
	}

	if (p_reg->rxc_rx_control & FWD_PAS_REG_RXC_RX_CONTROL_RXBUF_NEAR_FULL)
		xradio_dbg(XRADIO_DBG_ALWY, "status: rx buf near full.\n");

	if (p_reg->rxc_rx_control & FWD_PAS_REG_RXC_RX_CONTROL_RXBUF_OVERFLOW)
		xradio_dbg(XRADIO_DBG_ALWY, "status: rx buf overflow.\n");

	if (p_reg->rxc_rx_control & FWD_PAS_REG_RXC_RX_CONTROL_RXBUF_OVERFLOW)
		xradio_dbg(XRADIO_DBG_ALWY, "status: rx buf overflow.\n");

	if (p_reg->rxc_rx_control & FWD_PAS_REG_RXC_RX_CONTROL_RXFIFO_OVERFLOW)
		xradio_dbg(XRADIO_DBG_ALWY, "status: rx fifo overflow.\n");

	if (p_reg->rxc_rx_control & FWD_PAS_REG_RXC_RX_CONTROL_RX_CTRL_RXBUSY)
		xradio_dbg(XRADIO_DBG_ALWY, "status: receiver busy.\n");

	/* rx buffer */
	xradio_dbg(XRADIO_DBG_ALWY, "rxc rx buffer:%d kbytes-----------\n",
			p_reg->rxc_buffer_size * 4);
	xradio_dbg(XRADIO_DBG_ALWY, "in  pointer: 0x%05x(%d)\n",
		p_reg->rxc_rx_buf_in_pointer, p_reg->rxc_rx_buf_in_pointer);
	xradio_dbg(XRADIO_DBG_ALWY, "out pointer: 0x%05x(%d)\n",
		p_reg->rxc_rx_buf_out_pointer, p_reg->rxc_rx_buf_out_pointer);

	/* error code */
	xradio_dbg(XRADIO_DBG_ALWY, "rxc error code-----------\n");

	xradio_dbg(XRADIO_DBG_ALWY, "setting: 0x%01x\n",
			(p_reg->rxc_error_code & FWD_PAS_REG_RXC_ERROR_CODE_SETTING_MASK)
			>> FWD_PAS_REG_RXC_ERROR_CODE_SETTING_SHIFT);

	xradio_dbg(XRADIO_DBG_ALWY, "code: 0x%06x\n",
			p_reg->rxc_error_code & FWD_PAS_REG_RXC_ERROR_CODE_ERROR_CODE);

	/* error counter */
	xradio_dbg(XRADIO_DBG_ALWY, "rxc error counter-----------\n");

	/* counter 0*/
	if (FWD_PAS_REG_RXC_ERROR_COUNT(p_reg->rxc_rx_err_cnt0, 0))
		xradio_dbg(XRADIO_DBG_ALWY,
			"%3d : Legacy signal field parity check error\n",
			FWD_PAS_REG_RXC_ERROR_COUNT(p_reg->rxc_rx_err_cnt0, 0));

	if (FWD_PAS_REG_RXC_ERROR_COUNT(p_reg->rxc_rx_err_cnt0, 8))
		xradio_dbg(XRADIO_DBG_ALWY,
			"%3d : HT signal field CRC failure\n",
			FWD_PAS_REG_RXC_ERROR_COUNT(p_reg->rxc_rx_err_cnt0, 8));

	if (FWD_PAS_REG_RXC_ERROR_COUNT(p_reg->rxc_rx_err_cnt0, 16))
		xradio_dbg(XRADIO_DBG_ALWY,
			"%3d : Length field greater than maximum length\n",
			FWD_PAS_REG_RXC_ERROR_COUNT(p_reg->rxc_rx_err_cnt0, 16));

	if (FWD_PAS_REG_RXC_ERROR_COUNT(p_reg->rxc_rx_err_cnt0, 24))
		xradio_dbg(XRADIO_DBG_ALWY,
			"%3d : Decode signal valid but MCS or rate reserved\n",
			FWD_PAS_REG_RXC_ERROR_COUNT(p_reg->rxc_rx_err_cnt0, 24));

	/* counter 1*/
	if (FWD_PAS_REG_RXC_ERROR_COUNT(p_reg->rxc_rx_err_cnt1, 0))
		xradio_dbg(XRADIO_DBG_ALWY,
			"%3d : DSSS signal field CRC failure\n",
			FWD_PAS_REG_RXC_ERROR_COUNT(p_reg->rxc_rx_err_cnt1, 0));

	if (FWD_PAS_REG_RXC_ERROR_COUNT(p_reg->rxc_rx_err_cnt1, 8))
		xradio_dbg(XRADIO_DBG_ALWY,
			"%3d : DSSS field valid but RATE is reserved\n",
			FWD_PAS_REG_RXC_ERROR_COUNT(p_reg->rxc_rx_err_cnt1, 8));

	if (FWD_PAS_REG_RXC_ERROR_COUNT(p_reg->rxc_rx_err_cnt1, 16))
		xradio_dbg(XRADIO_DBG_ALWY,
			"%3d : Missed preamble\n",
			FWD_PAS_REG_RXC_ERROR_COUNT(p_reg->rxc_rx_err_cnt1, 16));

	if (FWD_PAS_REG_RXC_ERROR_COUNT(p_reg->rxc_rx_err_cnt1, 24))
		xradio_dbg(XRADIO_DBG_ALWY,
			"%3d : PHY Abort request - Mode Indicator Failure\n",
			FWD_PAS_REG_RXC_ERROR_COUNT(p_reg->rxc_rx_err_cnt1, 24));

	/* counter 2*/
	if (FWD_PAS_REG_RXC_ERROR_COUNT(p_reg->rxc_rx_err_cnt2, 0))
		xradio_dbg(XRADIO_DBG_ALWY,
			"%3d : PHY Abort request - CCA FSM Timeout\n",
			FWD_PAS_REG_RXC_ERROR_COUNT(p_reg->rxc_rx_err_cnt2, 0));

	if (FWD_PAS_REG_RXC_ERROR_COUNT(p_reg->rxc_rx_err_cnt2, 8))
		xradio_dbg(XRADIO_DBG_ALWY,
			"%3d : Unknown format violations\n",
			FWD_PAS_REG_RXC_ERROR_COUNT(p_reg->rxc_rx_err_cnt2, 8));

	/*delimiter error counter */
	if (FWD_PAS_REG_RXC_ERROR_COUNT(p_reg->rxc_rx_delimiter_err_cnt, 0))
		xradio_dbg(XRADIO_DBG_ALWY,
			"%3d : delimiter error\n",
			FWD_PAS_REG_RXC_ERROR_COUNT(p_reg->rxc_rx_delimiter_err_cnt, 0));

	xradio_dbg(XRADIO_DBG_ALWY, "\n");

	return 0;
}

static int fw_dbg_pas_parse_hw_reg_ebm(struct fwd_pas_hw_reg *p_reg)
{
	u8 ac_idx;

	u32 stat_timing_state;
	u32 stat_tx_pending;
	u32 stat_tx_pending_next;
	u32 stat_ttcs_loaded;
	u32 stat_aifs_done;
	u32 stat_backoff_done;
	u32 stat_winning_ac;
	u32 stat_in_txop;
	u32 stat_control_state;
	u32 stat_ack_received;
	u32 stat_last_frame;

	u32 stat2_control_state;
	u32 stat2_ttcs_load_state;
	u32 stat2_phy_slot_start;
	u32 stat2_clear_backoff;
	u32 stat2_aifs_complete;
	u32 stat2_backoff_counter_enable;
	u32 stat2_backoff_complete;
	u32 stat2_ebm_tx_pending;
	u32 stat2_in_txop;

	u32 stat3_slot_end;
	u32 stat3_ack_received;
	u32 stat3_sifi_complete;
	u32 stat3_rxc_rx_end;
	u32 stat3_ack_required;
	u32 stat3_last_crc_error;
	u32 stat3_in_txop;
	u32 stat3_timing_state_vector;
	u32 stat3_rsp_timeout_event;
	u32 stat3_sifs_complete;
	u32 stat3_ifs_timeout_enabled;
	u32 stat3_ack_received2;
	u32 stat3_full_ebm_state;

	stat_timing_state = FWD_GET_REG_SECTION(p_reg->tim_ebm_stat, 0xF, 0);
	stat_tx_pending = FWD_GET_REG_SECTION(p_reg->tim_ebm_stat, 0xF, 4);
	stat_tx_pending_next = FWD_GET_REG_SECTION(p_reg->tim_ebm_stat, 0xF, 8);
	stat_ttcs_loaded = FWD_GET_REG_SECTION(p_reg->tim_ebm_stat, 0xF, 12);
	stat_aifs_done = FWD_GET_REG_SECTION(p_reg->tim_ebm_stat, 0xF, 16);
	stat_backoff_done = FWD_GET_REG_SECTION(p_reg->tim_ebm_stat, 0xF, 20);
	stat_winning_ac = FWD_GET_REG_SECTION(p_reg->tim_ebm_stat, 0xF, 24);
	stat_in_txop = FWD_GET_REG_SECTION(p_reg->tim_ebm_stat, 0x1, 26);
	stat_control_state = FWD_GET_REG_SECTION(p_reg->tim_ebm_stat, 0x3, 27);
	stat_ack_received = FWD_GET_REG_SECTION(p_reg->tim_ebm_stat, 0x1, 29);
	stat_last_frame = FWD_GET_REG_SECTION(p_reg->tim_ebm_stat, 0x1, 30);

	stat2_control_state = FWD_GET_REG_SECTION(p_reg->tim_ebm_stat_2, 0x3, 0);
	stat2_ttcs_load_state = FWD_GET_REG_SECTION(p_reg->tim_ebm_stat_2, 0x3, 2);
	stat2_phy_slot_start = FWD_GET_REG_SECTION(p_reg->tim_ebm_stat_2, 0xf, 4);
	stat2_clear_backoff = FWD_GET_REG_SECTION(p_reg->tim_ebm_stat_2, 0xf, 8);
	stat2_aifs_complete = FWD_GET_REG_SECTION(p_reg->tim_ebm_stat_2, 0xf, 12);
	stat2_backoff_counter_enable = FWD_GET_REG_SECTION(p_reg->tim_ebm_stat_2, 0xf, 16);
	stat2_backoff_complete = FWD_GET_REG_SECTION(p_reg->tim_ebm_stat_2, 0xf, 20);
	stat2_ebm_tx_pending = FWD_GET_REG_SECTION(p_reg->tim_ebm_stat_2, 0xf, 24);
	stat2_in_txop = FWD_GET_REG_SECTION(p_reg->tim_ebm_stat_2, 0x1, 28);

	stat3_slot_end = FWD_GET_REG_SECTION(p_reg->tim_ebm_stat_3, 0x1, 0);
	stat3_ack_received = FWD_GET_REG_SECTION(p_reg->tim_ebm_stat_3, 0x1, 1);
	stat3_sifi_complete = FWD_GET_REG_SECTION(p_reg->tim_ebm_stat_3, 0x1, 2);
	stat3_rxc_rx_end = FWD_GET_REG_SECTION(p_reg->tim_ebm_stat_3, 0x1, 3);
	stat3_ack_required = FWD_GET_REG_SECTION(p_reg->tim_ebm_stat_3, 0xF, 4);
	stat3_last_crc_error = FWD_GET_REG_SECTION(p_reg->tim_ebm_stat_3, 0x1, 8);
	stat3_in_txop = FWD_GET_REG_SECTION(p_reg->tim_ebm_stat_3, 0x1, 9);
	stat3_timing_state_vector = FWD_GET_REG_SECTION(p_reg->tim_ebm_stat_3, 0x1, 10);
	stat3_rsp_timeout_event = FWD_GET_REG_SECTION(p_reg->tim_ebm_stat_3, 0x1, 11);
	stat3_sifs_complete = FWD_GET_REG_SECTION(p_reg->tim_ebm_stat_3, 0x1, 12);
	stat3_ifs_timeout_enabled = FWD_GET_REG_SECTION(p_reg->tim_ebm_stat_3, 0x1, 13);
	stat3_ack_received2 = FWD_GET_REG_SECTION(p_reg->tim_ebm_stat_3, 0x1, 14);
	stat3_full_ebm_state = FWD_GET_REG_SECTION(p_reg->tim_ebm_stat_3, 0x3F, 15);

	xradio_dbg(XRADIO_DBG_ALWY, "ebm stat:0x%08x-----------\n", p_reg->tim_ebm_stat);

	xradio_dbg(XRADIO_DBG_ALWY, "timing state(%d):\n", stat_timing_state);
	switch (stat_timing_state) {
	case 0:
		xradio_dbg(XRADIO_DBG_ALWY, "0: idle\n");
		break;
	case 1:
		xradio_dbg(XRADIO_DBG_ALWY, "1: ifs_active\n");
		break;
	case 2:
		xradio_dbg(XRADIO_DBG_ALWY, "2: backoff_active\n");
		break;
	case 3:
		xradio_dbg(XRADIO_DBG_ALWY, "3: medium_busy\n");
		break;
	case 5:
		xradio_dbg(XRADIO_DBG_ALWY, "5: wait_for_air_clear\n");
		break;
	case 7:
		xradio_dbg(XRADIO_DBG_ALWY, "7: rest\n");
		break;
	default:
		xradio_dbg(XRADIO_DBG_ALWY, "other\n");
		break;
	}

	xradio_dbg(XRADIO_DBG_ALWY, "control state(%d):\n", stat_control_state);
	switch (stat_control_state) {
	case 0:
		xradio_dbg(XRADIO_DBG_ALWY, "0: idle\n");
		break;
	case 1:
		xradio_dbg(XRADIO_DBG_ALWY, "1: txop_tx\n");
		break;
	case 2:
		xradio_dbg(XRADIO_DBG_ALWY, "2: wait_for_response\n");
		break;
	case 3:
		xradio_dbg(XRADIO_DBG_ALWY, "3: wait_for_ifs\n");
		break;
	default:
		xradio_dbg(XRADIO_DBG_ALWY, "other\n");
		break;
	}

	if (stat_tx_pending) {
		u8 idx = 0;
		for (idx = 0; idx < 4; idx++) {
			if (stat_tx_pending & (1 << idx)) {
				xradio_dbg(XRADIO_DBG_ALWY, "AC(%d) is tx pending.\n", idx);
			}
		}
	}

	if (stat_tx_pending_next) {
		u8 idx = 0;
		for (idx = 0; idx < 4; idx++) {
			if (stat_tx_pending_next & (1 << idx)) {
				xradio_dbg(XRADIO_DBG_ALWY,
					"AC(%d) is multi mpdus pending.\n", idx);
			}
		}
	}

	if (stat_ttcs_loaded) {
		u8 idx = 0;
		for (idx = 0; idx < 4; idx++) {
			if (stat_ttcs_loaded & (1 << idx)) {
				xradio_dbg(XRADIO_DBG_ALWY,
					"AC(%d) is ttcs loaded.\n", idx);
			}
		}
	}

	if (stat_aifs_done) {
		u8 idx = 0;
		for (idx = 0; idx < 4; idx++) {
			if (stat_aifs_done & (1 << idx)) {
				xradio_dbg(XRADIO_DBG_ALWY,
					"AC(%d) is aifs done.\n", idx);
			}
		}
	}

	if (stat_backoff_done) {
		u8 idx = 0;
		for (idx = 0; idx < 4; idx++) {
			if (stat_backoff_done & (1 << idx)) {
				xradio_dbg(XRADIO_DBG_ALWY,
					"AC(%d) is backoff done.\n", idx);
			}
		}
	}

	if (stat_winning_ac) {
		u8 idx = 0;
		for (idx = 0; idx < 4; idx++) {
			if (stat_backoff_done & (1 << idx)) {
				xradio_dbg(XRADIO_DBG_ALWY,
					"AC(%d) is winning.\n", idx);
			}
		}
	}

	if (stat_in_txop)
		xradio_dbg(XRADIO_DBG_ALWY, "in txop\n");

	if (stat_ack_received)
		xradio_dbg(XRADIO_DBG_ALWY, "ack received\n");

	if (stat_last_frame)
		xradio_dbg(XRADIO_DBG_ALWY, "last frame\n");


	xradio_dbg(XRADIO_DBG_ALWY, "ebm stat2:0x%08x-----------\n", p_reg->tim_ebm_stat_2);

	xradio_dbg(XRADIO_DBG_ALWY, "control state(%d):\n", stat2_control_state);
	switch (stat2_control_state) {
	case 0:
		xradio_dbg(XRADIO_DBG_ALWY, "0: idle\n");
		break;
	case 1:
		xradio_dbg(XRADIO_DBG_ALWY, "1: txop_tx\n");
		break;
	case 2:
		xradio_dbg(XRADIO_DBG_ALWY, "2: wait_for_response\n");
		break;
	case 3:
		xradio_dbg(XRADIO_DBG_ALWY, "3: wait_for_ifs\n");
		break;
	default:
		xradio_dbg(XRADIO_DBG_ALWY, "other\n");
		break;
	}

	xradio_dbg(XRADIO_DBG_ALWY,
		"ttcs load state(%d):\n", stat2_ttcs_load_state);

	switch (stat2_ttcs_load_state) {
	case 0:
		xradio_dbg(XRADIO_DBG_ALWY, "0: idle\n");
		break;
	case 1:
		xradio_dbg(XRADIO_DBG_ALWY, "1: load ttcs ifsb\n");
		break;
	case 3:
		xradio_dbg(XRADIO_DBG_ALWY, "3: in reset\n");
		break;
	default:
		xradio_dbg(XRADIO_DBG_ALWY, "other\n");
		break;
	}

	for (ac_idx = 0; ac_idx < 4; ac_idx++) {

		xradio_dbg(XRADIO_DBG_ALWY, "ac = %d\n", ac_idx);

		if (stat2_phy_slot_start & (1 << ac_idx))
			xradio_dbg(XRADIO_DBG_ALWY,
					"AC(%d) is not start slot.\n", ac_idx);

		if (stat2_clear_backoff & (1 << ac_idx))
			xradio_dbg(XRADIO_DBG_ALWY,
					"AC(%d) is clear backoff.\n", ac_idx);

		if (stat2_aifs_complete & (1 << ac_idx))
			xradio_dbg(XRADIO_DBG_ALWY,
					"AC(%d) is completed aifsn.\n", ac_idx);

		if (stat2_backoff_counter_enable & (1 << ac_idx))
			xradio_dbg(XRADIO_DBG_ALWY,
					"AC(%d) is enable backoff.\n", ac_idx);

		if (stat2_backoff_complete & (1 << ac_idx))
			xradio_dbg(XRADIO_DBG_ALWY,
					"AC(%d) is complete backoff.\n", ac_idx);

		if (stat2_ebm_tx_pending & (1 << ac_idx))
			xradio_dbg(XRADIO_DBG_ALWY,
					"AC(%d) is tx penading in ebm.\n", ac_idx);
	}

	if (stat2_in_txop)
		xradio_dbg(XRADIO_DBG_ALWY, "in txop\n");

	xradio_dbg(XRADIO_DBG_ALWY, "ebm stat3:0x%08x-----------\n", p_reg->tim_ebm_stat_3);

	xradio_dbg(XRADIO_DBG_ALWY,
		"full ebm state(%d):\n", stat3_full_ebm_state);

	switch (stat3_full_ebm_state) {
	case 0:
		xradio_dbg(XRADIO_DBG_ALWY, "0: idle\n");
		break;
	case 1:
		xradio_dbg(XRADIO_DBG_ALWY, "1: wait for ifs\n");
		break;
	case 2:
		xradio_dbg(XRADIO_DBG_ALWY, "2: wait for aifsn\n");
		break;
	case 3:
		xradio_dbg(XRADIO_DBG_ALWY, "3: backoff active\n");
		break;
	case 4:
		xradio_dbg(XRADIO_DBG_ALWY, "4: TXOP Tx\n");
		break;
	case 5:
		xradio_dbg(XRADIO_DBG_ALWY, "5: DUR Calc\n");
		break;
	case 7:
		xradio_dbg(XRADIO_DBG_ALWY, "7: Wait for Tx\n");
		break;
	case 8:
		xradio_dbg(XRADIO_DBG_ALWY, "8: Wait for Response\n");
		break;
	case 9:
		xradio_dbg(XRADIO_DBG_ALWY, "9: TXOP Wait\n");
		break;
	case 10:
		xradio_dbg(XRADIO_DBG_ALWY, "10: TXOP Load Count\n");
		break;
	case 11:
		xradio_dbg(XRADIO_DBG_ALWY, "11: Medium Busy\n");
		break;
	case 12:
		xradio_dbg(XRADIO_DBG_ALWY, "12: Wait for air to clear\n");
		break;
	case 63:
		xradio_dbg(XRADIO_DBG_ALWY, "63: EBM In Reset\n");
		break;
	default:
		xradio_dbg(XRADIO_DBG_ALWY, "other\n");
		break;
	}

	if (stat3_ack_required) {
		u8 idx = 0;
		for (idx = 0; idx < 4; idx++) {
			if (stat3_ack_required & (1 << idx)) {
				xradio_dbg(XRADIO_DBG_ALWY,
					"AC(%d) is required ack.\n", idx);
			}
		}
	}

	if (stat3_slot_end)
		xradio_dbg(XRADIO_DBG_ALWY, "slot end\n");

	if (stat3_ack_received)
		xradio_dbg(XRADIO_DBG_ALWY, "ack is received for this TTCS\n");

	if (stat3_sifi_complete)
		xradio_dbg(XRADIO_DBG_ALWY, "SIFS period has been completed\n");

	if (stat3_rxc_rx_end)
		xradio_dbg(XRADIO_DBG_ALWY, "RXC has completed an RX\n");

	if (stat3_last_crc_error)
		xradio_dbg(XRADIO_DBG_ALWY, "the last RX had a CRC error\n");

	if (stat3_in_txop)
		xradio_dbg(XRADIO_DBG_ALWY, "EBM is inside a TXOP\n");

	if (stat3_timing_state_vector)
		xradio_dbg(XRADIO_DBG_ALWY, "bit3 of the timing state vecotr\n");

	if (stat3_rsp_timeout_event)
		xradio_dbg(XRADIO_DBG_ALWY, "response tiemout has occurred\n");

	if (stat3_sifs_complete)
		xradio_dbg(XRADIO_DBG_ALWY, "SIFS period has been completed\n");

	if (stat3_ifs_timeout_enabled)
		xradio_dbg(XRADIO_DBG_ALWY, "IFS period has being counted\n");

	if (stat3_ack_received2)
		xradio_dbg(XRADIO_DBG_ALWY, "ACK for the active TTCS has been Rxed\n");

	xradio_dbg(XRADIO_DBG_ALWY, "\n");

	return 0;
}

static ssize_t fw_dbg_pas_show_hw_stat(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{
	struct xradio_common *hw_priv = file->private_data;
	char buf[1024];
	size_t size = 0;

	struct fwd_msg *p_msg;
	struct fwd_pas_hw_stat *p_hw_stat;

	ssize_t msg_buf_size;
	ssize_t msg_req_size;
	ssize_t msg_cfm_size;

	sprintf(buf, "\nshow fw dbg pas hw stat\n\n");

	msg_req_size = sizeof(struct fwd_msg);
	msg_cfm_size = sizeof(struct fwd_msg)
			+ sizeof(struct fwd_pas_hw_stat);

	msg_buf_size = msg_cfm_size;

	p_msg = xr_kzalloc(msg_buf_size, false);
	if (p_msg == NULL) {
		sprintf(buf, "%s" "but not enough memory to show.\n", buf);
		goto err;
	}

	p_hw_stat = (struct fwd_pas_hw_stat *)(p_msg + 1);

	if (xradio_fw_dbg_request(hw_priv, p_msg, msg_req_size, msg_cfm_size,
		FWD_CMD_MAJOR_ID_PAS | FWD_CMD_MINOR_ID_PAS_HW_STAT)) {
		sprintf(buf, "%s" "but cfm msg status error.\n", buf);
		goto err;
	}

	fw_dbg_pas_parse_hw_stat(buf, p_hw_stat);

err:
	if (p_msg)
		kfree(p_msg);

	sprintf(buf, "%s\n", buf);
	size = strlen(buf);
	return simple_read_from_buffer(user_buf, count, ppos, buf, size);
}

static int fw_dbg_pas_parse_hw_stat(char *buf, struct fwd_pas_hw_stat *p_stat)
{
	sprintf(buf, "%s" "pas statistics counter-----------\n", buf);

	sprintf(buf, "%s" "fiq count :%4d, rx event:%4d\n", buf,
		p_stat->fiq_count, p_stat->rx_event_count);

	sprintf(buf, "%s" "tx [write port:%4d][success:%3d][failure:%2d]\n", buf,
		p_stat->write_port_count,
		p_stat->tx_success_count,
		p_stat->tx_failure_count);

	sprintf(buf, "%s" "rx [stored :%4d][error:%3d][beacon :%2d][multicast: %4d]\n", buf,
			p_stat->rx_frame_stored_count,
			p_stat->rx_frame_error_count,
			p_stat->rx_beacon_count,
			p_stat->rx_multicast_count);

	return 0;
}

static ssize_t fw_dbg_pas_config_force_mode(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct xradio_common *hw_priv = file->private_data;
	char  buf[256] = {0};
	u16   buf_size = (count > 255 ? 255 : count);
	char *startptr = &buf[0];
	char *endptr   = NULL;

	struct fwd_msg *p_msg;
	struct fwd_pas_force_mode_cfg *p_mode;

	ssize_t msg_buf_size;
	ssize_t msg_req_size;
	ssize_t msg_cfm_size;

	if (!count)
		return -EINVAL;
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	xradio_dbg(XRADIO_DBG_ALWY, "config fw dbg pas force mode. \n\n");

	msg_req_size = sizeof(struct fwd_msg)
			+ sizeof(struct fwd_pas_force_mode_cfg);
	msg_cfm_size = sizeof(struct fwd_msg);
	msg_buf_size = msg_req_size;

	p_msg = xr_kzalloc(msg_buf_size, false);
	if (p_msg == NULL) {
		xradio_dbg(XRADIO_DBG_ALWY, "but not enough memory to show.\n");
		goto err;
	}

	p_mode = (struct fwd_pas_force_mode_cfg *)(p_msg + 1);

	p_mode->force_mode_select = simple_strtoul(startptr, &endptr, 16);
	startptr = endptr + 1;

	if (p_mode->force_mode_select == FWD_PAS_FORCE_MODE_SELECT_TX_SCHEME) {
		p_mode->tx_scheme_type = simple_strtoul(startptr, &endptr, 0);
		p_mode->protection_type = 0;
		startptr = endptr + 1;
	} else if (p_mode->force_mode_select == FWD_PAS_FORCE_MODE_SELECT_PROTECTION) {
		p_mode->tx_scheme_type = 0;
		p_mode->protection_type = simple_strtoul(startptr, &endptr, 0);
		startptr = endptr + 1;
	} else {
		p_mode->force_mode_select = 0;
		p_mode->tx_scheme_type = 0;
		p_mode->protection_type = 0;
	}

	if (xradio_fw_dbg_request(hw_priv, p_msg, msg_req_size, msg_cfm_size,
		FWD_CMD_MAJOR_ID_PAS | FWD_CMD_MINOR_ID_PAS_PTCS_DUMP)) {
		xradio_dbg(XRADIO_DBG_ALWY, "but cfm msg status error.\n");
		goto err;
	}

	xradio_dbg(XRADIO_DBG_ALWY, "force mode select: %x, "
		"tx schemd type: %d, protection type : %d\n\n",
		p_mode->force_mode_select,
		p_mode->tx_scheme_type,
		p_mode->protection_type);

err:
	if (p_msg)
		kfree(p_msg);

	return count;
}


static ssize_t fw_dbg_pas_show_dur_entry(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct xradio_common *hw_priv = file->private_data;
	char  buf[256] = {0};
	u16   buf_size = (count > 255 ? 255 : count);
	char *startptr = &buf[0];
	char *endptr   = NULL;

	struct fwd_msg *p_msg;
	struct fwd_pas_dur_entry_cfg *p_cfg;
	struct fwd_pas_dur_entry *p_dur;

	ssize_t msg_buf_size;
	ssize_t msg_req_size;
	ssize_t msg_cfm_size;

	if (!count)
		return -EINVAL;
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	xradio_dbg(XRADIO_DBG_ALWY, "show fw dbg pas dur entry. \n\n");

	msg_req_size = sizeof(struct fwd_msg)
			+ sizeof(struct fwd_pas_dur_entry_cfg);

	msg_cfm_size = sizeof(struct fwd_msg)
			+ (sizeof(struct fwd_pas_dur_entry)) * 2;

	msg_buf_size = msg_cfm_size;

	p_msg = xr_kzalloc(msg_buf_size, false);
	if (p_msg == NULL) {
		xradio_dbg(XRADIO_DBG_ALWY, "but not enough memory to show.\n");
		goto err;
	}

	p_cfg = (struct fwd_pas_dur_entry_cfg *)(p_msg + 1);

	p_cfg->dur_idx = simple_strtoul(startptr, &endptr, 0);
	startptr = endptr + 1;

	xradio_dbg(XRADIO_DBG_ALWY, "idx: %d\n", p_cfg->dur_idx);

	if (xradio_fw_dbg_request(hw_priv, p_msg, msg_req_size, msg_cfm_size,
		FWD_CMD_MAJOR_ID_PAS | FWD_CMD_MINOR_ID_PAS_DUR_ENTRY)) {
		xradio_dbg(XRADIO_DBG_ALWY, "but cfm msg status error.\n");
		goto err;
	}

	p_dur = (struct fwd_pas_dur_entry *)(p_msg + 1);

	xradio_dbg(XRADIO_DBG_ALWY, "--20m entry\n");

	fw_dbg_pas_parse_dur_entry(p_dur);

	xradio_dbg(XRADIO_DBG_ALWY, "--40m entry\n");

	fw_dbg_pas_parse_dur_entry(&(p_dur[1]));

err:
	if (p_msg)
		kfree(p_msg);

	return count;
}


static int fw_dbg_pas_parse_dur_entry(struct fwd_pas_dur_entry *p_dur)
{
	u32 RespRate;
	u32 BasisBaDurAdj;
	u32 NormalAckCtsDurAdj;

	u32 RespMode;
	u32 RespPower;
	u32 TwoTidBaDurAdj;
	u32 CompressBaAdj;

	u32 RespBandwidth;
	u32 BasicBaLSig;
	u32 NormalAckCtsLSig;

	u32 TwoTidBaLSig;
	u32 CompressBaLSig;

	RespRate = ((p_dur->resp_rate_dur_adj & (0xff << 24)) >> 24);
	BasisBaDurAdj = ((p_dur->resp_rate_dur_adj & (0xfff << 12)) >> 12);
	NormalAckCtsDurAdj = ((p_dur->resp_rate_dur_adj & (0xfff << 0)) >> 0);

	RespMode = ((p_dur->resp_mode_dur_adj & (0xf << 28)) >> 28);
	RespPower = ((p_dur->resp_mode_dur_adj & (0xf << 24)) >> 24);
	TwoTidBaDurAdj = ((p_dur->resp_mode_dur_adj & (0xfff << 12)) >> 12);
	CompressBaAdj = ((p_dur->resp_mode_dur_adj & (0xfff << 0)) >> 0);

	RespBandwidth = ((p_dur->burst_length_0 & (0x1 << 24)) >> 24);
	BasicBaLSig = ((p_dur->burst_length_0 & (0xfff << 12)) >> 12);
	NormalAckCtsLSig = ((p_dur->burst_length_0 & (0xfff << 0)) >> 0);

	TwoTidBaLSig = ((p_dur->burst_length_1 & (0xfff << 12)) >> 12);
	CompressBaLSig = ((p_dur->burst_length_1 & (0xfff << 0)) >> 0);

	xradio_dbg(XRADIO_DBG_ALWY, "RespRateDurAdj[0x%08x]RespModeDurAdj[0x%08x]\n",
		p_dur->resp_rate_dur_adj, p_dur->resp_mode_dur_adj);
	xradio_dbg(XRADIO_DBG_ALWY, "BurstLength0  [0x%08x]BurstLength   [0x%08x]\n",
		p_dur->burst_length_0, p_dur->burst_length_1);

	xradio_dbg(XRADIO_DBG_ALWY,
		"RspMode[%d], RspBw[%d], RspRate[0x%x], RspPower[%d]\n",
		RespMode, RespBandwidth, RespRate, RespPower);

	xradio_dbg(XRADIO_DBG_ALWY,
		"Basic BaDurAdj[%5d], Basic BaLSig[%5d]\n",
		BasisBaDurAdj, BasicBaLSig);

	xradio_dbg(XRADIO_DBG_ALWY,
		"TwoTidBaDurAdj[%5d], TwoTidBaLSig[%5d]\n",
		TwoTidBaDurAdj, TwoTidBaLSig);

	xradio_dbg(XRADIO_DBG_ALWY,
		"NormalAckCtsDurAdj[%d], NormalAckCtsLSig[%d]\n",
		NormalAckCtsDurAdj, NormalAckCtsLSig);

	xradio_dbg(XRADIO_DBG_ALWY,
		"CompressBaAdj[%d], CompressBaLSig[%d]\n",
		CompressBaAdj, CompressBaLSig);

	return 0;

}

static ssize_t fw_dbg_pas_config_force_tx(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct xradio_common *hw_priv = file->private_data;
	char  buf[256] = {0};
	u16   buf_size = (count > 255 ? 255 : count);
	char *startptr = &buf[0];
	char *endptr   = NULL;

	struct fwd_msg *p_msg;
	struct fwd_pas_force_tx_cfg *p_tx;

	ssize_t msg_buf_size;
	ssize_t msg_req_size;
	ssize_t msg_cfm_size;

	if (!count)
		return -EINVAL;
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	xradio_dbg(XRADIO_DBG_ALWY, "config fw dbg pas force tx. \n\n");

	msg_req_size = sizeof(struct fwd_msg)
			+ sizeof(struct fwd_pas_force_tx_cfg);
	msg_cfm_size = sizeof(struct fwd_msg);
	msg_buf_size = msg_req_size;

	p_msg = xr_kzalloc(msg_buf_size, false);
	if (p_msg == NULL) {
		xradio_dbg(XRADIO_DBG_ALWY, "but not enough memory to show.\n");
		goto err;
	}

	p_tx = (struct fwd_pas_force_tx_cfg *)(p_msg + 1);

	p_tx->if_id = simple_strtoul(startptr, &endptr, 0);
	startptr = endptr + 1;

	p_tx->rate_entry = simple_strtoul(startptr, &endptr, 16);
	startptr = endptr + 1;

	if (xradio_fw_dbg_request(hw_priv, p_msg, msg_req_size, msg_cfm_size,
		FWD_CMD_MAJOR_ID_PAS | FWD_CMD_MINOR_ID_PAS_FORCE_TX)) {
		xradio_dbg(XRADIO_DBG_ALWY, "but cfm msg status error.\n");
		goto err;
	}

	xradio_dbg(XRADIO_DBG_ALWY, "force tx if_id: %d, "
		"force tx rate entry: %04x\n\n",
		p_tx->if_id, p_tx->rate_entry);

err:
	if (p_msg)
		kfree(p_msg);

	return count;
}

static int fw_dbg_phy_init(struct xradio_common *hw_priv,
				struct dentry *fw_node)
{
	fw_dbg.dbgfs_phy = debugfs_create_dir("phy", fw_node);
	if (!fw_dbg.dbgfs_phy) {
		xradio_dbg(XRADIO_DBG_ERROR, "phy is not created.\n");
		goto err;
	}

	return 0;

err:
	xradio_dbg(XRADIO_DBG_ERROR, "%s faild\n", __func__);
	if (fw_dbg.dbgfs_phy)
		debugfs_remove_recursive(fw_dbg.dbgfs_phy);
	fw_dbg.dbgfs_phy = NULL;
	return 1;
}

static void fw_dbg_phy_deinit(void)
{
	if (fw_dbg.dbgfs_phy)
		debugfs_remove_recursive(fw_dbg.dbgfs_phy);
	fw_dbg.dbgfs_phy = NULL;
}

static int fw_dbg_rf_init(struct xradio_common *hw_priv,
				struct dentry *fw_node)
{
	fw_dbg.dbgfs_rf = debugfs_create_dir("rf", fw_node);
	if (!fw_dbg.dbgfs_rf) {
		xradio_dbg(XRADIO_DBG_ERROR, "rf is not created.\n");
		goto err;
	}

	return 0;

err:
	xradio_dbg(XRADIO_DBG_ERROR, "%s faild\n", __func__);
	if (fw_dbg.dbgfs_rf)
		debugfs_remove_recursive(fw_dbg.dbgfs_rf);
	fw_dbg.dbgfs_rf = NULL;
	return 1;
}

static void fw_dbg_rf_deinit(void)
{
	if (fw_dbg.dbgfs_rf)
		debugfs_remove_recursive(fw_dbg.dbgfs_rf);
	fw_dbg.dbgfs_rf = NULL;
}

static int fw_dbg_epta_init(struct xradio_common *hw_priv,
				struct dentry *fw_node)
{
	fw_dbg.dbgfs_epta = debugfs_create_dir("epta", fw_node);
	if (!fw_dbg.dbgfs_epta) {
		xradio_dbg(XRADIO_DBG_ERROR, "epta is not created.\n");
		goto err;
	}

	if (!debugfs_create_file("time_line_ctrl", S_IRUSR | S_IWUSR,
			fw_dbg.dbgfs_epta, hw_priv, &fw_dbg_epta_time_line_ctrl))
		goto err;

	if (!debugfs_create_file("hw_rf_stat_ctrl", S_IRUSR | S_IWUSR,
			fw_dbg.dbgfs_epta, hw_priv, &fw_dbg_epta_rf_stat_ctrl))
		goto err;

	fwd_priv.epta_time_line_file = NULL;
	fwd_priv.epta_time_line_file_size = 0;

	fwd_priv.epta_rf_stat.origin_file = NULL;
	fwd_priv.epta_rf_stat.parsed_file = NULL;

	/* Initialize locks. */
	sema_init(&fwd_priv.epta_rf_stat.epta_rf_stat_sema, 1);
	sema_init(&fwd_priv.epta_time_line_sema, 1);

	return 0;

err:
	xradio_dbg(XRADIO_DBG_ERROR, "%s faild\n", __func__);
	if (fw_dbg.dbgfs_epta)
		debugfs_remove_recursive(fw_dbg.dbgfs_epta);
	fw_dbg.dbgfs_epta = NULL;
	return 1;
}

static void fw_dbg_epta_deinit(void)
{
	if (fw_dbg.dbgfs_epta)
		debugfs_remove_recursive(fw_dbg.dbgfs_epta);
	fw_dbg.dbgfs_epta = NULL;

	down(&fwd_priv.epta_time_line_sema);
	if (fwd_priv.epta_time_line_file) {
		xr_fileclose(fwd_priv.epta_time_line_file);
		fwd_priv.epta_time_line_file = NULL;
		fwd_priv.epta_time_line_file_size = 0x0;
	}
	up(&fwd_priv.epta_time_line_sema);

	down(&fwd_priv.epta_rf_stat.epta_rf_stat_sema);
	if (fwd_priv.epta_rf_stat.origin_file) {
		xr_fileclose(fwd_priv.epta_rf_stat.origin_file);
		fwd_priv.epta_rf_stat.origin_file = NULL;
	}

	if (fwd_priv.epta_rf_stat.parsed_file) {
		xr_fileclose(fwd_priv.epta_rf_stat.parsed_file);
		fwd_priv.epta_rf_stat.parsed_file = NULL;
	}
	up(&fwd_priv.epta_rf_stat.epta_rf_stat_sema);
}

static int fw_dbg_epta_indicate(struct fwd_msg *p_ind)
{
	switch (p_ind->dbg_id & FWD_CMD_MINOR_ID_MASK) {
	case FWD_CMD_MINOR_ID_EPTA_TIME_LINE:
		fw_dbg_epta_indicate_time_line(p_ind);
		break;
	case FWD_CMD_MINOR_ID_EPTA_RF_STAT:
		fw_dbg_epta_indicate_rf_stat(p_ind);
		break;
	default:
		xradio_dbg(XRADIO_DBG_ALWY, "fw dbg epta indicate: "
			"undefined sys minor id:0x%04x \n",
			(p_ind->dbg_id & FWD_CMD_MINOR_ID_MASK));
		break;
	}
	return 0;
}

static ssize_t fw_dbg_epta_show_time_line_ctrl(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{
	struct xradio_common *hw_priv = file->private_data;
	char buf[1024];
	size_t size = 0;

	struct fwd_msg *p_msg;
	struct fwd_epta_time_line_cfg *p_cfg;

	ssize_t msg_buf_size;
	ssize_t msg_req_size;
	ssize_t msg_cfm_size;

	sprintf(buf, "\nshow fw dbg epta time line config\n\n");

	msg_req_size = sizeof(struct fwd_msg)
			+ sizeof(struct fwd_epta_time_line_cfg);
	msg_cfm_size = msg_req_size;
	msg_buf_size = msg_cfm_size;


	p_msg = xr_kzalloc(msg_buf_size, false);
	if (p_msg == NULL) {
		sprintf(buf, "%s" "but not enough memory to show.\n", buf);
		goto err;
	}

	p_cfg = (struct fwd_epta_time_line_cfg *)(p_msg + 1);

	p_cfg->operation = FWD_EPTA_TIME_LINE_CFG_OPERATION_READ;

	if (xradio_fw_dbg_request(hw_priv, p_msg, msg_req_size, msg_cfm_size,
		FWD_CMD_MAJOR_ID_EPTA | FWD_CMD_MINOR_ID_EPTA_TIME_LINE)) {
		sprintf(buf, "%s" "but cfm msg status error.\n", buf);
		goto err;
	}

	sprintf(buf, "%s" "rf_switch_ctrl: %08x\n\n", buf, p_cfg->rf_switch_ctrl);

err:
	if (p_msg)
		kfree(p_msg);

	sprintf(buf, "%s\n", buf);
	size = strlen(buf);
	return simple_read_from_buffer(user_buf, count, ppos, buf, size);
}

static ssize_t fw_dbg_epta_config_time_line_ctrl(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct xradio_common *hw_priv = file->private_data;
	char  buf[256] = {0};
	u16   buf_size = (count > 255 ? 255 : count);
	char *startptr = &buf[0];
	char *endptr   = NULL;

	u32 cmd;

	struct fwd_msg *p_msg = NULL;
	struct fwd_epta_time_line_cfg *p_cfg;

	ssize_t msg_buf_size;
	ssize_t msg_req_size;
	ssize_t msg_cfm_size;

	if (!count)
		return -EINVAL;
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	xradio_dbg(XRADIO_DBG_ALWY, "config fw dbg epta time line. \n\n");

	cmd = simple_strtoul(startptr, &endptr, 16);
	startptr = endptr + 1;

	if (cmd == FWD_EPTA_TIME_LINE_CFG_CMD_SET_FW) {
		msg_req_size = sizeof(struct fwd_msg)
				+ sizeof(struct fwd_epta_time_line_cfg);
		msg_cfm_size = msg_req_size;
		msg_buf_size = msg_cfm_size;

		p_msg = xr_kzalloc(msg_buf_size, false);
		if (p_msg == NULL) {
			xradio_dbg(XRADIO_DBG_ALWY, "but not enough memory to show.\n");
			goto err;
		}

		p_cfg = (struct fwd_epta_time_line_cfg *)(p_msg + 1);

		p_cfg->operation = FWD_EPTA_TIME_LINE_CFG_OPERATION_WRITE;

		p_cfg->rf_switch_ctrl = simple_strtoul(startptr, &endptr, 16);
		startptr = endptr + 1;

		if (xradio_fw_dbg_request(hw_priv, p_msg, msg_req_size, msg_cfm_size,
			FWD_CMD_MAJOR_ID_EPTA | FWD_CMD_MINOR_ID_EPTA_TIME_LINE)) {
			xradio_dbg(XRADIO_DBG_ALWY, "but cfm msg status error.\n");
			goto err;
		}

		xradio_dbg(XRADIO_DBG_ALWY, "rf_switch_ctrl: %08x\n\n",
					p_cfg->rf_switch_ctrl);
	} else {

		fwd_priv.epta_time_line_file_size = FWD_EPTA_TIME_LINE_FILE_MAX_SIZE + 1;

		down(&fwd_priv.epta_time_line_sema);
		if (fwd_priv.epta_time_line_file) {
			xr_fileclose(fwd_priv.epta_time_line_file);
			fwd_priv.epta_time_line_file = NULL;
		}

		fwd_priv.epta_time_line_file =
				xr_fileopen("/data/tl", O_RDWR | O_CREAT | O_TRUNC, 0);
		fwd_priv.epta_time_line_file_size = 0x0;
		up(&fwd_priv.epta_time_line_sema);

		if (fwd_priv.epta_time_line_file) {
			xradio_dbg(XRADIO_DBG_ALWY,
				"epta time line: re-open /data/tl file success.\n");
		} else {
			xradio_dbg(XRADIO_DBG_ALWY,
				"epta time line: re-open /data/tl file failure.\n");
		}
	}
err:
	if (p_msg)
		kfree(p_msg);
	return count;
}

static int fw_dbg_epta_indicate_time_line(struct fwd_msg *p_ind)
{
	struct fwd_epta_time_line_dump *p_dump;

	if (fwd_priv.epta_time_line_file_size > FWD_EPTA_TIME_LINE_FILE_MAX_SIZE) {
		goto end;
	}

	p_dump = (struct fwd_epta_time_line_dump *)(p_ind + 1);

	down(&fwd_priv.epta_time_line_sema);
	if (!fwd_priv.epta_time_line_file) {

		fwd_priv.epta_time_line_file =
				xr_fileopen("/data/tl", O_RDWR | O_CREAT, 0);

		xradio_dbg(XRADIO_DBG_ALWY,
			"epta time line: open /data/tl, msg len %d, data len %d.\n",
			p_ind->dbg_len, p_dump->tl_data_len);
	}

	if (fwd_priv.epta_time_line_file) {

		xr_filewrite(fwd_priv.epta_time_line_file,
			(char *)(&p_dump->tl_data[0]), p_dump->tl_data_len);

		fwd_priv.epta_time_line_file_size += p_dump->tl_data_len;

		if (fwd_priv.epta_time_line_file_size > FWD_EPTA_TIME_LINE_FILE_MAX_SIZE) {

			xr_fileclose(fwd_priv.epta_time_line_file);
			fwd_priv.epta_time_line_file = NULL;

			xradio_dbg(XRADIO_DBG_ALWY,
				"epta time line: max file size %d\n",
				fwd_priv.epta_time_line_file_size);

			goto end;
		}
	} else {
		xradio_dbg(XRADIO_DBG_ALWY,
			"epta time line: can not open /data/tl file.\n");
		goto end;
	}
end:
	up(&fwd_priv.epta_time_line_sema);
	return 0;
}

static ssize_t fw_dbg_epta_show_rf_stat_ctrl(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{
	struct xradio_common *hw_priv = file->private_data;
	char buf[1024];
	size_t size = 0;

	struct fwd_msg *p_msg;
	struct fwd_epta_rf_stat_cfg *p_cfg;

	ssize_t msg_buf_size;
	ssize_t msg_req_size;
	ssize_t msg_cfm_size;

	sprintf(buf, "\nshow fw dbg epta rf stat config\n\n");

	msg_req_size = sizeof(struct fwd_msg)
			+ sizeof(struct fwd_epta_rf_stat_cfg);
	msg_cfm_size = msg_req_size;
	msg_buf_size = msg_cfm_size;


	p_msg = xr_kzalloc(msg_buf_size, false);
	if (p_msg == NULL) {
		sprintf(buf, "%s" "but not enough memory to show.\n", buf);
		goto err;
	}

	p_cfg = (struct fwd_epta_rf_stat_cfg *)(p_msg + 1);

	p_cfg->operation = FWD_EPTA_RF_STAT_CFG_OPERATION_READ;

	if (xradio_fw_dbg_request(hw_priv, p_msg, msg_req_size, msg_cfm_size,
		FWD_CMD_MAJOR_ID_EPTA | FWD_CMD_MINOR_ID_EPTA_RF_STAT)) {
		sprintf(buf, "%s" "but cfm msg status error.\n", buf);
		goto err;
	}

	sprintf(buf, "%s" "on_off_ctrl: 0x%08x\n\n", buf, p_cfg->on_off_ctrl);

err:
	if (p_msg)
		kfree(p_msg);

	sprintf(buf, "%s\n", buf);
	size = strlen(buf);
	return simple_read_from_buffer(user_buf, count, ppos, buf, size);
}

static ssize_t fw_dbg_epta_config_rf_stat_ctrl(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct xradio_common *hw_priv = file->private_data;
	char  buf[256] = {0};
	u16   buf_size = (count > 255 ? 255 : count);
	char *startptr = &buf[0];
	char *endptr   = NULL;

	u32 cmd;

	struct fwd_msg *p_msg = NULL;
	struct fwd_epta_rf_stat_cfg *p_cfg;

	ssize_t msg_buf_size;
	ssize_t msg_req_size;
	ssize_t msg_cfm_size;

	if (!count)
		return -EINVAL;
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	xradio_dbg(XRADIO_DBG_ALWY, "config fw dbg epta rf stat. \n\n");

	cmd = simple_strtoul(startptr, &endptr, 16);
	startptr = endptr + 1;

	if (cmd == FWD_EPTA_RF_STAT_CFG_CMD_RESET_FILE) {
		down(&fwd_priv.epta_rf_stat.epta_rf_stat_sema);
		if (fwd_priv.epta_rf_stat.parsed_file) {
			xr_fileclose(fwd_priv.epta_rf_stat.parsed_file);
			fwd_priv.epta_rf_stat.parsed_file = NULL;
		}
		fwd_priv.epta_rf_stat.parsed_file = xr_fileopen("/data/rfstat.txt",
				O_RDWR | O_CREAT | O_TRUNC, 0);

		xr_filewrite(fwd_priv.epta_rf_stat.parsed_file, NULL, 1);

		xr_fileclose(fwd_priv.epta_rf_stat.parsed_file);
					fwd_priv.epta_rf_stat.parsed_file = NULL;

		fwd_priv.epta_rf_stat.epta_index = 0;

		xradio_dbg(XRADIO_DBG_ALWY,
							"epta rf stat: clear /data/rfstat.txt file.\n");

		if (fwd_priv.epta_rf_stat.origin_file) {
			xr_fileclose(fwd_priv.epta_rf_stat.origin_file);
			fwd_priv.epta_rf_stat.origin_file = NULL;
		}
		fwd_priv.epta_rf_stat.origin_file = xr_fileopen("/data/origin_rfstat.txt",
				O_RDWR | O_CREAT | O_TRUNC, 0);

		xr_filewrite(fwd_priv.epta_rf_stat.origin_file, NULL, 1);

		xr_fileclose(fwd_priv.epta_rf_stat.origin_file);
					fwd_priv.epta_rf_stat.origin_file = NULL;

		up(&fwd_priv.epta_rf_stat.epta_rf_stat_sema);

		xradio_dbg(XRADIO_DBG_ALWY,
							"epta rf stat: clear /data/origin_rfstat.txt file.\n");
	} else {
		if (cmd == FWD_EPTA_RF_STAT_CFG_CMD_START_INFO) {
			fwd_priv.epta_rf_stat.epta_rf_first = 1;
			fwd_priv.epta_rf_stat.epta_index = 0;
		}

		msg_req_size = sizeof(struct fwd_msg)
				+ sizeof(struct fwd_epta_rf_stat_cfg);
		msg_cfm_size = msg_req_size;
		msg_buf_size = msg_cfm_size;

		p_msg = xr_kzalloc(msg_buf_size, false);
		if (p_msg == NULL) {
			xradio_dbg(XRADIO_DBG_ALWY, "but not enough memory to show.\n");
			goto err;
		}

		p_cfg = (struct fwd_epta_rf_stat_cfg *)(p_msg + 1);

		p_cfg->operation = FWD_EPTA_RF_STAT_CFG_OPERATION_WRITE;

		p_cfg->on_off_ctrl = cmd;

		if (xradio_fw_dbg_request(hw_priv, p_msg, msg_req_size, msg_cfm_size,
			FWD_CMD_MAJOR_ID_EPTA | FWD_CMD_MINOR_ID_EPTA_RF_STAT)) {
			xradio_dbg(XRADIO_DBG_ALWY, "but cfm msg status error.\n");
			goto err;
		}

		xradio_dbg(XRADIO_DBG_ALWY, "on_off_ctrl: 0x%08x\n\n",
					p_cfg->on_off_ctrl);
	}
err:
	if (p_msg)
		kfree(p_msg);
	return count;
}

static u32 fw_dbg_epta_rf_stat_recombine(u32 data, u8 len)
{
	u8 i;
	u32 redata = 0;

	for (i = 0; i < 4; i++) {
		redata |= (((data >> (i * len)) & ((1 << len) - 1)) << (i * 4));
	}
	return redata;
}

static int fw_dbg_epta_indicate_rf_stat(struct fwd_msg *p_ind)
{
	char *pbuffer;
	u32 *epta_rf;

	u32 epta_data0;
	u32 epta_data0_n;
	u32 epta_data1;
	u32 epta_data2;
	u32 epta_data3;
	u32 epta_data4;
	u32 epta_data5;
	u32 dbg_buf_len;
	u32 info_miss_duration;
	u32 entry_status;
	u32 queue_en;
	u32 wlan_type;
	u32 wlan_prio;
	u32 i = 0;
	u32 i_first = 0;

	u8 epta_rf_occupy;
	u8 info_miss_style;
	u8 bt_retry;
	u8 bt_tx_type_idx;
	u8 wlan_idle_flag = 0;

	struct fwd_epta_rf_stat_dump *p_dump;

	char epta_rf_stat_wlan_req_type[16][20] = {
					"rx_recovery", "rx_mcast", "rx_beacon",
					"tx_beacon", "tx_cts", "tx_high",
					"tx_uapsd", "fast_ps", "scan",
					"tx_pspoll_rx_ucast", "tx_low",
					"common", "x", "x", "x", "x"};
	char epta_rf_stat_bt_tx_type[FWD_EPTA_RF_STAT_BT_TX_TYPE_NUM][20]
					= { "NULL", "POLL", "FHS", "DM1",
					"DH1", "HV1", "HV2", "HV3_EV3",
					"DV", "AUX1", "DM3", "DH3",
					"EV4", "EV5", "DM5", "DH5",
					"NULL", "POLL", "FHS", "DM1",
					"2DH1", "HV1", "2EV3", "3EV3",
					"3DH1", "AUX1", "2DH3", "3DH3",
					"2EV5", "3EV5", "2DH5", "3DH5",
					"ADV_IND", "ADV_DIRECT_IND",
					"ADV_NONCONN_IND", "SCAN_REQ",
					"SCAN_RSP", "CONNECT_REQ",
					"ADV_SCAN_IND", "x" };
	char epta_rf_stat_bt_caton_type[8][20] = {
					"x", "low", "mid", "hig", "host-loss" };
	char epta_rf_stat_miss_type[8][20] = {
					"MISS_DBG_IRQ", "MISS_IRQ_STATUS",
					"MISS_HI_SEND",};

	p_dump = (struct fwd_epta_rf_stat_dump *)(p_ind + 1);
	epta_rf = (u32 *)(&p_dump->rf_stat_record[0][0]);

#if (FWD_EPTA_RF_STAT_RECORD_PARSED_DATA)

	/* write to file after parse */
	memset(fwd_priv.epta_rf_stat.buffer, 0, FWD_EPTA_RF_DATA_BUF_LEN);
	pbuffer = fwd_priv.epta_rf_stat.buffer;

	if (fwd_priv.epta_rf_stat.epta_rf_first == 1) {
		i_first = 1;

		fwd_priv.epta_rf_stat.epta_data0_l = *(epta_rf + 0 * FWD_EPTA_RF_STAT_POINT_NUM);
		fwd_priv.epta_rf_stat.epta_data1_l = *(epta_rf + 1 * FWD_EPTA_RF_STAT_POINT_NUM);
		fwd_priv.epta_rf_stat.epta_data2_l = *(epta_rf + 2 * FWD_EPTA_RF_STAT_POINT_NUM);
		fwd_priv.epta_rf_stat.epta_data3_l = *(epta_rf + 3 * FWD_EPTA_RF_STAT_POINT_NUM);
		fwd_priv.epta_rf_stat.epta_data4_l = *(epta_rf + 4 * FWD_EPTA_RF_STAT_POINT_NUM);
		fwd_priv.epta_rf_stat.epta_data5_l = *(epta_rf + 5 * FWD_EPTA_RF_STAT_POINT_NUM);

		fwd_priv.epta_rf_stat.epta_rf_first = 0;
	}

	pbuffer = fwd_priv.epta_rf_stat.buffer + strlen(fwd_priv.epta_rf_stat.buffer);

	for (i = i_first; i < FWD_EPTA_RF_STAT_POINT_NUM - 1; i++) {
		epta_data0 = *(epta_rf + 0 * FWD_EPTA_RF_STAT_POINT_NUM + i);
		epta_data1 = *(epta_rf + 1 * FWD_EPTA_RF_STAT_POINT_NUM + i);
		epta_data2 = *(epta_rf + 2 * FWD_EPTA_RF_STAT_POINT_NUM + i);
		epta_data3 = *(epta_rf + 3 * FWD_EPTA_RF_STAT_POINT_NUM + i);
		epta_data4 = *(epta_rf + 4 * FWD_EPTA_RF_STAT_POINT_NUM + i);
		epta_data5 = *(epta_rf + 5 * FWD_EPTA_RF_STAT_POINT_NUM + i);
		epta_rf_occupy = epta_data1 & 0x7;

		if (((epta_data1 & 0x7) == FWD_EPTA_RF_STAT_BT2WLAN_NOW_WLAN)
			&& (((epta_data1  >> 3) & 0x3) == FWD_EPTA_RF_STAT_SUB_WLAN_ACTIVE)) {
			fwd_priv.epta_rf_stat.epta_wl_type = (epta_data1 >> 8) & 0xf;
			fwd_priv.epta_rf_stat.epta_wl_prio = (epta_data1 >> 5) & 0x7;
		}

		switch (epta_rf_occupy) {
		case FWD_EPTA_RF_STAT_WLAN2BT_NOW_BT:  /* wlan2bt, and now is bt */
			if (i > 0) {
				fwd_priv.epta_rf_stat.epta_data0_l
					= *(epta_rf + 0 * FWD_EPTA_RF_STAT_POINT_NUM + i - 1);
				fwd_priv.epta_rf_stat.epta_data1_l
					= *(epta_rf + 1 * FWD_EPTA_RF_STAT_POINT_NUM + i - 1);
				fwd_priv.epta_rf_stat.epta_data2_l
					= *(epta_rf + 2 * FWD_EPTA_RF_STAT_POINT_NUM + i - 1);
				fwd_priv.epta_rf_stat.epta_data3_l
					= *(epta_rf + 3 * FWD_EPTA_RF_STAT_POINT_NUM + i - 1);
				fwd_priv.epta_rf_stat.epta_data4_l
					= *(epta_rf + 4 * FWD_EPTA_RF_STAT_POINT_NUM + i - 1);
				fwd_priv.epta_rf_stat.epta_data5_l
					= *(epta_rf + 5 * FWD_EPTA_RF_STAT_POINT_NUM + i - 1);
			}

			/* entry status(after change): bit0~3:queue0, bit4~7:queue1, bit8~11:queue2, bit12~15:queue3 */
			entry_status = (fwd_priv.epta_rf_stat.epta_data3_l >> 18) & 0xfff;
			entry_status = fw_dbg_epta_rf_stat_recombine(entry_status, 3);

			/* queue enable(after change): bit0~3:queue0, bit4~7:queue1, bit8~11:queue2, bit12~15:queue3 */
			queue_en = (fwd_priv.epta_rf_stat.epta_data3_l >> 14) & 0xf;
			queue_en = fw_dbg_epta_rf_stat_recombine(queue_en, 1);

			wlan_type = (fwd_priv.epta_rf_stat.epta_data1_l >> 8) & 0xf;
			wlan_prio = (fwd_priv.epta_rf_stat.epta_data1_l >> 5) & 0x7;

			if (((fwd_priv.epta_rf_stat.epta_data1_l  >> 3) & 0x3)
					== FWD_EPTA_RF_STAT_SUB_JUST_BT2WLAN
					|| ((fwd_priv.epta_rf_stat.epta_data1_l	>> 3) & 0x3)
					== FWD_EPTA_RF_STAT_SUB_WLAN_INACTIVE) {

				/* 1:wlan in blockBT stat, 0:in unblockBT stat */
				if ((fwd_priv.epta_rf_stat.epta_data3_l >> 31) & 0x1) {

					/* if it has not unblockBT, treat it as wlan */
					wlan_idle_flag = FWD_EPTA_RF_STAT_WLAN_DUR;
					wlan_type = fwd_priv.epta_rf_stat.epta_wl_type;
					wlan_prio = fwd_priv.epta_rf_stat.epta_wl_prio;
				} else {

					 /* only if it has unblockBT, treat it as idle */
					wlan_idle_flag = FWD_EPTA_RF_STAT_IDLE_DUR;
				}
			} else if (((fwd_priv.epta_rf_stat.epta_data1_l & 0x7)
					== FWD_EPTA_RF_STAT_BT2WLAN_NOW_WLAN)
					&& (((fwd_priv.epta_rf_stat.epta_data1_l  >> 3) & 0x3)
					== FWD_EPTA_RF_STAT_SUB_WLAN_ACTIVE)){
				wlan_idle_flag = FWD_EPTA_RF_STAT_WLAN_DUR;
			}

			/* if last one is just bt2wlan or unblockBT, means that it is idle during this time */
			if (((fwd_priv.epta_rf_stat.epta_data1_l & 0x7) != FWD_EPTA_RF_STAT_MISS_INFO)
				&& (wlan_idle_flag == FWD_EPTA_RF_STAT_IDLE_DUR)) {
				u32 rf_idle_duration = fwd_priv.epta_rf_stat.epta_data1_l >> 12;

				/* if last is JUST_BT2WLAN and it is not overflow, use hw consume time */
				if (!((((fwd_priv.epta_rf_stat.epta_data1_l	>> 3) & 0x3)
					== FWD_EPTA_RF_STAT_SUB_JUST_BT2WLAN)
					&& (rf_idle_duration != 0xfffff))) {
					rf_idle_duration = FWD_EPTA_RF_STAT_GET_DURATION
						((fwd_priv.epta_rf_stat.epta_data0_l), (epta_data0));
				}

				sprintf(pbuffer, "%s%d%s%s%s%s%s%d%s%s%s%s%s%d%s%04x"
						"%s%04X%s%d%s%d%s%d%s%d%s%s%s%d"
						"%s%s%s%d%s%s%s%s%s0x%08X%s%d%s%d"
						"%s%d%s%d%s%d",
						"\n", ++(fwd_priv.epta_rf_stat.epta_index),
						"\t",  /* occupy(wlan) */
						"\t",  /* wlan_duration */
						"\t", "Idle",  /* occupy(idle) */
						"\t", rf_idle_duration,  /* idle_duration */
						"\t",  /* occupy(bt) */
						"\t",  /* bt_duration */
						"\t",	/* wlan type */
						"\t",	/* wlan prio */
						"\t", (fwd_priv.epta_rf_stat.epta_data3_l >> 11) & 0x7, /* wlan hw prio */
						"\t", queue_en, /* queue_en status */
						"\t", entry_status, /* seq entry status */
						"\t", (fwd_priv.epta_rf_stat.epta_data2_l >> 0) & 0x3ff, /* TTCS_END num */
						"\t", (fwd_priv.epta_rf_stat.epta_data2_l >> 10) & 0xf, /* RECIPE_END num */
						"\t", (fwd_priv.epta_rf_stat.epta_data2_l >> 14) & 0xf, /* RX_EVENT num */
						"\t", (fwd_priv.epta_rf_stat.epta_data2_l >> 18) & 0xf, /* bt_abort num */
						"\t", epta_rf_stat_wlan_req_type
									[(fwd_priv.epta_rf_stat.epta_data3_l >> 4) & 0xf], /* pending wlan req type */
						"\t", (fwd_priv.epta_rf_stat.epta_data2_l >> 22) & 0x3f,	/* bt req num */
						"\t",  /* 1:bt tx; 0:bt rx */
						"\t",  /* bt tx type */
						"\t", (fwd_priv.epta_rf_stat.epta_data3_l >> 8) & 0x7, /* bt prio */
						"\t",  /* bt linkid */
						"\t",  /* bt retry */
						"\t", epta_rf_stat_bt_caton_type[fwd_priv.epta_rf_stat.epta_data3_l & 0x7],  /* bt caton */
						"\t", fwd_priv.epta_rf_stat.epta_data4_l, /* pif msg */
						"\t", (fwd_priv.epta_rf_stat.epta_data3_l >> 30) & 0x1, /* rf force */
						"\t", (fwd_priv.epta_rf_stat.epta_data3_l >> 31) & 0x1, /* wlan block bt */
						"\t", (fwd_priv.epta_rf_stat.epta_data5_l >> 0) & 0xff, /* u8_resv_1 */
						"\t", (fwd_priv.epta_rf_stat.epta_data5_l >> 8) & 0xff, /* u8_resv_2 */
						"\t", (fwd_priv.epta_rf_stat.epta_data5_l >> 16) & 0xffff /* u16_resv_1 */
						);
			} else if (wlan_idle_flag == FWD_EPTA_RF_STAT_WLAN_DUR) {

				/* if last one is just blockBT, means that it is wlan during this time */
				sprintf(pbuffer, "%s%d%s%s%s%d%s%s%s%s%s%s%s%d%s%d"
						"%s%04x%s%04X%s%d%s%d%s%d%s%d%s%s%s%d"
						"%s%s%s%d%s%s%s%s%s0x%08X%s%d%s%d"
						"%s%d%s%d%s%d",
						"\n", ++(fwd_priv.epta_rf_stat.epta_index),
						"\t", "Wlan",  /* occupy(wlan) */
						"\t", FWD_EPTA_RF_STAT_GET_DURATION
								((fwd_priv.epta_rf_stat.epta_data0_l), (epta_data0)),	/* wlan_duration */
						"\t",  /* occupy(idle) */
						"\t",  /* idle_duration */
						"\t",  /* occupy(bt) */
						"\t",  /* bt_duration */
						"\t",	epta_rf_stat_wlan_req_type[wlan_type],  /* wlan type */
						"\t",	wlan_prio, /* wlan sw prio */
						"\t", (fwd_priv.epta_rf_stat.epta_data3_l >> 11) & 0x7, /* wlan hw prio */
						"\t", queue_en, /* queue_en status */
						"\t", entry_status, /* seq entry status */
						"\t", (fwd_priv.epta_rf_stat.epta_data2_l >> 0) & 0x3ff, /* TTCS_END num */
						"\t", (fwd_priv.epta_rf_stat.epta_data2_l >> 10) & 0xf, /* RECIPE_END num */
						"\t", (fwd_priv.epta_rf_stat.epta_data2_l >> 14) & 0xf, /* RX_EVENT num */
						"\t", (fwd_priv.epta_rf_stat.epta_data2_l >> 18) & 0xf, /* bt_abort num */
						"\t", epta_rf_stat_wlan_req_type
									[(fwd_priv.epta_rf_stat.epta_data3_l >> 4) & 0xf], /* pending wlan req type */
						"\t", (fwd_priv.epta_rf_stat.epta_data2_l >> 22) & 0x3f, /* bt req num */
						"\t",  /* 1:bt tx; 0:bt rx */
						"\t",  /* bt tx type */
						"\t", (fwd_priv.epta_rf_stat.epta_data3_l >> 8) & 0x7, /* bt prio */
						"\t",  /* bt linkid */
						"\t",  /* bt retry */
						"\t", epta_rf_stat_bt_caton_type[fwd_priv.epta_rf_stat.epta_data3_l & 0x7],  /* bt caton */
						"\t", fwd_priv.epta_rf_stat.epta_data4_l, /* pif msg */
						"\t", (fwd_priv.epta_rf_stat.epta_data3_l >> 30) & 0x1, /* rf force */
						"\t", (fwd_priv.epta_rf_stat.epta_data3_l >> 31) & 0x1, /* wlan block bt */
						"\t", (fwd_priv.epta_rf_stat.epta_data5_l >> 0) & 0xff, /* u8_resv_1 */
						"\t", (fwd_priv.epta_rf_stat.epta_data5_l >> 8) & 0xff, /* u8_resv_2 */
						"\t", (fwd_priv.epta_rf_stat.epta_data5_l >> 16) & 0xffff /* u16_resv_1 */
						);
			}
			pbuffer = fwd_priv.epta_rf_stat.buffer + strlen(fwd_priv.epta_rf_stat.buffer);
			wlan_idle_flag = 0; /* reset */

			if (((epta_data1 >> 3) & 0x1) == 1) {  /* bit3:tx/rx */
				bt_retry = (epta_data1 >> 12) & 0x1;  /* bt tx retry bit */

				/* bit15: is ble or not; bit14: is edr or not */
				bt_tx_type_idx = ((epta_data1 >> 15) & 0x1) ? (32 + ((epta_data2 >> 28) & 0x7))
							: (16 * ((epta_data1 >> 14) & 0x1) + ((epta_data2 >> 28) & 0xf));
			} else {
				bt_retry = (epta_data1 >> 12) & 0x2;  /* bt rx retry bit */
				bt_tx_type_idx = FWD_EPTA_RF_STAT_BT_TX_TYPE_NUM - 1;
			}

			/* entry status(after change): bit0~3:queue0, bit4~7:queue1, bit8~11:queue2, bit12~15:queue3 */
			entry_status = (fwd_priv.epta_rf_stat.epta_data3_l >> 18) & 0xfff;
			entry_status = fw_dbg_epta_rf_stat_recombine(entry_status, 3);

			/* queue enable(after change): bit0~3:queue0, bit4~7:queue1, bit8~11:queue2, bit12~15:queue3 */
			queue_en = (fwd_priv.epta_rf_stat.epta_data3_l >> 14) & 0xf;
			queue_en = fw_dbg_epta_rf_stat_recombine(queue_en, 1);

			sprintf(pbuffer, "%s%d%s%s%s%s%s%s%s%d%s%s%s%d"
					"%s%04x%s%04X%s%d%s%d%s%d%s%d"
					"%s%s%s%d%s%s%s%s%s%d%s%d%s%d%s"
					"%s%s0x%08X%s%d%s%d%s%d%s%d%s%d",
					"\n", ++(fwd_priv.epta_rf_stat.epta_index),
					"\t",  /* occupy(wlan) */
					"\t",  /* wlan_duration */
					"\t",  /* occupy(idle) */
					"\t",  /* idle_duration */
					"\t", "Bt",  /* occupy(bt) */
					"\t", epta_data1 >> 16,  /* bt_duration */
					"\t",  /* wlan type */
					"\t",  /* wlan prio */
					"\t", (epta_data3 >> 11) & 0x7, /* wlan hw prio */
					"\t", queue_en, /* queue_en status */
					"\t", entry_status, /* seq entry status */
					"\t", (epta_data2 >> 0) & 0x3ff, /* TTCS_END num */
					"\t", (epta_data2 >> 10) & 0xf, /* RECIPE_END num */
					"\t", (epta_data2 >> 14) & 0xf, /* RX_EVENT num */
					"\t", (epta_data2 >> 18) & 0xf, /* bt_abort num */
					"\t", epta_rf_stat_wlan_req_type
									[(epta_data3 >> 4) & 0xf], /* pending wlan req type */
					"\t", (epta_data2 >> 22) & 0x3f,  /* bt req num */
					"\t", FWD_EPTA_RF_STAT_GET_BT_TYPE((epta_data1 >> 3) & 0x1), /* 1:bt tx; 0:bt rx */
					"\t", epta_rf_stat_bt_tx_type[bt_tx_type_idx], /* bt tx type */
					"\t", (epta_data1 >> 4) & 0x1, /* bt prio */
					"\t", (epta_data1 >> 5) & 0x7f, /* bt linkid */
					"\t", bt_retry,  /* bt retry */
					"\t", epta_rf_stat_bt_caton_type[epta_data3 & 0x7], /* bt caton */
					"\t", epta_data4, /* pif msg */
					"\t", (epta_data3 >> 30) & 0x1, /* rf force */
					"\t", (epta_data3 >> 31) & 0x1, /* wlan block bt */
					"\t", (epta_data5 >> 0) & 0xff, /* u8_resv_1 */
					"\t", (epta_data5 >> 8) & 0xff, /* u8_resv_2 */
					"\t", (epta_data5 >> 16) & 0xffff /* u16_resv_1 */
					);
			break;

		case FWD_EPTA_RF_STAT_BT2WLAN_NOW_WLAN: /* bt2wlan, and now is wlan */
			while ((((*(epta_rf + 1 * FWD_EPTA_RF_STAT_POINT_NUM + i)) & 0x7)
					== FWD_EPTA_RF_STAT_BT2WLAN_NOW_WLAN)
					&& (i < FWD_EPTA_RF_STAT_POINT_NUM - 1)) {
				u8 wlan_subtype;
				if (i > 0) {
					fwd_priv.epta_rf_stat.epta_data0_l
						= *(epta_rf + 0 * FWD_EPTA_RF_STAT_POINT_NUM + i - 1);
					fwd_priv.epta_rf_stat.epta_data1_l
						= *(epta_rf + 1 * FWD_EPTA_RF_STAT_POINT_NUM + i - 1);
					fwd_priv.epta_rf_stat.epta_data2_l
						= *(epta_rf + 2 * FWD_EPTA_RF_STAT_POINT_NUM + i - 1);
					fwd_priv.epta_rf_stat.epta_data3_l
						= *(epta_rf + 3 * FWD_EPTA_RF_STAT_POINT_NUM + i - 1);
					fwd_priv.epta_rf_stat.epta_data4_l
						= *(epta_rf + 4 * FWD_EPTA_RF_STAT_POINT_NUM + i - 1);
					fwd_priv.epta_rf_stat.epta_data5_l
						= *(epta_rf + 5 * FWD_EPTA_RF_STAT_POINT_NUM + i - 1);
				}
				epta_data0 = *(epta_rf + 0 * FWD_EPTA_RF_STAT_POINT_NUM + i);
				epta_data1 = *(epta_rf + 1 * FWD_EPTA_RF_STAT_POINT_NUM + i);
				wlan_subtype = (epta_data1 >> 3) & 0x3;
				epta_data2 = *(epta_rf + 2 * FWD_EPTA_RF_STAT_POINT_NUM + i);
				epta_data3 = *(epta_rf + 3 * FWD_EPTA_RF_STAT_POINT_NUM + i);
				epta_data4 = *(epta_rf + 4 * FWD_EPTA_RF_STAT_POINT_NUM + i);
				epta_data5 = *(epta_rf + 5 * FWD_EPTA_RF_STAT_POINT_NUM + i);

				 /* entry status(after change): bit0~3:queue0, bit4~7:queue1, bit8~11:queue2, bit12~15:queue3 */
				 entry_status = (fwd_priv.epta_rf_stat.epta_data3_l >> 18) & 0xfff;
				 entry_status = fw_dbg_epta_rf_stat_recombine(entry_status, 3);

				 /* queue enable(after change): bit0~3:queue0, bit4~7:queue1, bit8~11:queue2, bit12~15:queue3 */
				 queue_en = (fwd_priv.epta_rf_stat.epta_data3_l >> 14) & 0xf;
				 queue_en = fw_dbg_epta_rf_stat_recombine(queue_en, 1);

				 /* 0x0: bt2wlan; 0x1: blockBT; 0x2: unblockBT; 0x3: Bt Req num */
				if (wlan_subtype == FWD_EPTA_RF_STAT_SUB_WLAN_ACTIVE) {
					 fwd_priv.epta_rf_stat.epta_wl_type = (epta_data1 >> 8) & 0xf; /* store */
					 fwd_priv.epta_rf_stat.epta_wl_prio = (epta_data1 >> 5) & 0x7;
					sprintf(pbuffer, "%s%d%s%s%s%s%s%d%s%s%s%s%s%d%s%04x"
							"%s%04X%s%d%s%d%s%d%s%d%s%s%s%d%s%s%s%d"
							"%s%s%s%s%s0x%08X%s%d%s%d%s%d%s%d%s%d",
							"\n", ++(fwd_priv.epta_rf_stat.epta_index),
							"\t",  /* occupy(wlan) */
							"\t",  /* wlan_duration */
							"\t", "Idle",  /* occupy(idle) */
							"\t", FWD_EPTA_RF_STAT_GET_DURATION
									((fwd_priv.epta_rf_stat.epta_data0_l), (epta_data0)),	/* idle_duration */
							"\t",  /* occupy(bt) */
							"\t",  /* bt_duration */
							"\t",  /* wlan type */
							"\t",  /* wlan prio */
							"\t", (fwd_priv.epta_rf_stat.epta_data3_l >> 11) & 0x7, /* wlan hw prio */
							"\t", queue_en, /* queue_en status */
							"\t", entry_status, /* seq entry status */
							"\t", (fwd_priv.epta_rf_stat.epta_data2_l >> 0) & 0x3ff,  /* TTCS_END num */
							"\t", (fwd_priv.epta_rf_stat.epta_data2_l >> 10) & 0xf,	/* RECIPE_END num */
							"\t", (fwd_priv.epta_rf_stat.epta_data2_l >> 14) & 0xf,	/* RX_EVENT num */
							"\t", (fwd_priv.epta_rf_stat.epta_data2_l >> 18) & 0xf,	/* bt_abort num */
							"\t", epta_rf_stat_wlan_req_type
									[(fwd_priv.epta_rf_stat.epta_data3_l >> 4) & 0xf], /* pending wlan req type */
							"\t", (fwd_priv.epta_rf_stat.epta_data2_l >> 22) & 0x3f, /* bt req num */
							"\t",  /* 1:bt tx; 0:bt rx */
							"\t",  /* bt tx type */
							"\t", (fwd_priv.epta_rf_stat.epta_data3_l >> 8) & 0x7, /* bt prio */
							"\t", /* bt linkid */
							"\t",  /* bt retry */
							"\t", epta_rf_stat_bt_caton_type[fwd_priv.epta_rf_stat.epta_data3_l & 0x7], /* bt caton */
							"\t", fwd_priv.epta_rf_stat.epta_data4_l, /* pif msg */
							"\t", (fwd_priv.epta_rf_stat.epta_data3_l >> 30) & 0x1, /* rf force */
							"\t", (fwd_priv.epta_rf_stat.epta_data3_l >> 31) & 0x1, /* wlan block bt */
							"\t", (fwd_priv.epta_rf_stat.epta_data5_l >> 0) & 0xff, /* u8_resv_1 */
							"\t", (fwd_priv.epta_rf_stat.epta_data5_l >> 8) & 0xff, /* u8_resv_2 */
							"\t", (fwd_priv.epta_rf_stat.epta_data5_l >> 16) & 0xffff /* u16_resv_1 */
							);
				} else if (wlan_subtype == FWD_EPTA_RF_STAT_SUB_WLAN_INACTIVE) {
					/* unblockBT */
					sprintf(pbuffer, "%s%d%s%s%s%d%s%s%s%s%s%s%s%d%s%d"
							"%s%04x%s%04X%s%d%s%d%s%d%s%d%s%s%s%d"
							"%s%s%s%d%s%s%s%s%s0x%08X%s%d%s%d%s%d"
							"%s%d%s%d",
							"\n", ++(fwd_priv.epta_rf_stat.epta_index),
							"\t", "Wlan",  /* occupy(wlan) */
							"\t", FWD_EPTA_RF_STAT_GET_DURATION
									((fwd_priv.epta_rf_stat.epta_data0_l), (epta_data0)),	/* wlan_duration */
							"\t",  /* occupy(idle) */
							"\t",  /* idle_duration */
							"\t",  /* occupy(bt) */
							"\t",  /* bt_duration */
							"\t", epta_rf_stat_wlan_req_type
									[(epta_data1 >> 8) & 0xf], /* wlan type */
							"\t", (epta_data1 >> 5) & 0x7, /* wlan prio */
							"\t", (fwd_priv.epta_rf_stat.epta_data3_l >> 11) & 0x7, /* wlan hw prio */
							"\t", queue_en, /* queue_en status */
							"\t", entry_status, /* seq entry status */
							"\t",	(fwd_priv.epta_rf_stat.epta_data2_l >> 0) & 0x3ff,  /* TTCS_END num */
							"\t", (epta_data1 >> 12) & 0x3ff,  /* RECIPE_END num */
							"\t", (epta_data1 >> 22) & 0x3ff,  /* RX_EVENT num */
							"\t", (fwd_priv.epta_rf_stat.epta_data2_l >> 18) & 0xf,	/* bt_abort num */
							"\t", epta_rf_stat_wlan_req_type
									[(fwd_priv.epta_rf_stat.epta_data3_l >> 4) & 0xf], /* pending wlan req type */
							"\t", (fwd_priv.epta_rf_stat.epta_data2_l >> 22) & 0x3f, /* bt req num */
							"\t",  /* 1:bt tx; 0:bt rx */
							"\t",  /* bt tx type */
							"\t", (fwd_priv.epta_rf_stat.epta_data3_l >> 8) & 0x7, /* bt prio */
							"\t", /* bt linkid */
							"\t",  /* bt retry */
							"\t", epta_rf_stat_bt_caton_type[fwd_priv.epta_rf_stat.epta_data3_l & 0x7], /* bt caton */
							"\t", fwd_priv.epta_rf_stat.epta_data4_l, /* pif msg */
							"\t", (fwd_priv.epta_rf_stat.epta_data3_l >> 30) & 0x1, /* rf force */
							"\t", (fwd_priv.epta_rf_stat.epta_data3_l >> 31) & 0x1, /* wlan block bt */
							"\t", (fwd_priv.epta_rf_stat.epta_data5_l >> 0) & 0xff, /* u8_resv_1 */
							"\t", (fwd_priv.epta_rf_stat.epta_data5_l >> 8) & 0xff, /* u8_resv_2 */
							"\t", (fwd_priv.epta_rf_stat.epta_data5_l >> 16) & 0xffff /* u16_resv_1 */
							);
				}
				i++;
				pbuffer = fwd_priv.epta_rf_stat.buffer + strlen(fwd_priv.epta_rf_stat.buffer);
			}
			i--;
			break;

		case FWD_EPTA_RF_STAT_MISS_INFO: /* miss info */
			info_miss_style = (epta_data1 >> 3) & 0x7;
			epta_data0 = *(epta_rf + 0 * FWD_EPTA_RF_STAT_POINT_NUM + i);
			epta_data0_n = *(epta_rf + 0 * FWD_EPTA_RF_STAT_POINT_NUM + i + 1);

			if (info_miss_style == FWD_EPTA_RF_STAT_MISS_HIF_SEND)
				info_miss_duration = FWD_EPTA_RF_STAT_GET_DURATION
						(fwd_priv.epta_rf_stat.epta_data0_l, epta_data0);
			else
				info_miss_duration
						= FWD_EPTA_RF_STAT_GET_DURATION(epta_data0, epta_data0_n);

			sprintf(pbuffer, "%s%d%s%s%s%d%s%s",
					"\n", ++(fwd_priv.epta_rf_stat.epta_index),
					"\t", "Lost",
					"\t", info_miss_duration,  /* lost time */
					"\t", epta_rf_stat_miss_type[(info_miss_style - 1) & 0x7]  /* lost style */
					);
			break;

		default:
			break;

		}
		pbuffer = fwd_priv.epta_rf_stat.buffer + strlen(fwd_priv.epta_rf_stat.buffer);
	}

	/* save the last one */
	fwd_priv.epta_rf_stat.epta_data0_l = *(epta_rf + 0 * FWD_EPTA_RF_STAT_POINT_NUM
									+ (FWD_EPTA_RF_STAT_POINT_NUM - 1) - 1);
	fwd_priv.epta_rf_stat.epta_data1_l = *(epta_rf + 1 * FWD_EPTA_RF_STAT_POINT_NUM
									+ (FWD_EPTA_RF_STAT_POINT_NUM - 1) - 1);
	fwd_priv.epta_rf_stat.epta_data2_l = *(epta_rf + 2 * FWD_EPTA_RF_STAT_POINT_NUM
									+ (FWD_EPTA_RF_STAT_POINT_NUM - 1) - 1);
	fwd_priv.epta_rf_stat.epta_data3_l = *(epta_rf + 3 * FWD_EPTA_RF_STAT_POINT_NUM
									+ (FWD_EPTA_RF_STAT_POINT_NUM - 1) - 1);
	fwd_priv.epta_rf_stat.epta_data4_l = *(epta_rf + 4 * FWD_EPTA_RF_STAT_POINT_NUM
									+ (FWD_EPTA_RF_STAT_POINT_NUM - 1) - 1);
	fwd_priv.epta_rf_stat.epta_data5_l = *(epta_rf + 5 * FWD_EPTA_RF_STAT_POINT_NUM
									+ (FWD_EPTA_RF_STAT_POINT_NUM - 1) - 1);

	dbg_buf_len = strlen(fwd_priv.epta_rf_stat.buffer);

	down(&fwd_priv.epta_rf_stat.epta_rf_stat_sema);
	if (!fwd_priv.epta_rf_stat.parsed_file) {
		fwd_priv.epta_rf_stat.parsed_file = xr_fileopen("/data/rfstat.txt",
							O_RDWR|O_CREAT|O_APPEND, 0);
		sprintf(fwd_priv.epta_rf_stat.head_buf, "%s%s%s%s%s%s%s%s%s%s%s%s"
						"%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s"
						"%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s"
						"%s%s%s%s%s%s%s%s%s%s%s%s",
				"\n", "index",
				"\t", "rf_occupy",
				"\t", "wlan_duration",
				"\t", "rf_occupy",
				"\t", "idle_duration",
				"\t", "rf_occupy",
				"\t", "bt_duration",
				"\t", "wlan_type",
				"\t", "sw_prio",
				"\t", "hw_prio",
				"\t", "queue_en",
				"\t", "entry_status",
				"\t", "TtcsEnd_num",
				"\t", "RecipeEnd_num",
				"\t", "RxEvent_num",
				"\t", "bt_abort_num",
				"\t", "wl_pending_req",
				"\t", "bt_req_num",
				"\t", "bt_type",
				"\t", "bt_tx_type",
				"\t", "bt_prio",
				"\t", "bt_linkid",
				"\t", "bt_retry",
				"\t", "bt_caton",
				"\t", "pif_msg",
				"\t", "rf_force",
				"\t", "wl_block_bt",
				"\t", "u8_resv_1",
				"\t", "u8_resv_2",
				"\t", "u16_resv_1"
				);
		xr_filewrite(fwd_priv.epta_rf_stat.parsed_file,
					(char *)&(fwd_priv.epta_rf_stat.head_buf),
					strlen(fwd_priv.epta_rf_stat.head_buf));
		memset(fwd_priv.epta_rf_stat.head_buf, 0, FWD_EPTA_RF_HEAD_BUF_LEN);
	}
	if (fwd_priv.epta_rf_stat.parsed_file) {
		int xr_file_size = 0;

		xr_filewrite(fwd_priv.epta_rf_stat.parsed_file,
					(char *)&(fwd_priv.epta_rf_stat.buffer), dbg_buf_len);
		/*get file size.*/
		if (fwd_priv.epta_rf_stat.parsed_file->fp->f_op->llseek != NULL) {
			mm_segment_t old_fs = get_fs();
			set_fs(KERNEL_DS);
			vfs_llseek(fwd_priv.epta_rf_stat.parsed_file->fp, 0, SEEK_SET);  /* move to first */
			 /* get the size, then move to end */
			xr_file_size = vfs_llseek(fwd_priv.epta_rf_stat.parsed_file->fp, 0, SEEK_END);
			set_fs(old_fs);
		}
		xradio_dbg(XRADIO_DBG_MSG,  "[rf_stat]file_size = %d\n", xr_file_size);

		if (xr_file_size > FWD_EPTA_RF_STAT_FILE_MAX_SIZE) {
			xr_fileclose(fwd_priv.epta_rf_stat.parsed_file);
			fwd_priv.epta_rf_stat.parsed_file = NULL;
			xradio_dbg(XRADIO_DBG_WARN,  "[rf_stat]file_size max, close file\n");
			up(&fwd_priv.epta_rf_stat.epta_rf_stat_sema);
			goto end;;
		}
	}
	up(&fwd_priv.epta_rf_stat.epta_rf_stat_sema);
#endif  /* end of write to file after parse */

#if (FWD_EPTA_RF_STAT_RECORD_ORIGIN_DATA)

	/* write to file without parse */
	memset(fwd_priv.epta_rf_stat.buffer, 0, FWD_EPTA_RF_DATA_BUF_LEN);
	pbuffer = fwd_priv.epta_rf_stat.buffer;
	for (i = 0; i < FWD_EPTA_RF_STAT_POINT_NUM - 1; i++) {
		sprintf(pbuffer, "%02u%s0x%08X%s0x%08X%s0x%08X%s0x%08X"
				"%s0x%08X%s0x%08X%s",
				i, "\t", *(epta_rf + 0 * FWD_EPTA_RF_STAT_POINT_NUM + i),
				"\t", *(epta_rf + 1 * FWD_EPTA_RF_STAT_POINT_NUM + i),
				"\t", *(epta_rf + 2 * FWD_EPTA_RF_STAT_POINT_NUM + i),
				"\t", *(epta_rf + 3 * FWD_EPTA_RF_STAT_POINT_NUM + i),
				"\t", *(epta_rf + 4 * FWD_EPTA_RF_STAT_POINT_NUM + i),
				"\t", *(epta_rf + 5 * FWD_EPTA_RF_STAT_POINT_NUM + i), "\n");
		pbuffer = fwd_priv.epta_rf_stat.buffer + strlen(fwd_priv.epta_rf_stat.buffer);
	}

	dbg_buf_len = strlen(fwd_priv.epta_rf_stat.buffer);

	down(&fwd_priv.epta_rf_stat.epta_rf_stat_sema);
	if (!fwd_priv.epta_rf_stat.origin_file) {
		fwd_priv.epta_rf_stat.origin_file = xr_fileopen("/data/origin_rfstat.txt",
					O_RDWR|O_CREAT|O_APPEND, 0);
		sprintf(fwd_priv.epta_rf_stat.head_buf, "%s%s%s%s%s%s%s"
			"%s%s%s%s%s%s%s%s",
			"\n", "Index", "\t", "TimeStamp", "\t", "description1", "\t", "description2",
			"\t", "description3", "\t", "description4", "\t", "description5", "\n");

		xr_filewrite(fwd_priv.epta_rf_stat.origin_file,
				(char *)&(fwd_priv.epta_rf_stat.head_buf),
				strlen(fwd_priv.epta_rf_stat.head_buf));
		memset(fwd_priv.epta_rf_stat.head_buf, 0, FWD_EPTA_RF_HEAD_BUF_LEN);
	}
	if (fwd_priv.epta_rf_stat.origin_file) {
		int xr_file_size = 0;
		xr_filewrite(fwd_priv.epta_rf_stat.origin_file,
				(char *)&(fwd_priv.epta_rf_stat.buffer), dbg_buf_len);
		/*get file size.*/
		if (fwd_priv.epta_rf_stat.origin_file->fp->f_op->llseek != NULL) {
			mm_segment_t old_fs = get_fs();
			set_fs(KERNEL_DS);
			vfs_llseek(fwd_priv.epta_rf_stat.origin_file->fp, 0, SEEK_SET);  /* move to first */
			/* get the size, then move to end */
			xr_file_size = vfs_llseek(fwd_priv.epta_rf_stat.origin_file->fp, 0, SEEK_END);
			set_fs(old_fs);
		}
		xradio_dbg(XRADIO_DBG_MSG,  "[rf_stat]origin_file_size = %d\n", xr_file_size);

		if (xr_file_size > FWD_EPTA_RF_STAT_FILE_MAX_SIZE) {
			xr_fileclose(fwd_priv.epta_rf_stat.origin_file);
			fwd_priv.epta_rf_stat.origin_file = NULL;
			xradio_dbg(XRADIO_DBG_WARN,  "[rf_stat]origin_file_size max, close file\n");
			up(&fwd_priv.epta_rf_stat.epta_rf_stat_sema);
			goto end;;
		}
	}
	up(&fwd_priv.epta_rf_stat.epta_rf_stat_sema);
	/* end of  write to file without parse */
#endif

end:
	return 0;
}

static int fw_dbg_generic_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

int xradio_fw_dbg_init(struct xradio_common *hw_priv)
{
	if (!debugfs_initialized()) {
		xradio_dbg(XRADIO_DBG_ERROR, "debugfs is not initialized.\n");
		return 0;
	}

	fw_dbg.dbgfs_fw = debugfs_create_dir("xradio_fw_dbg", NULL);
	if (!fw_dbg.dbgfs_fw) {
		xradio_dbg(XRADIO_DBG_ERROR, "fw debugfs is not created.\n");
		goto err;
	}

	if (fw_dbg_sys_init(hw_priv, fw_dbg.dbgfs_fw)) {
		xradio_dbg(XRADIO_DBG_ERROR, "sys init error.\n");
		goto err;
	}

	if (fw_dbg_soc_init(hw_priv, fw_dbg.dbgfs_fw)) {
		xradio_dbg(XRADIO_DBG_ERROR, "soc init error.\n");
		goto err;
	}

	if (fw_dbg_lmc_init(hw_priv, fw_dbg.dbgfs_fw)) {
		xradio_dbg(XRADIO_DBG_ERROR, "lmc init error.\n");
		goto err;
	}

	if (fw_dbg_pas_init(hw_priv, fw_dbg.dbgfs_fw)) {
		xradio_dbg(XRADIO_DBG_ERROR, "pas init error.\n");
		goto err;
	}

	if (fw_dbg_phy_init(hw_priv, fw_dbg.dbgfs_fw)) {
		xradio_dbg(XRADIO_DBG_ERROR, "phy init error.\n");
		goto err;
	}

	if (fw_dbg_rf_init(hw_priv, fw_dbg.dbgfs_fw)) {
		xradio_dbg(XRADIO_DBG_ERROR, "rf init error.\n");
		goto err;
	}

	if (fw_dbg_epta_init(hw_priv, fw_dbg.dbgfs_fw)) {
		xradio_dbg(XRADIO_DBG_ERROR, "epta init error.\n");
		goto err;
	}

	return 0;

err:
	xradio_dbg(XRADIO_DBG_ERROR, "%s faild\n", __func__);
	if (fw_dbg.dbgfs_fw)
		debugfs_remove_recursive(fw_dbg.dbgfs_fw);
	fw_dbg.dbgfs_fw = NULL;
	return 0;
}

void xradio_fw_dbg_deinit(void)
{
	fw_dbg_sys_deinit();
	fw_dbg_soc_deinit();
	fw_dbg_lmc_deinit();
	fw_dbg_pas_deinit();
	fw_dbg_phy_deinit();
	fw_dbg_rf_deinit();
	fw_dbg_epta_deinit();

	if (fw_dbg.dbgfs_fw)
		debugfs_remove_recursive(fw_dbg.dbgfs_fw);

	fw_dbg.dbgfs_fw = NULL;
}

static int xradio_fw_dbg_request(struct xradio_common *hw_priv,
	struct fwd_msg *p_req, u16 req_len, u16 buf_size, u16 req_id)
{
	p_req->dbg_len = req_len;
	p_req->dbg_id = req_id;
	p_req->dbg_info = buf_size;

	SYS_WARN(wsm_fw_dbg(hw_priv, (void *)p_req, req_len));

	return p_req->dbg_info;
}

int xradio_fw_dbg_confirm(void *buf_data, void *arg)
{
	struct fwd_msg *p_cfm = (struct fwd_msg *)buf_data;
	struct fwd_msg *p_req = (struct fwd_msg *)arg;
	struct fwd_msg *p_buf = (struct fwd_msg *)arg;

	if (p_cfm->dbg_info != 0x0) {
		xradio_dbg(XRADIO_DBG_ERROR,
			"cfm status err: %04x\n", p_cfm->dbg_info);
		p_buf->dbg_info = 0x1;
		goto err;
	}

	if (p_cfm->dbg_id != p_req->dbg_id) {
		xradio_dbg(XRADIO_DBG_ERROR,
			"cfm id err: cfm(%04x), req(%04x)\n",
			p_cfm->dbg_id, p_req->dbg_id);
		p_buf->dbg_info = 0x1;
		goto err;
	}

	if (p_buf->dbg_info != p_cfm->dbg_len) {
		xradio_dbg(XRADIO_DBG_ERROR,
			"cfm length err: buf(%04x), cfm(%04x)\n",
			p_buf->dbg_info, p_cfm->dbg_len);
		p_buf->dbg_info = 0x1;
		goto err;
	}

	memcpy(p_buf, p_cfm, p_cfm->dbg_len);

err:
	return 0;
}

int xradio_fw_dbg_indicate(void *buf_data)
{
	struct fwd_msg *p_wsm = (struct fwd_msg *)buf_data;

	switch (p_wsm->dbg_id & FWD_CMD_MAJOR_ID_MASK) {
	case FWD_CMD_MAJOR_ID_SYS:
		fw_dbg_sys_indicate(p_wsm);
		break;
	case FWD_CMD_MAJOR_ID_PAS:
		fw_dbg_pas_indicate(p_wsm);
		break;
	case FWD_CMD_MAJOR_ID_EPTA:
		fw_dbg_epta_indicate(p_wsm);
		break;
	default:
		xradio_dbg(XRADIO_DBG_ALWY, "fw debug indicate: "
			"undefined major id:0x%04x \n",
			(p_wsm->dbg_id & FWD_CMD_MAJOR_ID_MASK));
		break;
	}
	return 0;
}

void xradio_fw_dbg_set_dump_flag_on_fw_exception(void)
{
	fwd_priv.sys_dump_flag |= (FWD_SYS_FW_DUMP_FLAG_FW_BIN);
}

void xradio_fw_dbg_dump_in_direct_mode(struct xradio_common *hw_priv)
{
	u32 dump_cnt = 0;

	xradio_dbg(XRADIO_DBG_ALWY,
		"firmware dump frame, command, function.===================\n");

	if (fw_dbg_sys_change_to_direct_mode(hw_priv)) {
		goto exit;
	}

	for (dump_cnt = 0x0; dump_cnt < 2; dump_cnt++) {

		xradio_dbg(XRADIO_DBG_ALWY,
				"firmware dump cnt: %d.\n", dump_cnt);

		if ((fwd_priv.sys_config.frm_trace_addr != 0x0)
			&& (fwd_priv.sys_dump_flag & FWD_SYS_FW_DUMP_FLAG_FRAME)) {
			struct fwd_sys_frame_trace frame_trace;
			if (fw_dbg_sys_read(hw_priv,
					fwd_priv.sys_config.frm_trace_addr,
					&frame_trace, sizeof(frame_trace))) {
				xradio_dbg(XRADIO_DBG_ALWY,
					"dump frame trace error.\n");
				goto exit;
			}
			fw_dbg_sys_parse_frame_trace(&frame_trace);
		}

		if ((fwd_priv.sys_config.cmd_trace_addr != 0x0)
			&& (fwd_priv.sys_dump_flag & FWD_SYS_FW_DUMP_FLAG_CMD)) {
			struct fwd_sys_cmd_trace cmd_trace;
			if (fw_dbg_sys_read(hw_priv,
					fwd_priv.sys_config.cmd_trace_addr,
					&cmd_trace, sizeof(cmd_trace))) {
				xradio_dbg(XRADIO_DBG_ALWY,
					"dump cmd trace error.\n");
				goto exit;
			}
			fw_dbg_sys_parse_cmd_trace(&cmd_trace);
		}

		if ((fwd_priv.sys_config.func_trace_addr != 0x0)
			&& (fwd_priv.sys_dump_flag & FWD_SYS_FW_DUMP_FLAG_FUNC)) {
			struct fwd_sys_func_trace func_trace;
			if (fw_dbg_sys_read(hw_priv,
					fwd_priv.sys_config.func_trace_addr,
					&func_trace, sizeof(func_trace))) {
				xradio_dbg(XRADIO_DBG_ALWY,
				"dump func trace error.\n");
				goto exit;
			}
			fw_dbg_sys_parse_func_trace(&func_trace);
		}

		msleep(1000);
	}

	fwd_priv.sys_dump_flag &= ~(FWD_SYS_FW_DUMP_FLAG_FRAME);
	fwd_priv.sys_dump_flag &= ~(FWD_SYS_FW_DUMP_FLAG_CMD);
	fwd_priv.sys_dump_flag &= ~(FWD_SYS_FW_DUMP_FLAG_FUNC);

	xradio_dbg(XRADIO_DBG_ALWY, "firmware dump fiq.====================\n");

	if ((fwd_priv.sys_config.fiq_dump_addr != 0x0)
		&& (fwd_priv.sys_dump_flag & FWD_SYS_FW_DUMP_FLAG_FIQ_DUMP)) {
		struct fwd_pas_fiq_capture fiq_cap;
		if (fw_dbg_sys_read(hw_priv, fwd_priv.sys_config.fiq_dump_addr,
					&fiq_cap, sizeof(fiq_cap))) {
			xradio_dbg(XRADIO_DBG_ALWY, "dump fiq cap error.\n");
			goto exit;
		}
		fwd_priv.sys_dump_flag &= ~(FWD_SYS_FW_DUMP_FLAG_FIQ_DUMP);
		fw_dbg_pas_parse_fiq_capture(&fiq_cap);
	}

	if ((fwd_priv.sys_config.fiq_trace_addr != 0x0)
		&& (fwd_priv.sys_dump_flag & FWD_SYS_FW_DUMP_FLAG_FIQ_TRACE)) {
		struct fwd_pas_fiq_trace fiq_trace;
		if (fw_dbg_sys_read(hw_priv, fwd_priv.sys_config.fiq_trace_addr,
					&fiq_trace, sizeof(fiq_trace))) {
			xradio_dbg(XRADIO_DBG_ALWY, "dump fiq trace error.\n");
			goto exit;
		}
		fwd_priv.sys_dump_flag &= ~(FWD_SYS_FW_DUMP_FLAG_FIQ_TRACE);
		fw_dbg_pas_parse_fiq_trace(&fiq_trace);
	}

	xradio_dbg(XRADIO_DBG_ALWY, "firmware dump code.===================\n");

	if ((fwd_priv.sys_config.fiq_dump_addr != 0x0)
			&& (fwd_priv.sys_dump_flag & FWD_SYS_FW_DUMP_FLAG_FW_BIN)) {
		if (fw_dbg_sys_process_fw_dump(hw_priv)) {
			xradio_dbg(XRADIO_DBG_ALWY, "dump fw error.\n");
			goto exit;
		}
		fwd_priv.sys_dump_flag &= ~(FWD_SYS_FW_DUMP_FLAG_FW_BIN);
	}

	if (fw_dbg_sys_change_to_queue_mode(hw_priv)) {
		goto exit;
	}

exit:
	xradio_dbg(XRADIO_DBG_ALWY, "firmware dump end.====================\n");

	return;
}

#endif
