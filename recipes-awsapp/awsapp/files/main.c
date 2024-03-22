/*
 * socket read/write program for AWS IoT Core (mqtt5_pubsub)
 *
 * [usage]
 * 1) Send message to AWS IoT core
 *    awsapp send <socket_name> <file>
 *    ex)
 *     awsapp send /tmp/aws_socket_rmsg ./message.txt
 *    description)
 *      <socket_name> : socket name to send message.
 *      <file>        : MQTT message file to send
 *                      format:
 *                      1   | <topic>\n
 *                      2...| <message>
 * 
 * 2) Receive message from AWS IoT Core
 *     awsapp recv <socket_name> <number>
 *     ex)
 *      awsapp recv /tmp/aws_socket_smsg  10000
 *     description)
 *      <socket_name> : socket name to receive message.
 *      <number>      : number of receiving message.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/un.h>

char *file_read(char *fname)
{
	struct stat file;
	int fd, nread;
	char *buff;

	if(stat(fname, &file)) {
		return NULL;
	}

	buff = malloc(file.st_size);
	if(!buff) {
		return NULL;
	}

	fd = open(fname, O_RDONLY);
	if(fd < 0) {
		free(buff);
		return NULL;
	}
	nread = read(fd, buff, file.st_size);
	if(nread != file.st_size) {
		close(fd);
		free(buff);
		return NULL;
	}
	close(fd);
	
	return buff;   /* filesize is not return */
}

#if 0
int file_write(char *fname, char *data, int len)
{
	int fd, nwrite;

	unlink(fname);

	fd = open(fname, O_WRONLY);
	if(fd < 0) {
		return -1;
	}
	nwrite = write(fd, data, len);
	if(nwrite != len) {
		close(fd);
		return -2;
	}
	close(fd);
	return 0;
}
#endif

void send_message(char *socket_name, char *message)
{
	int client_sock;
	struct sockaddr_un server_addr;
	memset(&server_addr, 0, sizeof(server_addr));

	client_sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (client_sock < 0) {
		fprintf(stdout, "socket() failed(%s).\n", strerror(errno));
		return;
	}

	server_addr.sun_family = AF_UNIX;
	strncpy(server_addr.sun_path, socket_name, sizeof(server_addr.sun_path)-1);
	if (connect(client_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0){
	    fprintf(stdout, "connect() failed(%s).\n", strerror(errno));
		close(client_sock);
		return;
	}

	int len   = strlen(message);
	int nsend = send(client_sock, message, len, 0);
	if(nsend == len) {
		fprintf(stdout, "send message to %s.\n", socket_name);
	} else {
		fprintf(stdout, "send() failed(%d byte: %s).\n", nsend, strerror(errno));
	}
	close(client_sock);
}

void execute_recv_process(char *message, int len)
{
	/*
	 * Do Something ...
	 */
	fprintf(stdout, "Receive %d Byte\n%s\n", len, message);
}

void recv_message(char *socket_name, int maxloop)
{
	int server_sock;
	struct sockaddr_un server_addr;

	unlink(socket_name);
	memset(&server_addr, 0, sizeof(server_addr));

	server_sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (server_sock < 0) {
		fprintf(stdout, "socket() failed(%s).\n", strerror(errno));
		return;
	}
	server_addr.sun_family = AF_UNIX;
	strncpy(server_addr.sun_path, socket_name, sizeof(server_addr.sun_path)-1);
	if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)	{
		fprintf(stdout, "bind() failed(%s).\n", strerror(errno));
		exit(-1);
	}
	if (listen(server_sock, 5))	{
		fprintf(stdout, "listen() failed(%s).\n", strerror(errno));
		exit(-1);
	}

	for(int loop=0; loop<maxloop; loop++) {
		int client_sock;
		struct sockaddr_un client_addr;
		socklen_t len = sizeof(client_addr);
		char buffer[1024];
		int nrecv;

		fprintf(stdout, "wait message...\n");
		memset(&client_addr, 0, sizeof(client_addr));
		client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &len);
		if (client_sock < 0)
		{
			fprintf(stdout, "accept() failed(%s).\n", strerror(errno));
			continue;
		}

		nrecv = recv(client_sock, buffer, sizeof(buffer)-1, 0);
		if (nrecv < 0) {
			fprintf(stdout, "recv() failed(%s).\n", strerror(errno));
			close(client_sock);
			continue;
		}
		buffer[nrecv] = '\0';
		close(client_sock);

		/*
		 * This program does not receive next message
		 * until execute_recv_process() is finished.
		 */
		execute_recv_process(buffer, nrecv);
	}
	close(server_sock);
}

int main(int argc, char *argv[])
{
	char *command, *socket_name;

	if(argc != 4) {
		fprintf(stderr, "[usage]\n" \
		 				"awsapp send <socket_name> <message-file>\n" \
						"awsapp recv <socket_name> <number-of-receiving messages>\n" \
						"\n" \
						"<message-file>: \n" \
						"   line 1 is published topic, and line 2..n are message\n" \
						"\n" \
						"ex)\n" \
						"awsapp send /tmp/aws_socket_smsg ./message.txt\n" \
						"awsapp recv /tmp/aws_socket_rmsg  10000\n");
		exit(1);
	}

	command = argv[1];
	socket_name = argv[2];

	if(strcmp(command, "send") == 0) {
		char *file_name   = argv[3];
		char *message = file_read(file_name);
		if(message) {
			send_message(socket_name, message);
			free(message);
		} else {
			fprintf(stderr, "sending file not found\n...\n");
		}
	} else if(strcmp(command, "recv") == 0) {
		int maxloop  = (int)strtol(argv[3], NULL, 10);
		recv_message(socket_name, maxloop);
	} else {
		fprintf(stderr, "command not found.\n");
		exit(1);
	}
	return 0;
}
