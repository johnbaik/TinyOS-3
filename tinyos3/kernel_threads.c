
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "util.h"
#include "kernel_cc.h"

static Mutex kernel_mutex = MUTEX_INIT;


int find(Tid_t tid){

	for(int i=0;i<MAX_NUM_OF_PTCBS; i++){

		if(CURPROC->ptcb[i]->tid==tid){
			return i;
		}
	}

	return -1;
}

void start_thread()
{

	  int exitval;

	  Task call =  CURTHREAD->owner_ptcb->main_task;
	  int argl = CURTHREAD->owner_ptcb->argl;
	  void*  args  = CURTHREAD->owner_ptcb->args;

	  exitval= call(argl,args);

	 ThreadExit(exitval);
}

PTCB* spawn_ptcb1(PCB* pcb, TCB* tcb,Task task, int argl, void* args){

	  PTCB* ptcb = (PTCB*) xmalloc(sizeof(PTCB));
	  ptcb->pcb=pcb;//dixnei to pcb
	  ptcb->argl=argl;

	  if(args!=NULL) {
		  ptcb->args=args;
	  }
	  else
		  ptcb->args=NULL;

	  ptcb->ref_count=1;
	  ptcb->main_task=task;
	  ptcb->tcb=tcb;

      ptcb->is_main=0;
	  pcb->ptcbcount++;
	  ptcb->is_detach=UNDETACH;
	  ptcb->ptcb_cv=COND_INIT;
	  ptcb->tid= (Tid_t)tcb;
	  ptcb->state=tcb->state;
return ptcb;


}


/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{
	if(task != NULL) {
		int i=0;

		while(CURPROC->ptcb[i]!=NULL){
			i++;

		}

		TCB* tcb=spawn_thread(CURPROC,start_thread);
		CURPROC->ptcb[i]=spawn_ptcb1(CURPROC,tcb, task,  argl,  args);
		CURPROC->ptcb[i]->tcb->owner_ptcb=CURPROC->ptcb[i];
		wakeup(tcb);


	return  (Tid_t)tcb;
	}

	return NOTHREAD;

}

/**
  @brief Join the given thread.
 */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{


	TCB* tmp_tcb=(TCB *)tid;
	if(tmp_tcb==NULL) return -1;

	int i = find(tid);

	  if(i==-1 || CURPROC->ptcb[i]->is_detach==DETACH || (Tid_t)CURTHREAD== tid) {

		  return -1;
	  }


	  CURPROC->ptcb[i]->ref_count++;

		  while (CURPROC->ptcb[i]->state!=EXITED && CURPROC->ptcb[i]->is_detach!=DETACH ) {
		  kernel_wait(&CURPROC->ptcb[i]->ptcb_cv,SCHED_USER);


		  }

		  if(CURPROC->ptcb[i]->is_detach==DETACH) {

			  return -1;
		  }

		  if(exitval!=NULL)
		  *exitval=CURPROC->ptcb[i]->exitval;

		  return 0;




}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{


	TCB* tcb=(TCB*)tid;
	if(tcb==NULL) return -1;


	 int i =find(tid);
	if(i!=-1 || CURPROC->ptcb[i]->state!=EXITED){
	CURPROC->ptcb[i]->is_detach=DETACH;
	kernel_broadcast(&CURPROC->ptcb[i]->ptcb_cv);

	return 0;
	}

		return -1;
}

Tid_t sys_ThreadSelf()
{
	return (Tid_t) CURTHREAD;
}

/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{

int i= find((Tid_t)CURTHREAD);
	CURPROC->ptcbcount--;
	CURPROC->ptcb[i]->exitval=exitval;

	kernel_broadcast(&CURPROC->ptcb[i]->ptcb_cv);
	CURPROC->ptcb[i]->state=EXITED;
	 kernel_unlock();

	sleep_releasing(EXITED, &kernel_mutex,SCHED_USER,0);
	kernel_lock();

}

