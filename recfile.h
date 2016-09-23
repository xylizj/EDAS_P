#ifndef __RECFILE_H_
#define __RECFILE_H_

#define RECORD_FILE_LIST "/recordrecord.txt"
#define RECORD_NUM_MAX 100


typedef struct
{
	unsigned char filename[255];
	//int filecnt;
	int offset;
	int flag;       // 0:no trans 1:transce half 2:trans done	
}tFilesRecord;


extern tFilesRecord  filesRecord[RECORD_NUM_MAX];
extern int filescnt;


extern void get_recfile_list(void);
extern void get_recfile_name(void);
extern void save_recfile_list(void);





#endif/*__HANDLE_FILE_H_*/
