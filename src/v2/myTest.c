// myTest
#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char*argv[]){
    int frames[10];
    int pids[10];
    int numframes = 5;

    int i = dump_physmem(frames,pids,numframes);

    for(int j = 0 ; j < numframes; j++){
        printf(1,"frames[%d] = %d   ",j,frames[j]);
        printf(1,"pid[%d] = %d \n",j,pids[j]);
    }

    printf(1,"return value of dumpmem: %d \n",i);

    exit();
}
