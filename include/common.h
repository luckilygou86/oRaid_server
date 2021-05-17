/*
 * @Descripttion: 用到的的函数比较多的，大概都能用到的，解析发送过来的rest指令，和构造发送的rest指令
 * @version: 
 * @Author: sql
 * @Date: 2020-08-06 09:12:42
 * @LastEditors: sql
 * @LastEditTime: 2020-09-12 10:59:15
 */
#ifndef COMMON_H_
#define COMMON_H_

#include <vector>
#include <cstring>
#include <pthread.h>
using namespace std;


//存放数据的服务器的ip地址和端口号方法名
struct tDataServerInfo
{
    char data_server_ip[32];
    char data_server_port[16];
    char data_server_method[128] = {0};  //方法
    
};


/**
 * 解析通过rest服务发送过来的json指令
 * @param buffer:rest服务发送过来的指令;content：json信息;method:方法;header:包头;content_type: 数据格式
 * @return {无} 
 */
void get_input_http_info(const unsigned char *buffer, char *content, char *method, char *header, char *content_type);
//输出http信息解析
/**
 * 构造rest服务发送的指令
 * param 
 * -send_http_info:打包的rest发送的指令;
 * -send_http_content:打包的json指令;
 * -http_header：包头;
 * -content_type:数据格式
 * return 无
 * */
void set_output_http_info(char *send_http_info, const char *send_http_content, const char *http_header, const char *content_type);

#endif