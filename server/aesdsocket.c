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

typedef enum server_state
{
    SOCKET_CREATE = 0,
    SOCKET_BIND = 1,
    LISTEN = 2,
    ACCEPT = 3,
    RECEIVE = 4,
    SEND = 5,
} server_state_t;

#define AESD_PORT "9000"
#define DEBUG_MSG(X, Y)    printf("[DEBUG] %s %s \n", X, Y)


bool deamon_flag = false;
char rx_buff[1024];
struct addrinfo *address;
char *packet, *tx_buff = NULL;
uint16_t len = 0u;
int socketfd ,fd , acceptfd;
static server_state_t g_state = SOCKET_CREATE;

static void gracefull_exit(int signo);

int main(int argc, char *argv[])
{


    int opt, bind_ret = 0;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    char ipstr[INET6_ADDRSTRLEN];


    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);
    (void)signal(SIGINT, gracefull_exit);
    (void)signal(SIGTERM, gracefull_exit);

    while ((opt = getopt(argc, argv, "d")) != -1)
    {
        switch (opt)
        {
            case 'd':
                deamon_flag = true;
                DEBUG_MSG("Deamon mode active","");
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
                    DEBUG_MSG("Socket created successfully","");
                }

                int getaddr_ret = getaddrinfo(NULL, AESD_PORT, &hints, &address);
                if (getaddr_ret == -1)
                {
                    perror("getaddrinfo");
                    return -1;
                }
                else
                {
                    DEBUG_MSG("Socket opened successfully","");
                }
                g_state = SOCKET_BIND;

                break;
            
            case SOCKET_BIND:
                bind_ret = bind(socketfd, address->ai_addr, sizeof(struct sockaddr));
                if (bind_ret == -1)
                {
                    perror("bind");
                    if(errno == 98)
                    {
                        printf("Closing open socket file descriptor");
                        close(socketfd);
                        break;
                    }
                    else{
                        return -1;
                    }
                }
                else
                {
                    DEBUG_MSG("Socket bound successfully","");
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
                    DEBUG_MSG("Listening","");
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
                    if (((struct sockaddr *)&address)->sa_family == AF_INET) {
                        in_addr = &(((struct sockaddr_in *)&address)->sin_addr);
                    } else {
                        in_addr = &(((struct sockaddr_in6 *)&address)->sin6_addr);
                    }
                    inet_ntop(address->ai_family, in_addr, ipstr, sizeof ipstr);
                    printf("Accepting connection to %s port %s\n", ipstr , AESD_PORT);
                    syslog(LOG_INFO, "Accepted connection from %s", ipstr);
                }

                fd = open("/var/tmp/aesdsocketdata",O_RDWR | O_CREAT | O_APPEND, 0644);
                packet = NULL;
                len = 0u;

                g_state = RECEIVE;
                break;

            case RECEIVE:

                int rx_ret = recv(acceptfd, rx_buff, sizeof(rx_buff), 0);

                if(rx_ret == -1)
                {
                    /*Rx error*/
                    perror("Rx");
                    return 0;
                }
                else if (rx_ret == 0)
                {
                    break;
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
                        int wr_ret = write(fd , packet, len);
                        if(wr_ret == -1)
                        {
                            return -1;
                        }
                        g_state = SEND;
                    }
                }
                /*free dynamically allocated packet*/
                // free(packet);
                // len = 0;
                // packet = NULL;
                break;
            
            case SEND:
                /*Send Messge*/
                /*Read file into a new buffer*/
                struct stat st;
                if (fstat(fd, &st) == -1) {
                    perror("fstat");
                    close(fd);
                    return -1;
                }

                size_t size = st.st_size;
                tx_buff = malloc(size);
                fd = open("/var/tmp/aesdsocketdata",O_RDWR | O_CREAT | O_APPEND, 0644);
                int read_ret = read(fd , tx_buff , size);

                if(read_ret == 0)
                {
                    /*end of file reached*/
                    DEBUG_MSG("End of file reached","");
                    g_state = ACCEPT;
                    break;
                }
                else if (read_ret == -1)
                {
                    perror("read");
                    close(fd);
                    return -1;
                }
                else
                {
                    /*Send buffer*/
                    DEBUG_MSG("TX: ", tx_buff);
                    for(int i = 0; i < read_ret ; i++)
                    {
                        if((send(acceptfd, &tx_buff[i], sizeof(tx_buff[0]) , 0)) == -1)
                        {
                            perror("Send");
                            close(acceptfd);
                            return -1;
                        }
                    }
                    g_state = ACCEPT;
                    /*Connection Closed*/
                    syslog(LOG_INFO, "Closed connection from %s", ipstr);
                    free(tx_buff);
                }
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
    free(packet);
    freeaddrinfo(address);
    printf("\ngracefull exit in progress, signo: %d\n", signo);
    syslog(LOG_INFO, "Caught signal, exiting");
    unlink("/var/tmp/aesdsocketdata");
    closelog();
    exit(EXIT_SUCCESS);
}