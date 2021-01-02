#include "tinyos.h"
#include "kernel_streams.h"
#include "kernel_pipe.h"
#include "kernel_dev.h"
#include "kernel_proc.h"
#include "util.h"
#include "kernel_cc.h"

file_ops pipe_fops_writer = { .Open = NULL, //writer,
		.Read = NULL, .Write = writer, .Close = closeWriter };

file_ops pipe_fops_reader = { .Open = NULL, //reader,
		.Read = reader, .Write = NULL, .Close = closeReader };

int sys_Pipe(pipe_t* pipe) {

	PPCB* ppcb = (PPCB *) xmalloc(sizeof(PPCB));
	FCB* fcb[2];
	Fid_t fid[2];

	fcb[0] = NULL;
	fcb[1] = NULL;

	int ret1 = FCB_reserve(2, fid, fcb);

	if (ret1 == 1) {
		pipe->read = fid[0];
		pipe->write = fid[1];
		ppcb->reader = fid[0];
		ppcb->writer = fid[1];
		ppcb->FCBr = fcb[0];
		ppcb->FCBw = fcb[1];
		ppcb->readerCV = COND_INIT;
		ppcb->writerCV = COND_INIT;
		ppcb->pipe_mx = MUTEX_INIT;
		ppcb->FCBr->streamobj = ppcb;
		ppcb->FCBw->streamobj = ppcb;
		ppcb->read_count_16k = 0;
		ppcb->write_count_16k = 0;
		ppcb->read_count = 0;
		ppcb->write_count = 0;

		ppcb->FCBr->streamfunc = &pipe_fops_reader;
		ppcb->FCBw->streamfunc = &pipe_fops_writer;

		return 0;
	}

	return -1;
}

int reader(void* reader, char* buffer, unsigned int size) {
	PPCB* ppcb = (PPCB*) reader;

	int until = 0;
	int read_size = size; //posa bytes prepei na kanoyme read
	int read_count = 0; //posa exoume kanei read
	int bytes = ppcb->write_count - ppcb->read_count; //diathesima bytes gia read
	int b = 0;

	if (ppcb->FCBr == NULL)
		return -1;

	while (bytes == 0 && ppcb->FCBw != NULL) { //an den yparxoun bytes gia read,koimizoume ton reader
		kernel_broadcast(&ppcb->writerCV);
		kernel_wait(&ppcb->readerCV, SCHED_PIPE);
		bytes = ppcb->write_count - ppcb->read_count;
	}

	while (read_size > 0 && bytes > 0) { //elegxoume an exoun meinei bytes gia read
		if (read_size <= bytes) {
			until = read_size;
		} else {
			until = bytes;
		}
		for (int i = 0; i < until; i++) {
			if (ppcb->read_count_16k < MAX_NUM_OF_DATA) {
				buffer[i + b] = ppcb->buffer[ppcb->read_count_16k];
			} else {
				ppcb->read_count_16k = 0;
				buffer[i + b] = ppcb->buffer[ppcb->read_count_16k];
			}
			ppcb->read_count++;
			ppcb->read_count_16k++;
			read_count++;

		}
		kernel_broadcast(&ppcb->writerCV);
		read_size = read_size - read_count;
		b = read_count + b;

		bytes = ppcb->write_count - ppcb->read_count;
		read_count = 0;

	}

	return size - read_size;

}

int writer(void* writer, const char* buffer, unsigned int size) {
	PPCB* ppcb = (PPCB*) writer;

	int write_size = size;
	int write_count = 0;
	int b = 0;
	int until = 0;
	int bytes = MAX_NUM_OF_DATA - (ppcb->write_count - ppcb->read_count);
	if (ppcb->FCBr == NULL || ppcb->FCBw == NULL)
		return -1;

	while (bytes == 0) { //an den yparxoun bytes gia write,koimizoume ton writer
		kernel_broadcast(&ppcb->readerCV);
		kernel_wait(&ppcb->writerCV, SCHED_PIPE);
		bytes = MAX_NUM_OF_DATA - (ppcb->write_count - ppcb->read_count);
	}

	while (write_size > 0 && bytes > 0) {
		if (write_size <= bytes) {
			until = write_size;
		} else {
			until = bytes;
		}
		for (int i = 0; i < until; i++) {
			if (ppcb->write_count_16k >= MAX_NUM_OF_DATA) ppcb->write_count_16k = 0;
			ppcb->buffer[ppcb->write_count_16k] = buffer[i + b];

			ppcb->write_count++;
			ppcb->write_count_16k++;
			write_count++;

		}
		kernel_broadcast(&ppcb->readerCV);
		write_size = write_size - write_count;
		b = write_count + b;
		bytes = MAX_NUM_OF_DATA - (ppcb->write_count - ppcb->read_count);
		write_count = 0;
	}

	return size - write_size;

}

int closeWriter(void* fid) {

	PPCB* ppcb = (PPCB*) fid;
	if (ppcb->FCBw->refcount == 0) {

		ppcb->FCBw = NULL;
		ppcb->writer = -1;

		if (ppcb->FCBr == NULL) {
			free(ppcb);
			ppcb = NULL;
		} else
			kernel_broadcast(&ppcb->readerCV);
		return 0;

	}

	return -1;
}

int closeReader(void* fid) {

	PPCB* ppcb = (PPCB*) fid;
	if (ppcb->FCBr->refcount == 0) {

		ppcb->FCBr = NULL;
		ppcb->reader = -1;

		if (ppcb->FCBw == NULL) {
			free(ppcb);
			ppcb = NULL;
			return 0;
		} else
			kernel_broadcast(&ppcb->writerCV);
		return 0;

	}

	return -1;

}

