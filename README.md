# Yet another TCP server implemented with POSIX threads
TCP server implemented with POSIX threads.  For every accept call we create a
thread.  When a client is accepted the function `handle_session` is called.
There is static member that shares between sessions called `VALUE`.

Every session can
- increment
- decrement
- get

`VALUE`.

We lock the mutex to protect `VALUE` from the interference of other threads
when one of these operations is performing. This mutex is `val_mutex`.

## Build
Compile and run it with the following commands:

```bash
$ g++ -pthread server.cpp -o server
$ ./server
```

## Use
Use `telnet` program to connect to the server with port 6666:

```bash
[roman ~]$ telnet 0 6666
Trying 0.0.0.0...
Connected to 0.
Escape character is '^]'.
show
0
up
Ok
show
1
```
