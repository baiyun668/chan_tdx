/*
 * bi.h - 笔构建模块
 */
#ifndef CHAN_TDX_BI_H
#define CHAN_TDX_BI_H

#include "chan_tdx.h"

/* 构建笔序列 */
void chan_bi_build(ChanState *s);

/* 检查分型是否合法 */
int bi_fx_check_valid(ChanState *s, const Fractal *fx, const MergedKLine *merged);

/* 检查笔的端点是否为极值 */
int bi_check_peak(ChanState *s, const Bi *b);

/* 检查缺口 */
int bi_is_gap(ChanState *s, int merged_idx);

#endif /* CHAN_TDX_BI_H */
