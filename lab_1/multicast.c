#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>


#define PORT 12345
#define LEN_IP 100
#define BUFF_SIZE 256


typedef struct  {
    int is_ipv6;
    char *multicast_ip;
    int socket_fd;
    struct sockaddr_in *multicast_addr;
    struct sockaddr_in6 *multicast_addr6;
} thread_args;



void join_multicast_group(int sockfd, const char *multicast_addr, int is_ipv6) {
    if (is_ipv6) {
        struct sockaddr_in6 group_addr;
        struct ipv6_mreq mreq;

        memset(&group_addr, 0, sizeof(group_addr));
        group_addr.sin6_family = AF_INET6;
        inet_pton(AF_INET6, multicast_addr, &group_addr.sin6_addr);
        
        mreq.ipv6mr_multiaddr = group_addr.sin6_addr;
        mreq.ipv6mr_interface = 0;

        // add to the group
        if (setsockopt(sockfd, IPPROTO_IPV6, IPV6_JOIN_GROUP, (void*)&mreq, sizeof(mreq)) < 0) {
            perror("setsockopt IPV6_JOIN_GROUP");
            exit(1);
        }
    } else {
        struct sockaddr_in group_addr;
        struct ip_mreq mreq;

        memset(&group_addr, 0, sizeof(group_addr));
        group_addr.sin_family = AF_INET;
        inet_pton(AF_INET, multicast_addr, &group_addr.sin_addr);
        
        mreq.imr_multiaddr = group_addr.sin_addr;
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);

        // add to the group
        if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*)&mreq, sizeof(mreq)) < 0) {
            perror("setsockopt IP_ADD_MEMBERSHIP");
            exit(1);
        }
    }
}


void *send_messages(void *_args) {
    char buffer[BUFF_SIZE];
    int error_check;

    thread_args *args = (thread_args *) _args;
    printf("args->multicast_ip: %s\n", args->multicast_ip);
    char *multicast_ip = args->multicast_ip;

    socklen_t len = args->is_ipv6 ? sizeof(*(args->multicast_addr6)) : sizeof(*(args->multicast_addr));
    struct sockaddr *to = args->is_ipv6 ? (struct sockaddr *) (args->multicast_addr6) : (struct sockaddr *) (args->multicast_addr);
    

    while (1) {
        memset(buffer, 0, BUFF_SIZE);

        int my_pid = getpid();
        int num_of_el = 0;
        for (int i = 0; my_pid != 0; ++i) {
            num_of_el++;
            my_pid /= 10;
        }

        my_pid = getpid();
        printf("line: %d my_pid: %d\n", __LINE__, my_pid);
        for (int i = num_of_el - 1; i >= 0; i--) {
            buffer[i] = my_pid % 10 + '0';
            my_pid /=10;
        }
        buffer[num_of_el] = '\0';

        error_check = sendto(args->socket_fd, buffer, strlen(buffer) + 1, 0, to, len) ;
        if (error_check == -1) {
            printf("read error: %s\n", strerror(errno));
            close(args->socket_fd);
            exit(1);
        } else {
            printf("buffer was written to sender_sockaddr line:%d\n\t buffer:%swas sent bytes: %d\n", __LINE__, buffer, error_check);

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

    socklen_t len = args->is_ipv6 ? sizeof(*(args->multicast_addr6)) : sizeof(*(args->multicast_addr));
    struct sockaddr *from = args->is_ipv6 ? (struct sockaddr *) (args->multicast_addr6) : (struct sockaddr *) (args->multicast_addr);

    while (1) {
        memset(buffer, 0, BUFF_SIZE);
        printf("ready to hear messages\n");
        
        if (recvfrom(args->socket_fd, buffer, BUFF_SIZE, 0, from, &len) == -1) {
            printf("fuck %d\n", __LINE__);
            perror("recvfrom() failed");
            close(args->socket_fd);
            exit(1);
        }



        printf("received message: %s\n", buffer);

        int got_pid = 0;
        for (int i = 0; i < BUFF_SIZE && buffer[i] != '\0'; ++i) {
            got_pid *= 10;
            got_pid += buffer[i];
        }
        printf("got pid: %d\n", got_pid);


        if (got_pid == getpid()) {
            printf("sucess!!!\n");
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

    // Пif ipv4 is multicast
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

    thread_args *args = malloc (sizeof (thread_args));
    args->multicast_ip = &(multicast_ip[0]);
    args->socket_fd = sckt;
    args->is_ipv6 = is_ipv6;



    if (is_ipv6) {
        args->multicast_addr6 = (struct sockaddr_in6 *) malloc(sizeof(struct sockaddr_in6));
        args->multicast_addr = NULL;
        memset(args->multicast_addr6, 0, sizeof(struct sockaddr_in6));
        args->multicast_addr6->sin6_family = AF_INET6;
        args->multicast_addr6->sin6_port = htons(PORT);
        args->multicast_addr6->sin6_addr = in6addr_any;

        bind(sckt, (struct sockaddr*)args->multicast_addr6, sizeof(args->multicast_addr6));
        join_multicast_group(sckt, &(multicast_ip[0]), 1);
    } else {
        args->multicast_addr = (struct sockaddr_in *) malloc(sizeof(struct sockaddr_in));
        args->multicast_addr6 = NULL;

        memset((args->multicast_addr), 0, sizeof(struct sockaddr));
        args->multicast_addr->sin_family = AF_INET;
        (*(args->multicast_addr)).sin_port = htons(PORT);
        args->multicast_addr->sin_addr.s_addr = htonl(INADDR_ANY);


        bind(sckt, (struct sockaddr*)args->multicast_addr, sizeof(args->multicast_addr));
        join_multicast_group(sckt, &(multicast_ip[0]), 0);
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
