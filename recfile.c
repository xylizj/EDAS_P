#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include "common.h"
#include "recfile.h"


tFilesRecord  filesRecord[RECORD_NUM_MAX];
tFilesRecord  filesOldRecord[RECORD_NUM_MAX];
unsigned char *auth_code;//unsigned char auth_code[509]={0};


struct _recfile_info recfile_info;


void get_recfile_list(void)
{
	FILE *fp;
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
				filesOldRecord[recfile_info.fileoldscnt].filename = (char *)malloc(strlen(buff)-1);
				if(NULL != filesOldRecord[recfile_info.fileoldscnt].filename){
					memcpy(filesOldRecord[recfile_info.fileoldscnt].filename,&buff[1],strlen(buff)-1);
				}
				break;
				//case '#':  //offset
				//	filesOldRecord[recfile_info.fileoldscnt].offset = atoi(buff);
				//	break;
				case '*':  //flag
					filesOldRecord[recfile_info.fileoldscnt].flag = buff[1]-'0';
					recfile_info.fileoldscnt++;
					break;
				default: 
					break;
			}
			if(recfile_info.fileoldscnt >= RECORD_NUM_MAX) 
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
	int cnt = 0;
		
	if((dirptr = opendir(RECORDDIR)) == NULL)
	{
		printf("open dir error !\n");
		return;
	}
	else
	{
		printf_va_args("get_recfile_name ok\n");
		while ((entry = readdir(dirptr)) != NULL)
		{			
			printf_va_args("%s\n", entry->d_name);

			if((strcmp(entry->d_name,".")==0)||(strcmp(entry->d_name,"..")==0))
			{
				continue;
			}

			filesRecord[cnt].filename = (char *)malloc(strlen(filesRecord[recfile_info.filescnt].filename));
			if(NULL != filesRecord[cnt].filename)
			{
				memset(filesRecord[cnt].filename,0,strlen(filesRecord[recfile_info.filescnt].filename));
				strcpy(filesRecord[cnt].filename,entry->d_name);

				filesRecord[cnt].flag = 0;
				filesRecord[cnt].offset = 0;
				
				for(i=0; i<recfile_info.fileoldscnt; i++)
				{				
					if(NULL != filesOldRecord[i].filename){
						if(memcmp(filesRecord[cnt].filename,filesOldRecord[i].filename,strlen(filesRecord[recfile_info.filescnt].filename))==0)
						{
							free(filesOldRecord[i].filename);
							filesOldRecord[i].filename = NULL;
							filesRecord[cnt].flag = filesOldRecord[i].flag;
							filesRecord[cnt].offset = filesOldRecord[i].offset;
							break;					
						}
					}
				}
				cnt++;		
				
				recfile_info.filescnt = cnt;
				if(cnt >= RECORD_NUM_MAX) 
					break;
			}
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

	for(i=0; i<recfile_info.filescnt; i++)
	{
		if(NULL != filesRecord[i].filename){
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

			free(filesRecord[i].filename);
			filesRecord[i].filename = NULL;
		}
	}

	fflush(fp);
	fclose(fp);
}












