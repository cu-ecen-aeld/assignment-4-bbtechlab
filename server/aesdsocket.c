#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h> 
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <pthread.h>

#define MAXDATASIZE     65536
#define FILE_NAME       "/var/tmp/aesdsocketdata"

// Global variables
int             gSockfd, gFd; 
pthread_mutex_t gMutex;

struct thread_data{
     pthread_t  threadId;
     int        sockfd;
     char       s[INET6_ADDRSTRLEN];
     bool       thread_complete_success;
};

void* threadfunc(void* thread_param)
{
    struct  thread_data* pData   = (struct thread_data *) thread_param;
    char    *buf                 = malloc(sizeof(char)*MAXDATASIZE);
    int     numbytes             =0;
    ssize_t byteWrite;

    do
    {
        int byteRead;
        if ((byteRead = recv(pData->sockfd, (buf + numbytes), MAXDATASIZE-1, 0)) == -1)
        {
            syslog(LOG_DEBUG,"%s:: recv error, numbytes:%d",__FILE__,numbytes);
            close(pData->sockfd);
            pData->thread_complete_success = true;
            free(buf);
            return thread_param;
        }
        numbytes += byteRead;
    } while(*(buf + numbytes-1) != '\n');

    pthread_mutex_lock(&gMutex);

    gFd = open (FILE_NAME, O_RDWR | O_CREAT | O_APPEND,
               S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP | S_IROTH);
    if (gFd == -1)
    {
        syslog(LOG_DEBUG,"%s:: Failed to open file:%s", __FILE__, FILE_NAME);
        goto exit;
    }

    byteWrite = write (gFd, buf, numbytes);
    if (byteWrite == -1)
    {
        syslog(LOG_DEBUG,"%s:: Failed to write file:%s", __FILE__, FILE_NAME);
        goto exit;
    }

    numbytes = lseek(gFd,0,SEEK_END);
    lseek(gFd,0,SEEK_SET);

    if (read(gFd, buf, numbytes) == -1)
    {
        syslog(LOG_DEBUG,"%s:: read error in %s with error %d",__FILE__, FILE_NAME,errno);
        goto exit;
    }
    
    if (send(pData->sockfd, buf, numbytes, 0) == -1)
    {
        syslog(LOG_DEBUG,"%s:: Failed to send",__FILE__);
        goto exit;
    }

    pthread_mutex_unlock(&gMutex);

exit:
    close(gFd);
    close(pData->sockfd);
    free(buf);
    pData->thread_complete_success = true;
    return thread_param;
}

void timer_signal_handler(int signum)
{
    if ( signum == SIGALRM )
    {
        syslog(LOG_DEBUG,"%s::SIGALRM caught", __FILE__);
    }
}

void* timer_threadfunc(void* thread_param)
{
    char timestamp[256];
    time_t t;
    struct tm *_tm;

    while(true)
    {
        sleep(10);
        t = time(NULL);
        _tm = localtime(&t);

        strftime(timestamp, sizeof(timestamp), "timestamp:%a, %d %b %Y %T %z\n", _tm);

        pthread_mutex_lock(&gMutex);

        gFd = open (FILE_NAME, O_RDWR | O_CREAT | O_APPEND,
                   S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP | S_IROTH);
        if (gFd == -1)
        {
            syslog(LOG_DEBUG,"%s:: file not open", FILE_NAME);
            continue;
        }

        if(write (gFd, timestamp, strlen(timestamp)) == -1)
        {
            syslog(LOG_DEBUG,"%s:: Unable to write to file", FILE_NAME);
            close(gFd);
            continue;
        }
        
        if (close (gFd) == -1)
        {
            syslog(LOG_DEBUG,"%s:: file not closed", FILE_NAME);
        }

        pthread_mutex_unlock(&gMutex);

    }

    return NULL;
}

void *get_sockaddr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void signal_handler(int signum)
{
    syslog(LOG_DEBUG,"Caught signal:%d", signum);

    pthread_mutex_destroy(&gMutex);

    if (shutdown (gSockfd, SHUT_RDWR) == -1)
    {
        syslog(LOG_DEBUG,"%s::failed to close socket",__FILE__);
    }

    if (close (gFd) == -1)
    {
        syslog(LOG_DEBUG,"%s::Failed to close file:%s",__FILE__, FILE_NAME);
    }

    if (remove(FILE_NAME) == 0)
        syslog(LOG_DEBUG,"%s::%s Deleted successfully",__FILE__, FILE_NAME);

    exit(0);
}

int main(int argc, char *argv[])
{
    int sockOpt=1;
    struct sockaddr_storage storageAddr;
    struct addrinfo hints, *res;
    char s[INET6_ADDRSTRLEN];
    struct sigaction sigterm, sigint;
    pthread_t timer_thread;
    socklen_t sockLen = sizeof(storageAddr);
    int sockfd, threadId; 
    struct thread_data *pThrData;
  
    memset(&sigint, 0, sizeof(sigint));
    sigint.sa_handler = signal_handler;
    sigint.sa_flags = 0;
    sigfillset(&sigint.sa_mask);
    sigaction(SIGINT, &sigint, NULL);
  
    memset(&sigterm, 0, sizeof(sigterm));
    sigterm.sa_handler = signal_handler;
    sigaction(SIGTERM, &sigterm, NULL);
  
    if (pthread_mutex_init(&gMutex, NULL) != 0)
    {
        return -1;
    }
    // Fix me
    if (argc == 2 && (strcmp(argv[1], "-d") == 0)) 
    {
        pid_t pid;
        pid = fork();
        if (pid == -1)
            return -1;
        else if (pid != 0)
            exit(EXIT_SUCCESS); // parent process will exit
        // child process
        if (setsid() == -1)
            return -1;
        if (chdir ("/") == -1)
            return -1;
        close (0);
        close (1);
        close (2);

        int fdNull = open("/dev/null", O_RDWR, 0666);
        if(fdNull<0)
        {
            printf("open() /dev/null failed\n");
            exit(1);
        }
        printf("%s::%d\n",__FILE__,__LINE__);
        dup(0);
        dup(0);
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    getaddrinfo(NULL, "9000", &hints, &res);

    gSockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  
    if (setsockopt(gSockfd, SOL_SOCKET, SO_REUSEADDR, &sockOpt, sizeof(int)) == -1)
    {
        syslog(LOG_DEBUG,"%s:: setsockopt error",__FILE__);
        return -1;
    }

    if(bind(gSockfd, res->ai_addr, res->ai_addrlen) == -1)
    {
        syslog(LOG_DEBUG,"%s:: bind error",__FILE__);
        close(gSockfd);
        return -1;
    }

    if (listen(gSockfd, 10) == -1)
    {
        syslog(LOG_DEBUG,"%s:: listen error",__FILE__);
        close(gSockfd);
        return -1;
    }

    freeaddrinfo(res); //all done with this structure

    pthread_create(&timer_thread, NULL, &timer_threadfunc, "Timer thread");

    while(true)
    {
        pThrData = malloc(sizeof(struct thread_data));
        if (pThrData == NULL)
        {
            syslog(LOG_DEBUG,"%s:: pThrData error %d",__FILE__, errno);
            return -1;
        }

        sockfd = accept(gSockfd, (struct sockaddr *)&storageAddr, &sockLen);
        if (sockfd == -1)
        {
          syslog(LOG_DEBUG,"%s:: accept error %d",__FILE__, errno);
          close(gSockfd);
          return -1;
        }

        inet_ntop(storageAddr.ss_family,get_sockaddr((struct sockaddr *)&storageAddr),s, sizeof(s));

        syslog(LOG_DEBUG,"Accepted connection from %s", s);

        pThrData->sockfd = sockfd;
        pThrData->thread_complete_success = false;
        strcpy(pThrData->s, s);

        threadId = pthread_create(&(pThrData->threadId), NULL, &threadfunc, pThrData);

        pThrData->thread_complete_success = (threadId==0?false:true);
        pthread_join(pThrData->threadId, NULL);
        free(pThrData);
    }

    return 0;
}

