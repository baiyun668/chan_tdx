/*
 * seg.c - 线段构建（特征序列法）
 *
 * 对齐 chan.py 的 Segment 模块
 *
 * 算法：
 * 1. 上升线段中取下降笔构成特征序列，下降线段中取上升笔构成特征序列
 * 2. 特征序列元素 = (high, low)，high=笔的起点价, low=笔的终点价（或反之）
 * 3. 特征序列支持包含关系合并
 * 4. 合并后的特征序列中出现分型则线段结束
 * 5. 需要验证 actual_break（实际突破）
 * 6. 线段至少包含3笔
 */

#include "seg.h"

/* ============================================================
 * 特征序列元素
 * ============================================================ */
typedef struct {
    float high;
    float low;
    int   bi_idx;   /* 对应的笔索引 */
    int   merged;   /* 是否被合并 */
} EigenElement;

/* 特征序列最大长度 */
#define MAX_EIGEN MAX_BI

/* ============================================================
 * 构建特征序列
 *
 * 对于上升线段(dir=UP)：取其中的下降笔(DIR_DOWN)作为特征序列
 * 对于下降线段(dir=DOWN)：取其中的上升笔(DIR_UP)作为特征序列
 *
 * 特征序列元素的high/low由笔的起止价格决定
 * ============================================================ */
static int build_eigen_sequence(ChanState *s, int bi_start, int bi_end,
                                 Direction seg_dir, EigenElement *eigen) {
    int count = 0;
    for (int i = bi_start; i <= bi_end && i < s->bi_count; i++) {
        Bi *b = &s->bi_list[i];
        /* 选取与线段方向相反的笔 */
        if ((seg_dir == DIR_UP && b->dir == DIR_DOWN) ||
            (seg_dir == DIR_DOWN && b->dir == DIR_UP)) {
            if (count >= MAX_EIGEN) break;
            eigen[count].bi_idx = i;
            eigen[count].merged = 0;

            if (seg_dir == DIR_UP) {
                /* 上升线段的下降笔：high=笔起始价, low=笔结束价 */
                eigen[count].high = b->start_fx.high;
                eigen[count].low  = b->end_fx.low;
            } else {
                /* 下降线段的上升笔：high=笔结束价, low=笔起始价 */
                eigen[count].high = b->end_fx.high;
                eigen[count].low  = b->start_fx.low;
            }
            count++;
        }
    }
    return count;
}

/* ============================================================
 * 特征序列包含关系合并
 * ============================================================ */
static int merge_eigen(EigenElement *eigen, int count) {
    if (count <= 1) return count;

    int merged_count = 1;
    for (int i = 1; i < count; i++) {
        EigenElement *last = &eigen[merged_count - 1];
        EigenElement *curr = &eigen[i];

        /* 检查包含关系 */
        int inc = 0;
        if (last->high >= curr->high && last->low <= curr->low) {
            inc = 1; /* last包含curr */
        } else if (curr->high >= last->high && curr->low <= last->low) {
            inc = -1; /* curr包含last */
        }

        if (inc != 0) {
            /* 确定合并方向 */
            Direction dir;
            if (merged_count >= 2) {
                EigenElement *prev = &eigen[merged_count - 2];
                if (last->high > prev->high) {
                    dir = DIR_UP;
                } else {
                    dir = DIR_DOWN;
                }
            } else {
                if (curr->high >= last->high) {
                    dir = DIR_UP;
                } else {
                    dir = DIR_DOWN;
                }
            }

            if (dir == DIR_UP) {
                last->high = float_max(last->high, curr->high);
                last->low  = float_max(last->low, curr->low);
            } else {
                last->high = float_min(last->high, curr->high);
                last->low  = float_min(last->low, curr->low);
            }
            /* bi_idx 保留last的 */
        } else {
            if (merged_count != i) {
                eigen[merged_count] = *curr;
            }
            merged_count++;
        }
    }
    return merged_count;
}

/* ============================================================
 * 在特征序列中寻找分型
 *
 * 返回分型的中间元素索引，-1=未找到
 * ============================================================ */
static int find_eigen_fractal(EigenElement *eigen, int count, Direction seg_dir) {
    if (count < 3) return -1;

    for (int i = 1; i < count - 1; i++) {
        EigenElement *left  = &eigen[i - 1];
        EigenElement *mid   = &eigen[i];
        EigenElement *right = &eigen[i + 1];

        if (seg_dir == DIR_UP) {
            /* 上升线段结束于顶分型 */
            if (mid->high > left->high && mid->high > right->high &&
                mid->low > left->low && mid->low > right->low) {
                return i;
            }
        } else {
            /* 下降线段结束于底分型 */
            if (mid->low < left->low && mid->low < right->low &&
                mid->high < left->high && mid->high < right->high) {
                return i;
            }
        }
    }
    return -1;
}

/* ============================================================
 * 验证 actual_break（实际突破）
 *
 * chan.py 中要求特征序列分型确认后，
 * 需要后续笔的价格实际突破确认
 * ============================================================ */
static int verify_actual_break(ChanState *s, int eigen_mid_bi_idx,
                                Direction seg_dir, int bi_end_limit) {
    if (eigen_mid_bi_idx < 1) return -1;

    /* 检查分型之后的笔是否有实际突破 */
    int check_start = eigen_mid_bi_idx + 1;

    /* 获取特征序列分型中间元素对应笔的前一笔（特征序列的前一个元素） */
    Bi *prev_bi = &s->bi_list[eigen_mid_bi_idx - 1];

    for (int i = check_start; i <= bi_end_limit && i < s->bi_count; i++) {
        Bi *b = &s->bi_list[i];
        if (seg_dir == DIR_UP) {
            /* 上升线段结束后，后续下降笔的低点应突破前一个下降笔的低点 */
            if (b->dir == DIR_DOWN && b->end_fx.low < prev_bi->end_fx.low) {
                return i;
            }
        } else {
            /* 下降线段结束后，后续上升笔的高点应突破前一个上升笔的高点 */
            if (b->dir == DIR_UP && b->end_fx.high > prev_bi->end_fx.high) {
                return i;
            }
        }
    }
    return -1;
}

/* ============================================================
 * 线段构建主函数
 * ============================================================ */
void chan_seg_build(ChanState *s) {
    s->seg_count = 0;
    int nb = s->bi_count;
    if (nb < 3) return;

    int bi_start = 0;

    while (bi_start < nb - 2) {
        Bi *first_bi = &s->bi_list[bi_start];

        /* 确定线段方向：由第一笔决定 */
        Direction seg_dir = first_bi->dir;

        /* 尝试找到线段的结束位置 */
        int seg_end_bi = -1;

        /* 从至少3笔开始检查 */
        for (int bi_end = bi_start + 2; bi_end < nb; bi_end++) {
            /* 构建特征序列 */
            static EigenElement eigen[MAX_EIGEN];
            int eigen_count = build_eigen_sequence(s, bi_start, bi_end, seg_dir, eigen);
            if (eigen_count < 3) continue;

            /* 合并特征序列 */
            eigen_count = merge_eigen(eigen, eigen_count);
            if (eigen_count < 3) continue;

            /* 寻找分型 */
            int fx_idx = find_eigen_fractal(eigen, eigen_count, seg_dir);
            if (fx_idx < 0) continue;

            /* 找到分型，验证 actual_break */
            int eigen_bi_idx = eigen[fx_idx].bi_idx;
            int break_bi = verify_actual_break(s, eigen_bi_idx, seg_dir, bi_end);

            if (break_bi >= 0) {
                /* 线段结束于分型对应的笔 */
                seg_end_bi = eigen_bi_idx;
                break;
            }
        }

        if (seg_end_bi < 0) {
            /* 没找到线段结束，整个序列作为一个不确定的线段 */
            if (nb - bi_start >= 3) {
                if (s->seg_count < MAX_SEG) {
                    Segment *seg = &s->seg_list[s->seg_count];
                    seg->dir       = seg_dir;
                    seg->bi_begin  = bi_start;
                    seg->bi_end    = nb - 1;
                    seg->bar_begin = s->bi_list[bi_start].bar_begin;
                    seg->bar_end   = s->bi_list[nb - 1].bar_end;

                    /* 计算高低点 */
                    seg->high = s->bi_list[bi_start].low;
                    seg->low  = s->bi_list[bi_start].high;
                    for (int i = bi_start; i <= nb - 1; i++) {
                        if (s->bi_list[i].high > seg->high) seg->high = s->bi_list[i].high;
                        if (s->bi_list[i].low < seg->low)   seg->low  = s->bi_list[i].low;
                    }
                    seg->is_sure = 0;
                    s->seg_count++;
                }
            }
            break;
        }

        /* 记录确认的线段 */
        if (s->seg_count >= MAX_SEG) break;

        Segment *seg = &s->seg_list[s->seg_count];
        seg->dir       = seg_dir;
        seg->bi_begin  = bi_start;
        seg->bi_end    = seg_end_bi;
        seg->bar_begin = s->bi_list[bi_start].bar_begin;
        seg->bar_end   = s->bi_list[seg_end_bi].bar_end;

        /* 计算高低点 */
        seg->high = s->bi_list[bi_start].low;
        seg->low  = s->bi_list[bi_start].high;
        for (int i = bi_start; i <= seg_end_bi; i++) {
            if (s->bi_list[i].high > seg->high) seg->high = s->bi_list[i].high;
            if (s->bi_list[i].low < seg->low)   seg->low  = s->bi_list[i].low;
        }
        seg->is_sure = 1;
        s->seg_count++;

        /* 下一线段从结束笔开始 */
        bi_start = seg_end_bi;
    }
}
