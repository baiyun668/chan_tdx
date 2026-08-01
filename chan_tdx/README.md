# chan_tdx - 通达信DLL缠论插件

基于 [chan.py](https://github.com/Vespa314/chan.py) 的缠论算法，用纯C语言实现的通达信(TDX)DLL插件。

## 功能特性

### 核心算法（严格对齐chan.py）

| 模块 | 功能 | 说明 |
|------|------|------|
| K线合并 | 包含关系处理 | UP方向取max(high,low), DOWN方向取min(high,low) |
| 分型识别 | 顶/底分型 | 支持exclude_included严格模式 |
| 笔构建 | 笔端点识别 | 严格笔(span>=4), 支持缺口成笔、次高低点 |
| 线段构建 | 特征序列法 | EigenFX算法，支持actual_break验证 |
| 中枢构建 | 三笔重叠 | 支持中枢合并(zs/peak模式)、延伸 |
| 买卖点 | 一二三类 | T1/T1P/T2/T2S/T3A/T3B完整分类 |
| MACD | 标准MACD | DIFF/DEA/MACD柱, 支持area/peak/full_area力度算法 |

### TDX输出函数

| 函数名 | 输出 | 说明 |
|--------|------|------|
| `ZEN_BI_FRAC` | 1=顶, -1=底, 0=无 | 分型标记 |
| `ZEN_BI` | 1=笔顶, -1=笔底, 0=无 | 笔端点 |
| `ZEN_SEG` | 1=线段顶, -1=线段底, 0=无 | 线段端点 |
| `ZEN_ZS_HIGH` | float | 中枢上沿ZG |
| `ZEN_ZS_LOW` | float | 中枢下沿ZD |
| `ZEN_BUY` | 1/1.1/2/2.2/3/3.1 | 买点(一买/盘整一买/二买/类二买/三买A/三买B) |
| `ZEN_SELL` | -1/-1.1/-2/-2.2/-3/-3.1 | 卖点 |
| `ZEN_MACD` | float | MACD柱状值 |
| `ZEN_DIFF` | float | DIFF线 |
| `ZEN_DEA` | float | DEA线 |

带`_P`后缀的版本支持自定义参数（通过pfINc前16个元素传入）。

## 编译

### 方法1: Makefile (推荐)

```bash
# 需要安装 mingw-w64 交叉编译器
# Ubuntu/Debian:
sudo apt-get install mingw-w64

# 编译
cd chan_tdx
make

# 编译结果: chan_tdx.dll
```

### 方法2: CMake

```bash
cd chan_tdx
mkdir build && cd build
cmake ..
make
```

### 方法3: Windows原生编译

```cmd
:: 使用 MinGW
gcc -m32 -O2 -shared -o chan_tdx.dll main.c macd.c kline.c bi.c seg.c zs.c bsp.c chan_tdx.def

:: 使用 MSVC
cl /O2 /LD /DDLL_EXPORTS main.c macd.c kline.c bi.c seg.c zs.c bsp.c /Fe:chan_tdx.dll
```

## 安装到通达信

1. 将 `chan_tdx.dll` 复制到通达信的 `T0002/dllnew/` 目录
2. 重启通达信
3. 在公式编辑器中调用DLL函数

### TDX公式示例

```
{缠论分型标记}
分型:ZEN_BI_FRAC(HIGH,LOW,CLOSE),NODRAW;
DRAWTEXT(分型=1,H*1.01,'顶'),COLORGREEN;
DRAWTEXT(分型=-1,L*0.99,'底'),COLORRED;

{缠论笔}
笔顶底:ZEN_BI(HIGH,LOW,CLOSE),NODRAW;
DRAWTEXT(笔顶底=1,H*1.02,'笔顶'),COLORGREEN,LINETHICK2;
DRAWTEXT(笔顶底=-1,L*0.98,'笔底'),COLORRED,LINETHICK2;

{缠论线段}
线段:ZEN_SEG(HIGH,LOW,CLOSE),NODRAW;
DRAWTEXT(线段=1,H*1.03,'段顶'),COLORYELLOW,LINETHICK3;
DRAWTEXT(线段=-1,L*0.97,'段底'),COLORMAGENTA,LINETHICK3;

{中枢}
中枢高:ZEN_ZS_HIGH(HIGH,LOW,CLOSE),COLORYELLOW,LINETHICK1;
中枢低:ZEN_ZS_LOW(HIGH,LOW,CLOSE),COLORYELLOW,LINETHICK1;

{买卖点}
买点:ZEN_BUY(HIGH,LOW,CLOSE),NODRAW;
DRAWTEXT(买点=1,L*0.97,'一买'),COLORRED,LINETHICK3;
DRAWTEXT(买点=2,L*0.97,'二买'),COLORRED,LINETHICK2;
DRAWTEXT(买点=3,L*0.97,'三买'),COLORRED;

卖点:ZEN_SELL(HIGH,LOW,CLOSE),NODRAW;
DRAWTEXT(卖点=-1,H*1.03,'一卖'),COLORGREEN,LINETHICK3;
DRAWTEXT(卖点=-2,H*1.03,'二卖'),COLORGREEN,LINETHICK2;
DRAWTEXT(卖点=-3,H*1.03,'三卖'),COLORGREEN;

{MACD}
DIF:ZEN_DIFF(HIGH,LOW,CLOSE),COLORWHITE;
DEA:ZEN_DEA(HIGH,LOW,CLOSE),COLORYELLOW;
MACD柱:ZEN_MACD(HIGH,LOW,CLOSE),COLORSTICK;
```

### 带参数版本示例

```
{自定义参数的笔函数}
{pfINc的前16个元素为参数，-999表示使用默认值}
{参数顺序: bi_strict, bi_fx_check, gap_as_kl, bi_end_is_peak, seg_algo,
           divergence_rate, macd_algo, max_bs2_rate, bs1_peak, zs_combine_mode,
           one_bi_zs, exclude_included, bi_allow_sub_peak, macd_fast, macd_slow, macd_signal}

笔:ZEN_BI_P(HIGH,LOW,CONST),NODRAW;
```

## 参数说明

| 参数 | 默认值 | 说明 |
|------|--------|------|
| bi_strict | 1 | 严格笔模式(1=严格span>=4, 0=宽松span>=3) |
| bi_fx_check | 0 | 分型检查(0=严格, 1=宽松, 2=半严格, 3=不检查) |
| gap_as_kl | 0 | 缺口成笔(1=启用) |
| bi_end_is_peak | 0 | 笔终点极值验证(1=启用) |
| bi_allow_sub_peak | 0 | 次高低点成笔(1=启用) |
| seg_algo | 0 | 线段算法(0=chan特征序列法) |
| divergence_rate | 1.0 | 背驰判定比率 |
| macd_algo | 0 | MACD力度(0=半区域, 1=峰值, 2=全区域) |
| max_bs2_rate | 0.9999 | 二类买卖点最大回撤比 |
| bs1_peak | 0 | 一买一卖要求突破极值(1=启用) |
| zs_combine_mode | 1 | 中枢合并(0=不合并, 1=中枢重叠, 2=峰值重叠) |
| one_bi_zs | 0 | 一笔中枢(1=启用) |
| exclude_included | 0 | 排除包含关系分型(1=启用) |
| macd_fast | 12 | MACD快线周期 |
| macd_slow | 26 | MACD慢线周期 |
| macd_signal | 9 | MACD信号线周期 |

## 容量限制

| 项目 | 默认值 | 宏定义 |
|------|--------|--------|
| 最大K线数 | 8192 | MAX_BARS |
| 最大合并K线数 | 8192 | MAX_MERGED_KL |
| 最大分型数 | 4096 | MAX_FRACTALS |
| 最大笔数 | 2048 | MAX_BI |
| 最大线段数 | 682 | MAX_SEG |
| 最大中枢数 | 1364 | MAX_ZS |
| 最大买卖点数 | 64 | MAX_BSP |

可通过编译宏调整，例如：`gcc -DMAX_BARS=16384 ...`

## 代码结构

```
chan_tdx/
├── chan_tdx.h      # 总头文件、数据结构、公共定义
├── macd.h/c        # MACD计算模块
├── kline.h/c       # K线合并、分型识别
├── bi.h/c          # 笔构建
├── seg.h/c         # 线段构建（特征序列法）
├── zs.h/c          # 中枢构建
├── bsp.h/c         # 买卖点识别
├── main.c          # DLL入口、TDX接口函数、RegisterFunc
├── chan_tdx.def    # DLL导出定义
├── CMakeLists.txt  # CMake构建配置
├── Makefile        # Make构建配置
└── README.md       # 本文件
```

## 算法对齐说明

本实现严格对齐 chan.py 的核心算法逻辑：

1. **K线合并**：对齐 `KLineCombiner`，包含关系处理方向由前后K线决定
2. **分型**：对齐 `FenXing`，顶分型要求中间K线的high和low均高于两侧
3. **笔**：对齐 `Bi`，支持严格/宽松模式、分型验证、极值验证
4. **线段**：对齐 `Segment`，使用特征序列法(EigenFX)，支持包含合并和actual_break验证
5. **中枢**：对齐 `ZhongShu`，三笔重叠构成，支持延伸和合并
6. **买卖点**：对齐 `BSPoint`，完整实现T1/T1P/T2/T2S/T3A/T3B

## 注意事项

1. TDX数据从旧到新排列（index 0是最老的K线）
2. 每次函数调用会从头计算（TDX传入完整历史数据）
3. DLL内部使用全局状态，非线程安全
4. 建议在日线或以上级别使用，分钟级别可能需要调整容量限制
5. 带`_P`版本的参数通过pfINc前16个元素传入，数据从第17个元素开始

## License

MIT License
