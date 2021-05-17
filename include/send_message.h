/*
 * @Descripttion: 一些错误消息
 * @version: 
 * @Author: sql
 * @Date: 2020-07-28 15:08:59
 * @LastEditors: sql
 * @LastEditTime: 2020-09-07 14:36:48
 */ 
#ifndef MACRO_H
#define MACRO_H

/*server send client*/

#define RIGHT 0
#define ERROR_FILE_PATH     -1      //文件路径
#define ERROR_FILE_SIZE     -2      //文件大小
#define ERROR_FILE_OPEN     -3      //打开文件
#define ERROR_APPLY_MALLOC  -4      //申请空间
#define ERROR_FREAD_FILE    -5      //读文件
#define ERROR_FWRITE_FILE   -6      //写文件
#define ERROR_CJSON_PARSE   -7      //jion格式
#define ERROR_CJSON_FORMAT  -8      //json对象
#define ERROR_OUTPUT_FILE   -9      //输出文件
#define ERROR_METHOD_NOT_FOUND -10  //方法

#define ERROR_XOR_FILE      -11     //异或失败


#define     ERROR_CREAT_TRANSMISSION_DATA_CONNECTION -101    //创建数据连接服务器失败
#define     ERROR_GET_FILE_DATA -102
#define     ERROR_SEND_FILE_NAME_CLIENT -103


#define     ERROR_FILE_NAME -21 //文件路径错误


enum
{
    RAID0 = 0x1000,
    RAID1 = 0x1001,
    RAID2 = 0x1002,
    RAID3 = 0x1003,
    RAID4 = 0x1004,
    RAID5 = 0x1005
};

#endif 