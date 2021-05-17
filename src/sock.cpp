/*
 * @Descripttion: 
 * @version: 
 * @Author: sql
 * @Date: 2020-07-28 15:08:59
 * @LastEditors: sql
 * @LastEditTime: 2020-08-06 09:24:22
 */ 
#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <dirent.h>
#include <sstream>
#include "cjson/cJSON.h"
#include "sock.h"
#include "send_message.h"
#include "glog/logging.h"
#define MAX_PATH_SIZE 256

//解析得到的文件的信息
int get_sock_read_file_info(const char *info, vector<string> *dir_file_path)
{
    string file_path_info = info;
    istringstream pp(file_path_info);//从string中读取字符
    string f;

    DIR *d = NULL;
    struct dirent *dp = NULL;
    struct stat st = {0};
    char p[MAX_PATH_SIZE] = {0};//文件的路径
    int num = 0; //记录字符串中的单词组的个数
    for(; *info != '\0'; info++)
    {
        if(*info == ' ')
            num++;
    }
    num++;
    while(pp >> f)
    {
        num--;
        const char *path_file = f.c_str();
        if(stat(path_file, &st) < 0 && num != 0)
        {
            LOG(ERROR) << path_file <<"_oinvalid path";
            return -1;
        }
        //是目录的情况
        if(S_ISDIR(st.st_mode))
        {
            if(!(d = opendir(path_file)))
            {
                LOG(ERROR) << "opendir " << path_file << " error";
                return -1;
            }
            while((dp = readdir(d)) != NULL)
            {
                /*
                * 把当前目录中的目录， 当前目录 . , 上一级目录 .. , 以及隐藏的目录去掉
                */
                if ((!strncmp(dp->d_name, ".", 1)) || (!strncmp(dp->d_name, "..", 2)))
                    continue;
                snprintf(p, sizeof(p) - 1, "%s/%s", path_file, dp->d_name);
                stat(p, &st);
                if (!S_ISDIR(st.st_mode))
                {
                    dir_file_path->push_back(p);
                    printf("dir_file_path name:%s\n", p);
                }
                else //(假设目录路径下没有目录，如果以后有目录修改这个地方，递归)
                    continue;
            }
        }
        else
        {
            dir_file_path->push_back(path_file);
        }
    }
    LOG(INFO) << "dir_file_path size() = " << dir_file_path->size();
    return 0;
}