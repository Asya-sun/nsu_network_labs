#define _GNU_SOURCE     /* To get defns of NI_MAXSERV and NI_MAXHOST */
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
#include <time.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <linux/if_link.h>


#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>


#define PORT 12345
#define LEN_IP 100
#define BUFF_SIZE 256

#define MY_IP_LEN NI_MAXHOST

char my_ip[MY_IP_LEN];


//1 порт для получения
//разные порты для отправки

typedef struct  {
    int is_ipv6;
    char *multicast_ip;
    int sender_socket;
    struct sockaddr_in *multicast_addr;
    struct sockaddr_in6 *multicast_addr6;
} thread_args;


void get_my_local_ip(char *host, int is_ipv6, int maxlen_host) {
    struct ifaddrs *ifaddr;
    int family, s;

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

    /* Walk through linked list, maintaining head pointer so we
        can free list later. */

    for (struct ifaddrs *ifa = ifaddr; ifa != NULL;
            ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;

        family = ifa->ifa_addr->sa_family;

        /* For an AF_INET* interface address, display the address. */

        if ((family == AF_INET && !is_ipv6)|| (family == AF_INET6 && is_ipv6)) {
            s = getnameinfo(ifa->ifa_addr,
                    (family == AF_INET) ? sizeof(struct sockaddr_in) :
                                            sizeof(struct sockaddr_in6),
                    host, /*NI_MAXHOST,*/ maxlen_host,
                    NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                printf("getnameinfo() failed: %s\n", gai_strerror(s));
                exit(EXIT_FAILURE);
            }

            printf("\t\taddress: <%s>\n", host);

        }
    }

    freeifaddrs(ifaddr);
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

        printf("buffer: %s\n", buffer);

        error_check = sendto(args->sender_socket, buffer, strlen(buffer) + 1, 0, (struct sockaddr *) to, len) ;
        if (error_check == -1) {
            printf("read error: %s\n", strerror(errno));
            close(args->sender_socket);
            exit(1);
        } else {
            printf("sent message: %s\n", buffer);
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
        // printf("ready to hear messages\n");

        int nbytes = recvfrom(args->sender_socket, buffer, BUFF_SIZE, 0, (struct sockaddr *) &from, &len);
        if ( nbytes == -1) {
            perror("recvfrom() failed");
            close(args->sender_socket);
            exit(1);
        }
        
        if (! args->is_ipv6) { 

            char sender_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &from.sin_addr, sender_ip, sizeof(sender_ip));

            printf("received message: %s\n", buffer);
            printf("Sender IPv4: %s:%d\n", sender_ip, ntohs(from.sin_port));
        } else {
            char sender_ip[INET6_ADDRSTRLEN];
            memset(sender_ip, 0, INET6_ADDRSTRLEN);
            inet_ntop(AF_INET6, &from.sin_addr, sender_ip, sizeof(sender_ip));

            printf("received message: %s\n", buffer);
            printf("Sender IPv6: %s:%d\n", sender_ip, ntohs(from.sin_port));


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


    get_my_local_ip(my_ip, is_ipv6, MY_IP_LEN);
    printf("my_ip: %s\n", my_ip);
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
        inet_pton(AF_INET6, my_ip, &local_addr.sin6_addr);

        if (bind(sckt, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
            perror("bind");
            exit(1);
        }


        struct ipv6_mreq mreq;
        
        mreq.ipv6mr_multiaddr = args->multicast_addr6->sin6_addr;
        inet_pton(AF_INET6, &(multicast_ip[0]), &mreq.ipv6mr_multiaddr);
        inet_pton(AF_INET6, my_ip, &mreq.ipv6mr_interface);

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

        args->multicast_addr->sin_addr.s_addr = inet_addr(&(multicast_ip[0]));
        bzero(&((*(args->multicast_addr)).sin_zero),8);

        // Привязка сокета к любому адресу
        struct sockaddr_in local_addr;
        memset(&local_addr, 0, sizeof(local_addr));
        local_addr.sin_family = AF_INET;
        inet_pton(AF_INET6, my_ip,  &local_addr.sin_addr);

        local_addr.sin_port = htons(PORT);

        if (bind(sckt, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
            perror("bind");
            exit(1);
        }

        // Установка опции для присоединения к мульткаст-группе
        // Для IPv4
        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = inet_addr(&(multicast_ip[0]));
        inet_pton(AF_INET, my_ip, &mreq.imr_interface.s_addr);
        
        if (setsockopt(sckt, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq)) < 0) {
            perror("setsockopt");
            exit(1);
        }
    }

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
