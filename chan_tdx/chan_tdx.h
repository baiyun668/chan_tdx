/*
 * chan_tdx.h - 通达信DLL缠论插件 总头文件
 *
 * 严格对齐 chan.py 算法实现
 * 纯C实现，无外部依赖
 */

#ifndef CHAN_TDX_H
#define CHAN_TDX_H

#include <math.h>
#include <string.h>
#include <float.h>

/* ============================================================
 * 编译器/平台兼容
 * ============================================================ */
#ifdef _WIN32
  #define EXPORT __declspec(dllexport)
#else
  #define EXPORT
#endif

#ifndef STDCALL
  #ifdef _WIN32
    #define STDCALL __stdcall
  #else
    #define STDCALL
  #endif
#endif

#ifndef CDECL
  #ifdef _WIN32
    #define CDECL __cdecl
  #else
    #define CDECL
  #endif
#endif

/* ============================================================
 * TDX DLL 函数签名
 * ============================================================ */
typedef float (CDECL *TDX_FUNC)(int DataLen,
                                 float *pfOUT,
                                 float *pfINa,
                                 float *pfINb,
                                 float *pfINc);

/* ============================================================
 * 容量常量 (可通过编译宏调整)
 * ============================================================ */
#ifndef MAX_BARS
#define MAX_BARS       8192
#endif

#ifndef MAX_MERGED_KL
#define MAX_MERGED_KL  (MAX_BARS)
#endif

#ifndef MAX_FRACTALS
#define MAX_FRACTALS   (MAX_BARS / 2)
#endif

#ifndef MAX_BI
#define MAX_BI         (MAX_FRACTALS / 2)
#endif

#ifndef MAX_SEG
#define MAX_SEG        (MAX_BI / 3)
#endif

#ifndef MAX_ZS
#define MAX_ZS         (MAX_SEG * 2)
#endif

#ifndef MAX_BSP
#define MAX_BSP        64
#endif

/* ============================================================
 * 方向枚举
 * ============================================================ */
typedef enum {
    DIR_UP   =  1,
    DIR_DOWN = -1,
    DIR_NONE =  0
} Direction;

/* ============================================================
 * 分型类型
 * ============================================================ */
typedef enum {
    FX_NONE   = 0,
    FX_TOP    = 1,   /* 顶分型 */
    FX_BOTTOM = -1   /* 底分型 */
} FractalType;

/* ============================================================
 * 笔分型检查模式
 * ============================================================ */
typedef enum {
    FX_CHECK_STRICT   = 0,  /* 严格：不允许包含关系K线 */
    FX_CHECK_LOSS     = 1,  /* 宽松：允许 */
    FX_CHECK_HALF     = 2,  /* 半严格 */
    FX_CHECK_TOTALLY  = 3   /* 完全不检查 */
} BiFxCheckMode;

/* ============================================================
 * MACD力度算法
 * ============================================================ */
typedef enum {
    MACD_AREA      = 0,  /* 半区域 */
    MACD_PEAK      = 1,  /* 峰值 */
    MACD_FULL_AREA = 2   /* 全区域 */
} MacdAlgo;

/* ============================================================
 * 中枢合并模式
 * ============================================================ */
typedef enum {
    ZS_COMBINE_NONE = 0,
    ZS_COMBINE_ZS   = 1,  /* 中枢重叠合并 */
    ZS_COMBINE_PEAK = 2   /* 峰值重叠合并 */
} ZsCombineMode;

/* ============================================================
 * 线段算法
 * ============================================================ */
typedef enum {
    SEG_ALGO_CHAN = 0   /* chan算法（特征序列法） */
} SegAlgo;

/* ============================================================
 * 原始K线
 * ============================================================ */
typedef struct {
    float high;
    float low;
    float open;    /* 可选，MACD用close */
    float close;
    float vol;
    int   index;   /* 原始BAR索引 */
} RawBar;

/* ============================================================
 * 合并K线
 * ============================================================ */
typedef struct {
    float high;
    float low;
    int   begin;   /* 起始原始BAR索引 */
    int   end;     /* 结束原始BAR索引（包含） */
    int   count;   /* 包含的原始K线数 */
} MergedKLine;

/* ============================================================
 * 分型
 * ============================================================ */
typedef struct {
    FractalType type;
    int   mid_idx;   /* 中间K线在合并K线序列中的索引 */
    int   begin;     /* 分型起始合并K线索引 */
    int   end;       /* 分型结束合并K线索引 */
    int   bar_idx;   /* 对应的原始BAR索引 */
    float high;      /* 分型最高价 */
    float low;       /* 分型最低价 */
} Fractal;

/* ============================================================
 * 笔
 * ============================================================ */
typedef struct {
    Direction  dir;      /* UP=上升笔, DOWN=下降笔 */
    Fractal    start_fx; /* 起始分型 */
    Fractal    end_fx;   /* 结束分型 */
    int        begin;    /* 起始合并K线索引 */
    int        end;      /* 结束合并K线索引 */
    int        bar_begin;/* 起始原始BAR索引 */
    int        bar_end;  /* 结束原始BAR索引 */
    float      high;     /* 笔最高价 */
    float      low;      /* 笔最低价 */
    float      amp;      /* 笔幅度 |high - low| */
    int        is_sure;  /* 是否确认 */
} Bi;

/* ============================================================
 * 线段
 * ============================================================ */
typedef struct {
    Direction  dir;
    int        bi_begin; /* 起始笔索引 */
    int        bi_end;   /* 结束笔索引 */
    int        bar_begin;
    int        bar_end;
    float      high;
    float      low;
    int        is_sure;
} Segment;

/* ============================================================
 * 中枢
 * ============================================================ */
typedef struct {
    float zg;        /* 中枢上沿 = min(3笔的high) */
    float zd;        /* 中枢下沿 = max(3笔的low) */
    float gg;        /* 中枢最高 = max(所有涉及笔的high) */
    float dd;        /* 中枢最低 = min(所有涉及笔的low) */
    int   bi_begin;  /* 构成中枢的第一笔索引 */
    int   bi_end;    /* 中枢延伸的最后一笔索引 */
    int   bi_in;     /* 进入中枢的笔索引 */
    int   bi_out;    /* 离开中枢的笔索引（-1=未离开） */
    int   bar_begin;
    int   bar_end;
    int   level;     /* 中枢级别 */
    int   is_sure;
} ZhongShu;

/* ============================================================
 * 买卖点
 * ============================================================ */
typedef enum {
    BSP_BUY1  = 1,
    BSP_BUY1P = 11,  /* 盘整买1 */
    BSP_BUY2  = 2,
    BSP_BUY2S = 22,  /* 类二买 */
    BSP_BUY3A = 31,  /* 三买（中枢在一买后面） */
    BSP_BUY3B = 32,  /* 三买（中枢在一买前面） */
    BSP_SELL1  = -1,
    BSP_SELL1P = -11,
    BSP_SELL2  = -2,
    BSP_SELL2S = -22,
    BSP_SELL3A = -31,
    BSP_SELL3B = -32,
    BSP_NONE   = 0
} BspType;

typedef struct {
    BspType  type;
    int      bi_idx;   /* 对应笔索引 */
    int      bar_idx;  /* 对应BAR索引 */
    float    price;    /* 价格 */
} Bsp;

/* ============================================================
 * 配置参数
 * ============================================================ */
typedef struct {
    /* 笔参数 */
    int    bi_strict;         /* 严格笔模式 (0/1) */
    int    bi_fx_check;       /* 分型检查模式 (0-3) */
    int    gap_as_kl;         /* 缺口成笔 (0/1) */
    int    bi_end_is_peak;    /* 笔终点极值验证 (0/1) */
    int    bi_allow_sub_peak; /* 次高低点成笔 (0/1) */

    /* 线段参数 */
    int    seg_algo;          /* 线段算法 (0=chan) */

    /* MACD参数 */
    int    macd_fast;         /* 快线周期, 默认12 */
    int    macd_slow;         /* 慢线周期, 默认26 */
    int    macd_signal;       /* 信号线周期, 默认9 */

    /* 买卖点参数 */
    float  divergence_rate;   /* 背驰判定比率, 默认1.0 */
    int    macd_algo;         /* MACD力度算法 (0=area,1=peak,2=full_area) */
    float  max_bs2_rate;      /* 二类买卖点最大回撤比, 默认0.9999 */
    int    bs1_peak;          /* 一买一卖要求突破极值 (0/1) */

    /* 中枢参数 */
    int    zs_combine_mode;   /* 中枢合并模式 (0/1/2) */
    int    one_bi_zs;         /* 一笔中枢 (0/1) */

    /* 通用 */
    int    exclude_included;  /* 排除包含关系K线构成的分型 (0/1) */
} ChanConfig;

/* ============================================================
 * 全局状态（每次调用重新计算）
 * ============================================================ */
typedef struct {
    /* 原始数据 */
    RawBar     bars[MAX_BARS];
    int        bar_count;

    /* 合并K线 */
    MergedKLine merged[MAX_MERGED_KL];
    int         merged_count;

    /* 分型 */
    Fractal     fractals[MAX_FRACTALS];
    int         fractal_count;

    /* 笔 */
    Bi          bi_list[MAX_BI];
    int         bi_count;

    /* 线段 */
    Segment     seg_list[MAX_SEG];
    int         seg_count;

    /* 中枢 */
    ZhongShu    zs_list[MAX_ZS];
    int         zs_count;

    /* 买卖点 */
    Bsp         bsp_list[MAX_BSP];
    int         bsp_count;

    /* MACD */
    float       diff[MAX_BARS];
    float       dea[MAX_BARS];
    float       macd_val[MAX_BARS];

    /* 配置 */
    ChanConfig  config;

    /* 计算标记 */
    int         computed;
} ChanState;

/* ============================================================
 * 全局状态实例
 * ============================================================ */
extern ChanState g_state;

/* ============================================================
 * 默认配置
 * ============================================================ */
static inline void chan_config_default(ChanConfig *cfg) {
    cfg->bi_strict         = 1;
    cfg->bi_fx_check       = FX_CHECK_STRICT;
    cfg->gap_as_kl         = 0;
    cfg->bi_end_is_peak    = 0;
    cfg->bi_allow_sub_peak = 0;
    cfg->seg_algo          = SEG_ALGO_CHAN;
    cfg->macd_fast         = 12;
    cfg->macd_slow         = 26;
    cfg->macd_signal       = 9;
    cfg->divergence_rate   = 1.0f;
    cfg->macd_algo         = MACD_AREA;
    cfg->max_bs2_rate      = 0.9999f;
    cfg->bs1_peak          = 0;
    cfg->zs_combine_mode   = ZS_COMBINE_ZS;
    cfg->one_bi_zs         = 0;
    cfg->exclude_included  = 0;
}

/* ============================================================
 * 模块接口
 * ============================================================ */

/* macd */
void chan_macd_compute(ChanState *s);

/* kline */
void chan_kline_merge(ChanState *s);
void chan_fractal_find(ChanState *s);

/* bi */
void chan_bi_build(ChanState *s);

/* seg */
void chan_seg_build(ChanState *s);

/* zs */
void chan_zs_build(ChanState *s);

/* bsp */
void chan_bsp_find(ChanState *s);

/* 主计算入口 */
void chan_compute_all(ChanState *s);

/* ============================================================
 * 工具函数
 * ============================================================ */
static inline float float_max(float a, float b) { return a > b ? a : b; }
static inline float float_min(float a, float b) { return a < b ? a : b; }
static inline int   int_max(int a, int b) { return a > b ? a : b; }
static inline int   int_min(int a, int b) { return a < b ? a : b; }
static inline float float_abs(float x) { return x < 0 ? -x : x; }

#endif /* CHAN_TDX_H */
