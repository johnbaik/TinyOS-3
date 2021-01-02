/*
 * kernel_socket.h
 *
 *  Created on: Deca 6, 2017
 *      Author: george
 */

#ifndef KERNEL_SOCKET_H_
#define KERNEL_SOCKET_H_

#include "tinyos.h"
#include "kernel_streams.h"

typedef enum SocketType {
    UNBOUND,
    LISTENER,
    PEER
} soctype;

typedef struct socket_control_block SCB;

typedef struct Listener{
    rlnode requests;
    CondVar listenerCV;
} listener;
typedef struct Peer{
    pipe_t write;
	pipe_t read;
    SCB* PtoP;
} peer;

typedef struct request{
	Fid_t fid;
	FCB *fcb;
	SCB *scb;
	CondVar requestCV;
	int admited;
}request;


typedef struct socket_control_block{
	Fid_t fid;
	FCB* FCBs;
	soctype type;
	port_t port;
	int refcount;
	listener* l;
	peer* p;
}SCB;

SCB *Portmap[MAX_PORT + 1];

int sread(void* read, char* buffer, unsigned int size);
int swrite(void* write, const char* buffer, unsigned int size);
int sclose(void* fid);
Fid_t sys_Socket(port_t port);


int sys_Listen(Fid_t socket);



Fid_t sys_Accept(Fid_t lsocket);



int sys_Connect(Fid_t socket, port_t port, timeout_t timeout);



int sys_ShutDown(Fid_t socket, shutdown_mode how);

#endif /* KERNEL_SOCKET_H_ */
