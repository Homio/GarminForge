/*
 * main.c — GarminForge 坐标变换入口
 *
 * 遍历 IMG 文件中的 TRE (空间索引树) 和 NOD (节点) 子文件，
 * 对其中所有 Garmin 24-bit 坐标应用 wgs2gcj 高精度变换。
 *
 * 变换范围:
 *   - TRE header: 地图四至边界 (west/south/east/north)
 *   - TRE subdivision: 每个子分块的几何中心
 *   - NOD3 boundary: 路径边界节点
 *
 * 输出: 原地修改 .img 文件 (通过 mmap 写回)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "garmin_transform.h"
#include "gimglib.h"
#include "garmin_struct.h"

/* 调试模式: dryrun = 1 时只打印不改写文件 */
static int dryrun = 0;
/* 冗余输出 */
static int verbose = 0;

/* ====================================================================
 *  fix_subdiv — 处理单个 TRE subdivision
 *
 *  每个 subdiv 存储了 (center_lng, center_lat, width, height)。
 *  流程: 根据 center ± width/height 算出四至角点，
 *        对四个角点做 garmin_transform_rect 变换，
 *        重新计算 center = (x0+x1)/2, (y0+y1)/2。
 * ==================================================================== */
static int fix_subdiv(struct subfile_struct *map,
                      struct garmin_tre_map_level *maplevel,
                      struct garmin_tre_subdiv *div)
{
    int bitshift = 24 - maplevel->bits;
    int center_x, center_y;

    center_x = bytes_to_sint24(div->center_lng);
    center_y = bytes_to_sint24(div->center_lat);

    /* 计算四至角点 */
    int x0 = center_x - (div->width << bitshift);
    int y0 = center_y - (div->height << bitshift);
    int x1 = center_x + (div->width << bitshift);
    int y1 = center_y + (div->height << bitshift);

    if (verbose) {
        printf("  subdiv: center=(%s, %s) size=%dx%d\n",
               sint24_to_lng(center_x), sint24_to_lat(center_y),
               (div->width << bitshift) * 2 + 1,
               (div->height << bitshift) * 2 + 1);
    }

    /* 对整个矩形做 WGS-84 → GCJ-02 变换 */
    garmin_transform_rect(&x0, &y0, &x1, &y1);

    /* 重新计算中心 */
    center_x = (x0 + x1) / 2;
    center_y = (y0 + y1) / 2;

    if (verbose) {
        printf("  subdiv: fixed center=(%s, %s)\n",
               sint24_to_lng(center_x), sint24_to_lat(center_y));
    }

    if (!dryrun) {
        sint24_to_bytes(center_x, div->center_lng);
        sint24_to_bytes(center_y, div->center_lat);
    }

    return 0;
}

/* ====================================================================
 *  fix_tre — 处理 TRE (Tree) 子文件
 *
 *  处理内容:
 *   1. 地图四至边界 (westbound, eastbound, southbound, northbound)
 *   2. 所有 zoom level 的所有 subdivision
 * ==================================================================== */
static int fix_tre(struct subfile_struct *tre)
{
    struct garmin_tre *header = (struct garmin_tre *)tre->header;
    struct garmin_tre_map_level *maplevels;
    int x0, y0, x1, y1;
    int level, global_index, level_index;
    unsigned char *subdiv_ptr;

    /* 处理四至边界 */
    x0 = bytes_to_sint24(header->westbound);
    y0 = bytes_to_sint24(header->southbound);
    x1 = bytes_to_sint24(header->eastbound);
    y1 = bytes_to_sint24(header->northbound);

    if (verbose) {
        printf("  bounds: orig=(%s, %s, %s, %s)\n",
               sint24_to_lng(x0), sint24_to_lat(y0),
               sint24_to_lng(x1), sint24_to_lat(y1));
    }

    garmin_transform_rect(&x0, &y0, &x1, &y1);

    if (verbose) {
        printf("  bounds: fixed=(%s, %s, %s, %s)\n",
               sint24_to_lng(x0), sint24_to_lat(y0),
               sint24_to_lng(x1), sint24_to_lat(y1));
    }

    if (!dryrun) {
        sint24_to_bytes(x0, header->westbound);
        sint24_to_bytes(y0, header->southbound);
        sint24_to_bytes(x1, header->eastbound);
        sint24_to_bytes(y1, header->northbound);
    }

    /* 读取 TRE1 (map level table) */
    if (header->comm.locked) {
        maplevels = (struct garmin_tre_map_level *)malloc(header->tre1_size);
        if (maplevels == NULL) {
            fprintf(stderr, "Error: malloc failed for maplevels\n");
            return 1;
        }
        unlockml((unsigned char *)maplevels,
                 (unsigned char *)tre->base + header->tre1_offset,
                 header->tre1_size,
                 *(unsigned int *)(header->key + 16));
    } else {
        maplevels = (struct garmin_tre_map_level *)malloc(header->tre1_size);
        if (maplevels == NULL) {
            fprintf(stderr, "Error: malloc failed for maplevels\n");
            return 1;
        }
        memcpy(maplevels, tre->base + header->tre1_offset, header->tre1_size);
    }

    /* 遍历所有 zoom level 的 subdivision */
    for (subdiv_ptr = tre->base + header->tre2_offset,
         level = 0, global_index = 1;
         level < (int)(header->tre1_size / 4); level++) {

        for (level_index = 0;
             level_index < maplevels[level].nsubdiv;
             level_index++, global_index++) {

            if (fix_subdiv(tre, &maplevels[level],
                           (struct garmin_tre_subdiv *)subdiv_ptr)) {
                free(maplevels);
                return 1;
            }

            /* 最后一级 subdiv 结构体少 2 字节 */
            subdiv_ptr += (level == (int)(header->tre1_size / 4) - 1)
                ? sizeof(struct garmin_tre_subdiv) - 2
                : sizeof(struct garmin_tre_subdiv);
        }
    }

    free(maplevels);
    return 0;
}

/* ====================================================================
 *  fix_nod — 处理 NOD (节点) 子文件
 *
 *  处理 NOD3 section 中的边界节点坐标。
 *  NOD3 每条记录的前 6 字节是 (经度, 纬度) 各 24-bit。
 * ==================================================================== */
static int fix_nod(struct subfile_struct *nod)
{
    struct garmin_nod *header = (struct garmin_nod *)nod->header;

    if (header->nod3_length == 0) return 0;

    unsigned char *ptr = nod->base + header->nod3_offset;
    int length = header->nod3_length;
    int recsize = header->nod3_recsize;

    if (recsize < 9) {
        fprintf(stderr, "Error: NOD3 boundary nodes recsize %d < 9\n", recsize);
        return 1;
    }

    for (; length >= recsize; ptr += recsize, length -= recsize) {
        int x = bytes_to_sint24(ptr);
        int y = bytes_to_sint24(ptr + 3);

        if (verbose) {
            printf("  nod: orig=(%s, %s)\n",
                   sint24_to_lng(x), sint24_to_lat(y));
        }

        garmin_transform_point(&x, &y);

        if (verbose) {
            printf("  nod: fixed=(%s, %s)\n",
                   sint24_to_lng(x), sint24_to_lat(y));
        }

        if (!dryrun) {
            sint24_to_bytes(x, ptr);
            sint24_to_bytes(y, ptr + 3);
        }
    }

    return 0;
}

/* ====================================================================
 *  fix_map — 处理单个子地图 (包含 TRE + NOD)
 * ==================================================================== */
static int fix_map(struct submap_struct *map)
{
    printf("  Processing: %s\n", map->name);

    if (map->tre == NULL) {
        fprintf(stderr, "  Warning: %s has no TRE, skipped\n", map->name);
        return 0;
    }

    if (fix_tre(map->tre)) return 1;

    if (map->nod != NULL) {
        if (fix_nod(map->nod)) return 1;
    } else {
        if (verbose) printf("  (no NOD section)\n");
    }

    return 0;
}

/* ====================================================================
 *  fix_img — 遍历整个 IMG 文件的所有子地图
 * ==================================================================== */
static int fix_img(struct gimg_struct *img)
{
    struct submap_struct *map;
    int count = 0;

    for (map = img->submaps; map != NULL; map = map->next) {
        if (fix_map(map)) return 1;
        count++;
    }

    printf("Total submaps processed: %d\n", count);
    return 0;
}

/* ====================================================================
 *  usage — 用法说明
 * ==================================================================== */
static void usage(int code)
{
    printf("GarminForge — 高精度 WGS-84 → GCJ-02 地图加偏工具\n");
    printf("\n");
    printf("Usage: garmin_forge [options] <file.img>\n");
    printf("\n");
    printf("Options:\n");
    printf("  -dryrun      仅打印变换信息，不修改文件\n");
    printf("  -v           冗余输出 (显示每个坐标变换细节)\n");
    printf("  -h, --help   显示此帮助\n");
    printf("\n");
    printf("作用: 对 Garmin .img 地图文件中所有坐标\n");
    printf("      应用 wgs2gcj 高精度迭代加偏算法\n");
    printf("      将 WGS-84 坐标转换为 GCJ-02 (火星坐标)\n");
    printf("\n");
    printf("算法:\n");
    printf("  wgs2gcj = wgs2gcj(正向公式) + gcj2wgs_exact(迭代验证)\n");
    printf("  精度: < 1e-12 度 (约 0.0001 mm)\n");
    exit(code);
}

/* ====================================================================
 *  main — 程序入口
 * ==================================================================== */
int main(int argc, char **argv)
{
    const char *img_path = NULL;
    struct gimg_struct *img;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 ||
            strcmp(argv[i], "--help") == 0 ||
            strcmp(argv[i], "-?") == 0) {
            usage(0);
        } else if (strcmp(argv[i], "-dryrun") == 0) {
            dryrun = 1;
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = 1;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage(1);
        } else {
            if (img_path == NULL) {
                img_path = argv[i];
            } else {
                fprintf(stderr, "Only one input file allowed.\n");
                usage(1);
            }
        }
    }

    if (img_path == NULL) {
        fprintf(stderr, "No input file specified.\n");
        usage(1);
    }

    printf("GarminForge — 高精度 WGS-84 → GCJ-02 加偏工具\n");
    printf("Input:  %s\n", img_path);
    printf("Mode:   %s\n", dryrun ? "DRY RUN (no write)" : "LIVE (write back)");
    printf("\n");

    img = gimg_open(img_path, dryrun ? 0 : 1);
    if (img == NULL) {
        fprintf(stderr, "Failed to open or parse: %s\n", img_path);
        return 1;
    }

    int ret = fix_img(img);

    gimg_close(img);

    if (ret == 0) {
        printf("\nDone. All coordinates transformed (WGS-84 → GCJ-02).\n");
        if (dryrun) {
            printf("(Dry run: file was NOT modified)\n");
        }
    }

    return ret;
}
