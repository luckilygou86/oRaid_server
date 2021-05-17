/*
 * @Descripttion: 解析接受的rest服务发送过来的json指令，打包json发送给对应的客户端或服务器
 * @version: 
 * @Author: sql
 * @Date: 2020-07-28 15:08:59
 * @LastEditors: sql
 * @LastEditTime: 2020-09-11 14:15:13
 */ 
#ifndef JSON_H_
#define JSON_H_
#include <vector>
#include <string>
#include "file_info.h"

using namespace std;

/**
 * 设置发送给对应的客户json（错误正确的消息
 * -senf_http_content:  返回客户端的connect
 * -ret:    代表的信息码
 * 返回值：无
 * */
void set_send_json_info(char *send_http_content, int ret);

/**
 * 设置发送给客户端已经异或完成的字节数，可能时主动的也可能是被动的
 * param:
 * -senf_http_content:  返回客户端的connect
 * -file_size：文件的总大小
 * -xor_size:异或的文件的大小
 * */
void set_send_json_xor_size(char *send_http_content, const off_t file_size, const off_t xor_size);

/**
 * 解析发送过来的json关于异或文件的文件名信息
 * -info:   发送过来的content
 * -a_set_of_files: 一组文件名/路径的信息（文件数量大于2
 * -output_file_name:   输出的文件路径信息（数量等于1
 * 全局的变量的赋值，发送过来的文件所对应的ip，port，method
 * 返回值：ERROR_CJSON_FORMAT，当json格式，输出文件名不存在，输入文件小于2等发生错误;RIGHT，正确
 * */
int get_json_need_xor_file_info(const char* info, vector<string> *a_set_of_files, char *output_file_name);

/**
 * 设置发送存在异地文件的json信息
 * -send_http_content： 发送的content，数据信息
 * -send_http_ip_port_method：  把ip:port/method封装到一起
 * -file_name_total：   存放文件路径的容器
 * 封装成json
 * 返回值：无
 * */
void set_send_nonlocal_http_content(char *send_http_content, char *send_http_ip_port_method, vector<string> file_name_total);

/**
 * 解析发送过来的json文件的大小
 * -info:   发送过来的content
 * -file_size:  文件的大小
 * 返回值：ERROR_FILE_NAME，名字错误;ERROR_FILE_SIZE:这组文件的文件大小不一致
 * */
int get_json_nonlocal_file_attribute_info(const char *info, off_t &file_size);

//处理rest发送过来的本地json信息()
//暂时用不到了
int get_json_read_file_info(const char *info, char *return_output_file_name, vector<string> *dir_file_path);

//得到所有文件的路径
int get_total_json_file_path(const char *path_dir, vector<string> *zdir_file_path);


#endif
