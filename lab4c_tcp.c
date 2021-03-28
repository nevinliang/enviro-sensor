// NAME Nevin Liang
// EMAIL: nliang868@g.ucla.edu
// ID: 705575353

#include <signal.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/time.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>

#ifdef DUMMY
#define MRAA_GPIO_IN 0
#define MRAA_GPIO_EDGE_RISING 0
typedef int mraa_gpio_context;
typedef int mraa_aio_context;

mraa_aio_context mraa_aio_init(int p) {p++; return 0;}
mraa_aio_context mraa_aio_read(mraa_aio_context c) {c++; return 650;}
void mraa_aio_close(mraa_aio_context c) {c++;}

mraa_gpio_context mraa_gpio_init(int p) {p++; return 0;}
int mraa_gpio_read(mraa_gpio_context c) {c++; return 0;}
void mraa_gpio_dir(mraa_gpio_context c, int d) {c++; d++;}
void mraa_gpio_close(mraa_gpio_context c) {c++;}
void mraa_gpio_isr(mraa_gpio_context c, int d, void(*f)(), void *a) {
	c++; d++; if (f == NULL || a == NULL) {}
}

#else
#include <mraa/gpio.h>
#include <mraa/aio.h>
#endif

#define B 4275
#define R0 100000.0

int period = 1, logger = -1, gen_reports = 1, id = -1, port = -1;
int sockfd;
char scale = 'F', buffer[256] = {0}, outbuf[256] = {0};
char *filename = NULL, *host = NULL;

float convert_temper_reading(int reading) {
	float R = (1023.0 / ((float) reading) - 1.0) * R0;
	float C = 1.0 / (log(R / R0) / B + 1 / 298.15) - 273.15;
	float F = C * 9 / 5 + 32;
	return (scale == 'F') ? F : C;
}

void printall(float temp) {
	struct timespec ts;
	struct tm* tm;
	clock_gettime(CLOCK_REALTIME, &ts);
	tm = localtime(&(ts.tv_sec));

	if (temp != -1) {
		dprintf(sockfd, "%02d:%02d:%02d %.1f\n", tm->tm_hour, tm->tm_min, tm->tm_sec, temp);
		if (filename != NULL)
			dprintf(logger, "%02d:%02d:%02d %.1f\n", tm->tm_hour, tm->tm_min, tm->tm_sec, temp);
	}
	else {
		dprintf(sockfd, "%02d:%02d:%02d SHUTDOWN\n", tm->tm_hour, tm->tm_min, tm->tm_sec);
		if (filename != NULL)
			dprintf(logger, "%02d:%02d:%02d SHUTDOWN\n", tm->tm_hour, tm->tm_min, tm->tm_sec);
	}
}

void process_args(int argc, char* argv[]) {
	static struct option opts[] = {
        {"period", 	required_argument, 0, 'p'},
        {"scale", 	required_argument, 0, 's'},
        {"log", 	required_argument, 0, 'l'},
        {"id",		required_argument, 0, 'i'},
        {"host",	required_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    int opt_it = 0;
    while (1) {
        int opt = getopt_long(argc, argv, "p:s:l:", opts, &opt_it);
        if (opt == -1) break;
        else if (opt == 'p') period = atoi(optarg);
        else if (opt == 's') scale = optarg[0];
        else if (opt == 'l') filename = optarg;
        else if (opt == 'h') host = optarg;
        else if (opt == 'i') id = atoi(optarg);
        else {
        	fprintf(stderr, "usage: ./lab4 [--period=#] [--scale=F/C] \
        					[--log=filename] [--id=#########] [--host=hostname] \
        					[--port=#]\n");
        	exit(1);
        }
    }
    if (optind < argc)
    	port = atoi(argv[optind]);
    else {
		fprintf(stderr, "port number mandatory!\n");
		exit(1);
	}
    	
}

int process_inp(int len) {
	int i, j;
	for (i = 0; i < len; i++) {
		while (i < len && (buffer[i] == ' ' || buffer[i] == '\t')) i++;
		j = i;
		while (j < len && buffer[j] != '\n') j++;
		if (j == len) return 0;
		if (filename != NULL)
			write(logger, &buffer[i], j - i + 1);
		buffer[j] = '\0';
		char* block = buffer + i;
		if (strncmp(block, "SCALE=F", 7) == 0) scale = 'F';
		else if (strncmp(block, "SCALE=C", 7) == 0) scale = 'C';
		else if (strncmp(block, "PERIOD=", 7) == 0) period = atoi(block += 7);
		else if (strncmp(block, "LOG", 3) == 0) {}
		else if (strncmp(block, "OFF", 3) == 0) return 1;
		else if (strncmp(block, "STOP", 4) == 0) gen_reports = 0;
		else if (strncmp(block, "START", 5) == 0) gen_reports = 1;
		i = j;
	}
	return 0;
}

void client_connect(char *host_name, unsigned int port) {
	struct sockaddr_in serv_addr;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	struct hostent *server = gethostbyname(host_name);
    memset(&serv_addr, 0, sizeof(struct sockaddr_in));
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_port = htons(port);
    connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
}

int main(int argc, char* argv[]) {
	process_args(argc, argv);
	mraa_aio_context tsensor = mraa_aio_init(1);
	
	if (filename != NULL)
		logger = open(filename, O_WRONLY | O_CREAT | O_APPEND, S_IRWXU);
	client_connect(host, port);
	
    dprintf(sockfd, "ID=%d\n", id);
	if (filename != NULL)
    	dprintf(logger, "ID=%d\n", id);

	struct timeval tv;
	time_t last_read = -1000;
	struct pollfd pollsock = {sockfd, POLLIN, 0};
	while (1) {
		gettimeofday(&tv, NULL);
		time_t cur_read = tv.tv_sec;

		if (poll(&pollsock, 1, 0) == 1 && ((pollsock.revents & POLLIN) == POLLIN))
			if (process_inp(read(sockfd, buffer, 256)))
				break;

		if (cur_read - last_read >= period && gen_reports) {
			last_read = cur_read;
			int t_ret = mraa_aio_read(tsensor);
			float temp = convert_temper_reading(t_ret);
			printall(temp);
		}
	}
	printall(-1);
	if (filename != NULL)
		close(logger);
	close(sockfd);
	mraa_aio_close(tsensor);
	return 0;
}

