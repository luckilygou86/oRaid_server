/*
 * @Descripttion: 发送数据给别的端口
 * @version: 
 * @Author: sql
 * @Date: 2020-08-05 09:08:14
 * @LastEditors: sql
 * @LastEditTime: 2020-09-07 13:29:41
 */
#ifndef SEND_FILE_NAME_CLIENT_H_
#define SEND_FILE_NAME_CLIENT_H_

#include <vector>
#include <cstring>
#include <iostream>

using namespace std;

/**
 * 把文件的路径发送给存放文件的异地服务器
 * -a_set_of_files: 一组文件的路径
 * -file_attribyte: 得到文件服务器返回的文件的属性信息（此处得到的文件的大小
 * 其中包括了设置发送的json信息函数
 * 返回值：ERROR_SEND_FILE_NAME_CLIENT，当发送失败的时候（网络断开的情况）返回的值;RIGHT,发送成功
 * */
int send_file_name_main(vector<string> a_set_of_files, char *file_attribyte);

/**
 * 服务器和客户端连接的通信，write
 * -fd：连接的套接字
 * -buffer：发送的buf
 * -length：发送的长度
 * 返回值：WRITE_EXEC_ERROR，写失败;RIGHT,成功
 * */
int write_buf(int fd, void *buffer, int length);


#endif // !SEND_FILE_NAME_CLIENT_H_
