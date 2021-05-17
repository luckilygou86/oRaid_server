/*
 * riad.cpp
 *
 *  Created on: 2020年6月12日
 *      Author: root
 */

#include "raid_all_file.h"

#include <cstdio>
#include <string>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <vector>
#include <iostream>
#include <malloc.h>
#include <ctime>
#include "send_message.h"
#include "glog/logging.h"

using namespace std;

//输出参数的提示
static void usage(){
	cout << "usage :" << endl;
	cout << "usage : example ./riad -d path_dir -p path_file READ_SIZE_M" << endl;
}

//字符数组异或
#if 1
#define READ_SIZE 4*1024*1024 //每次进行异或字节数
static int s_riad(FILE *out_fd, const off_t file_total_size, const int dir_file_path_size, FILE *in_fd[MAX_FILE_NUM])
{
	char c0, c1;
	int num0 = 0, num1 = 0;//记录返回的个数
	unsigned int *input_buf[dir_file_path_size];//异或生成的放到input_buf[0]
	for(int i = 0; i < dir_file_path_size; i++)
	{
		input_buf[i] = (unsigned int *)malloc(READ_SIZE * sizeof(int) + 1);
		if(input_buf[i] == NULL)
		{
			LOG(ERROR) << "input_buf_" << i << "apply malloc failure";
			return ERROR_APPLY_MALLOC;
		}
	}
	int read_size = 0;//每次进行异或的字节数
	off_t xor_total_size = 0;//文件异或的大小
	printf("xor_total_size===%ld\n", xor_total_size);
	while(1)
	{
		if((file_total_size - xor_total_size) <= READ_SIZE)
			read_size = (file_total_size - xor_total_size) / 4;
		else
			read_size = (READ_SIZE) / 4;
		for(int i =0;i < dir_file_path_size; i++)
		{
			if(read_size == 0)
				break;
			if(i == 0)
			{
				num0 = fread(input_buf[i], sizeof(int), read_size, in_fd[i]);
			}
			else
			{
				num1 = fread(input_buf[i], sizeof(int), read_size, in_fd[i]);
				//当存在异或的文件不一样大小的时候
				if(num0 != num1)
				{
					LOG(ERROR) << "num0 != num1";
					for (int i = 0; i < dir_file_path_size; i++)
						free(input_buf[i]); //释放内存
					return ERROR_FREAD_FILE;
				}
				num0 = num1;
			}
		}

		if (read_size == 0)
		{
			//文件还剩与字节未读完
			if (file_total_size != xor_total_size)
			{
				for (int i = 0; i < dir_file_path_size; i++)
				{
					fseek(in_fd[i], xor_total_size, SEEK_SET);
				}
			}
			while ((c0 = fgetc(in_fd[0])) != EOF)
			{
				cout << "文件剩余字节异或";
				for (int i = 1; i < dir_file_path_size; i++)
				{
					c1 = fgetc(in_fd[i]);
					c0 ^= c1;
				}
				fputc(c0, out_fd);
				xor_total_size++;
			}
			if (file_total_size == xor_total_size)
			{
				cout << "xor_total_size = " << xor_total_size << endl;
				LOG(INFO) << "xor_total_size = " << xor_total_size;
				for (int i = 0; i < dir_file_path_size; i++)
					free(input_buf[i]); //释放内存
				return RIGHT;
			}
		}
		//作异或
		//			unsigned int *output_buf1 = output_buf;
		unsigned int *input_buf_p[dir_file_path_size];
		for (int i = 0; i < dir_file_path_size; i++)
		{
			input_buf_p[i] = input_buf[i];
		}
		//			*output_buf = input_buf_p[0];
		for (int i = 0; i < num0; i++)
		{
			for (int j = 1; j < dir_file_path_size; j++)
			{
				*input_buf_p[0] ^= *input_buf_p[j];
				input_buf_p[j]++;
			}
			input_buf_p[0]++;
		}
		int ret = fwrite(input_buf[0], sizeof(int), num0, out_fd);
		if (ret < 0)
		{
			LOG(ERROR) << "fwrite error";
			for (int i = 0; i < dir_file_path_size; i++)
				free(input_buf[i]); //释放内存
			return ERROR_FWRITE_FILE;
		}
		xor_total_size += num0 * sizeof(int);
		LOG(INFO) << "xor_total_size = " << xor_total_size;
		cout << "xor_total_size = " << xor_total_size << endl;
		if (xor_total_size == file_total_size)
		{
			for (int i = 0; i < dir_file_path_size; i++)
				free(input_buf[i]); //释放内存
			return RIGHT;
		}
		else if(xor_total_size > file_total_size)
		{
			for (int i = 0; i < dir_file_path_size; i++)
				free(input_buf[i]); //释放内存
			return ERROR_XOR_FILE;
		}
	}
}
#endif
//文件打开z
int open_file(const char *output_file_name, vector<string> a_set_of_files)
{
	int dir_file_path_size = a_set_of_files.size();
	//char *output_test = (char *)"dd_tets";//输出文件的名字
	LOG(INFO) << "open_file :" << output_file_name ;

	FILE *out_fd;				//输出文件
	FILE *in_fd[MAX_FILE_NUM];	//文件套接字，多少个文件创建多少个

	for(int i = 0; i < a_set_of_files.size(); i++)
	{
		in_fd[i] = fopen(a_set_of_files[i].c_str(), "r");
		if(in_fd[i] == NULL)
		{
			LOG(ERROR) << "open " << a_set_of_files[i] << "failure";
			return ERROR_FILE_OPEN;
		}
	}

	char tmp[1024] = {0};					//存放文件的路径
	memset(tmp, 0, 1024);
	snprintf(tmp, sizeof(tmp), "/home/file/%s", output_file_name);

	out_fd = fopen(tmp, "w");//没有此文件的话创建
	if(out_fd == NULL){
		cout << "open or creat riad_test failure" << endl;
		return ERROR_OUTPUT_FILE;
	}

	//单个字符
	#if 0
	c_riad(fd0, fd1, fd2);
	//int数组异或
	#else
	off_t size0 = 0, size1 = 0;//分别记录前后两个文件的大小
	//获取输入文件的大小
	//判断输入文件大小是否相同，不一样大报错返回。
	struct stat info = {0};
	for(int i = 0; i < a_set_of_files.size(); i++)
	{
		if(i == 0)
		{
			lstat(a_set_of_files[i].c_str(), &info);
			size0 = info.st_size;
		}
		else	
		{
			memset(&info, 0, sizeof(struct stat));
			lstat(a_set_of_files[i].c_str(), &info);
			size1= info.st_size;
			if(size0 != size1)
			{
				LOG(ERROR) << "file1_size != file2_size";
				return ERROR_FILE_SIZE;
			}
			else
				size0 = size1;
		}
	}
	int ret = s_riad(out_fd, size0, dir_file_path_size, in_fd);
	if(ret < 0)
		return ret;
	#endif
	
	LOG(INFO) << "start_fclose_fd0";
	for(int i = 0; i < a_set_of_files.size(); i++)
	{
		LOG(INFO) << "star_fclose_" << a_set_of_files[i].c_str();
		fclose(in_fd[i]);
		LOG(INFO) << "end_fclose_" << a_set_of_files[i].c_str();
	}
	fclose(out_fd);
	return RIGHT;
}

#if 0
static int open_dir(const char* path_dir)
{
	/*
	 * DIR ： directory stream, opendir打开一个目录创建一个目录流， 供readdir调用
	 * readdir返回dirent结构体
	 * dirent结构体包括：节点号和文件名字
	 * stat取得指定文件的文件属性，文件属性存储在结构体stat里
	 */
	DIR *d = NULL;
	struct dirent *dp = NULL;
	struct stat st = {0};
	char p[MAX_PATH_SIZE] = {0};
	//stat取得指定的文件的属性， 文件属性储存在结构体stat中
	if(stat(path_dir, &st) < 0 || !S_ISDIR(st.st_mode))
	{
        printf("invalid path: %s\n", path_dir);
        return -1;
	}
	//打开目录文件
	if(!(d = opendir(path_dir)))
	{
		printf("opendir[%s] error : %m\n", path_dir);
		return -1;
	}
	//读取目录中的文件
	while((dp = readdir(d)) != NULL)
	{
		/*
		 * 把当前目录中的目录， 当前目录 . , 上一级目录 .. , 以及隐藏的目录去掉
		 */
		if((!strncmp(dp->d_name, ".", 1)) || (!strncmp(dp->d_name, "..", 2)))
			continue;
		snprintf(p, sizeof(p)-1, "%s%s", path_dir, dp->d_name);
		stat(p, &st);
		if(!S_ISDIR(st.st_mode))
			dir_file_path.push_back(p);
		else
			continue;
	}
	LOGD <<  "dir_file_path.size = " << dir_file_path.size();
	return 0;
}

int main (int argc,char **argv)
{

	clock_t start,finish;
	double total_time ;//总时间
	start = clock();//开始的时间

	if(argc != 6)
	{
		usage();
		return 0;
	}

	char log_path[1024] = {0};
	//创建日志文件
	sprintf(log_path, "./riad.log");
	plog::init(plog::debug, log_path);
	//
	READ_SIZE = atoi(argv[5]) * 1024 * 1024;


	int ret = open_dir(argv[2]);
	if(ret < 0)
	{
		cout << "open dir filture" << endl;
		return ret;
	}
	ret = open_file(argv[4]);
	if(ret < 0)
	{
		cout << "produced riad_file filture" << endl;
		return ret;
	}
	finish = clock();//结束时间
	total_time = (double)(finish - start)/CLOCKS_PER_SEC;
	cout << "program tatal_time is : " << total_time << endl;
	return 0;
}
#endif
