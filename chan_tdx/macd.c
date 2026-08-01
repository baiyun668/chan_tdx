/*
 * macd.c - MACD计算
 *
 * 标准MACD算法：
 *   DIFF = EMA(close, fast) - EMA(close, slow)
 *   DEA  = EMA(DIFF, signal)
 *   MACD = 2 * (DIFF - DEA)
 */

#include "macd.h"

/* EMA递推计算 */
static void calc_ema(const float *src, float *dst, int n, int period) {
    if (n <= 0 || period <= 0) return;

    float alpha = 2.0f / (period + 1);
    dst[0] = src[0];
    for (int i = 1; i < n; i++) {
        dst[i] = alpha * src[i] + (1.0f - alpha) * dst[i - 1];
    }
}

void chan_macd_compute(ChanState *s) {
    int n = s->bar_count;
    if (n <= 0) return;

    int fast   = s->config.macd_fast;
    int slow   = s->config.macd_slow;
    int signal = s->config.macd_signal;

    /* 提取收盘价 */
    static float close[MAX_BARS];
    for (int i = 0; i < n; i++) {
        close[i] = s->bars[i].close;
    }

    /* 计算快速EMA和慢速EMA */
    static float ema_fast[MAX_BARS];
    static float ema_slow[MAX_BARS];
    calc_ema(close, ema_fast, n, fast);
    calc_ema(close, ema_slow, n, slow);

    /* DIFF = EMA(fast) - EMA(slow) */
    for (int i = 0; i < n; i++) {
        s->diff[i] = ema_fast[i] - ema_slow[i];
    }

    /* DEA = EMA(DIFF, signal) */
    calc_ema(s->diff, s->dea, n, signal);

    /* MACD = 2 * (DIFF - DEA) */
    for (int i = 0; i < n; i++) {
        s->macd_val[i] = 2.0f * (s->diff[i] - s->dea[i]);
    }
}

/*
 * MACD力度计算
 *
 * algo:
 *   MACD_AREA      (0): 半区域 - 只计算与零轴同侧的面积
 *   MACD_PEAK      (1): 峰值 - 取绝对值最大的MACD
 *   MACD_FULL_AREA (2): 全区域 - 全部MACD面积
 */
float macd_power_range(ChanState *s, int bar_begin, int bar_end, int algo) {
    if (bar_begin < 0 || bar_end < 0 || bar_begin > bar_end) return 0.0f;
    if (bar_end >= s->bar_count) bar_end = s->bar_count - 1;

    float power = 0.0f;

    switch (algo) {
    case MACD_AREA: {
        /* 半区域：只累加与趋势方向同侧的MACD值 */
        /* 判断趋势方向：看首尾MACD符号 */
        float sum_sign = 0.0f;
        for (int i = bar_begin; i <= bar_end; i++) {
            sum_sign += s->macd_val[i];
        }
        float sign = (sum_sign >= 0) ? 1.0f : -1.0f;
        for (int i = bar_begin; i <= bar_end; i++) {
            if (s->macd_val[i] * sign > 0) {
                power += s->macd_val[i] * sign;
            }
        }
        break;
    }
    case MACD_PEAK: {
        /* 峰值：取MACD绝对值的最大值 */
        for (int i = bar_begin; i <= bar_end; i++) {
            float v = float_abs(s->macd_val[i]);
            if (v > power) power = v;
        }
        break;
    }
    case MACD_FULL_AREA: {
        /* 全区域：累加所有MACD值的绝对值 */
        for (int i = bar_begin; i <= bar_end; i++) {
            power += float_abs(s->macd_val[i]);
        }
        break;
    }
    default:
        break;
    }

    return power;
}

/*
 * 获取某笔的MACD力度
 * 使用笔的bar_begin到bar_end区间
 */
float bi_macd_power(ChanState *s, int bi_idx) {
    if (bi_idx < 0 || bi_idx >= s->bi_count) return 0.0f;
    Bi *b = &s->bi_list[bi_idx];
    return macd_power_range(s, b->bar_begin, b->bar_end, s->config.macd_algo);
}
