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
#include <sys/time.h>
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

/* for memory allocation test */
#if 1

static int count = 0;
static int use = 0;

void* test_malloc(size_t size)
{
	void* p;

	p = malloc(size + sizeof(int));
	if (p)
	{
		count++;
		*(int*)p = size;
		use += size;
	}
	return (void*)((char*)p + sizeof(int));
}

void test_free(void* block)
{
	void* p = block;
	p = (void *)((char*)p - sizeof(int));
	use -= *(int*)p;
	count--;
	free(p);
}

void check_used(void)
{
	printf("count = %d use = %d\r\n", count, use);
	if (count)
	{
		printf("***************************************************************\r\n");
		printf("************************* error *******************************\r\n");
		printf("***************************************************************\r\n");
	}
}
#else 
#define test_malloc malloc
#define test_free free
#define check_used()
#endif 


int fd;
/*字符包含判断*/
int starts_with(const char* prefix, const char* str)
{
	while(*prefix)
	{
		if (*prefix++ != *str++)
		{
			return 0;
		}
	}
	return 1;
}
/*判断是否存在*/
int FileExist(const char* filename)
{
    if (filename && access(filename, F_OK) == 0) {
        return 1;
    }
    return 0;
}
/*字符转小写*/
char* str_tolower(const char* str)
{
	size_t len = strlen(str);
    char *lower = calloc(len+1, sizeof(char));
    for (size_t i = 0; i < len; ++i) {
        lower[i] = tolower((unsigned char)str[i]);
    }
	/*free(upper);*/
	return lower;
}
/*字符转大写*/
char* str_toupper(const char* str)
{
	size_t len = strlen(str);
    char *upper = calloc(len+1, sizeof(char));
    for (size_t i = 0; i < len; ++i) {
        upper[i] = toupper((unsigned char)str[i]);
    }
	/*free(upper);*/
	return upper;
}
/*char*转char[]*/
char strx_tostrarr(const char* str)
{
	return 0;
}
/*
 *  这里的内容是处理%20之类的东西！是"解码"过程。
 *  %20 URL编码中的‘ ’(space)
 *  %21 '!' %22 '"' %23 '#' %24 '$'
 *  %25 '%' %26 '&' %27 ''' %28 '('......
 *  相关知识html中的‘ ’(space)是&nbsp
 */
void encode_str(char* to, int tosize, const char* from)
{
    int tolen;
 
    for (tolen = 0; *from != '\0' && tolen + 4 < tosize; ++from)
    {
        if (isalnum(*from) || strchr("/_.-~", *from) != (char*)0)
        {
            *to = *from;
            ++to;
            ++tolen;
        }
        else
        {
            sprintf(to, "%%%02x", (int) *from & 0xff);
            to += 3;
            tolen += 3;
        }
 
    }
    *to = '\0';
}
// 16进制数转化为10进制
int hexit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
 
    return 0;
}

void decode_str(char *to, char *from)
{
    for ( ; *from != '\0'; ++to, ++from  )
    {
        if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2]))
        {
 
            *to = hexit(from[1])*16 + hexit(from[2]);
 
            from += 2;
        }
        else
        {
            *to = *from;
 
        }
 
    }
    *to = '\0';
}
//字符向右查找实现截取内容
int trim_dots(const char * path)
{
    char *  pos = NULL;
    pos = strrchr(path, '/');
    printf("%s\n", pos);

    if(pos == NULL)
    {
        perror("strrchr");
        return -1;
    }
    if((strcmp(pos+1, ".") == 0) || (strcmp(pos+1, "..") == 0))
        return 0;
    else
        return -2;

    return 0;
}
// 创建函数（方法一）：是否包含字符串函数
int is_in(char *wenben, char *search_word)
{
    int i = 0, j = 0, flag = -1;
    while (i < strlen(wenben) && j < strlen(search_word))
    {
        if (wenben[i] == search_word[j])
        { //如果字符相同则两个字符都增加
            i++;
            j++;
        }
        else
        {
            i = i - j + 1; //主串字符回到比较最开始比较的后一个字符
            j = 0;         //字串字符重新开始
        }
        if (j == strlen(search_word))
        {             //如果匹配成功
            flag = 1; //字串出现
            break;
        }
    }
    return flag;
}

char *ATb = "\r\n";
char *dev_name = "/dev/ttyUSB3";//根据实际情况选择串口
int PORT = 8888;

#define _CRT_SECURE_NO_WARNINGS
#define BUFFER_SIZE 4096

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
 		if (strcmp(suffix, "favicon.ico") == 0)
		{

		}
		else{
			json_t json, t;
			/* create root node */
			json = json_create_object(NULL);
			char a[] ="HTTP/1.1 200 OK\r\nConnection: close\r\nAccept-Ranges: bytes\r\nContent-Type: application/json\r\n\r\n";
			if(starts_with("AT",suffix) == 0 && starts_with("at",suffix) == 0 && starts_with("At",suffix) == 0 && starts_with("aT",suffix) == 0){
				json_add_string_to_object(json, "Code", "404");
				json_add_string_to_object(json, "AT", suffix);
			}
			else if (strcmp(str_toupper(suffix), "AT+CMGL=4") == 0)
			{
				json_add_string_to_object(json, "Code", "404");
				json_add_string_to_object(json, "AT", "不支持读取短信列表，只可以当条读取");
			}
			else
			{

				//printf("1: %d\n", starts_with("AT",suffix));
				int nread;
				char buffer[MAX_BUFF_SIZE];
				serial_parse phandle;
				phandle.rxbuffsize = 0;
				//char ATa[] ="AT+QENG=\"servingcell\"";
				
				char *ATcStr = strcat(suffix, ATb);
				int ATlen = strlen(ATcStr); // 获取C风格字符串长度
				char sendAT[ATlen + 1]; // 创建与C风格字符串相同大小的char数组（+1表示空字符'\0'）
				strcpy(sendAT, ATcStr); // 将C风格字符串复制到char数组中
				write(fd, sendAT, strlen(sendAT));
				usleep(10000);  //休眠1ms

				nread = read(fd , phandle.buff + phandle.rxbuffsize, MAX_BUFF_SIZE - phandle.rxbuffsize);
				phandle.rxbuffsize += nread;
				phandle.buff[phandle.rxbuffsize] = '\0';
				json_add_string_to_object(json, "Code", "200");
				json_add_string_to_object(json, "AT", suffix);
				json_add_string_to_object(json, "Result", phandle.buff);
				printf("data: %s\n", phandle.buff);

			}

	

			char* out;
			int len;
			if (!json) return -1;
			out = json_dumps(json, 0, 0, &len);
			if (!out) return -1;
			char *b = out;
			char *cStr = strcat(a, b);
			int len2 = strlen(cStr); // 获取C风格字符串长度
			char result[len2]; // 创建与C风格字符串相同大小的char数组（+1表示空字符'\0'）
			strcpy(result, cStr); // 将C风格字符串复制到char数组中
			send(conn, (void *)result, sizeof(result), 0);


		}
	}
	close(conn);
	//关闭连接
}




int main(int argc,char *argv[]) {

	/* Used for testing memory usage */
	json_set_hooks(test_malloc, test_free, NULL);
    //int fd;
	PORT = 8888;
	ATb = "\r\n";
    //dev_name = "/dev/ttyUSB2";//根据实际情况选择串口
    while(1) 
    {  
        fd = OpenDev(dev_name); //打开串口 
        if(fd>0)
        {
            set_speed(fd,19200); //设置波特率
            printf("set speed success!\n");
        }     
        else  
        { 
            printf("Can't Open Serial Port!\n"); 
            sleep(1);  //休眠1s
            continue; 
        } 
        break;
    }

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
	return 0;
}
