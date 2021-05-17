/* 
 * @Descripttion: 
 * @version: 
 * @Author: sql
 * @Date: 2020-07-28 15:08:59
 * @LastEditors: sql
 * @LastEditTime: 2020-09-11 14:07:10
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "glog/logging.h"

//构造http发送的信息
void set_output_http_info(char *send_http_info, const char *send_http_content, const char *http_header, const char *content_type)
{
    strcpy(send_http_info, http_header);
    strcat(send_http_info, " 200 OK");
    strcat(send_http_info, "\r\nContent-Type: ");
    strcat(send_http_info, content_type);
    strcat(send_http_info, "\r\nServer: localhost");
    strcat(send_http_info, "\r\nContent-Length: ");
    char buffer[20] = {0};
    sprintf(buffer, "%lu", strlen(send_http_content));
    strcat(send_http_info, buffer);
    time_t rawtime;
    time(&rawtime);
    strcat(send_http_info, "\r\nDate: ");
    strcat(send_http_info, ctime(&rawtime));
    strcat(send_http_info, "\r\n");
    strcat(send_http_info, send_http_content);
    return;
}

//输出http信息解析
void get_input_http_info(const unsigned char *buffer, char *content, char *method, char *header, char *content_type)
{
    LOG(INFO) << "buffer is : " << buffer; 
    char *p = (char *)buffer;
    int i = 0;
    int j = 0;
    char buf[1024] = {0};
    while (p[i])
    {
        if (p[i] != '\r')
        {
            i++;
            continue;
        }
        memset(buf, 0, 1024);
        strncpy(buf, (char *)buffer + j, i - j);
        LOG(INFO) << "receive buf===" << buf;
        if (j == 0)
        {
            //
            //获取method和header
            char *buf1 = strrchr(buf, ' '); //和strchr一样，只不过从后向前
            char *buf2 = strchr(buf, ' ');  //查找字符' '，返回第一次在buf出现的位置，未找到返回NULL
            if (buf2 == NULL && buf1)
            {
                if (strlen(buf1) > 1)
                    strcpy(method, buf1 + 1);
            }
            else if (buf1 && buf2)
            {
                if (strlen(buf1) > 1)
                    strcpy(header, buf1 + 1);
                strncpy(method, buf2 + 2, strlen(buf2) - strlen(buf1) - 2);
            }

            printf("method is===%s\n", method);
            LOG(INFO) << "method is : " << method;
            printf("header is===%s\n", header);
            LOG(INFO) << "header is : " << header;
        }
        else
        {
            if (strlen(buf) > 14 && strncmp(buf, "Content-Type:", 13) == 0)
            {
                strcpy(content_type, buf + 14);
                LOG(INFO) << "content_type is:" << content_type;
            }
        }
        i++;
        i++;
        j = i;
    }
    strncpy(content, (char *)buffer + j, i - j);
    LOG(INFO) << "content is:" << content;
}