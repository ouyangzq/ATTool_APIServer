#include "tool.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

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
