#include "tinyos.h"
#include "kernel_streams.h"
#include "kernel_pipe.h"
#include "kernel_dev.h"
#include "kernel_proc.h"
#include "util.h"
#include "kernel_cc.h"
#include "kernel_socket.h"

file_ops socket_fops = { .Open = NULL, .Read = sread, .Write = swrite, .Close =
		sclose };
file_ops pipe_fops_writer1 = { .Open = NULL, //writer,
		.Read = NULL, .Write = writer, .Close = closeWriter };

file_ops pipe_fops_reader1 = { .Open = NULL, //reader,
		.Read = reader, .Write = NULL, .Close = closeReader };

int sread(void* read, char* buffer, unsigned int size) {
	SCB *scb = (SCB *) read;
	if (scb != NULL) {
		pipe_t pipe = scb->p->read;
		if (scb->type == PEER) {
			int reader_ret = reader(&pipe.read, buffer, size);
			return reader_ret;
		}
	}
	return -1;

}
int swrite(void* write, const char* buffer, unsigned int size) {

	SCB *scb = (SCB *) write;
	if (scb != NULL) {
		pipe_t pipe = scb->p->write;

		if (scb->type == PEER) {
			int writer_ret = writer(&pipe.write, buffer, size);
			return writer_ret;
		}
	}
	return -1;
}

int sclose(void* fid) {

	SCB* scb = (SCB*) fid;
	if (scb == NULL)
		return -1;

	if (scb->type == UNBOUND) {
		free(scb);
		return 0;

	}
	if (scb->type == LISTENER) {
		Portmap[scb->port] = NULL;

		while (!is_rlist_empty(&scb->l->requests)) {
			rlnode *node = rlist_pop_front(&scb->l->requests);
			kernel_broadcast(&node->req->requestCV);
		}

		free(scb->l);
		free(scb);

		return 0;
	}
	if (scb->type == PEER) {

		if (scb->p->PtoP != NULL) {
			closeReader(&scb->p->read);
			closeWriter(&scb->p->write);
			scb->p->PtoP->p->PtoP = NULL;
		}

		free(scb->p);
		free(scb);

		return 0;

	}
	return -1;
}

Fid_t sys_Socket(port_t port) {

	if (port < 0 || port > MAX_PORT)
		return -1;

	SCB* scb = (SCB *) xmalloc(sizeof(SCB));
	FCB* fcb=NULL;
	Fid_t fid=-1;

	int ret = FCB_reserve(1, &fid, &fcb);
	if (ret == 1) {
		scb->FCBs = fcb;
		scb->fid = fid;
		scb->type = UNBOUND;
		scb->port = port;
		scb->refcount = 0;
		scb->FCBs->streamfunc = &socket_fops;
		scb->FCBs->streamobj = scb;
		return fid;
	}

	free(scb);
	return -1;
}

int sys_Listen(Fid_t socket) {
	
	FCB *fcb = get_fcb(socket);
	if (fcb == NULL)
		return -1;
	SCB* scb = fcb->streamobj;
	if (scb == NULL)
		return -1;

	if (scb->port <= 0 || scb->port > MAX_PORT)return -1;


	if (Portmap[scb->port] != NULL||scb->type != UNBOUND )
		return -1;

	scb->type = LISTENER;
	scb->p = NULL;
	scb->l = (listener *) xmalloc(sizeof(listener));
	scb->l->listenerCV = COND_INIT;
	rlnode_init(&scb->l->requests, NULL);
	Portmap[scb->port] = scb;
	return 0;
}

Fid_t sys_Accept(Fid_t lsocket) {

	FCB *fcb = get_fcb(lsocket);
	if (fcb == NULL){

		return -1;}

	SCB* lscb = fcb->streamobj;

	if (lscb == NULL || lscb->type != LISTENER){

		return -1;}

	while (is_rlist_empty(&lscb->l->requests) != 0 && lscb != NULL) {
		kernel_wait(&lscb->l->listenerCV, SCHED_USER);
	}

	request *req = rlist_pop_front(&lscb->l->requests)->req;

	fcb = get_fcb(lsocket);
		if (fcb == NULL){

			return -1;}

		lscb = fcb->streamobj;

	if (lscb == NULL) {
		kernel_broadcast(&req->requestCV);
		return -1;
	}


	Fid_t first_connectorfid = Socket(NOPORT);
	if(first_connectorfid==NOFILE){
		kernel_broadcast(&req->requestCV);
	    return -1;
	}


	FCB *fcb1 = get_fcb(first_connectorfid);
	if (fcb1 == NULL)
		return -1;
	SCB* scbserver = fcb1->streamobj;
	if (scbserver == NULL)
		return -1;

	SCB *scbclient = req->scb;

	peer *peerserver = (peer*) xmalloc(sizeof(peer));

	peer *peerclient = (peer*) xmalloc(sizeof(peer));

	scbserver->p = peerserver;
	scbserver->l = NULL;

	scbclient->p = peerclient;
	scbclient->l = NULL;

	pipe_t serverWrite_pipe;
	pipe_t clientWrite_pipe;

	PPCB* ppcb_server_write_pipe = (PPCB *) xmalloc(sizeof(PPCB));
	PPCB* ppcb_server_read_pipe = (PPCB *) xmalloc(sizeof(PPCB));


	serverWrite_pipe.read = req->fid;	//sundeoume to akro read tou write pipe tou server ston client socket
	serverWrite_pipe.write = scbserver->fid;//sundeoume to akro write tou write pipe tou server ston server socket
	ppcb_server_write_pipe->reader = req->fid;
	ppcb_server_write_pipe->writer = scbserver->fid;

	ppcb_server_write_pipe->FCBr = req->fcb;
	ppcb_server_write_pipe->FCBw = fcb1;
	ppcb_server_write_pipe->readerCV = COND_INIT;
	ppcb_server_write_pipe->writerCV = COND_INIT;
	ppcb_server_write_pipe->pipe_mx = MUTEX_INIT;
	ppcb_server_write_pipe->FCBr->streamobj = ppcb_server_write_pipe;
	ppcb_server_write_pipe->FCBw->streamobj = ppcb_server_write_pipe;
	ppcb_server_write_pipe->read_count_16k = 0;
	ppcb_server_write_pipe->write_count_16k = 0;
	ppcb_server_write_pipe->read_count = 0;
	ppcb_server_write_pipe->write_count = 0;

	ppcb_server_write_pipe->FCBr->streamfunc = &pipe_fops_reader1;
	ppcb_server_write_pipe->FCBw->streamfunc = &pipe_fops_writer1;


	clientWrite_pipe.read = scbserver->fid;//sundeoume to akro read tou read pipe tou server ston server socket
	clientWrite_pipe.write = req->fid;//sundeoume to akro write tou read pipe tou server ston client socket
	ppcb_server_read_pipe->reader = scbserver->fid;
	ppcb_server_write_pipe->writer = req->fid;
	ppcb_server_read_pipe->FCBr = fcb1;
	ppcb_server_read_pipe->FCBw = req->fcb;
	ppcb_server_read_pipe->readerCV = COND_INIT;
	ppcb_server_read_pipe->writerCV = COND_INIT;
	ppcb_server_read_pipe->pipe_mx = MUTEX_INIT;
	ppcb_server_read_pipe->FCBr->streamobj = ppcb_server_read_pipe;
	ppcb_server_read_pipe->FCBw->streamobj = ppcb_server_read_pipe;
	ppcb_server_read_pipe->read_count_16k = 0;
	ppcb_server_read_pipe->write_count_16k = 0;
	ppcb_server_read_pipe->read_count = 0;
	ppcb_server_read_pipe->write_count = 0;

	ppcb_server_read_pipe->FCBr->streamfunc = &pipe_fops_reader1;
	ppcb_server_read_pipe->FCBw->streamfunc = &pipe_fops_writer1;

	peerserver->PtoP = scbclient; // peer to peer connect
	peerserver->write = serverWrite_pipe; //o server grafei se auto to pipe
	peerserver->read = clientWrite_pipe; //o server diabazei apo auto to pipe

	peerclient->PtoP = scbserver; // peer to peer connect
	peerclient->write = clientWrite_pipe; //o client grafei se auto to pipe
	peerclient->read = serverWrite_pipe; //o client diabazei apo auto to pipe

	scbclient->type = PEER;
	scbserver->type = PEER;
	req->admited = 0;

	kernel_broadcast(&req->requestCV);


	return scbserver->fid;
}

int sys_Connect(Fid_t client, port_t lport, timeout_t timeout) {
	if (lport < 0 || lport > MAX_PORT)return -1;

	FCB *fcb = get_fcb(client);
	if (fcb == NULL){

		return -1;}
	SCB* scb_client = fcb->streamobj;
	if (scb_client == NULL){

		return -1;}
	if( Portmap[lport] == NULL) return -1;

	if (Portmap[lport]->type != LISTENER )return -1;

	if (scb_client->type !=UNBOUND) return-1;


	request* req = (request*) xmalloc(sizeof(request));


	req->scb = scb_client;
	req->fcb = scb_client->FCBs;
	req->fid = scb_client->fid;
	req->requestCV = COND_INIT;
	req->admited = -1;
	rlnode tmp;

	rlnode_init(&tmp, req);

	rlist_push_back(&Portmap[lport]->l->requests, &tmp);


	kernel_broadcast(&Portmap[lport]->l->listenerCV);

	kernel_unlock();

	kernel_timedwait(&req->requestCV, SCHED_USER, timeout);


	rlist_remove(&tmp);

	return req->admited;
}

int sys_ShutDown(Fid_t socket, shutdown_mode how) {

	FCB *fcb = get_fcb(socket);
	if (fcb == NULL){
		return -1;}
	SCB* scb = fcb->streamobj;
	if (scb == NULL){
		return -1;}

	if (how == SHUTDOWN_READ)
		return closeReader(&scb->p->read);
	else if (how == SHUTDOWN_WRITE)
		return closeWriter(&scb->p->write);
	else
		return sclose(scb);

	return -1;
}
