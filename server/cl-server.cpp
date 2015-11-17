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
#include <set>
#include "msgfmt.h"
#include "defconfig.h"
#include "ip_sc.h"

#if 0
#include <sys/time.h>
#define ALOG(fmt,args...)  do { struct timeval tv; gettimeofday(&tv,NULL); printf("[%08lu.%06lu] [S] " fmt, tv.tv_sec, tv.tv_usec, ##args); } while(0)
#else
#define ALOG(fmt,args...)  do{;}while(0)
#endif

InterProcessSharedCounter *sm;

class Server;
class Session {
	DsMsg *msg;
	uint32_t  buflen, rxlen;
	pid_t    cli_id;
	int fd;
	int sys_errno;
	bool closed;

	inline bool MsgReady();
	void TryAlloc(unsigned int nb);
	void SanityCheck();
public:
	enum {
		S_ERR = -1,
		S_MOK,
		S_MORE,
	};
	Session();
	~Session();
	int RecvMsg();
	inline void Start(int fd);
	inline void ClearMsg();
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

#define READ_CHECKED(ret,fd,buf,n)  do { \
	ret = read(fd,buf,n);      \
	if(ret < 0) {              \
		if(errno == EAGAIN)    \
			goto no_more_data; \
		else                   \
			goto recv_failure; \
	} else if(ret == 0) {      \
		closed = true;         \
		goto no_more_data;     \
	}                          \
} while(0)

int Session::RecvMsg()
{
	DsMsg *msghdr;
	ssize_t ret;
	int retval = S_MORE;

	if( MsgLen() == 0 ) {
		assert(rxlen <= sizeof(msghdr->len));
		TryAlloc(128);
		READ_CHECKED(ret, fd, ((char*)msg) + rxlen, sizeof(DsMsg) - rxlen);
		rxlen += ret;
		if(rxlen >= sizeof(msghdr->len)) {
			if(MsgLen() <= sizeof(DsMsg)) {
				ALOG("(%06u) Invalid msglen %u, rxlen=%u\n", cli_id, MsgLen(), rxlen);
				exit(250);
			}
		} else
			goto no_more_data;

	}

	TryAlloc(MsgLen());
	msghdr = (DsMsg *) msg;
	READ_CHECKED(ret, fd, (char*)msg + rxlen, MsgLen()-rxlen);
	rxlen += ret;
	if(rxlen >= sizeof(DsMsg)) {
		msghdr = (DsMsg *) msg;
		if(cli_id == 0)
			cli_id = msghdr->pid;

	}

no_more_data:
	SanityCheck();
	if(MsgReady())
		retval = S_MOK;
	return retval;

recv_failure:
	sys_errno = errno;
	ALOG("%s:%u: pid=%06u, fd=%d, errno=%d, ret=%zi, read_size=%zi, rxlen=%u\n", __func__, __LINE__, cli_id, fd, sys_errno, ret, read_size, rxlen);
	return S_ERR;
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

	struct lt
	{
		bool operator()(const Md5Sum& s1, const Md5Sum& s2) const
		{ return memcmp(s1.data, s2.data, sizeof(s1.data)) < 0; }
	};
	typedef std::set<Md5Sum,lt> Md5Set;

	bool alive;
	pthread_t thread;
	Semaphore sema;
	pthread_mutex_t mq_lock;
	void *MainThread();
	static void *MainThread(void *obj);
	std::list<DsMsg *> mq;
	Md5Set *md5_set;
	int msg_type;
	FILE *fp;
public:
	bool Post(DsMsg *msg);
	FileAgent(const char *rt_dir, const char *filename, int mtype);
	~FileAgent();
	char *filename;
	uint32_t msg_wr, msg_rx;
};

FileAgent::FileAgent(const char *rt_dir, const char *filename_, int mtype_)
{
	int ret;

	msg_rx = 0;
	msg_wr = 0;

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

#if HAVE_MD5
	if(mtype_ == MSGT_CV)
		md5_set = new Md5Set;
	else
#endif
		md5_set = NULL;

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

			msg_wr++;
			fwrite(msg->data, 1, msg->len - sizeof(DsMsg), fp);
			free((void*)msg);
		}
	}
	fclose(fp);
	printf("[%u] %u MSGs received, %u MSGs written out; %u sent from clients\n", msg_type, msg_rx, msg_wr, sm->records[msg_type].writes);
	return NULL;
}

bool FileAgent::Post(DsMsg *msg)
{
	msg_rx++;

	if(md5_set && (msg->flags & DMF_MD5) ) {
		bool md5_exists = false;
		if( md5_set->find(msg->md5_sum) != md5_set->end() )
			md5_exists = true;
		else
			md5_set->insert(msg->md5_sum);
		if(md5_exists)
			return true;
	}

	pthread_mutex_lock(&mq_lock);
	mq.push_back(msg);
	pthread_mutex_unlock(&mq_lock);

	sema.Post();
	return true;
}

//---------------------------------------------------------------------------//
//  A Message Server Class
//---------------------------------------------------------------------------//
class Server {
	const char *of_array[MSGT_MAX];
	char *server_addr;
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

static bool setblocking(int fd, int blocking)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
		return false;
	flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
	return (fcntl(fd, F_SETFL, flags) == 0) ? true : false;
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

	len = sizeof(saddr_un);
	if ((clifd = accept(listenfd, (struct sockaddr *)&saddr_un, &len)) < 0)
		goto errout;     /* often errno=EINTR, if signal caught */

#ifndef __CYGWIN__
/* FIXME: Right now we have no way to determine the
 *        bound socket name of the peer's socket.  For now
 *        we just fake an unbound socket on the other side. */

	/* obtain the client's uid from its calling address */
	len -= offsetof(struct sockaddr_un, sun_path); /* len of pathname */
	saddr_un.sun_path[len] = 0;           /* null terminate */

	if (stat(saddr_un.sun_path, &statbuf) < 0)
		goto errout;

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
			switch(errno) {
			/* Continue to receive data from sockets if there are any */
			case EINTR:
				break;
			default:
				last_error = errno;
				printf("Poll failed %d, \"%s\"\n", errno, strerror(errno));
				return false;
			}
		}

		if (pollfds[0].revents == POLLIN) {
			int clifd ;

			clifd = Accept();
			if (clifd < 0) {
				fprintf(stderr, "Cannot accept, errno=%d (%s)\n", errno, strerror(errno));
				exit(EXIT_FAILURE);
			}
			if (nfds == max_clients)
				Expand();
			if (nfds == max_clients) {
				fprintf(stderr, "No space for new connection (%d)\n", clifd);
				close(clifd);
				continue;
			}

			assert(nfds >= 1);
			ALOG("new connection (%d) set up\n", clifd);
			if( ! setblocking(clifd, 0) ) {
				fprintf(stderr, "Cannot set fd %d to non-blocking mode\n", clifd);
				exit(EXIT_FAILURE);
			}
			NewSession(clifd, POLLIN);
		} else {
			Session *sess;
			unsigned int i;
			for(i = 1; i < nfds; i++) {
				assert(pollfds[i].fd == sessions[i]->GetFD());
				sess = sessions[i];
				if( pollfds[i].revents & POLLIN ) {
					 do {
						ret = sess->RecvMsg();
						if(ret == Session::S_MOK) {
							const int mt = sess->MsgType();
							FileAgent *const ag = agents[mt];
							assert(mt >= 0 && mt < MSGT_MAX);
							assert(ag);
							ag->Post(sess->GetMsg());
							sess->ClearMsg();
						} else if (ret == Session::S_ERR ) {
							ALOG("errno=%d\n", ret);
							exit(-EINVAL);
						}
					} while(ret != Session::S_MORE);
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
	rt_dir = strdup(DEF_RT_DIR);
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
        {"save-dep",        1, 0, COP_SAVE_DEPENDENCY },
        {"save-condvals",   1, 0, COP_SAVE_CONDVALS },
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
			if(rt_dir)
				free((void*)rt_dir);
			rt_dir = strdup(getopt_default(optarg,DEF_RT_DIR));
			break;
		}
	}
	if(!server_addr) {
		size_t n;
		n = snprintf(NULL, 0, "%s/%s", rt_dir, DEF_SVR_ADDR);
		server_addr = (char *)malloc(n + 4);
		snprintf(server_addr, n+4, "%s/%s", rt_dir, DEF_SVR_ADDR);
	}
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
//	InterProcessSharedCounter *sm;

	signal(SIGUSR1, sighandler);

	if(server.ParseOptions(argc,argv))
		exit(1);

	sm = ipsc_create("/var/tmp/SM0");
	if(sm == NULL)
		exit(ENOMEM);

	ret = server.Listen();
	if(!ret)
		goto do_exit;

	ret = server.Run();
	if(!ret)
		goto do_exit;

//	printf("%u (%uK) writes in %u.%03u seconds\n", sm->u32_array[0], (uint32_t)(sm->u64_array[1]/1024),
//		(uint32_t)(sm->u64_array[0]/1000000ULL), (uint32_t)(sm->u64_array[0]%1000000ULL)/1000);
	return 0;

do_exit:
	if(server.last_error)
		ALOG("Server exit with errno %d\n", server.last_error);
	return -server.last_error;
}


