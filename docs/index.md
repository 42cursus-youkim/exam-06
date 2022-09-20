# Exam 06

[github page](https://42cursus-youkim.github.io/exam-06/)

[reference code](https://github.com/markveligod/examrank-02-03-04-05-06/blob/master/examRank06/mini_serv.c)

## Pitfalls

- don't forget `FD_ZERO`, `FD_SET`, `FD_ISSET`. `FD_CLR`
- `accept()` uses `sock_fd`
- `select()` `nfds` is `get_max_fd() + 1`
- remember `receive_msg` and `broadcast_msg`
- `recv <= 0`!!!!!!
