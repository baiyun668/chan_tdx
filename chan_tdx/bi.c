/*
 * bi.c - 笔构建
 *
 * 对齐 chan.py 的 Bi 模块
 *
 * 规则：
 * 1. 顶分型到底分型为下降笔，底分型到顶分型为上升笔
 * 2. 严格模式：两个分型之间至少4根合并K线（bi_span >= 4）
 * 3. 非严格模式：span>=3 且 原始K线数>=3
 * 4. bi_end_is_peak: 笔终点必须是极值
 * 5. bi_fx_check: 分型验证
 * 6. gap_as_kl: 缺口当成K线
 * 7. bi_allow_sub_peak: 次高低点成笔
 */

#include "bi.h"

/* ============================================================
 * 检查是否存在缺口（相邻合并K线无重叠）
 * gap_as_kl 模式下，缺口处可降低笔的跨度要求
 * ============================================================ */
int bi_is_gap(ChanState *s, int idx) {
    if (idx <= 0 || idx >= s->merged_count) return 0;
    MergedKLine *prev = &s->merged[idx - 1];
    MergedKLine *curr = &s->merged[idx];
    /* 向上缺口：prev.high < curr.low */
    if (prev->high < curr->low) return 1;
    /* 向下缺口：prev.low > curr.high */
    if (prev->low > curr->high) return 1;
    return 0;
}

/* ============================================================
 * 分型验证
 *
 * bi_fx_check 模式:
 *   STRICT  (0): 分型三根K线不能有包含关系
 *   LOSS    (1): 允许有包含关系
 *   HALF    (2): 只检查中间K线与两侧之一
 *   TOTALLY (3): 完全不检查
 * ============================================================ */
int bi_fx_check_valid(ChanState *s, const Fractal *fx, const MergedKLine *merged) {
    int mode = s->config.bi_fx_check;

    if (mode == FX_CHECK_TOTALLY) return 1;

    if (fx->begin < 0 || fx->end >= s->merged_count) return 0;

    const MergedKLine *left  = &merged[fx->begin];
    const MergedKLine *mid   = &merged[fx->mid_idx];
    const MergedKLine *right = &merged[fx->end];

    /* 检查包含关系 */
    int inc_lr = (left->high >= mid->high && left->low <= mid->low) ||
                 (mid->high >= left->high && mid->low <= left->low);
    int inc_mr = (mid->high >= right->high && mid->low <= right->low) ||
                 (right->high >= mid->high && right->low <= mid->low);

    switch (mode) {
    case FX_CHECK_STRICT:
        /* 不允许任何包含关系 */
        if (inc_lr || inc_mr) return 0;
        break;
    case FX_CHECK_LOSS:
        /* 允许包含关系 */
        break;
    case FX_CHECK_HALF:
        /* 只允许一侧有包含关系 */
        if (inc_lr && inc_mr) return 0;
        break;
    default:
        break;
    }

    return 1;
}

/* ============================================================
 * 检查笔的端点是否为极值
 * bi_end_is_peak 模式：遍历笔内所有合并K线，确认端点是极值
 * ============================================================ */
int bi_check_peak(ChanState *s, const Bi *b) {
    if (!s->config.bi_end_is_peak) return 1;

    int start = b->begin;
    int end   = b->end;
    if (start < 0 || end >= s->merged_count || start > end) return 0;

    if (b->dir == DIR_UP) {
        /* 上升笔：终点的high应该是区间最高 */
        float max_high = s->merged[start].high;
        for (int i = start + 1; i <= end; i++) {
            if (s->merged[i].high > max_high) {
                max_high = s->merged[i].high;
            }
        }
        /* 终点分型的high应该等于最高 */
        if (b->end_fx.high < max_high - 0.0001f) return 0;

        /* 起点的low应该是区间最低 */
        float min_low = s->merged[start].low;
        for (int i = start + 1; i <= end; i++) {
            if (s->merged[i].low < min_low) {
                min_low = s->merged[i].low;
            }
        }
        if (b->start_fx.low > min_low + 0.0001f) return 0;
    } else {
        /* 下降笔：终点的low应该是区间最低 */
        float min_low = s->merged[start].low;
        for (int i = start + 1; i <= end; i++) {
            if (s->merged[i].low < min_low) {
                min_low = s->merged[i].low;
            }
        }
        if (b->end_fx.low > min_low + 0.0001f) return 0;

        /* 起点的high应该是区间最高 */
        float max_high = s->merged[start].high;
        for (int i = start + 1; i <= end; i++) {
            if (s->merged[i].high > max_high) {
                max_high = s->merged[i].high;
            }
        }
        if (b->start_fx.high < max_high - 0.0001f) return 0;
    }

    return 1;
}

/* ============================================================
 * 检查两个分型之间能否构成笔
 *
 * 返回: 1=可以, 0=不可以
 * ============================================================ */
static int can_form_bi(ChanState *s, const Fractal *start_fx, const Fractal *end_fx) {
    /* 必须是不同类型的分型 */
    if (start_fx->type == end_fx->type) return 0;

    /* 方向检查 */
    if (start_fx->type == FX_BOTTOM && end_fx->type == FX_TOP) {
        /* 上升笔：底分型在前，顶分型在后 */
        if (end_fx->mid_idx <= start_fx->mid_idx) return 0;
        /* 底分型的low应低于顶分型的high */
        if (start_fx->low >= end_fx->high) return 0;
    } else if (start_fx->type == FX_TOP && end_fx->type == FX_BOTTOM) {
        /* 下降笔：顶分型在前，底分型在后 */
        if (end_fx->mid_idx <= start_fx->mid_idx) return 0;
        if (start_fx->high <= end_fx->low) return 0;
    } else {
        return 0;
    }

    /* 分型验证 */
    if (!bi_fx_check_valid(s, start_fx, s->merged)) return 0;
    if (!bi_fx_check_valid(s, end_fx, s->merged)) return 0;

    /* 跨度检查 */
    int span = end_fx->end - start_fx->begin + 1;
    int bar_span = 0;
    /* 计算原始K线数 */
    if (start_fx->begin >= 0 && start_fx->begin < s->merged_count &&
        end_fx->end >= 0 && end_fx->end < s->merged_count) {
        bar_span = s->merged[end_fx->end].end - s->merged[start_fx->begin].begin + 1;
    }

    int has_gap = 0;
    if (s->config.gap_as_kl) {
        /* 检查是否有缺口 */
        for (int i = start_fx->end + 1; i <= end_fx->begin - 1; i++) {
            if (bi_is_gap(s, i)) {
                has_gap = 1;
                break;
            }
        }
    }

    if (s->config.bi_strict) {
        /* 严格模式：合并K线span >= 4 */
        if (has_gap) {
            /* 有缺口时可降低要求 */
            if (span < 3) return 0;
        } else {
            if (span < 4) return 0;
        }
    } else {
        /* 非严格模式：span >= 3 且 原始K线 >= 3 */
        if (span < 3) return 0;
        if (bar_span < 3) return 0;
    }

    return 1;
}

/* ============================================================
 * 创建笔
 * ============================================================ */
static void create_bi(ChanState *s, const Fractal *start_fx, const Fractal *end_fx) {
    if (s->bi_count >= MAX_BI) return;

    Bi *b = &s->bi_list[s->bi_count];

    if (start_fx->type == FX_BOTTOM) {
        b->dir = DIR_UP;
    } else {
        b->dir = DIR_DOWN;
    }

    b->start_fx  = *start_fx;
    b->end_fx    = *end_fx;
    b->begin     = start_fx->begin;
    b->end       = end_fx->end;
    b->bar_begin = s->merged[start_fx->begin].begin;
    b->bar_end   = s->merged[end_fx->end].end;
    b->high      = float_max(start_fx->high, end_fx->high);
    b->low       = float_min(start_fx->low, end_fx->low);
    b->amp       = b->high - b->low;
    b->is_sure   = 1;

    s->bi_count++;
}

/* ============================================================
 * 笔构建主函数
 *
 * chan.py 的 Bi_list 构建逻辑：
 * 1. 用分型列表中的第一个分型开始
 * 2. 寻找匹配的结束分型
 * 3. 如果找到更极端的同类分型，替换起点
 * ============================================================ */
void chan_bi_build(ChanState *s) {
    s->bi_count = 0;
    int nf = s->fractal_count;
    if (nf < 2) return;

    Fractal *fx = s->fractals;

    /* 第一个分型作为候选起点 */
    int start_idx = 0;

    while (start_idx < nf - 1) {
        Fractal *start_fx = &fx[start_idx];
        int best_end = -1;
        int best_start = start_idx;

        /* 寻找匹配的结束分型 */
        for (int j = start_idx + 1; j < nf; j++) {
            Fractal *end_fx = &fx[j];

            /* 如果遇到同类型分型，比较极值 */
            if (end_fx->type == start_fx->type) {
                if (start_fx->type == FX_TOP) {
                    /* 更高的顶分型，替换起点 */
                    if (end_fx->high > start_fx->high) {
                        best_start = j;
                        start_fx = &fx[j];
                    }
                } else {
                    /* 更低的底分型，替换起点 */
                    if (end_fx->low < start_fx->low) {
                        best_start = j;
                        start_fx = &fx[j];
                    }
                }
                /* 更新start_idx后重新搜索 */
                continue;
            }

            /* 不同类型分型，检查能否成笔 */
            if (can_form_bi(s, start_fx, end_fx)) {
                best_end = j;
                /* chan.py: 不立即break，继续找可能更合适的终点 */
                /* 但如果后面有同类型的更极端的分型，可能会替换 */
                /* 简化处理：找到第一个有效终点就使用 */
                break;
            }
        }

        if (best_end >= 0 && best_start >= 0) {
            create_bi(s, &fx[best_start], &fx[best_end]);
            start_idx = best_end;
        } else {
            /* 没找到匹配，移到下一个分型 */
            start_idx++;
            if (best_start > start_idx) {
                start_idx = best_start;
            }
        }
    }

    /* bi_end_is_peak 验证 */
    if (s->config.bi_end_is_peak) {
        /* 逐笔验证，不满足则回退 */
        for (int i = s->bi_count - 1; i >= 0; i--) {
            if (!bi_check_peak(s, &s->bi_list[i])) {
                /* 移除该笔及之后的笔 */
                s->bi_count = i;
            }
        }
    }
}
