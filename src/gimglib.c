/*
 * gimglib.c — Garmin IMG 文件解析库 (精简版)
 *
 * 仅保留 GarminForge 所需功能:
 *   - gimg_open:   打开并解析 .img 文件 (mmap 到内存)
 *   - gimg_close:  关闭并释放资源
 *   - get_submap:  按名称查找子地图
 *   - get_subfile: 按文件名查找子文件
 *
 * 来源: gimgtools (https://github.com/wuyongzheng/gimgtools)
 * 精简: 移除 dump 系列函数，仅保留 IMG 解析核心
 */

#include "gimglib.h"

/* 将 IMG 文件映射到内存 */
static int map_img(const char *path, int writable,
                   uint8_t **pbase, unsigned int *psize)
{
    int img_fd;
    struct stat sb;
    uint8_t *base;

    assert((writable & 1) == writable);

    img_fd = open(path, writable ? O_RDWR : O_RDONLY);
    if (img_fd == -1) {
        fprintf(stderr, "Error: cannot open %s (err=%d)\n", path, errno);
        return 1;
    }

    if (fstat(img_fd, &sb) == -1) {
        fprintf(stderr, "Error: cannot stat %s (err=%d)\n", path, errno);
        close(img_fd);
        return 1;
    }
    if (sb.st_size == 0) {
        fprintf(stderr, "Error: %s is empty\n", path);
        close(img_fd);
        return 1;
    }

    base = mmap(NULL, sb.st_size,
                PROT_READ | (PROT_WRITE * writable),
                MAP_SHARED, img_fd, 0);
    if (base == MAP_FAILED) {
        fprintf(stderr, "Error: cannot mmap %s (err=%d)\n", path, errno);
        close(img_fd);
        return 1;
    }

    close(img_fd);
    *pbase = base;
    *psize = sb.st_size;
    return 0;
}

/* 解除内存映射 */
static void unmap_img(uint8_t *base, unsigned int size)
{
    munmap(base, size);
}

/* 解析 IMG 文件结构 */
static int parse_img(struct gimg_struct *img)
{
    struct garmin_img *img_header = (struct garmin_img *)(img->base);
    struct subfile_struct *subfile, *subfiles, *subfiles_tail;
    struct subfile_struct *orphans, *orphans_tail;
    struct submap_struct *submap, *submaps, *submaps_tail;
    unsigned int block_size, fatstart, fatend, i;
    unsigned int prev_block = 0;

    if (img_header->xor_byte != 0) {
        fprintf(stderr, "Error: XOR byte is not 0. Use gimgxor first.\n");
        return 1;
    }
    if (strcmp(img_header->signature, "DSKIMG") != 0) {
        fprintf(stderr, "Error: not a valid Garmin IMG file\n");
        return 1;
    }

    block_size = 1 << (img_header->blockexp1 + img_header->blockexp2);

    subfiles = subfiles_tail = NULL;
    fatstart = img_header->fat_offset == 0 ? 3 : img_header->fat_offset;
    if (img_header->data_offset == 0) {
        struct garmin_fat *fat = (struct garmin_fat *)
            (img->base + fatstart * 512);
        if (fat->flag != 1 ||
            memcmp(fat->name, "        ", 8) != 0 ||
            memcmp(fat->type, "   ", 3) != 0) {
            fprintf(stderr, "Error: invalid root directory\n");
            return 1;
        }
        fatstart++;
        fatend = fat->size / 512;
    } else {
        fatend = img_header->data_offset / 512;
    }

    /* 遍历 FAT 表，收集所有子文件 */
    for (i = fatstart; i < fatend; i++) {
        struct garmin_fat *fat = (struct garmin_fat *)(img->base + i * 512);
        int j;

        if (fat->flag != 1) continue;
        if (memcmp(fat->name, "        ", 8) == 0) continue;

        if (fat->part == 0) {
            if (memcmp(fat->type, "GMP", 3) == 0) {
                struct garmin_gmp *gmp = (struct garmin_gmp *)
                    (img->base + fat->blocks[0] * block_size);
                int k;

                for (k = 0; k < NUMBER_SUBFILES &&
                     0x1d + k * 4 <= gmp->comm.hlen; k++) {
                    uint32_t rel_offset = *(&gmp->tre_offset + k);
                    if (rel_offset == 0) continue;

                    subfile = (struct subfile_struct *)
                        calloc(1, sizeof(struct subfile_struct));
                    subfile->header = (struct garmin_subfile *)
                        (img->base + rel_offset + fat->blocks[0] * block_size);
                    subfile->base = (unsigned char *)gmp;
                    subfile->offset = fat->blocks[0] * block_size;
                    subfile->size = fat->size;
                    memcpy(subfile->name, fat->name, 8);
                    string_trim(subfile->name, -1);
                    strncpy(subfile->type, get_subtype_name(k), 3);
                    sprintf(subfile->fullname, "%s.%s",
                            subfile->name, subfile->type);
                    subfile->typeid = k;

                    subfile->next = NULL;
                    if (subfiles_tail == NULL)
                        subfiles_tail = subfiles = subfile;
                    else
                        subfiles_tail = subfiles_tail->next = subfile;
                }

                /* 添加 GMP 自身 */
                subfile = calloc(1, sizeof(struct subfile_struct));
                subfile->header = (struct garmin_subfile *)
                    (img->base + fat->blocks[0] * block_size);
                subfile->base = (unsigned char *)gmp;
                subfile->offset = fat->blocks[0] * block_size;
                subfile->size = fat->size;
                memcpy(subfile->name, fat->name, 8);
                string_trim(subfile->name, -1);
                memcpy(subfile->type, "GMP", 3);
                sprintf(subfile->fullname, "%s.GMP", subfile->name);
                subfile->typeid = ST_GMP;

                subfile->next = NULL;
                if (subfiles_tail == NULL)
                    subfiles_tail = subfiles = subfile;
                else
                    subfiles_tail = subfiles_tail->next = subfile;
            } else {
                subfile = calloc(1, sizeof(struct subfile_struct));
                subfile->header = (struct garmin_subfile *)
                    (img->base + fat->blocks[0] * block_size);
                subfile->base = img->base + fat->blocks[0] * block_size;
                subfile->offset = fat->blocks[0] * block_size;
                subfile->size = fat->size;
                memcpy(subfile->name, fat->name, 8);
                string_trim(subfile->name, -1);
                memcpy(subfile->type, fat->type, 3);
                string_trim(subfile->type, -1);
                sprintf(subfile->fullname, "%s.%s",
                        subfile->name, subfile->type);
                subfile->typeid = get_subtype_id(subfile->type);

                subfile->next = NULL;
                if (subfiles_tail == NULL)
                    subfiles_tail = subfiles = subfile;
                else
                    subfiles_tail = subfiles_tail->next = subfile;
            }
            prev_block = fat->blocks[0] - 1;
        }

        for (j = 0; j < 240; j++) {
            if (fat->blocks[j] != 0xffff &&
                fat->blocks[j] != ++prev_block) {
                fprintf(stderr, "Error: non-contiguous file blocks\n");
                return 1;
            }
        }
    }

    /* 构建 submap 链表 */
    submaps = submaps_tail = NULL;
    orphans = orphans_tail = NULL;
    for (subfile = subfiles; subfile != NULL; subfile = subfile->next) {
        if (subfile->typeid != ST_TRE) continue;
        submap = calloc(1, sizeof(struct submap_struct));
        strcpy(submap->name, subfile->name);
        submap->next = NULL;
        if (submaps_tail == NULL)
            submaps_tail = submaps = submap;
        else
            submaps_tail = submaps_tail->next = submap;
    }
    for (subfile = subfiles; subfile != NULL; subfile = subfile->next) {
        for (submap = submaps; submap != NULL; submap = submap->next)
            if (strcmp(subfile->name, submap->name) == 0) break;
        subfile->map = submap;
        if (submap == NULL) {
            subfile->orphan_next = NULL;
            if (orphans_tail == NULL)
                orphans = orphans_tail = subfile;
            else
                orphans_tail = orphans_tail->orphan_next = subfile;
        } else {
            switch (subfile->typeid) {
            case ST_TRE: case ST_RGN: case ST_LBL:
            case ST_NET: case ST_NOD: case ST_DEM: case ST_MAR:
                if (submap->subfiles[subfile->typeid])
                    fprintf(stderr, "Warning: duplicate %s in %s\n",
                            subfile->type, subfile->name);
                submap->subfiles[subfile->typeid] = subfile;
                break;
            case ST_SRT:
                submap->srt = subfile;
                break;
            case ST_GMP:
                submap->gmp = subfile;
                break;
            default:
                fprintf(stderr, "Warning: unknown type %s in %s\n",
                        subfile->type, subfile->name);
            }
        }
    }

    img->subfiles = subfiles;
    img->submaps = submaps;
    img->orphans = orphans;
    return 0;
}

/* 按名称查找子地图 */
struct submap_struct *get_submap(struct gimg_struct *img, const char *mapname)
{
    struct submap_struct *submap;
    for (submap = img->submaps; submap != NULL; submap = submap->next)
        if (strcasecmp(mapname, submap->name) == 0)
            return submap;
    return NULL;
}

/* 按完整文件名查找子文件 */
struct subfile_struct *get_subfile(struct gimg_struct *img,
                                   const char *subfilename)
{
    struct subfile_struct *subfile;
    for (subfile = img->subfiles; subfile != NULL; subfile = subfile->next)
        if (strcasecmp(subfilename, subfile->fullname) == 0)
            return subfile;
    return NULL;
}

/* 打开并解析 .img 文件 */
struct gimg_struct *gimg_open(const char *path, int writable)
{
    struct gimg_struct *img = calloc(1, sizeof(struct gimg_struct));
    if (img == NULL) {
        fprintf(stderr, "Error: malloc failed\n");
        return NULL;
    }

    img->path = path;
    if (map_img(path, writable, &img->base, &img->size))
        goto errout;
    if (parse_img(img))
        goto errout;

    return img;

errout:
    free(img);
    return NULL;
}

/* 关闭并释放资源 */
void gimg_close(struct gimg_struct *img)
{
    if (img) {
        unmap_img(img->base, img->size);
        /* 注意: subfile/submap 的内存由 mmap 管理，无需释放 */
        free(img);
    }
}
