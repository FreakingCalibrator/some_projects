#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>

int main(int argc, char** argv)
{
DIR* dir;
char buff[41];
char c;
struct dirent *entry;
int pid,i,count=0;
dir=opendir("/proc/");
if (!dir){
	printf("Eroor in diropen proc");
	exit(1);
}
while ((entry=readdir(dir))!=NULL)
{
	char str[30]="/proc/";
	if (atoi(entry->d_name))
	{
		i=0;
		strcat(str,entry->d_name);
		strcat(str,"/status");
		strcat(str,"\0");
		//printf("%s-------",str);
		pid=open(str,O_RDONLY);
		for (i=0;i<6;i++)
			read(pid,&c,1);
		read(pid,&c,1);
		for (i=0;i<30;i++)
			buff[i]=0;
		i=0;
		while (c!='\n'){
			//printf("%d",(int)c);
			//strcat(buff,c);
			buff[i++]=c;
			read(pid,&c,1);
		}
		strcat(buff,"\0");
		//buff[i++]=0;
		//printf("%s\n",buff);
		close(pid);
		count+=!strcmp(buff,"genenv");
	}
}
printf("%d\n",count);
return 0;
}
