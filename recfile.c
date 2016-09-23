#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include "common.h"
#include "recfile.h"





int curTransFile = -1;

tFilesRecord  filesRecord[RECORD_NUM_MAX];
tFilesRecord  filesOldRecord[RECORD_NUM_MAX];
int filescnt;
int fileoldscnt;



void get_recfile_list(void)
{
	FILE *fp;
	int fsize;
	char buff[RECORD_NUM_MAX];
	
	fp = fopen(RECORD_FILE_LIST,"r");
	if(fp == NULL)		
	{
		printf("no FilesRecordfile file\n");
		return;
	}
	else
	{
		rewind(fp);
		
		while(fgets(buff,RECORD_NUM_MAX,fp)!=NULL)
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
				default: 
					break;
			}
			if(fileoldscnt >= RECORD_NUM_MAX) 
				break;
		}			
	}
	fclose(fp);	
}



void get_recfile_name(void)
{
	DIR *dirptr = NULL;
	struct dirent *entry;
	int i;
	int cnt;
		
	if((dirptr = opendir(RECORDDIR)) == NULL)
	{
		printf("open dir error !\n");
		return;
	}
	else
	{
		printf_va_args("get_recfile_name ok\n");
		while (entry = readdir(dirptr))
		{			
			printf_va_args("%s\n", entry->d_name);

			if((strcmp(entry->d_name,".")==0)||(strcmp(entry->d_name,"..")==0))
			{
				continue;
			}

			cnt = 0;
			memset(filesRecord[cnt].filename,0,255);
			strcpy(filesRecord[cnt].filename,entry->d_name);

			filesRecord[cnt].flag = 0;
			filesRecord[cnt].offset = 0;
			
			for(i=0; i<fileoldscnt; i++)
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
			if(cnt >= RECORD_NUM_MAX) 
				break;
		}
		closedir(dirptr);
	}
}


void save_recfile_list(void)
{
	int i;
	FILE *fp;
	char buf[RECORD_NUM_MAX];
	
	fp = fopen(RECORD_FILE_LIST,"w");

	for(i=0; i<filescnt; i++)
	{
		memset(buf,0,RECORD_NUM_MAX);
		buf[0] = '$';
		memcpy(&buf[1],filesRecord[i].filename,strlen(filesRecord[i].filename));
		fwrite(buf,strlen(buf),1,fp);
		fwrite("\n",1,1,fp);
		memset(buf,0,RECORD_NUM_MAX);
		buf[0] = '*';
		buf[1] = '0' + filesRecord[i].flag;
		fwrite(buf,2,1,fp);
		fwrite("\n",1,1,fp);	
	}

	fflush(fp);
	fclose(fp);
}












