#include <iostream>
#include <thread>
#include <map>
#include <atomic>
 
#include <queue.h>
#include <common.h>

#define _SCL_SECURE 0

static packet packets[RECV_WINDOW]; 
static sequence next_frame_expected = 0;

typedef struct receiver {
	int sockfd, outfd;
	std::thread writer, worker;

	mutable std::map<uint32_t, packet*> buffered;
	mutable sf::queue<packet*> idle, busy;

	receiver(int argc, char** &args) {
		if(argc != 3) {
			fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", args[0]);
			exit(EXIT_FAILURE);
		}

		init_udp_socket((uint16_t)atoi(args[1]));
		init_output_fd(args[2]);
		init_workers();
	}

	~receiver() {
		writer.join();

		close(sockfd);
		close(outfd);
	}

	inline void init_udp_socket(uint16_t port) {
		struct sockaddr_in myaddr;      
		memset((void *)&myaddr, 0, sizeof(myaddr));
		myaddr.sin_family = AF_INET;
		myaddr.sin_addr.s_addr = INADDR_ANY;
		myaddr.sin_port = htons(port);

		if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			perror("socket");
			exit(EXIT_FAILURE);
		}
		if (bind(sockfd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
			perror("bind");
			exit(EXIT_FAILURE);
		}
	}

	inline void init_output_fd(const char* f) {
		outfd = open(f, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
	}

	inline void init_workers() {
		// idle = uiuc::queue_create(0);
		// busy = uiuc::queue_create(0);
		// packets = new packet [RECV_WINDOW];
		// if (!packets) {
		// 	fprintf(stderr, "OUT OF MEMORY\n");
		// 	exit(EXIT_FAILURE);
		// }
		for (int i=0; i<RECV_WINDOW; ++i) {
			idle.push(&packets[i]);
		}
		writer = std::thread([this] {process();});
	}

	void process() const {
		packet *task = nullptr;
		while (1) {
			busy.pull(task);
			
			write(outfd, task->buf, task->len);
			idle.push(task);

			for (auto it=buffered.begin(); it!=buffered.end();) {
				if (it->first < next_frame_expected) {
					idle.push(it->second);
					it = buffered.erase(it);
				} else if (it->first == next_frame_expected) {
					busy.push(it->second);
					++ next_frame_expected;
					it = buffered.erase(it);
				} else {
					++ it;
				}
			}
			if (task->fin) break;
		}
	}

	void listen() const {
		struct sockaddr_in remaddr;
		socklen_t addrlen = sizeof(remaddr);

		ssize_t  recvlen;
		packet* p_in = nullptr;
		while (true) { 
			idle.pull(p_in);
			recvlen = recvfrom(sockfd, p_in, sizeof(packet), 0, (struct sockaddr *)&remaddr, &addrlen);
			if (recvlen == sizeof(sequence)) break;

#ifdef DEBUG
printf("cli recv seq %d\n", p_in->seq);
#endif

			sequence seq = p_in->seq;
			sendto(sockfd, (void*)&seq, sizeof(sequence), 0, (struct sockaddr *)&remaddr, sizeof(remaddr));

			if (seq == next_frame_expected) {
				busy.push(p_in);
				++ next_frame_expected;
			} else if (seq > next_frame_expected) {
				if (buffered.find(seq) == buffered.end()) {
					buffered[seq] = p_in;
				} else {
					idle.push(p_in);
				}
			} else {
				idle.push(p_in);
			}
		}
	}

} receiver;


int main(int argc, char** argv) {
	receiver r(argc, argv);
	r.listen();
	return 0;
}
