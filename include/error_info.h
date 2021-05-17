#ifndef ERROR_INFO_H_
#define ERROR_INFO_H_

#define     CLIENTSOCKETFAILED      0x9008          //创建socket失败
#define     CONNECTSERVERFAILED     0x9009          //连接服务器失败
#define     RECEIVE_MEASSAGE_DISORDER 0x900A        //服务端和客户端信息不对应
#define     OPEN_FILE_FAILED        0x9001          //打开文件失败
#define     READ_ORIGIN_FILE_FAILED 0x9002          //读原始文件失败
#define     OPEN_DIR_FAILED         0x9003          //打开目录失败
#define     LSEEK_EXEC_ERROR        0x9004          //LSEEK失败
#define     READ_EXEC_ERROR         0x9005         //read函数失败
#define     WRITE_EXEC_ERROR        0x9006          //write函数失败





#endif /* ERROR_INFO_H_ */

