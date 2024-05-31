#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <semaphore.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdlib.h>
#include <signal.h>
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <chrono>
using namespace std;

/* traffic statistics */
#define KEY 16789824
#define GID_NUM 100
#define CACHE_NUM 521
#define PORT 8080
#define MICROSECONDS_PER_SECOND 1000000
#define TIME (30 * MICROSECONDS_PER_SECOND)

typedef struct {
    char gid[16];
    uint64_t tx;
    uint64_t rx;
} Amount;

typedef struct {
	uint32_t qp_num;
	uint8_t path_mtu;
	char gid[16];
} Cache;

typedef struct {
	Amount amount[GID_NUM];
	Cache cache[CACHE_NUM];
} SHM;

SHM *shm;
int shm_size = sizeof(SHM);
int amount_size = sizeof(Amount) * GID_NUM;
int shmid;
string server_ip;

/* read message from sock_fd */
ssize_t sock_read(int sock_fd, void *buffer, size_t len)
{
    ssize_t nr, tot_read;
    char *buf = (char *)buffer; // avoid pointer arithmetic on void pointer
    tot_read = 0;

    while (len != 0 && (nr = read(sock_fd, buf, len)) != 0)
    {
        if (nr < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            else
            {
                return -1;
            }
        }
        len -= nr;
        buf += nr;
        tot_read += nr;
    }

    return tot_read;
}

/* write message to sock_fd*/
ssize_t sock_write(int sock_fd, void *buffer, size_t len)
{
    ssize_t nw, tot_written;
    char *buf = (char *)buffer; // avoid pointer arithmetic on void pointer

    for (tot_written = 0; tot_written < len;)
    {
        nw = write(sock_fd, buf, len - tot_written);

        if (nw <= 0)
        {
            if (nw == -1 && errno == EINTR)
            {
                continue;
            }
            else
            {
                return -1;
            }
        }

        tot_written += nw;
        buf += nw;
    }
    return tot_written;
}

int alloc_shared_memory()
{
    shmid = shmget(KEY, shm_size, IPC_CREAT | 0666);
    // cout << shmid << endl;
    if (shmid < 0)
    {
        fprintf(stderr, "Creating shared memory failed\n");
        return -errno;
    }
    shm = (SHM *)shmat(shmid, NULL, 0); // associate shared memory with this process
    bzero(shm, shm_size);
    return 0;
}

/* This function releases shared memory and semaphore */
int release_shared_memory()
{
    shmdt(shm);                 // disassociate shared memory from process
    shmctl(shmid, IPC_RMID, NULL); // delete shared memory
    return 0;
}

int read_server_list()
{
    ifstream file("server.txt");
    if (file.is_open())
    {
        getline(file, server_ip);
        file.close();
    }
    else
    {
        cerr << "unable to open file." << endl;
        return -1;
    }
    return 0;
}