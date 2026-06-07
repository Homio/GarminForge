#!/usr/bin/env python3
"""
GarminForge Pipeline — 佳明手表高精度中国地形图（四等分版）全自动流水线

流程:
  1. 加载并解压地图包（.zip → .gmap/ 子文件）
  2. 将子文件合并为每个 tile 的 .img
  3. 解析每个 tile 的四至边界，按几何中心分流 NE/SE/NW/SW
  4. 每个象限合成为一个 .img 并加偏 (wgs2gcj)
  5. 生成验证报告

用法:
  ./pipeline.py -i OpenStreetMap_China_Topo.zip
  ./pipeline.py                           # 自动下载
  ./pipeline.py -h                        # 帮助
"""

import sys
import os
import argparse
import subprocess
import zipfile
import shutil
import glob
import re
import textwrap
import hashlib
import math
from pathlib import Path
from datetime import datetime
from concurrent.futures import ThreadPoolExecutor, as_completed

# ============================================================
#  路径
# ============================================================
SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent
SRC_DIR = PROJECT_ROOT / "src"
GARMIN_FORGE = SCRIPT_DIR / "garmin_forge"
GMT = SCRIPT_DIR / "gmt"

# ============================================================
#  常量
# ============================================================
CENTER_LON = 108.9   # 分割中心 东经
CENTER_LAT = 34.2    # 分割中心 北纬

QUADRANTS = [
    {"name": "NE", "fid": "6324", "pid": "1", "label": "OSM_China_NE",
     "desc": "东北"},
    {"name": "SE", "fid": "6325", "pid": "2", "label": "OSM_China_SE",
     "desc": "东南"},
    {"name": "NW", "fid": "6326", "pid": "3", "label": "OSM_China_NW",
     "desc": "西北"},
    {"name": "SW", "fid": "6327", "pid": "4", "label": "OSM_China_SW",
     "desc": "西南"},
]

DEFAULT_DOWNLOAD_URL = (
    "https://alternativaslibres.org/files/OpenStreetMap_China_Topo.zip"
)

# 工作子目录
WORK = PROJECT_ROOT / "work"
RAW = WORK / "raw"              # zip 解压后的原始子文件
TILES_RAW = WORK / "tiles_raw"  # 合并后的单 tile .img
TILES_QUAD = WORK / "tiles_quad"  # 按象限分拣
OUTPUT = PROJECT_ROOT / "output"

# 并行度（tile 合并与解析可并行）
MAX_WORKERS = 4


# ============================================================
#  日志与工具函数
# ============================================================

def log(msg: str, level: str = "INFO"):
    ts = datetime.now().strftime("%H:%M:%S")
    print(f"[{ts}] [{level}] {msg}")


def run_cmd(cmd, desc="", check=True, timeout=3600):
    log(f"执行: {desc or cmd[0]}")
    print(f"  $ {' '.join(str(c) for c in cmd)}")
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, text=True, bufsize=1,
                            errors='replace')
    out_lines, err_lines = [], []
    for line in iter(proc.stdout.readline, ''):
        if line:
            out_lines.append(line.rstrip())
    for line in iter(proc.stderr.readline, ''):
        if line:
            err_lines.append(line.rstrip())
    proc.stdout.close()
    proc.stderr.close()
    ret = proc.wait(timeout=timeout)
    if check and ret != 0:
        raise RuntimeError(f"CMD FAILED (code {ret}): {desc}\n" +
                           "\n".join(err_lines[-20:]))
    return ret, out_lines, err_lines


def ensure_dir(p: Path):
    p.mkdir(parents=True, exist_ok=True)
    return p


def clean_dir(p: Path):
    if p.exists():
        shutil.rmtree(p)
    p.mkdir(parents=True, exist_ok=True)
    return p


def megabytes(path: Path) -> float:
    return path.stat().st_size / (1024 * 1024)


# ============================================================
#  阶段 1: 加载解压
# ============================================================

def stage1_ingest(input_path: str | None) -> Path:
    """
    解压 .zip 到 raw/ 目录。
    返回 raw 目录路径。
    """
    log("=" * 60)
    log("阶段 1/5: 地图加载与解压")
    log("=" * 60)

    if input_path is None:
        import urllib.request
        zip_path = RAW / "OpenStreetMap_China_Topo.zip"

        # 先检查已有文件，再决定是否清空 raw 目录
        if zip_path.exists():
            mb = megabytes(zip_path)
            print(f"\n  已存在: {zip_path} ({mb:.0f} MB)")
            ans = input("  使用已有文件? [Y/n] ").strip().lower()
            if ans in ("", "y", "yes"):
                log("使用已有文件，跳过下载")
                need_download = False
                raw_dir = ensure_dir(RAW)  # 保留已有内容
            else:
                log("重新下载，将清空 raw 目录...")
                raw_dir = clean_dir(RAW)
                need_download = True
        else:
            raw_dir = clean_dir(RAW)
            need_download = True

        if need_download:
            log(f"下载地址: {DEFAULT_DOWNLOAD_URL}")
            log(f"保存路径: {zip_path}")
            log("下载中，请耐心等待...")

            def report(block, blocksize, totalsize):
                downloaded = block * blocksize
                if totalsize > 0:
                    pct = min(100, downloaded * 100 / totalsize)
                    mb_dl = downloaded / (1024 * 1024)
                    mb_total = totalsize / (1024 * 1024)
                    print(f"\r  [{'█' * int(pct // 5)}{'░' * (20 - int(pct // 5))}]"
                          f" {pct:.0f}% ({mb_dl:.0f}/{mb_total:.0f} MB)", end="",
                          flush=True)
                else:
                    print(f"\r  已下载: {downloaded / (1024*1024):.1f} MB", end="",
                          flush=True)

            urllib.request.urlretrieve(DEFAULT_DOWNLOAD_URL, zip_path, report)
            print()
            log(f"下载完成 ({megabytes(zip_path):.0f} MB)")
    else:
        zip_path = Path(input_path)
        if not zip_path.exists():
            raise FileNotFoundError(str(zip_path))
        raw_dir = clean_dir(RAW)

    # 解压
    log(f"解压: {zip_path.name} ({megabytes(zip_path):.0f} MB) → {raw_dir}")
    with zipfile.ZipFile(zip_path, "r") as zf:
        zf.extractall(raw_dir)
    log("解压完成")

    # 查找子文件
    gmap_dirs = list(raw_dir.rglob("*.gmap"))
    if gmap_dirs:
        gmap_dir = gmap_dirs[0]
        log(f"发现 .gmap 目录: {gmap_dir.name}")

        # 找到产品目录（含子文件的）
        product_dirs = sorted(gmap_dir.glob("Product*/*/"))
        if not product_dirs:
            product_dirs = sorted(gmap_dir.glob("Product*"))
            # 每个 product 可能直接包含子文件
            product_dirs = [d for d in product_dirs if d.is_dir()]

        # 如果有 typ 文件，记下来
        typ_files = list(gmap_dir.glob("*.typ"))
        if typ_files:
            log(f"发现 TYP 文件: {[t.name for t in typ_files]}")

        # 看看有多少子文件目录
        tile_sets = []
        for pd in product_dirs:
            for td in sorted(pd.iterdir()):
                if td.is_dir() and any(td.glob("*.TRE")):
                    tile_sets.append(td)

        if not tile_sets:
            # 可能是平铺结构
            tile_sets = sorted(gmap_dir.glob("Product*/*/")) if not product_dirs else product_dirs

        if not tile_sets:
            # 直接找所有含 TRE 的目录
            tile_sets = [d for d in gmap_dir.rglob("*") if d.is_dir() and list(d.glob("*.TRE"))]

        log(f"发现 {len(tile_sets)} 个地图 tile 目录")
        return raw_dir

    # 如果没找到 .gmap，可能是直接有子文件
    subdirs = [d for d in raw_dir.iterdir() if d.is_dir()]
    tile_sets = []
    for sd in subdirs:
        tre_files = list(sd.glob("*.TRE"))
        if tre_files:
            tile_sets.append(sd)
    log(f"发现 {len(tile_sets)} 个地图 tile 目录")
    return raw_dir


# ============================================================
#  阶段 2: 合并子文件为 tile .img
# ============================================================

def get_tile_bounds(img_path: Path) -> dict | None:
    """解析 gmt -i -v 输出中的 N/S/W/E 边界"""
    try:
        ret, out, err = run_cmd(
            [str(GMT), "-i", "-v", str(img_path)],
            desc=f"解析边界: {img_path.name}",
            check=False, timeout=30)
    except Exception:
        return None
    if ret != 0:
        return None

    text = "\n".join(out + err)
    m = re.search(r'N:\s*([\d.]+),\s*S:\s*([\d.]+),\s*W:\s*([\d.]+),\s*E:\s*([\d.]+)', text)
    if m:
        return {
            'max_lat': float(m.group(1)),
            'min_lat': float(m.group(2)),
            'min_lon': float(m.group(3)),
            'max_lon': float(m.group(4)),
        }
    return None


def get_tile_levels(img_path: Path) -> list[int] | None:
    """从 gmt -i -v 输出中解析 levels 数组（如 [16,17,19,20,21,22,23]）"""
    try:
        ret, out, err = run_cmd(
            [str(GMT), "-i", "-v", str(img_path)],
            desc=f"解析 levels: {img_path.name}",
            check=False, timeout=30)
    except Exception:
        return None
    if ret != 0:
        return None

    text = "\n".join(out + err)
    m = re.search(r'levels\s*\[([^\]]+)\]', text)
    if m:
        return [int(x.strip()) for x in m.group(1).split(',')]
    return None


def _get_internal_map_id(img_path: Path) -> str | None:
    """从 gmt -i -v 输出中提取内部地图 ID（如 map 1fa3da8 (33177000) → '33177000'）"""
    try:
        ret, out, err = run_cmd(
            [str(GMT), "-i", "-v", str(img_path)],
            desc=f"解析 map ID: {img_path.name}",
            check=False, timeout=30)
    except Exception:
        return None
    if ret != 0:
        return None

    text = "\n".join(out + err)
    m = re.search(r'map\s+\w+\s+\((\d+)\)', text)
    return m.group(1) if m else None


def is_overview_tile(tile_path: Path) -> bool:
    """
    严格 AND 条件判断是否为概览瓦片（冗余低精度底图）。

    只有同时满足以下三个条件才返回 True（删除）:
      A) 内部地图 ID 以 '33' 开头（如 33177000, 31177001）
      B) levels 数组中存在小于 15 的值（即 min(levels) < 15）
      C) 边界框极大（纬度跨度 > 30° 或 经度跨度 > 40°）

    任何一个条件不满足 → 返回 False（保留）。
    任何解析失败 → 默认保留（防误删）。
    """
    # ── 条件 A: 内部地图 ID 匹配 ──
    map_id = _get_internal_map_id(tile_path)
    if map_id is None:
        log(f"  无法解析 {tile_path.name} 的 map ID，默认保留", "WARN")
        return False
    if not map_id.startswith("33"):
        return False

    # ── 条件 B: 层级极低 ──
    levels = get_tile_levels(tile_path)
    if levels is None:
        log(f"  无法解析 {tile_path.name} 的 levels，默认保留", "WARN")
        return False
    if min(levels) >= 15:
        return False

    # ── 条件 C: 物理范围巨大 ──
    bbox = get_tile_bounds(tile_path)
    if bbox is None:
        log(f"  无法解析 {tile_path.name} 的边界，默认保留", "WARN")
        return False

    lat_span = bbox['max_lat'] - bbox['min_lat']
    lon_span = bbox['max_lon'] - bbox['min_lon']
    if lat_span <= 30.0 and lon_span <= 40.0:
        return False

    # 三层全部通过 → 确认为概览瓦片
    log(f"  概览瓦片: {tile_path.name} "
        f"(levels={levels}, {lat_span:.1f}°×{lon_span:.1f}°) → 删除")
    return True


def join_tile(tile_dir: Path, output_dir: Path) -> Path | None:
    """将 tile 目录中的子文件合并为 .img"""
    tile_name = tile_dir.name
    out_path = output_dir / f"{tile_name}.img"
    if out_path.exists():
        return out_path

    # 收集该 tile 的所有子文件
    files = sorted(tile_dir.glob("*"))
    # 排除已知非地图文件
    skip_patterns = re.compile(r'\.(mdx|tdb|unl|txt|xml)$', re.IGNORECASE)
    map_files = [f for f in files if not skip_patterns.search(f.name)]

    if not map_files:
        return None

    ret, _, _ = run_cmd(
        [str(GMT), "-j", "-f", "0", "-m", tile_name,
         "-o", str(out_path)] + [str(f) for f in map_files],
        desc=f"合并 tile: {tile_name}",
        check=False, timeout=120,
    )
    return out_path if ret == 0 and out_path.exists() else None


def stage2_join_tiles(raw_dir: Path) -> list[Path]:
    """
    将 stage1 解压出的子文件目录合并为单个 .img 文件。
    返回所有 tile .img 路径的列表。
    """
    log("=" * 60)
    log("阶段 2/5: 合并子文件为 tile .img")
    log("=" * 60)

    tiles_raw_dir = clean_dir(TILES_RAW)

    # 查找所有含 TRE 的目录
    tile_dirs = []
    for d in raw_dir.rglob("*"):
        if d.is_dir() and any(f.suffix.upper() == ".TRE" for f in d.iterdir()):
            tile_dirs.append(d)
    tile_dirs.sort()

    log(f"找到 {len(tile_dirs)} 个 tile 目录")
    if not tile_dirs:
        raise RuntimeError("未找到任何地图 tile 目录")

    # 先检查是否已经有缓存的 tile .img
    existing = list(tiles_raw_dir.glob("*.img"))
    if len(existing) == len(tile_dirs):
        log(f"所有 tile 已合并过（{len(existing)} 个），跳过合并")
        return sorted(existing)

    # 并行合并
    completed = []
    with ThreadPoolExecutor(max_workers=MAX_WORKERS) as pool:
        fut_map = {
            pool.submit(join_tile, td, tiles_raw_dir): td
            for td in tile_dirs
        }
        for fut in as_completed(fut_map):
            td = fut_map[fut]
            try:
                result = fut.result()
                if result:
                    completed.append(result)
                    log(f"  ✓ {td.name} → {result.name}")
                else:
                    log(f"  ✗ {td.name} 合并失败", "WARN")
            except Exception as e:
                log(f"  ✗ {td.name}: {e}", "WARN")

    log(f"阶段2 完成: {len(completed)}/{len(tile_dirs)} tile 合并成功")
    return sorted(completed)


# ============================================================
#  阶段 3: 解析边界 + 四等分分流
# ============================================================

def classify_tile(img_path: Path) -> str | None:
    """
    获取 tile 边界并判断属于哪个象限。
    返回: "NE" / "SE" / "NW" / "SW" 或 None
    """
    bbox = get_tile_bounds(img_path)
    if bbox is None:
        return None

    center_lon = (bbox["min_lon"] + bbox["max_lon"]) / 2
    center_lat = (bbox["min_lat"] + bbox["max_lat"]) / 2

    if center_lon > CENTER_LON and center_lat > CENTER_LAT:
        return "NE"
    elif center_lon > CENTER_LON:
        return "SE"
    elif center_lat > CENTER_LAT:
        return "NW"
    else:
        return "SW"


def stage3_sort(tiles: list[Path]):
    """
    解析每个 tile 边界，按几何中心分拣到四等分目录。

    流程:
      1. 概览瓦片过滤（严格 AND 条件，防误删）
      2. 分类 → 移动
    """
    log("=" * 60)
    log("阶段 3/5: 解析边界 + 四等分分流")
    log("=" * 60)

    quad_dirs = {}
    for q in QUADRANTS:
        quad_dirs[q["name"]] = clean_dir(TILES_QUAD / q["name"])

    # ── 步骤 1: 概览瓦片过滤（Strict AND） ──
    remaining = []
    removed = 0
    for tile_path in tiles:
        if is_overview_tile(tile_path):
            try:
                tile_path.unlink()
                removed += 1
            except OSError as e:
                log(f"  删除失败: {tile_path.name}: {e}", "WARN")
                remaining.append(tile_path)
        else:
            remaining.append(tile_path)

    if removed:
        log(f"过滤: 删除 {removed} 个概览瓦片，保留 {len(remaining)} 个")
    else:
        log("过滤: 未发现概览瓦片")

    # ── 步骤 2: 分类 + 移动 ──
    stats = {q["name"]: 0 for q in QUADRANTS}
    errors = 0

    for tile_path in remaining:
        quad = classify_tile(tile_path)
        if quad is None:
            log(f"  无法解析: {tile_path.name}，保留", "WARN")
            errors += 1
            continue

        dest = quad_dirs[quad] / tile_path.name
        shutil.move(str(tile_path), str(dest))
        stats[quad] += 1

    log("分流结果:")
    for q in QUADRANTS:
        quad_dir = quad_dirs[q["name"]]
        count = len(list(quad_dir.glob("*.img")))
        log(f"  {q['name']} ({q['desc']}): {count} tiles")
    if errors:
        log(f"  解析失败: {errors}", "WARN")


# ============================================================
#  阶段 4: 象限合成 + 加偏
# ============================================================

def _find_typ_files(raw_dir: Path) -> list[Path]:
    """在解压目录中查找 .typ 样式文件"""
    typ_files = list(raw_dir.rglob("*.typ"))
    return typ_files


def stage4_pack_and_transform(raw_dir: Path):
    """
    每个象限，严格四步流水:
      1. gmt -j     合并打包 (含原始 TYP)
      2. gmt -w -y  强刷内嵌 TYP 的 FID/PID
      3. gmt -i     熔断校验 (Sanity Check)
      4. garmin_forge  WGS-84 → GCJ-02 加偏
    """
    log("=" * 60)
    log("阶段 4/5: 合成 + 强刷皮肤 + 加偏")
    log("=" * 60)

    output_dir = ensure_dir(OUTPUT)

    # 查找原始 TYP 文件（要在打包时作为输入源）
    typ_files = _find_typ_files(raw_dir)
    typ_paths = [str(f) for f in typ_files]

    for q in QUADRANTS:
        quad_dir = TILES_QUAD / q["name"]
        tile_files = sorted(quad_dir.glob("*.img"))

        if not tile_files:
            log(f"{q['name']}: 无 tile，跳过", "WARN")
            continue

        output_file = output_dir / f"China_{q['name']}.img"
        fid_pid = f"{q['fid']},{q['pid']}"

        log(f"{q['name']}: {len(tile_files)} tiles → {output_file.name}")
        log(f"  FID={q['fid']}, PID={q['pid']}, MapName={q['label']}")

        # ═══════════════════════════════════════════════════════
        #  动作 1: 打包 (Join) — 原始 TYP + 所有 tile 一并输入
        # ═══════════════════════════════════════════════════════
        gmt_cmd = [
            str(GMT), "-j",
            "-f", fid_pid,
            "-m", q["label"],
            "-o", str(output_file),
        ]
        gmt_cmd.extend(typ_paths)
        gmt_cmd.extend(str(f) for f in tile_files)

        run_cmd(gmt_cmd, desc=f"gmt 打包 {q['name']}", timeout=3600)

        if not output_file.exists():
            log(f"  ✗ {output_file.name} 打包失败", "ERROR")
            continue

        log(f"  打包完成: {megabytes(output_file):.1f} MB")

        # ═══════════════════════════════════════════════════════
        #  动作 2: 强刷内嵌 TYP (Correct) — 洗白 FID/PID
        # ═══════════════════════════════════════════════════════
        log(f"  强刷内嵌 TYP: FID={q['fid']}, PID={q['pid']}...")
        correct_cmd = [
            str(GMT), "-w", "-y", fid_pid,
            str(output_file),
        ]
        run_cmd(correct_cmd, desc=f"gmt 修正 {q['name']} TYP", timeout=120)

        # ═══════════════════════════════════════════════════════
        #  动作 3: 熔断校验 (Sanity Check)
        # ═══════════════════════════════════════════════════════
        log("  执行结构安全校验 (Sanity Check)...")
        try:
            info_out = subprocess.check_output(
                [str(GMT), "-i", str(output_file)],
                text=True, timeout=30)
        except subprocess.CalledProcessError as e:
            log(f"  ✗ 熔断: gmt -i 返回错误 (code {e.returncode})", "ERROR")
            log(f"     {e.stderr[:200] if e.stderr else ''}")
            continue

        # 断言 1: 必须包含 TYP
        if "TYP" not in info_out:
            log(f"  ✗ 熔断: {output_file.name} 中未发现 TYP 皮肤！", "ERROR")
            output_file.unlink(missing_ok=True)
            continue

        # 断言 2: TYP 的 FID 和 PID 必须正确
        if f"FID {q['fid']}" not in info_out:
            log(f"  ✗ 熔断: TYP FID 应为 {q['fid']} 但修正失败！", "ERROR")
            output_file.unlink(missing_ok=True)
            continue

        if f"PID {q['pid']}" not in info_out:
            log(f"  ✗ 熔断: TYP PID 应为 {q['pid']} 但修正失败！", "ERROR")
            output_file.unlink(missing_ok=True)
            continue

        log("  ✓ 皮肤验证完美通过！")

        # ═══════════════════════════════════════════════════════
        #  动作 4: 加偏 (Transform)
        # ═══════════════════════════════════════════════════════
        log(f"  WGS-84 → GCJ-02 加偏...")
        run_cmd(
            [str(GARMIN_FORGE), str(output_file)],
            desc=f"garmin_forge {q['name']}",
            timeout=3600,
        )
        log(f"  ✓ {output_file.name} 全部完成")


# ============================================================
#  阶段 5: 验证报告
# ============================================================

def stage5_report() -> bool:
    log("=" * 60)
    log("阶段 5/5: 验证报告")
    log("=" * 60)

    output_dir = OUTPUT
    report_path = output_dir / "validation_report.txt"
    lines = []
    all_ok = True

    lines.append("=" * 70)
    lines.append("GarminForge 验证报告")
    lines.append(f"生成时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    lines.append("=" * 70)
    lines.append("")
    lines.append("一、输出文件清单")
    lines.append("-" * 40)

    total_mb = 0
    for q in QUADRANTS:
        fpath = output_dir / f"China_{q['name']}.img"
        if fpath.exists():
            mb = megabytes(fpath)
            total_mb += mb
            lines.append(f"  ✓ {fpath.name}  ({mb:.1f} MB)")
            lines.append(f"     FID={q['fid']}  PID={q['pid']}  MapName={q['label']}")
            # 用 gmt -i 获取详情
            try:
                _, out, err = run_cmd(
                    [str(GMT), "-i", str(fpath)],
                    check=False, timeout=10)
                info = "\n".join(out + err)
                # 截取 N/S/W/E 行
                m = re.search(r'N:.*E:.*', info)
                if m:
                    lines.append(f"     边界: {m.group()}")
            except Exception:
                pass
        else:
            lines.append(f"  ✗ {fpath.name} — 不存在！")
            all_ok = False
        lines.append("")

    lines.append(f"总大小: {total_mb:.1f} MB")
    lines.append("")

    lines.append("二、结构安全检查")
    lines.append("-" * 40)
    names = set()
    for q in QUADRANTS:
        if q["label"] in names:
            lines.append(f"  ✗ 名称冲突: {q['label']}")
            all_ok = False
        names.add(q["label"])
    lines.append("  ✓ 4个地图名称均唯一")

    fids = set()
    for q in QUADRANTS:
        if q["fid"] in fids:
            lines.append(f"  ✗ FID冲突: {q['fid']}")
            all_ok = False
        fids.add(q["fid"])
    lines.append("  ✓ 4个FID均唯一 (6324~6327)")

    pids = set()
    for q in QUADRANTS:
        if q["pid"] in pids:
            lines.append(f"  ✗ PID冲突: {q['pid']}")
            all_ok = False
        pids.add(q["pid"])
    lines.append("  ✓ 4个PID均唯一 (1~4)")
    lines.append("")

    lines.append("三、分割交叉验证")
    lines.append("-" * 40)
    lines.append(f"  分割中心: {CENTER_LON}°E, {CENTER_LAT}°N (西安)")
    for q in QUADRANTS:
        d = TILES_QUAD / q["name"]
        n = len(list(d.glob("*.img"))) if d.exists() else 0
        lines.append(f"  {q['name']}: {n} tiles")
    lines.append("")

    lines.append("四、文件校验")
    lines.append("-" * 40)
    for q in QUADRANTS:
        fpath = output_dir / f"China_{q['name']}.img"
        if fpath.exists():
            h = hashlib.sha256()
            with open(fpath, "rb") as f:
                for chunk in iter(lambda: f.read(65536), b""):
                    h.update(chunk)
            lines.append(f"  {fpath.name}")
            lines.append(f"    SHA256: {h.hexdigest()}")
    lines.append("")

    lines.append("五、BaseCamp 验证指引")
    lines.append("-" * 40)
    lines.append(textwrap.dedent("""\
      1. Windows 上用 ImDisk 虚拟磁盘 (如 V:)
      2. 创建 V:\\\\Garmin\\ 目录
      3. 将上述 4 个 .img 拷贝到 V:\\\\Garmin\\
      4. 打开 BaseCamp，检查左侧设备列表:
         必须同时识别出 4 个地图:
           OSM_China_NE, OSM_China_SE, OSM_China_NW, OSM_China_SW
      5. 放大至西安交界处 (108.9°E, 34.2°N)
         检查路网是否无缝对接，有无乱码
      ⚠ 未经 BaseCamp 验证严禁拷入手表！可能变砖！
    """))
    lines.append("=" * 70)

    report = "\n".join(lines)
    with open(report_path, "w", encoding="utf-8") as f:
        f.write(report)
    log(f"报告已生成: {report_path}")
    print("\n" + report)
    return all_ok


# ============================================================
#  主函数
# ============================================================

def main():
    global MAX_WORKERS, OUTPUT

    parser = argparse.ArgumentParser(
        description="GarminForge — 佳明手表高精度中国地形图全自动流水线",
    )
    parser.add_argument("-i", "--input", help="输入 .zip 或 .img 路径")
    parser.add_argument("-o", "--output", default=str(OUTPUT),
                        help=f"输出目录 (默认: {OUTPUT})")
    parser.add_argument("--skip-download", action="store_true",
                        help="禁止自动下载")
    parser.add_argument("-j", "--jobs", type=int, default=MAX_WORKERS,
                        help=f"并行数 (默认: {MAX_WORKERS})")
    args = parser.parse_args()
    # 所有路径基于 PROJECT_ROOT（脚本所在位置），不依赖 CWD
    OUTPUT = Path(args.output)
    if not OUTPUT.is_absolute():
        OUTPUT = (PROJECT_ROOT / OUTPUT).resolve()
    else:
        OUTPUT = OUTPUT.resolve()
    ensure_dir(OUTPUT)

    # 检查工具
    if not GMT.exists():
        log(f"GMapTool 未找到: {GMT}", "ERROR"); sys.exit(1)
    if not GARMIN_FORGE.exists():
        log(f"garmin_forge 未找到: {GARMIN_FORGE}", "ERROR"); sys.exit(1)

    try:
        raw_dir = stage1_ingest(args.input)

        if args.input and Path(args.input).suffix.lower() == ".img":
            # 直接处理单个 .img
            img_path = Path(args.input)
            log("输入为单 .img 文件，使用原始三阶段流程")
            run_cmd([str(GARMIN_FORGE), str(img_path)],
                    desc="garmin_forge 加偏")
            log("加偏完成，请手动用 gmt -s 切片后以 -S 重新打包")
            return

        tiles = stage2_join_tiles(raw_dir)
        stage3_sort(tiles)
        stage4_pack_and_transform(raw_dir)
        ok = stage5_report()

        print()
        if ok:
            log("✅ 流水线执行完成！")
            for q in QUADRANTS:
                log(f"  output/China_{q['name']}.img (FID={q['fid']})")
            log("⚠  请在 BaseCamp 验证后再使用！")
        else:
            log("⚠  有错误，请检查日志", "WARN")

    except Exception as e:
        log(f"流水线失败: {e}", "ERROR")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
