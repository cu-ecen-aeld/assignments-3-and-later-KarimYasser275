#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <syslog.h>
#include <fcntl.h>
#include <string.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/stat.h>
#include <pthread.h>
typedef enum server_state
{
    SOCKET_CREATE = 0,
    SOCKET_BIND = 1,
    LISTEN = 2,
    ACCEPT = 3,
    CHECK = 4
} server_state_t;

typedef enum substate_e
{
    RECEIVE,
    SEND,
    DONE
}substate_t;

typedef struct Node_s
{
    pthread_t t;
    int fd;
    int acceptfd;
    bool thread_completed;
    substate_t state;
    struct Node_s *next;
}Node_t;

Node_t* head = NULL;
pthread_mutex_t thread_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ll_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t time_stamp;
static void* thread_ReceiveSend(void* arg);
static void* thread_timeStamp(void* arg);

#define AESD_PORT "9000"
#define DEBUG_MSG(X, Y)    printf("[DEBUG] %s %s \n", X, Y)


bool deamon_flag = false;
char rx_buff[1024];
struct addrinfo *address;
// uint16_t len = 0u;
int socketfd ,fd , acceptfd;
static server_state_t g_state = SOCKET_CREATE;
char ipstr[INET6_ADDRSTRLEN];
static void gracefull_exit(int signo);

int main(int argc, char *argv[])
{


    int opt, bind_ret = 0;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;


    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);
    (void)signal(SIGINT, gracefull_exit);
    (void)signal(SIGTERM, gracefull_exit);

    while ((opt = getopt(argc, argv, "d")) != -1)
    {
        switch (opt)
        {
            case 'd':
                deamon_flag = true;
                // DEBUG_MSG("Deamon mode active","");
                break;
            default:
                deamon_flag = false;
                break;
        }
    }
    
    if(deamon_flag == true)
    {
        if(daemon(0,0) == -1)
        {
            perror("daemon");
            return -1;
        }
    }
    pthread_create(&time_stamp , NULL, thread_timeStamp, NULL);

    while(1)
    {
        switch (g_state)
        {
            case SOCKET_CREATE:
                /*Create socket file descriptor*/
                socketfd = socket(PF_INET, SOCK_STREAM, 0);
                if (socketfd == -1)
                {
                    perror("Socket");
                    return -1;
                }
                else
                {
                    // DEBUG_MSG("Socket created successfully","");
                }
                int yes = 1;
                setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
                int getaddr_ret = getaddrinfo(NULL, AESD_PORT, &hints, &address);
                if (getaddr_ret == -1)
                {
                    perror("getaddrinfo");
                    return -1;
                }
                else
                {
                    // DEBUG_MSG("Socket opened successfully","");
                }
                g_state = SOCKET_BIND;

                break;
            
            case SOCKET_BIND:
                bind_ret = bind(socketfd, address->ai_addr, sizeof(struct sockaddr));
                if (bind_ret == -1)
                {
                    perror("bind");
                }
                else
                {
                    // DEBUG_MSG("Socket bound successfully","");
                }
                g_state = LISTEN;
                break;
            
            case LISTEN:
                /*Listen for connections*/
                int listen_ret = listen(socketfd, 10);
                if(listen_ret == -1)
                {
                    perror("Listen");
                    return -1;
                }
                else{
                    // DEBUG_MSG("Listening","");
                }

                g_state = ACCEPT;
                break;

            case ACCEPT:
                /*Accept connections*/

                acceptfd = accept(socketfd, address->ai_addr, &(address->ai_addrlen));
                if(acceptfd == -1)
                {
                    perror("Accept");
                    return -1;
                }
                else{
                    void *in_addr;
                    if (((struct sockaddr *)address->ai_addr)->sa_family == AF_INET) {
                        in_addr = &(((struct sockaddr_in *)address->ai_addr)->sin_addr);
                    } else {
                        in_addr = &(((struct sockaddr_in6 *)address->ai_addr)->sin6_addr);
                    }
                    int family = ((struct sockaddr *)address->ai_addr)->sa_family;
                    inet_ntop(family, in_addr, ipstr, sizeof ipstr);
                    printf("Accepting connection to %s port %s\n", ipstr , AESD_PORT);
                    syslog(LOG_INFO, "Accepted connection from %s", ipstr);
                }

                fd = open("/var/tmp/aesdsocketdata",O_RDWR | O_CREAT | O_APPEND, 0644);
                // packet = NULL;
                // len = 0u;

                //create a thread for the new accepted connection
                Node_t* new_node = malloc(sizeof(Node_t));
                new_node->next = head;
                new_node->thread_completed = false;
                head = new_node;
                head->t;
                head->acceptfd = acceptfd;
                head->fd = fd;
                head->state = RECEIVE;
                pthread_create(&head->t , NULL, thread_ReceiveSend, head);
                g_state = CHECK;
                break;

            case CHECK:
                if(!head) break;
                pthread_mutex_lock(&ll_mutex);
                Node_t *it = head->next, *prev = head;
                
                if(prev->thread_completed == true)
                {
                    pthread_join(prev->t , NULL);
                    head=prev->next;
                    free(prev);
                    break;
                }
                pthread_mutex_unlock(&ll_mutex);
                
                while(it != NULL)
                {
                    if(it->thread_completed == true)
                    {
                        //join thread
                        pthread_join(it->t , NULL);
                        //remove from linked list
                        prev->next = it->next;
                        free(it);
                    }
                    prev = prev->next;
                    if(prev == NULL) break;
                    it = prev->next;//seg fault 7aseeeeeb!!!!!!
                }
                g_state = LISTEN;
                pthread_mutex_unlock(&ll_mutex);
                break;

            default:
                gracefull_exit(0);
                break;
        }

    }

    return 0;
}


static void gracefull_exit(int signo)
{
    close(socketfd);
    close(fd);
    close(acceptfd);
    // free(packet);
    freeaddrinfo(address);
    printf("\ngracefull exit in progress, signo: %d\n", signo);
    syslog(LOG_INFO, "Caught signal, exiting");
    unlink("/var/tmp/aesdsocketdata");
    closelog();
    exit(EXIT_SUCCESS);
}

static void* thread_ReceiveSend(void* arg)
{
    Node_t* thread = (Node_t*) arg;
    char *packet = NULL;
    char *tx_buff = NULL;
    uint16_t len = 0u;

    while(1)
    {
        switch(thread->state)
        {
            case RECEIVE:
                
                int rx_ret = recv(thread->acceptfd, rx_buff, sizeof(rx_buff), 0);

                if(rx_ret == -1)
                {
                    /*Rx error*/
                    perror("Rx");
                    free(packet);
                    thread->thread_completed = true;
                    return NULL;
                }
                else if (rx_ret == 0)
                {
                    /*Connection closed*/
                    free(packet);
                    thread->thread_completed = true;
                    return NULL;
                }
                else
                {
                    /*Rx == number of bytes received*/
                    packet = realloc(packet, len + rx_ret);
                    (void*)memcpy(&packet[len], rx_buff, rx_ret);
                    len += rx_ret;
                    
                    /*Send buffer*/
                    if (memchr(packet, '\n', len) != NULL)
                    {
                        /*New line reached*/
                        /*Append received msg to /var/tmp/aesdsocketdata*/
                        (void)pthread_mutex_lock(&thread_mutex);
                        int wr_ret = write(thread->fd , packet, len);
                        (void)pthread_mutex_unlock(&thread_mutex);
                        if(wr_ret == -1)
                        {
                            free(packet);
                            thread->thread_completed = true;
                            return NULL;
                        }
                        thread->state = SEND;
                    }
                }
                break;
            /*free dynamically allocated packet*/
            // free(packet);
            // len = 0;
            // packet = NULL;
            case SEND:

                /*Send Messge*/
                /*Read file into a new buffer*/
                struct stat st;
                if (fstat(thread->fd, &st) == -1) {
                    perror("fstat");
                    close(thread->fd);
                    thread->thread_completed = true;
                    return NULL;
                }

                size_t size = st.st_size;
                tx_buff = malloc(size);
                if (tx_buff == NULL) {
                    perror("malloc");
                    close(thread->fd);
                    thread->thread_completed = true;
                    return NULL;
                }
                thread->fd = open("/var/tmp/aesdsocketdata",O_RDWR | O_CREAT | O_APPEND, 0644);
                int read_ret = read(thread->fd , tx_buff , size);

                if(read_ret == 0)
                {
                    /*end of file reached*/
                    // DEBUG_MSG("End of file reached","");
                    free(tx_buff);
                    thread->state = DONE;
                }
                else if (read_ret == -1)
                {
                    perror("read");
                    free(tx_buff);
                    close(thread->fd);
                    thread->thread_completed = true;
                    return NULL;
                }
                else
                {
                    /*Send buffer*/
                    // DEBUG_MSG("TX: ", tx_buff);
                    for(int i = 0; i < read_ret ; i++)
                    {
                        if((send(thread->acceptfd, &tx_buff[i], sizeof(tx_buff[0]) , 0)) == -1)
                        {
                            perror("Send");
                            free(tx_buff);
                            close(thread->acceptfd);
                            thread->thread_completed = true;
                            return NULL;
                        }
                    }
                    free(tx_buff);
                    tx_buff = NULL;
                    thread->state = DONE;
                    /*Connection Closed*/
                    (void)pthread_mutex_lock(&thread_mutex);
                    syslog(LOG_INFO, "Closed connection from %s", ipstr);
                    (void)pthread_mutex_unlock(&thread_mutex);

                    // free(tx_buff);
                }
                break;

            case DONE:
                thread->thread_completed = true;
                free(packet);
                close(thread->fd);
                close(thread->acceptfd);
                if (tx_buff != NULL) {
                    free(tx_buff);
                }
                return NULL;
        }
    }
    
}

static void* thread_timeStamp(void* arg)
{
    while (1)
    {
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char buffer[128];

        // Format RFC 2822 style
        strftime(buffer, sizeof(buffer), "timestamp:%a, %d %b %Y %H:%M:%S %z\n", tm_info);

        pthread_mutex_lock(&thread_mutex);  // lock shared file
        int fd = open("/var/tmp/aesdsocketdata", O_WRONLY | O_APPEND | O_CREAT, 0644);
        if(fd != -1) {
            write(fd, buffer, strlen(buffer));
            close(fd);
        }
        pthread_mutex_unlock(&thread_mutex); // unlock

        sleep(10);  // wait 10 seconds
    }
}
