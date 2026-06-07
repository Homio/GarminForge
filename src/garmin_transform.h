#ifndef GARMIN_TRANSFORM_H
#define GARMIN_TRANSFORM_H

/*
 * garmin_transform.h — 高精度中国坐标变换引擎
 *
 * 基于 eviltransform 核心算法的 WGS-84 → GCJ-02 加偏实现。
 *
 * 核心逻辑:
 *   WGS-84 → wgs2gcj() → GCJ-02  (正向公式，数学精确，O(1))
 *   GCJ-02 → gcj2wgs_exact() → WGS-84 (二分迭代逆变换，调试用)
 *
 * Garmin 24-bit 单位:
 *   GarminUnit = degree * 2^24 / 360
 *   1 GarminUnit ≈ 2.1458e-5° ≈ 2.4m (赤道)
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== 核心坐标变换 ======================== */

/* WGS-84 → GCJ-02 标准正向变换（数学精确） */
void wgs2gcj(double wgsLat, double wgsLng,
             double *gcjLat, double *gcjLng);

/* GCJ-02 → WGS-84 近似逆变换（单步近似，精度~5m） */
void gcj2wgs(double gcjLat, double gcjLng,
             double *wgsLat, double *wgsLng);

/* GCJ-02 → WGS-84 迭代精确逆变换（二分法，精度 < 0.001mm） */
void gcj2wgs_exact(double gcjLat, double gcjLng,
                   double *wgsLat, double *wgsLng);

/* WGS-84 → GCJ-02: 直接使用 wgs2gcj，正向公式即 GCJ-02 规范定义 */
/* （无需迭代精化——Garmin 24-bit 格式分辨率仅 ~2.4m，远低于浮点误差） */

/* ================== Garmin 24-bit 单位转换 ================== */

/* 将 Garmin 24-bit 有符号整数转换为十进制度数 */
double garmin_unit_to_deg(int garmin_unit);

/* 将十进制度数转换为 Garmin 24-bit 有符号整数（四舍五入） */
int deg_to_garmin_unit(double deg);

/* ================== 坐标变换（Garmin单位层） ================== */

/* 对单个 Garmin 坐标点做 WGS-84 → GCJ-02 变换 */
void garmin_transform_point(int *px, int *py);

/*
 * 对 Garmin 矩形边界做 WGS-84 → GCJ-02 变换
 * 输入/输出均为 Garmin 24-bit 单位
 * 假设 px0 <= px1 && py0 <= py1
 */
void garmin_transform_rect(int *px0, int *py0, int *px1, int *py1);

/* 判断 Garmin 坐标点是否在中国区域内（用于快速跳过） */
int garmin_in_china(int x, int y);

#ifdef __cplusplus
}
#endif

#endif /* GARMIN_TRANSFORM_H */
