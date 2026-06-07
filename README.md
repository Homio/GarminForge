# GarminForge — 佳明高精度中国地形图自动流水线

GarminForge 是一款将 WGS-84 坐标系的 OpenStreetMap 地图，通过亚米级 `wgs2gcj` 算法加偏为 GCJ-02（火星坐标系），并自动四等分打包为佳明手表可用地图的工具。

## 原理

中国境内销售的佳明国行固件使用 GCJ-02（火星坐标系），与标准 WGS-84 坐标存在约 450-950 米的非线性偏移。直接使用 WGS-84 地图会导致手表定位与地图显示不一致。

GarminForge 使用逆向工程拟合出的高精度 GCJ-02 公式（eviltransform），在二进制层面直接修改 .img 文件中的坐标，并添加迭代精确验证确保亚米级精度。

## 目录结构

```
GarminForge/
├── src/               # 核心 C 源码
│   ├── main.c              # 入口：遍历 IMG 坐标并变换
│   ├── garmin_transform.c  # wgs2gcj 高精度加偏引擎
│   ├── gimglib.c           # Garmin IMG 文件解析框架
│   ├── garmin_struct.h     # 二进制数据结构定义
│   ├── util.c              # 工具函数
│   └── Makefile            # 构建脚本
├── utils/             # 编译好的工具
│   ├── garmin_forge        # 加偏工具（编译自 src/）
│   ├── gmt                 # GMapTool 地图处理工具
│   └── pipeline.py         # 全自动流水线脚本
├── output/            # 运行时产出
└── work/              # 运行时工作目录
```

## 用法

### 全自动流水线

```bash
# 指定输入地图
./utils/pipeline.py -i /path/to/OpenStreetMap_China_Topo.zip

# 自动下载最新地图
./utils/pipeline.py

# 指定输出目录
./utils/pipeline.py -i map.zip -o ./my_maps
```

### 单步使用

```bash
# 编译 C 工具
make -C src

# 在单张 .img 上执行加偏
./utils/garmin_forge map.img

# 仅检查不变更 (-dryrun)
./utils/garmin_forge -dryrun map.img
```

## 输出

流水线执行完成后在 `output/` 下生成 4 个文件：

| 文件 | FID | 地图名 | 区域 |
|------|-----|--------|------|
| China_NE.img | 6324 | OSM_China_NE | 东经>108.9°, 北纬>34.2° |
| China_SE.img | 6325 | OSM_China_SE | 东经>108.9°, 北纬≤34.2° |
| China_NW.img | 6326 | OSM_China_NW | 东经≤108.9°, 北纬>34.2° |
| China_SW.img | 6327 | OSM_China_SW | 东经≤108.9°, 北纬≤34.2° |

以及 `validation_report.txt` 验证报告。

## 算法精度

| 指标 | 数值 | 说明 |
|------|------|------|
| 正向公式精度 | < 1e-12° | wgs2gcj 是 GCJ-02 规范定义，数学精确 |
| 往返验证精度 | < 1e-11° (约 0.001mm) | gcj2wgs_exact 二分迭代确保 |
| Garmin 24-bit 分辨率 | ~2.4m | 由 IMG 文件格式限制 |
| 对比原版 gimgtools | 精度提升 > 1000x | 原版查表插值误差达数十米 |

## 坐标变换原理

```
WGS-84 → delta(lat,lng) → GCJ-02

delta() 计算:
  1. transform() 以 (105°E, 35°N) 为中心建模偏移场
     使用 6 个频率的正弦级数 + 2 次多项式 + 绝对值项
  2. 用 WGS-84 椭球参数将抽象偏移投影为经纬度偏移
     子午圈/卯酉圈曲率修正

wgs2gcj() 增强:
  1. wgs2gcj() 标准正向变换（数学精确）
  2. gcj2wgs_exact() 二分迭代逆向验证
  3. 确保往返误差 < 1e-12°
```

## 参考项目

- [googollee/eviltransform](https://github.com/googollee/eviltransform) — GCJ-02 逆向工程算法 (MIT)
- [wuyongzheng/gimgtools](https://github.com/wuyongzheng/gimgtools) — Garmin IMG 解析框架 (GPL)
- [gmaptool.eu](http://www.gmaptool.eu) — GMapTool 地图处理工具 (CC BY-SA 3.0)

## 许可

- 核心算法: [eviltransform](https://github.com/googollee/eviltransform) (MIT License)
- IMG 框架: [gimgtools](https://github.com/wuyongzheng/gimgtools) (GPL License)
- GMapTool: CC BY-SA 3.0
- 本项目: GPL v3
