/*
 * main.c - DLL入口、TDX接口函数、RegisterFunc
 *
 * 通达信DLL插件规范：
 *   函数签名: float __stdcall func(int DataLen, float* pfOUT, float* pfINa, float* pfINb, float* pfINc)
 *   DataLen: 数据长度
 *   pfOUT: 输出数组
 *   pfINa: HIGH数组, pfINb: LOW数组, pfINc: CLOSE数组（可复用传参）
 *
 * RegisterFunc: DLL导出函数，注册TDX可用的函数
 */

#include "chan_tdx.h"
#include "macd.h"
#include "kline.h"
#include "bi.h"
#include "seg.h"
#include "zs.h"
#include "bsp.h"

#ifdef _WIN32
#include <windows.h>
#endif

/* ============================================================
 * 全局状态实例
 * ============================================================ */
ChanState g_state;

/* ============================================================
 * 从TDX输入初始化状态
 *
 * TDX数据：从旧到新（index 0 = 最老的K线）
 * pfINa = HIGH, pfINb = LOW, pfINc = CLOSE
 *
 * 注意：pfINc 可能被复用为参数输入
 * 参数编码方案：当 DataLen < 0 时，pfINc 前几个元素为参数
 * ============================================================ */
static void init_from_tdx(ChanState *s, int DataLen,
                           float *pfINa, float *pfINb, float *pfINc,
                           int has_params) {
    /* 默认配置 */
    chan_config_default(&s->config);

    int n = DataLen;
    if (n < 0) n = -n;

    if (n > MAX_BARS) n = MAX_BARS;
    s->bar_count = n;

    /* 解析参数（如果有的话） */
    int param_offset = 0;
    if (has_params && pfINc) {
        /*
         * 参数编码：
         * pfINc[0] = bi_strict (0/1)
         * pfINc[1] = bi_fx_check (0-3)
         * pfINc[2] = gap_as_kl (0/1)
         * pfINc[3] = bi_end_is_peak (0/1)
         * pfINc[4] = seg_algo (0)
         * pfINc[5] = divergence_rate (float)
         * pfINc[6] = macd_algo (0-2)
         * pfINc[7] = max_bs2_rate (float)
         * pfINc[8] = bs1_peak (0/1)
         * pfINc[9] = zs_combine_mode (0-2)
         * pfINc[10] = one_bi_zs (0/1)
         * pfINc[11] = exclude_included (0/1)
         * pfINc[12] = bi_allow_sub_peak (0/1)
         * pfINc[13] = macd_fast
         * pfINc[14] = macd_slow
         * pfINc[15] = macd_signal
         *
         * 参数结束标记：pfINc[i] == -999.0
         */
        if (pfINc[0] != -999.0f) s->config.bi_strict         = (int)pfINc[0];
        if (pfINc[1] != -999.0f) s->config.bi_fx_check       = (int)pfINc[1];
        if (pfINc[2] != -999.0f) s->config.gap_as_kl         = (int)pfINc[2];
        if (pfINc[3] != -999.0f) s->config.bi_end_is_peak    = (int)pfINc[3];
        if (pfINc[4] != -999.0f) s->config.seg_algo          = (int)pfINc[4];
        if (pfINc[5] != -999.0f) s->config.divergence_rate   = pfINc[5];
        if (pfINc[6] != -999.0f) s->config.macd_algo         = (int)pfINc[6];
        if (pfINc[7] != -999.0f) s->config.max_bs2_rate      = pfINc[7];
        if (pfINc[8] != -999.0f) s->config.bs1_peak          = (int)pfINc[8];
        if (pfINc[9] != -999.0f) s->config.zs_combine_mode   = (int)pfINc[9];
        if (pfINc[10] != -999.0f) s->config.one_bi_zs        = (int)pfINc[10];
        if (pfINc[11] != -999.0f) s->config.exclude_included = (int)pfINc[11];
        if (pfINc[12] != -999.0f) s->config.bi_allow_sub_peak = (int)pfINc[12];
        if (pfINc[13] != -999.0f) s->config.macd_fast        = (int)pfINc[13];
        if (pfINc[14] != -999.0f) s->config.macd_slow        = (int)pfINc[14];
        if (pfINc[15] != -999.0f) s->config.macd_signal      = (int)pfINc[15];
        param_offset = 16;
    }

    /* 填充K线数据 */
    for (int i = 0; i < n; i++) {
        s->bars[i].high  = pfINa ? pfINa[i] : 0;
        s->bars[i].low   = pfINb ? pfINb[i] : 0;
        s->bars[i].close = pfINc ? pfINc[param_offset + i] : 0;
        s->bars[i].open  = s->bars[i].close; /* TDX不传open，用close代替 */
        s->bars[i].vol   = 0;
        s->bars[i].index = i;
    }

    s->computed = 0;
}

/* ============================================================
 * 从TDX输入初始化（使用默认配置，pfINc纯数据版）
 * ============================================================ */
static void init_from_tdx_simple(ChanState *s, int DataLen,
                                  float *pfINa, float *pfINb, float *pfINc) {
    chan_config_default(&s->config);

    int n = DataLen;
    if (n < 0) n = -n;
    if (n > MAX_BARS) n = MAX_BARS;
    s->bar_count = n;

    for (int i = 0; i < n; i++) {
        s->bars[i].high  = pfINa ? pfINa[i] : 0;
        s->bars[i].low   = pfINb ? pfINb[i] : 0;
        s->bars[i].close = pfINc ? pfINc[i] : 0;
        s->bars[i].open  = s->bars[i].close;
        s->bars[i].vol   = 0;
        s->bars[i].index = i;
    }

    s->computed = 0;
}

/* ============================================================
 * 确保计算完成
 * ============================================================ */
static void ensure_computed(ChanState *s) {
    if (s->computed) return;
    chan_compute_all(s);
    s->computed = 1;
}

/* ============================================================
 * 主计算入口
 * ============================================================ */
void chan_compute_all(ChanState *s) {
    /* 1. MACD计算 */
    chan_macd_compute(s);

    /* 2. K线合并 */
    chan_kline_merge(s);

    /* 3. 分型识别 */
    chan_fractal_find(s);

    /* 4. 笔构建 */
    chan_bi_build(s);

    /* 5. 线段构建 */
    chan_seg_build(s);

    /* 6. 中枢构建 */
    chan_zs_build(s);

    /* 7. 买卖点识别 */
    chan_bsp_find(s);
}

/* ============================================================
 * TDX 函数: ZEN_BI_FRAC - 分型标记
 *
 * 输出：1=顶分型, -1=底分型, 0=无
 * ============================================================ */
float STDCALL ZEN_BI_FRAC(int DataLen, float *pfOUT,
                                  float *pfINa, float *pfINb, float *pfINc) {
    init_from_tdx_simple(&g_state, DataLen, pfINa, pfINb, pfINc);
    ensure_computed(&g_state);

    /* 清零输出 */
    for (int i = 0; i < DataLen && i < MAX_BARS; i++) {
        pfOUT[i] = 0.0f;
    }

    /* 标记分型 */
    for (int i = 0; i < g_state.fractal_count; i++) {
        Fractal *fx = &g_state.fractals[i];
        int bar_idx = fx->bar_idx;
        if (bar_idx >= 0 && bar_idx < DataLen) {
            pfOUT[bar_idx] = (float)fx->type;
        }
    }

    return 0.0f;
}

/* ============================================================
 * TDX 函数: ZEN_BI - 笔端点
 *
 * 输出：1=笔顶点, -1=笔底点, 0=非笔端点
 * ============================================================ */
float STDCALL ZEN_BI(int DataLen, float *pfOUT,
                             float *pfINa, float *pfINb, float *pfINc) {
    init_from_tdx_simple(&g_state, DataLen, pfINa, pfINb, pfINc);
    ensure_computed(&g_state);

    for (int i = 0; i < DataLen && i < MAX_BARS; i++) {
        pfOUT[i] = 0.0f;
    }

    for (int i = 0; i < g_state.bi_count; i++) {
        Bi *b = &g_state.bi_list[i];

        /* 起始分型 */
        int start_bar = b->bar_begin;
        if (start_bar >= 0 && start_bar < DataLen) {
            if (b->dir == DIR_UP) {
                pfOUT[start_bar] = -1.0f; /* 底点 */
            } else {
                pfOUT[start_bar] = 1.0f;  /* 顶点 */
            }
        }

        /* 结束分型 */
        int end_bar = b->bar_end;
        if (end_bar >= 0 && end_bar < DataLen) {
            if (b->dir == DIR_UP) {
                pfOUT[end_bar] = 1.0f;  /* 顶点 */
            } else {
                pfOUT[end_bar] = -1.0f; /* 底点 */
            }
        }
    }

    return 0.0f;
}

/* ============================================================
 * TDX 函数: ZEN_SEG - 线段端点
 *
 * 输出：1=线段顶, -1=线段底, 0=非线段端点
 * ============================================================ */
float STDCALL ZEN_SEG(int DataLen, float *pfOUT,
                              float *pfINa, float *pfINb, float *pfINc) {
    init_from_tdx_simple(&g_state, DataLen, pfINa, pfINb, pfINc);
    ensure_computed(&g_state);

    for (int i = 0; i < DataLen && i < MAX_BARS; i++) {
        pfOUT[i] = 0.0f;
    }

    for (int i = 0; i < g_state.seg_count; i++) {
        Segment *seg = &g_state.seg_list[i];

        /* 起始点 */
        if (seg->bar_begin >= 0 && seg->bar_begin < DataLen) {
            if (seg->dir == DIR_UP) {
                pfOUT[seg->bar_begin] = -1.0f; /* 线段底 */
            } else {
                pfOUT[seg->bar_begin] = 1.0f;  /* 线段顶 */
            }
        }

        /* 结束点 */
        if (seg->bar_end >= 0 && seg->bar_end < DataLen) {
            if (seg->dir == DIR_UP) {
                pfOUT[seg->bar_end] = 1.0f;  /* 线段顶 */
            } else {
                pfOUT[seg->bar_end] = -1.0f; /* 线段底 */
            }
        }
    }

    return 0.0f;
}

/* ============================================================
 * TDX 函数: ZEN_ZS_HIGH - 中枢上沿
 * ============================================================ */
float STDCALL ZEN_ZS_HIGH(int DataLen, float *pfOUT,
                                  float *pfINa, float *pfINb, float *pfINc) {
    init_from_tdx_simple(&g_state, DataLen, pfINa, pfINb, pfINc);
    ensure_computed(&g_state);

    for (int i = 0; i < DataLen && i < MAX_BARS; i++) {
        pfOUT[i] = 0.0f;
    }

    /* 在中枢覆盖的BAR范围内输出ZG */
    for (int z = 0; z < g_state.zs_count; z++) {
        ZhongShu *zs = &g_state.zs_list[z];
        int begin = zs->bar_begin;
        int end   = zs->bar_end;
        if (begin < 0) begin = 0;
        if (end >= DataLen) end = DataLen - 1;
        for (int i = begin; i <= end; i++) {
            pfOUT[i] = zs->zg;
        }
    }

    return 0.0f;
}

/* ============================================================
 * TDX 函数: ZEN_ZS_LOW - 中枢下沿
 * ============================================================ */
float STDCALL ZEN_ZS_LOW(int DataLen, float *pfOUT,
                                 float *pfINa, float *pfINb, float *pfINc) {
    init_from_tdx_simple(&g_state, DataLen, pfINa, pfINb, pfINc);
    ensure_computed(&g_state);

    for (int i = 0; i < DataLen && i < MAX_BARS; i++) {
        pfOUT[i] = 0.0f;
    }

    for (int z = 0; z < g_state.zs_count; z++) {
        ZhongShu *zs = &g_state.zs_list[z];
        int begin = zs->bar_begin;
        int end   = zs->bar_end;
        if (begin < 0) begin = 0;
        if (end >= DataLen) end = DataLen - 1;
        for (int i = begin; i <= end; i++) {
            pfOUT[i] = zs->zd;
        }
    }

    return 0.0f;
}

/* ============================================================
 * TDX 函数: ZEN_BUY - 买点
 *
 * 输出：正数 = 买点类型 (1=一买, 2=二买, 3=三买)
 *       11=盘整一买, 22=类二买
 *       0=无买点
 * ============================================================ */
float STDCALL ZEN_BUY(int DataLen, float *pfOUT,
                              float *pfINa, float *pfINb, float *pfINc) {
    init_from_tdx_simple(&g_state, DataLen, pfINa, pfINb, pfINc);
    ensure_computed(&g_state);

    for (int i = 0; i < DataLen && i < MAX_BARS; i++) {
        pfOUT[i] = 0.0f;
    }

    for (int i = 0; i < g_state.bsp_count; i++) {
        Bsp *bsp = &g_state.bsp_list[i];
        if (bsp->bar_idx < 0 || bsp->bar_idx >= DataLen) continue;

        float val = 0.0f;
        switch (bsp->type) {
        case BSP_BUY1:  val = 1.0f; break;
        case BSP_BUY1P: val = 1.1f; break;
        case BSP_BUY2:  val = 2.0f; break;
        case BSP_BUY2S: val = 2.2f; break;
        case BSP_BUY3A: val = 3.0f; break;
        case BSP_BUY3B: val = 3.1f; break;
        default: continue; /* 卖点不输出 */
        }
        if (val > 0) {
            pfOUT[bsp->bar_idx] = val;
        }
    }

    return 0.0f;
}

/* ============================================================
 * TDX 函数: ZEN_SELL - 卖点
 *
 * 输出：负数 = 卖点类型 (-1=一卖, -2=二卖, -3=三卖)
 *       0=无卖点
 * ============================================================ */
float STDCALL ZEN_SELL(int DataLen, float *pfOUT,
                               float *pfINa, float *pfINb, float *pfINc) {
    init_from_tdx_simple(&g_state, DataLen, pfINa, pfINb, pfINc);
    ensure_computed(&g_state);

    for (int i = 0; i < DataLen && i < MAX_BARS; i++) {
        pfOUT[i] = 0.0f;
    }

    for (int i = 0; i < g_state.bsp_count; i++) {
        Bsp *bsp = &g_state.bsp_list[i];
        if (bsp->bar_idx < 0 || bsp->bar_idx >= DataLen) continue;

        float val = 0.0f;
        switch (bsp->type) {
        case BSP_SELL1:  val = -1.0f; break;
        case BSP_SELL1P: val = -1.1f; break;
        case BSP_SELL2:  val = -2.0f; break;
        case BSP_SELL2S: val = -2.2f; break;
        case BSP_SELL3A: val = -3.0f; break;
        case BSP_SELL3B: val = -3.1f; break;
        default: continue;
        }
        if (val < 0) {
            pfOUT[bsp->bar_idx] = val;
        }
    }

    return 0.0f;
}

/* ============================================================
 * TDX 函数: ZEN_MACD - MACD值
 * ============================================================ */
float STDCALL ZEN_MACD(int DataLen, float *pfOUT,
                               float *pfINa, float *pfINb, float *pfINc) {
    init_from_tdx_simple(&g_state, DataLen, pfINa, pfINb, pfINc);

    /* 只需要MACD计算 */
    chan_macd_compute(&g_state);

    int n = g_state.bar_count;
    for (int i = 0; i < DataLen && i < n; i++) {
        pfOUT[i] = g_state.macd_val[i];
    }

    return 0.0f;
}

/* ============================================================
 * TDX 函数: ZEN_DIFF - DIFF线
 * ============================================================ */
float STDCALL ZEN_DIFF(int DataLen, float *pfOUT,
                               float *pfINa, float *pfINb, float *pfINc) {
    init_from_tdx_simple(&g_state, DataLen, pfINa, pfINb, pfINc);
    chan_macd_compute(&g_state);

    int n = g_state.bar_count;
    for (int i = 0; i < DataLen && i < n; i++) {
        pfOUT[i] = g_state.diff[i];
    }
    return 0.0f;
}

/* ============================================================
 * TDX 函数: ZEN_DEA - DEA线
 * ============================================================ */
float STDCALL ZEN_DEA(int DataLen, float *pfOUT,
                              float *pfINa, float *pfINb, float *pfINc) {
    init_from_tdx_simple(&g_state, DataLen, pfINa, pfINb, pfINc);
    chan_macd_compute(&g_state);

    int n = g_state.bar_count;
    for (int i = 0; i < DataLen && i < n; i++) {
        pfOUT[i] = g_state.dea[i];
    }
    return 0.0f;
}

/* ============================================================
 * 带参数的版本：使用 pfINc 的前16个元素作为参数
 * 数据从 pfINc[16] 开始
 * ============================================================ */
float STDCALL ZEN_BI_FRAC_P(int DataLen, float *pfOUT,
                                    float *pfINa, float *pfINb, float *pfINc) {
    init_from_tdx(&g_state, DataLen, pfINa, pfINb, pfINc, 1);
    ensure_computed(&g_state);

    for (int i = 0; i < DataLen && i < MAX_BARS; i++) {
        pfOUT[i] = 0.0f;
    }

    for (int i = 0; i < g_state.fractal_count; i++) {
        Fractal *fx = &g_state.fractals[i];
        int bar_idx = fx->bar_idx;
        if (bar_idx >= 0 && bar_idx < DataLen) {
            pfOUT[bar_idx] = (float)fx->type;
        }
    }

    return 0.0f;
}

float STDCALL ZEN_BI_P(int DataLen, float *pfOUT,
                               float *pfINa, float *pfINb, float *pfINc) {
    init_from_tdx(&g_state, DataLen, pfINa, pfINb, pfINc, 1);
    ensure_computed(&g_state);

    for (int i = 0; i < DataLen && i < MAX_BARS; i++) {
        pfOUT[i] = 0.0f;
    }

    for (int i = 0; i < g_state.bi_count; i++) {
        Bi *b = &g_state.bi_list[i];
        int start_bar = b->bar_begin;
        int end_bar   = b->bar_end;

        if (start_bar >= 0 && start_bar < DataLen) {
            pfOUT[start_bar] = (b->dir == DIR_UP) ? -1.0f : 1.0f;
        }
        if (end_bar >= 0 && end_bar < DataLen) {
            pfOUT[end_bar] = (b->dir == DIR_UP) ? 1.0f : -1.0f;
        }
    }

    return 0.0f;
}

float STDCALL ZEN_SEG_P(int DataLen, float *pfOUT,
                                float *pfINa, float *pfINb, float *pfINc) {
    init_from_tdx(&g_state, DataLen, pfINa, pfINb, pfINc, 1);
    ensure_computed(&g_state);

    for (int i = 0; i < DataLen && i < MAX_BARS; i++) {
        pfOUT[i] = 0.0f;
    }

    for (int i = 0; i < g_state.seg_count; i++) {
        Segment *seg = &g_state.seg_list[i];
        if (seg->bar_begin >= 0 && seg->bar_begin < DataLen) {
            pfOUT[seg->bar_begin] = (seg->dir == DIR_UP) ? -1.0f : 1.0f;
        }
        if (seg->bar_end >= 0 && seg->bar_end < DataLen) {
            pfOUT[seg->bar_end] = (seg->dir == DIR_UP) ? 1.0f : -1.0f;
        }
    }

    return 0.0f;
}

float STDCALL ZEN_ZS_HIGH_P(int DataLen, float *pfOUT,
                                    float *pfINa, float *pfINb, float *pfINc) {
    init_from_tdx(&g_state, DataLen, pfINa, pfINb, pfINc, 1);
    ensure_computed(&g_state);

    for (int i = 0; i < DataLen && i < MAX_BARS; i++) {
        pfOUT[i] = 0.0f;
    }
    for (int z = 0; z < g_state.zs_count; z++) {
        ZhongShu *zs = &g_state.zs_list[z];
        int b = zs->bar_begin < 0 ? 0 : zs->bar_begin;
        int e = zs->bar_end >= DataLen ? DataLen - 1 : zs->bar_end;
        for (int i = b; i <= e; i++) pfOUT[i] = zs->zg;
    }
    return 0.0f;
}

float STDCALL ZEN_ZS_LOW_P(int DataLen, float *pfOUT,
                                   float *pfINa, float *pfINb, float *pfINc) {
    init_from_tdx(&g_state, DataLen, pfINa, pfINb, pfINc, 1);
    ensure_computed(&g_state);

    for (int i = 0; i < DataLen && i < MAX_BARS; i++) {
        pfOUT[i] = 0.0f;
    }
    for (int z = 0; z < g_state.zs_count; z++) {
        ZhongShu *zs = &g_state.zs_list[z];
        int b = zs->bar_begin < 0 ? 0 : zs->bar_begin;
        int e = zs->bar_end >= DataLen ? DataLen - 1 : zs->bar_end;
        for (int i = b; i <= e; i++) pfOUT[i] = zs->zd;
    }
    return 0.0f;
}

float STDCALL ZEN_BUY_P(int DataLen, float *pfOUT,
                                float *pfINa, float *pfINb, float *pfINc) {
    init_from_tdx(&g_state, DataLen, pfINa, pfINb, pfINc, 1);
    ensure_computed(&g_state);

    for (int i = 0; i < DataLen && i < MAX_BARS; i++) {
        pfOUT[i] = 0.0f;
    }
    for (int i = 0; i < g_state.bsp_count; i++) {
        Bsp *bsp = &g_state.bsp_list[i];
        if (bsp->bar_idx < 0 || bsp->bar_idx >= DataLen) continue;
        float val = 0.0f;
        switch (bsp->type) {
        case BSP_BUY1:  val = 1.0f; break;
        case BSP_BUY1P: val = 1.1f; break;
        case BSP_BUY2:  val = 2.0f; break;
        case BSP_BUY2S: val = 2.2f; break;
        case BSP_BUY3A: val = 3.0f; break;
        case BSP_BUY3B: val = 3.1f; break;
        default: continue;
        }
        pfOUT[bsp->bar_idx] = val;
    }
    return 0.0f;
}

float STDCALL ZEN_SELL_P(int DataLen, float *pfOUT,
                                 float *pfINa, float *pfINb, float *pfINc) {
    init_from_tdx(&g_state, DataLen, pfINa, pfINb, pfINc, 1);
    ensure_computed(&g_state);

    for (int i = 0; i < DataLen && i < MAX_BARS; i++) {
        pfOUT[i] = 0.0f;
    }
    for (int i = 0; i < g_state.bsp_count; i++) {
        Bsp *bsp = &g_state.bsp_list[i];
        if (bsp->bar_idx < 0 || bsp->bar_idx >= DataLen) continue;
        float val = 0.0f;
        switch (bsp->type) {
        case BSP_SELL1:  val = -1.0f; break;
        case BSP_SELL1P: val = -1.1f; break;
        case BSP_SELL2:  val = -2.0f; break;
        case BSP_SELL2S: val = -2.2f; break;
        case BSP_SELL3A: val = -3.0f; break;
        case BSP_SELL3B: val = -3.1f; break;
        default: continue;
        }
        pfOUT[bsp->bar_idx] = val;
    }
    return 0.0f;
}

/* ============================================================
 * 通达信标准DLL注册接口
 *
 * 按照 PluginTCalcFunc.h 规范实现
 * ============================================================ */

/* 通达信函数类型 */
typedef BOOL (STDCALL *pPluginFUNC)(int DataLen, float *pfOUT,
                                     float *pfINa, float *pfINb,
                                     float *pfINc, int nReserved);

/* 通达信函数注册结构 */
typedef struct {
    unsigned short nFuncMark;  /* 函数编号 */
    pPluginFUNC    pFunc;      /* 函数指针 */
} PluginTCalcFuncInfo;

/*
 * 函数注册表
 * 编号1-10: 默认配置版本
 * 编号11-17: 带参数版本
 * 以 {0, NULL} 结尾
 */
static PluginTCalcFuncInfo g_CalcFuncSets[] = {
    {1,  (pPluginFUNC)ZEN_BI_FRAC},   /* 分型标记 */
    {2,  (pPluginFUNC)ZEN_BI},        /* 笔端点 */
    {3,  (pPluginFUNC)ZEN_SEG},       /* 线段端点 */
    {4,  (pPluginFUNC)ZEN_ZS_HIGH},   /* 中枢上沿 */
    {5,  (pPluginFUNC)ZEN_ZS_LOW},    /* 中枢下沿 */
    {6,  (pPluginFUNC)ZEN_BUY},       /* 买点 */
    {7,  (pPluginFUNC)ZEN_SELL},      /* 卖点 */
    {8,  (pPluginFUNC)ZEN_MACD},      /* MACD柱 */
    {9,  (pPluginFUNC)ZEN_DIFF},      /* DIFF线 */
    {10, (pPluginFUNC)ZEN_DEA},       /* DEA线 */
    {11, (pPluginFUNC)ZEN_BI_FRAC_P}, /* 分型(带参数) */
    {12, (pPluginFUNC)ZEN_BI_P},      /* 笔(带参数) */
    {13, (pPluginFUNC)ZEN_SEG_P},     /* 线段(带参数) */
    {14, (pPluginFUNC)ZEN_ZS_HIGH_P}, /* 中枢上(带参数) */
    {15, (pPluginFUNC)ZEN_ZS_LOW_P},  /* 中枢下(带参数) */
    {16, (pPluginFUNC)ZEN_BUY_P},     /* 买点(带参数) */
    {17, (pPluginFUNC)ZEN_SELL_P},    /* 卖点(带参数) */
    {0,  NULL}                          /* 结束标记 */
};

#ifdef _WIN32
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)hinstDLL; (void)fdwReason; (void)lpvReserved;
    return TRUE;
}
#endif

/*
 * RegisterTdxFunc - 通达信DLL标准注册入口
 *
 * 通达信调用此函数获取DLL中所有可用的函数
 * 参数: pInfo - 指向函数信息指针的指针
 * 返回: TRUE=成功
 *
 * TDX通过以下方式调用:
 *   PluginTCalcFuncInfo *pInfo = NULL;
 *   RegisterTdxFunc(&pInfo);
 *   // 然后遍历 pInfo 数组直到 {0, NULL}
 */
BOOL STDCALL RegisterTdxFunc(PluginTCalcFuncInfo **pInfo) {
    if (pInfo == NULL) return FALSE;
    *pInfo = g_CalcFuncSets;
    return TRUE;
}

/* ============================================================
 * 使用 #pragma 强制导出干净的函数名（MSVC兼容）
 * ============================================================ */
#ifdef _MSC_VER
#endif
