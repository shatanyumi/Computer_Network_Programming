#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc,char ** argv) {
    int n = atoi(argv[3]);
    pid_t pid;
    int i;
    for(i=n-1;i>0;i--){
        pid = fork();
        if(pid == 0|| pid == -1) break;
    }

    if(pid == -1){
        printf("fork error");
    }
    if(pid == 0){
        printf("Child process %d %d\n",getpid(),i);
    } else{
        printf("Parent process %d %d\n",getpid(),i);
    }
    return 0;
}
