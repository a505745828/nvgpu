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

#include <nvgpu/gk20a.h>
#include <nvgpu/log.h>
#include <nvgpu/enabled.h>
#include <nvgpu/bug.h>
#include <nvgpu/utils.h>
#include <nvgpu/power_features/cg.h>
#include <nvgpu/nvgpu_err.h>
#include <nvgpu/boardobj.h>
#include <nvgpu/boardobjgrp.h>
#include <nvgpu/pmu.h>

/* PMU H/W error functions */
void nvgpu_pmu_report_bar0_pri_err_status(struct gk20a *g, u32 bar0_status,
	u32 error_type)
{
	nvgpu_report_pmu_err(g, NVGPU_ERR_MODULE_PMU,
		GPU_PMU_BAR0_ERROR_TIMEOUT, error_type, bar0_status);
}

/* PMU engine reset functions */
static int pmu_enable_hw(struct nvgpu_pmu *pmu, bool enable)
{
	struct gk20a *g = pmu->g;
	int err = 0;

	nvgpu_log_fn(g, " %s ", g->name);

	if (enable) {
		/* bring PMU falcon/engine out of reset */
		g->ops.pmu.reset_engine(g, true);

		nvgpu_cg_slcg_pmu_load_enable(g);

		nvgpu_cg_blcg_pmu_load_enable(g);

		if (nvgpu_falcon_mem_scrub_wait(pmu->flcn) != 0) {
			/* keep PMU falcon/engine in reset
			 * if IMEM/DMEM scrubbing fails
			 */
			g->ops.pmu.reset_engine(g, false);
			nvgpu_err(g, "Falcon mem scrubbing timeout");
			err = -ETIMEDOUT;
		}
	} else {
		/* keep PMU falcon/engine in reset */
		g->ops.pmu.reset_engine(g, false);
	}

	nvgpu_log_fn(g, "%s Done, status - %d ", g->name, err);
	return err;
}

static int pmu_enable(struct nvgpu_pmu *pmu, bool enable)
{
	struct gk20a *g = pmu->g;
	int err = 0;

	nvgpu_log_fn(g, " ");

	if (!enable) {
		if (!g->ops.pmu.is_engine_in_reset(g)) {
#ifdef CONFIG_NVGPU_LS_PMU
			g->ops.pmu.pmu_enable_irq(pmu, false);
#endif
			err = pmu_enable_hw(pmu, false);
			if (err != 0) {
				goto exit;
			}
		}
	} else {
		err = pmu_enable_hw(pmu, true);
		if (err != 0) {
			goto exit;
		}

		err = nvgpu_falcon_wait_idle(pmu->flcn);
		if (err != 0) {
			goto exit;
		}
	}

exit:
	nvgpu_log_fn(g, "Done, status - %d ", err);
	return err;
}

int nvgpu_pmu_reset(struct gk20a *g)
{
	struct nvgpu_pmu *pmu = g->pmu;
	int err = 0;

	nvgpu_log_fn(g, " %s ", g->name);

	err = pmu_enable(pmu, false);
	if (err != 0) {
		goto exit;
	}

	err = pmu_enable(pmu, true);
	if (err != 0) {
		goto exit;
	}

exit:
	nvgpu_log_fn(g, " %s Done, status - %d ", g->name, err);
	return err;
}

/* PMU unit deinit */
void nvgpu_pmu_remove_support(struct gk20a *g, struct nvgpu_pmu *pmu)
{
	if(pmu != NULL) {
#ifdef CONFIG_NVGPU_LS_PMU
		if (pmu->remove_support != NULL) {
			pmu->remove_support(g->pmu);
		}
#endif
		nvgpu_kfree(g, g->pmu);
		g->pmu = NULL;
	}
}

/* PMU unit init */
int nvgpu_pmu_early_init(struct gk20a *g, struct nvgpu_pmu **pmu_p)
{
	int err = 0;
	struct nvgpu_pmu *pmu;

	nvgpu_log_fn(g, " ");

	if (*pmu_p != NULL) {
		/* skip alloc/reinit for unrailgate sequence */
		nvgpu_pmu_dbg(g, "skip pmu init for unrailgate sequence");
		goto exit;
	}

	pmu = (struct nvgpu_pmu *) nvgpu_kzalloc(g, sizeof(struct nvgpu_pmu));
	if (pmu == NULL) {
		err = -ENOMEM;
		goto exit;
	}

	*pmu_p = pmu;
	pmu->g = g;
	pmu->flcn = &g->pmu_flcn;

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

#ifdef CONFIG_NVGPU_LS_PMU
	err = nvgpu_pmu_rtos_early_init(g, pmu);
#endif

exit:
	return err;
}
