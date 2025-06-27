#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "bit.h"
#include <arm_neon.h>
#include <stdint.h>
#include <time.h>  // 计时头文件
#include <vector>
#include <thread>
#include <memory>
#include <pthread.h> // 引入多线程相关的头文件

//分界线////////////////////////////////////////////////////////////////////////
//
//修改说明：改了get_img,start_scan。加入了黑白图
// 修改矫正函数，update了
// 
// 
// 
// 
// 
// 
// 
// 
// 
//模块端口寄存器
#define CTR_REG 0
#define CACHE_ADDR_REG 1
#define CACHE_SIZE_REG 2
#define LINE_NUM_REG 3
#define INTERVAL_TIME_REG 4
#define CHOSE 5
#define BLACK_OFFSET 6

/* 命令 */
#define LINE_DATA_READY 0x00000100
/* CIS工作模式 */
#define MODE1 0 
#define MODE2 1 
#define MODE3 2 
/* 扫描尺寸参数 */
#define SCAN_LINE_NUM 3507             //////////////// 
#define PIXEL_NUM_PER_LINE 2592
/* 缓存 */
#define CACHE_SIZE PIXEL_NUM_PER_LINE * 3 * 100     //这里改成100扩大了两倍哦   

//#define SER_IP_ADDR "172.31.225.181"
#define SER_PORT 8000
#define DATA_SIZE SCAN_LINE_NUM * PIXEL_NUM_PER_LINE * 3 
// ioctl命令
#define CMD_ALLOC_MEM   (_IO('a', 1))         
#define CMD_SET_DPI     (_IO('a', 2))        /* 设置dpi模式 */

struct trandata {
    unsigned int number;
    unsigned int data;
};

struct trandata buf;
static void* base = NULL;
static bool is_flip = true;
static bool change_scan = false;
std::vector<std::vector<unsigned char>>* black_level;
std::vector<std::vector<unsigned char>>* black_level_b;
std::vector<std::vector<unsigned char>>* white1_weight;
std::vector<std::vector<unsigned char>>* white1_weight_b;
//改为两个参数分别存起来了

//std::vector<std::vector<double>>* white_avg;
//std::vector<std::vector<double>>* black_avg;
//unsigned char * s_addr = NULL;

void write_reg(int fd, unsigned int number, unsigned int data)
{
    buf.number = number;
    buf.data = data;
    int ret = write(fd, &buf, sizeof(buf));
    if (0 > ret) {
        printf("Write reg %d failed!\r\n", number);
        close(fd);
        exit(1);
    }
}

void read_reg(int fd, unsigned int number, unsigned int* data)
{
    buf.number = number;
    buf.data = 0;
    read(fd, &buf, sizeof(buf));
    *data = buf.data;
}

void set_scan_mode(int fd, unsigned char mode)
{
    int ret = 0;
    ret = ioctl(fd, CMD_SET_DPI, mode);
    if (ret) {
        if (errno == EINVAL) {
            printf("Scan Mode Parameter Error!!\r\n");
            close(fd);
            exit(1);
        }
    }
}

void img_flip(void)
{
    if (is_flip)
        is_flip = false;
    else
        is_flip = true;
}

void change_scanner(int change)
{
    if (change == 0)
        change_scan = false;
    else if (change == 1)
        change_scan = true;
}
//这个是对黑线的区域，即1/3和2/3处，因为三路adc，路与路之间会有这种黑色的数据，下面对他取了周围一些值取了平均，目前是取了四个点来做平均，嫌效果不好，可以多取几个来平均
void write_bmp(char* bmpfile, unsigned char* addr)
{
    unsigned int width = PIXEL_NUM_PER_LINE;
    unsigned char bit_count_per_pixel = 24;
    unsigned int line_index = SCAN_LINE_NUM;
    FILE* bmp_fp = create_bmp_file(bit_count_per_pixel, PIXEL_NUM_PER_LINE, SCAN_LINE_NUM, bmpfile);
    while (line_index) {
        unsigned char* pdata = NULL;
        unsigned int cnt = 300;

        if (line_index < 300)
            cnt = line_index;
        pdata = (unsigned char*)malloc(width * cnt * 3);
        // 数据分多次写入bmp文件，每次写入cnt行数据
        for (int i = 0; i < cnt; i++) {
            unsigned char* line_data_addr = addr + ((line_index - 1 - i) * width) * 3;
            for (int j = 0; j < width; j++) {
                // 检查是否是目标列（1/3和2/3宽度处）
                if (j == width / 3 - 1 || j == width * 2 / 3 - 1||j == width / 3  || j == width * 2 / 3 ||j == width / 3 - 2 || j == width * 2 / 3 - 2) {
                    // 获取四个相邻列的索引（前两列和后两列）
                    int neighbor_cols[4] = { j - 4, j - 5, j + 5, j + 4 };
                    unsigned long sum_b = 0, sum_g = 0, sum_r = 0;

                    // 累加四个相邻列的各通道值
                    for (int k = 0; k < 4; k++) {
                        int col = neighbor_cols[k];
                        int offset_col;
                        if (is_flip)
                            offset_col = (width - 1 - col) * 3;
                        else
                            offset_col = col * 3;

                        sum_b += line_data_addr[offset_col];
                        sum_g += line_data_addr[offset_col + 1];
                        sum_r += line_data_addr[offset_col + 2];
                    }

                    // 计算平均值并赋值（除以4）
                    pdata[(i * width + j) * 3 + 0] = (unsigned char)(sum_b / 4);
                    pdata[(i * width + j) * 3 + 1] = (unsigned char)(sum_g / 4);
                    pdata[(i * width + j) * 3 + 2] = (unsigned char)(sum_r / 4);
                }
                else {
                    // 非目标列正常复制
                    int offset = 0;
                    if (is_flip)
                        offset = (width - 1 - j) * 3;
                    else
                        offset = j * 3;
                    pdata[(i * width + j) * 3 + 0] = line_data_addr[offset];
                    pdata[(i * width + j) * 3 + 1] = line_data_addr[offset + 1];
                    pdata[(i * width + j) * 3 + 2] = line_data_addr[offset + 2];
                }
            }
        }

        write_data_to_bmp(bmp_fp, (unsigned char*)pdata, width, cnt, bit_count_per_pixel);
        fflush(bmp_fp);
        fsync(fileno(bmp_fp));
        free(pdata);
        line_index -= cnt;
    }
}

// 向量化颜色校正函数
void color_correct_neon(uint8_t* pdata, int pitch, int row, int col,
    uint8_t* black_levels, uint8_t* white_weights) {
    for (int h = row - 1; h >= 0; h--) {
        uint8_t* pixelPtr = pdata + h * pitch;
        uint8_t* black_weightRGB = black_levels;
        uint8_t* white_weightRGB = white_weights;

        for (int w = 0; w < col * 3; w += 8) {
            uint8x8_t raw = vld1_u8(pixelPtr + w);
            uint8x8_t black = vld1_u8(black_weightRGB + w);
            uint8x8_t white = vld1_u8(white_weightRGB + w);

            int16x8_t diff = vreinterpretq_s16_u16(vsubl_u8(raw, black));
            int16x8_t zeros = vdupq_n_s16(0);
            diff = vmaxq_s16(diff, zeros);

            uint8x8_t denom_u8 = vqsub_u8(white, black);
            uint16x8_t denom_u16 = vmovl_u8(denom_u8);
            denom_u16 = vmaxq_u16(denom_u16, vdupq_n_u16(1));

            int32x4_t diff0 = vmovl_s16(vget_low_s16(diff));
            int32x4_t diff1 = vmovl_s16(vget_high_s16(diff));
            uint32x4_t denom0 = vmovl_u16(vget_low_u16(denom_u16));
            uint32x4_t denom1 = vmovl_u16(vget_high_u16(denom_u16));

            // Integer approximation of (diff * 255) / denom
            for (int i = 0; i < 4; i++) {
                diff0[i] = (diff0[i] * 255) / denom0[i];
                diff1[i] = (diff1[i] * 255) / denom1[i];
            }

            int16x8_t merged = vcombine_s16(vqmovn_s32(diff0), vqmovn_s32(diff1));
            uint8x8_t result = vqmovun_s16(merged);
            vst1_u8(pixelPtr + w, result);
        }
    }
}
//这个是用来做黑白矫正的，然后图片翻转之后，即is_flip变化之后，需要调整一下某些值才行（考虑到目前三个adc的效果都差不多，所以可以不用怎么做这个）
bool update_bmpFile(const char* filename, int use) {
    // use选择
    bmpInfo my_bmp;
    GetBmpInfo(&my_bmp, filename);

    U8* pdata = my_bmp.data;
    U8 BytePerPix = (my_bmp.bitCountPerPix) >> 3;
    U32 pitch = (my_bmp.col) * BytePerPix;

    int h;
    std::vector<std::vector<unsigned char>>* black_use;
    std::vector<std::vector<unsigned char>>* white_use;

    if (use == 0) {	//权重表的选取
        black_use = black_level;
        white_use = white1_weight;
    }
    else {
        black_use = black_level_b;
        white_use = white1_weight_b;
    }
    printf("for is ready\n");

    // 将二维向量转换为一维数组
    uint8_t* black_levels = new uint8_t[my_bmp.col * 3];
    uint8_t* white_weights = new uint8_t[my_bmp.col * 3];
    for (int w = 0; w < my_bmp.col; w++) {
        for (int i = 0; i < 3; i++) {
            black_levels[w * 3 + i] = (*black_use)[w][i];
            white_weights[w * 3 + i] = (*white_use)[w][i];
        }
    }
    // 开始计时：color_correct_neon函数
    clock_t start_time_color_correct = clock();
    // 调用向量化的颜色校正函数
    color_correct_neon(pdata, pitch, my_bmp.row, my_bmp.col, black_levels, white_weights);

    delete[] black_levels;
    delete[] white_weights;
    // 结束计时：color_correct_neon函数
    clock_t end_time_color_correct = clock();
    double time_used_color_correct = (double)(end_time_color_correct - start_time_color_correct) / CLOCKS_PER_SEC;
    printf("Time used for color_correct_neon: %.6f seconds\n", time_used_color_correct);
  
    // 开始计时：保存校正后的图像
    clock_t start_time_save = clock();
    if (use == 0) {
        // 保存校正后的图像
        const char* my_name = "/home/root/cis_app/res.bmp";
        GenBmpFile(my_bmp.data, my_bmp.bitCountPerPix, my_bmp.col, my_bmp.row, my_name);
    }
    else { // 保存校正后的图像
        const char* my_name = "/home/root/cis_app/res_b.bmp";
        GenBmpFile(my_bmp.data, my_bmp.bitCountPerPix, my_bmp.col, my_bmp.row, my_name);
    }

    // 结束计时：保存校正后的图像
    clock_t end_time_save = clock();
    double time_used_save = (double)(end_time_save - start_time_save) / CLOCKS_PER_SEC;
    printf("Time used for saving image: %.6f seconds\n", time_used_save);
    return true;
}

// 定义一个结构体用于传递给线程函数的参数
typedef struct {
    const char* file_path;
    int param; // 原函数的第二个参数
} ThreadParam;

// 定义用于 write_bmp_thread 的参数结构体
typedef struct {
    const char* file_path;
    unsigned char* buffer;
} WriteBmpParam;

void* write_bmp_thread(void* arg) {
    WriteBmpParam* params = static_cast<WriteBmpParam*>(arg);
    // 将 const char* 转换为 char*，并调用 write_bmp
    write_bmp(const_cast<char*>(params->file_path), params->buffer);
    free(params); // 释放分配的参数内存
    pthread_exit(NULL);
}

// 线程函数，用于执行 update_bmpFile
void* update_bmpFile_thread(void* arg) {
    ThreadParam* params = (ThreadParam*)arg;
    update_bmpFile(params->file_path, params->param);
    free(params); // 释放分配的参数内存
    pthread_exit(NULL);
}



void get_img(int fd, bool is_white_img, bool is_black_img,int mode)
{
    //d/////////////////////////////////////////////
    //参数mode说明：0给a路，用s_addr(a,b现在是对应的了，)，1用b路，s_addr_b,2,两路都用
    //
    //d/////////////////////////////////////////////
    unsigned char* s_addr = (unsigned char*)malloc(DATA_SIZE);
    unsigned char* s_addr_b = (unsigned char*)malloc(DATA_SIZE); // 新增第二个缓冲区
    // 初始化两个缓冲区为纯白色
    memset(s_addr, 255, DATA_SIZE);
    memset(s_addr_b, 255, DATA_SIZE);
    printf("white_bmp_create\n");
    unsigned char* p = s_addr;
    unsigned char* p_b = s_addr_b; // 新增第二个指针
    int s = DATA_SIZE;
    unsigned int ctr_data;
    unsigned int count=0;

    // 设置启动位
    read_reg(fd, CTR_REG, &ctr_data);
    write_reg(fd, CTR_REG, ctr_data | 4);

    while (1) {
        
        void* base = mmap(NULL, CACHE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

        if ((void*)-1 == base) {
            if (errno == ENOBUFS) {
                printf("Transmission completed.\r\n");
                break;
            }
            else if (errno == ENOMEM) {
                continue;
            }
        }
        //printf("mmap\n");
        size_t data_len = (s >= CACHE_SIZE / 2) ? CACHE_SIZE / 2 : s; // 修改为CACHE_SIZE的一半

        // 前一半数据复制到s_addr
        memcpy(p, base, data_len);
        p += data_len;
        s -= data_len;
        printf("s=%d\n", s);

        // 后一半数据复制到s_addr_b
        memcpy(p_b, (unsigned char*)base + CACHE_SIZE / 2, data_len);
        p_b += data_len;
        //s -= data_len; // 注意这里也要减，因为总共处理了data_len * 2的数据  （不需要哦这里）
        count = count + 1;
        munmap(base, CACHE_SIZE);
    }
    //注：这里黑白的到时候再改了，先验证原图可以不可以先，一步一步来，start-scan也是一样的，后面再修改哦
    if (is_white_img && !is_black_img) {
        if (mode == 0) {
            write_bmp("/home/root/cis_app/white.bmp", s_addr);//用a路的数据
        }
        if (mode == 1) {
            write_bmp("/home/root/cis_app/white_b.bmp", s_addr_b);//用b路的数据
        }
        if (mode == 2) {
            write_bmp("/home/root/cis_app/white.bmp", s_addr);//用a路和b路的数据的数据
            write_bmp("/home/root/cis_app/white_b.bmp", s_addr_b);
        }

    }
    else if (is_black_img && !is_white_img) {

        if (mode == 0) {
            write_bmp("/home/root/cis_app/black.bmp", s_addr);//用a路的数据
        }
        if (mode == 1) {
            write_bmp("/home/root/cis_app/black_b.bmp", s_addr_b);//用b路的数据
        }
        if (mode == 2) {
            write_bmp("/home/root/cis_app/black.bmp", s_addr);//用a路和b路的数据的数据
            write_bmp("/home/root/cis_app/black_b.bmp", s_addr_b);
        }

    }
    else {
        // 创建两个线程来保存两个BMP文件
        pthread_t thread_write1, thread_write2;
        WriteBmpParam* param_write1 = (WriteBmpParam*)malloc(sizeof(WriteBmpParam));
        param_write1->file_path = "/home/root/cis_app/origin.bmp";
        param_write1->buffer = s_addr;
        WriteBmpParam* param_write2 = (WriteBmpParam*)malloc(sizeof(WriteBmpParam));
        param_write2->file_path = "/home/root/cis_app/origin_b.bmp";
        param_write2->buffer = s_addr_b;

        pthread_create(&thread_write1, NULL, write_bmp_thread, param_write1);
        pthread_create(&thread_write2, NULL, write_bmp_thread, param_write2);

        // 等待保存文件的线程完成
        pthread_join(thread_write1, NULL);
        pthread_join(thread_write2, NULL);
    }
    printf("mmap's count=%u\n", count);
    printf("s=%d\n",s);
    free(s_addr);
    free(s_addr_b); // 新增释放第二个缓冲区
}
void start_scan(int fd)
{
    unsigned char* s_addr = (unsigned char*)malloc(DATA_SIZE);
    unsigned char* s_addr_b = (unsigned char*)malloc(DATA_SIZE); // 新增第二个缓冲区
    // 初始化两个缓冲区为纯白色
    memset(s_addr, 255, DATA_SIZE);
    memset(s_addr_b, 255, DATA_SIZE);
    //printf("white_bmp_create\n");
    unsigned char* p = s_addr;
    unsigned char* p_b = s_addr_b; // 新增第二个指针
    int s = DATA_SIZE;
    unsigned int ctr_data;
    unsigned int count = 0;

    // 设置启动位
    read_reg(fd, CTR_REG, &ctr_data);
    write_reg(fd, CTR_REG, ctr_data | 4);
    // 开始计时：color_correct_neon函数
    clock_t start_time_ctr_start = clock();
    while (1) {

        void* base = mmap(NULL, CACHE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

        if ((void*)-1 == base) {
            if (errno == ENOBUFS) {
                printf("Transmission completed.\r\n");
                break;
            }
            else if (errno == ENOMEM) {
                continue;
            }
        }
        //printf("mmap\n");
        size_t data_len = (s >= CACHE_SIZE / 2) ? CACHE_SIZE / 2 : s; // 修改为CACHE_SIZE的一半

        // 前一半数据复制到s_addr
        memcpy(p, base, data_len);
        p += data_len;
        s -= data_len;
        //printf("s=%d\n", s);

        // 后一半数据复制到s_addr_b
        memcpy(p_b, (unsigned char*)base + CACHE_SIZE / 2, data_len);
        p_b += data_len;
        //s -= data_len; // 注意这里也要减，因为总共处理了data_len * 2的数据  （不需要哦这里）
        count = count + 1;
        munmap(base, CACHE_SIZE);
    }
    // 结束计时：ctr启动时间
    clock_t end_time_ctr_start = clock();
    double time_used_get_img = (double)(end_time_ctr_start - start_time_ctr_start) / CLOCKS_PER_SEC;
    printf("Time used for get_img %.6f seconds\n", time_used_get_img);


    //clock_t start_time_save_origin = clock();
    //    write_bmp("/home/root/cis_app/origin.bmp", s_addr);
    //    write_bmp("/home/root/cis_app/origin_b.bmp", s_addr_b); // 新增保存第二个缓冲区

    //    clock_t end_time_save_origin = clock();
    //    double time_used_save_origin = (double)(end_time_save_origin - start_time_save_origin) / CLOCKS_PER_SEC;
    //    printf("Time used for save two origin bmp %.6f seconds\n", time_used_save_origin);

    printf("mmap's count=%u\n", count);
    //printf("s=%d\n", s);
    printf("scan success\n");
    // 创建两个线程来保存两个BMP文件
    pthread_t thread_write1, thread_write2;
    WriteBmpParam* param_write1 = (WriteBmpParam*)malloc(sizeof(WriteBmpParam));
    param_write1->file_path = "/home/root/cis_app/origin.bmp";
    param_write1->buffer = s_addr;
    WriteBmpParam* param_write2 = (WriteBmpParam*)malloc(sizeof(WriteBmpParam));
    param_write2->file_path = "/home/root/cis_app/origin_b.bmp";
    param_write2->buffer = s_addr_b;

    pthread_create(&thread_write1, NULL, write_bmp_thread, param_write1);
    pthread_create(&thread_write2, NULL, write_bmp_thread, param_write2);

    // 等待保存文件的线程完成
    pthread_join(thread_write1, NULL);
    pthread_join(thread_write2, NULL);

    // 创建两个线程来并发执行 update_bmpFile_thread
    pthread_t thread1, thread2;
    ThreadParam* param1 = (ThreadParam*)malloc(sizeof(ThreadParam));
    param1->file_path = "/home/root/cis_app/origin.bmp";
    param1->param = 0;
    ThreadParam* param2 = (ThreadParam*)malloc(sizeof(ThreadParam));
    param2->file_path = "/home/root/cis_app/origin_b.bmp";
    param2->param = 1;

    pthread_create(&thread1, NULL, update_bmpFile_thread, param1);
    pthread_create(&thread2, NULL, update_bmpFile_thread, param2);

    // 等待线程执行完成
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    printf("finish update\n");

    free(s_addr);
    free(s_addr_b); // 释放内存
}

void set_interval_time(int fd, unsigned int t)
{
    write_reg(fd, INTERVAL_TIME_REG, t);
    unsigned int ctr_data;
    read_reg(fd, INTERVAL_TIME_REG, &ctr_data);
    printf("interval time = %d\n", ctr_data);
    fflush(stdout);
}

// 初始化权重
std::vector<std::vector<unsigned char>> init_weight(const char* filename) {
    bmpInfo* info = new bmpInfo;
    GetBmpInfo(info, filename);

    std::vector<std::vector<unsigned char>> white_avg(info->col, std::vector<unsigned char>(3, 0));     // 全都初始化为0
    U8* pdata = info->data;
    U8 BytePerPix = (info->bitCountPerPix) >> 3;
    U32 pitch = (info->col) * BytePerPix;
    int w, h;

    for (w = 0; w < info->col; w++) {
        std::vector<long long> sum(3, 0);
        for (h = info->row - 1; h >= 0; h--) {
            sum[0] += pdata[h * pitch + w * BytePerPix + 0];
            sum[1] += pdata[h * pitch + w * BytePerPix + 1];
            sum[2] += pdata[h * pitch + w * BytePerPix + 2];
        }

        for (int i = 0; i < 3; i++) {
            white_avg[w][i] = static_cast<unsigned char>(sum[i] / info->row);       // 确保结果在unsigned char的范围内
        }
        sum.clear();
    }
    FreeBmpData(pdata);
    delete info;

    return white_avg;
}

// 初始化权重（黑色）
std::vector<std::vector<unsigned char>> init_weight_black(const char* filename) {
    bmpInfo* info = new bmpInfo;
    GetBmpInfo(info, filename);

    std::vector<std::vector<unsigned char>> black_avg(info->col, std::vector<unsigned char>(3, 0));     // 全都初始化为0
    U8* pdata = info->data;
    U8 BytePerPix = (info->bitCountPerPix) >> 3;
    U32 pitch = (info->col) * BytePerPix;
    int w, h;

    for (w = 0; w < info->col; w++) {
        std::vector<long long> sum(3, 0);
        for (h = info->row - 1; h >= 0; h--) {
            sum[0] += pdata[h * pitch + w * BytePerPix + 0];
            sum[1] += pdata[h * pitch + w * BytePerPix + 1];
            sum[2] += pdata[h * pitch + w * BytePerPix + 2];
        }

        for (int i = 0; i < 3; i++) {
            black_avg[w][i] = static_cast<unsigned char>(sum[i] / info->row);       // 确保结果在unsigned char的范围内
        }
        sum.clear();
    }
    FreeBmpData(pdata);
    delete info;

    return black_avg;
}


// 定义处理通道的参数结构体
struct ChannelProcessParams {
    const char* white_file;
    const char* black_file;
    std::vector<std::vector<unsigned char>>** white_weight_ptr;
    std::vector<std::vector<unsigned char>>** black_level_ptr;
};

// 线程处理函数
void process_channel_thread(ChannelProcessParams params) {
    // 处理白参考图像
    auto white_avg = init_weight(params.white_file);
    // 处理黑参考图像
    auto black_avg = init_weight_black(params.black_file);

    // 创建结果容器
    *(params.white_weight_ptr) = new std::vector<std::vector<unsigned char>>(
        white_avg.size(), std::vector<unsigned char>(3, 1));
    *(params.black_level_ptr) = new std::vector<std::vector<unsigned char>>(
        white_avg.size(), std::vector<unsigned char>(3, 0));

    // 填充计算结果
    for (size_t w = 0; w < white_avg.size(); w++) {
        for (int i = 0; i < 3; i++) {
            (**(params.white_weight_ptr))[w][i] = white_avg[w][i];
            (**(params.black_level_ptr))[w][i] = black_avg[w][i];
        }
    }
}

void update_weight_all(void) {
    // 准备参数
    ChannelProcessParams main_params = {
        "/home/root/cis_app/white.bmp",
        "/home/root/cis_app/black.bmp",
        &white1_weight,
        &black_level
    };

    ChannelProcessParams b_params = {
        "/home/root/cis_app/white_b.bmp",
        "/home/root/cis_app/black_b.bmp",
        &white1_weight_b,
        &black_level_b
    };

    // 创建线程
    std::thread main_thread(process_channel_thread, main_params);
    std::thread b_thread(process_channel_thread, b_params);

    // 等待线程完成
    if (main_thread.joinable()) main_thread.join();
    if (b_thread.joinable()) b_thread.join();
}

//这两个用来打印暗参考的值的，目前用不上，逻辑上不一定正确
// 
// 定义一个新的结构体用于返回多个值
typedef struct {
    unsigned char high8; // 无符号8位整数
    unsigned char mid8;  // 无符号8位整数
    unsigned char low8;  // 无符号8位整数
} BlackOffsetValues;

BlackOffsetValues read_black_offset(int fd) {
    unsigned int reg_value;
    BlackOffsetValues result = { 0, 0, 0 }; // 初始化为0

    // 读取 BLACK_OFFSET 寄存器的值
    read_reg(fd, BLACK_OFFSET, &reg_value);

    // 提取第16到23位的值并判断
    unsigned char high8 = (reg_value >> 16) & 0xFF;
    if (high8 > 0) {
        result.high8 = high8;
    }

    // 提取第8到15位的值并判断
    unsigned char mid8 = (reg_value >> 8) & 0xFF;
    if (mid8 > 0) {
        result.mid8 = mid8;
    }

    // 提取第0到7位的值并判断
    unsigned char low8 = reg_value & 0xFF;
    if (low8 > 0) {
        result.low8 = low8;
    }
    // 以十进制形式打印 high8, mid8, low8
    printf("high8: %d\n", result.high8);
    printf("mid8: %d\n", result.mid8);
    printf("low8: %d\n", result.low8);
    return result;
}

//这个是通过输入特定的网页，比如说chose_a,chose_b什么的，来让FPGA里面指定的寄存器的值去变成相应的值，可以通过这个来控制FPGA实现某种切换，目前这一版本没有用上，后续要改进可以用上这个
void chose_scanner(int fd, int choose) {
    unsigned int chose_data = 0;
    // 读取 CHOSE 寄存器的当前值
    read_reg(fd, CHOSE, &chose_data);

    if (choose == 1) {
        chose_data |= (1 << 31); // 最高位
        chose_data |= 1; // 最低位
    }
    else if (choose == 0) {
        chose_data &= ~(1 << 31); // 最高位
        chose_data &= ~1; // 最低位
    }
    else if (choose == 2) {
        chose_data |= (1 << 30); // 最高第二位
        chose_data |= 2; // 最低第位
       
    }
    else if (choose == 3) {
        chose_data &= ~(1 << 30); // 最高位
        chose_data &= ~2; // 最低第二位
    }

    // 写回 CHOSE 寄存器
    write_reg(fd, CHOSE, chose_data);

    read_reg(fd, CHOSE, &chose_data);
    printf("chose =%u\n", chose_data);


}

//这个是选择分辨率模式的，300dpi,600dpi,1200dpi，这个暂时不用，毕竟300dpi的图片就已经是26MB了，600dpi按尺度放大，会达到104,1200dpi的就更大了，估暂时不用
void dpi_chose(int fd, int choose) {

    unsigned int ctr_data;
    // 读取 CTR_REG寄存器的当前值
    read_reg(fd, CTR_REG, &ctr_data);

    //300dpi模式
    if (choose == 0) {
        // 写回 CHOSE 寄存器
        write_reg(fd, CTR_REG, ctr_data & ~3);
    }
    //600dpi模式
    else if (choose == 1) {
        write_reg(fd, CTR_REG, (ctr_data & ~3) | 1);
    }
    //1200dpi模式
    else if (choose == 2) {
        write_reg(fd, CTR_REG, (ctr_data & ~3) | 2);
    }

}




int init_device(unsigned long* ctr_data)
{
    int fd;
    fd = open("/dev/scanner", O_RDWR);
    if (0 > fd) {
        printf("file /dev/scanner open failed!\r\n");
        return -1;
    }


    update_weight_all();


    /* 设置缓存大小 */
    write_reg(fd, CACHE_SIZE_REG, CACHE_SIZE);

    /* 设置要扫描的行数 */
    write_reg(fd, LINE_NUM_REG, SCAN_LINE_NUM);
    write_reg(fd, INTERVAL_TIME_REG,2 );  //改为4
    /* 设置扫描模式 */
    set_scan_mode(fd, MODE1);


    ioctl(fd, CMD_ALLOC_MEM, 9);    //这里从18改到10了，10个就够了用了，用不了那么多，而且内存块太多好像存不了，不太确定，保留18先？看看情况，真的太大了
    return fd;

}




