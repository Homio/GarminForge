#ifndef GIMGLIB_H
#define GIMGLIB_H

/*
 * gimglib.h — Garmin IMG 文件解析库 (精简版)
 *
 * 仅保留 GarminForge 所需的 gimg_open/gimg_close 和数据结构。
 * 移除了 dump 系列函数 (为 gimginfo 工具服务，本工具不需要)。
 */

/* POSIX 平台 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <errno.h>

#include "garmin_struct.h"

#define NUMBER_SUBFILES 7

enum subtype {
    ST_TRE, ST_RGN, ST_LBL, ST_NET, ST_NOD, ST_DEM, ST_MAR,
    ST_SRT,
    ST_GMP,
    ST_TYP, ST_MDR, ST_TRF, ST_MPS, ST_QSI,
    ST_UNKNOWN
};

struct submap_struct;
struct subfile_struct {
    struct garmin_subfile *header;
    unsigned char *base;
    unsigned int offset;
    unsigned int size;
    char name[9];
    char type[4];
    char fullname[13];
    enum subtype typeid;
    struct submap_struct *map;
    struct subfile_struct *next;
    struct subfile_struct *orphan_next;
};

struct submap_struct {
    char name[9];
    union {
        struct subfile_struct *subfiles[NUMBER_SUBFILES];
        struct {
            struct subfile_struct *tre;
            struct subfile_struct *rgn;
            struct subfile_struct *lbl;
            struct subfile_struct *net;
            struct subfile_struct *nod;
            struct subfile_struct *dem;
            struct subfile_struct *mar;
        };
    };
    struct subfile_struct *srt;
    struct subfile_struct *gmp;
    struct submap_struct *next;
};

struct gimg_struct {
    const char *path;
    unsigned char *base;
    unsigned int size;
    struct subfile_struct *subfiles;
    struct submap_struct *submaps;
    struct subfile_struct *orphans;
};

/* 24-bit 整数读写 (内联，零开销) */
static inline unsigned int bytes_to_uint24(const unsigned char *bytes)
{
    return (*(const unsigned int *)bytes) & 0x00ffffff;
}

static inline int bytes_to_sint24(const unsigned char *bytes)
{
    int n = (*(const int *)bytes) & 0x00ffffff;
    return (n < 0x00800000) ? n : (n | 0xff000000);
}

static inline void sint24_to_bytes(int n, unsigned char *bytes)
{
    bytes[0] = n & 0xff;
    bytes[1] = (n >> 8) & 0xff;
    bytes[2] = (n >> 16) & 0xff;
}

/* util.c */
const char *sint24_to_lat(int n);
const char *sint24_to_lng(int n);
void unlockml(unsigned char *dst, const unsigned char *src,
              int size, unsigned int key);
enum subtype get_subtype_id(const char *str);
const char *get_subtype_name(enum subtype id);
void string_trim(char *str, int length);

/* gimglib.c */
struct submap_struct *get_submap(struct gimg_struct *img, const char *mapname);
struct subfile_struct *get_subfile(struct gimg_struct *img,
                                   const char *subfilename);
struct gimg_struct *gimg_open(const char *path, int writable);
void gimg_close(struct gimg_struct *img);

#endif /* GIMGLIB_H */
