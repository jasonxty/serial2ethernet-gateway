#include "csapp.h"

#define FALSE 0
#define TRUE  1

typedef struct {
	char *cfi;
	speed_t bautrate;
	char *mcastaddr;
	int port;
} conf;

typedef struct {
	int fd;
	conf thconf;
	unsigned int port; /* just indicate which port is in use for user*/
} thpara;

int fd[2];

pthread_t threads[10];

char buff[101];

static struct termios newtios,oldtios; /*termianal settings */
static int saved_portfd=-1;            /*serial port fd */
conf thconf1 = { "GP0001", B4800, "239.192.0.4", 60004};
conf thconf2 = { "AG0002", B9600, "239.192.0.4", 60004}; 
conf thconf3 = { "AI0003", B9600, "239.192.0.2", 60002};
conf thconf4 = { "WI0003", B4800, "239.192.0.4", 60004};

static void reset_tty_atexit(void)
{
	if(saved_portfd != -1)
	{
		tcsetattr(saved_portfd,TCSANOW,&oldtios); /* TCSANOW means occurs immediately */ 
	} 
}

/*cheanup signal handler */
static void reset_tty_handler(int signal)
{
	if(saved_portfd != -1)
	{
		tcsetattr(saved_portfd,TCSANOW,&oldtios);
	}
	_exit(EXIT_FAILURE);
}

static int open_port(const char *portname, speed_t bt)
{
	struct sigaction sa;
	int portfd;

	printf("opening serial port:%s\n",portname);
	/*open serial port */
	if((portfd=open(portname,O_RDONLY | O_NOCTTY)) < 0 ) /* O_NOCTTY means do not set the portname to be 
	                                                      controlling terminal. */
	{
   		printf("open serial port %s fail \n ",portname);
   		return portfd;
	}

	/*get serial port parnms,save away */
	tcgetattr(portfd,&newtios);
	//memcpy(&oldtios,&newtios,sizeof newtios); the origin's readability is bad
	oldtios = newtios;
	/* configure new values */
	cfmakeraw(&newtios); /*see man page */
	/* 
	 * noncanonical mode
	 * disable echo
	 * disable signal trigger
	 * */
	newtios.c_lflag &= ~(ICANON | ECHO | ISIG);
	newtios.c_iflag &= ~(IXON | IXOFF | IXANY);
	/*
	 * map NL to CR-NL, map lowercase to uppercase on output,
	 * map CR to NL on output, no CR output at column 0,
	 * NL performs CR function, use fill character for delay
	 */
	newtios.c_oflag &= ~(OPOST | ONLCR | OLCUC | OCRNL | ONOCR | ONLRET | OFILL); 
	/*
	 * The device is directly attached. Enable receiver.
	 */
	newtios.c_cflag = CS8 | CLOCAL | CREAD;
	/*
	 * Disable auto-flow control, no parity generation and checking
	 * one stop bit
	 */
	newtios.c_cflag &= ~(CRTSCTS | PARENB | CSTOPB);
	newtios.c_cc[VMIN]=0; /* block until 1 char received */
	newtios.c_cc[VTIME]=10; /*no inter-character timer, _POSIX_VDISABLE is 0 */
	/* 115200 bps */
	cfsetospeed(&newtios, bt);
	cfsetispeed(&newtios, bt);
	/* register cleanup stuff */
	atexit(reset_tty_atexit);
	memset(&sa,0,sizeof sa);
	sa.sa_handler = reset_tty_handler;
	sigaction(SIGHUP,&sa,NULL);
	sigaction(SIGINT,&sa,NULL);
	sigaction(SIGPIPE,&sa,NULL);
	sigaction(SIGTERM,&sa,NULL);
	/*apply modified termios */
	saved_portfd=portfd;
	/*
	 * flush buffer
	 */
	tcflush(portfd,TCIFLUSH);
	/*
	 * discarded all input data that has not been read;
	 * the change occurs after all output has been transmitted.
	 */
	tcsetattr(portfd,TCSADRAIN,&newtios);
	return portfd;
}

char *makeframe(char *str) {
	int framelen = 8;
	int loadlen;
	loadlen = strlen(str);
	char *restr;
	restr = Malloc(loadlen + 8); /* loadlen + 6 + 2 */
	char head[] = "UdPbC\0";
	int i;
	for (i = 0; i < 6; i++) {
		restr[i] = head[i];
	}
	for (; i < loadlen + 6; i++) {
		restr[i] = str[i-6];
	}
	restr[i] = '\r';
	restr[i+1] = '\n';
	return(restr);
}

void * process(void* arg)
{
	thpara *propara = (thpara *)arg;
	int portfd = propara->fd;
	int rev1, rev2;
	char RxBuffer[500];
	char bBuffer[500];
	char tag[600];
	char *datagram;
	char tagg[20];
	char tags[20];
	char tagn[20];
	int line, tline;
	char *ptr1, *ptr2;
	char checksum[3];
	int i;
	char check = 0;
	struct in_addr localInterface;
	struct sockaddr_in groupSock;
	int sd, datalen;
	rev1 = 0;
	rev2 = 0;
	int groupn = 1;
	int linen = 0;

	conf myconf;
	myconf = propara->thconf;

	printf("trying to reading port:%d\n", propara->port);
	/* Create UDP socket and set socket */
	sd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sd < 0) {
		perror("Opening datagram socket error");
		exit(1);
	} else {
//	        printf("Opening the datagram socket ... OK. \n");
	}
	memset((char *) &groupSock, 0, sizeof(groupSock));
	groupSock.sin_family = AF_INET;
	groupSock.sin_addr.s_addr = inet_addr(myconf.mcastaddr);
	groupSock.sin_port = htons(myconf.port);
	localInterface.s_addr = inet_addr("192.168.1.221");
	if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_IF, (char *)& \
				localInterface, sizeof(localInterface)) < 0) {
		perror("Setting local interface error");
		exit(1);
	} else {
//		printf("Setting the local interface ... OK\n");
	}

	while(1) {
		/* read a sentence  from serial port */
		while(RxBuffer[rev2 - rev1] != '\n') {
			rev1 = read(portfd, (RxBuffer+rev2), 1);
			rev2 += rev1;
		}
		RxBuffer[rev2-rev1-1] = '\0'; /* remove '\r' and '\n' */
		/* add sentence checksum*/
		if (!strchr(RxBuffer, '*')) {
			check = RxBuffer[0];
			for (i = 1; i < strlen(RxBuffer)-1; i++) {
				check ^= RxBuffer[i];
			}
			sprintf(checksum, "%x", check);
			strcat(RxBuffer, "*");
			strcat(RxBuffer, checksum);
		}
		
		/* make g tag */
		strcpy(tag, "\\");
		if (strstr(RxBuffer, "VDM")) {   /* check whether it is a VDM sentence */	
			ptr1 = strchr(RxBuffer, ',');
			strcpy(bBuffer, ptr1+1);
			ptr2 = strchr(bBuffer, ',');
			*ptr2 = '\0';
			/* get totoal number of sentence from the sentence */
			tline = atoi(bBuffer);
			strcpy(bBuffer, ptr2+1);
			ptr1 = strchr(bBuffer, ',');
			*ptr1 = '\0';
			/* get the number of sentence form the sentence */
			line = atoi(bBuffer);
			/* ensure group code range 0-99 */
			if (groupn == 100)
				groupn = 1;
			/* make g tag */
			sprintf(tagg, "g:%d-%d-%d,", line, tline, groupn);
			if (line == tline)
				groupn++;
		        memset(bBuffer, '\0', sizeof(char)*500);
		        strcat(tag, tagg);
		}
		/* make s tag */
		sprintf(tags, "s:%s,", myconf.cfi);
		linen++;
		/* ensure line number range 1-999 */
		if (linen == 1000)
			linen = 1;
		/* make n tag */
		sprintf(tagn, "n:%d", linen);
		strcat(tag, tags);
		strcat(tag, tagn);
		/* calculate tag checksum */
		check = tag[1];
		for (i = 2; i < strlen(tag); i++) {
			check ^= tag[i];
		}
		/* add tag checksum */
		strcat(tag, "*");
		sprintf(checksum, "%x", check);
		strcat(tag, checksum);
		strcat(tag, "\\");
		/* add tag block*/
		strcat(tag, RxBuffer);
		printf("%s\n", RxBuffer);
		/* make IEC61162-450 frame */
		datagram = makeframe(tag);	
		datalen = strlen(tag) + 8;
		printf("before last:%d\n", datagram[datalen-2]);
		printf("last:%d\n", datagram[datalen-1]);
		if (sendto(sd, datagram, datalen, 0, (struct sockaddr*)&groupSock, \
					sizeof(groupSock)) < 0) {
			perror("Sending datagram message error");
		} else {
	//		printf("Sending datagram message...OK\n");
		}
		
		memset(RxBuffer, '\0', sizeof(char)*500);
		memset(tag, '\0', sizeof(char)*500);
		memset(tagg, '\0', sizeof(char)*20);
		memset(tags, '\0', sizeof(char)*20);
		memset(tagn, '\0', sizeof(char)*20);
		Free(datagram);
		rev1 = 0;
		rev2 = 0;
		fflush(stdout);
	}
}	
int main(int argc, char **argv)
{
	/* 
	 * define the use of 4 serial port
	 * GP means GPS
	 * AG means gyrocompass
	 * AI means AIS
	 * WI means weather instrument
	 */
	char *eq[4] = {"GP", "AG", "AI", "WI"};
	char *dev[4]={"/dev/ttyS6", "/dev/ttyS8", "/dev/ttyS1", "/dev/ttyS4"};
	unsigned int i, j;
	int flag = 0;
	thpara *para;
	
	/* check usage of the program */
	if (argc != 5) {
		printf("wrong use of serial2ethernet\n");
		exit(1);
	}
	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++) {
			if (!strcmp(argv[i+1], eq[j]))
				flag++;
		}
	}
	if (flag != 4) {
		printf("wrong use of serial2ehternet\n");
		exit(1);
	}

	/* open up 4 threads to handle 4 serial port */
	for(i = 0; i < 4; i++) {
		para = Malloc(sizeof(thpara));
		if (!strcmp(argv[i+1], "GP"))
			para->thconf = thconf1;
		if (!strcmp(argv[i+1], "AG"))
			para->thconf = thconf2;
		if (!strcmp(argv[i+1], "AI"))
			para->thconf = thconf3;
		if (!strcmp(argv[i+1], "WI"))
			para->thconf = thconf4;
		if((fd[i] = open_port(dev[i], para->thconf.bautrate))<0)
   			return -1;
		para->fd = fd[i];
		para->port = i;
	        pthread_create(&threads[i], NULL, process, (void*)(para));
	}

	for (i = 0; i < 4; i++) {
		pthread_join(threads[i], NULL);
	}
	
       	return 0;
}
