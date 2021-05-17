#include "glog/logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace std;

void log_info_init(const char *exec_path, const char *log_path)
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
	FLAGS_logbufsecs = 5;

	//设置日志文件大小，单位M，默认1800MB
	FLAGS_max_log_size = 50;
	
}

void test11()
{
	//第1,11,21...次输出log信息
	LOG_EVERY_N(INFO, 10) << "Got the " << google::COUNTER << "th exec this func";
}

int main(int argc, char* argv[])
{
 	//传入log path
	log_info_init(argv[0], argv[1]);

	for(int i=0; i<10000000; i++)
	{
		//高级别的日志会在低级别的日志中输出
		//INFO级别的日志
		LOG(INFO) << "Hello,GOOGLE!";
		//WARNING级别的日志
		LOG(WARNING) << "warning test"; 
		//ERROR级别的日志
		LOG(ERROR) << "ERROR, GOOGLE!";
		usleep(100);
	}

	//满足 num > 10 时才会打印出info信息
	int num = 9;
	LOG_IF(INFO, num > 10) << "num is:" << num;

	for(num = 0; num < 30; num ++)
		test11();	

	
	google::ShutdownGoogleLogging();
}
