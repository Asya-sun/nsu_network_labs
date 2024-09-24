#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <linux/in.h> 


#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>


#define PORT 12345
#define LEN_IP 100
#define BUFF_SIZE 256
#define MY_IPV4 "192.168.1.10"


//1 порт для получения
//разные порты для отправки

typedef struct  {
    int is_ipv6;
    char *multicast_ip;
    int sender_socket;
    struct sockaddr_in *multicast_addr;
    struct sockaddr_in6 *multicast_addr6;
} thread_args;



char *get_ip_str(const struct sockaddr *sa, char *s, size_t maxlen)
{
    switch(sa->sa_family) {
        case AF_INET:
            inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr),
                    s, maxlen);
            break;

        case AF_INET6:
            inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr),
                    s, maxlen);
            break;

        default:
            strncpy(s, "Unknown AF", maxlen);
            return NULL;
    }

    return s;
}


void *send_messages(void *_args) {
    char buffer[BUFF_SIZE];
    int error_check;
    char *ip_b = malloc(BUFF_SIZE);

    thread_args *args = (thread_args *) _args;

    socklen_t len = args->is_ipv6 ? sizeof(*(args->multicast_addr6)) : sizeof(*(args->multicast_addr));
    struct sockaddr *to = args->is_ipv6 ? (struct sockaddr *) (args->multicast_addr6) : (struct sockaddr *) (args->multicast_addr);
    

    while (1) {
        memset(buffer, 0, BUFF_SIZE);
        memset(ip_b, 0, BUFF_SIZE);

        int my_pid = getpid();
        sprintf(buffer, "%d", my_pid);

        // ip_b = get_ip_str(to, ip_b, BUFF_SIZE);
        // memcpy(buffer, ip_b, strlen(ip_b) + 1);
        printf("buffer: %s\n", buffer);

        error_check = sendto(args->sender_socket, buffer, strlen(buffer) + 1, 0, (struct sockaddr *) to, len) ;
        if (error_check == -1) {
            printf("read error: %s\n", strerror(errno));
            close(args->sender_socket);
            exit(1);
        } else {
            printf("sent message: %s\n", buffer);
            //printf("buffer was written to sender_sockaddr line:%d\n\t buffer: %s was sent bytes: %d\n", __LINE__, buffer, error_check);

        }


        sleep(1);
    }

    return 0;
}

void *receive_messages(void *_args) {
    thread_args *args = (thread_args *) _args;
    printf("args->multicast_ip: %s\n", args->multicast_ip);
    char *multicast_ip = args->multicast_ip;
    char buffer[BUFF_SIZE];

    char ip_buffer[BUFF_SIZE];
    memset(ip_buffer, 0, BUFF_SIZE);


    socklen_t len;
    struct sockaddr_in from;

    while (1) {
        len = 0;
        memset(&from, 0, sizeof(struct sockaddr));
        memset(buffer, 0, BUFF_SIZE);
        printf("ready to hear messages\n");


        int nbytes = recvfrom(args->sender_socket, buffer, BUFF_SIZE, 0, (struct sockaddr *) &from, &len);
        if ( nbytes == -1) {
            perror("recvfrom() failed");
            close(args->sender_socket);
            exit(1);
        }

        printf("received message: %s\n", buffer);
        
        if (! args->is_ipv6) { 

            char sender_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &from.sin_addr, sender_ip, sizeof(sender_ip));

            printf("Получено сообщение: %s\n", buffer);
            printf("Отправитель IPv4: %s:%d\n", sender_ip, ntohs(from.sin_port));
        } else {
            char sender_ip[INET6_ADDRSTRLEN];
            memset(sender_ip, 0, INET6_ADDRSTRLEN);
            inet_ntop(AF_INET6, &from.sin_addr, sender_ip, sizeof(sender_ip));

            printf("Получено сообщение: %s\n", buffer);
            printf("Отправитель IPv6: %s:%d\n", sender_ip, ntohs(from.sin_port));


        }
    }
    return 0;
}


/**
 * return 1 if multicast
 * otherwise return 0
 */
int validate_multicast(char *addr) {
    struct in_addr ipv4;
    struct in6_addr ipv6;

    // if ipv4 is multicast
    if (inet_pton(AF_INET, addr, &ipv4) == 1) {
        if ((ntohl(ipv4.s_addr) & 0xF0000000) == 0xE0000000) {
            return 1;
        }
    }
    // if ipv6 is multicast
    else if (inet_pton(AF_INET6, addr, &ipv6) == 1) {
        if (ipv6.s6_addr[0] == 0xFF) {
            return 1;
        }
    }

    // address is not multicast
    return 0;
}



int main(int argc, char *argv[ ]) {
    char multicast_ip[LEN_IP];
    memset(multicast_ip, 0, LEN_IP);
    if (argc < 2) {
        memcpy(multicast_ip, "224.0.0.251", 12);
    } else {
        int is_multicast = validate_multicast(argv[1]);
        printf("is_multicast: %d\n", is_multicast);
        if (is_multicast == 1) {
            memcpy(multicast_ip, argv[1], strlen(argv[1]) + 1);
        } else {
            memcpy(multicast_ip, "224.0.0.251", 12);
        }
    }
    printf("multicast addr: %s\n", multicast_ip);
    int is_ipv6 = (strchr(multicast_ip, ':') != NULL);
    printf("is_ipv6: %d\n", is_ipv6);
    //by this point multicast_ip is set

    int error_check;
    int sckt = socket(is_ipv6 ? AF_INET6 : AF_INET, SOCK_DGRAM, 0);
    if (sckt == -1) {
        perror("socket");
        exit(1);
    }
    const int optval = 1;
    setsockopt(sckt, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    thread_args *args = malloc (sizeof (thread_args));
    args->multicast_ip = &(multicast_ip[0]);
    args->sender_socket = sckt;
    args->is_ipv6 = is_ipv6;



    if (is_ipv6) {
        // Настройка адреса для отправки
        args->multicast_addr6 = (struct sockaddr_in6 *) malloc(sizeof(struct sockaddr_in6));
        args->multicast_addr = NULL;
        memset(args->multicast_addr6, 0, sizeof(struct sockaddr_in6));
        args->multicast_addr6->sin6_family = AF_INET6;
        args->multicast_addr6->sin6_port = htons(PORT);
        //args->multicast_addr6->sin6_scope_id
        inet_pton(AF_INET6, &(multicast_ip[0]), &((*(args->multicast_addr6)).sin6_addr));

        // Привязка сокета к любому адресу
        struct sockaddr_in6 local_addr;
        memset(&local_addr, 0, sizeof(local_addr));
        local_addr.sin6_family = AF_INET6;
        local_addr.sin6_port = htons(PORT);
        local_addr.sin6_addr = in6addr_any;

        if (bind(sckt, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
            perror("bind");
            exit(1);
        }


        struct ipv6_mreq mreq;
        
        mreq.ipv6mr_multiaddr = args->multicast_addr6->sin6_addr;
        inet_pton(AF_INET6, &(multicast_ip[0]), &mreq.ipv6mr_multiaddr);
        mreq.ipv6mr_interface = 0;

        // add to the group
        if (setsockopt(sckt, IPPROTO_IPV6, IPV6_JOIN_GROUP , (void*)&mreq, sizeof(mreq)) < 0) {
            perror("setsockopt IPV6_JOIN_GROUP");
            exit(1);
        }
    } else {

        // Настройка адреса для отправки
        args->multicast_addr = (struct sockaddr_in *) malloc(sizeof(struct sockaddr_in));
        args->multicast_addr6 = NULL;

        memset((args->multicast_addr), 0, sizeof(struct sockaddr_in));
        args->multicast_addr->sin_family = AF_INET;
        args->multicast_addr->sin_port = htons(PORT);

        //inet_pton(AF_INET, &(multicast_ip[0]), &((*(args->multicast_addr)).sin_addr));
        args->multicast_addr->sin_addr.s_addr = inet_addr(&(multicast_ip[0]));
        bzero(&((*(args->multicast_addr)).sin_zero),8);

        // Привязка сокета к любому адресу
        struct sockaddr_in local_addr;
        memset(&local_addr, 0, sizeof(local_addr));
        local_addr.sin_family = AF_INET;
        // local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        //I work in Linux so I bind to multicast address
        inet_pton(AF_INET, &(multicast_ip[0]), &local_addr.sin_addr);
        local_addr.sin_port = htons(PORT);

        if (bind(sckt, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
            perror("bind");
            exit(1);
        }

        // Установка опции для присоединения к мульткаст-группе
        // Для IPv4
        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = inet_addr(&(multicast_ip[0]));
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        // inet_pton(AF_INET, MY_IPV4, &mreq.imr_interface.s_addr);
        
        if (setsockopt(sckt, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq)) < 0) {
            perror("setsockopt");
            exit(1);
        }
    }

    int sender_socket; //= receiver socket
    char buffer[BUFF_SIZE];
    
    pthread_t sender;
    pthread_t receiver;
	int err;
    err = pthread_create(&receiver, NULL, receive_messages, args);
    if (err) {
        printf("receiver: pthread_create() failed: %s\n", strerror(err));
        return -1;
    }
    err = pthread_create(&sender, NULL, send_messages, args);
    if (err) {
        printf("sender: pthread_create() failed: %s\n", strerror(err));
        return -1;
    }
    sleep(100);
    err = pthread_cancel(sender);
    if (err) {
        printf("main: pthread_cancel() failed: %s\n", strerror(err));
    }
    err = pthread_cancel(receiver);
    if (err) {
        printf("main: pthread_cancel() failed: %s\n", strerror(err));
    }

    free(args);

	return 0;
}
