#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <unistd.h>
#include "bit.h"

FILE * create_bmp_file(U8 bitCountPerPix, U32 width, U32 height, const char *filename)  
{  
    FILE *fp = fopen(filename, "wb");  
    if(!fp)  
    {  
        printf("fopen failed : %s, %d\n", __FILE__, __LINE__);  
        exit(1);  
    }  
  
    U32 bmppitch = ((width*bitCountPerPix + 31) >> 5) << 2;  
    U32 filesize = bmppitch*height;  
  
    BITMAPFILE bmpfile;  
  
    bmpfile.bfHeader.bfType = 0x4D42;  
    bmpfile.bfHeader.bfSize = filesize + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);  
    bmpfile.bfHeader.bfReserved1 = 0;  
    bmpfile.bfHeader.bfReserved2 = 0;  
    bmpfile.bfHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);  
  
    bmpfile.biInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);  
    bmpfile.biInfo.bmiHeader.biWidth = width;  
    bmpfile.biInfo.bmiHeader.biHeight = height;  
    bmpfile.biInfo.bmiHeader.biPlanes = 1;  
    bmpfile.biInfo.bmiHeader.biBitCount = bitCountPerPix;  
    bmpfile.biInfo.bmiHeader.biCompression = 0;  
    bmpfile.biInfo.bmiHeader.biSizeImage = 0;  
    bmpfile.biInfo.bmiHeader.biXPelsPerMeter = 0;  
    bmpfile.biInfo.bmiHeader.biYPelsPerMeter = 0;  
    bmpfile.biInfo.bmiHeader.biClrUsed = 0;  
    bmpfile.biInfo.bmiHeader.biClrImportant = 0;  
  
    fwrite(&(bmpfile.bfHeader), sizeof(BITMAPFILEHEADER), 1, fp);  
    fwrite(&(bmpfile.biInfo.bmiHeader), sizeof(BITMAPINFOHEADER), 1, fp);  
  
    //U8 *pEachLinBuf = (U8*)malloc(bmppitch);  
    //memset(pEachLinBuf, 0, bmppitch);  
//
    //if(pEachLinBuf)  
    //{  
    //    int h;  
    //    for(h = height-1; h >= 0; h--)  
    //        fwrite(pEachLinBuf, bmppitch, 1, fp);  
    //    free(pEachLinBuf);  
    //}  
    return fp;  
}  

void write_data_to_bmp(FILE *fp, U8 *pdata, U32 width, U32 line_cnt, U8 bit_count_per_pixel)
{
    // Windows规定一个扫描行所占的字节数必须是4的倍数，不足的以0填充
    U32 size_per_line = ((width * bit_count_per_pixel + 31) >> 5) << 2;  
    U32 total_size = size_per_line * line_cnt;  
    
    U8 *one_line_buf = (U8*)malloc(size_per_line);  
    memset(one_line_buf, 0, size_per_line); 
    U8 byte_per_pixel = bit_count_per_pixel >> 3; 
    U32 byte_per_line = width * byte_per_pixel;  

    int h,w;
    for (h = 0; h < line_cnt; h++) {
        memcpy(one_line_buf, pdata + h * byte_per_line, byte_per_line);
        fwrite(one_line_buf, size_per_line, 1, fp);
    }
    free(one_line_buf);
}
  
int GenBmpFile(U8 *pData, U8 bitCountPerPix, U32 width, U32 height, const char *filename)  
{  
    FILE *fp = fopen(filename, "wb");  
    if(!fp)  
    {  
        printf("fopen failed : %s, %d\n", __FILE__, __LINE__);  
        return 0;  
    }  
  
    U32 bmppitch = ((width*bitCountPerPix + 31) >> 5) << 2;  
    U32 filesize = bmppitch*height;  
  
    BITMAPFILE bmpfile;  
  
    bmpfile.bfHeader.bfType = 0x4D42;  
    bmpfile.bfHeader.bfSize = filesize + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);  
    bmpfile.bfHeader.bfReserved1 = 0;  
    bmpfile.bfHeader.bfReserved2 = 0;  
    bmpfile.bfHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);  
  
    bmpfile.biInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);  
    bmpfile.biInfo.bmiHeader.biWidth = width;  
    bmpfile.biInfo.bmiHeader.biHeight = height;  
    bmpfile.biInfo.bmiHeader.biPlanes = 1;  
    bmpfile.biInfo.bmiHeader.biBitCount = bitCountPerPix;  
    bmpfile.biInfo.bmiHeader.biCompression = 0;  
    bmpfile.biInfo.bmiHeader.biSizeImage = 0;  
    bmpfile.biInfo.bmiHeader.biXPelsPerMeter = 0;  
    bmpfile.biInfo.bmiHeader.biYPelsPerMeter = 0;  
    bmpfile.biInfo.bmiHeader.biClrUsed = 0;  
    bmpfile.biInfo.bmiHeader.biClrImportant = 0;  
  
    fwrite(&(bmpfile.bfHeader), sizeof(BITMAPFILEHEADER), 1, fp);  
    fwrite(&(bmpfile.biInfo.bmiHeader), sizeof(BITMAPINFOHEADER), 1, fp);  
  
    U8 *pEachLinBuf = (U8*)malloc(bmppitch);  
    memset(pEachLinBuf, 0, bmppitch);  
    U8 BytePerPix = bitCountPerPix >> 3;  
    U32 pitch = width * BytePerPix;  
    if(pEachLinBuf)  
    {  
        int h,w;  
        for(h = height-1; h >= 0; h--)  
        {  
            for(w = 0; w < width; w++)  
            {  
                //copy by a pixel  
                pEachLinBuf[w*BytePerPix+0] = pData[h*pitch + w*BytePerPix + 0];  
                pEachLinBuf[w*BytePerPix+1] = pData[h*pitch + w*BytePerPix + 1];  
                pEachLinBuf[w*BytePerPix+2] = pData[h*pitch + w*BytePerPix + 2];  
            }  
            fwrite(pEachLinBuf, bmppitch, 1, fp);  
              
        }  
        free(pEachLinBuf);  
    }  
  
    fflush(fp); // 强制刷新缓冲区到操作系统
    fsync(fileno(fp)); // 强制从操作系统缓存写入磁盘（Linux）
    fclose(fp);  
  
    return 1;  
}  
  
void GetBmpInfo(bmpInfo* info, const char* filename){
    FILE *pf = fopen(filename, "rb");  
    if(!pf)  
    {  
        printf("fopen failed : %s, %d\n", __FILE__, __LINE__);  
        return; 
    }  
  
    BITMAPFILE bmpfile;  
    fread(&(bmpfile.bfHeader), sizeof(BITMAPFILEHEADER), 1, pf);  
    fread(&(bmpfile.biInfo.bmiHeader), sizeof(BITMAPINFOHEADER), 1, pf);  
  

    info->bitCountPerPix = bmpfile.biInfo.bmiHeader.biBitCount;  
    info->col = bmpfile.biInfo.bmiHeader.biWidth;  
    info->row = bmpfile.biInfo.bmiHeader.biHeight;  
   
  
    U32 bmppicth = (((info->col)*(info->bitCountPerPix) + 31) >> 5) << 2;  
    U8 *pdata = (U8*)malloc((info->row)*bmppicth);  
       
    U8 *pEachLinBuf = (U8*)malloc(bmppicth);  
    memset(pEachLinBuf, 0, bmppicth);  
    U8 BytePerPix = (info->bitCountPerPix) >> 3;  
    U32 pitch = (info->col) * BytePerPix;  
  
    if(pdata && pEachLinBuf)  
    {  
        int w, h;  
        for(h = (info->row) - 1; h >= 0; h--)  
        {  
            fread(pEachLinBuf, bmppicth, 1, pf);  
            for(w = 0; w < (info->col); w++)  
            {  
                pdata[h*pitch + w*BytePerPix + 0] = pEachLinBuf[w*BytePerPix+0];  
                pdata[h*pitch + w*BytePerPix + 1] = pEachLinBuf[w*BytePerPix+1];  
                pdata[h*pitch + w*BytePerPix + 2] = pEachLinBuf[w*BytePerPix+2];  
            }  
        }  
        free(pEachLinBuf);  
    }  
    fclose(pf);  
    
    info->data = pdata;
}


//获取BMP文件的位图数据(无颜色表的位图):丢掉BMP文件的文件信息头和位图信息头，获取其RGB(A)位图数据  
U8* GetBmpData(U8 *bitCountPerPix, U32 *width, U32 *height, const char* filename)  
{  
    FILE *pf = fopen(filename, "rb");  
    if(!pf)  
    {  
        printf("fopen failed : %s, %d\n", __FILE__, __LINE__);  
        return NULL;  
    }  
  
    BITMAPFILE bmpfile;  
    fread(&(bmpfile.bfHeader), sizeof(BITMAPFILEHEADER), 1, pf);  
    fread(&(bmpfile.biInfo.bmiHeader), sizeof(BITMAPINFOHEADER), 1, pf);  
  
    if(bitCountPerPix)  
    {  
        *bitCountPerPix = bmpfile.biInfo.bmiHeader.biBitCount;  
    }  
    if(width)  
    {  
        *width = bmpfile.biInfo.bmiHeader.biWidth;  
    }  
    if(height)  
    {  
        *height = bmpfile.biInfo.bmiHeader.biHeight;  
    }  
  
    U32 bmppicth = (((*width)*(*bitCountPerPix) + 31) >> 5) << 2;  
    U8 *pdata = (U8*)malloc((*height)*bmppicth);  
       
    U8 *pEachLinBuf = (U8*)malloc(bmppicth);  
    memset(pEachLinBuf, 0, bmppicth);  
    U8 BytePerPix = (*bitCountPerPix) >> 3;  
    U32 pitch = (*width) * BytePerPix;  
  
    if(pdata && pEachLinBuf)  
    {  
        int w, h;  
        for(h = (*height) - 1; h >= 0; h--)  
        {  
            fread(pEachLinBuf, bmppicth, 1, pf);  
            for(w = 0; w < (*width); w++)  
            {  
                pdata[h*pitch + w*BytePerPix + 0] = pEachLinBuf[w*BytePerPix+0];  
                pdata[h*pitch + w*BytePerPix + 1] = pEachLinBuf[w*BytePerPix+1];  
                pdata[h*pitch + w*BytePerPix + 2] = pEachLinBuf[w*BytePerPix+2];  
            }  
        }  
        free(pEachLinBuf);  
    }  
    fclose(pf);  
       
    return pdata;  
}  
  
//释放GetBmpData分配的空间  
void FreeBmpData(U8 *pdata)  
{  
    if(pdata)  
    {  
        free(pdata);  
        pdata = NULL;  
    }  
} 
