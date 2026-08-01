/*
 * zs.h - 中枢构建模块
 */
#ifndef CHAN_TDX_ZS_H
#define CHAN_TDX_ZS_H

#include "chan_tdx.h"

/* 构建中枢序列 */
void chan_zs_build(ChanState *s);

/* 中枢合并 */
void chan_zs_combine(ChanState *s);

#endif /* CHAN_TDX_ZS_H */
