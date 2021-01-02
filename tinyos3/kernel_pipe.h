/*
 * kernel_pipe.h
 *
 *  Created on: 27 Νώε 2017
 *      Author: george
 */


#ifndef KERNEL_PIPE_H_
#define KERNEL_PIPE_H_



#include "tinyos.h"


#define MAX_NUM_OF_DATA 512


typedef struct pipe_control_block{
	Fid_t reader;
	Fid_t writer;
	FCB* FCBr;
	FCB* FCBw;
	int read_count;
	int write_count;
	int read_count_16k;
	int write_count_16k;
	Mutex pipe_mx;
	CondVar readerCV;
	CondVar writerCV;
	char buffer[MAX_NUM_OF_DATA];

}PPCB;

int sys_Pipe(pipe_t* pipe);

int closeReader(void* fid);

int closeWriter(void* fid);

int reader(void* reader, char* buffer,unsigned int size);

int writer(void* writer,const char* buffer,unsigned int size);

#endif /* KERNEL_PIPE_H_ */
