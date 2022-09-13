# Exam 06

## Checklist

- don't forget `FD_ZERO`, `FD_SET`, `FD_ISSET`. `FD_CLR`
- `accept()` uses `sock_fd`
- `select()` `nfds` is `get_max_fd() + 1`
- remember `receive_msg` and `broadcast_msg`
