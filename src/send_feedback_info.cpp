/*
 * @Descripttion: 
 * @version: 
 * @Author: sql
 * @Date: 2020-08-05 09:07:50
 * @LastEditors: sql
 * @LastEditTime: 2020-09-07 13:16:07
 */
#include "send_feedback_info.h"


#include "restclient-cpp/restclient.h"
#include "restclient-cpp/connection.h"

#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cstring>
#include <iostream>

#include "glog/logging.h"
#include "json.h"
#include "send_message.h"
#include "error_info.h"
#include "common.h"

using namespace std;

#define BUF_MAX 1024 * 16

//发送文件名字给文件服务器 获取到文件属性信息
int send_file_name_main(vector<string> a_set_of_files, char *file_attribyte)
{
    char send_http_content[BUF_MAX] = {0};	   	//存放文件名的jaon
	char send_http_ip_port_method[1024] = {0}; 	//ip:port/method
	
	vector<string> file_name_t = (vector<string>)a_set_of_files;	//文件名的容器
	memset(send_http_content, 0, strlen(send_http_content));

	set_send_nonlocal_http_content(send_http_content, send_http_ip_port_method, file_name_t);

	RestClient::Response r = RestClient::post(send_http_ip_port_method, "application/json", send_http_content, 3);
	
	for (int k = 0; k < 3; k++)
	{
		if (r.code != 200)
		{
			cout << "Try to connect : " << k << endl;
			LOG(INFO) << " send " << send_http_content << "code = " << r.code << " : The network impassability";
			sleep(2);
			r = RestClient::post(send_http_ip_port_method, "application/json", send_http_content, 3);
		}
		else
			break;
		if (k == 2)
		{
			LOG(ERROR) << "连接存放文件的服务器失败";
			LOG(ERROR) << "r.code = " << r.code << ", r.body = " << r.body;
			return ERROR_SEND_FILE_NAME_CLIENT;
		}
	}
	
    LOG(INFO) << "返回的文件的属性的json" << r.body;
	cout << r.body;
	//解析返回的信息
	strcpy(file_attribyte, r.body.c_str());

    return RIGHT;
}

//发送传输数据的反馈消息
int write_buf(int fd, void *buffer, int length)
{
    LOG(INFO) << "write file start";
	int bytes_left;				//写的字节数
	int written_bytes;			//write返回的字节数
	char *ptr = (char *)buffer; //写的buf
	bytes_left = length;
	while (bytes_left > 0)
	{
		written_bytes = write(fd, ptr, bytes_left);
		if (written_bytes <= 0)
		{
			if (errno == EINTR) //函数调用被中断
				written_bytes = 0;
			else if (errno == EAGAIN) //资源0是不可用，稍作再次调用或成功
				break;
			else
			{
				LOG(ERROR) << "write exec happen error";
				return -WRITE_EXEC_ERROR;
			}
		}
		bytes_left -= written_bytes;
		ptr += written_bytes;
	}
	LOG(INFO) << "write file end";
	return 0;
}

