#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct s_client {
  int fd, id;
  struct s_client* next;
} t_client;

t_client* g_clients = NULL;
int sock_fd, g_id = 0;
fd_set curr_sock, fd_read, fd_write;
char msg[64], str[64 * 4096], tmp[64 * 4096], buf[64 * (4096 + 1)];

void write_fd(int fd, char* str) {
  write(fd, str, strlen(str));
}
void fatal() {
  write_fd(2, "Fatal error\n");
  close(sock_fd);
  exit(1);
}

int get_id(int fd) {
  t_client* curs = g_clients;

  while (curs) {
    if (curs->fd == fd)
      return (curs->id);
    curs = curs->next;
  }
  return -1;
}

int get_max_fd() {
  int max = sock_fd;
  t_client* curs = g_clients;

  while (curs) {
    if (curs->fd > max)
      max = curs->fd;
    curs = curs->next;
  }
  return max;
}

void send_all_except(int fd, char* msg) {
  t_client* curs = g_clients;

  while (curs) {
    if (curs->fd != fd && FD_ISSET(curs->fd, &fd_write)) {
      if (send(curs->fd, msg, strlen(msg), 0) == -1)
        fatal();
    }
    curs = curs->next;
  }
}

// 새 클라이언트 만들어 리스트에 추가
int insert_client(int fd) {
  t_client *curs = g_clients, *new;
  t_client cons = {
      .id = g_id++,
      .fd = fd,
      .next = NULL,
  };

  if (!(new = malloc(sizeof(t_client))))
    fatal();
  *new = cons;

  if (!g_clients) {
    g_clients = new;
  } else {
    while (curs->next)
      curs = curs->next;
    curs->next = new;
  }
  return new->id;
}

int delete_client(int fd) {
  t_client *del, *curs = g_clients;
  const int id = get_id(fd);

  if (curs && curs->fd == fd) {
    g_clients = curs->next;
    free(curs);
  } else {
    while (curs && curs->next && curs->next->fd != fd)
      curs = curs->next;
    del = curs->next;
    curs->next = curs->next->next;
    free(del);
  }
  return id;
}

void accept_client() {
  struct sockaddr_in clientaddr;
  socklen_t len = sizeof(clientaddr);
  int client_fd;

  if ((client_fd = accept(sock_fd, (struct sockaddr*)&clientaddr, &len)) < 0)
    fatal();

  sprintf(msg, "server: client %d just arrived\n", insert_client(client_fd));
  send_all_except(client_fd, msg);
  FD_SET(client_fd, &curr_sock);
}

void receive_msg(int fd) {
  int j = 0;

  for (int i = 0; str[i]; i++) {
    tmp[j++] = str[i];
    if (str[i] == '\n') {
      sprintf(buf, "client %d: %s", get_id(fd), tmp);
      send_all_except(fd, buf);
      j = 0;
      memset(&tmp, 0, strlen(tmp));
      memset(&buf, 0, strlen(buf));
    }
  }
  memset(&str, 0, strlen(str));
}

int main(int ac, char* av[]) {
  // 포트 초기화
  if (ac != 2) {
    write_fd(2, "Wrong number of arguments\n");
    exit(1);
  }
  const uint16_t port = atoi(av[1]);

  // 주소 초기화
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  addr.sin_port = htons(port);

  // 소켓 생성 -> 소켓 fd에 주소 바인드 -> 듣기
  if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0
      || bind(sock_fd, (const struct sockaddr*)&addr, sizeof(addr)) < 0
      || listen(sock_fd, 0) < 0)
    fatal();

  FD_ZERO(&curr_sock);          // 초기화
  FD_SET(sock_fd, &curr_sock);  // 소켓 fd 추가

  // 버퍼 초기화
  memset(str, 0, sizeof(str));
  memset(tmp, 0, sizeof(tmp));
  memset(buf, 0, sizeof(buf));

  // 메인 루프
  while (true) {
    fd_write = fd_read = curr_sock;
    if (select(get_max_fd() + 1, &fd_read, &fd_write, NULL, NULL) < 0)
      continue;
    for (int fd = 0; fd <= get_max_fd(); fd++) {
      if (FD_ISSET(fd, &fd_read)) {
        if (fd == sock_fd) {
          memset(&msg, 0, sizeof(msg));
          accept_client();
          break;
        } else {
          int len = 1000;
          while (len == 1000 || str[strlen(str) - 1] != '\n') {
            if ((len = recv(fd, str + strlen(str), 1000, 0)) <= 0)
              break;
          }
          if (len <= 0) {
            memset(&msg, 0, sizeof(msg));
            sprintf(msg, "server: client %d just left\n", delete_client(fd));
            send_all_except(fd, msg);
            FD_CLR(fd, &curr_sock);
            close(fd);
            break;
          } else {
            receive_msg(fd);
          }
        }
      }
    }
  }
}
