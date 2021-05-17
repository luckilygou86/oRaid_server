/*
 * @Descripttion: 主函数的头文件，进行连接客户端分析所发送过来的数据信息
 * @version: 
 * @Author: sql
 * @Date: 2020-09-07 09:26:45
 * @LastEditors: sql
 * @LastEditTime: 2020-09-08 14:53:57
 */
#ifndef ORAID_SERVER_H_
#define ORAID_SERVER_H_
#include <cstring>
#include <cstdio>
#include <vector>
#include <iostream>

struct client
{
    int     fd; // The clients socket.
    struct  event_base *base;
    struct  event *ev_accept;
    int     write_state;        //写的状态
    size_t  write_index;        //实际写入的数据长度
    size_t  write_total_size;   //需要写入数据的总长度
    char    file_name[1024];    //文件名
    int     file_fd;            //文件套接字
    off_t   write_offset;       //需要从文件写入的offset
};

/**
 * 设置google日志文件
 * -exec_path   初始化
 * -log_path    目录的路径
 * 设置输出的文件名前缀
 * 返回值：无
 * */
static void log_info_init(const char *exec_path, const char *log_path);

/**
 * SIGCHLD信号处理函数
 * -signo   信号
 * 子进程中止时会向父进程发送SIGCHLD，告诉父进程回收自己，但是信号默认处理动作忽略，因此父进程仍然不会回收子进程，
 * 需要捕捉处理实现子进程的回收
 * 返回值：无
 * */
static void sig_chld(int signo);

/**
 * SIGSEGV、SIGFPE、SIGABRT、SIGILL、SIGILL、SIGINT信号的处理函数
 * -sig     信号
 * SIGSEG   指针所对应的地址是无效地址，没有物理内存对应该地址
 * SIGFPE   除零信号的捕捉
 * SIGABRT  多次free、执行abort函数、执行到assert函数
 * SIGILL   无效的程序映像，如无效指令
 * SIGINT   程序终止(interrupt)信号, 在用户键入INTR字符(通常是Ctrl-C)时发出
 * 返回值：无
 * */
static void DebugBacktrace(int sig);

/**
 * 与客户端进行连间的函数
 * -fd   监听socket,是否有新的客户端过来连接
 * -ev   事件触发状态EV_READ | EV_PERSIST调用函数
 * -arg  NULL
 * 当与客户端进行accept连接时，会生成一个新的套接字，代表的时与客户端的连接，此时可以对这个连接的socket_fd用函数setsockopt   设置发送/接受缓冲区，设置阻塞等操作
 * 创建线程 连接创建成功之后单独对这个socket_fd进行操作
 * 返回值：无
 * */
static void on_accept(int fd, short ev, void *arg);

/**
 * 线程函数单独处理连接的客户端服务端之间的通信
 * -arg 连接的套接字socket_fd
 * 创建事件用来检测触发事件的状态EV_READ | EV_PERSIST
 * EV_READ:当socket进行读取的时候
 * EV_PERSIST：事件的持续化，这个事件会保存挂起的状态,非挂起的时，event_del,event_add可以设置超时调用
 * 返回值：无
 * */
static void *process_in_new_thread_when_accepted(void *arg);

/**
 * 触发事件状态执行的函数
 * -sock：  连接的套接字
 * -event： 事件触发状态EV_READ | EV_PERSIST调用函数
 * -arg:    结构体：保存一些连接信息，event的信息，fd...
 * 函数体里对方以rest服务发送一些指令，当本地文件时、异地文件时，进行解析
 * 本地文件和异地文件以判断ip地址的方式进行区别，其中也包括反馈给对应的客户端函数，在send_feedback_info.cpp里
 * 返回值：无
 * */
static void buffered_on_read(int sock, short event, void *arg);


#endif // !ORAID_SERVER_H_