/*
 * Copyright (c) 2017-2019, NVIDIA CORPORATION.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <nvgpu/pmu.h>
#include <nvgpu/dma.h>
#include <nvgpu/log.h>
#include <nvgpu/bug.h>
#include <nvgpu/pmuif/nvgpu_gpmu_cmdif.h>
#include <nvgpu/firmware.h>
#include <nvgpu/enabled.h>
#include <nvgpu/utils.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/boardobj.h>
#include <nvgpu/boardobjgrp.h>
#include <nvgpu/pmu/pstate.h>
#include <nvgpu/pmu/volt.h>
#include <nvgpu/pmu/clk/clk.h>
#include <nvgpu/pmu/clk/clk_vf_point.h>

/* PMU NS UCODE IMG */
#define NVGPU_PMU_NS_UCODE_IMAGE	"gpmu_ucode.bin"

#define NVGPU_PMU_UCODE_IMAGE "gpmu_ucode_image.bin"
#define NVGPU_PMU_UCODE_DESC "gpmu_ucode_desc.bin"
#define NVGPU_PMU_UCODE_SIG "pmu_sig.bin"

/* PMU F/W version */
#define APP_VERSION_TU10X	25622342U
#define APP_VERSION_GV11B	25005711U
#define APP_VERSION_GV10X	25633490U
#define APP_VERSION_GP10X	24076634U
#define APP_VERSION_GP10B	23782727U
#define APP_VERSION_GM20B	20490253U

/* PMU version specific functions */
static u32 pmu_perfmon_cntr_sz_v2(struct nvgpu_pmu *pmu)
{
	return (u32)sizeof(struct pmu_perfmon_counter_v2);
}

static void *get_perfmon_cntr_ptr_v2(struct nvgpu_pmu *pmu)
{
	return (void *)(&pmu->perfmon_counter_v2);
}

static void set_perfmon_cntr_ut_v2(struct nvgpu_pmu *pmu, u16 ut)
{
	pmu->perfmon_counter_v2.upper_threshold = ut;
}

static void set_perfmon_cntr_lt_v2(struct nvgpu_pmu *pmu, u16 lt)
{
	pmu->perfmon_counter_v2.lower_threshold = lt;
}

static void set_perfmon_cntr_valid_v2(struct nvgpu_pmu *pmu, u8 valid)
{
	pmu->perfmon_counter_v2.valid = valid;
}

static void set_perfmon_cntr_index_v2(struct nvgpu_pmu *pmu, u8 index)
{
	pmu->perfmon_counter_v2.index = index;
}

static void set_perfmon_cntr_group_id_v2(struct nvgpu_pmu *pmu, u8 gid)
{
	pmu->perfmon_counter_v2.group_id = gid;
}

static void set_pmu_cmdline_args_falctracedmabase_v4(struct nvgpu_pmu *pmu)
{
	pmu->args_v4.dma_addr.dma_base = ((u32)pmu->trace_buf.gpu_va)/0x100U;
	pmu->args_v4.dma_addr.dma_base1 = 0;
	pmu->args_v4.dma_addr.dma_offset = 0;
}

static u32 pmu_cmdline_size_v4(struct nvgpu_pmu *pmu)
{
	return (u32)sizeof(struct pmu_cmdline_args_v4);
}

static void set_pmu_cmdline_args_cpufreq_v4(struct nvgpu_pmu *pmu, u32 freq)
{
	pmu->args_v4.cpu_freq_hz = freq;
}
static void set_pmu_cmdline_args_secure_mode_v4(struct nvgpu_pmu *pmu, u8 val)
{
	pmu->args_v4.secure_mode = val;
}

static void set_pmu_cmdline_args_falctracesize_v4(
			struct nvgpu_pmu *pmu, u32 size)
{
	pmu->args_v4.falc_trace_size = size;
}
static void set_pmu_cmdline_args_falctracedmaidx_v4(
			struct nvgpu_pmu *pmu, u32 idx)
{
	pmu->args_v4.falc_trace_dma_idx = idx;
}

static u32 pmu_cmdline_size_v5(struct nvgpu_pmu *pmu)
{
	return (u32)sizeof(struct pmu_cmdline_args_v5);
}

static u32 pmu_cmdline_size_v6(struct nvgpu_pmu *pmu)
{
	return (u32)sizeof(struct pmu_cmdline_args_v6);
}

static void set_pmu_cmdline_args_cpufreq_v5(struct nvgpu_pmu *pmu, u32 freq)
{
	pmu->args_v5.cpu_freq_hz = 204000000;
}
static void set_pmu_cmdline_args_secure_mode_v5(struct nvgpu_pmu *pmu, u8 val)
{
	pmu->args_v5.secure_mode = val;
}

static void set_pmu_cmdline_args_falctracesize_v5(
			struct nvgpu_pmu *pmu, u32 size)
{
	/* set by surface describe */
}

static void set_pmu_cmdline_args_falctracedmabase_v5(struct nvgpu_pmu *pmu)
{
	struct gk20a *g = gk20a_from_pmu(pmu);

	nvgpu_pmu_surface_describe(g, &pmu->trace_buf, &pmu->args_v5.trace_buf);
}

static void config_pmu_cmdline_args_super_surface_v6(struct nvgpu_pmu *pmu)
{
	struct gk20a *g = gk20a_from_pmu(pmu);

	if (g->ops.pmu.alloc_super_surface != NULL) {
		nvgpu_pmu_surface_describe(g, &pmu->super_surface_buf,
			&pmu->args_v6.super_surface);
	}
}

static void set_pmu_cmdline_args_falctracedmaidx_v5(
			struct nvgpu_pmu *pmu, u32 idx)
{
	/* set by surface describe */
}

static u32 pmu_cmdline_size_v3(struct nvgpu_pmu *pmu)
{
	return (u32)sizeof(struct pmu_cmdline_args_v3);
}

static void set_pmu_cmdline_args_cpufreq_v3(struct nvgpu_pmu *pmu, u32 freq)
{
	pmu->args_v3.cpu_freq_hz = freq;
}
static void set_pmu_cmdline_args_secure_mode_v3(struct nvgpu_pmu *pmu, u8 val)
{
	pmu->args_v3.secure_mode = val;
}

static void set_pmu_cmdline_args_falctracesize_v3(
			struct nvgpu_pmu *pmu, u32 size)
{
	pmu->args_v3.falc_trace_size = size;
}

static void set_pmu_cmdline_args_falctracedmabase_v3(struct nvgpu_pmu *pmu)
{
	pmu->args_v3.falc_trace_dma_base = ((u32)pmu->trace_buf.gpu_va)/0x100U;
}

static void set_pmu_cmdline_args_falctracedmaidx_v3(
			struct nvgpu_pmu *pmu, u32 idx)
{
	pmu->args_v3.falc_trace_dma_idx = idx;
}

static void *get_pmu_cmdline_args_ptr_v4(struct nvgpu_pmu *pmu)
{
	return (void *)(&pmu->args_v4);
}

static void *get_pmu_cmdline_args_ptr_v3(struct nvgpu_pmu *pmu)
{
	return (void *)(&pmu->args_v3);
}

static void *get_pmu_cmdline_args_ptr_v5(struct nvgpu_pmu *pmu)
{
	return (void *)(&pmu->args_v5);
}

static u32 get_pmu_allocation_size_v3(struct nvgpu_pmu *pmu)
{
	return (u32)sizeof(struct pmu_allocation_v3);
}

static u32 get_pmu_allocation_size_v2(struct nvgpu_pmu *pmu)
{
	return (u32)sizeof(struct pmu_allocation_v2);
}

static u32 get_pmu_allocation_size_v1(struct nvgpu_pmu *pmu)
{
	return (u32)sizeof(struct pmu_allocation_v1);
}

static void set_pmu_allocation_ptr_v3(struct nvgpu_pmu *pmu,
	void **pmu_alloc_ptr, void *assign_ptr)
{
	struct pmu_allocation_v3 **pmu_a_ptr =
		(struct pmu_allocation_v3 **)pmu_alloc_ptr;

	*pmu_a_ptr = (struct pmu_allocation_v3 *)assign_ptr;
}

static void set_pmu_allocation_ptr_v2(struct nvgpu_pmu *pmu,
	void **pmu_alloc_ptr, void *assign_ptr)
{
	struct pmu_allocation_v2 **pmu_a_ptr =
		(struct pmu_allocation_v2 **)pmu_alloc_ptr;

	*pmu_a_ptr = (struct pmu_allocation_v2 *)assign_ptr;
}

static void set_pmu_allocation_ptr_v1(struct nvgpu_pmu *pmu,
	void **pmu_alloc_ptr, void *assign_ptr)
{
	struct pmu_allocation_v1 **pmu_a_ptr =
		(struct pmu_allocation_v1 **)pmu_alloc_ptr;

	*pmu_a_ptr = (struct pmu_allocation_v1 *)assign_ptr;
}

static void pmu_allocation_set_dmem_size_v3(struct nvgpu_pmu *pmu,
	void *pmu_alloc_ptr, u16 size)
{
	struct pmu_allocation_v3 *pmu_a_ptr =
		(struct pmu_allocation_v3 *)pmu_alloc_ptr;

	pmu_a_ptr->alloc.dmem.size = size;
}

static void pmu_allocation_set_dmem_size_v2(struct nvgpu_pmu *pmu,
	void *pmu_alloc_ptr, u16 size)
{
	struct pmu_allocation_v2 *pmu_a_ptr =
		(struct pmu_allocation_v2 *)pmu_alloc_ptr;

	pmu_a_ptr->alloc.dmem.size = size;
}

static void pmu_allocation_set_dmem_size_v1(struct nvgpu_pmu *pmu,
	void *pmu_alloc_ptr, u16 size)
{
	struct pmu_allocation_v1 *pmu_a_ptr =
		(struct pmu_allocation_v1 *)pmu_alloc_ptr;

	pmu_a_ptr->alloc.dmem.size = size;
}

static u16 pmu_allocation_get_dmem_size_v3(struct nvgpu_pmu *pmu,
	void *pmu_alloc_ptr)
{
	struct pmu_allocation_v3 *pmu_a_ptr =
		(struct pmu_allocation_v3 *)pmu_alloc_ptr;

	return pmu_a_ptr->alloc.dmem.size;
}

static u16 pmu_allocation_get_dmem_size_v2(struct nvgpu_pmu *pmu,
	void *pmu_alloc_ptr)
{
	struct pmu_allocation_v2 *pmu_a_ptr =
		(struct pmu_allocation_v2 *)pmu_alloc_ptr;

	return pmu_a_ptr->alloc.dmem.size;
}

static u16 pmu_allocation_get_dmem_size_v1(struct nvgpu_pmu *pmu,
	void *pmu_alloc_ptr)
{
	struct pmu_allocation_v1 *pmu_a_ptr =
		(struct pmu_allocation_v1 *)pmu_alloc_ptr;

	return pmu_a_ptr->alloc.dmem.size;
}

static u32 pmu_allocation_get_dmem_offset_v3(struct nvgpu_pmu *pmu,
	void *pmu_alloc_ptr)
{
	struct pmu_allocation_v3 *pmu_a_ptr =
		(struct pmu_allocation_v3 *)pmu_alloc_ptr;

	return pmu_a_ptr->alloc.dmem.offset;
}

static u32 pmu_allocation_get_dmem_offset_v2(struct nvgpu_pmu *pmu,
	void *pmu_alloc_ptr)
{
	struct pmu_allocation_v2 *pmu_a_ptr =
		(struct pmu_allocation_v2 *)pmu_alloc_ptr;

	return pmu_a_ptr->alloc.dmem.offset;
}

static u32 pmu_allocation_get_dmem_offset_v1(struct nvgpu_pmu *pmu,
	void *pmu_alloc_ptr)
{
	struct pmu_allocation_v1 *pmu_a_ptr =
		(struct pmu_allocation_v1 *)pmu_alloc_ptr;

	return pmu_a_ptr->alloc.dmem.offset;
}

static u32 *pmu_allocation_get_dmem_offset_addr_v3(struct nvgpu_pmu *pmu,
	void *pmu_alloc_ptr)
{
	struct pmu_allocation_v3 *pmu_a_ptr =
		(struct pmu_allocation_v3 *)pmu_alloc_ptr;

	return &pmu_a_ptr->alloc.dmem.offset;
}

static void *pmu_allocation_get_fb_addr_v3(
				struct nvgpu_pmu *pmu, void *pmu_alloc_ptr)
{
	struct pmu_allocation_v3 *pmu_a_ptr =
			(struct pmu_allocation_v3 *)pmu_alloc_ptr;

	return (void *)&pmu_a_ptr->alloc.fb;
}

static u32 pmu_allocation_get_fb_size_v3(
				struct nvgpu_pmu *pmu, void *pmu_alloc_ptr)
{
	struct pmu_allocation_v3 *pmu_a_ptr =
			(struct pmu_allocation_v3 *)pmu_alloc_ptr;

	return (u32)sizeof(pmu_a_ptr->alloc.fb);
}

static u32 *pmu_allocation_get_dmem_offset_addr_v2(struct nvgpu_pmu *pmu,
	void *pmu_alloc_ptr)
{
	struct pmu_allocation_v2 *pmu_a_ptr =
		(struct pmu_allocation_v2 *)pmu_alloc_ptr;

	return &pmu_a_ptr->alloc.dmem.offset;
}

static u32 *pmu_allocation_get_dmem_offset_addr_v1(struct nvgpu_pmu *pmu,
	void *pmu_alloc_ptr)
{
	struct pmu_allocation_v1 *pmu_a_ptr =
		(struct pmu_allocation_v1 *)pmu_alloc_ptr;

	return &pmu_a_ptr->alloc.dmem.offset;
}

static void pmu_allocation_set_dmem_offset_v3(struct nvgpu_pmu *pmu,
	void *pmu_alloc_ptr, u32 offset)
{
	struct pmu_allocation_v3 *pmu_a_ptr =
		(struct pmu_allocation_v3 *)pmu_alloc_ptr;

	pmu_a_ptr->alloc.dmem.offset = offset;
}

static void pmu_allocation_set_dmem_offset_v2(struct nvgpu_pmu *pmu,
	void *pmu_alloc_ptr, u32 offset)
{
	struct pmu_allocation_v2 *pmu_a_ptr =
		(struct pmu_allocation_v2 *)pmu_alloc_ptr;

	pmu_a_ptr->alloc.dmem.offset = offset;
}

static void pmu_allocation_set_dmem_offset_v1(struct nvgpu_pmu *pmu,
	void *pmu_alloc_ptr, u32 offset)
{
	struct pmu_allocation_v1 *pmu_a_ptr =
		(struct pmu_allocation_v1 *)pmu_alloc_ptr;

	pmu_a_ptr->alloc.dmem.offset = offset;
}

static void *get_pmu_msg_pmu_init_msg_ptr_v5(struct pmu_init_msg *init)
{
	return (void *)(&(init->pmu_init_v5));
}

static void *get_pmu_msg_pmu_init_msg_ptr_v4(struct pmu_init_msg *init)
{
	return (void *)(&(init->pmu_init_v4));
}

static void *get_pmu_msg_pmu_init_msg_ptr_v3(struct pmu_init_msg *init)
{
	return (void *)(&(init->pmu_init_v3));
}

static u16 get_pmu_init_msg_pmu_sw_mg_off_v5(union pmu_init_msg_pmu *init_msg)
{
	struct pmu_init_msg_pmu_v5 *init =
		(struct pmu_init_msg_pmu_v5 *)(&init_msg->v5);

	return init->sw_managed_area_offset;
}

static u16 get_pmu_init_msg_pmu_sw_mg_off_v4(union pmu_init_msg_pmu *init_msg)
{
	struct pmu_init_msg_pmu_v4 *init =
		(struct pmu_init_msg_pmu_v4 *)(&init_msg->v4);

	return init->sw_managed_area_offset;
}

static u16 get_pmu_init_msg_pmu_sw_mg_off_v3(union pmu_init_msg_pmu *init_msg)
{
	struct pmu_init_msg_pmu_v3 *init =
		(struct pmu_init_msg_pmu_v3 *)(&init_msg->v3);

	return init->sw_managed_area_offset;
}

static u16 get_pmu_init_msg_pmu_sw_mg_size_v5(union pmu_init_msg_pmu *init_msg)
{
	struct pmu_init_msg_pmu_v5 *init =
		(struct pmu_init_msg_pmu_v5 *)(&init_msg->v5);

	return init->sw_managed_area_size;
}

static u16 get_pmu_init_msg_pmu_sw_mg_size_v4(union pmu_init_msg_pmu *init_msg)
{
	struct pmu_init_msg_pmu_v4 *init =
		(struct pmu_init_msg_pmu_v4 *)(&init_msg->v4);

	return init->sw_managed_area_size;
}

static u16 get_pmu_init_msg_pmu_sw_mg_size_v3(union pmu_init_msg_pmu *init_msg)
{
	struct pmu_init_msg_pmu_v3 *init =
		(struct pmu_init_msg_pmu_v3 *)(&init_msg->v3);

	return init->sw_managed_area_size;
}

static void *get_pmu_msg_pmu_init_msg_ptr_v1(struct pmu_init_msg *init)
{
	return (void *)(&(init->pmu_init_v1));
}

static u16 get_pmu_init_msg_pmu_sw_mg_off_v1(union pmu_init_msg_pmu *init_msg)
{
	struct pmu_init_msg_pmu_v1 *init =
		(struct pmu_init_msg_pmu_v1 *)(&init_msg->v1);

	return init->sw_managed_area_offset;
}

static u16 get_pmu_init_msg_pmu_sw_mg_size_v1(union pmu_init_msg_pmu *init_msg)
{
	struct pmu_init_msg_pmu_v1 *init =
		(struct pmu_init_msg_pmu_v1 *)(&init_msg->v1);

	return init->sw_managed_area_size;
}

static u32 get_pmu_perfmon_cmd_start_size_v3(void)
{
	return (u32)sizeof(struct pmu_perfmon_cmd_start_v3);
}

static u32 get_pmu_perfmon_cmd_start_size_v2(void)
{
	return (u32)sizeof(struct pmu_perfmon_cmd_start_v2);
}

static u32 get_pmu_perfmon_cmd_start_size_v1(void)
{
	return (u32)sizeof(struct pmu_perfmon_cmd_start_v1);
}

static int get_perfmon_cmd_start_offsetofvar_v3(
	enum pmu_perfmon_cmd_start_fields field, u32 *offset)
{
	int status = 0;

	switch (field) {
	case COUNTER_ALLOC:
		*offset = (u32)offsetof(struct pmu_perfmon_cmd_start_v3,
				counter_alloc);
		break;

	default:
		status = -EINVAL;
	}

	return status;
}

static int get_perfmon_cmd_start_offsetofvar_v2(
	enum pmu_perfmon_cmd_start_fields field, u32 *offset)
{
	int status = 0;

	switch (field) {
	case COUNTER_ALLOC:
		*offset = (u32)offsetof(struct pmu_perfmon_cmd_start_v2,
				counter_alloc);
		break;

	default:
		status = -EINVAL;
	}

	return status;
}

static int get_perfmon_cmd_start_offsetofvar_v1(
	enum pmu_perfmon_cmd_start_fields field, u32 *offset)
{
	int status = 0;

	switch (field) {
	case COUNTER_ALLOC:
		*offset = (u32)offsetof(struct pmu_perfmon_cmd_start_v1,
				counter_alloc);
		break;

	default:
		status = -EINVAL;
	}

	return status;
}

static u32 get_pmu_perfmon_cmd_init_size_v3(void)
{
	return (u32)sizeof(struct pmu_perfmon_cmd_init_v3);
}

static u32 get_pmu_perfmon_cmd_init_size_v2(void)
{
	return (u32)sizeof(struct pmu_perfmon_cmd_init_v2);
}

static u32 get_pmu_perfmon_cmd_init_size_v1(void)
{
	return (u32)sizeof(struct pmu_perfmon_cmd_init_v1);
}

static int get_perfmon_cmd_init_offsetofvar_v3(
	enum pmu_perfmon_cmd_start_fields field, u32 *offset)
{
	int status = 0;

	switch (field) {
	case COUNTER_ALLOC:
		*offset = (u32)offsetof(struct pmu_perfmon_cmd_init_v3,
				counter_alloc);
		break;

	default:
		status = -EINVAL;
	}

	return status;
}

static int get_perfmon_cmd_init_offsetofvar_v2(
	enum pmu_perfmon_cmd_start_fields field, u32 *offset)
{
	int status = 0;

	switch (field) {
	case COUNTER_ALLOC:
		*offset = (u32)offsetof(struct pmu_perfmon_cmd_init_v2,
				counter_alloc);
		break;

	default:
		status = -EINVAL;
	}

	return status;
}

static int get_perfmon_cmd_init_offsetofvar_v1(
	enum pmu_perfmon_cmd_start_fields field, u32 *offset)
{
	int status = 0;

	switch (field) {
	case COUNTER_ALLOC:
		*offset = (u32)offsetof(struct pmu_perfmon_cmd_init_v1,
				counter_alloc);
		break;

	default:
		status = -EINVAL;
	}

	return status;
}

static void perfmon_start_set_cmd_type_v3(struct pmu_perfmon_cmd *pc, u8 value)
{
	struct pmu_perfmon_cmd_start_v3 *start = &pc->start_v3;

	start->cmd_type = value;
}

static void perfmon_start_set_cmd_type_v2(struct pmu_perfmon_cmd *pc, u8 value)
{
	struct pmu_perfmon_cmd_start_v2 *start = &pc->start_v2;

	start->cmd_type = value;
}

static void perfmon_start_set_cmd_type_v1(struct pmu_perfmon_cmd *pc, u8 value)
{
	struct pmu_perfmon_cmd_start_v1 *start = &pc->start_v1;

	start->cmd_type = value;
}

static void perfmon_start_set_group_id_v3(struct pmu_perfmon_cmd *pc, u8 value)
{
	struct pmu_perfmon_cmd_start_v3 *start = &pc->start_v3;

	start->group_id = value;
}

static void perfmon_start_set_group_id_v2(struct pmu_perfmon_cmd *pc, u8 value)
{
	struct pmu_perfmon_cmd_start_v2 *start = &pc->start_v2;

	start->group_id = value;
}

static void perfmon_start_set_group_id_v1(struct pmu_perfmon_cmd *pc, u8 value)
{
	struct pmu_perfmon_cmd_start_v1 *start = &pc->start_v1;

	start->group_id = value;
}

static void perfmon_start_set_state_id_v3(struct pmu_perfmon_cmd *pc, u8 value)
{
	struct pmu_perfmon_cmd_start_v3 *start = &pc->start_v3;

	start->state_id = value;
}

static void perfmon_start_set_state_id_v2(struct pmu_perfmon_cmd *pc, u8 value)
{
	struct pmu_perfmon_cmd_start_v2 *start = &pc->start_v2;

	start->state_id = value;
}

static void perfmon_start_set_state_id_v1(struct pmu_perfmon_cmd *pc, u8 value)
{
	struct pmu_perfmon_cmd_start_v1 *start = &pc->start_v1;

	start->state_id = value;
}

static void perfmon_start_set_flags_v3(struct pmu_perfmon_cmd *pc, u8 value)
{
	struct pmu_perfmon_cmd_start_v3 *start = &pc->start_v3;

	start->flags = value;
}

static void perfmon_start_set_flags_v2(struct pmu_perfmon_cmd *pc, u8 value)
{
	struct pmu_perfmon_cmd_start_v2 *start = &pc->start_v2;

	start->flags = value;
}

static void perfmon_start_set_flags_v1(struct pmu_perfmon_cmd *pc, u8 value)
{
	struct pmu_perfmon_cmd_start_v1 *start = &pc->start_v1;

	start->flags = value;
}

static u8 perfmon_start_get_flags_v3(struct pmu_perfmon_cmd *pc)
{
	struct pmu_perfmon_cmd_start_v3 *start = &pc->start_v3;

	return start->flags;
}

static u8 perfmon_start_get_flags_v2(struct pmu_perfmon_cmd *pc)
{
	struct pmu_perfmon_cmd_start_v2 *start = &pc->start_v2;

	return start->flags;
}

static u8 perfmon_start_get_flags_v1(struct pmu_perfmon_cmd *pc)
{
	struct pmu_perfmon_cmd_start_v1 *start = &pc->start_v1;

	return start->flags;
}

static void perfmon_cmd_init_set_sample_buffer_v3(struct pmu_perfmon_cmd *pc,
	u16 value)
{
	struct pmu_perfmon_cmd_init_v3 *init = &pc->init_v3;

	init->sample_buffer = value;
}

static void perfmon_cmd_init_set_sample_buffer_v2(struct pmu_perfmon_cmd *pc,
	u16 value)
{
	struct pmu_perfmon_cmd_init_v2 *init = &pc->init_v2;

	init->sample_buffer = value;
}


static void perfmon_cmd_init_set_sample_buffer_v1(struct pmu_perfmon_cmd *pc,
	u16 value)
{
	struct pmu_perfmon_cmd_init_v1 *init = &pc->init_v1;

	init->sample_buffer = value;
}

static void perfmon_cmd_init_set_dec_cnt_v3(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v3 *init = &pc->init_v3;

	init->to_decrease_count = value;
}

static void perfmon_cmd_init_set_dec_cnt_v2(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v2 *init = &pc->init_v2;

	init->to_decrease_count = value;
}

static void perfmon_cmd_init_set_dec_cnt_v1(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v1 *init = &pc->init_v1;

	init->to_decrease_count = value;
}

static void perfmon_cmd_init_set_base_cnt_id_v3(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v3 *init = &pc->init_v3;

	init->base_counter_id = value;
}

static void perfmon_cmd_init_set_base_cnt_id_v2(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v2 *init = &pc->init_v2;

	init->base_counter_id = value;
}

static void perfmon_cmd_init_set_base_cnt_id_v1(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v1 *init = &pc->init_v1;

	init->base_counter_id = value;
}

static void perfmon_cmd_init_set_samp_period_us_v3(struct pmu_perfmon_cmd *pc,
	u32 value)
{
	struct pmu_perfmon_cmd_init_v3 *init = &pc->init_v3;

	init->sample_period_us = value;
}

static void perfmon_cmd_init_set_samp_period_us_v2(struct pmu_perfmon_cmd *pc,
	u32 value)
{
	struct pmu_perfmon_cmd_init_v2 *init = &pc->init_v2;

	init->sample_period_us = value;
}

static void perfmon_cmd_init_set_samp_period_us_v1(struct pmu_perfmon_cmd *pc,
	u32 value)
{
	struct pmu_perfmon_cmd_init_v1 *init = &pc->init_v1;

	init->sample_period_us = value;
}

static void perfmon_cmd_init_set_num_cnt_v3(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v3 *init = &pc->init_v3;

	init->num_counters = value;
}

static void perfmon_cmd_init_set_num_cnt_v2(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v2 *init = &pc->init_v2;

	init->num_counters = value;
}

static void perfmon_cmd_init_set_num_cnt_v1(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v1 *init = &pc->init_v1;

	init->num_counters = value;
}

static void perfmon_cmd_init_set_mov_avg_v3(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v3 *init = &pc->init_v3;

	init->samples_in_moving_avg = value;
}

static void perfmon_cmd_init_set_mov_avg_v2(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v2 *init = &pc->init_v2;

	init->samples_in_moving_avg = value;
}

static void perfmon_cmd_init_set_mov_avg_v1(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v1 *init = &pc->init_v1;

	init->samples_in_moving_avg = value;
}

static void get_pmu_init_msg_pmu_queue_params_v1(
	u32 id, void *pmu_init_msg, u32 *index, u32 *offset, u32 *size)
{
	struct pmu_init_msg_pmu_v1 *init =
		(struct pmu_init_msg_pmu_v1 *)pmu_init_msg;

	*index = init->queue_info[id].index;
	*offset = init->queue_info[id].offset;
	*size = init->queue_info[id].size;
}

static void get_pmu_init_msg_pmu_queue_params_v4(
	u32 id, void *pmu_init_msg, u32 *index, u32 *offset, u32 *size)
{
	struct pmu_init_msg_pmu_v4 *init = pmu_init_msg;
	u32 current_ptr = 0;
	u32 i;

	if (id == PMU_COMMAND_QUEUE_HPQ) {
		id = PMU_QUEUE_HPQ_IDX_FOR_V3;
	} else if (id == PMU_COMMAND_QUEUE_LPQ) {
		id = PMU_QUEUE_LPQ_IDX_FOR_V3;
	} else if (id == PMU_MESSAGE_QUEUE) {
		id = PMU_QUEUE_MSG_IDX_FOR_V3;
	} else {
		return;
	}

	*index = init->queue_index[id];
	*size = init->queue_size[id];
	if (id != 0U) {
		for (i = 0 ; i < id; i++) {
			current_ptr += init->queue_size[i];
		}
	}
	*offset = init->queue_offset + current_ptr;
}

static void get_pmu_init_msg_pmu_queue_params_v5(
	u32 id, void *pmu_init_msg, u32 *index, u32 *offset, u32 *size)
{
	struct pmu_init_msg_pmu_v5 *init = pmu_init_msg;
	u32 current_ptr = 0;
	u32 i;

	if (id == PMU_COMMAND_QUEUE_HPQ) {
		id = PMU_QUEUE_HPQ_IDX_FOR_V3;
	} else if (id == PMU_COMMAND_QUEUE_LPQ) {
		id = PMU_QUEUE_LPQ_IDX_FOR_V3;
	} else if (id == PMU_MESSAGE_QUEUE) {
		id = PMU_QUEUE_MSG_IDX_FOR_V5;
	} else {
		return;
	}

	*index = init->queue_index[id];
	*size = init->queue_size[id];
	if (id != 0U) {
		for (i = 0 ; i < id; i++) {
			current_ptr += init->queue_size[i];
		}
	}
	*offset = init->queue_offset + current_ptr;
}

static void get_pmu_init_msg_pmu_queue_params_v3(
	u32 id, void *pmu_init_msg, u32 *index, u32 *offset, u32 *size)
{
	struct pmu_init_msg_pmu_v3 *init =
		(struct pmu_init_msg_pmu_v3 *)pmu_init_msg;
	u32 current_ptr = 0;
	u32 i;

	if (id == PMU_COMMAND_QUEUE_HPQ) {
		id = PMU_QUEUE_HPQ_IDX_FOR_V3;
	} else if (id == PMU_COMMAND_QUEUE_LPQ) {
		id = PMU_QUEUE_LPQ_IDX_FOR_V3;
	} else if (id == PMU_MESSAGE_QUEUE) {
		id = PMU_QUEUE_MSG_IDX_FOR_V3;
	} else {
		return;
	}
	*index = init->queue_index[id];
	*size = init->queue_size[id];
	if (id != 0U) {
		for (i = 0 ; i < id; i++) {
			current_ptr += init->queue_size[i];
		}
	}
	*offset = init->queue_offset + current_ptr;
}

static void *get_pmu_sequence_in_alloc_ptr_v3(struct pmu_sequence *seq)
{
	return (void *)(&seq->in_v3);
}

static void *get_pmu_sequence_in_alloc_ptr_v1(struct pmu_sequence *seq)
{
	return (void *)(&seq->in_v1);
}

static void *get_pmu_sequence_out_alloc_ptr_v3(struct pmu_sequence *seq)
{
	return (void *)(&seq->out_v3);
}

static void *get_pmu_sequence_out_alloc_ptr_v1(struct pmu_sequence *seq)
{
	return (void *)(&seq->out_v1);
}

static u8 pg_cmd_eng_buf_load_size_v0(struct pmu_pg_cmd *pg)
{
	return (u32)sizeof(pg->eng_buf_load_v0);
}

static u8 pg_cmd_eng_buf_load_size_v1(struct pmu_pg_cmd *pg)
{
	return (u32)sizeof(pg->eng_buf_load_v1);
}

static u8 pg_cmd_eng_buf_load_size_v2(struct pmu_pg_cmd *pg)
{
	return (u32)sizeof(pg->eng_buf_load_v2);
}

static void pg_cmd_eng_buf_load_set_cmd_type_v0(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v0.cmd_type = value;
}

static void pg_cmd_eng_buf_load_set_cmd_type_v1(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v1.cmd_type = value;
}

static void pg_cmd_eng_buf_load_set_cmd_type_v2(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v2.cmd_type = value;
}

static void pg_cmd_eng_buf_load_set_engine_id_v0(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v0.engine_id = value;
}
static void pg_cmd_eng_buf_load_set_engine_id_v1(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v1.engine_id = value;
}
static void pg_cmd_eng_buf_load_set_engine_id_v2(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v2.engine_id = value;
}
static void pg_cmd_eng_buf_load_set_buf_idx_v0(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v0.buf_idx = value;
}
static void pg_cmd_eng_buf_load_set_buf_idx_v1(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v1.buf_idx = value;
}
static void pg_cmd_eng_buf_load_set_buf_idx_v2(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v2.buf_idx = value;
}

static void pg_cmd_eng_buf_load_set_pad_v0(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v0.pad = value;
}
static void pg_cmd_eng_buf_load_set_pad_v1(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v1.pad = value;
}
static void pg_cmd_eng_buf_load_set_pad_v2(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v2.pad = value;
}

static void pg_cmd_eng_buf_load_set_buf_size_v0(struct pmu_pg_cmd *pg,
	u16 value)
{
	pg->eng_buf_load_v0.buf_size = value;
}
static void pg_cmd_eng_buf_load_set_buf_size_v1(struct pmu_pg_cmd *pg,
	u16 value)
{
	pg->eng_buf_load_v1.dma_desc.dma_size = value;
}
static void pg_cmd_eng_buf_load_set_buf_size_v2(struct pmu_pg_cmd *pg,
	u16 value)
{
	pg->eng_buf_load_v2.dma_desc.params = value;
}

static void pg_cmd_eng_buf_load_set_dma_base_v0(struct pmu_pg_cmd *pg,
	u32 value)
{
	pg->eng_buf_load_v0.dma_base = (value >> 8);
}
static void pg_cmd_eng_buf_load_set_dma_base_v1(struct pmu_pg_cmd *pg,
	u32 value)
{
	pg->eng_buf_load_v1.dma_desc.dma_addr.lo |= u64_lo32(value);
	pg->eng_buf_load_v1.dma_desc.dma_addr.hi |= u64_hi32(value);
}
static void pg_cmd_eng_buf_load_set_dma_base_v2(struct pmu_pg_cmd *pg,
	u32 value)
{
	pg->eng_buf_load_v2.dma_desc.address.lo = u64_lo32(value);
	pg->eng_buf_load_v2.dma_desc.address.hi = u64_lo32(value);
}

static void pg_cmd_eng_buf_load_set_dma_offset_v0(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v0.dma_offset = value;
}
static void pg_cmd_eng_buf_load_set_dma_offset_v1(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v1.dma_desc.dma_addr.lo |= value;
}
static void pg_cmd_eng_buf_load_set_dma_offset_v2(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v2.dma_desc.address.lo |= u64_lo32(value);
	pg->eng_buf_load_v2.dma_desc.address.hi |= u64_lo32(value);
}

static void pg_cmd_eng_buf_load_set_dma_idx_v0(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v0.dma_idx = value;
}

static void pg_cmd_eng_buf_load_set_dma_idx_v1(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v1.dma_desc.dma_idx = value;
}

static void pg_cmd_eng_buf_load_set_dma_idx_v2(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v2.dma_desc.params |= (U32(value) << U32(24));
}

static int init_pmu_fw_ver_ops(struct nvgpu_pmu *pmu, u32 app_version)
{
	struct gk20a *g = gk20a_from_pmu(pmu);
	struct pmu_v *pv = &g->ops.pmu_ver;
	int err = 0;

	nvgpu_log_fn(g, " ");

	switch (app_version) {
	case APP_VERSION_GP10B:
		g->ops.pmu_ver.pg_cmd_eng_buf_load_size =
				pg_cmd_eng_buf_load_size_v1;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_cmd_type =
				pg_cmd_eng_buf_load_set_cmd_type_v1;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_engine_id =
				pg_cmd_eng_buf_load_set_engine_id_v1;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_buf_idx =
				pg_cmd_eng_buf_load_set_buf_idx_v1;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_pad =
				pg_cmd_eng_buf_load_set_pad_v1;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_buf_size =
				pg_cmd_eng_buf_load_set_buf_size_v1;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_base =
				pg_cmd_eng_buf_load_set_dma_base_v1;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_offset =
				pg_cmd_eng_buf_load_set_dma_offset_v1;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_idx =
				pg_cmd_eng_buf_load_set_dma_idx_v1;
		g->ops.pmu_ver.get_perfmon_cntr_ptr = get_perfmon_cntr_ptr_v2;
		g->ops.pmu_ver.set_perfmon_cntr_ut = set_perfmon_cntr_ut_v2;
		g->ops.pmu_ver.set_perfmon_cntr_lt = set_perfmon_cntr_lt_v2;
		g->ops.pmu_ver.set_perfmon_cntr_valid =
			set_perfmon_cntr_valid_v2;
		g->ops.pmu_ver.set_perfmon_cntr_index =
			set_perfmon_cntr_index_v2;
		g->ops.pmu_ver.set_perfmon_cntr_group_id =
			set_perfmon_cntr_group_id_v2;
		g->ops.pmu_ver.get_perfmon_cntr_sz = pmu_perfmon_cntr_sz_v2;
		g->pmu_ver_cmd_id_zbc_table_update = 16;
		nvgpu_set_enabled(g, NVGPU_PMU_ZBC_SAVE, true);
		g->ops.pmu_ver.get_pmu_cmdline_args_size =
			pmu_cmdline_size_v4;
		g->ops.pmu_ver.set_pmu_cmdline_args_cpu_freq =
			set_pmu_cmdline_args_cpufreq_v4;
		g->ops.pmu_ver.set_pmu_cmdline_args_secure_mode =
			set_pmu_cmdline_args_secure_mode_v4;
		g->ops.pmu_ver.set_pmu_cmdline_args_trace_size =
			set_pmu_cmdline_args_falctracesize_v4;
		g->ops.pmu_ver.set_pmu_cmdline_args_trace_dma_base =
			set_pmu_cmdline_args_falctracedmabase_v4;
		g->ops.pmu_ver.set_pmu_cmdline_args_trace_dma_idx =
			set_pmu_cmdline_args_falctracedmaidx_v4;
		g->ops.pmu_ver.get_pmu_cmdline_args_ptr =
			get_pmu_cmdline_args_ptr_v4;
		g->ops.pmu_ver.get_pmu_allocation_struct_size =
			get_pmu_allocation_size_v2;
		g->ops.pmu_ver.set_pmu_allocation_ptr =
			set_pmu_allocation_ptr_v2;
		g->ops.pmu_ver.pmu_allocation_set_dmem_size =
			pmu_allocation_set_dmem_size_v2;
		g->ops.pmu_ver.pmu_allocation_get_dmem_size =
			pmu_allocation_get_dmem_size_v2;
		g->ops.pmu_ver.pmu_allocation_get_dmem_offset =
			pmu_allocation_get_dmem_offset_v2;
		g->ops.pmu_ver.pmu_allocation_get_dmem_offset_addr =
			pmu_allocation_get_dmem_offset_addr_v2;
		g->ops.pmu_ver.pmu_allocation_set_dmem_offset =
			pmu_allocation_set_dmem_offset_v2;
		g->ops.pmu_ver.get_pmu_init_msg_pmu_queue_params =
			get_pmu_init_msg_pmu_queue_params_v1;
		g->ops.pmu_ver.get_pmu_msg_pmu_init_msg_ptr =
			get_pmu_msg_pmu_init_msg_ptr_v1;
		g->ops.pmu_ver.get_pmu_init_msg_pmu_sw_mg_off =
			get_pmu_init_msg_pmu_sw_mg_off_v1;
		g->ops.pmu_ver.get_pmu_init_msg_pmu_sw_mg_size =
			get_pmu_init_msg_pmu_sw_mg_size_v1;
		g->ops.pmu_ver.get_pmu_perfmon_cmd_start_size =
			get_pmu_perfmon_cmd_start_size_v2;
		g->ops.pmu_ver.get_perfmon_cmd_start_offsetofvar =
			get_perfmon_cmd_start_offsetofvar_v2;
		g->ops.pmu_ver.perfmon_start_set_cmd_type =
			perfmon_start_set_cmd_type_v2;
		g->ops.pmu_ver.perfmon_start_set_group_id =
			perfmon_start_set_group_id_v2;
		g->ops.pmu_ver.perfmon_start_set_state_id =
			perfmon_start_set_state_id_v2;
		g->ops.pmu_ver.perfmon_start_set_flags =
			perfmon_start_set_flags_v2;
		g->ops.pmu_ver.perfmon_start_get_flags =
			perfmon_start_get_flags_v2;
		g->ops.pmu_ver.get_pmu_perfmon_cmd_init_size =
			get_pmu_perfmon_cmd_init_size_v2;
		g->ops.pmu_ver.get_perfmon_cmd_init_offsetofvar =
			get_perfmon_cmd_init_offsetofvar_v2;
		g->ops.pmu_ver.perfmon_cmd_init_set_sample_buffer =
			perfmon_cmd_init_set_sample_buffer_v2;
		g->ops.pmu_ver.perfmon_cmd_init_set_dec_cnt =
			perfmon_cmd_init_set_dec_cnt_v2;
		g->ops.pmu_ver.perfmon_cmd_init_set_base_cnt_id =
			perfmon_cmd_init_set_base_cnt_id_v2;
		g->ops.pmu_ver.perfmon_cmd_init_set_samp_period_us =
			perfmon_cmd_init_set_samp_period_us_v2;
		g->ops.pmu_ver.perfmon_cmd_init_set_num_cnt =
			perfmon_cmd_init_set_num_cnt_v2;
		g->ops.pmu_ver.perfmon_cmd_init_set_mov_avg =
			perfmon_cmd_init_set_mov_avg_v2;
		g->ops.pmu_ver.get_pmu_seq_in_a_ptr =
			get_pmu_sequence_in_alloc_ptr_v1;
		g->ops.pmu_ver.get_pmu_seq_out_a_ptr =
			get_pmu_sequence_out_alloc_ptr_v1;
		break;
	case APP_VERSION_GV11B:
	case APP_VERSION_GV10X:
	case APP_VERSION_TU10X:
		g->ops.pmu_ver.pg_cmd_eng_buf_load_size =
				pg_cmd_eng_buf_load_size_v2;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_cmd_type =
				pg_cmd_eng_buf_load_set_cmd_type_v2;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_engine_id =
				pg_cmd_eng_buf_load_set_engine_id_v2;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_buf_idx =
				pg_cmd_eng_buf_load_set_buf_idx_v2;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_pad =
				pg_cmd_eng_buf_load_set_pad_v2;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_buf_size =
				pg_cmd_eng_buf_load_set_buf_size_v2;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_base =
				pg_cmd_eng_buf_load_set_dma_base_v2;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_offset =
				pg_cmd_eng_buf_load_set_dma_offset_v2;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_idx =
				pg_cmd_eng_buf_load_set_dma_idx_v2;
		g->ops.pmu_ver.get_perfmon_cntr_ptr = get_perfmon_cntr_ptr_v2;
		g->ops.pmu_ver.set_perfmon_cntr_ut = set_perfmon_cntr_ut_v2;
		g->ops.pmu_ver.set_perfmon_cntr_lt = set_perfmon_cntr_lt_v2;
		g->ops.pmu_ver.set_perfmon_cntr_valid =
			set_perfmon_cntr_valid_v2;
		g->ops.pmu_ver.set_perfmon_cntr_index =
			set_perfmon_cntr_index_v2;
		g->ops.pmu_ver.set_perfmon_cntr_group_id =
			set_perfmon_cntr_group_id_v2;
		g->ops.pmu_ver.get_perfmon_cntr_sz = pmu_perfmon_cntr_sz_v2;
		g->pmu_ver_cmd_id_zbc_table_update = 16;
		nvgpu_set_enabled(g, NVGPU_PMU_ZBC_SAVE, false);
		g->ops.pmu_ver.get_pmu_cmdline_args_size =
			pmu_cmdline_size_v6;
		g->ops.pmu_ver.set_pmu_cmdline_args_cpu_freq =
			set_pmu_cmdline_args_cpufreq_v5;
		g->ops.pmu_ver.set_pmu_cmdline_args_secure_mode =
			set_pmu_cmdline_args_secure_mode_v5;
		g->ops.pmu_ver.set_pmu_cmdline_args_trace_size =
			set_pmu_cmdline_args_falctracesize_v5;
		g->ops.pmu_ver.set_pmu_cmdline_args_trace_dma_base =
			set_pmu_cmdline_args_falctracedmabase_v5;
		g->ops.pmu_ver.set_pmu_cmdline_args_trace_dma_idx =
			set_pmu_cmdline_args_falctracedmaidx_v5;
		g->ops.pmu_ver.config_pmu_cmdline_args_super_surface =
			config_pmu_cmdline_args_super_surface_v6;
		g->ops.pmu_ver.get_pmu_cmdline_args_ptr =
			get_pmu_cmdline_args_ptr_v5;
		g->ops.pmu_ver.get_pmu_allocation_struct_size =
			get_pmu_allocation_size_v3;
		g->ops.pmu_ver.set_pmu_allocation_ptr =
			set_pmu_allocation_ptr_v3;
		g->ops.pmu_ver.pmu_allocation_set_dmem_size =
			pmu_allocation_set_dmem_size_v3;
		g->ops.pmu_ver.pmu_allocation_get_dmem_size =
			pmu_allocation_get_dmem_size_v3;
		g->ops.pmu_ver.pmu_allocation_get_dmem_offset =
			pmu_allocation_get_dmem_offset_v3;
		g->ops.pmu_ver.pmu_allocation_get_dmem_offset_addr =
			pmu_allocation_get_dmem_offset_addr_v3;
		g->ops.pmu_ver.pmu_allocation_set_dmem_offset =
			pmu_allocation_set_dmem_offset_v3;
		g->ops.pmu_ver.pmu_allocation_get_fb_addr =
				pmu_allocation_get_fb_addr_v3;
		g->ops.pmu_ver.pmu_allocation_get_fb_size =
				pmu_allocation_get_fb_size_v3;
		if (app_version == APP_VERSION_GV10X ||
			app_version == APP_VERSION_TU10X) {
			g->ops.pmu_ver.get_pmu_init_msg_pmu_queue_params =
				get_pmu_init_msg_pmu_queue_params_v5;
			g->ops.pmu_ver.get_pmu_msg_pmu_init_msg_ptr =
				get_pmu_msg_pmu_init_msg_ptr_v5;
			g->ops.pmu_ver.get_pmu_init_msg_pmu_sw_mg_off =
				get_pmu_init_msg_pmu_sw_mg_off_v5;
			g->ops.pmu_ver.get_pmu_init_msg_pmu_sw_mg_size =
				get_pmu_init_msg_pmu_sw_mg_size_v5;
			g->ops.pmu_ver.boardobj.boardobjgrp_pmucmd_construct_impl =
				boardobjgrp_pmucmd_construct_impl_v1;
			g->ops.pmu_ver.boardobj.boardobjgrp_pmuset_impl =
				boardobjgrp_pmuset_impl_v1;
			g->ops.pmu_ver.boardobj.boardobjgrp_pmugetstatus_impl =
				boardobjgrp_pmugetstatus_impl_v1;
			g->ops.pmu_ver.boardobj.is_boardobjgrp_pmucmd_id_valid =
				is_boardobjgrp_pmucmd_id_valid_v1;
			g->ops.pmu_ver.clk.get_vbios_clk_domain =
				nvgpu_clk_get_vbios_clk_domain_gv10x;
			g->ops.pmu_ver.clk.clk_avfs_get_vin_cal_data =
					nvgpu_clk_avfs_get_vin_cal_fuse_v20;
			g->ops.pmu_ver.clk.clk_vf_change_inject_data_fill =
					nvgpu_clk_vf_change_inject_data_fill_gv10x;
			if (app_version == APP_VERSION_GV10X) {
				g->ops.pmu_ver.clk.clk_set_boot_clk =
					nvgpu_clk_set_boot_fll_clk_gv10x;
			} else {
				g->ops.pmu_ver.clk.clk_set_boot_clk = NULL;
			}
		} else {
			g->ops.pmu_ver.get_pmu_init_msg_pmu_queue_params =
				get_pmu_init_msg_pmu_queue_params_v4;
			g->ops.pmu_ver.get_pmu_msg_pmu_init_msg_ptr =
				get_pmu_msg_pmu_init_msg_ptr_v4;
			g->ops.pmu_ver.get_pmu_init_msg_pmu_sw_mg_off =
				get_pmu_init_msg_pmu_sw_mg_off_v4;
			g->ops.pmu_ver.get_pmu_init_msg_pmu_sw_mg_size =
				get_pmu_init_msg_pmu_sw_mg_size_v4;
		}

		g->ops.pmu_ver.get_pmu_perfmon_cmd_start_size =
			get_pmu_perfmon_cmd_start_size_v3;
		g->ops.pmu_ver.get_perfmon_cmd_start_offsetofvar =
			get_perfmon_cmd_start_offsetofvar_v3;
		g->ops.pmu_ver.perfmon_start_set_cmd_type =
			perfmon_start_set_cmd_type_v3;
		g->ops.pmu_ver.perfmon_start_set_group_id =
			perfmon_start_set_group_id_v3;
		g->ops.pmu_ver.perfmon_start_set_state_id =
			perfmon_start_set_state_id_v3;
		g->ops.pmu_ver.perfmon_start_set_flags =
			perfmon_start_set_flags_v3;
		g->ops.pmu_ver.perfmon_start_get_flags =
			perfmon_start_get_flags_v3;
		g->ops.pmu_ver.get_pmu_perfmon_cmd_init_size =
			get_pmu_perfmon_cmd_init_size_v3;
		g->ops.pmu_ver.get_perfmon_cmd_init_offsetofvar =
			get_perfmon_cmd_init_offsetofvar_v3;
		g->ops.pmu_ver.perfmon_cmd_init_set_sample_buffer =
			perfmon_cmd_init_set_sample_buffer_v3;
		g->ops.pmu_ver.perfmon_cmd_init_set_dec_cnt =
			perfmon_cmd_init_set_dec_cnt_v3;
		g->ops.pmu_ver.perfmon_cmd_init_set_base_cnt_id =
			perfmon_cmd_init_set_base_cnt_id_v3;
		g->ops.pmu_ver.perfmon_cmd_init_set_samp_period_us =
			perfmon_cmd_init_set_samp_period_us_v3;
		g->ops.pmu_ver.perfmon_cmd_init_set_num_cnt =
			perfmon_cmd_init_set_num_cnt_v3;
		g->ops.pmu_ver.perfmon_cmd_init_set_mov_avg =
			perfmon_cmd_init_set_mov_avg_v3;
		g->ops.pmu_ver.get_pmu_seq_in_a_ptr =
			get_pmu_sequence_in_alloc_ptr_v3;
		g->ops.pmu_ver.get_pmu_seq_out_a_ptr =
			get_pmu_sequence_out_alloc_ptr_v3;
		break;
	case APP_VERSION_GP10X:
		g->ops.pmu_ver.pg_cmd_eng_buf_load_size =
				pg_cmd_eng_buf_load_size_v2;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_cmd_type =
				pg_cmd_eng_buf_load_set_cmd_type_v2;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_engine_id =
				pg_cmd_eng_buf_load_set_engine_id_v2;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_buf_idx =
				pg_cmd_eng_buf_load_set_buf_idx_v2;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_pad =
				pg_cmd_eng_buf_load_set_pad_v2;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_buf_size =
				pg_cmd_eng_buf_load_set_buf_size_v2;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_base =
				pg_cmd_eng_buf_load_set_dma_base_v2;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_offset =
				pg_cmd_eng_buf_load_set_dma_offset_v2;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_idx =
				pg_cmd_eng_buf_load_set_dma_idx_v2;
		g->ops.pmu_ver.get_perfmon_cntr_ptr = get_perfmon_cntr_ptr_v2;
		g->ops.pmu_ver.set_perfmon_cntr_ut = set_perfmon_cntr_ut_v2;
		g->ops.pmu_ver.set_perfmon_cntr_lt = set_perfmon_cntr_lt_v2;
		g->ops.pmu_ver.set_perfmon_cntr_valid =
			set_perfmon_cntr_valid_v2;
		g->ops.pmu_ver.set_perfmon_cntr_index =
			set_perfmon_cntr_index_v2;
		g->ops.pmu_ver.set_perfmon_cntr_group_id =
			set_perfmon_cntr_group_id_v2;
		g->ops.pmu_ver.get_perfmon_cntr_sz = pmu_perfmon_cntr_sz_v2;
		g->pmu_ver_cmd_id_zbc_table_update = 16;
		nvgpu_set_enabled(g, NVGPU_PMU_ZBC_SAVE, true);
		g->ops.pmu_ver.get_pmu_cmdline_args_size =
			pmu_cmdline_size_v5;
		g->ops.pmu_ver.set_pmu_cmdline_args_cpu_freq =
			set_pmu_cmdline_args_cpufreq_v5;
		g->ops.pmu_ver.set_pmu_cmdline_args_secure_mode =
			set_pmu_cmdline_args_secure_mode_v5;
		g->ops.pmu_ver.set_pmu_cmdline_args_trace_size =
			set_pmu_cmdline_args_falctracesize_v5;
		g->ops.pmu_ver.set_pmu_cmdline_args_trace_dma_base =
			set_pmu_cmdline_args_falctracedmabase_v5;
		g->ops.pmu_ver.set_pmu_cmdline_args_trace_dma_idx =
			set_pmu_cmdline_args_falctracedmaidx_v5;
		g->ops.pmu_ver.get_pmu_cmdline_args_ptr =
			get_pmu_cmdline_args_ptr_v5;
		g->ops.pmu_ver.get_pmu_allocation_struct_size =
			get_pmu_allocation_size_v3;
		g->ops.pmu_ver.set_pmu_allocation_ptr =
			set_pmu_allocation_ptr_v3;
		g->ops.pmu_ver.pmu_allocation_set_dmem_size =
			pmu_allocation_set_dmem_size_v3;
		g->ops.pmu_ver.pmu_allocation_get_dmem_size =
			pmu_allocation_get_dmem_size_v3;
		g->ops.pmu_ver.pmu_allocation_get_dmem_offset =
			pmu_allocation_get_dmem_offset_v3;
		g->ops.pmu_ver.pmu_allocation_get_dmem_offset_addr =
			pmu_allocation_get_dmem_offset_addr_v3;
		g->ops.pmu_ver.pmu_allocation_set_dmem_offset =
			pmu_allocation_set_dmem_offset_v3;
		g->ops.pmu_ver.pmu_allocation_get_fb_addr =
				pmu_allocation_get_fb_addr_v3;
		g->ops.pmu_ver.pmu_allocation_get_fb_size =
				pmu_allocation_get_fb_size_v3;
		g->ops.pmu_ver.get_pmu_init_msg_pmu_queue_params =
				get_pmu_init_msg_pmu_queue_params_v3;
		g->ops.pmu_ver.get_pmu_msg_pmu_init_msg_ptr =
				get_pmu_msg_pmu_init_msg_ptr_v3;
		g->ops.pmu_ver.get_pmu_init_msg_pmu_sw_mg_off =
				get_pmu_init_msg_pmu_sw_mg_off_v3;
		g->ops.pmu_ver.get_pmu_init_msg_pmu_sw_mg_size =
				get_pmu_init_msg_pmu_sw_mg_size_v3;
		g->ops.pmu_ver.get_pmu_perfmon_cmd_start_size =
			get_pmu_perfmon_cmd_start_size_v3;
		g->ops.pmu_ver.get_perfmon_cmd_start_offsetofvar =
			get_perfmon_cmd_start_offsetofvar_v3;
		g->ops.pmu_ver.perfmon_start_set_cmd_type =
			perfmon_start_set_cmd_type_v3;
		g->ops.pmu_ver.perfmon_start_set_group_id =
			perfmon_start_set_group_id_v3;
		g->ops.pmu_ver.perfmon_start_set_state_id =
			perfmon_start_set_state_id_v3;
		g->ops.pmu_ver.perfmon_start_set_flags =
			perfmon_start_set_flags_v3;
		g->ops.pmu_ver.perfmon_start_get_flags =
			perfmon_start_get_flags_v3;
		g->ops.pmu_ver.get_pmu_perfmon_cmd_init_size =
			get_pmu_perfmon_cmd_init_size_v3;
		g->ops.pmu_ver.get_perfmon_cmd_init_offsetofvar =
			get_perfmon_cmd_init_offsetofvar_v3;
		g->ops.pmu_ver.perfmon_cmd_init_set_sample_buffer =
			perfmon_cmd_init_set_sample_buffer_v3;
		g->ops.pmu_ver.perfmon_cmd_init_set_dec_cnt =
			perfmon_cmd_init_set_dec_cnt_v3;
		g->ops.pmu_ver.perfmon_cmd_init_set_base_cnt_id =
			perfmon_cmd_init_set_base_cnt_id_v3;
		g->ops.pmu_ver.perfmon_cmd_init_set_samp_period_us =
			perfmon_cmd_init_set_samp_period_us_v3;
		g->ops.pmu_ver.perfmon_cmd_init_set_num_cnt =
			perfmon_cmd_init_set_num_cnt_v3;
		g->ops.pmu_ver.perfmon_cmd_init_set_mov_avg =
			perfmon_cmd_init_set_mov_avg_v3;
		g->ops.pmu_ver.get_pmu_seq_in_a_ptr =
			get_pmu_sequence_in_alloc_ptr_v3;
		g->ops.pmu_ver.get_pmu_seq_out_a_ptr =
			get_pmu_sequence_out_alloc_ptr_v3;
		g->ops.pmu_ver.boardobj.boardobjgrp_pmucmd_construct_impl =
			boardobjgrp_pmucmd_construct_impl;
		g->ops.pmu_ver.boardobj.boardobjgrp_pmuset_impl =
			boardobjgrp_pmuset_impl;
		g->ops.pmu_ver.boardobj.boardobjgrp_pmugetstatus_impl =
			boardobjgrp_pmugetstatus_impl;
		g->ops.pmu_ver.boardobj.is_boardobjgrp_pmucmd_id_valid =
			is_boardobjgrp_pmucmd_id_valid_v0;
		g->ops.pmu_ver.clk.get_vbios_clk_domain =
			nvgpu_clk_get_vbios_clk_domain_gp10x;
		g->ops.pmu_ver.clk.clk_avfs_get_vin_cal_data =
				nvgpu_clk_avfs_get_vin_cal_fuse_v10;
		g->ops.pmu_ver.clk.clk_vf_change_inject_data_fill =
				nvgpu_clk_vf_change_inject_data_fill_gp10x;
		break;
	case APP_VERSION_GM20B:
		g->ops.pmu_ver.pg_cmd_eng_buf_load_size =
				pg_cmd_eng_buf_load_size_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_cmd_type =
				pg_cmd_eng_buf_load_set_cmd_type_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_engine_id =
				pg_cmd_eng_buf_load_set_engine_id_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_buf_idx =
				pg_cmd_eng_buf_load_set_buf_idx_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_pad =
				pg_cmd_eng_buf_load_set_pad_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_buf_size =
				pg_cmd_eng_buf_load_set_buf_size_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_base =
				pg_cmd_eng_buf_load_set_dma_base_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_offset =
				pg_cmd_eng_buf_load_set_dma_offset_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_idx =
				pg_cmd_eng_buf_load_set_dma_idx_v0;
		g->ops.pmu_ver.get_perfmon_cntr_ptr = get_perfmon_cntr_ptr_v2;
		g->ops.pmu_ver.set_perfmon_cntr_ut = set_perfmon_cntr_ut_v2;
		g->ops.pmu_ver.set_perfmon_cntr_lt = set_perfmon_cntr_lt_v2;
		g->ops.pmu_ver.set_perfmon_cntr_valid =
			set_perfmon_cntr_valid_v2;
		g->ops.pmu_ver.set_perfmon_cntr_index =
			set_perfmon_cntr_index_v2;
		g->ops.pmu_ver.set_perfmon_cntr_group_id =
			set_perfmon_cntr_group_id_v2;
		g->ops.pmu_ver.get_perfmon_cntr_sz = pmu_perfmon_cntr_sz_v2;
		g->pmu_ver_cmd_id_zbc_table_update = 16;
		nvgpu_set_enabled(g, NVGPU_PMU_ZBC_SAVE, true);
		g->ops.pmu_ver.get_pmu_cmdline_args_size =
			pmu_cmdline_size_v3;
		g->ops.pmu_ver.set_pmu_cmdline_args_cpu_freq =
			set_pmu_cmdline_args_cpufreq_v3;
		g->ops.pmu_ver.set_pmu_cmdline_args_secure_mode =
			set_pmu_cmdline_args_secure_mode_v3;
		g->ops.pmu_ver.set_pmu_cmdline_args_trace_size =
			set_pmu_cmdline_args_falctracesize_v3;
		g->ops.pmu_ver.set_pmu_cmdline_args_trace_dma_base =
			set_pmu_cmdline_args_falctracedmabase_v3;
		g->ops.pmu_ver.set_pmu_cmdline_args_trace_dma_idx =
			set_pmu_cmdline_args_falctracedmaidx_v3;
		g->ops.pmu_ver.get_pmu_cmdline_args_ptr =
			get_pmu_cmdline_args_ptr_v3;
		g->ops.pmu_ver.get_pmu_allocation_struct_size =
			get_pmu_allocation_size_v1;
		g->ops.pmu_ver.set_pmu_allocation_ptr =
			set_pmu_allocation_ptr_v1;
		g->ops.pmu_ver.pmu_allocation_set_dmem_size =
			pmu_allocation_set_dmem_size_v1;
		g->ops.pmu_ver.pmu_allocation_get_dmem_size =
			pmu_allocation_get_dmem_size_v1;
		g->ops.pmu_ver.pmu_allocation_get_dmem_offset =
			pmu_allocation_get_dmem_offset_v1;
		g->ops.pmu_ver.pmu_allocation_get_dmem_offset_addr =
			pmu_allocation_get_dmem_offset_addr_v1;
		g->ops.pmu_ver.pmu_allocation_set_dmem_offset =
			pmu_allocation_set_dmem_offset_v1;
		g->ops.pmu_ver.get_pmu_init_msg_pmu_queue_params =
			get_pmu_init_msg_pmu_queue_params_v1;
		g->ops.pmu_ver.get_pmu_msg_pmu_init_msg_ptr =
			get_pmu_msg_pmu_init_msg_ptr_v1;
		g->ops.pmu_ver.get_pmu_init_msg_pmu_sw_mg_off =
			get_pmu_init_msg_pmu_sw_mg_off_v1;
		g->ops.pmu_ver.get_pmu_init_msg_pmu_sw_mg_size =
			get_pmu_init_msg_pmu_sw_mg_size_v1;
		g->ops.pmu_ver.get_pmu_perfmon_cmd_start_size =
			get_pmu_perfmon_cmd_start_size_v1;
		g->ops.pmu_ver.get_perfmon_cmd_start_offsetofvar =
			get_perfmon_cmd_start_offsetofvar_v1;
		g->ops.pmu_ver.perfmon_start_set_cmd_type =
			perfmon_start_set_cmd_type_v1;
		g->ops.pmu_ver.perfmon_start_set_group_id =
			perfmon_start_set_group_id_v1;
		g->ops.pmu_ver.perfmon_start_set_state_id =
			perfmon_start_set_state_id_v1;
		g->ops.pmu_ver.perfmon_start_set_flags =
			perfmon_start_set_flags_v1;
		g->ops.pmu_ver.perfmon_start_get_flags =
			perfmon_start_get_flags_v1;
		g->ops.pmu_ver.get_pmu_perfmon_cmd_init_size =
			get_pmu_perfmon_cmd_init_size_v1;
		g->ops.pmu_ver.get_perfmon_cmd_init_offsetofvar =
			get_perfmon_cmd_init_offsetofvar_v1;
		g->ops.pmu_ver.perfmon_cmd_init_set_sample_buffer =
			perfmon_cmd_init_set_sample_buffer_v1;
		g->ops.pmu_ver.perfmon_cmd_init_set_dec_cnt =
			perfmon_cmd_init_set_dec_cnt_v1;
		g->ops.pmu_ver.perfmon_cmd_init_set_base_cnt_id =
			perfmon_cmd_init_set_base_cnt_id_v1;
		g->ops.pmu_ver.perfmon_cmd_init_set_samp_period_us =
			perfmon_cmd_init_set_samp_period_us_v1;
		g->ops.pmu_ver.perfmon_cmd_init_set_num_cnt =
			perfmon_cmd_init_set_num_cnt_v1;
		g->ops.pmu_ver.perfmon_cmd_init_set_mov_avg =
			perfmon_cmd_init_set_mov_avg_v1;
		g->ops.pmu_ver.get_pmu_seq_in_a_ptr =
			get_pmu_sequence_in_alloc_ptr_v1;
		g->ops.pmu_ver.get_pmu_seq_out_a_ptr =
			get_pmu_sequence_out_alloc_ptr_v1;
		break;
	default:
		nvgpu_err(g, "PMU code version not supported version: %d\n",
			app_version);
		err = -EINVAL;
		break;
	}
	pv->set_perfmon_cntr_index(pmu, 3); /* GR & CE2 */
	pv->set_perfmon_cntr_group_id(pmu, PMU_DOMAIN_GROUP_PSTATE);

	return err;
}

static void nvgpu_remove_pmu_support(struct nvgpu_pmu *pmu)
{
	struct gk20a *g = gk20a_from_pmu(pmu);
	struct mm_gk20a *mm = &g->mm;
	struct vm_gk20a *vm = mm->pmu.vm;
	struct boardobj *pboardobj, *pboardobj_tmp;
	struct boardobjgrp *pboardobjgrp, *pboardobjgrp_tmp;

	nvgpu_log_fn(g, " ");

	if (nvgpu_alloc_initialized(&pmu->dmem)) {
		nvgpu_alloc_destroy(&pmu->dmem);
	}

	nvgpu_list_for_each_entry_safe(pboardobjgrp, pboardobjgrp_tmp,
		&g->boardobjgrp_head, boardobjgrp, node) {
		pboardobjgrp->destruct(pboardobjgrp);
	}

	nvgpu_list_for_each_entry_safe(pboardobj, pboardobj_tmp,
			&g->boardobj_head, boardobj, node) {
		pboardobj->destruct(pboardobj);
	}

	if (pmu->fw_image != NULL) {
		nvgpu_release_firmware(g, pmu->fw_image);
	}

	if (pmu->fw_desc != NULL) {
		nvgpu_release_firmware(g, pmu->fw_desc);
	}

	if (pmu->fw_sig != NULL) {
		nvgpu_release_firmware(g, pmu->fw_sig);
	}

	if (nvgpu_mem_is_valid(&pmu->ucode)) {
		nvgpu_dma_unmap_free(vm, &pmu->ucode);
	}

	if (nvgpu_mem_is_valid(&pmu->seq_buf)) {
		nvgpu_dma_unmap_free(vm, &pmu->seq_buf);
	}

	if (nvgpu_mem_is_valid(&pmu->super_surface_buf)) {
		nvgpu_dma_unmap_free(vm, &pmu->super_surface_buf);
	}

	nvgpu_mutex_destroy(&pmu->pmu_pg.elpg_mutex);
	nvgpu_mutex_destroy(&pmu->pmu_pg.pg_mutex);
	nvgpu_mutex_destroy(&pmu->isr_mutex);
	nvgpu_mutex_destroy(&pmu->pmu_copy_lock);
	nvgpu_mutex_destroy(&pmu->pmu_seq_lock);
}

static int init_pmu_ucode(struct nvgpu_pmu *pmu)
{
	struct gk20a *g = gk20a_from_pmu(pmu);
	struct pmu_ucode_desc *desc;
	int err = 0;

	if (pmu->fw_image != NULL) {
		goto exit;
	}

	if (!nvgpu_is_enabled(g, NVGPU_SEC_PRIVSECURITY)) {
		/* non-secure PMU boot uocde */
		pmu->fw_image = nvgpu_request_firmware(g,
				NVGPU_PMU_NS_UCODE_IMAGE, 0);
		if (pmu->fw_image == NULL) {
			nvgpu_err(g,
				"failed to load non-secure pmu ucode!!");
			goto exit;
		}

		desc = (struct pmu_ucode_desc *)
			(void *)pmu->fw_image->data;
	} else {
		/* secure boot ucodes's */
		nvgpu_pmu_dbg(g, "requesting PMU ucode image");
		pmu->fw_image = nvgpu_request_firmware(g, NVGPU_PMU_UCODE_IMAGE, 0);
		if (pmu->fw_image == NULL) {
			nvgpu_err(g, "failed to load pmu ucode!!");
			err = -ENOENT;
			goto exit;
		}

		nvgpu_pmu_dbg(g, "requesting PMU ucode desc");
		pmu->fw_desc = nvgpu_request_firmware(g, NVGPU_PMU_UCODE_DESC, 0);
		if (pmu->fw_desc == NULL) {
			nvgpu_err(g, "failed to load pmu ucode desc!!");
			err = -ENOENT;
			goto release_img_fw;
		}

		nvgpu_pmu_dbg(g, "requesting PMU ucode sign");
		pmu->fw_sig = nvgpu_request_firmware(g, NVGPU_PMU_UCODE_SIG, 0);
		if (pmu->fw_sig == NULL) {
			nvgpu_err(g, "failed to load pmu sig!!");
			err = -ENOENT;
			goto release_desc;
		}

		desc = (struct pmu_ucode_desc *)(void *)pmu->fw_desc->data;
	}

	err = init_pmu_fw_ver_ops(pmu, desc->app_version);
	if (err != 0) {
		nvgpu_err(g, "failed to set function pointers");
		goto release_sig;
	}

	goto exit;

release_sig:
	nvgpu_release_firmware(g, pmu->fw_sig);
release_desc:
	nvgpu_release_firmware(g, pmu->fw_desc);
release_img_fw:
	nvgpu_release_firmware(g, pmu->fw_image);
exit:
	return err;
}

int nvgpu_early_init_pmu_sw(struct gk20a *g, struct nvgpu_pmu *pmu)
{
	int err = 0;

	nvgpu_log_fn(g, " ");

	pmu->g = g;

	if (!g->support_ls_pmu) {
		goto exit;
	}

	if (!g->ops.pmu.is_pmu_supported(g)) {
		g->support_ls_pmu = false;

		/* Disable LS PMU global checkers */
		g->can_elpg = false;
		g->elpg_enabled = false;
		g->aelpg_enabled = false;
		nvgpu_set_enabled(g, NVGPU_PMU_PERFMON, false);
		goto exit;
	}

	err = nvgpu_mutex_init(&pmu->pmu_pg.elpg_mutex);
	if (err != 0) {
		return err;
	}

	err = nvgpu_mutex_init(&pmu->pmu_pg.pg_mutex);
	if (err != 0) {
		goto fail_elpg;
	}

	err = nvgpu_mutex_init(&pmu->isr_mutex);
	if (err != 0) {
		goto fail_pg;
	}

	err = nvgpu_mutex_init(&pmu->pmu_copy_lock);
	if (err != 0) {
		goto fail_isr;
	}

	err = nvgpu_mutex_init(&pmu->pmu_seq_lock);
	if (err != 0) {
		goto fail_pmu_copy;
	}

	err = init_pmu_ucode(pmu);
	if (err != 0) {
		goto fail_seq_lock;
	}

	pmu->remove_support = nvgpu_remove_pmu_support;

	goto exit;

fail_seq_lock:
nvgpu_mutex_destroy(&pmu->pmu_seq_lock);
fail_pmu_copy:
	nvgpu_mutex_destroy(&pmu->pmu_copy_lock);
fail_isr:
	nvgpu_mutex_destroy(&pmu->isr_mutex);
fail_pg:
	nvgpu_mutex_destroy(&pmu->pmu_pg.pg_mutex);
fail_elpg:
	nvgpu_mutex_destroy(&pmu->pmu_pg.elpg_mutex);
exit:
	return err;
}

int nvgpu_pmu_prepare_ns_ucode_blob(struct gk20a *g)
{
	struct nvgpu_pmu *pmu = &g->pmu;
	struct mm_gk20a *mm = &g->mm;
	struct vm_gk20a *vm = mm->pmu.vm;
	struct pmu_ucode_desc *desc;
	u32 *ucode_image = NULL;
	int err = 0;

	nvgpu_log_fn(g, " ");

	desc = (struct pmu_ucode_desc *)(void *)pmu->fw_image->data;
	ucode_image = (u32 *)(void *)((u8 *)desc + desc->descriptor_size);

	err = nvgpu_dma_alloc_map_sys(vm, GK20A_PMU_UCODE_SIZE_MAX,
			&pmu->ucode);
	if (err != 0) {
		goto exit;
	}

	nvgpu_mem_wr_n(g, &pmu->ucode, 0, ucode_image,
		desc->app_start_offset + desc->app_size);

exit:
	return err;
}
