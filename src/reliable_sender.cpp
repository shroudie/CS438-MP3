#include <iostream>
#include <sys/time.h>
#include <sys/stat.h>
#include <time.h> 

#include <map>
#include <list>
#include <mutex>
#include <thread>
#include <atomic> 
#include <condition_variable>
#include <functional>

#include <queue.h>
#include <common.h>

// static packet packets[SEND_WINDOW];

static constexpr struct timespec SEND_TO = {0,250000};

typedef struct nonstop_sender {
	int infd, sockfd;
	std::thread reader, sender, recver;

	mutable sf::queue<packet*> idle;
	mutable std::map<sequence, packet*> buffered;
	// std::list<packet*> used;
	// std::list<packet*>::iterator last; //last acked packet.

	packet *packets = nullptr;

	sequence curr_frame = 0, total_frame_expected;
	mutable file_size bytes_to_transmit, bytes_transmitted = 0, bytes_read = 0;

	std::mutex m;
	mutable std::condition_variable cv;

#ifdef DEBUG
	mutable sequence total_frame_sent = 0;
#endif

	nonstop_sender(int argc, char** args) {
		if(argc != 5) {
			fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", args[0]);
			exit(EXIT_FAILURE);
		}

		struct sockaddr_in dst = { 0 };
		dst.sin_family = AF_INET;
		dst.sin_port = htons((uint16_t)atoi(args[2]));
		dst.sin_addr.s_addr = inet_addr(args[1]);
		if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			perror("socket");
			exit(EXIT_FAILURE);
		}
		if(connect(sockfd, (struct sockaddr *)&dst, sizeof(dst)) < 0) { 
			perror("connect");
			exit(EXIT_FAILURE); 
		} 

		bytes_to_transmit =  atoll(args[4]);
		total_frame_expected = (bytes_to_transmit - 1) / FRAME_SIZE + 1;

#ifdef DEBUG
		printf("TOTAL FRAM EXPECTD: %u\n", total_frame_expected);
#endif
		packets = new packet[std::min(SEND_WINDOW, total_frame_expected)];

		init_fds(args[3]);
		init_workers();
		wait_workers();
	}

	~nonstop_sender() {
		delete[] packets;
	}

	void wait_workers() {
		reader.join();
		sender.join();
		recver.join();
	}

	void send_packet() const {

		// used.loop([this](const PIter& it) { 
		// 	nanosleep(&SEND_TO, NULL);
		// 	printf("send %d\n", (*it)->seq);
		// 	send(sockfd, (*it), sizeof(packet), 0);
		// 	// printf("THIS WORKED %u\n", (*it)->seq); 
		// }, [this]() {
		// 	return bytes_transmitted < bytes_to_transmit;
		// });

		packet* mp;
		auto it = buffered.begin();
		while (bytes_transmitted < bytes_to_transmit) {
        	nanosleep(&SEND_TO, NULL);
        	// nanosleep((const struct timespec[]){{1,0}}, NULL);
			if (it != buffered.end()) {
				// if (mp->p->seq >= next_ack_expected + RECV_WINDOW) {
				// 	it = buffered.begin();
				// 	continue;
				// }
				// printf("SEND PACK %d\n", mp->seq);
				// sendto(sockfd, mp, sizeof(packet), 0, (struct sockaddr *)&dst, sizeof(dst));
#ifdef DEBUG
				total_frame_sent ++;
#endif
				send(sockfd, it->second, sizeof(packet), 0);
				++ it;
			} 
			else {
				it = buffered.begin();
			}
		}
		finalize();
	}

	inline void finalize() const {
		//for the competition's sake, this is equivalent to waiting for 1 RTT.
		send(sockfd, &total_frame_expected, sizeof(sequence), 0);
		send(sockfd, &total_frame_expected, sizeof(sequence), 0);
		send(sockfd, &total_frame_expected, sizeof(sequence), 0);
		send(sockfd, &total_frame_expected, sizeof(sequence), 0);
		send(sockfd, &total_frame_expected, sizeof(sequence), 0);
		send(sockfd, &total_frame_expected, sizeof(sequence), 0);
		send(sockfd, &total_frame_expected, sizeof(sequence), 0);
		send(sockfd, &total_frame_expected, sizeof(sequence), 0);
		// sendto(sockfd, &total_frame_expected, sizeof(sequence), 0, (struct sockaddr *)&dst, sizeof(dst));
		// sendto(sockfd, &total_frame_expected, sizeof(sequence), 0, (struct sockaddr *)&dst, sizeof(dst));
		// sendto(sockfd, &total_frame_expected, sizeof(sequence), 0, (struct sockaddr *)&dst, sizeof(dst));
		// sendto(sockfd, &total_frame_expected, sizeof(sequence), 0, (struct sockaddr *)&dst, sizeof(dst));
		// sendto(sockfd, &total_frame_expected, sizeof(sequence), 0, (struct sockaddr *)&dst, sizeof(dst));
		// sendto(sockfd, &total_frame_expected, sizeof(sequence), 0, (struct sockaddr *)&dst, sizeof(dst));
		// sendto(sockfd, &total_frame_expected, sizeof(sequence), 0, (struct sockaddr *)&dst, sizeof(dst));
#ifdef DEBUG
		fprintf(stderr, "%u(sent) %u(expected)\n", total_frame_sent, total_frame_expected);
#endif
		exit(EXIT_SUCCESS);
	}

	void read_packet() {
		packet* mp;
		while (bytes_read < bytes_to_transmit) {
			std::unique_lock<std::mutex> lk(m);
			cv.wait(lk, [this]{
				if (!buffered.empty()) {
					return curr_frame - buffered.begin()->first < RECV_WINDOW;
				} 
				return true;
			});
			// cv.wait(lk, [this]{ return });
			fill_single_buffer(mp);
			lk.unlock();
		}

		// fill_final_buffer(mp);
	}

	void recv_packet() const {
		sequence recvack;
		while (true) {
			if (recv(sockfd, (void*)&recvack, sizeof(sequence), 0) > -1) {
#ifdef DEBUG
				printf("RECV bytes ACK %d \n", recvack);
#endif
				// if (recvlen == -1) {
				// 	printf("recv: %s\n", strerror(errno));
				// }

				auto it = buffered.find(recvack);
				if (it != buffered.end()) {
					bytes_transmitted += it->second->len;
					idle.push(it->second);
					buffered.erase(it);
					cv.notify_one();
				}
			}
		}
	}

	inline void init_fds(const char* f) {
		infd = open(f, O_RDONLY);
		if (infd == -1) {
			perror("open");
			exit(EXIT_FAILURE);
		}
	}

	inline void init_workers() {
		for (int i=0; i<SEND_WINDOW; ++i) {
			idle.push(&packets[i]);
		}
		reader = std::thread([this] {read_packet();});
		sender = std::thread([this] {send_packet();});
		recver = std::thread([this] {recv_packet();});
	}

	inline void fill_single_buffer(packet* &mp) {
		idle.pull(mp);
		mp->seq = (curr_frame ++);
		if (curr_frame == total_frame_expected) mp->fin = 1;
		// fprintf(stderr, "server reads: %u\n", curr_frame);
		
		uint64_t to_read = std::min(FRAME_SIZE, bytes_to_transmit - bytes_read);
		bytes_read += to_read;

		read(infd, mp->buf, to_read);
		mp->len = to_read;

		// used.push(mp);
		buffered[mp->seq] = mp;
	}

	inline void fill_final_buffer(packet* &mp) {
		idle.pull(mp);
		mp->seq = (curr_frame ++);
		mp->len = 0;
	}

} nonstop_sender;

int main(int argc, char** argv)
{
#ifdef DEBUG
	printf("FRAME SIZE: %lu\n", FRAME_SIZE);
#endif

	nonstop_sender s(argc, argv);
} 
