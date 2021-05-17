/*
 * @Descripttion: 和存放数据的服务器进行通信连接，请求数据，得到数据之后在进行异或，数据边传输边异或，一个文件创建一个连接
 *              每个文件都有两个buf进行轮循存放传送过来的数据
 * @version: 
 * @Author: sql
 * @Date: 2020-08-05 11:32:34
 * @LastEditors: sql
 * @LastEditTime: 2021-05-11 10:09:30
 */
#ifndef GET_DATA_CLIENT_FILE_DATA_H_
#define GET_DATA_CLIENT_FILE_DATA_H_
#include <cstring>
#include <cstdio>
#include <vector>
#include <iostream>

using namespace std;

#define MAX_CONNECT_SOCKET_NUM 65536
#define MAXLINE 2048000     //read读到
#define READ_BUF_SIZE 100 * 1024 * 1024	//异或buf的大小
#define READ_BUF 10 * 1024 * 1024


/* 把传过来的一组数据保存的结构体和异或成功或者失败的一些信息 */
struct in_file_info
{
    
    unsigned char *in_buf0;
    unsigned char *in_buf1;
    off_t   buf_size[2];//记录buf存取了多少字节，是否存满
    // off_t   buf2_size;//记录读取多长
    off_t   cp_size;

    //off_t   write_size; //异或了总共多少字节

    // off_t write_test_size[2];
    //off_t write_buf_test;   //记录每个线程读取的总共字节数
    
    int buf_flag[2]; //0：空的状态    1:正在存储  2:buf储存结束
    // int buf2_flag[100]; //0：空的状态    1:正在存储  2:buf储存结束

    //int buf_full_num[2];  //总共多少文件buf存好的标志，存好一个加一个
    // int buf2_full_num;
    /* int xor_flag;       //异或标志 --- 未0      异或完 1
    int correct_flag;   //错误的标志为 正确0     错误-1 */
};

////一个文件信息
struct single_file_info
{
    struct  in_file_info in_buf_info;
    int     fd; // The clients socket.
    int     file_fd;            //文件套接字
    
    struct  event_base *base;
    struct  event *ev_accept;
    
    int     write_state;        //写的状态
    size_t  write_index;        //实际写入的数据长度
    size_t  write_total_size;   //需要写入数据的总长度

    char    file_path[1024];    //文件路径

   // off_t   file_block_size;
    off_t   file_total_size;    //文件总大小
    off_t   file_index;         //文件读取的总字节数
    
    off_t   write_offset;       //需要从文件写入的offset
};

//一组文件的信息
struct tGroupFileInfo
{
    //vector<string> group_file_name;
	//string group_file_name[100];//一组文件
    struct  single_file_info single_file_array[50];
    //char    group_file_path[100][100];//一组文件
	off_t   file_size;      //文件大小
    size_t  file_num = 0;   //文件数量
    int     not_transfer_end_num;   //没有传输完的数量
    int     group_empty;          //判断文件组是否为空，空：0     不为空：1
    
    off_t   xor_size;       //异或了总共多少字节
    int     buf_full_num[2];//总共多少文件buf存好的标志，存好一个加一个
    int     xor_flag;       //异或标志 --- 未0      异或完 1
    int     correct_flag;   //错误的标志为 正确0     错误-1
};


/**
 * 每个文件一个线程，与存放数据的服务器进行连接，把文件的信息存放到全局的文件组结构体变量里
 * param ：
 * -file_size:文件的大小
 * -file_name:存放一组文件路径的容器
 * -output_file_name：异或生成的文件的名字
 * return:
 * ERROR_CREAT_TRANSMISSION_DATA_CONNECTION：错误
 * RIGHT：传送并且异或正确
 * */
int create_client_connect_data_server(const off_t file_size, const vector<string> file_name, const char *output_file_name);

/**
 * 判断是否达到异或的条件
 * param :
 * -out_fd:输出文件的文件句柄
 * return：
 * ERROR_CREAT_TRANSMISSION_DATA_CONNECTION:存在失败的过程
 * RIGHT：异或正确
 * */
static int judge_xor_condition(FILE *out_fd);
/**
 * 进行异或的函数，每次四个字节进行一或，把char转化成int类型的，当一或到了小于四个字节的时候进行单个字节异或
 * param:
 * -inbuf[]:异或的文件buf
 * -file_num：文件的数目，大于等于2
 * -out_fd：输出文件的文件句柄
 * -k：每个文件总共两个buf轮循存放数据，k代表的是哪一个buf
 * return：-1,失败;0，成功
 * */
static int buf_xor(unsigned char *in_buf[], int file_num, FILE *out_fd, int k);

/**
 * 线程函数，每个文件都与服务器创建一个连接，连接创建生成新的连接套接字，此时对这个套接字可以通过setsockopt设置发送缓冲区接受缓冲区和是否阻塞
 * 发送头部消息，用libevent创建事件检测触发事件的状态EV_READ | EV_PERSIST
 * EV_READ:当socket进行读取的时候
 * EV_PERSIST：事件的持续化，这个事件会保存挂起的状态,非挂起的时，event_del,event_add可以设置超时调用
 * 在这里设置了超时60s当达到60s了触发事件，调用函数
 * param:
 * -arg：当前的文件在文件组中的下标
 * return：无
 * */
static void *process_in_new_thread_file_num(void *arg);

/**
 * 接受发送过来的文件的数据，发送端每次发送的时候都加上个包头，所以要判断接收是否首次接收
 * param:
 * sock_fd：客户端与服务连接的套接字
 * event：事件触发状态EV_READ | EV_PERSIST调用函数
 * arg：当前的文件在文件组中的下标
 * return：无
 * */
void socket_read_file_data(int sock_fd, short event, void *arg);

/**
 * 把read读取的buf存到定义异或的buf中，文件组结构体中记录异或buf存满的个数当达到文件的数目之后进行异或
 * param：
 * buffer：read读取的buf
 * length：读取的buf的真是数据的长度
 * return: 0
 * */
static int write_buf(void *buffer, int length, int s_sub);

/**
 * 把文件信息打包起来
 * param:
 * -socket_fd：客户端与服务连接的套接字
 * -s_sub：当前的文件在文件组中的下标
 * return：
 * ret，write失败;
 * -error_code，返回的信息错误;
 * -return_obj.reply_value, 返回的值错误;
 * 0，正确
 * */
static int socket_write_header(const int socket_fd, const int s_sub);

/**
 * 发送给文件服务器信息
 * param：
 * -socket_fd：客户端与服务连接的套接字
 * -buf：发送给文件服务器的包头
 * -length：发送的长度
 * -read_buff：发送成功接收反馈的消息
 * -read_size：返回数据结构体的大小
 * return：CONNECTSERVERFAILED，发送失败;0，正确
 * */
static int communicate_with_server(const int socket_fd, void *buf, long int length, void *read_buff, size_t *read_size);

/**
 * 当每次接收完dataserver发送过来缓存到接受缓冲区的数据之后向发送端反馈回去接受完的消息
 * param：
 * -fd：客户端与服务端连接的套接字
 * -buffer：反馈回去的数据
 * -length：数据长度
 * return：WRITE_EXEC_ERROR,反馈的消息失败;0：正确
 * */
static int write_buf_data_server(int fd, void *buffer, int length);

/**
 * 客户端连接服务器
 * -socket：创建的连接的套接字
 * -client_addr：对sockaddr_in进行赋值然后再转化成
 * */
static int connect_to_server(int &socket_fd, struct sockaddr_in client_addr);


#endif // !GET_DATA_CLIENT_FILE_DATA_H_