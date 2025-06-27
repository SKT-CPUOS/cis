#ifndef _LI_BITMAP_H_
#define _LI_BITMAP_H_

#include <stdio.h>

#pragma pack(push, 1)
 
typedef unsigned char  U8;
typedef unsigned short U16;
typedef unsigned int   U32;
 
typedef struct tagBITMAPFILEHEADER
{
 
 U16 bfType;
 U32 bfSize;
 U16 bfReserved1;
 U16 bfReserved2;
 U32 bfOffBits;
} BITMAPFILEHEADER;
 
typedef struct tagBITMAPINFOHEADER
{
 U32 biSize;
 U32 biWidth;
 U32 biHeight;
 U16 biPlanes;
 U16 biBitCount;
 U32 biCompression;
 U32 biSizeImage;
 U32 biXPelsPerMeter;
 U32 biYPelsPerMeter;
 U32 biClrUsed;
 U32 biClrImportant;
} BITMAPINFOHEADER;
 
typedef struct tagRGBQUAD
{
 U8 rgbBlue;
 U8 rgbGreen;
 U8 rgbRed;
 U8 rgbReserved;
} RGBQUAD;


struct bmpInfo{
    U32 row;
    U32 col;
    U8* data;
    U8 bitCountPerPix;
};

typedef struct tagBITMAPINFO
{
 BITMAPINFOHEADER bmiHeader;
 RGBQUAD bmiColors[1];
} BITMAPINFO;
 
 
typedef struct tagBITMAP
{
 BITMAPFILEHEADER bfHeader;
 BITMAPINFO biInfo;
}BITMAPFILE;
#pragma pack(pop)

 //生成BMP图片(无颜色表的位图):在RGB(A)位图数据的基础上加上文件信息头和位图信息头
int GenBmpFile(U8 *pData, U8 bitCountPerPix, U32 width, U32 height, const char *filename);
//获取BMP文件的位图数据(无颜色表的位图):丢掉BMP文件的文件信息头和位图信息头，获取其RGB(A)位图数据
U8* GetBmpData(U8 *bitCountPerPix, U32 *width, U32 *height, const char* filename);

void GetBmpInfo(bmpInfo*, const char* filename);
//释放GetBmpData分配的空间
void FreeBmpData(U8 *pdata);
 
FILE* create_bmp_file(U8 bitCountPerPix, U32 width, U32 height, const char *filename);
void write_data_to_bmp(FILE *fp, U8 *pdata, U32 width, U32 line_cnt, U8 bit_count_per_pixel);

#endif