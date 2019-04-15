/*
 * GV11B Fifo
 *
 * Copyright (c) 2016-2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef FIFO_GV11B_H
#define FIFO_GV11B_H

#define PBDMA_SUBDEVICE_ID  1U

#define FIFO_INVAL_PBDMA_ID	(~U32(0U))
#define FIFO_INVAL_VEID		(~U32(0U))

#define CHANNEL_INFO_VEID0		0U

#define MAX_PRE_SI_RETRIES		200000U	/* 1G/500KHz * 100 */

struct gpu_ops;

void gv11b_mmu_fault_id_to_eng_pbdma_id_and_veid(struct gk20a *g,
	u32 mmu_fault_id, u32 *active_engine_id, u32 *veid, u32 *pbdma_id);

int gv11b_fifo_is_preempt_pending(struct gk20a *g, u32 id,
		 unsigned int id_type);
int gv11b_fifo_preempt_channel(struct gk20a *g, struct channel_gk20a *ch);
int gv11b_fifo_preempt_tsg(struct gk20a *g, struct tsg_gk20a *tsg);
void gv11b_fifo_teardown_ch_tsg(struct gk20a *g, u32 act_eng_bitmask,
			u32 id, unsigned int id_type, unsigned int rc_type,
			 struct mmu_fault_info *mmfault);
void gv11b_fifo_init_pbdma_intr_descs(struct fifo_gk20a *f);
int gv11b_init_fifo_reset_enable_hw(struct gk20a *g);
int gv11b_init_fifo_setup_hw(struct gk20a *g);

u32 gv11b_fifo_get_preempt_timeout(struct gk20a *g);

#endif
