#include <stddef.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <poll.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <list>
#include "msgfmt.h"

#if 0
#define ALOG(fmt,args...)  printf("[S] " fmt,##args)
#else
#define ALOG(fmt,args...)  do{;}while(0)
#endif

class Server;
class Session {
	DsMsg *msg;
	uint32_t  buflen, rxlen;
	pid_t    cli_id;
	int fd;
	int sys_errno;
	bool closed;

	void TryAlloc(unsigned int nb);
	void SanityCheck();
public:
	Session();
	~Session();
	ssize_t RecvMsg();
	inline void Start(int fd);
	inline void ClearMsg();
	inline bool MsgReady();
	inline bool IsClosed();
	inline int MsgType();
	inline DsMsg *GetMsg();
	inline uint32_t MsgLen();
	inline uint32_t MsgPayloadLen();
	inline int GetFD();
	inline pid_t ClientPid();
};

//---------------------------------------------------------------------------//
//  Session implementaions
//---------------------------------------------------------------------------//
Session::Session()
{
	memset(this, 0, sizeof(*this));
	fd = -1;
}

Session::~Session()
{
	if(fd > 0)
		close(fd);
	if(msg)
		free((void*)msg);
}

void Session::Start(int fd)
{   this->fd = fd; }

int Session::MsgType()
{   return ((DsMsg *)msg)->type; }

DsMsg *Session::GetMsg()
{   return (DsMsg *)msg; }

bool Session::MsgReady()
{   return MsgLen() == rxlen && rxlen > sizeof(DsMsg); }

void Session::ClearMsg()
{
	if(msg) {
		msg = NULL;
		buflen = 0;
	}
	rxlen = 0;
}

bool Session::IsClosed()
{   return closed; }

pid_t Session::ClientPid()
{   return cli_id; }

int Session::GetFD()
{   return fd; }

void Session::TryAlloc(unsigned int nb)
{
	if(buflen < nb) {
		msg = (DsMsg *) realloc(msg, nb+4);
		buflen = nb;
	}
	if(msg == NULL)
		exit(ENOMEM);
}

uint32_t Session::MsgLen()
{
	if(!msg || rxlen < sizeof(msg->len))
		return 0;
	return msg->len;
}

uint32_t Session::MsgPayloadLen()
{
	uint32_t len = MsgLen();
	return len ? len - sizeof(DsMsg) : 0;
}

#define READ2(ret,fd,buf,n)  do { ret = read(fd,buf,n); closed = (ret == 0); if(ret <= 0) goto recv_failure; } while(0)

ssize_t Session::RecvMsg()
{
	ssize_t ret = 0;
	DsMsg *msghdr;

	if( MsgLen() == 0 ) {
		assert(rxlen <= sizeof(msghdr->len));
		TryAlloc(128);
		READ2(ret, fd, ((char*)msg) + rxlen, sizeof(DsMsg) - rxlen);
		rxlen += ret;
		if(rxlen >= sizeof(msghdr->len)) {
			if(MsgLen() <= sizeof(DsMsg)) {
				ALOG("(%06u) Invalid msglen %u, rxlen=%u\n", cli_id, MsgLen(), rxlen);
				exit(250);
			}
		} else
			return ret;
	}

	SanityCheck();

	TryAlloc(MsgLen());
	msghdr = (DsMsg *) msg;
	READ2(ret, fd, (char*)msg + rxlen, MsgLen()-rxlen);
	rxlen += ret;
	if(rxlen >= sizeof(DsMsg)) {
		msghdr = (DsMsg *) msg;
		if(cli_id == 0)
			cli_id = msghdr->pid;
	}
	SanityCheck();
	return ret;

recv_failure:
	sys_errno = errno;
	return -1;
}

void Session::SanityCheck()
{
	DsMsg *msghdr;
	char prefix[256] = {0};

	if(rxlen >= sizeof(msghdr->len))
		snprintf(prefix, sizeof(prefix), "(%06u) %u of %u received, ", cli_id, rxlen, MsgLen());
	else
		snprintf(prefix, sizeof(prefix), "(%06u) %u of * received, ", cli_id, rxlen);

	if(rxlen < sizeof(*msghdr))
		return;

	msghdr = (DsMsg *)msg;
	if(MsgLen() && rxlen > MsgLen()) {
		ALOG("%s: rxlen %u exceeds MsgLen() %u\n", prefix, rxlen, MsgLen());
dump_and_exit:
		ALOG("%s: MsgLen()=%u, rxlen=%u, buflen=%u, fd=%d\n", prefix, MsgLen(), rxlen, buflen, fd);
		exit(250);
	}
	if(msghdr->len <= sizeof(*msghdr) ) {
		ALOG("%s: Invalid msg len %u\n", prefix, msghdr->len);
		goto dump_and_exit;
	}
	if(cli_id && msghdr->pid != cli_id) {
		ALOG("%s: Pid mismatch %06u\n", prefix, msghdr->pid);
		goto dump_and_exit;
	}
	return;
}

class Semaphore {
	pthread_mutex_t m;
	pthread_cond_t  c;
	unsigned int val;
public:
	void Wait();
	void Post();
	Semaphore();
};

Semaphore::Semaphore()
{
	pthread_mutex_init(&m, NULL);
	pthread_cond_init(&c, NULL);
	val = 0;
}

void Semaphore::Wait()
{
	if(val == 0) {
		pthread_mutex_lock(&m);
		while(val == 0)
			pthread_cond_wait(&c, &m);
		pthread_mutex_unlock(&m);
	}
	pthread_mutex_lock(&m);
	val--;
	pthread_mutex_unlock(&m);
}

void Semaphore::Post()
{
	pthread_mutex_lock(&m);
	val++;
	pthread_cond_signal(&c);
	pthread_mutex_unlock(&m);
}


//---------------------------------------------------------------------------//
//  A File Server Class
//---------------------------------------------------------------------------//
class FileAgent {
	bool alive;
	pthread_t thread;
	Semaphore sema;
	pthread_mutex_t mq_lock;
	void *MainThread();
	static void *MainThread(void *obj);
	std::list<DsMsg *> mq;
	int msg_type;
	FILE *fp;
public:
	bool Post(DsMsg *msg);
	FileAgent(const char *rt_dir, const char *filename, int mtype);
	~FileAgent();
	char *filename;
	uint64_t total_rx, total_wr;
	uint32_t wr_cnt;
};

FileAgent::FileAgent(const char *rt_dir, const char *filename_, int mtype_)
{
	int ret;

	total_rx = 0;
	total_wr = 0;
	wr_cnt = 0;

	pthread_mutex_init(&mq_lock, NULL);
	msg_type = mtype_;
	alive = true;

	filename = (char *) malloc(512 + 4);
	if(filename_[0] != '/')
		sprintf(filename, "%s/%s", rt_dir, filename_);
	else
		strcpy(filename, filename_);
	fp = fopen(filename, "wb+");
	if(fp == NULL)
		throw errno;

	ret = pthread_create(&thread, NULL, MainThread, this);
	if(ret)
		throw ret;
}

FileAgent::~FileAgent()
{
	void *retval;
	alive = false;
	sema.Post();
	pthread_join(thread, &retval);
}

void * FileAgent::MainThread(void *obj)
{
	return ((FileAgent*)obj)->MainThread();
}

void * FileAgent::MainThread()
{
	while(alive || mq.size()) {
		sema.Wait();
		while(mq.size()) {
			DsMsg *msg;

			pthread_mutex_lock(&mq_lock);
			msg = mq.front();
			mq.pop_front();
			pthread_mutex_unlock(&mq_lock);

			wr_cnt++;
			total_wr += fwrite(msg->data, 1, msg->len - sizeof(DsMsg), fp);
			free((void*)msg);
		}
	}
	fclose(fp);
	return NULL;
}

bool FileAgent::Post(DsMsg *new_msg)
{
	pthread_mutex_lock(&mq_lock);
	mq.push_back(new_msg);
	total_rx += new_msg->len - sizeof(*new_msg);
	pthread_mutex_unlock(&mq_lock);

	sema.Post();
	return true;
}

//---------------------------------------------------------------------------//
//  A Message Server Class
//---------------------------------------------------------------------------//
class Server {
	const char *of_array[MSGT_MAX],  *server_addr;
	const char *rt_dir;
	FileAgent *agents[MSGT_MAX];
	unsigned int max_clients;
	unsigned int nfds;
	Session **sessions;
	struct pollfd *pollfds;
	int listenfd;
	uid_t uid;

	int Accept();
	void NewSession(int fd, int events);
	void DelSession(int index);
	void Expand();
public:
	int last_error;
	bool Listen();
	bool Run();
	Server(unsigned int nc);
	bool ParseOptions(int argc, char *argv[]);
	static bool proc_exit;
};

bool Server::proc_exit = false;

static int setnonblocking(int fd, int blocking)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
		return 0;
	flags = blocking ? (flags&~O_NONBLOCK) : (flags|O_NONBLOCK);
	return (fcntl(fd, F_SETFL, flags) == 0) ? 1 : 0;
}

/*
 * Create a server endpoint of a connection.
 * Returns true if all OK, false on error.
 */
bool Server::Listen()
{
	int len;
	struct sockaddr_un saddr_un;
	int fd;

	/* create a UNIX domain stream socket */
	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
		goto errout;
	unlink(server_addr);   /* in case it already exists */

	/* fill in socket address structure */
	memset(&saddr_un, 0, sizeof(saddr_un));
	saddr_un.sun_family = AF_UNIX;
	strcpy(saddr_un.sun_path, server_addr);
	len = offsetof(struct sockaddr_un, sun_path) + strlen(server_addr);

	/* bind the name to the descriptor */
	if (bind(fd, (struct sockaddr *)&saddr_un, len) < 0)
		goto errout;
	if (listen(fd, 10) < 0) /* tell kernel we're a server */
		goto errout;
	listenfd = fd;
	return true;

errout:
	last_error = errno;
	close(fd);
	return false;
}

int Server::Accept()
{
	int clifd;
	socklen_t len;
	struct sockaddr_un  saddr_un;
	struct stat statbuf;

	ALOG("%s:%u\n", __func__, __LINE__);
	len = sizeof(saddr_un);
	if ((clifd = accept(listenfd, (struct sockaddr *)&saddr_un, &len)) < 0)
		goto errout;     /* often errno=EINTR, if signal caught */

	ALOG("%s:%u\n", __func__, __LINE__);
#ifndef __CYGWIN__
/* FIXME: Right now we have no way to determine the
 *        bound socket name of the peer's socket.  For now
 *        we just fake an unbound socket on the other side. */

	/* obtain the client's uid from its calling address */
	len -= offsetof(struct sockaddr_un, sun_path); /* len of pathname */
	saddr_un.sun_path[len] = 0;           /* null terminate */

	ALOG("%s:%u\n", __func__, __LINE__);
	if (stat(saddr_un.sun_path, &statbuf) < 0)
		goto errout;

	ALOG("%s:%u: %s: 0x%x\n", __func__, __LINE__, saddr_un.sun_path, statbuf.st_mode);
	/* Not a socket */
	if (S_ISSOCK(statbuf.st_mode) == 0)
		goto errout;

	uid = statbuf.st_uid;   /* return uid of caller */
	unlink(saddr_un.sun_path);    /* we're done with pathname now */
#endif
	return(clifd);

errout:
	last_error = errno;
	close(clifd);
	return -1;
}

void Server::NewSession(int fd, int events)
{
	pollfds[nfds].fd = fd;
	pollfds[nfds].events = events;
	sessions[nfds] = new Session;
	sessions[nfds]->Start(fd);
	nfds++;
}

void Server::DelSession(int index)
{
	delete sessions[index];
	--nfds;
	const int n = nfds - index;
	if(n == 0)
		return;
	memmove(&pollfds[index], &pollfds[index+1], sizeof(pollfds[0]) * n);
	memmove(&sessions[index], &sessions[index+1], sizeof(sessions[0]) * n);
}

bool Server::Run()
{
	unsigned int i;
	memset(pollfds, 0, sizeof(pollfds[0]) * max_clients);
	pollfds[0].events = POLLIN ;
	pollfds[0].fd = listenfd;
	nfds = 1;

	for(i = 0; i < MSGT_MAX; i++) {
		if(of_array[i] == NULL)
			continue;
		try {
			agents[i]  = new FileAgent(rt_dir, of_array[i], i);
		} catch(int e) {
			printf("Catch error \"%s\"\n", strerror(e));
			return false;
		}
	}

	while (!proc_exit) {
		int ret;

		ret = poll(pollfds, nfds, -1);
		if (ret == -1) {
			if(errno == EINTR)
				continue;
			last_error = errno;
			printf("Poll failed %d, \"%s\"\n", errno, strerror(errno));
			return false;
		}

		if (pollfds[0].revents == POLLIN) {
			int clifd ;

			clifd = Accept();
			if (clifd < 0) {
				perror("accept");
				exit(EXIT_FAILURE);
			}
			if (nfds == max_clients)
				Expand();
			if (nfds == max_clients) {
				ALOG("No space for new connection (%d)\n", clifd);
				close(clifd);
				continue;
			}

			assert(nfds >= 1);
			ALOG("new connection (%d) set up\n", clifd);
			setnonblocking(clifd, 1);
			NewSession(clifd, POLLIN);
		} else {
			Session *sess;
			unsigned int i;
			for(i = 1; i < nfds; i++) {
				assert(pollfds[i].fd == sessions[i]->GetFD());
				sess = sessions[i];
				if( pollfds[i].revents & POLLIN ) {
					ret = sess->RecvMsg();
					if(sess->MsgReady()) {
						const int mt = sess->MsgType();
						FileAgent *const ag = agents[mt];
						assert(mt >= 0 && mt < MSGT_MAX);
						assert(ag);
						ag->Post(sess->GetMsg());
						sess->ClearMsg();
					}
				}
				if( sess->IsClosed() || (pollfds[i].revents & POLLHUP) ) {
					assert(nfds > 1);
					ALOG("(%06u) DelSession(%d)\n", sess->ClientPid(), i);
					DelSession(i);
					assert(nfds >= 1);
					i--;
					continue;
				}
			}
		}
	}

	for(i = 0; i < MSGT_MAX; i++) {
		if(agents[i]) {
			delete agents[i];
			agents[i] = NULL;
		}
	}
	return true;
}

Server::Server(unsigned int nc)
{
	memset(this, 0, sizeof(*this));
	sessions = new Session *[nc];
	pollfds  = new struct pollfd[nc];
	max_clients = nc;
	rt_dir = "/var/tmp";
}

void Server::Expand()
{
	const unsigned int n = max_clients + 4;
	pollfds  = (struct pollfd *) realloc(pollfds, sizeof(pollfds[0]) * n);
	sessions = (Session **) realloc(sessions,sizeof(sessions[0]) * n);
	max_clients = n;
}

bool Server::ParseOptions(int argc, char *argv[])
{
	enum {
		COP_SERVER_ADDR = 270,
		COP_SAVE_COMMAND_LINE,
		COP_SAVE_DEPENDENCY,
		COP_SAVE_CONDVALS,
		COP_RTDIR
	};
	static const struct option long_options[] = {
        {"server-addr",     1, 0, COP_SERVER_ADDR },
        {"save-cl",         1, 0, COP_SAVE_COMMAND_LINE },
        {"yz-save-cl",      1, 0, COP_SAVE_COMMAND_LINE },
        {"save-dep",        1, 0, COP_SAVE_DEPENDENCY },
        {"yz-save-dep",     1, 0, COP_SAVE_DEPENDENCY },
        {"save-condvals",   1, 0, COP_SAVE_CONDVALS },
        {"yz-save-condvals",1, 0, COP_SAVE_CONDVALS },
        {"rt-dir",          1, 0, COP_RTDIR },
        {0, 0, 0, 0}
    };
    bool retval = true;

    while (1) {
        int c;

        c = getopt_long_only(argc, argv, "", long_options, NULL);
        if (c == -1) {
			retval = false;
            break;
		}

        switch (c) {
        case COP_SERVER_ADDR:
			server_addr = strdup(optarg);
            break;
        case COP_SAVE_COMMAND_LINE:
			of_array[MSGT_CL] = strdup(optarg);
			break;
        case COP_SAVE_DEPENDENCY:
			of_array[MSGT_DEP] = strdup(optarg);
			break;
        case COP_SAVE_CONDVALS:
			of_array[MSGT_CV] = strdup(optarg);
			break;
        case COP_RTDIR:
			rt_dir = strdup(optarg);
			break;
		}
	}
	if(!server_addr)
		server_addr = "/var/tmp/cl-server";
	return retval;
}

static void sighandler(int signum)
{
	if(signum == SIGUSR1)
		Server::proc_exit = true;
}

int main(int argc, char *argv[])
{
	Server server(32);
	int ret;

	signal(SIGUSR1, sighandler);

	if(server.ParseOptions(argc,argv))
		exit(1);

	ret = server.Listen();
	if(!ret)
		goto do_exit;

	ret = server.Run();
	if(!ret)
		goto do_exit;

	return 0;

do_exit:
	if(server.last_error)
		ALOG("Server exit with errno %d\n", server.last_error);
	return -server.last_error;
}


