#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
int main()
{
char str[5];
int num=0;
char c;
int len;
int fd=open("/proc/self/status", O_RDONLY);
while ((read(fd,&str,4))>0)
{
str[4]=0;
c=9;
//printf("%s", str);

if (strcmp(str,"PPid"))
while (c!='\n'){
read(fd,&c,1);
//printf("%c ", c);
}
else
{
read(fd,&c,1);
while (c!='\n'){
read(fd,&c,1);
if (c>47&&c<58)
num=num*10+(int)c-48;
}
break;
}
}
printf("%d\n",num);
close(fd);
return 0;
} //
