
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"


PTCB* init_PTCB(Task task, int argl, void* args, PCB* pcb)
{

    PTCB* ptcb = (PTCB*)malloc(sizeof(PTCB));

    
    rlnode_init(& ptcb->ptcb_list_node, ptcb); 

    rlist_push_back(& (pcb->PTCB_list), & ptcb->ptcb_list_node); //

     // PTCB INIT
    ptcb->tcb= NULL;
    ptcb->task = task;
    ptcb->owner_pcb = pcb;
    ptcb->exitval = 0; 
    ptcb->detached = 0; 
    ptcb->exit_cv = COND_INIT;
    ptcb->exited = 0;
    ptcb->argl = argl;
    ptcb->args = args;
    ptcb->refcount = 1;
    return  ptcb;
}





void start_thread()
{ 
  int exitval;

  Task call =  CURTHREAD->ptcb->task;
  int argl = CURTHREAD->ptcb->argl;
  void* args = CURTHREAD->ptcb->args;

  exitval = call(argl,args);

  ThreadExit(exitval);  
}

/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{
   // fprintf(stderr, "CREATE THREAD START\n" );
    TCB* tcb;
    PTCB* ptcb = init_PTCB(task, argl, args, CURPROC);

    if(task!=NULL)
    {

      tcb = spawn_thread(CURPROC, start_thread);
      ptcb->tcb=tcb; 
      ptcb->tcb->ptcb = ptcb;
      ptcb->refcount= 0;

    }
    else{
        return NOTHREAD; //on success epistrefei to thread id on fail epistrefei nothread
    }

    wakeup(ptcb->tcb);

    // fprintf(stderr, "CREATE THREAD END\n" );
    return (Tid_t) ptcb;
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
	return (Tid_t) CURTHREAD->ptcb;
}

/**
  @brief Join the given thread.
  */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{
	{
  

  PTCB* ptcb = (PTCB*)tid;


  if((ptcb->detached == 0) && (ptcb->owner_pcb == CURPROC) )
    {

      ptcb->refcount++;

    if(rlist_find(&CURPROC->PTCB_list,ptcb,NULL)==NULL){ 
      return -1;  
    }


    if(ptcb==CURTHREAD->ptcb){     
      return -1;
    }

    if(ptcb->detached==1){            
      return -1;
    }

    ptcb->refcount--;

    while( (ptcb->exited!=1) ){
      kernel_wait(& ptcb->exit_cv, SCHED_USER); // ???
    }

    if(exitval!=NULL){//Save exit value
      *exitval=ptcb->exitval; //
    }
    int ret;
    if( (ptcb->refcount<=0) && (ptcb->exited==1) ) 
    {
      release_PTCB(ptcb);

      ret = 0;
    }else if ( ptcb == NULL || ptcb == CURTHREAD ||  ptcb->detached == 1 )
    {
      ret = -1;
    }
    return ret;
  }

  return -1;
}
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{
	 PTCB* ptcb = (PTCB*)tid;

  if(rlist_find(&CURPROC->PTCB_list,ptcb,NULL)!=NULL){ 
      return -1;  
  }

  if(ptcb->exited==1){                              
    return -1;
  }

  ptcb->detached=1;
  ptcb->refcount= 0;
  kernel_broadcast(& ptcb->exit_cv); 
  return 0;
}

/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{
 
  CURTHREAD->ptcb->exitval = exitval;
  CURTHREAD->ptcb->exited = 1;
  kernel_broadcast(& CURTHREAD->ptcb->exit_cv);
  kernel_sleep(EXITED, SCHED_USER);

 
}

