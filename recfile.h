#ifndef __RECFILE_H_
#define __RECFILE_H_

#define RECORD_FILE_LIST "/recordrecord.txt"
#define RECORD_NUM_MAX 100
#define PATH_AND_FILE_LEN 100


typedef struct
{
	char *filename;//unsigned char filename[255];
	int offset;
	int flag;       // 0:no trans 1:transce half 2:trans done	
}tFilesRecord;

struct _recfile_info{
	int filescnt;
	int fileoldscnt;
};



extern tFilesRecord  filesRecord[RECORD_NUM_MAX];
extern struct _recfile_info recfile_info;
extern unsigned char *auth_code;//extern unsigned char auth_code[509];



extern void get_recfile_list(void);
extern void get_recfile_name(void);
extern void save_recfile_list(void);





#endif/*__HANDLE_FILE_H_*/
