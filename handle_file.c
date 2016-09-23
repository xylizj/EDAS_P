#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include "common.h"

#define FileRecordRecord "/recordrecord.txt"





tFilesRecord  filesRecord[100];
int filescnt;
int curTransFile = -1;

tFilesRecord  filesOldRecord[100];
int fileoldscnt;



#if 1

void getFilesRecords()
{
	FILE *fp;
	int fsize;
	char buff[100];

	
	fp = fopen(FileRecordRecord,"r");
	if(fp == NULL)		
	{
		printf("no FilesRecordfile file\n");
		return;
	}
	else
	{
		rewind(fp);
		
		while(fgets(buff,100,fp)!=NULL)
		{
			switch(buff[0])
			{
				case '$':  //file name
					memcpy(filesOldRecord[fileoldscnt].filename,&buff[1],strlen(buff)-1);
					break;
				//case '#':  //offset
				//	filesOldRecord[fileoldscnt].offset = atoi(buff);
				//	break;
				case '*':  //flag
					filesOldRecord[fileoldscnt].flag = buff[1]-'0';
					fileoldscnt++;
					break;
				default: break;
			}
			if(fileoldscnt >= 100) break;
		}	
		
	}
	fclose(fp);	
}
#endif
#if 1
void getFilesName()
{
	DIR *dirptr = NULL;
	struct dirent *entry;
	int i;

	int cnt = 0;
	
	cnt = 0;
		
	if((dirptr = opendir(RECORDDIR)) == NULL)
	{
		printf("open dir error !\n");
		return;
	}
	else
	{
		printf_va_args("getFilesName ok\n");
		while (entry = readdir(dirptr))
		{			
			printf_va_args("%s\n", entry->d_name);

			if((strcmp(entry->d_name,".")==0)||(strcmp(entry->d_name,"..")==0))
			{
				continue;
			}
			
			memset(filesRecord[cnt].filename,0,255);
			strcpy(filesRecord[cnt].filename,entry->d_name);

			filesRecord[cnt].flag = 0;
			filesRecord[cnt].offset = 0;
			
			for(i=0;i<fileoldscnt;i++)
			{				
				if(memcmp(filesRecord[cnt].filename,filesOldRecord[i].filename,strlen(filesRecord[filescnt].filename))==0)
				{
					filesRecord[cnt].flag = filesOldRecord[i].flag;
					filesRecord[cnt].offset = filesOldRecord[i].offset;
					break;					
				}
			}
			cnt++;		
			
			filescnt = cnt;
			if(cnt >= 100) break;
		}
		closedir(dirptr);
	}
}
#endif
void writeFilesRecords()
{
	int i;
	FILE *fp;
	char buf[100];
	
	fp = fopen(FileRecordRecord,"w");

#if 1
	for(i=0;i<filescnt;i++)
	{
		memset(buf,0,100);
		buf[0] = '$';
		memcpy(&buf[1],filesRecord[i].filename,strlen(filesRecord[i].filename));
		fwrite(buf,strlen(buf),1,fp);
		fwrite("\n",1,1,fp);

#if 1

		memset(buf,0,100);
		buf[0] = '*';
		switch(filesRecord[i].flag)
		{
			case 0:
				buf[1] = '0';
				break;
			case 1:
				buf[1] = '1';
				break;
			case 2:
				buf[1] = '2';
				break;
		}		
		fwrite(buf,2,1,fp);
		fwrite("\n",1,1,fp);	
#endif
	}
#endif
	fflush(fp);
	fclose(fp);
}












