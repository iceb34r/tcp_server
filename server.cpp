#include <liburing.h>
#include <iostream>
#include <fstream>
#include <string>
#include <netinet/in.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>


#define MAX_CONNECTIONS     5
#define ENTRIES				1024
#define BACKLOG             512
#define MAX_MESSAGE_LEN     128

void submit_accept(struct io_uring *ring, int fd, struct sockaddr *client_addr, socklen_t *client_len, int flag);
void submit_read(struct io_uring *ring, int fd, size_t size, int flag);
void submit_write(struct io_uring *ring, int fd,  size_t size, int flag);

enum {
    ACCEPT,
    READ,
    WRITE,
};

typedef struct conn_info {
    int fd;
    unsigned type;
} conn_info;

conn_info conns[MAX_CONNECTIONS];

int set_listening_socket(int port) {

    struct sockaddr_in serv_addr;

    int serv_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (serv_fd == -1) { std::cerr << "socket error " << std::endl; exit(EXIT_FAILURE); }
	
    const int enable = 1;
    setsockopt(serv_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serv_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        std::cerr << "bind error " << std::endl;
        exit(EXIT_FAILURE);
    }
	
    if (listen(serv_fd, BACKLOG) == -1) {
        std::cerr << "listen error " << std::endl;
        exit(EXIT_FAILURE);
    }
    std::cout << "server listening on port: " << port << std::endl;

return serv_fd;
}

char buff[MAX_CONNECTIONS][MAX_MESSAGE_LEN] = {0};
const char acceptmsg[10] = "ACCEPTED\n";

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cout << "Usage : ./server [port]" << std::endl;
        exit(0);
    }

	std::string fileName = argv[1];
    int port = atoi(argv[1]);
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

	int servfd = set_listening_socket(port);
	std::fstream file ;
	file.open(fileName + ".txt", std::ios::app | std::ios::in);
	if(!file) {
		std::cout << "File creation error " << std::endl;
		shutdown(servfd, SHUT_RDWR);
		exit(0);
	}

    struct io_uring_params params;
    struct io_uring ring;
    memset(&params, 0, sizeof(params));

    if (io_uring_queue_init_params(ENTRIES, &ring, &params) < 0) {
        std::cerr <<"io_uring_init failed " << std::endl;
        exit(EXIT_FAILURE);
    }

    if (!(params.features & IORING_FEAT_FAST_POLL)) {
        std::cerr << "IORING_FEAT_FAST_POLL not available in the kernel " << std::endl;
        exit(EXIT_FAILURE);
    }

    submit_accept(&ring, servfd, (struct sockaddr *) &client_addr, &client_len, O_NONBLOCK);

    while (1) {
        struct io_uring_cqe *cqe;
        int ret;

        io_uring_submit(&ring);

        ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret != 0) { 
			std::cerr << "io_uring_wait_cqe error" << std::endl;
			exit(EXIT_FAILURE);
		}
			
        struct io_uring_cqe *cqes[BACKLOG];
        int cqe_count = io_uring_peek_batch_cqe(&ring, cqes, sizeof(cqes) / sizeof(cqes[0]));

        for (int i = 0; i < cqe_count; ++i) {
            cqe = cqes[i];

            struct conn_info *user_data = (struct conn_info *) io_uring_cqe_get_data(cqe);

            unsigned type = user_data->type;
            if (type == ACCEPT) {
				
                int sock_conn_fd = cqe->res;
				std::cout << "new connection established" << std::endl;
                submit_read(&ring, sock_conn_fd, MAX_MESSAGE_LEN, O_NONBLOCK);
                submit_accept(&ring, servfd, (struct sockaddr *) &client_addr, &client_len, O_NONBLOCK);
				
            } else if (type == READ) {
				
                int bytes_read = cqe->res;
				std::cout << "read client message" << std::endl;

				for (int i = 0; i < bytes_read; i++) {
					file << buff[user_data->fd][i];
					file.flush();
				}
                if (bytes_read <= 0) {
                    shutdown(user_data->fd, SHUT_RDWR);
					close(servfd);
					exit(0);
                } else {
					sleep(3);
                    submit_write(&ring, user_data->fd, bytes_read, O_NONBLOCK);
                }
				
            } else if (type == WRITE) {
                submit_read(&ring, user_data->fd, MAX_MESSAGE_LEN, O_NONBLOCK);
            }

            io_uring_cqe_seen(&ring, cqe);
        }
    }
}


void submit_accept(struct io_uring *ring, int fd, struct sockaddr *client_addr, socklen_t *client_len, int flag) {

    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    io_uring_prep_accept(sqe, fd, client_addr, client_len, 0);
	io_uring_sqe_set_flags(sqe, flag);

    conn_info *conn_i = &conns[fd];
    conn_i->fd = fd;
    conn_i->type = ACCEPT;

    io_uring_sqe_set_data(sqe, conn_i);
}


void submit_read(struct io_uring *ring, int fd, size_t size, int flag) {
	
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    io_uring_prep_recv(sqe, fd, &buff[fd], size, 0);
	io_uring_sqe_set_flags(sqe, flag);

    conn_info *conn_i = &conns[fd];
    conn_i->fd = fd;
    conn_i->type = READ;

    io_uring_sqe_set_data(sqe, conn_i);
}


void submit_write(struct io_uring *ring, int fd, size_t size, int flag) {

    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    io_uring_prep_send(sqe, fd, &acceptmsg, sizeof(acceptmsg), 0);
	io_uring_sqe_set_flags(sqe, flag);

    conn_info *conn_i = &conns[fd];
    conn_i->fd = fd;
    conn_i->type = WRITE;

    io_uring_sqe_set_data(sqe, conn_i);
}
