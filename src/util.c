/*
 * util.c — 工具函数集 (精简版)
 *
 * 提供 GarminForge 所需:
 *   - sint24_to_lat/lng:  24-bit Garmin 单位 → 可读经纬度字符串
 *   - get_subtype_id/name: 子文件类型识别
 *   - string_trim:         字符串去尾部空格
 *   - unlockml:            解锁加密的 TRE map level 表
 *
 * 来源: gimgtools (https://github.com/wuyongzheng/gimgtools)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "gimglib.h"

const char *sint24_to_lat(int n)
{
    static char buffers[8][12];
    static unsigned int buffer_ptr = 0;
    char *buffer = buffers[buffer_ptr++ % 8];

    assert(n < 0x800000 && n >= -0x800000);
    if (n < 0)
        sprintf(buffer, "%.5fS", n * (360.0 / 0x01000000));
    else
        sprintf(buffer, "%.5fN", n * (360.0 / 0x01000000));
    return buffer;
}

const char *sint24_to_lng(int n)
{
    static char buffers[8][12];
    static unsigned int buffer_ptr = 0;
    char *buffer = buffers[buffer_ptr++ % 8];

    assert(n < 0x800000 && n >= -0x800000);
    if (n < 0)
        sprintf(buffer, "%.5fW", n * (360.0 / 0x01000000));
    else
        sprintf(buffer, "%.5fE", n * (360.0 / 0x01000000));
    return buffer;
}

void unlockml(unsigned char *dst, const unsigned char *src,
              int size, unsigned int key)
{
    static const unsigned char shuf[] = {
        0xb, 0xc, 0xa, 0x0,
        0x8, 0xf, 0x2, 0x1,
        0x6, 0x4, 0x9, 0x3,
        0xd, 0x5, 0x7, 0xe};
    int i, ringctr;
    int key_sum = shuf[((key >> 24) + (key >> 16) + (key >> 8) + key) & 0xf];

    for (i = 0, ringctr = 16; i < size; i++) {
        unsigned int upper = src[i] >> 4;
        unsigned int lower = src[i];

        upper -= key_sum;
        upper -= key >> ringctr;
        upper -= shuf[(key >> ringctr) & 0xf];
        ringctr = ringctr ? ringctr - 4 : 16;

        lower -= key_sum;
        lower -= key >> ringctr;
        lower -= shuf[(key >> ringctr) & 0xf];
        ringctr = ringctr ? ringctr - 4 : 16;

        dst[i] = ((upper << 4) & 0xf0) | (lower & 0xf);
    }
}

enum subtype get_subtype_id(const char *str)
{
    if (memcmp(str, "TRE", 3) == 0) return ST_TRE;
    if (memcmp(str, "RGN", 3) == 0) return ST_RGN;
    if (memcmp(str, "LBL", 3) == 0) return ST_LBL;
    if (memcmp(str, "NET", 3) == 0) return ST_NET;
    if (memcmp(str, "NOD", 3) == 0) return ST_NOD;
    if (memcmp(str, "DEM", 3) == 0) return ST_DEM;
    if (memcmp(str, "MAR", 3) == 0) return ST_MAR;
    if (memcmp(str, "SRT", 3) == 0) return ST_SRT;
    if (memcmp(str, "GMP", 3) == 0) return ST_GMP;
    if (memcmp(str, "TYP", 3) == 0) return ST_TYP;
    if (memcmp(str, "MDR", 3) == 0) return ST_MDR;
    if (memcmp(str, "TRF", 3) == 0) return ST_TRF;
    if (memcmp(str, "MPS", 3) == 0) return ST_MPS;
    if (memcmp(str, "QSI", 3) == 0) return ST_QSI;
    return ST_UNKNOWN;
}

const char *get_subtype_name(enum subtype id)
{
    static const char *type_names[] = {
        "TRE", "RGN", "LBL", "NET", "NOD", "DEM", "MAR",
        "SRT", "GMP", "TYP", "MDR", "TRF",
        "MPS", "QSI"};
    return id >= ST_UNKNOWN ? NULL : type_names[id];
}

void string_trim(char *str, int length)
{
    int i;
    if (length == -1)
        length = strlen(str);
    for (i = length - 1; i >= 0 && str[i] == ' '; i--)
        str[i] = '\0';
}
