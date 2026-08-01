/*
 * zs.c - 中枢构建
 *
 * 对齐 chan.py 的 ZhongShu 模块
 *
 * 规则：
 * 1. 3笔重叠构成中枢：ZG = min(Bi[i].high, i=0..2), ZD = max(Bi[i].low, i=0..2)
 * 2. ZG > ZD 才构成有效中枢
 * 3. 中枢延伸：后续笔若仍在 [ZD, ZG] 范围内
 * 4. 中枢合并：相邻中枢有重叠
 * 5. 一笔中枢(one_bi_zs)选项
 * 6. 记录 bi_in, bi_out, peak_high, peak_low
 */

#include "zs.h"

/* ============================================================
 * 尝试从指定笔开始构建中枢
 * 返回中枢的结束笔索引，-1=不能构成
 * ============================================================ */
static int try_build_zs(ChanState *s, int bi_start, ZhongShu *zs) {
    int nb = s->bi_count;
    if (bi_start + 2 >= nb) return -1;

    /* 取前3笔的重叠区间 */
    Bi *b0 = &s->bi_list[bi_start];
    Bi *b1 = &s->bi_list[bi_start + 1];
    Bi *b2 = &s->bi_list[bi_start + 2];

    float zg = float_min(float_min(b0->high, b1->high), b2->high);
    float zd = float_max(float_max(b0->low, b1->low), b2->low);

    if (zg <= zd) return -1; /* 无重叠 */

    /* 初始化中枢 */
    zs->zg        = zg;
    zs->zd        = zd;
    zs->bi_begin  = bi_start;
    zs->bi_end    = bi_start + 2;
    zs->bi_in     = bi_start;
    zs->bi_out    = -1;
    zs->bar_begin = b0->bar_begin;
    zs->bar_end   = b2->bar_end;
    zs->level     = 0;
    zs->is_sure   = 1;

    /* peak_high/peak_low = 所有涉及笔的极值 */
    zs->gg = float_max(float_max(b0->high, b1->high), b2->high);
    zs->dd = float_min(float_min(b0->low, b1->low), b2->low);

    /* 中枢延伸 */
    int last_bi = bi_start + 2;
    for (int i = bi_start + 3; i < nb; i++) {
        Bi *b = &s->bi_list[i];
        /* 笔与中枢有重叠 */
        if (b->high > zs->zd && b->low < zs->zg) {
            last_bi = i;
            zs->bi_end  = i;
            zs->bar_end = b->bar_end;
            if (b->high > zs->gg) zs->gg = b->high;
            if (b->low < zs->dd)  zs->dd = b->low;
        } else {
            /* 笔完全离开中枢 */
            zs->bi_out = i;
            break;
        }
    }

    return last_bi;
}

/* ============================================================
 * 中枢合并
 *
 * zs_combine_mode:
 *   ZS_COMBINE_NONE (0): 不合并
 *   ZS_COMBINE_ZS   (1): 中枢区间 [ZD, ZG] 有重叠则合并
 *   ZS_COMBINE_PEAK (2): 峰值区间 [DD, GG] 有重叠则合并
 * ============================================================ */
void chan_zs_combine(ChanState *s) {
    if (s->config.zs_combine_mode == ZS_COMBINE_NONE) return;

    int changed = 1;
    while (changed) {
        changed = 0;
        for (int i = 0; i < s->zs_count - 1; i++) {
            ZhongShu *a = &s->zs_list[i];
            ZhongShu *b = &s->zs_list[i + 1];

            int overlap = 0;
            if (s->config.zs_combine_mode == ZS_COMBINE_ZS) {
                /* 中枢区间重叠 */
                overlap = (a->zg > b->zd && a->zd < b->zg);
            } else if (s->config.zs_combine_mode == ZS_COMBINE_PEAK) {
                /* 峰值区间重叠 */
                overlap = (a->gg > b->dd && a->dd < b->gg);
            }

            if (overlap) {
                /* 合并中枢到a */
                a->zg       = float_min(a->zg, b->zg);
                a->zd       = float_max(a->zd, b->zd);
                a->gg       = float_max(a->gg, b->gg);
                a->dd       = float_min(a->dd, b->dd);
                a->bi_end   = b->bi_end;
                a->bar_end  = b->bar_end;
                if (b->bi_out >= 0) {
                    a->bi_out = b->bi_out;
                }

                /* 删除b，后面的前移 */
                for (int j = i + 1; j < s->zs_count - 1; j++) {
                    s->zs_list[j] = s->zs_list[j + 1];
                }
                s->zs_count--;
                changed = 1;
                break;
            }
        }
    }
}

/* ============================================================
 * 中枢构建主函数
 * ============================================================ */
void chan_zs_build(ChanState *s) {
    s->zs_count = 0;
    int nb = s->bi_count;
    if (nb < 3) return;

    int bi_idx = 0;

    while (bi_idx < nb - 2) {
        if (s->config.one_bi_zs) {
            /* 一笔中枢模式：每笔都可以是中枢 */
            /* 尝试从当前笔开始 */
        }

        ZhongShu zs;
        int end_bi = try_build_zs(s, bi_idx, &zs);

        if (end_bi >= 0) {
            if (s->zs_count >= MAX_ZS) break;
            s->zs_list[s->zs_count] = zs;
            s->zs_count++;
            /* 从中枢最后一笔的下一笔开始寻找下一个中枢 */
            /* 但如果bi_out有值，从bi_out开始（可能形成新中枢） */
            if (zs.bi_out >= 0) {
                bi_idx = zs.bi_out;
            } else {
                bi_idx = end_bi + 1;
            }
        } else {
            bi_idx++;
        }
    }

    /* 中枢合并 */
    chan_zs_combine(s);
}
