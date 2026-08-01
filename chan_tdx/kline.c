/*
 * kline.c - K线合并与分型识别
 *
 * 对齐 chan.py 的 KLineCombiner 和 FenXing 模块
 *
 * 包含关系处理规则：
 *   K[i] 包含 K[j]：K[i].high >= K[j].high && K[i].low <= K[j].low
 *   K[j] 包含 K[i]：K[j].high >= K[i].high && K[j].low <= K[i].low
 *
 * 合并方向：
 *   UP   -> 取 max(high), max(low)
 *   DOWN -> 取 min(high), min(low)
 */

#include "kline.h"

/* ============================================================
 * 判断两根K线是否存在包含关系
 * 返回: 0=无包含, 1=i包含j, -1=j包含i
 * ============================================================ */
static int has_inclusion(const MergedKLine *a, const MergedKLine *b) {
    /* a 包含 b */
    if (a->high >= b->high && a->low <= b->low) return 1;
    /* b 包含 a */
    if (b->high >= a->high && b->low <= a->low) return -1;
    return 0;
}

/* ============================================================
 * K线合并
 * chan.py 中的 KLineCombiner
 * ============================================================ */
void chan_kline_merge(ChanState *s) {
    int n = s->bar_count;
    if (n <= 0) return;

    s->merged_count = 0;
    MergedKLine *m = s->merged;

    /* 第一根K线直接加入 */
    m[0].high  = s->bars[0].high;
    m[0].low   = s->bars[0].low;
    m[0].begin = 0;
    m[0].end   = 0;
    m[0].count = 1;
    s->merged_count = 1;

    for (int i = 1; i < n; i++) {
        int mc = s->merged_count;
        MergedKLine *last = &m[mc - 1];

        /* 创建临时MergedKLine用于比较 */
        MergedKLine cur;
        cur.high  = s->bars[i].high;
        cur.low   = s->bars[i].low;
        cur.begin = i;
        cur.end   = i;
        cur.count = 1;

        int inc = has_inclusion(last, &cur);

        if (inc != 0) {
            /* 存在包含关系，需要合并 */
            /* 确定合并方向 */
            Direction dir = DIR_NONE;
            if (mc >= 2) {
                /* 用前一根合并K线与当前last的关系确定方向 */
                MergedKLine *prev = &m[mc - 2];
                if (last->high > prev->high) {
                    dir = DIR_UP;
                } else if (last->high < prev->high) {
                    dir = DIR_DOWN;
                } else {
                    /* 相等时用low判断 */
                    if (last->low > prev->low) {
                        dir = DIR_UP;
                    } else {
                        dir = DIR_DOWN;
                    }
                }
            } else {
                /* 只有一根，用当前K线与前一根的方向判断 */
                if (cur.high >= last->high) {
                    dir = DIR_UP;
                } else {
                    dir = DIR_DOWN;
                }
            }

            /* 合并 */
            if (dir == DIR_UP) {
                last->high = float_max(last->high, cur.high);
                last->low  = float_max(last->low, cur.low);
            } else {
                last->high = float_min(last->high, cur.high);
                last->low  = float_min(last->low, cur.low);
            }
            last->end   = i;
            last->count += 1;
        } else {
            /* 无包含关系，直接加入 */
            if (s->merged_count >= MAX_MERGED_KL) break;
            m[s->merged_count] = cur;
            s->merged_count++;
        }
    }
}

/* ============================================================
 * 分型识别
 *
 * 顶分型：中间K线的 high > 左右两根的 high，
 *         且 middle.low >= 左右两根的 low（严格模式需 >）
 *
 * 底分型：中间K线的 low < 左右两根的 low，
 *         且 middle.high <= 左右两根的 high（严格模式需 <）
 *
 * chan.py 中的 FenXing 模块
 * ============================================================ */
void chan_fractal_find(ChanState *s) {
    int n = s->merged_count;
    if (n < 3) return;

    s->fractal_count = 0;
    int exclude = s->config.exclude_included;

    for (int i = 1; i < n - 1; i++) {
        MergedKLine *left  = &s->merged[i - 1];
        MergedKLine *mid   = &s->merged[i];
        MergedKLine *right = &s->merged[i + 1];

        /* 检查中间K线与左右K线是否有包含关系 */
        if (exclude) {
            if (has_inclusion(left, mid) != 0 || has_inclusion(mid, right) != 0) {
                continue;
            }
        }

        /* 顶分型 */
        if (mid->high > left->high && mid->high > right->high) {
            /* chan.py: 严格要求 low 也高于两侧 */
            if (mid->low > left->low && mid->low > right->low) {
                if (s->fractal_count >= MAX_FRACTALS) break;
                Fractal *fx = &s->fractals[s->fractal_count];
                fx->type    = FX_TOP;
                fx->mid_idx = i;
                fx->begin   = i - 1;
                fx->end     = i + 1;
                fx->bar_idx = mid->end;  /* 取中间K线的最后一根原始K线 */
                fx->high    = mid->high;
                fx->low     = mid->low;
                s->fractal_count++;
            }
        }
        /* 底分型 */
        else if (mid->low < left->low && mid->low < right->low) {
            if (mid->high < left->high && mid->high < right->high) {
                if (s->fractal_count >= MAX_FRACTALS) break;
                Fractal *fx = &s->fractals[s->fractal_count];
                fx->type    = FX_BOTTOM;
                fx->mid_idx = i;
                fx->begin   = i - 1;
                fx->end     = i + 1;
                fx->bar_idx = mid->end;
                fx->high    = mid->high;
                fx->low     = mid->low;
                s->fractal_count++;
            }
        }
    }
}
