#include "json.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include "send_message.h"
#include "cjson/cJSON.h"
#include "glog/logging.h"
#include "common.h"

#define MAX_PATH_SIZE 256

//data服务器的信息
struct tDataServerInfo g_tDataDerverInfo;

extern struct tGroupFileInfo g_tgroupFile;

void set_send_json_info(char *send_http_content, int ret)
{
	char message[256] = {0};
	char c_errno[16] = {0};
	sprintf(c_errno, "%d", errno);
	switch (ret)
	{
		case RIGHT:
			strcpy(message, "success!");
			break;
		case ERROR_FILE_PATH:
			strcpy(message, "file path error");
			//http_send_content = strcat(error_file_path, " is file path error!");
			break;
		case ERROR_FILE_SIZE:
			strcpy(message, "File sizes are inconsistent");
			break;
		case ERROR_FILE_OPEN:
			strcpy(message, c_errno);
			strcat(message, ",Error opening file,");
			strcat(message, strerror(errno));
			break;
		case ERROR_APPLY_MALLOC:
			strcpy(message, c_errno);
			strcat(message, ",Request memory error,");
			strcat(message, strerror(errno));
			break;
		case ERROR_FREAD_FILE:
			strcpy(message, c_errno);
			strcat(message, ",The fread file failed,");
			strcat(message, strerror(errno));
			break;
		case ERROR_FWRITE_FILE:
			strcpy(message, c_errno);
			strcat(message, ",The fwrite file failed,");
			strcat(message, strerror(errno));
			break;
		case ERROR_CJSON_PARSE:
			strcpy(message, "error cjson parse");
			break;
		case ERROR_OUTPUT_FILE:
			strcpy(message, c_errno);
			strcat(message, ",Error opening output file,");
			strcat(message, strerror(errno));
			break;
		case ERROR_METHOD_NOT_FOUND:
			strcpy(message, "Error method not found");
			break;
		default:
			strcpy(message, "unknown error");
			break;
	}
	sprintf(send_http_content, "{\"code\":\"%d\", \"message\":\"%s\"}", ret, message);
}

void set_send_json_xor_size(char *send_http_content, const off_t file_size, const off_t xor_size)
{
	sprintf(send_http_content, "文件的总字节数：%jd, 文件已经异或的字节数为：%jd", file_size, xor_size);
}


pthread_mutex_t server_mutex = PTHREAD_MUTEX_INITIALIZER;
//处理rest发送过来的不是本地json信息(得到ip、port、method、一组文件名)
int get_json_need_xor_file_info(const char* info, vector<string> *a_set_of_files, char *output_file_name)
{
	//错误标志
	int sign = 0;

	cJSON *json, *arrayItem, *item, *object;
	json = cJSON_Parse(info);
	LOG(INFO) << info;
	if(!json)
	{
		LOG(ERROR) << "ERROR get_json_nonlocal_file_info cJson parse: " << cJSON_GetErrorPtr();;
		return ERROR_CJSON_PARSE;
	}

	//放文件的ip地址
	if((arrayItem = cJSON_GetObjectItem(json, "data_server_ip")) != NULL)
	{
		LOG(INFO) << "data_server ip is : " << arrayItem->valuestring;
		pthread_mutex_lock(&server_mutex);
		strcpy(g_tDataDerverInfo.data_server_ip, arrayItem->valuestring);
		cout << "data_ip : " << g_tDataDerverInfo.data_server_ip << endl;;
		pthread_mutex_unlock(&server_mutex);
	}
	else
		sign = -1;
	//port
	if((arrayItem = cJSON_GetObjectItem(json, "data_server_port")) != NULL)
	{
		
		pthread_mutex_lock(&server_mutex);
		strcpy(g_tDataDerverInfo.data_server_port, arrayItem->valuestring);
		pthread_mutex_unlock(&server_mutex);
		LOG(INFO) << "data_server port is : " << g_tDataDerverInfo.data_server_port;
	}
	else
		sign = -1;
	//方法
	if((arrayItem = cJSON_GetObjectItem(json, "data_server_method")) != NULL)
	{
		
		pthread_mutex_lock(&server_mutex);
		strcpy(g_tDataDerverInfo.data_server_method, arrayItem->valuestring);
		pthread_mutex_unlock(&server_mutex);
		LOG(INFO) << "data_server method is : " << g_tDataDerverInfo.data_server_method;
	}
	else
		sign = -1;
	//输出文件
	if((arrayItem = cJSON_GetObjectItem(json, "output_file_name")) != NULL)
	{
		strcpy(output_file_name, arrayItem->valuestring);
		LOG(INFO) << "output_file name is : " << output_file_name;
		
	}
	else
		sign = -1;

	if((arrayItem = cJSON_GetObjectItem(json, "file_name_total")) != NULL)
	{
		int size = cJSON_GetArraySize(arrayItem);
		LOG(INFO) << "nonlocal file num is : " << size;
		
		for(int i = 0; i < size; i++)
		{
			object = cJSON_GetArrayItem(arrayItem, i);
			//文件名字
			if((item = cJSON_GetObjectItem(object, "file_path")) != NULL)
				a_set_of_files->push_back(item->valuestring);
			else
				sign = -1;
		}
	}
	else
		sign = -1;
	if(sign != 0)
	{
		LOG(ERROR) << "json格式错误";
		return ERROR_CJSON_FORMAT;
	}
	return RIGHT;
}

//设置发送给存放异地文件服务器的文件名
void set_send_nonlocal_http_content(char *send_http_content, char *send_http_ip_port_method, vector<string> file_name_total)
{
	strcat(send_http_content, "{\"nonlocal_file_name\": [");
	for(int i = 0; i < file_name_total.size();i++)
	{
		strcat(send_http_content, "{\"file_name\": \"");
		strcat(send_http_content, file_name_total[i].c_str());
		strcat(send_http_content, "\"}");
		if(i != (file_name_total.size() - 1))
			strcat(send_http_content, ",");
	}
	strcat(send_http_content, "]}");
	LOG(INFO) << "send nonlocal content is : " << send_http_content; 
	strcpy(send_http_ip_port_method, g_tDataDerverInfo.data_server_ip);
	strcat(send_http_ip_port_method, ":");
	strcat(send_http_ip_port_method, g_tDataDerverInfo.data_server_port);
	strcat(send_http_ip_port_method, "/");
	strcat(send_http_ip_port_method, g_tDataDerverInfo.data_server_method);
	LOG(INFO) << "send nonlocal http ip_port_method is : " << send_http_ip_port_method;
}
//解析得到的文件的大小信息
int get_json_nonlocal_file_attribute_info(const char *info, off_t &file_size)
{
	cJSON *json, *item;
	json = cJSON_Parse(info);
	if(!json)
	{
		LOG(ERROR) << "ERROR get_json_nonlocal_file_info cJson parse: " << cJSON_GetErrorPtr();
		return ERROR_FILE_NAME;
	}
	//文件信息正确
	if((item = cJSON_GetObjectItem(json, "file_total_size")) != NULL)
	{
		file_size = atoi(item->valuestring);
		LOG(INFO) << "file size is " << file_size;
	}
	//当文件名字没有的时候
	if((item = cJSON_GetObjectItem(json, "error_file_name")) != NULL)
	{
		LOG(ERROR) << "发送的文件名字在服务器没有找到";
		return ERROR_FILE_NAME;
	}
	//文件大小不一致
	if((item = cJSON_GetObjectItem(json, "error_file_size")) != NULL)
	{
		LOG(ERROR) << "文件大小不一致";
		return ERROR_FILE_SIZE;
	}

	return RIGHT;
}


//处理rest发送过来的json信息
int get_json_read_file_info(const char *info, char *return_output_file_name, vector<string> *dir_file_path)
{
	int output_sign = 0;//存在输出文件的标志

	struct stat st = {0};//存取文件的信息

	cJSON *json, *arrayItem, *item, *object;
	int i;
	json = cJSON_Parse(info);
	if (!json)
	{
		LOG(ERROR) << "Error before : " << cJSON_GetErrorPtr();
		return ERROR_CJSON_PARSE;
	}
	else
	{
		arrayItem = cJSON_GetObjectItem(json, "path_file");
		if (arrayItem != NULL)
		{
			int size = cJSON_GetArraySize(arrayItem);
			printf("cJSON_GetArraySize: size=%d\n", size);

			for (i = 0; i < size; i++)
			{
				//printf("i=%d\n",i);
				object = cJSON_GetArrayItem(arrayItem, i);
				//cout << cJSON_Print(object) << endl;

				//文件名
				if ((item = cJSON_GetObjectItem(object, "input_file_path")) != NULL)
				{
					if(stat(item->valuestring, &st) < 0)
					{
						//strcpy(error_file_path, item->valuestring);
						return ERROR_FILE_PATH;
					}
					dir_file_path->push_back(item->valuestring);
				}
				//raid文件
				else if ((item = cJSON_GetObjectItem(object, "output_file_path")) != NULL)
				{
					strcpy(return_output_file_name, item->valuestring);
					LOG(ERROR) << "return_output_file_name is : " << return_output_file_name;
				}
				//目录文件名
				else if ((item = cJSON_GetObjectItem(object, "input_dir_path")) != NULL)
				{
					char *path_dir = item->valuestring;
					int ret = get_total_json_file_path(path_dir, dir_file_path);
					if(ret < 0)
						return ret;
				}
			}
		}
		return RIGHT;
	}
}
//检索目录下的文件
int get_total_json_file_path(const char *path_dir, vector<string> *dir_file_path)
{
	DIR *d = NULL;
	struct dirent *dp = NULL;
	struct stat st = {0};
	char p[MAX_PATH_SIZE] = {0};
	//stat取得指定的文件的属性， 文件属性储存在结构体stat中
	if (stat(path_dir, &st) < 0 || !S_ISDIR(st.st_mode))
	{
		LOG(ERROR) << "oinvalid path don't dir " << path_dir;
		return ERROR_FILE_PATH;
	}
	//打开目录文件
	if (!(d = opendir(path_dir)))
	{
		LOG(ERROR) << "opendir " << path_dir << "error";
		return ERROR_FILE_OPEN;
	}
	//读取目录中的文件
	while ((dp = readdir(d)) != NULL)
	{
		/*
		* 把当前目录中的目录， 当前目录 . , 上一级目录 .. , 以及隐藏的目录去掉
		*/
		if ((!strncmp(dp->d_name, ".", 1)) || (!strncmp(dp->d_name, "..", 2)))
			continue;
		snprintf(p, sizeof(p) - 1, "%s/%s", path_dir, dp->d_name);
		stat(p, &st);
		if (!S_ISDIR(st.st_mode))
		{	
			dir_file_path->push_back(p);
			//printf("dir_file_path_name:%s\n", p);
		}
		else //(假设目录路径下没有目录，如果以后有目录修改这个地方，递归)
			continue;
	}
	printf("dir_file_path size: %zu\n", dir_file_path->size());
	LOG(ERROR) << "dir_file_path size = " << dir_file_path->size();
	closedir(d);
	return RIGHT;
}