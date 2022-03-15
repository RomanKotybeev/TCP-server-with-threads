#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

char ok_msg[] = "Ok\n";
char unknown_cmd[] = "Options: up, down, show (or \\q to quit)\n";

enum {
	listen_queue_len = 16,
	buf_len = 1024,
};

class SocketObj {
	sockaddr_in addr;
	int fd;
public:
	SocketObj(sockaddr_in& addr, int fd)
		: addr(addr), fd(fd)
	{}
	int GetFd() const { return fd; }
	virtual ~SocketObj() { close(fd); }
};

class Session : public SocketObj {
	int buf_used;
	static int VALUE;
	pthread_mutex_t val_mut;
public:
	char buffer[buf_len];
	Session(sockaddr_in& addr, int fd)
		: SocketObj(addr, fd)
		, buf_used(0)
		{ val_mut = PTHREAD_MUTEX_INITIALIZER; }
	bool EnterInteractiveMode();
	bool ProcessMsg();
	void CheckLine();
	void Send(char *msg) const;
};

int Session::VALUE = 0;

bool Session::EnterInteractiveMode()
{
	int rc = read(GetFd(), buffer, buf_len);
	if (rc <= 0)
		return false;

	buf_used = rc;
	CheckLine();

	pthread_mutex_lock(&val_mut); // Because VALUE is
	bool ret = ProcessMsg();      // changeable.
	pthread_mutex_unlock(&val_mut);

	return ret;
}

void Session::Send(char *msg) const
{
	write(GetFd(), msg, strlen(msg));
}

bool Session::ProcessMsg()
{
	if (strcmp(buffer, "up") == 0) {
		VALUE++;
		Send(ok_msg);
	} else if (strcmp(buffer, "down") == 0) {
		VALUE--;
		Send(ok_msg);
	} else if (strcmp(buffer, "show") == 0) {
		char *msg = new char[5];
		sprintf(msg, "%d\n", VALUE);
		Send(msg);
	} else if (strcmp(buffer, "\\q") == 0) { // Close Session
		return false;
	} else {
		Send(unknown_cmd);
	}
	return true;
}

void Session::CheckLine()
{
	for (int i = 0; i < buf_used; i++) {
		if (buffer[i] == '\n') {
			if (i - 1 > 0 && buffer[i-1] == '\r') {
				buffer[i] = 0;
				i--;
			}
			buffer[i] = 0;
			buf_used = i;
			return;
		}
	}
}

/* ------------------------------------------------------- */
/* ------------------------ Server ----------------------- */
/* ------------------------------------------------------- */

class Server : public SocketObj {
	Server(sockaddr_in& addr, int fd)
		: SocketObj(addr, fd)
		{}
public:
	static Server* Run(int port);
	Session *AcceptSession();
};

Server *Server::Run(int port)
{
	int sd = socket(AF_INET, SOCK_STREAM, 0);
	if (sd == -1)
		return 0;

	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	int is_ok = bind(sd, (sockaddr *)&addr, sizeof(addr));
	if (is_ok == -1)
		return 0;

	is_ok = listen(sd, listen_queue_len);
	if (is_ok == -1)
		return 0;

	return new Server(addr, sd);
}

Session *Server::AcceptSession()
{
	sockaddr_in addr;
	socklen_t len = sizeof(addr);
	int sd = accept(GetFd(), (sockaddr *)&addr, &len);
	Session *session = new Session(addr, sd);
	return session;
}

/* ------------------------------------------------------- */
/* -------------- main and thread function --------------- */
/* ------------------------------------------------------- */

void *handle_session(void *session)
{
	Session *sess = reinterpret_cast<Session*>(session);
	while (true) {
		bool ok = sess->EnterInteractiveMode();
		if (!ok) {
			delete sess;
			pthread_exit(0);
			return 0;
		}
	}
	return sess;
}

int main()
{
	Server *server = Server::Run(6666);
	if (!server) {
		perror("server");
		return 1;
	}
	while (true) {
		pthread_t thr;
		Session *session = server->AcceptSession();	
		pthread_create(&thr, NULL, handle_session, session);
	}
	return 0;
}
