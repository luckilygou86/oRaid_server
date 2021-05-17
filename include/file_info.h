/*
 * @Descripttion: 
 * @version: 
 * @Author: sql
 * @Date: 2020-07-28 18:11:57
 * @LastEditors: sql
 * @LastEditTime: 2020-08-07 13:51:12
 */
/**
 *连接存取数据服器的客户端程序
 
*/

#ifndef ORAID_SERVER_H_
#define ORID_SERVER_H_

enum
{
	CMD_READ = 1,
	CMD_WRITE = 2,
	CMD_REMOVE = 3,
};

#define CMD_LEN    1065

typedef struct  //16
{
	unsigned char synCode[8];
	unsigned char synNum[4];
	unsigned char status;
	unsigned char message;
	unsigned char ReturnLenHighByte;
	unsigned char ReturnLenLowByte;
} CMD_HEADER_T;//头消息

typedef struct//1049
{
    CMD_HEADER_T headerObj;
    unsigned char command;	
	unsigned int mode;
	char file_name[1024];
	unsigned long long offset;
	unsigned long long data_len;
	unsigned char reserved[4];
}__attribute__ ((packed)) CMD_WRITE_T;

typedef struct
{
	CMD_HEADER_T headerObj; //16
	unsigned char command; //1
	char file_name[1024];
	unsigned long long offset; //8
	unsigned long long data_len;//8
	unsigned long long reply_value;//8
}__attribute__ ((packed)) CMD_REPLY_WRITE_T;

typedef struct 
{
	CMD_HEADER_T headerObj;//16
	unsigned char command; //1
	char file_name[1024];
	unsigned long long offset;//8
	unsigned long long data_len; //8
	unsigned char reserved[8];
}__attribute__ ((packed)) CMD_READ_T;

typedef struct
{
	CMD_HEADER_T headerObj;
	unsigned char command;
	char file_name[1024];
	unsigned long long offset;
	unsigned long long data_len;
	unsigned long long reply_value;
}__attribute__ ((packed)) CMD_REPLY_READ_T;

typedef struct
{
        CMD_HEADER_T headerObj;//16
        unsigned char command; //1
	char file_name[1024];
	unsigned char reserved[24];
}__attribute__ ((packed)) CMD_REMOVE_T;

typedef struct
{
        CMD_HEADER_T headerObj;
        unsigned char command;
        char file_name[1024];
	unsigned long long reply_value;
	unsigned char reserved[16];
}__attribute__ ((packed)) CMD_REPLY_REMOVE_T;

#endif

