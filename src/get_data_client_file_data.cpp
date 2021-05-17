/*
 * @Descripttion: 
 * @version: 
 * @Author: sql
 * @Date: 2020-08-05 11:30:36
 * @LastEditors: sql
 * @LastEditTime: 2020-09-28 15:11:11
 */

#include "get_data_client_file_data.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/resource.h>
#include <malloc.h>
#include <libgen.h>
#include <signal.h>
#include <execinfo.h>
#include <sys/wait.h>

#include "error_info.h"
#include "file_info.h"
#include "glog/logging.h"
#include "send_feedback_info.h"
#include "event2/event.h"
#include "event2/event_struct.h"
#include "event2/bufferevent.h"
#include "event2/buffer.h"
#include "event2/bufferevent_struct.h"
#include "send_message.h"
#include "common.h"


pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;//初始化锁

extern struct tDataServerInfo g_tDataDerverInfo;

struct tGroupFileInfo g_tgroupFile;


static int setnonblocking(int sockfd)
{
	if (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0) | O_NONBLOCK) == -1)
	{
		return -1;
	}
	return 0;
}

//客户端连接服务器
static int connect_to_server(int &socket_fd, struct sockaddr_in client_addr)
{
	//设置一个socket地址结构client_addr,代表客户机internet地址, 端口
	bzero(&client_addr, sizeof(client_addr));		 //把一段内存区的内容全部设置为0
	client_addr.sin_family = AF_INET;				 //internet协议族
	client_addr.sin_addr.s_addr = htons(INADDR_ANY); //INADDR_ANY表示自动获取本机地址
	client_addr.sin_port = htons(0);				 //0表示让系统自动分配一个空闲端口
	//创建用于internet的流协议(TCP)socket,用client_socket代表客户机socket
	//执行过程中资源的限制

	struct rlimit rt;
	rt.rlim_max = rt.rlim_cur = MAX_CONNECT_SOCKET_NUM;
	if (setrlimit(RLIMIT_NOFILE, &rt) == -1)
	{
		LOG(ERROR) << "set socket max num error";
		return -CONNECTSERVERFAILED;
	}
	//创建一个流式套接字
	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd < 0)
	{
		LOG(ERROR) << "Create Socket Failed!" << strerror(errno);
		return -CONNECTSERVERFAILED;
	}
	LOG(ERROR) << "socket_fd=" << socket_fd;

	//把客户机的socket和客户机的socket地址结构联系起来
	if (bind(socket_fd, (struct sockaddr *)&client_addr, sizeof(client_addr)))
	{
		LOG(ERROR) << "Client Bind Port Failed!";
		close(socket_fd);
		return -CONNECTSERVERFAILED;
	}

	//接收缓冲区
	int nRecvBuf = 200 * 1024 * 1024;
	setsockopt(socket_fd, SOL_SOCKET, SO_RCVBUF, (const char *)&nRecvBuf, sizeof(int));
	//接受缓冲区
	int nSendBuf = 200 * 1024 * 1024;
	setsockopt(socket_fd, SOL_SOCKET, SO_SNDBUF, (const char *)&nSendBuf, sizeof(int));

	//设置一个socket地址结构server_addr,代表服务器的internet地址, 端口
	struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	if (inet_aton(g_tDataDerverInfo.data_server_ip, &server_addr.sin_addr) == 0) //服务器的IP地址来自程序的参数
	{
		LOG(ERROR) << "Server IP Address Error!";
		close(socket_fd);
		return -CONNECTSERVERFAILED;
	}
	server_addr.sin_port = htons(atoi(g_tDataDerverInfo.data_server_port)); //主机字节序转化为网络字节序
	socklen_t server_addr_length = sizeof(server_addr);
	//向服务器发起连接,连接成功后client_socket代表了客户机和服务器的一个socket连接
	// 非阻塞的方式打开
	fcntl(socket_fd, F_SETFL, fcntl(socket_fd, F_GETFL, 0) | O_NONBLOCK);

	int connection_fd = connect(socket_fd, (struct sockaddr *)&server_addr, server_addr_length);

#if 1
	int ret = -1;
	if (connection_fd != 0)
	{
		if (errno != EINPROGRESS)
		{
			LOG(ERROR) << "connect error :" << strerror(errno);
			close(socket_fd);
			return -CONNECTSERVERFAILED;
		}
		else
		{
			struct timeval tm = {5, 0};
			fd_set wset, rset;

			FD_ZERO(&wset); //清理文件描述符集合
			FD_ZERO(&rset);
			FD_SET(socket_fd, &wset); //向某个文件描述符集合中加入文件描述符
			FD_SET(socket_fd, &rset);
			long t1 = time(NULL); //获取当前时间
			int res = select(socket_fd + 1, &rset, &wset, NULL, &tm);
			long t2 = time(NULL);
			if (res < 0)
			{
				LOG(ERROR) << "network error in connect";
				close(socket_fd);
				return -CONNECTSERVERFAILED;
			}
			else if (res == 0)
			{
				LOG(ERROR) << "connect time out";
				close(socket_fd);
				return -CONNECTSERVERFAILED;
			}
			else if (1 == res)
			{
				if (FD_ISSET(socket_fd, &wset))
				{
					fcntl(socket_fd, F_SETFL, fcntl(socket_fd, F_GETFL, 0) & ~O_NONBLOCK);
					ret = 0;
				}
				else
				{
					LOG(ERROR) << "other error when select:" << strerror(errno);
					close(socket_fd);
					return -CONNECTSERVERFAILED;
				}
			}
		}
	}
#endif

	return 0;
}

//反馈给客户端消息
static int write_buf_data_server(int fd, void *buffer, int length)
{
	LOG(INFO) << "write data_client start";
	int bytes_left;				//写的字节数
	int written_bytes;			//write返回的字节数
	char *ptr = (char *)buffer; //写的buf
	bytes_left = length;
	while (bytes_left > 0)
	{
		written_bytes = write(fd, ptr, bytes_left);
		if (written_bytes <= 0)
		{
			if (errno == EINTR) //函数调用被中断
				written_bytes = 0;
			else if (errno == EAGAIN) //资源0是不可用，稍作再次调用或成功
				break;
			else
			{
				LOG(ERROR) << "write exec happen error";
				return -WRITE_EXEC_ERROR;
			}
		}
		bytes_left -= written_bytes;
		ptr += written_bytes;
	}
	LOG(INFO) << "write data_client end";
	return 0;
}
//发送给文件服务器信息
static int communicate_with_server(const int socket_fd, void *buf, long int length, void *read_buff, size_t *read_size)
{
	int send_real_len = send(socket_fd, buf, length, 0); //发送
	if (send_real_len < 0)
	{
		LOG(ERROR) << "write to server error" << strerror(errno);
		return -CONNECTSERVERFAILED;
	}
	#if 0
	//需要接收用来判断是否发送成功
	int recv_len = 0;
	int i = 0;
	for (i = 0; i < 2; i++) //try 3 times
	{
		recv_len = recv(socket_fd, read_buff, *read_size, 0);
		if (recv_len < 0)
		{
			//      LOGD << "recv from MPS error" << strerror(errno);
			continue;
		}
		else
		{
			break;
		}
	}
	if (i == 2)
	{
		LOG(ERROR) << "recv from server error" << strerror(errno);
		return -CONNECTSERVERFAILED;
	}
	#endif

	return 0;
}
//打包文件信息
static int socket_write_header(const int socket_fd, const int s_sub)
{
	CMD_WRITE_T write_file_obj = {0};

	memset(write_file_obj.headerObj.synCode, 0xFF, sizeof(write_file_obj.headerObj.synCode));
	//文件块的长度
	//write_file_obj.data_len = single_file_info->file_block_size[single_file_info->now_thread];
	//文件的offset
	//write_file_obj.offset = single_file_info->write_offset[single_file_info->now_thread];
	//文件的路径
	strcpy(write_file_obj.file_name, g_tgroupFile.single_file_array[s_sub].file_path);
	//接收超时
	timeval tv = {10, 0};
	setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));

	//
	CMD_REPLY_WRITE_T return_obj = {0};
	size_t return_size = sizeof(CMD_REPLY_WRITE_T);
	int ret = communicate_with_server(socket_fd, &write_file_obj, CMD_LEN, &return_obj, &return_size);
	if (ret < 0)
	{
		return ret;
	}
	unsigned int error_code = return_obj.headerObj.ReturnLenHighByte * 256 + return_obj.headerObj.ReturnLenLowByte;
	if (error_code != 0)
	{
		LOG(ERROR) << "write : error_code = " << error_code << endl;
		return -error_code;
	}

	if (return_obj.reply_value != 0)
	{
		LOG(ERROR) << "header reply value is:" << hex << return_obj.reply_value;
		return -return_obj.reply_value;
	}

	return 0;
}

static int write_buf(void *buffer, int length, int s_sub)
{
	struct  in_file_info *in_buf_info = &g_tgroupFile.single_file_array[s_sub].in_buf_info;
	char *buf = (char *)buffer;
	int flag = 0;
	/* unsigned char *buf1 = new unsigned char[READ_BUF * sizeof(char) + 1];
	if (buf1 == NULL)
	{
		LOG(ERROR) << "buf apply malloc error";
		in_buf_info.correct_flag == -1;
	} */
	while (1)
	{
		//存到buf0
		if (in_buf_info->buf_flag[0] == 1)
		{
			//当buf存不下读的字节的时候能够存取的字节去存取
			
			if ((in_buf_info->buf_size[0] + length) > READ_BUF_SIZE)
			{
				
				//能存多少是多少
				in_buf_info->cp_size = READ_BUF_SIZE - in_buf_info->buf_size[0];
				//strncat((char *)in_buf_info->in_buf0, (char *)buf, in_buf_info->cp_size);
				memcpy(in_buf_info->in_buf0 + in_buf_info->buf_size[0], buf, in_buf_info->cp_size);

				in_buf_info->buf_size[0] += in_buf_info->cp_size; //buf1现在存的字节数
				
				in_buf_info->buf_flag[0] = 2;
				LOG(INFO) << "线程 ： " << s_sub << "buf_size[0] = " << in_buf_info->buf_size[0];
 
				/* int ret = select_single_file_where_group(g_tgroupFile.single_file_array[s_sub].file_path);
				if(ret < 0)
				{
					LOG(ERROR) << "ret : " << ret << "ERROR 文件 ：" << g_tgroupFile.single_file_array[s_sub].file_path << ", 在文件组容器里不存在";
				} */
				
				g_tgroupFile.buf_full_num[0]++;
				

				LOG(INFO) << "线程 : " << s_sub << ", in_buf0 full bun is : " << g_tgroupFile.buf_full_num[0];
				flag = 1; //标志没有存完
			}
			else //追加结束
			{

#if 1
				g_tgroupFile.single_file_array[s_sub].file_index += length;
#endif

				//追加到后面
				if (flag == 1)
				{

					//memcpy(buf1, buf + in_buf_info->cp_size, length - in_buf_info->cp_size);
					//strncat((char *)in_buf_info->in_buf0, buf + in_buf_info->cp_size, length - in_buf_info->cp_size);
					memcpy(in_buf_info->in_buf0 + in_buf_info->buf_size[0], buf + in_buf_info->cp_size, length - in_buf_info->cp_size);

					in_buf_info->buf_size[0] += length - in_buf_info->cp_size;

					LOG(INFO) << "线程 ： " << s_sub << "buf_size[0] = " << in_buf_info->buf_size[0];
					
					//memset(buf1, 0, sizeof(buf1));
					in_buf_info->cp_size = 0;
					flag = 0;
				}
				else
				{

					//strncat((char *)in_buf_info->in_buf0, (char *)buf, length);
					memcpy(in_buf_info->in_buf0 + in_buf_info->buf_size[0], buf, length);
					
					in_buf_info->buf_size[0] += length;
					LOG(INFO) << "线程 ： " << s_sub << "buf_size[0] = " << in_buf_info->buf_size[0];

				}
				if (g_tgroupFile.single_file_array[s_sub].file_index >= g_tgroupFile.file_size)
				{
					in_buf_info->buf_flag[0] = 2;

					/* int ret = select_single_file_where_group(g_tgroupFile.single_file_array[s_sub].file_path);
					if(ret < 0)
					{
						LOG(ERROR) << "ret : " << ret << "ERROR 文件 ：" << g_tgroupFile.single_file_array[s_sub].file_path << ", 在文件组容器里不存在";
					} */
					
					g_tgroupFile.buf_full_num[0]++;

					LOG(INFO) << "线程 : " << s_sub << ", in_buf0 full bun is : " << g_tgroupFile.buf_full_num[0];
				}
				//
				break;
			}
		}
		//存到buf1
		else if (in_buf_info->buf_flag[1] == 1)
		{

			//当buf存不下读的字节的时候街区能够存取的字节去存取
			if ((in_buf_info->buf_size[1] + length) > READ_BUF_SIZE)
			{
				//能存多少是多少
				in_buf_info->cp_size = READ_BUF_SIZE - in_buf_info->buf_size[1];

				//strncat((char *)in_buf_info->in_buf1, (char *)buf, in_buf_info->cp_size);
				memcpy(in_buf_info->in_buf1 + in_buf_info->buf_size[1], buf, in_buf_info->cp_size);

				in_buf_info->buf_size[1] += in_buf_info->cp_size;

				LOG(INFO) << "线程 ： " << s_sub << "buf_size[1] = " << in_buf_info->buf_size[1];

				in_buf_info->buf_flag[1] = 2;
				/* int ret = select_single_file_where_group(g_tgroupFile.single_file_array[s_sub].file_path);
				if(ret < 0)
				{
					LOG(ERROR) << "ret : " << ret << "ERROR 文件 ：" << g_tgroupFile.single_file_array[s_sub].file_path << ", 在文件组容器里不存在";
				} */
				
				g_tgroupFile.buf_full_num[1]++;
			

				LOG(INFO) << "线程 : " << s_sub << "in_buf1 full num is : " << g_tgroupFile.buf_full_num[1];

				flag = 1; //标志没有存完
			}
			else
			{
#if 1
				g_tgroupFile.single_file_array[s_sub].file_index += length;
#endif

				//追加到后面
				if (flag == 1)
				{

					//memcpy(buf1, buf + in_buf_info->cp_size[n], length - in_buf_info->no_cp_size[n]);
					//strncat((char *)in_buf_info->in_buf1, buf + in_buf_info->cp_size, length - in_buf_info->cp_size);
					memcpy(in_buf_info->in_buf1 + in_buf_info->buf_size[1], buf + in_buf_info->cp_size, length - in_buf_info->cp_size);

					in_buf_info->buf_size[1] += length - in_buf_info->cp_size;

					LOG(INFO) << "线程 ： " << s_sub << "buf_size[1] = " << in_buf_info->buf_size[1];

					//memset(buf1, 0, sizeof(buf1));
					in_buf_info->cp_size = 0;
					flag = 0;
				}
				else
				{

					//strncat((char *)in_buf_info->in_buf1, (char *)buf, length);
					memcpy(in_buf_info->in_buf1 + in_buf_info->buf_size[1], buf, length);
					in_buf_info->buf_size[1] += length;
					LOG(INFO) << "线程 ： " << s_sub << "buf_size[1] = " << in_buf_info->buf_size[1];

				}
				if (g_tgroupFile.single_file_array[s_sub].file_index >= g_tgroupFile.file_size)
				{

					in_buf_info->buf_flag[1] = 2;
					/* int ret = select_single_file_where_group(g_tgroupFile.single_file_array[s_sub].file_path);
					if(ret < 0)
					{
						LOG(ERROR) << "ret : " << ret << "ERROR 文件 ：" << g_tgroupFile.single_file_array[s_sub].file_path << ", 在文件组容器里不存在";
					} */
					g_tgroupFile.buf_full_num[1]++;
					
					LOG(INFO) << "线程 : " << s_sub << ", in_buf1 full num is : " << g_tgroupFile.buf_full_num[1];
				}

				break;
			}
		}
		else
		{
			usleep(500);
		}
		if (in_buf_info->buf_flag[0] == 0 && in_buf_info->buf_flag[1] != 1)
		{
			LOG(INFO) << "把buf0置为可写状态";
			//buf0置为可写的状态
			memset(in_buf_info->in_buf0, 0, READ_BUF_SIZE);
			in_buf_info->buf_flag[0] = 1;
			in_buf_info->buf_size[0] = 0;

			//LOG(INFO) << "文件" << n << ",buf0 flag is : " << in_buf_info->buf_flag[0][n] << ", buf1 flag is : " << in_buf_info->buf_flag[1][n];
		}
		if (in_buf_info->buf_flag[1] == 0 && in_buf_info->buf_flag[0] != 1)
		{
			//buf1置为可写的状态
			LOG(INFO) << "把buf1置为可写状态";
			memset(in_buf_info->in_buf1, 0, READ_BUF_SIZE);
			in_buf_info->buf_flag[1] = 1;
			in_buf_info->buf_size[1] = 0;

			//LOG(INFO) << "文件" << n << ",buf0 flag is : " << in_buf_info.buf_flag[0][n] << ", buf1 flag is : " << in_buf_info.buf_flag[1][n];
		}
	}
	return 0;
}

void socket_read_file_data(int sock_fd, short event, void *arg)
{	
	int s_sub = *(long *)arg;
	/* struct single_file_info *single_file_info = (struct single_file_info *)arg;
	int n = single_file_info->now_thread; */

	
	//buf进行接受read
	unsigned char buf[MAXLINE] = {0};

	if (g_tgroupFile.xor_flag == 1 || g_tgroupFile.correct_flag == -1)
	{
		//free(buf);
		free(g_tgroupFile.single_file_array	[s_sub].in_buf_info.in_buf0);
		free(g_tgroupFile.single_file_array[s_sub].in_buf_info.in_buf1);
		return;
	}

	memset(buf, 0, MAXLINE);
	//int ret = recv(single_file_info->fd, buf, READ_BUF + CMD_LEN, MSG_WAITALL);
	int ret = recv(g_tgroupFile.single_file_array[s_sub].fd, buf, MAXLINE, 0);
	//LOG(INFO) << "线程 : " << s_sub << ", ret = " << ret << ", write index is : " << g_tgroupFile.single_file_array[s_sub].write_index
	//		  << ", write total size is : " << g_tgroupFile.single_file_array[s_sub].write_total_size << ", write state is : " << g_tgroupFile.single_file_array[s_sub].write_state;

//文件传输完

	if (ret == 0 && g_tgroupFile.single_file_array[s_sub].write_state == 0)
	{

		event_del(g_tgroupFile.single_file_array[s_sub].ev_accept);
		event_base_loopexit(g_tgroupFile.single_file_array[s_sub].base, NULL);
		if (g_tgroupFile.single_file_array[s_sub].ev_accept != NULL)
			free(g_tgroupFile.single_file_array[s_sub].ev_accept);

		close(sock_fd);
		return;
	}
	//LOG(INFO) << "buf = " << buf;
	//发生错误，（断网、、、
	if(ret < 0)
	{
		event_del(g_tgroupFile.single_file_array[s_sub].ev_accept);
		event_base_loopexit(g_tgroupFile.single_file_array[s_sub].base, NULL);
		if (g_tgroupFile.single_file_array[s_sub].ev_accept != NULL)
			free(g_tgroupFile.single_file_array[s_sub].ev_accept);
		
		g_tgroupFile.correct_flag = -1;
		close(sock_fd);
		return;
	}
	//第一次写
	if (g_tgroupFile.single_file_array[s_sub].write_state == 0)
	{
		if (ret < 16)
		{
			LOG(INFO) << ("recv error");
		}
		int command = buf[16];
		long int cmd_ret = 0;
		LOG(INFO) << "command : " << command;
		switch (command)
		{
		case CMD_READ:
			break;
		case CMD_REMOVE:
			break;
		case CMD_WRITE:
			CMD_WRITE_T cmd_obj = {0};
			CMD_REPLY_WRITE_T reply_obj = {0};

			memcpy(&cmd_obj, buf, sizeof(cmd_obj));

			//判断写是否结束
			if ((ret - CMD_LEN) >= cmd_obj.data_len)
			{

#if 0
				in_buf_info.write_test_size[s_sub] += single_file_info->write_total_size;
				cout << "write_test_size   " << s_sub << " is : " << in_buf_info.write_test_size[s_sub] << endl;
#endif

				LOG(INFO) << "第一次写结束";
				LOG(INFO) << "ret - CMD_LEN is : " << ret - CMD_LEN << ", data_len is : " << cmd_obj.data_len;
				int cmd_ret = write_buf(buf + CMD_LEN, ret - CMD_LEN, s_sub);

				g_tgroupFile.single_file_array[s_sub].write_state = 0;

				reply_obj.reply_value = cmd_obj.data_len;
				reply_obj.command = CMD_WRITE;

				write_buf_data_server(g_tgroupFile.single_file_array[s_sub].fd, &reply_obj, sizeof(reply_obj));
			}
			else
			{
				LOG(INFO) << "第一次写未结束";
				LOG(INFO) << "ret - CMD_LEN is : " << ret - CMD_LEN << ", data_len is : " << cmd_obj.data_len;
				int cmd_ret = write_buf(buf + CMD_LEN, ret - CMD_LEN, s_sub);

				g_tgroupFile.single_file_array[s_sub].write_state = 1;
				g_tgroupFile.single_file_array[s_sub].write_index = ret - CMD_LEN;
				g_tgroupFile.single_file_array[s_sub].write_total_size = cmd_obj.data_len;
			}
			break;
		}
	}
	else
	{
		//LOG(INFO) << "第二次写";
		CMD_REPLY_WRITE_T reply_obj = {0};
		if (ret <= 0)
		{
			LOG(ERROR) << "recv file error : " << strerror(errno);
			reply_obj.headerObj.ReturnLenHighByte = (READ_EXEC_ERROR >> 8) & 0xFF;
			reply_obj.headerObj.ReturnLenLowByte = READ_EXEC_ERROR & 0xFF;
			g_tgroupFile.single_file_array[s_sub].write_state = 0;
			write_buf_data_server(g_tgroupFile.single_file_array[s_sub].fd, &reply_obj, sizeof(reply_obj));
		}
		else
		{
			int cmd_ret = write_buf(buf, ret, s_sub);
			//写结束
			if (g_tgroupFile.single_file_array[s_sub].write_total_size <= ret + g_tgroupFile.single_file_array[s_sub].write_index)
			{

#if 0
					in_buf_info.write_test_size[s_sub] += g_tgroupFile.single_file_array[s_sub].write_total_size;
					cout << "write_test_size   " << s_sub << " is : " << in_buf_info.write_test_size[s_sub] << endl;
#endif

				LOG(INFO) << "第二次写结束， write total size is : " << g_tgroupFile.single_file_array[s_sub].write_total_size << ", write index is : " << g_tgroupFile.single_file_array[s_sub].write_index + ret;
				reply_obj.reply_value = g_tgroupFile.single_file_array[s_sub].write_total_size;
				reply_obj.command = CMD_WRITE;

				g_tgroupFile.single_file_array[s_sub].write_index = 0;
				g_tgroupFile.single_file_array[s_sub].write_state = 0;
				g_tgroupFile.single_file_array[s_sub].write_total_size = 0;

				write_buf_data_server(g_tgroupFile.single_file_array[s_sub].fd, &reply_obj, sizeof(reply_obj));
			}
			else
			{
				g_tgroupFile.single_file_array[s_sub].write_index += ret;
			}
		}
	}
	return;
}

//每个文件线程连接服务器进行发送数据
static void *process_in_new_thread_file_num(void *arg)
{
	int s_sub = *(long *)arg;
	free(arg);
	int sock_fd; //连接客户端的套接字
	int len;
	int ret = 0;
	struct sockaddr_in client_addr;

	/* struct single_file_info single_file_info = {0};
	memcpy(&single_file_info, arg, sizeof(single_file_info));
 */
	//创建套接字
	//LOG(INFO) << "线程 : " << s_sub << ", 文件" << g_tgroupFile.single_file_array[s_sub].file_path;
	
	if ((sock_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) //创建 流式套接字
	{
		LOG(ERROR) << "creat socket error, error is : " << strerror(errno);
		g_tgroupFile.correct_flag = -1;
		return (void *)0;
	}
	//设置服务器地址
	LOG(INFO) << "连接data server";
	ret = connect_to_server(sock_fd, client_addr);
	if (ret != 0)
	{
		LOG(ERROR) << "连接数据服务器失败";
		g_tgroupFile.correct_flag = -1;
		return (void *)0;
	}
	LOG(INFO) << "发送文件头";
	ret = socket_write_header(sock_fd, s_sub);
	if (ret != 0)
	{
		close(sock_fd);
		LOG(ERROR) << "发送文件头失败";
		g_tgroupFile.correct_flag = -1;
		return (void *)0;
	}

#if 0
	if (setnonblocking(sock_fd) < 0)
	{
		printf("set non block error\n");
		return (void *)0;
	}
#endif
	//SO_RCVBUF接受缓冲区
	int nRecvBuf = 200 * 1024 * 1024;
	setsockopt(sock_fd, SOL_SOCKET, SO_RCVBUF, (const char *)&nRecvBuf, sizeof(int));

	//SO_SNDBUF发送缓冲区
	int nSendBuf = 200 * 1024 * 1024;
	setsockopt(sock_fd, SOL_SOCKET, SO_SNDBUF, (const char *)&nSendBuf, sizeof(int));

#if 0
	//设置超时
	struct timeval timeout {30, 0};
	//SO_SNDTIMEO
	setsockopt(sock_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

	//SO_RCVTIMEO
	setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	

	single_file_info.fd = sock_fd;

	ret = socket_read_file_data(sock_fd, &single_file_info);

#else
	//int n = single_file_info.now_thread;

	//申请内存空间
	g_tgroupFile.single_file_array[s_sub].in_buf_info.in_buf0 = (unsigned char *)malloc(READ_BUF_SIZE * sizeof(char));
	if (g_tgroupFile.single_file_array[s_sub].in_buf_info.in_buf0 == NULL || g_tgroupFile.correct_flag == -1)
	{
		/**
		 * 查找存在错误的一组数据，并把标志为置为错误的标志
		 * */
		LOG(ERROR) << "in_buf_info.in_buf1 : " << s_sub << "apply malloc error";
		g_tgroupFile.correct_flag == -1;
		return (void *)0;
	}
	g_tgroupFile.single_file_array[s_sub].in_buf_info.in_buf1 = (unsigned char *)malloc(READ_BUF_SIZE * sizeof(char));
	if (g_tgroupFile.single_file_array[s_sub].in_buf_info.in_buf1 == NULL || g_tgroupFile.correct_flag == -1)
	{
		/**
		 * 查找存在错误的一组数据，并把标志为置为错误的标志
		 * */
		LOG(ERROR) << "in_buf_info.in_buf1 : " << s_sub << "apply malloc error";
		g_tgroupFile.correct_flag == -1;
		return (void *)0;
	}
	
	LOG(INFO) << "初始化把buf0置为可写状态,buf1为空";
	g_tgroupFile.single_file_array[s_sub].in_buf_info.buf_flag[0] = 1;
	g_tgroupFile.single_file_array[s_sub].in_buf_info.buf_flag[1] = 0;

	struct timeval timeout {60, 0};

	g_tgroupFile.single_file_array[s_sub].fd = sock_fd;
	//事件
	struct event *read_ev = (struct event *)malloc(sizeof(struct event));
	g_tgroupFile.single_file_array[s_sub].ev_accept = read_ev;
	//初始化libevent并保存返回的指针
	struct event_base *evbase = event_base_new();
	g_tgroupFile.single_file_array[s_sub].base = evbase;
	//把事件与event_base绑定
	event_assign(read_ev, evbase, sock_fd, EV_READ | EV_PERSIST, socket_read_file_data, (void *)&s_sub);
	//添加事件
	event_add(read_ev, &timeout);
	//时间进入循环，等待就绪事件并执行
	event_base_dispatch(evbase);
	//释放指针内存
	event_base_free(evbase);
	//关闭套接字
	close(sock_fd);
#endif
	return (void *)0;
}

static int buf_xor(unsigned char *in_buf[], int file_num, FILE *out_fd, int k)
{

	int num0, num1;
	char c0, c1;
	for (int i = 0; i < file_num; i++)
	{
		if (i == 0)
		{
			num0 = g_tgroupFile.single_file_array[i].in_buf_info.buf_size[k];
		}
		else
		{
			num1 = g_tgroupFile.single_file_array[i].in_buf_info.buf_size[k];
			//当存在异或的文件不一样大小的时候
			if (num0 != num1)
			{
				LOG(ERROR) << "i = " << i << "num0 = " << num0 << ", num1 = " << num1;
				LOG(ERROR) << "num0 != num1";
				g_tgroupFile.correct_flag = -1;
				return -1;
			}
		}
	}
	LOG(INFO) << "num0 = " << num0;
	unsigned int *in_buf_full[100];
	for (int i = 0; i < file_num; i++)
	{
		in_buf_full[i] = (unsigned int *)in_buf[i];
	}
	//作异或
	for (int i = 0; i < num0 / 4; i++)
	{
		for (int j = 1; j < file_num; j++)
		{
			*in_buf_full[0] ^= *in_buf_full[j];
			in_buf_full[j]++;
		}
		in_buf_full[0]++;
	}
	int ret = fwrite(in_buf[0], sizeof(int), num0 / 4, out_fd);
	if (ret != num0 / 4)
	{
		LOG(ERROR) << "fwrite error"
				   << "error is : " << strerror(errno);
		g_tgroupFile.correct_flag = -1;
		return -1;
	}
	//异或剩余字节
	if (num0 % 4 != 0)
	{
		unsigned char *remain_buf_bytes[100];
		int remain_byte = num0 % 4;
		LOG(INFO) << "剩余字节数 ： " << remain_byte;
		for (int i = 0; i < file_num; i++)
		{
			remain_buf_bytes[i] = in_buf[i] + (num0 - remain_byte);
		}
		for (int i = 0; i < remain_byte; i++)
		{
			for (int j = 1; j < file_num; j++)
			{
				*remain_buf_bytes[0] ^= *remain_buf_bytes[j];
				remain_buf_bytes[j]++;
			}
			remain_buf_bytes[0]++;
		}
		fwrite(in_buf[0] + (num0 - remain_byte), sizeof(char), remain_byte, out_fd);
	}
	
	pthread_mutex_lock(&data_mutex);
	g_tgroupFile.xor_size += num0;
	pthread_mutex_unlock(&data_mutex);

	LOG(INFO) << "in_buf_info.write_size = " << g_tgroupFile.xor_size << ", file total size = " << g_tgroupFile.file_size;
	cout << "in_buf_info.write_size = " << g_tgroupFile.xor_size << endl;
	//异或结束
	if (g_tgroupFile.xor_size == g_tgroupFile.file_size)
	{
		LOG(INFO) << "文件异或完成";
		
		pthread_mutex_lock(&data_mutex);
		g_tgroupFile.xor_flag = 1; //异或结束
		pthread_mutex_unlock(&data_mutex);

		for (int i = 0; i < file_num; i++)
		{
			free(g_tgroupFile.single_file_array[i].in_buf_info.in_buf0);
			free(g_tgroupFile.single_file_array[i].in_buf_info.in_buf1);
		}
	}
	return 0;
}

//进行判断异或条件
static int judge_xor_condition(FILE *out_fd)
{
	unsigned char *in_buf[100];
	int file_num = g_tgroupFile.file_num;
	do
	{
		//LOG(INFO)<< "buf full num is = " << g_tgroupFile.buf_full_num[0] << ", file num = " << file_num;
		if (g_tgroupFile.buf_full_num[0] == file_num)
		{
			for (int i = 0; i < file_num; i++)
			{
				in_buf[i] = g_tgroupFile.single_file_array[i].in_buf_info.in_buf0;
			}
			LOG(INFO) << "buf0开始异或"
					  << ", errno : " << errno << ", error is : " << strerror(errno);
#if 1
			buf_xor(in_buf, file_num, out_fd, 0);
#endif
			

			g_tgroupFile.buf_full_num[0] = 0; //

			for (int i = 0; i < file_num; i++)
			{
				g_tgroupFile.single_file_array[i].in_buf_info.buf_flag[0] = 0;
			}
			LOG(INFO) << "buf0结束异或";
		}
		else if (g_tgroupFile.buf_full_num[1] == file_num)
		{
			for (int i = 0; i < file_num; i++)
			{
				in_buf[i] = g_tgroupFile.single_file_array[i].in_buf_info.in_buf1;
			}
			LOG(INFO) << "buf1开始异或";

#if 1
			buf_xor(in_buf, file_num, out_fd, 1);
#endif

			g_tgroupFile.buf_full_num[1] = 0;
			for (int i = 0; i < file_num; i++)
			{
				g_tgroupFile.single_file_array[i].in_buf_info.buf_flag[1] = 0;
			}
			LOG(INFO) << "buf1结束异或";
		}
		else
		{
			usleep(500);
		}
	} while (g_tgroupFile.xor_flag == 0 && g_tgroupFile.correct_flag == 0);
	if(g_tgroupFile.correct_flag != 0)
	{
		cout << "ERROR group file correct flag is :" << g_tgroupFile.correct_flag << endl;
		return ERROR_CREAT_TRANSMISSION_DATA_CONNECTION;
	}
	else if(g_tgroupFile.file_size != g_tgroupFile.xor_size)
	{
		cout << "异或错误 ：file_size is ：" << g_tgroupFile.file_size << ", xor_size is : " << g_tgroupFile.xor_size << endl;
		return ERROR_CREAT_TRANSMISSION_DATA_CONNECTION;
	}
	else
		return RIGHT;
}
//
int create_client_connect_data_server(const off_t file_size, const vector<string> file_path, const char *output_file_name)
{

	LOG(INFO) << "创建socket传输文件的连接";
	//获取输入文件的文件属性
	int err;
	int len;
	char tmp[1024] = {0};					//存放文件的路径
	int thread_num = 0;						//线程的个数 初始化
	off_t file_num = file_path.size();		//一组文件的个数
	off_t file_total_size = file_size;		//文件总大小
	//初始化
	memset(&g_tgroupFile, 0, sizeof(g_tgroupFile));
	//一组文件的信息
	LOG(INFO) << "文件的个数 ：" << file_num;
	for (int i = 0; i < file_num; i++)
	{
		LOG(INFO) << "拷贝名字";
		//tGroupFileInfo.g_tgroupFile_name.push_back(file_name[i]);//一组文件
		//文件路径
		strcpy(g_tgroupFile.single_file_array[i].file_path, file_path[i].c_str());

	}
	g_tgroupFile.file_size = file_total_size;
	g_tgroupFile.file_num = file_num;
	g_tgroupFile.not_transfer_end_num = file_total_size;


	//几个文件创建几个线程,创建线程的时候创建文件，并把文件设置成
	LOG(INFO) << "创建线程, 一组文件的个数 ：" << file_num << ", 文件的大小 ： " << file_total_size;
	//打开输出文件
	memset(tmp, 0, 1024);
	snprintf(tmp, sizeof(tmp), "/home/file/%s", output_file_name);

	FILE *out_fd = fopen(tmp, "w");
	if (out_fd == NULL)
	{
		LOG(ERROR) << "打开或创建文件失败 : " << output_file_name << "errno : " << strerror(errno);

		return ERROR_CREAT_TRANSMISSION_DATA_CONNECTION;
	}
	truncate(tmp, file_total_size); //把文件打设置成发送过来的文件的大小

	pthread_t thread[file_num];
	for (int i = 0; i < file_num; i++)
	{
		int *arg = (int *)malloc(sizeof(*arg));
		if(arg == NULL)
		{
			LOG(ERROR) << "申请内存空间失败";
			return ERROR_CREAT_TRANSMISSION_DATA_CONNECTION;
		}
		*arg = i;//记录单个文件在文件容器的下标

		err = pthread_create(&thread[i], NULL, process_in_new_thread_file_num, arg);
		if (err != 0) //线程创建失败
		{
			LOG(ERROR) << "连接数据服务器的线程创建失败";
			g_tgroupFile.correct_flag = -1;
			return ERROR_CREAT_TRANSMISSION_DATA_CONNECTION;
		}
		pthread_detach(thread[i]);
		
	}
	
	//进行异或运算
	LOG(INFO) << "进行异或运算";
	int ret = judge_xor_condition(out_fd);
	
	LOG(INFO) << "关闭文件句柄";
	fclose(out_fd); //关闭文件句柄 */
	
	return ret;
}
