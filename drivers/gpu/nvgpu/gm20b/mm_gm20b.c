/*
 * GM20B MMU
 *
 * Copyright (c) 2014-2018, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/sizes.h>
#include <nvgpu/gk20a.h>

#include "mm_gm20b.h"

#include <nvgpu/hw/gm20b/hw_gmmu_gm20b.h>

u32 gm20b_mm_get_big_page_sizes(void)
{
	return SZ_64K | SZ_128K;
}

u32 gm20b_mm_get_default_big_page_size(void)
{
	return SZ_64K;
}

bool gm20b_mm_is_bar1_supported(struct gk20a *g)
{
	return true;
}

u64 gm20b_gpu_phys_addr(struct gk20a *g,
			struct nvgpu_gmmu_attrs *attrs, u64 phys)
{
	return phys;
}

u32 gm20b_get_kind_invalid(void)
{
	return gmmu_pte_kind_invalid_v();
}

u32 gm20b_get_kind_pitch(void)
{
	return gmmu_pte_kind_pitch_v();
}
