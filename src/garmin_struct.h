#ifndef GARMIN_STRUCT_H
#define GARMIN_STRUCT_H

/*
 * garmin_struct.h — Garmin IMG 二进制数据结构定义
 *
 * 来源: gimgtools (https://github.com/wuyongzheng/gimgtools)
 * 参考: http://ati.land.cz/
 */

#ifdef GT_POSIX
# define PACK_STRUCT __attribute__((packed))
#else
# define PACK_STRUCT
# pragma pack(push, 1)
#endif

#include <stdint.h>

struct garmin_img {
    uint8_t  xor_byte;
    uint8_t  unknown_001[9];
    uint8_t  umonth;
    uint8_t  uyear;
    uint8_t  unknown_00c[3];
    uint8_t  checksum;
    char     signature[7];
    uint8_t  unknown_017;
    uint16_t sectors;
    uint16_t heads;
    uint16_t cylinders;
    uint16_t unknown_01e;
    uint8_t  unknown_020[25];
    uint16_t cyear;
    uint8_t  cmonth;
    uint8_t  cdate;
    uint8_t  chour;
    uint8_t  cminute;
    uint8_t  csecond;
    uint8_t  fat_offset;
    char     identifier[7];
    uint8_t  unknown_048;
    char     desc1[20];
    uint16_t heads1;
    uint16_t sectors1;
    uint8_t  blockexp1;
    uint8_t  blockexp2;
    uint16_t unknown_063;
    char     desc2[30];
    uint8_t  unknown_083;
    uint8_t  unknown_084[904];
    uint32_t data_offset;
    uint8_t  unknown_410[16];
    uint16_t blocks[240];
} PACK_STRUCT ;

struct garmin_fat {
    uint8_t  flag;
    char     name[8];
    char     type[3];
    uint32_t size;
    uint16_t part;
    uint8_t  unknown_012[14];
    uint16_t blocks[240];
} PACK_STRUCT ;

struct garmin_subfile {
    uint16_t hlen;
    char     type[10];
    uint8_t  unknown_00c;
    uint8_t  locked;
    uint16_t year;
    uint8_t  month;
    uint8_t  date;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
} PACK_STRUCT ;

struct garmin_tre {
    struct garmin_subfile comm;
    uint8_t  northbound[3];
    uint8_t  eastbound[3];
    uint8_t  southbound[3];
    uint8_t  westbound[3];
    uint32_t tre1_offset;
    uint32_t tre1_size;
    uint32_t tre2_offset;
    uint32_t tre2_size;
    uint32_t tre3_offset;
    uint32_t tre3_size;
    uint16_t tre3_rec_size;
    uint8_t  unknown_03b[4];
    uint8_t  POI_flags;
    uint8_t  drawprio;
    uint8_t  unknown_041[9];
    uint32_t tre4_offset;
    uint32_t tre4_size;
    uint16_t tre4_rec_size;
    uint8_t  unknown_054[4];
    uint32_t tre5_offset;
    uint32_t tre5_size;
    uint16_t tre5_rec_size;
    uint8_t  unknown_062[4];
    uint32_t tre6_offset;
    uint32_t tre6_size;
    uint16_t tre6_rec_size;
    uint8_t  unknown_070[4];
    uint32_t mapID;
    uint8_t  unknown_078[4];
    uint32_t tre7_offset;
    uint32_t tre7_size;
    uint16_t tre7_rec_size;
    uint8_t  unknown_086[4];
    uint32_t tre8_offset;
    uint32_t tre8_size;
    uint16_t tre8_rec_size;
    uint8_t  unknown_094[6];
    uint8_t  key[20];
    uint32_t tre9_offset;
    uint32_t tre9_size;
    uint16_t tre9_rec_size;
    uint8_t  unknown_0b8[4];
    uint32_t tre10_offset;
    uint32_t tre10_size;
    uint16_t tre10_rec_size;
    uint8_t  unknown_0c6[4];
    uint8_t  unknown_0ca[4];
    uint8_t  unknown_0ce;
} PACK_STRUCT ;

struct garmin_tre_map_level {
    uint8_t  level       :4;
    uint8_t  bit456      :3;
    uint8_t  inherited   :1;
    uint8_t  bits;
    uint16_t nsubdiv;
} PACK_STRUCT ;

struct garmin_tre_subdiv {
    uint8_t  rgn_offset[3];
    uint8_t  elements;
    uint8_t  center_lng[3];
    uint8_t  center_lat[3];
    uint16_t width       :15;
    uint16_t terminate   :1;
    uint16_t height      :15;
    uint16_t unknownbit  :1;
    uint16_t next;
} PACK_STRUCT ;

struct garmin_rgn {
    struct garmin_subfile comm;
    uint32_t rgn1_offset;
    uint32_t rgn1_length;
    uint32_t rgn2_offset;
    uint32_t rgn2_length;
    uint8_t  unknown_025[20];
    uint32_t rgn3_offset;
    uint32_t rgn3_length;
    uint8_t  unknown_041[20];
    uint32_t rgn4_offset;
    uint32_t rgn4_length;
    uint8_t  unknown_05d[20];
    uint32_t rgn5_offset;
    uint32_t rgn5_length;
    uint32_t unknown_079;
} PACK_STRUCT ;

struct garmin_lbl {
    struct garmin_subfile comm;
    uint32_t lbl1_offset;
    uint32_t lbl1_length;
    uint8_t  addr_shift;
    uint8_t  coding;
    uint32_t lbl2_offset;
    uint32_t lbl2_length;
    uint16_t lbl2_recsize;
    uint32_t lbl2_u;
    uint32_t lbl3_offset;
    uint32_t lbl3_length;
    uint16_t lbl3_recsize;
    uint32_t lbl3_u;
    uint32_t lbl4_offset;
    uint32_t lbl4_length;
    uint16_t lbl4_recsize;
    uint32_t lbl4_u;
    uint32_t lbl5_offset;
    uint32_t lbl5_length;
    uint16_t lbl5_recsize;
    uint32_t lbl5_u;
    uint32_t lbl6_offset;
    uint32_t lbl6_length;
    uint8_t  lbl6_addr_shift;
    uint8_t  lbl6_glob_mask;
    uint8_t  lbl6_u[3];
    uint32_t lbl7_offset;
    uint32_t lbl7_length;
    uint16_t lbl7_recsize;
    uint32_t lbl7_u;
    uint32_t lbl8_offset;
    uint32_t lbl8_length;
    uint16_t lbl8_recsize;
    uint32_t lbl8_u;
    uint32_t lbl9_offset;
    uint32_t lbl9_length;
    uint16_t lbl9_recsize;
    uint32_t lbl9_u;
    uint32_t lbl10_offset;
    uint32_t lbl10_length;
    uint16_t lbl10_recsize;
    uint32_t lbl10_u;
    uint32_t lbl11_offset;
    uint32_t lbl11_length;
    uint16_t lbl11_recsize;
    uint32_t lbl11_u;
    uint16_t codepage;
    uint16_t codepage2;
    uint16_t codepage3;
    uint32_t lbl12_offset;
    uint32_t lbl12_length;
    uint32_t lbl13_offset;
    uint32_t lbl13_length;
    uint16_t lbl13_recsize;
    uint16_t lbl13_u;
    uint32_t lbl14_offset;
    uint32_t lbl14_length;
    uint16_t lbl14_recsize;
    uint16_t lbl14_u;
    uint32_t lbl15_offset;
    uint32_t lbl15_length;
    uint16_t lbl15_recsize;
    uint32_t lbl15_u;
    uint32_t lbl16_offset;
    uint32_t lbl16_length;
    uint16_t lbl16_recsize;
    uint32_t lbl16_u;
    uint32_t lbl17_offset;
    uint32_t lbl17_length;
    uint16_t lbl17_recsize;
    uint32_t lbl17_u;
    uint32_t lbl18_offset;
    uint32_t lbl18_length;
    uint16_t lbl18_recsize;
    uint32_t lbl18_u;
    uint32_t lbl19_offset;
    uint32_t lbl19_length;
    uint16_t lbl19_recsize;
    uint32_t lbl19_u;
    uint32_t lbl20_offset;
    uint32_t lbl20_length;
    uint16_t lbl20_recsize;
    uint32_t lbl20_u;
    uint32_t lbl21_offset;
    uint32_t lbl21_length;
    uint16_t lbl21_recsize;
    uint32_t lbl21_u;
    uint32_t lbl22_offset;
    uint32_t lbl22_length;
    uint16_t lbl22_recsize;
    uint32_t lbl22_u;
    uint32_t lbl23_offset;
    uint32_t lbl23_length;
    uint16_t lbl23_recsize;
    uint32_t lbl23_u;
    uint32_t lbl24_offset;
    uint32_t lbl24_length;
    uint16_t lbl24_recsize;
    uint16_t lbl24_u;
    uint32_t lbl25_offset;
    uint32_t lbl25_length;
    uint16_t lbl25_recsize;
    uint32_t lbl25_u;
    uint32_t lbl26_offset;
    uint32_t lbl26_length;
    uint16_t lbl26_recsize;
    uint32_t lbl26_u;
    uint32_t lbl27_offset;
    uint32_t lbl27_length;
    uint16_t lbl27_recsize;
    uint32_t lbl27_u;
    uint32_t lbl28_offset;
    uint32_t lbl28_length;
    uint16_t lbl28_recsize;
    uint32_t lbl28_u;
    uint32_t lbl29_offset;
    uint32_t lbl29_length;
    uint32_t lbl30_offset;
    uint32_t lbl30_length;
    uint16_t lbl30_recsize;
    uint16_t lbl30_u;
    uint32_t lbl31_offset;
    uint32_t lbl31_length;
    uint16_t lbl31_recsize;
    uint16_t lbl31_u;
    uint32_t lbl32_offset;
    uint32_t lbl32_length;
    uint16_t lbl32_recsize;
    uint16_t lbl32_u;
    uint32_t lbl33_offset;
    uint32_t lbl33_length;
    uint16_t lbl33_recsize;
    uint32_t lbl33_u;
    uint32_t lbl34_offset;
    uint32_t lbl34_length;
    uint16_t lbl34_recsize;
    uint32_t lbl34_u;
    uint32_t lbl35_offset;
    uint32_t lbl35_length;
    uint16_t lbl35_recsize;
    uint32_t lbl35_u;
    uint32_t lbl36_offset;
    uint32_t lbl36_length;
    uint16_t lbl36_recsize;
    uint16_t lbl36_u;
} PACK_STRUCT ;

struct garmin_net {
    struct garmin_subfile comm;
    uint32_t net1_offset;
    uint32_t net1_length;
    uint8_t  net1_shift;
    uint32_t net2_offset;
    uint32_t net2_length;
    uint8_t  net2_shift;
    uint32_t net3_offset;
    uint32_t net3_length;
    uint16_t net3_recsize;
    uint32_t unknown_031;
    uint16_t unknown_035;
    uint32_t unknown_037;
    uint32_t unknown_03b;
    uint32_t unknown_03f;
    uint32_t net4_offset;
    uint32_t net4_length;
    uint8_t  net4_u;
    uint32_t net5_offset;
    uint32_t net5_length;
    uint16_t net5_recsize;
    uint32_t net6_offset;
    uint32_t net6_length;
    uint16_t net6_recsize;
    uint32_t unknown_060;
} PACK_STRUCT ;

struct garmin_nod {
    struct garmin_subfile comm;
    uint32_t nod1_offset;
    uint32_t nod1_length;
    uint8_t  nod_bits[4];
    uint8_t  align;
    uint8_t  unknown_022;
    uint16_t roadptrsize;
    uint32_t nod2_offset;
    uint32_t nod2_length;
    uint32_t unknown_02d;
    uint32_t nod3_offset;
    uint32_t nod3_length;
    uint16_t nod3_recsize;
    uint32_t unknown_03c;
    uint32_t nod4_offset;
    uint32_t nod4_length;
    uint32_t unknown_047;
    uint32_t unknown_04b;
    uint32_t unknown_04f;
    uint32_t unknown_053;
    uint32_t unknown_057;
    uint8_t  unknown_05b[12];
    uint32_t nod5_offset;
    uint32_t nod5_length;
    uint16_t nod5_recsize;
    uint32_t nod6_offset;
    uint32_t nod6_length;
    uint16_t nod6_recsize;
    uint32_t unknown_07b;
} PACK_STRUCT ;

struct garmin_dem {
    struct garmin_subfile comm;
    uint32_t flags;
    uint16_t zoom_levels;
    uint32_t reserved0;
    uint16_t record_size;
    uint32_t points_to_block3;
    uint32_t reserved2;
} PACK_STRUCT ;

struct garmin_gmp {
    struct garmin_subfile comm;
    uint32_t unknown_015;
    uint32_t tre_offset;
    uint32_t rgn_offset;
    uint32_t lbl_offset;
    uint32_t net_offset;
    uint32_t nod_offset;
    uint32_t dem_offset;
    uint32_t mar_offset;
} PACK_STRUCT ;

struct garmin_typ {
    struct garmin_subfile comm;
    uint16_t codepage;
    uint32_t point_datoff;
    uint32_t point_datsize;
    uint32_t line_datoff;
    uint32_t line_datsize;
    uint32_t polygon_datoff;
    uint32_t polygon_datsize;
    uint16_t fid;
    uint16_t pid;
    uint32_t point_arroff;
    uint16_t point_arrmod;
    uint32_t point_arrsize;
    uint32_t line_arroff;
    uint16_t line_arrmod;
    uint32_t line_arrsize;
    uint32_t polygon_arroff;
    uint16_t polygon_arrmod;
    uint32_t polygon_arrsize;
    uint32_t draworder_arroff;
    uint16_t draworder_arrmod;
    uint32_t draworder_arrsize;
    uint32_t nt1_arroff;
    uint16_t nt1_arrmod;
    uint32_t nt1_arrsize;
    uint8_t  nt1_flag;
    uint32_t nt1_datoff;
    uint32_t nt1_datsize;
    uint32_t blok0_x;
    uint32_t blok0_off;
    uint32_t blok0_size;
    uint32_t blok0_y;
    uint32_t blok1_x;
    uint32_t blok1_off;
    uint32_t blok1_size;
    uint32_t blok1_y;
    uint32_t blok2_x;
    uint32_t blok2_off;
    uint32_t blok2_size;
    uint16_t unknown_09a;
} PACK_STRUCT ;

#ifdef GT_POSIX
#else
# pragma pack(pop)
#endif

#endif /* GARMIN_STRUCT_H */
