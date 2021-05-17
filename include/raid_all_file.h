/*
 * @Descripttion: 本地文件异或
 * @version: 
 * @Author: sql
 * @Date: 2020-07-28 15:08:59
 * @LastEditors: sql
 * @LastEditTime: 2020-09-08 15:35:24
 */ 
#ifndef RAID_ALL_FILE_H
#define RAID_ALL_FILE_H
#include <vector>
#include <string>
using namespace std;

#define MAX_FILE_NUM 1024   //文件最大的数量

/**
 * 本地文件打开打开文件
 * -output_file_name:    输出文件
 * -dir_file_path:  一组文件名字
 * 返回值：ERROR_FILE_OPEN，打开文件失败;ERROR_OUTPUT_FILE:打开/创建输入文件失败;ERROR_FILE_SIZE:这一组文件大小不一值
 * ret，异或函数发生错误的返回值;RIGHT，正确
 * */
int open_file(const char *output_file_name, vector<string> a_set_of_files); //进行异或

/**
 * 本地文件异或的函数
 * -out_fd:  输出文件的文件句柄
 * -file_total_size:    文件的大小
 * -in_fd:  输入文件的文件句柄
 * 思路：把所有的文件都拿出相同的部分进行依次异或生成新的buf存到输出文件里，每次4个字节进行异或，当异或到最后，
 * 剩下小于四个字节时候，用fseek把文件偏移到剩余的字节的位置，再单个字节进行异或
 * 返回值：ERROR_APPLY_MALLOC,申请内存失败;ERROR_FREAD_FILE，fread文件失败;ERROR_FILE_SIZE,读到的文件大小不一致
 * RIGHT，异或成功;ERROR_FWRITE_FILE，写到输出文件失败;ERROR_XOR_FILE,异或失败
 * */
static int s_riad(FILE *out_fd, const off_t file_total_size, const int dir_file_path_size, FILE *in_fd[MAX_FILE_NUM]);

#endif // !RAID_ALL_FILE_H