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
fd_set fd_all, fd_read, fd_write;
char str[64 * 4096], tmp[64 * 4096], buf[64 * (4096 + 1)];

void write_fd(int fd, char* s) {
  write(fd, s, strlen(s));
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
      return curs->id;
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

void send_all_except(int fd, char* s) {
  t_client* curs = g_clients;

  while (curs) {
    // 쓰기 설정된 소켓에만 골라서 보냄
    if (curs->fd != fd && FD_ISSET(curs->fd, &fd_write)) {
      if (send(curs->fd, s, strlen(s), 0) == -1)
        fatal();
    }
    curs = curs->next;
  }
}

// 새 클라이언트 만들어 리스트에 추가
int insert_client(int fd) {
  t_client *curs = g_clients, *new;

  if (!(new = malloc(sizeof(t_client))))
    fatal();
  *new = (t_client){.id = g_id++, .fd = fd, .next = NULL};

  if (!g_clients) {  // 리스트가 비어있으면 첫 원소로
    g_clients = new;
  } else {
    while (curs->next)  // 마지막 원소 찾기
      curs = curs->next;
    curs->next = new;  // 마지막 원소 다음에 추가
  }
  return new->id;
}

int delete_client(int fd) {
  t_client *del, *curs = g_clients;
  const int id = get_id(fd);

  if (curs && curs->fd == fd) {  // 첫 원소일때
    g_clients = curs->next;
    free(curs);
  } else {
    // 삭제할 원소 찾기
    while (curs && curs->next && curs->next->fd != fd)
      curs = curs->next;
    del = curs->next;
    curs->next = del->next;
    free(del);
  }
  return id;
}

/** @brief 연결 수락 후 curr_sock에 추가, 접속 메시지 송신 */
void accept_connection(void) {
  // 주소 초기화
  struct sockaddr_in clientaddr;
  socklen_t len = sizeof(clientaddr);
  int client_fd;

  // 연결을 수락해 만들어진 소켓을 client_fd에 바인딩
  if ((client_fd = accept(sock_fd, (struct sockaddr*)&clientaddr, &len)) < 0)
    fatal();

  // 버퍼에 환영 메시지 작성 후 송신
  memset(buf, 0, 64);
  sprintf(buf, "server: client %d just arrived\n", insert_client(client_fd));
  send_all_except(client_fd, buf);

  // 새로운 클라이언트의 소켓을 curr_sock 목록에 추가
  FD_SET(client_fd, &fd_all);
}

void close_connection(int fd) {
  // 버퍼에 퇴장 메시지 작성 후 송신
  memset(buf, 0, 64);
  sprintf(buf, "server: client %d just left\n", delete_client(fd));
  send_all_except(fd, buf);

  // 소켓을 curr_sock 목록에서 제거 후 접속 종료
  FD_CLR(fd, &fd_all);
  close(fd);
}

void broadcast_msg(int fd) {
  int j = 0;

  for (int i = 0; str[i]; i++) {
    tmp[j++] = str[i];
    if (str[i] == '\n') {
      memset(&buf, 0, strlen(buf));
      sprintf(buf, "client %d: %s", get_id(fd), tmp);
      send_all_except(fd, buf);

      j = 0;
      memset(&tmp, 0, strlen(tmp));
    }
  }
  memset(&str, 0, strlen(str));
}

int receive_msg(int fd) {
  int len = 1000;
  while (len == 1000 || str[strlen(str) - 1] != '\n') {
    if ((len = recv(fd, str + strlen(str), 1000, 0)) <= 0)
      break;
  }
  return len;
}

int main(int ac, char* av[]) {
  // 인자 개수 확인
  if (ac != 2) {
    write_fd(2, "Wrong number of arguments\n");
    exit(1);
  }

  // 주소 및 포트 초기화
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  addr.sin_port = htons(atoi(av[1]));

  // 소켓 생성 -> 소켓 fd에 주소 연결 -> 소켓에서 듣기
  if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0
      || bind(sock_fd, (const struct sockaddr*)&addr, sizeof(addr)) < 0
      || listen(sock_fd, 0) < 0)
    fatal();

  // fd_set 초기화
  FD_ZERO(&fd_all);          // 초기화
  FD_SET(sock_fd, &fd_all);  // 소켓 fd 추가

  // 버퍼 초기화
  memset(str, 0, sizeof(str));
  memset(tmp, 0, sizeof(tmp));
  memset(buf, 0, sizeof(buf));

  // 메인 루프
  while (true) {
    // 듣기/쓰기 소켓 변동 목록 초기화
    fd_write = fd_read = fd_all;
    // 들어온 이벤트가 생길 때까지 대기
    if (select(get_max_fd() + 1, &fd_read, &fd_write, NULL, NULL) < 0)
      continue;
    for (int fd = 0; fd <= get_max_fd(); fd++) {
      if (FD_ISSET(fd, &fd_read)) {  // 이벤트가 생긴 *읽기* 파일 식별자만
        if (fd == sock_fd) {  // 소켓 식별자 자체에서 생긴 이벤트면
          accept_connection();  // 연결이 들어온것이니 사용자 생성
          break;                // 연결 목록이 변했으니 새로고침
        } else {  // 사용자 소켓 중 하나에서 생긴 이벤트면
          int len = receive_msg(fd);  // 메시지 수신
          if (len > 0) {
            broadcast_msg(fd);  // 다른 사용자에게 전부 송신
          } else {
            close_connection(fd);  // 연결이 끊겼으니 사용자 제거
            break;                 // 연결 목록이 변했으니 새로고침
          }
        }
      }
    }
  }
}
