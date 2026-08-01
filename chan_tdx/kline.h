/*
 * kline.h - K线合并与分型识别
 */
#ifndef CHAN_TDX_KLINE_H
#define CHAN_TDX_KLINE_H

#include "chan_tdx.h"

/* K线合并（处理包含关系） */
void chan_kline_merge(ChanState *s);

/* 分型识别 */
void chan_fractal_find(ChanState *s);

#endif /* CHAN_TDX_KLINE_H */
