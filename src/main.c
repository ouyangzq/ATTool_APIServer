#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include <assert.h> 
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdarg.h>
#include <sys/select.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include "openDev.h"
#include "json.h"
#include "tool.h"
#include <sys/socket.h>
#ifdef __linux__
#include <sys/time.h>
unsigned long long reckon_usec(void)
{
	struct timeval mstime;
	unsigned long long us = 0;
	gettimeofday(&mstime, NULL);
	us = mstime.tv_sec * 1000000 + mstime.tv_usec;
	return us;
}
#else 
#define reckon_usec() 0
#endif 

static struct termios save_tio;
static int port = -1;
static const char* dev = "/dev/ttyUSB2";
static const char* storage = "";
static const char* dateformat = "%D %T";

int fd;

char *dev_name = "/dev/ttyUSB3";//根据实际情况选择串口
int PORT = 8888;
#define _CRT_SECURE_NO_WARNINGS
#define BUFFER_SIZE 4096


static void usage()
{
	fprintf(stderr,
		"usage: [options] send phoneNumber message\n"
		"       [options] recv\n"
		"       [options] delete msg_index | all\n"
		"       [options] status\n"
		"       [options] ussd code\n"
		"       [options] at command\n"
		"options:\n"
		"\t-b <baudrate> (default: 115200)\n"
		"\t-d <tty device> (default: /dev/ttyUSB0)\n"
		"\t-D debug (for ussd and at)\n"
		"\t-f <date/time format> (for sms/recv)\n"
		"\t-j json output (for sms/recv)\n"
		"\t-R use raw input (for ussd)\n"
		"\t-r use raw output (for ussd and sms/recv)\n"
		"\t-s <preferred storage> (for sms/recv/status)\n"
		);
	exit(2);
}
static void timeout()
{
	fprintf(stderr,"No response from modem.\n");
	exit(2);
}

int handle(int conn) {

	int len = 0;
	char buffer[BUFFER_SIZE];
	char *pos = buffer;
	bzero(buffer, BUFFER_SIZE);
	len = recv(conn, buffer, BUFFER_SIZE, 0);
	if (len <= 0 ) {
		printf ("recv error");
		return -1;
	} else {
		// //printf("Debug request:\n--------------\n%s\n\n",buffer);
		// 定义存放请求行、主机名等信息的变量
		char method[10];
		char path[100];
		char protocol[10];
		char hostname[100];
		char useragent[100];
		char acceptheader[100];
		// 将requestHeader按换行符分隔为多行
		int lineCount = 0;
		for(const char *line = strtok((char*)buffer, "\r\n"); line != NULL; line = strtok(NULL, "\r\n")){
			if(strlen(line)>0){
				switch(lineCount++){
					case 0: sscanf(line,"%s %[^ ]",method,path); break;
					case 1: sscanf(line,"%s:%s",hostname,protocol); break;
					case 2: sprintf(useragent,"%s",line); break;
					case 3: sprintf(acceptheader,"%s",line); break;
				}
			}
		}
		// printf("Method: %s\nPath: %s\nProtocol: %s\nHostname: %s\nUser Agent: %s\nAccept Header: %s\n", method, enstr, protocol, hostname, useragent, acceptheader);
		//文件类型判断 
		char enstr[1024] = {0};
		decode_str(enstr, path);
		char* suffix;
		if ((suffix = strrchr(enstr, '/')) != NULL)
			suffix = suffix + 1;
 		if (strcmp(suffix, "favicon.ico") == 0){

		}
		else{

			json_t json, t;
			/* create root node */
			json = json_create_object(NULL);
			time_t currentTime;
			currentTime = time(NULL);
			json_add_int_to_object(json, "time", (long)currentTime);
			char a[] ="HTTP/1.1 200 OK\r\nConnection: close\r\nAccept-Ranges: bytes\r\nContent-Type: application/json\r\n\r\n";
			if(starts_with("AT",suffix) == 0 && starts_with("at",suffix) == 0 && starts_with("At",suffix) == 0 && starts_with("aT",suffix) == 0){
				json_add_string_to_object(json, "Code", "404");
				json_add_string_to_object(json, "AT", suffix);
			}
			else if (strstr(str_toupper(suffix), "AT+CMGL=") || strstr(str_toupper(suffix), "AT+CMGR="))
			{
				json_add_string_to_object(json, "Code", "404");
				json_add_string_to_object(json, "AT", "不支持读取短信列表");
			}
			else
			{
				serial_parse phandle = SendAT(suffix);
				json_add_string_to_object(json, "Code", "200");
				json_add_string_to_object(json, "AT", suffix);
				json_add_string_to_object(json, "Result", phandle.buff);
			}
			char* out;
			int len;
			if (!json) return -1;
			out = json_dumps(json, 0, 0, &len);
			if (!out) return -1;
			char *b = out;
			char *cStr = strcat(a, b);
			char result[strlen(cStr)];
			strcpy(result, cStr);
			send(conn, result, sizeof(result), 0);
			//send(conn, (void *)result, sizeof(result), 0);
		}
	}
	close(conn);
	//关闭连接
}


int main(int argc,char *argv[]) {

	// int ch;
	// int baudrate = 115200;
	// int rawinput = 0;
	// int rawoutput = 0;
	// int jsonoutput = 0;
	// int debug = 0;

	// while ((ch = getopt(argc, argv, "b:d:Ds:f:jRr")) != -1){
	// 	switch (ch) {
	// 	case 'b': baudrate = atoi(optarg); break;
	// 	case 'd': dev = optarg; break;
	// 	case 'D': debug = 1; break;
	// 	case 's': storage = optarg; break;
	// 	case 'f': dateformat = optarg; break;
	// 	case 'j': jsonoutput = 1; break;
	// 	case 'R': rawinput = 1; break;
	// 	case 'r': rawoutput = 1; break;
	// 	default:
	// 		usage();
	// 	}
	// }

	// argv += optind; argc -= optind;

	// if (argc < 1)
	// 	usage();
	// if (!strcmp("send", argv[0]))
	// {
	// 	if(argc < 3)
	// 		usage();
	// 	if(strlen(argv[2]) > 160)
	// 		fprintf(stderr,"sms message too long: '%s'\n", argv[2]);
	// }else if (!strcmp("delete",argv[0]))
	// {
	// 	if(argc < 2)
	// 		usage();
	// }else if (!strcmp("recv", argv[0]))
	// {
	// }else if (!strcmp("status", argv[0]))
	// {
	// }else if (!strcmp("ussd", argv[0]))
	// {
	// }else if (!strcmp("at", argv[0])){
	// 	if(argc < 2)
	// 		usage();
	// }else{
	// 	usage();
	// }
		

	// signal(SIGALRM,timeout);

    //int fd;
	PORT = 8888;
    //dev_name = "/dev/ttyUSB2";//根据实际情况选择串口
    fd = OpenDev(dev_name);

    if(set_Parity(fd,8,1,'N')==FALSE) //设置校验位 
    {
        printf("Set Parity Error\n"); 
        exit(1);
    }
    else
    {
        printf("Set Parity Success!\n"); 
    }

	struct sockaddr_in client_sockaddr;
	struct sockaddr_in server_sockaddr;
	int listenfd = socket(AF_INET,SOCK_STREAM,0);
	int opt = 1;
	int conn;
	socklen_t length = sizeof(struct sockaddr_in);
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));
	server_sockaddr.sin_family = AF_INET;
	server_sockaddr.sin_port = htons(PORT);
	server_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	if(bind(listenfd,(struct sockaddr *)&server_sockaddr,sizeof(server_sockaddr))==-1) {
		printf("bind error!\n");
		return -1;
	}
	if(listen(listenfd, 10) < 0) {
		printf("listen failed!\n");
		return -1;
	}
	while(1) {
		conn = accept(listenfd, (struct sockaddr*)&client_sockaddr, &length);
		if(conn < 0) {
			printf("connect error!\n");
			 sleep(1); 
			continue;
		}
		if (handle(conn) < 0) {
			printf("connect error!\n");
			close(conn);
			 sleep(1); 
			continue;
		}
	}
	close(fd);
	return 0;
}
