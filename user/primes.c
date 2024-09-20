#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void primes(int pipe_parent[2]) __attribute__((noreturn));
void primes(int pipe_parent[2])
{
    int pipe_child[2];
    int prime;
    int n;
    close(pipe_parent[1]);
    //close(pipe_child[0]); // 事实上这时候还没被创建！！！
    if (read(pipe_parent[0],&prime,sizeof(int)) ==sizeof(int)){
        printf("prime %d\n",prime);
        if (pipe(pipe_child) < 0){
            fprintf(2,"pipe failed\n");
            exit(1);
        }
        if (fork() == 0){
            close(pipe_parent[0]);//需要特别注意，如果这里不关闭，那么父进程会一直阻塞在read
            primes(pipe_child);
            exit(0);
        }
        else{
            close(pipe_child[0]);
            while (read(pipe_parent[0], &n, sizeof(int)) == sizeof(int))
            {
                if (n % prime != 0){
                    write(pipe_child[1],&n,sizeof(int));
                }
            }
            close(pipe_child[1]);
        }
    }
    close(pipe_parent[0]);
    //printf("pipe %d closed\n",pipe_parent[0]);
    wait(0);
    exit(0);
}
int main(int argc, char *argv[])
{
    int pipe_parent[2];
    if (pipe(pipe_parent) < 0){
        fprintf(2,"pipe failed\n");
        exit(1);
    }
    if (fork() == 0){
        primes(pipe_parent);
    }else{
        close(pipe_parent[0]);
        for (int i = 2; i <= 280; i++){
            write(pipe_parent[1],&i,sizeof(int));
        }
        close(pipe_parent[1]);
        wait(0);
    }
    exit(0);
}