

int starts_with(const char* prefix, const char* str);
int FileExist(const char* filename);
char* str_tolower(const char* str);
char* str_toupper(const char* str);
/*char*转char[]*/
char strx_tostrarr(const char* str);
/*
 *  这里的内容是处理%20之类的东西！是"解码"过程。
 *  %20 URL编码中的‘ ’(space)
 *  %21 '!' %22 '"' %23 '#' %24 '$'
 *  %25 '%' %26 '&' %27 ''' %28 '('......
 *  相关知识html中的‘ ’(space)是&nbsp
 */
void encode_str(char* to, int tosize, const char* from);
// 16进制数转化为10进制
int hexit(char c);

void decode_str(char *to, char *from);

//字符向右查找实现截取内容
int trim_dots(const char * path);

// 创建函数（方法一）：是否包含字符串函数
int is_in(char *wenben, char *search_word);
