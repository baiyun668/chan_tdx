/*
 * macd.h - MACD计算模块
 */
#ifndef CHAN_TDX_MACD_H
#define CHAN_TDX_MACD_H

#include "chan_tdx.h"

/* 计算MACD (DIFF, DEA, MACD柱) */
void chan_macd_compute(ChanState *s);

/* 获取某笔的MACD力度 */
float bi_macd_power(ChanState *s, int bi_idx);

/* 获取某段区间的MACD力度 */
float macd_power_range(ChanState *s, int bar_begin, int bar_end, int algo);

#endif /* CHAN_TDX_MACD_H */
