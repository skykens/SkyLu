#include "daemon.h"
#include <errno.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>


namespace skylu{
    void  Daemon::start() {
        int pid = fork();
        if(pid < 0){
            fprintf(stderr,"fork error : %d , errno = %d, strerror = %s\n",pid, errno,strerror(errno));
            exit(1);
        }

        if(pid > 0)
            exit(0);


        if(setsid()<0){
            fprintf(stderr,"setsid errno = %d, strerror: %s\n",errno,strerror(errno));
            exit(1);
        }


        chdir("/");

        for(int i=0;i<3;i++){
            close(i);
        }
        umask(0);







    }
}