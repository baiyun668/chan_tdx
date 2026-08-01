/*
 * bsp.c - 买卖点识别
 *
 * 对齐 chan.py 的 BSPoint 模块
 *
 * 一类买卖点(T1)：线段末端 + 背驰
 *   - 背驰：出中枢笔MACD力度 <= 进中枢笔MACD力度 * divergence_rate
 *   - bs1_peak: 要求突破中枢极值
 *
 * 一类盘整买卖点(T1P)：无中枢时同向相邻笔背驰
 *
 * 二类买卖点(T2)：一类之后的第一次回调/反弹
 *   - retrace_rate = bsp2_bi.amp / break_bi.amp <= max_bs2_rate
 *
 * 类二买卖点(T2S)：二类之后的类似形态
 *
 * 三类买卖点(T3A/T3B)：中枢后的突破
 *   - T3A: 中枢在一买后面
 *   - T3B: 中枢在一买前面
 */

#include "bsp.h"
#include "macd.h"

/* ============================================================
 * 添加买卖点
 * ============================================================ */
static void add_bsp(ChanState *s, BspType type, int bi_idx, float price) {
    if (s->bsp_count >= MAX_BSP) return;
    Bsp *b = &s->bsp_list[s->bsp_count];
    b->type    = type;
    b->bi_idx  = bi_idx;
    b->bar_idx = s->bi_list[bi_idx].bar_end;
    b->price   = price;
    s->bsp_count++;
}

/* ============================================================
 * 背驰判断
 *
 * 比较两笔的MACD力度
 * power_out <= power_in * divergence_rate => 背驰
 * ============================================================ */
static int is_divergence(ChanState *s, int bi_out_idx, int bi_in_idx) {
    float power_out = bi_macd_power(s, bi_out_idx);
    float power_in  = bi_macd_power(s, bi_in_idx);
    if (power_in <= 0) return 0;
    return (power_out <= power_in * s->config.divergence_rate);
}

/* ============================================================
 * 寻找笔所在的中枢
 * 返回中枢索引，-1=不在任何中枢
 * ============================================================ */
static int find_zs_for_bi(ChanState *s, int bi_idx) {
    for (int i = 0; i < s->zs_count; i++) {
        ZhongShu *zs = &s->zs_list[i];
        if (bi_idx >= zs->bi_begin && bi_idx <= zs->bi_end) {
            return i;
        }
    }
    return -1;
}

/* ============================================================
 * 寻找笔之前最近的中枢
 * ============================================================ */
static int find_zs_before_bi(ChanState *s, int bi_idx) {
    int best = -1;
    for (int i = 0; i < s->zs_count; i++) {
        if (s->zs_list[i].bi_end < bi_idx) {
            if (best < 0 || s->zs_list[i].bi_end > s->zs_list[best].bi_end) {
                best = i;
            }
        }
    }
    return best;
}

/* ============================================================
 * 寻找笔之后最近的中枢
 * ============================================================ */
static int find_zs_after_bi(ChanState *s, int bi_idx) {
    int best = -1;
    for (int i = 0; i < s->zs_count; i++) {
        if (s->zs_list[i].bi_begin > bi_idx) {
            if (best < 0 || s->zs_list[i].bi_begin < s->zs_list[best].bi_begin) {
                best = i;
            }
        }
    }
    return best;
}

/* ============================================================
 * 一类买点检测
 *
 * 条件：
 * 1. 下降笔（或上升线段末端的下降笔）
 * 2. 笔在中枢下方或中枢中
 * 3. 背驰：与进入中枢的同向笔比较MACD力度
 * 4. bs1_peak: 笔低点需低于中枢DD
 * ============================================================ */
static void detect_bsp_t1_buy(ChanState *s) {
    int nb = s->bi_count;
    if (nb < 3) return;

    /* 遍历所有下降笔（找买点） */
    for (int i = 2; i < nb; i++) {
        Bi *b = &s->bi_list[i];
        if (b->dir != DIR_DOWN) continue;

        /* 寻找笔之前的中枢 */
        int zs_idx = find_zs_before_bi(s, i);
        if (zs_idx < 0) {
            /* 无中枢：检查盘整背驰 T1P */
            /* 找前一个同向（下降）笔 */
            int prev_down = -1;
            for (int j = i - 2; j >= 0; j -= 2) {
                if (s->bi_list[j].dir == DIR_DOWN) {
                    prev_down = j;
                    break;
                }
            }
            if (prev_down >= 0) {
                if (is_divergence(s, i, prev_down)) {
                    /* 一买盘整：当前笔低点更低但MACD力度更小 */
                    if (b->low < s->bi_list[prev_down].low) {
                        add_bsp(s, BSP_BUY1P, i, b->low);
                    }
                }
            }
            continue;
        }

        ZhongShu *zs = &s->zs_list[zs_idx];

        /* 当前笔应该在中枢下方或从中枢出去 */
        /* 找中枢的进入笔 (bi_in) */
        int bi_in = zs->bi_in;
        if (bi_in < 0 || bi_in >= nb) continue;

        /* 进入笔应该是下降笔（进入中枢的下降笔） */
        if (s->bi_list[bi_in].dir != DIR_DOWN) continue;

        /* 检查背驰 */
        if (!is_divergence(s, i, bi_in)) continue;

        /* bs1_peak: 要求笔的低点突破中枢极值 */
        if (s->config.bs1_peak) {
            if (b->low >= zs->dd) continue;
        }

        add_bsp(s, BSP_BUY1, i, b->low);
    }
}

/* ============================================================
 * 一类卖点检测
 * ============================================================ */
static void detect_bsp_t1_sell(ChanState *s) {
    int nb = s->bi_count;
    if (nb < 3) return;

    for (int i = 2; i < nb; i++) {
        Bi *b = &s->bi_list[i];
        if (b->dir != DIR_UP) continue;

        int zs_idx = find_zs_before_bi(s, i);
        if (zs_idx < 0) {
            /* 无中枢：检查盘整背驰 T1P */
            int prev_up = -1;
            for (int j = i - 2; j >= 0; j -= 2) {
                if (s->bi_list[j].dir == DIR_UP) {
                    prev_up = j;
                    break;
                }
            }
            if (prev_up >= 0) {
                if (is_divergence(s, i, prev_up)) {
                    if (b->high > s->bi_list[prev_up].high) {
                        add_bsp(s, BSP_SELL1P, i, b->high);
                    }
                }
            }
            continue;
        }

        ZhongShu *zs = &s->zs_list[zs_idx];

        int bi_in = zs->bi_in;
        if (bi_in < 0 || bi_in >= nb) continue;
        if (s->bi_list[bi_in].dir != DIR_UP) continue;

        if (!is_divergence(s, i, bi_in)) continue;

        if (s->config.bs1_peak) {
            if (b->high <= zs->gg) continue;
        }

        add_bsp(s, BSP_SELL1, i, b->high);
    }
}

/* ============================================================
 * 二类买卖点检测
 *
 * 一类买点之后的第一次回调（上升笔之后的下降笔）
 * 条件：回调幅度 / 突破幅度 <= max_bs2_rate
 * ============================================================ */
static void detect_bsp_t2(ChanState *s) {
    int nb = s->bi_count;

    for (int bi = 0; bi < s->bsp_count; bi++) {
        Bsp *bsp = &s->bsp_list[bi];

        if (bsp->type == BSP_BUY1 || bsp->type == BSP_BUY1P) {
            /* 一买之后找二买 */
            int break_bi = bsp->bi_idx;  /* 一买对应的笔 */
            if (break_bi + 2 >= nb) continue;

            /* 一买笔之后应该是上升笔，再之后是下降笔（回调） */
            Bi *up_bi   = &s->bi_list[break_bi + 1];
            Bi *retrace = &s->bi_list[break_bi + 2];

            if (up_bi->dir != DIR_UP || retrace->dir != DIR_DOWN) continue;

            /* 回调幅度不超过突破幅度 */
            float break_amp = up_bi->amp;
            float retrace_amp = retrace->amp;
            if (break_amp <= 0) continue;

            float rate = retrace_amp / break_amp;
            if (rate <= s->config.max_bs2_rate) {
                add_bsp(s, BSP_BUY2, break_bi + 2, retrace->low);
            }
        }
        else if (bsp->type == BSP_SELL1 || bsp->type == BSP_SELL1P) {
            /* 一卖之后找二卖 */
            int break_bi = bsp->bi_idx;
            if (break_bi + 2 >= nb) continue;

            Bi *down_bi = &s->bi_list[break_bi + 1];
            Bi *retrace = &s->bi_list[break_bi + 2];

            if (down_bi->dir != DIR_DOWN || retrace->dir != DIR_UP) continue;

            float break_amp = down_bi->amp;
            float retrace_amp = retrace->amp;
            if (break_amp <= 0) continue;

            float rate = retrace_amp / break_amp;
            if (rate <= s->config.max_bs2_rate) {
                add_bsp(s, BSP_SELL2, break_bi + 2, retrace->high);
            }
        }
    }
}

/* ============================================================
 * 类二买卖点检测 (T2S)
 *
 * 二买之后的类似形态
 * ============================================================ */
static void detect_bsp_t2s(ChanState *s) {
    int nb = s->bi_count;
    int bsc = s->bsp_count;

    for (int bi = 0; bi < bsc; bi++) {
        Bsp *bsp = &s->bsp_list[bi];

        if (bsp->type == BSP_BUY2) {
            /* 二买之后找类二买 */
            int t2_bi = bsp->bi_idx;
            if (t2_bi + 2 >= nb) continue;

            Bi *up_bi   = &s->bi_list[t2_bi + 1];
            Bi *retrace = &s->bi_list[t2_bi + 2];

            if (up_bi->dir != DIR_UP || retrace->dir != DIR_DOWN) continue;

            float break_amp = up_bi->amp;
            float retrace_amp = retrace->amp;
            if (break_amp <= 0) continue;

            float rate = retrace_amp / break_amp;
            if (rate <= s->config.max_bs2_rate) {
                add_bsp(s, BSP_BUY2S, t2_bi + 2, retrace->low);
            }
        }
        else if (bsp->type == BSP_SELL2) {
            int t2_bi = bsp->bi_idx;
            if (t2_bi + 2 >= nb) continue;

            Bi *down_bi = &s->bi_list[t2_bi + 1];
            Bi *retrace = &s->bi_list[t2_bi + 2];

            if (down_bi->dir != DIR_DOWN || retrace->dir != DIR_UP) continue;

            float break_amp = down_bi->amp;
            float retrace_amp = retrace->amp;
            if (break_amp <= 0) continue;

            float rate = retrace_amp / break_amp;
            if (rate <= s->config.max_bs2_rate) {
                add_bsp(s, BSP_SELL2S, t2_bi + 2, retrace->high);
            }
        }
    }
}

/* ============================================================
 * 三类买卖点检测
 *
 * T3A: 中枢在一买后面 - 笔向上突破中枢后回踩不回中枢
 * T3B: 中枢在一买前面 - 笔向下突破中枢后反弹不回中枢
 *
 * 通用规则：
 * - 离开中枢的笔后，下一笔回调/反弹不进入中枢 [ZD, ZG]
 * - 买点：离开中枢的上升笔 + 回调不破ZD
 * - 卖点：离开中枢的下降笔 + 反弹不破ZG
 * ============================================================ */
static void detect_bsp_t3(ChanState *s) {
    int nb = s->bi_count;

    for (int zs_idx = 0; zs_idx < s->zs_count; zs_idx++) {
        ZhongShu *zs = &s->zs_list[zs_idx];

        /* 找中枢之后的笔 */
        int out_bi = zs->bi_end + 1;
        if (out_bi >= nb) continue;

        Bi *break_bi = &s->bi_list[out_bi];

        /* 检查离开中枢的笔 */
        if (break_bi->dir == DIR_UP) {
            /* 向上离开中枢，检查是否形成三买 */
            if (break_bi->high <= zs->zg) continue; /* 没有真正突破 */

            /* 回调笔 */
            if (out_bi + 1 >= nb) continue;
            Bi *retrace_bi = &s->bi_list[out_bi + 1];
            if (retrace_bi->dir != DIR_DOWN) continue;

            /* 回调不进入中枢 */
            if (retrace_bi->low >= zs->zd) {
                /* 判断T3A还是T3B */
                /* T3A: 中枢在一买后面（中枢后面离开） */
                /* T3B: 中枢在一买前面 */
                int bsp_type = BSP_BUY3A; /* 默认T3A */
                add_bsp(s, bsp_type, out_bi + 1, retrace_bi->low);
            }
        }
        else if (break_bi->dir == DIR_DOWN) {
            /* 向下离开中枢，检查是否形成三卖 */
            if (break_bi->low >= zs->zd) continue;

            if (out_bi + 1 >= nb) continue;
            Bi *retrace_bi = &s->bi_list[out_bi + 1];
            if (retrace_bi->dir != DIR_UP) continue;

            if (retrace_bi->high <= zs->zg) {
                int bsp_type = BSP_SELL3A;
                add_bsp(s, bsp_type, out_bi + 1, retrace_bi->high);
            }
        }
    }
}

/* ============================================================
 * 买卖点识别主函数
 * ============================================================ */
void chan_bsp_find(ChanState *s) {
    s->bsp_count = 0;

    /* 需要MACD数据 */
    chan_macd_compute(s);

    /* 一类买卖点 */
    detect_bsp_t1_buy(s);
    detect_bsp_t1_sell(s);

    /* 二类买卖点 */
    detect_bsp_t2(s);

    /* 类二买卖点 */
    detect_bsp_t2s(s);

    /* 三类买卖点 */
    detect_bsp_t3(s);
}
