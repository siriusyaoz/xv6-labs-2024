#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0; // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  backtrace();
  int n;
  uint ticks0;

  argint(0, &n);
  if (n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (killed(myproc()))
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64 sys_sigalarm(void)
{
  int ticks;
  uint64 handler;

  struct proc *p = myproc();
  argint(0, &ticks);
  argaddr(1, &handler);
  if (ticks < 0)
  {
    return -1;
  }
  if (ticks == 0)
  {
    // 取消定时器
    p->alarm_interval = 0;
    p->alarm_handler = 0;
    p->alarm_ticks = 0;
  }
  else
  {
    p->alarm_interval = ticks;
    p->alarm_handler = (void (*)(void))handler;
    p->alarm_ticks = ticks;
  }
  return 0;
}

uint64 sys_sigreturn(void)
{
  struct proc *p = myproc();
  if (!p->in_handler)
    return -1;
  // uint64 kernel_satp = p->trapframe->kernel_satp;
  // uint64 kernel_sp = p->trapframe->kernel_sp;
  // uint64 kernel_trap = p->trapframe->kernel_trap;
  // uint64 kernel_hartid = p->trapframe->kernel_hartid;
  memmove(p->trapframe, &p->sig_trapframe, sizeof(struct trapframe));
  // p->trapframe->kernel_satp = kernel_satp;
  // p->trapframe->kernel_sp = kernel_sp;
  // p->trapframe->kernel_trap = kernel_trap;
  // p->trapframe->kernel_hartid = kernel_hartid;
  p->in_handler = 0;
  //printf("\nvalue of ptrapframe a0 is %lx,    psigtrapframe a0 is %lx\n", p->trapframe->a0, p->sig_trapframe.a0);
  return p->trapframe->a0;
}