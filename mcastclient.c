/*************************************************************************
    > File Name: mcastclient.c
    > Author: xtydtc
    > Mail: 172713431@qq.com 
    > Created Time: Tue 27 Jun 2017 17:43:50 CST
 ************************************************************************/
/*
 * 1. Create an AF_INET, SOCK_DGRAM type socket.
 * 2. Set the SO_REUSEADDR option to allow multiple applications to
 * receive datagrams that are destined to the same local port number.
 * 3. Use the bind() verb to specify the local port number. Specify 
 * the IP address as INADDR_ANY in order to receive datagrams that are 
 * addressed to a multicast group.
 * 4. Use the IP_ADD_MEMBERSHIP socket option to join the multicast group 
 * that receives the datagrams. When joining a group, specify the class D
 * group address along with the IP address of a local interface. The system 
 * must call the IP_ADD_MEMBERSHIP socket option for each local interface 
 * receiving the multicast datagrams.
 * 5. Receive the datagram.
 */

/* Receiver/client multicast Datagram example. */
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct sockaddr_in localSock;
struct ip_mreq group;
int sd;
int rev;
int datalen;
char databuf[1024];

int main(int argc, char *argv[])
{
/* Create datagram socket on which to receive. */
	sd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sd < 0) {
		perror("Opening datagram socket error");
		exit(1);
	} else {
		printf("Opening datagram socket...OK.\n");
	}

	/* 
	 * Enable SO_REUSEADDR to allow multiple instances of this 
	 * application to receive copies of the multicast datagrams.
	 */
	int reuse = 1;
	if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse,
				sizeof(reuse)) < 0) {
		perror("Setting SO_REUSEADDR error");
		close(sd);
		exit(1);
	} else {
		printf("Setting SO_REUSEADDR...OK.\n");
	}

	/* 
	 * Bind to the proper port number with the IP address
	 * specified as INADDR_ANY.
	 */
	memset((char *) &localSock, 0, sizeof(localSock));
	localSock.sin_family = AF_INET;
	localSock.sin_port = htons(60004);
	localSock.sin_addr.s_addr = INADDR_ANY;
	if (bind(sd, (struct sockaddr*)&localSock, sizeof(localSock))) {
		perror("Binding datagram socket error");
		close(sd);
		exit(1);
	} else {
		printf("Binding datagram socket...OK.\n");
	}

	/*
	 * Join the multicast group 226.1.1.1 on the local
	 * 192.168.93.128 interface. Note that this IP_ADD_MEMBERSHIP
	 * option must be called for each local interface over which 
	 * the multicast datagrams are to be received.
	 */
	group.imr_multiaddr.s_addr = inet_addr("239.192.0.4");
	group.imr_interface.s_addr = inet_addr("192.168.1.189");
	if (setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group,
				sizeof(group)) < 0) {
		perror("Adding multicast group error");
		close(sd);
		exit(1);
	} else {
		printf("Adding multicast group...OK.\n");
	}

	/* Read from the socket. */
	datalen = sizeof(databuf);
	while(1) {
		if ((rev = read(sd, databuf, datalen)) < 0) {
			perror("Reading datagram message error.");
			close(sd);
			exit(1);
		} else {
			databuf[rev] = '\0';
			if (databuf[5] == '\0')
				databuf[5] = '0';
			else 
				printf("wrong header\n");
			
			printf("%s", databuf);
			printf("before last:%d\n", databuf[rev-2]);
			printf("last:%d\n", databuf[rev-1]);
		}

	}
	return 0;
}

