/*
 * @Descripttion: 
 * @version: 
 * @Author: sql
 * @Date: 2020-07-28 15:08:59
 * @LastEditors: sql
 * @LastEditTime: 2021-05-12 10:45:25
 */ 
#include "oraid_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <iostream>
#include <string>
#include <signal.h>
#include <execinfo.h>
#include <sys/wait.h>
#include <vector>

#include "event2/event.h"
#include "event2/event_struct.h"
#include "event2/bufferevent.h"
#include "event2/buffer.h"
#include "event2/bufferevent_struct.h"
#include "raid_all_file.h"
#include "common.h"
#include "json.h"
#include "sock.h"
#include "send_message.h"
#include "glog/logging.h" // google日志
#include "send_feedback_info.h"
#include "get_data_client_file_data.h"
#include "error_info.h"

using namespace std;

#define BUF_MAX 1024 * 16

#define DEFAULT_PORT 3260 //端口号

//static struct event_base *evbase;

extern struct tDataServerInfo g_tDataDerverInfo;

static int setnonblocking(int sockfd)
{
    if (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0) | O_NONBLOCK) == -1)
    {
        return -1;
    }
    return 0;
}

static void buffered_on_read(int sock, short event, void *arg)
{
	struct sockaddr_in local_addr;		//获取本地的ip地址
	bzero(&local_addr, sizeof(local_addr));
	socklen_t len = sizeof(local_addr);
	getsockname(sock, (struct sockaddr *)&local_addr, &len);
	LOG(INFO) << "本地的ip地址为 : " << inet_ntoa(local_addr.sin_addr);

	
    if (NULL == arg)
    {
        return;
    }
    //vector<string> local_dir_file_path;     //存放本地文件文件的容器
	vector<string> a_set_of_files;	//一组
    struct client *client = (struct client *)arg;
    unsigned char recvbuf[BUF_MAX] = {0};   //接收buf
    unsigned char *sendBuf;                 //发送buf
    int ret = 0;

    //读客户端发送过来的数据
    ret = read(client->fd, recvbuf, BUF_MAX);
    LOG(INFO) << "read buffer bytes ret is : " << ret << ", write status is : " << client->write_state << ", file_name"
         << ", file offset is : " << client->write_offset << ", file total size is : " << client->write_total_size
         << ", file index is : " << client->write_index;
    if (ret <= 0 &&client->write_offset == 0)
    {
        event_del(client->ev_accept);
        event_base_loopexit(client->base, NULL);
        if (NULL != client->ev_accept)
        {
            free(client->ev_accept);
        }
        free(client);
        close(sock);
        return;
    }

    //printf("recvbuf is:%s\n", recvbuf);

    if (ret > 10 && (strncmp((char *)recvbuf, "GET", 3) == 0 ||
                     strncmp((char *)recvbuf, "HEAD", 4) == 0 ||
                     strncmp((char *)recvbuf, "POST", 4) == 0 ||
                     strncmp((char *)recvbuf, "PUT", 3) == 0 ||
                     strncmp((char *)recvbuf, "DELETE", 6) == 0 ||
                     strncmp((char *)recvbuf, "CONNECT", 7) == 0 ||
                     strncmp((char *)recvbuf, "OPTIONS", 7) == 0 ||
                     strncmp((char *)recvbuf, "TRACE", 5) == 0))
    {
        //http 协议
        printf("rest connect\n");
        char receive_http_content[1024] = {0}; //输入的内容
        char receive_http_method[256] = {0};   //方法名
        char receive_http_header[256] = {0};
        char receive_http_content_type[256] = {0}; //输入的http content type
        char output_file_name[1024] = {0};		//输出文件名字
        char error_file_path[200] = {0}; 		//错误的文件名

        //解析收到的http信息
        get_input_http_info(recvbuf, receive_http_content, receive_http_method, receive_http_header, receive_http_content_type);
        //方法
        cout << "reveive http content is = " << receive_http_content << endl;

		//解析发送过来关于文件的json信息
        ret = get_json_need_xor_file_info(receive_http_content, &a_set_of_files, output_file_name);
		
        if(strcmp(inet_ntoa(local_addr.sin_addr), g_tDataDerverInfo.data_server_ip) != 0)//异地文件
        {
            cout << "nonlocal file" << endl;
            LOG(INFO) << "nonlocal file";
            
            //LOG(INFO) << "nonlocal ip is : " << nonlocal_server_ip << ", nonlocal port us : " << nonlocal_server_port << ", nonlocal method is : " << nonlocal_server_method;
            for(auto &i : a_set_of_files)
			{
				LOG(INFO) << "异地文件路径 : " << i;
				cout << "异地文件路径 : " << i << endl;
			}
			if(ret == 0)
			{
				//把文件名字发送给存放文件的服务器
				char get_file_attribute_content[BUF_MAX] = {0}; //返回的存放json文件属性信息
				//发送文件名获取到文件属性信息(大小)
				ret = send_file_name_main(a_set_of_files, get_file_attribute_content);
				if (ret == 0)
				{
					//解析数据服务器反馈来的文件信息，文件大小
					off_t file_size = 0;
					
					ret = get_json_nonlocal_file_attribute_info(get_file_attribute_content, file_size);
					if (ret == 0)
					{
						//创建去连接服务器的客户端
						
						ret = create_client_connect_data_server(file_size, a_set_of_files, output_file_name);
					}
				}
			}

            //close(oraid_socket_fd);
            char send_http_content[128] = {0};//构造返回给客户端的content
            set_send_json_info(send_http_content, ret);

            //构造http发送消息
            char send_http_info[BUF_MAX] = {0}; //发送给客户端http info
            set_output_http_info(send_http_info, send_http_content, receive_http_header, receive_http_content_type);

            cout << "send http content is : " << send_http_content << endl;

            if(write_buf(client->fd, send_http_info, strlen(send_http_info) + 1) < 0)
            {
                LOG(ERROR) << "write error";
            }
			return;
        }
        else	//本地文件
        {
            //解析生成的json
			cout << "local file" << endl;
            LOG(INFO) << "local file";  
            //ret = get_json_read_file_info(receive_http_content, output_file_name, &local_dir_file_path);
            if (ret == 0)
            {
				for (auto &i : a_set_of_files)
				{
					LOG(INFO) << "本地文件路径 : " << i;
					cout << "本地文件路径 : " << i << endl;
				}
				ret = open_file(output_file_name, a_set_of_files);
            }

            printf("ret = %d\n", ret);
            //构造返回给客户端的content
            char send_http_content[128] = {0};
            set_send_json_info(send_http_content, ret);

            //构造http发送消息
            char send_http_info[BUF_MAX] = {0}; //发送给客户端http info
            set_output_http_info(send_http_info, send_http_content, receive_http_header, receive_http_content_type);

            //发送 http info
            printf("message is:%s\n", send_http_info);

            if(write_buf(client->fd, send_http_info, strlen(send_http_info) + 1) < 0)
            {
                LOG(ERROR) << "write error";
            }
        }
		#if 0
        else if (strcmp(receive_http_method, "select_raid_kind") ==0)
        {
            //查询正在使用的raid种类
        }
        else
        {   //没找到方法的
            cout << "没有找到方法的" << endl;
            ret = ERROR_METHOD_NOT_FOUND;
            //构造返回给客户端的content
            char send_http_content[128] = {0};
            set_send_json_info(send_http_content, ret);

            //构造http发送消息
            char send_http_info[BUF_MAX] = {0}; //发送给客户端http info
            set_output_http_info(send_http_info, send_http_content, receive_http_header, receive_http_content_type);

            //发送 http info
            printf("message is:%s\n", send_http_info);

            if(write_buf(client->fd, send_http_info, strlen(send_http_info) + 1) < 0)
            {
                LOG(ERROR) << "write error";
            }
        }
		#endif
    }
    else
    {
        //socket接受数据
        printf("socket_connece\n");
        //创建接受文件数据的函数接口
#if 0
        vector<string> dir_file_path;
        char output_file_name[1024] = {0};
        ret = get_sock_read_file_info((char *)recvbuf, &dir_file_path);
        if (ret < 0)
        {
            LOG(ERROR) << "get_sock_read_file_info is error";
            return;
        }
        else
        {
            strcpy(output_file_name, dir_file_path[dir_file_path.size() - 1].c_str()); //提取最后生成的文件名称
            dir_file_path.pop_back();
            LOG(INFO) << "dir_file_path.size() = " << dir_file_path.size();

            for (int i = 0; i < dir_file_path.size(); i++)
                cout << dir_file_path[i] << endl;
            //printf("--output_file_name:%s--\n", output_file_name);
            int ret = open_file(output_file_name, dir_file_path);
            if (ret < 0)
            {
                LOG(ERROR) << "open_file error";
            }
        }
        sendBuf = (unsigned char *)malloc(100);
        strcpy((char *)sendBuf, "dsad");
        printf("sendBuf = %s", sendBuf);
        write_buf(client->fd, sendBuf, sizeof(sendBuf) + 1);
        free(sendBuf);
#endif
    }
    return;
}

//线程处理新接收到的socket连接
static void *process_in_new_thread_when_accepted(void *arg)
{
	struct client *client;
	long long_fd = (long)arg;
	int client_fd = (int)long_fd;
	if(client_fd < 0)
	{
		LOG(ERROR) << "process_in_new_thread_when_accepted error";
		return (void *)0;
	}
	//设置非阻塞状态
	if(setnonblocking(client_fd) < 0)
	{
		LOG(ERROR) << "set no block error";
		return (void *)0;
	}
	//申请client内存空间
	if((client = (struct client *)calloc(1, sizeof(*client))) == NULL)
	{
		LOG(ERROR) << "malloc failure " << "errno : " << errno << "strerror(errno) : " << strerror(errno); 
		return (void *)0;
	}

	client->fd = client_fd;
	//事件
	struct event *ev_accept = (struct event*)malloc(sizeof(struct event));
	client->ev_accept = ev_accept;
	//初始化libevent并保存返回的指针
	struct event_base *base = event_base_new();
	client->base = base;
	//把事件与event_base绑定
	event_assign(ev_accept, base, client_fd, EV_READ | EV_PERSIST, buffered_on_read, client);
	//添加事件
	event_add(ev_accept, NULL);
	//时间进入循环，等待就绪事件并执行
	event_base_dispatch(base);
	//释放指针指向得到内存
	event_base_free(base);
	return (void *)0;
}

static void on_accept(int fd, short ev, void *arg)
{
	int client_fd;
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);

	//与客户端进行连接
	client_fd = accept(fd, (struct sockaddr *)&client_addr, &client_len);

	if (client_fd < 0)
	{
		LOG(ERROR) << "accept error :" << errno;
		return;
	}

	// 接收缓冲区
	int nRecvBuf = 200 * 1024 * 1024;
	setsockopt(client_fd, SOL_SOCKET, SO_RCVBUF, (const char *)&nRecvBuf, sizeof(int));
	
	//发送缓冲区
	int nSendBuf = 200 * 1024 * 1024;
	setsockopt(client_fd, SOL_SOCKET, SO_SNDBUF, (const char *)&nSendBuf, sizeof(int));
	//打印客户端的ip地址
	char client_ip[64] = {0};
	sprintf(client_ip, "%s", inet_ntoa(client_addr.sin_addr));
	LOG(INFO) << "client ip is:" << client_ip;

	//创建线程处理新接收到的socket连接
	pthread_t thread;
	pthread_create(&thread, NULL, process_in_new_thread_when_accepted, (void *)client_fd);
	pthread_detach(thread);

	return;
}

//
#define SIZE 100
static void DebugBacktrace(int sig)
{
	void *array[SIZE];
	int size, i;
	char **strings;
	time_t timep;
	time(&timep);

	LOG(INFO) << "BackTrace";
	LOG(INFO) << "Data : " << ctime(&timep);

	size = backtrace(array, SIZE);
	char buf[10] = {0};
	string str = "backtrack";
	sprintf(buf, "%d", size);
	str += buf;
	str += "deep";
	LOG(INFO) << str;
	LOG(INFO) << "backtrack" << size << "deep" << '\n';

	strings = backtrace_symbols(array, size);
	for (int i = 0; i < size; i++)
	{
		LOG(INFO) << strings[i];
	}
	free(strings);
	exit(-1);
}

static void sig_chld(int signo)
{
	LOG(INFO) << "SIGGHID start";
	pid_t pid;
	int stat;
	pid = wait(&stat);
	LOG(INFO) << "SIGGHID end";
	return;
}

static void log_info_init(const char *exec_path, const char *log_path)
{
	
	FLAGS_log_dir = log_path;
    	//初始化glog
	google::InitGoogleLogging(exec_path);


	//设置输出日志文件名前缀
	char log_file[264] = {0};
	sprintf(log_file, "%s", "INFO_MESSAGE_");	
	google::SetLogDestination(google::INFO, log_file);

	memset(log_file, 0, 264);
	sprintf(log_file, "%s", "WARNING_MESSAGE_");
	google::SetLogDestination(google::WARNING, log_file);

	memset(log_file, 0, 264);
	sprintf(log_file, "%s", "ERROR_MESSAGE_");
	google::SetLogDestination(google::ERROR, log_file);

	//设置输出到stderr信息，将大于等于该级别的日志同时输出到stderr。
	//日志级别 INFO, WARNING, ERROR, FATAL 的值分别为0、1、2、3。
	FLAGS_stderrthreshold = 3;

	//每行log加前缀
   	FLAGS_log_prefix = true;

	//设置logbuflevel，哪些级别立即写入文件还是先缓存,默认INFO级别以上都是直接落盘，会有io影响
	//日志级别 INFO, WARNING, ERROR, FATAL 的值分别为0、1、2、3。
	FLAGS_logbuflevel = 2;

	//设置logbufsecs，最多延迟（buffer）多少秒写入文件，另外还有默认超过10^6 chars就写入文件。
	FLAGS_logbufsecs = 0;

	//设置日志文件大小，单位M，默认1800MB
	FLAGS_max_log_size = 50;
	
}
/* void process_group_file()
{
	pthread_t thread;
	pthread_create(&thread, NULL, process_group_file_new_thread, NULL);
	pthread_detach(thread);
} */


int main(int argc, char **argv)
{

	//创建配置上下文
	event_config *conf = event_config_new();

	//显示支持的网络模式
	const char **methods = event_get_supported_methods();
	cout << "supported methods: " << endl;
	for(int i = 0; methods[i] != NULL; i++)
	{
		cout << methods[i] << endl;
	}
	
	
    //日志文件
/*     char log_path[1024] = {0};
    sprintf(log_path, "./raid.log");
    plog::init(plog::debug, log_path); */
 
    /* google日志文件 初始化glog */
    log_info_init(argv[0], "123");

 
    //设置socket服务器
	int socket_fd;					//连接客户端的套接字
	struct sockaddr_in server_addr; //服务端的addr

	LOG(INFO) << "socket data server start";
	
	//创建线程扫描文件组中一组文件是否已经传输完传输完移除掉
	//process_group_file();

	//linux 信号处理机制	
	/* 设置SIGSEGV信号的处理函数 */
	signal(SIGSEGV, DebugBacktrace); /* 设置SIGSEGV信号的处理函数 */
	signal(SIGFPE, DebugBacktrace);	 /* 设置SIGFPE信号的处理函数 */
	signal(SIGABRT, DebugBacktrace); /* 设置SIGABRT信号的处理函数 会导致进程流产的信号 */
	signal(SIGILL, DebugBacktrace);	 /* 设置SIGILL信号的处理函数  不能恢复至默认动作的信号*/
	signal(SIGINT, DebugBacktrace);	 /* 设置SIGINT信号的处理函数  会导致进程退出的信号， 通常是由ctr+c造成的*/
	signal(SIGCHLD, &sig_chld);		 /* 子进程中止时会向父进程发送SIGCHLD，告诉父进程回收自己*/
	signal(SIGPIPE, SIG_IGN);		 /* 当服务端关闭，客户端试图想向套接字写入数据的时候会产生SIGPIPE
										SIG_IGN:忽略这个信号;SIG_DFL:使用默认的信号处理，缠上呢个core文件 */
	//创建socket_fd
	if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		LOG(ERROR) << "creat socket error, error is : " << strerror(errno);
		return -CLIENTSOCKETFAILED;
	}
	//配置服务器
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(DEFAULT_PORT);

	//tcp连接断开的时候调用close()函数。linger设置断开方式
	int opt = 1;
	struct linger so_linger;
	so_linger.l_onoff = true;
	so_linger.l_linger = 0;
	//SO_LINGER所有套接字里排队的消息成功发送或到达延迟时间后>才会返回
	setsockopt(socket_fd, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(opt));

	//SO_REUSEADDR打开或关闭地址复用功能。
	unsigned int value = 1;
	setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, (void *)&value, sizeof(value));
	
	//SO_RCVBUF接受缓冲区
	int nRecvBuf = 200 * 1024 * 1024;
	setsockopt(socket_fd, SOL_SOCKET, SO_RCVBUF, (const char *)&nRecvBuf, sizeof(int));
	
	//SO_SNDBUF发送缓冲区
	int nSendBuf = 200 * 1024 * 1024;
	setsockopt(socket_fd, SOL_SOCKET, SO_SNDBUF, (const char *)&nSendBuf, sizeof(int));
 
	//把sock_addr和socket绑定到一起
	if (bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
	{
		LOG(ERROR) << "bind socket error is : " << strerror(errno);
		return -CONNECTSERVERFAILED;
	}

	//设置文件的阻塞
/* 	if(setnonblocking(socket_fd) < 0)
	{
		LOG(ERROR) << "set non blocj error";
		return -CONNECTSERVERFAILED;
	} */
	
	//cout << "listen" << endl;
	//设置监听客户端的队列
	if (listen(socket_fd, 1024) == -1)
	{
		printf("listen socket error: %s(errno: %d)\n", strerror(errno), errno);
		LOG(ERROR) << "listen error, error code is:" << strerror(errno);
		return -CONNECTSERVERFAILED;
	}
	cout << "libevent accept" << endl;
	//事件
	struct event ev_accept;
	//初始化libevent并保存返回的指针
	static struct event_base *evbase = event_base_new();

	//获取当前的网络模型
	cout << "current methods : " << event_base_get_method(evbase) << endl;
	int f = event_base_get_features(evbase);
	if(f&EV_FEATURE_ET)
		cout << "EV_FEATURE_ET event are supported" << endl;
	else
		cout << "EV_FEATURE_ET event are not supported" << endl;

	//把事件与event_base绑定
	event_assign(&ev_accept, evbase, socket_fd, EV_READ | EV_PERSIST, on_accept, NULL);
	//添加事件
	event_add(&ev_accept, NULL);
	//时间进入循环，等待就绪事件并执行
	event_base_dispatch(evbase);
	//删除事件
	event_del(&ev_accept);
	//释放指针内存
	event_base_free(evbase);
	//关闭套接字
	close(socket_fd);
    //
    google::ShutdownGoogleLogging();
	return RIGHT;
}
