#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef struct s_client
{
	int	id;
	char *m;

} t_client;
void	fatal()
{
	write(2, "Fatal error\n", 12);
	exit(1);
}

void broadcast(int client_fd, int max_fd, char* str, fd_set write_set)
{
	for (int actual_fd = 0; actual_fd <= max_fd; ++actual_fd)
	{
		if (actual_fd != client_fd)
		{
			if (FD_ISSET(actual_fd, &write_set))
			{
				send(actual_fd, str, strlen(str), 0);
			}
		}
	}
}

void clean(int max_fd, t_client *clients, fd_set active_set)
{
	for (int fd = 0; fd <= max_fd; ++fd)
	{
		if (FD_ISSET(fd, &active_set))
		{
			FD_CLR(fd, &active_set);
			close(fd);
		}
		if (clients[fd].m)
		{
			free(clients[fd].m);
			clients[fd].m = NULL;
		}
	}	
}

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int		i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}


int main(int ac, char **av) {
	if (ac != 2)
	{
		write (2, "Wrong number of arguments\n", 26);
		return 1;
	}
	int sockfd, connfd, client_fd, id, max_fd;
	unsigned int len;
	struct sockaddr_in servaddr, cli; 

	// socket create and verification 
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1) { 
		fatal();
	} 
	bzero(&servaddr, sizeof(servaddr)); 

	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(av[1])); 

	// Binding newly created socket to given IP and verification 
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) { 
		fatal();

	} 
	if (listen(sockfd, 10) != 0) {
		fatal();
	}
	len = sizeof(cli);
	const int BSIZE = 1024;
	const int MSIZE = 64;
	const int CSIZE = 16 * 4096;
	t_client clients[CSIZE];
	bzero(clients, sizeof(clients));
	fd_set write_set, read_set, active_set;
	FD_ZERO(&active_set);
	FD_SET(sockfd, &active_set);
	max_fd = sockfd;
	id = 0;
	while (1)
	{
		read_set = write_set = active_set;
		int active = select(max_fd + 1, &read_set, &write_set, NULL, NULL);
		if (active < 0)
		{
			clean(max_fd, clients, active_set);
			fatal();
		}
		if (FD_ISSET(sockfd, &read_set))
		{
			client_fd = accept(sockfd, (struct sockaddr *)&cli, &len);
			if (client_fd >= 0)
			{
				clients[client_fd].id = id++;
				FD_SET(client_fd, &active_set);
				if (client_fd > max_fd) max_fd = client_fd;
				char m[MSIZE];
				bzero(m, MSIZE);
				sprintf(m, "server: client %d just arrived\n", clients[client_fd].id);
				broadcast(client_fd, max_fd, m, write_set);
			}
		}
		else
		{
			for (int actual_fd = 0; actual_fd <= max_fd; actual_fd++)
			{
				if (FD_ISSET(actual_fd, &read_set))
				{
					char buff[BSIZE];
					bzero(buff, BSIZE);
					int size_read = recv(actual_fd, buff, BSIZE - 1, 0);
					if (size_read <= 0)
					{
						char m[MSIZE];
						bzero(m, MSIZE);
						sprintf(m, "server: client %d just left\n", clients[actual_fd].id);
						broadcast(actual_fd, max_fd, m, write_set);
						FD_CLR(actual_fd, &active_set);
						close(actual_fd);
						if (clients[actual_fd].m)
						{
							free(clients[actual_fd].m);
							clients[actual_fd].m = NULL;
						}

					}
					else
					{
						char *line = NULL;
						clients[actual_fd].m = str_join(clients[actual_fd].m , buff);
						while (extract_message(&(clients[actual_fd].m) , &line))
						{
							char m[MSIZE + strlen(line)];
							bzero(m, MSIZE + strlen(line));
							sprintf(m, "client %d: %s", clients[actual_fd].id, line);
							broadcast(actual_fd, max_fd, m, write_set);
							free(line);
							line = NULL;
						}
						if (clients[actual_fd].m && clients[actual_fd].m[0] == '\0')
						{
							free (clients[actual_fd].m);
							clients[actual_fd].m = NULL;
						}

					}
				}
			}
		}
	}
	clean(max_fd, clients, active_set);
}
