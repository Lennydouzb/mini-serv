
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef struct client
{
  int id;
  char *buf;
} t_client;

void  fatal()
{
  write(2, "Fatal error\n", 12);
  exit(1);
}

void  broadcast(int client_fd, fd_set fds, int maxfd, char *msg)
{
  size_t i = 0;
  while (i <= maxfd)
  {
    if (FD_ISSET(i, &fds) && i != client_fd)
    {
      send(i, msg, strlen(msg), 0);
    }
    ++i;
  }
}

void  clean(int max_fd, t_client *clients, fd_set active_fds)
{
  for (size_t aFd = 0; aFd <= max_fd; aFd++)
  {
    if (FD_ISSET(aFd, &active_fds))
    {
      FD_CLR(aFd, &active_fds);
      close(aFd);
    }
    if (clients[aFd].buf)
      free(clients[aFd].buf);
  }
}
void  wrongArgs()
{
  write(2, "Wrong number of arguments\n", 26);
  exit(1); 
}

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

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


int main(int ac,  char **av) 
{
  if (ac != 2)
    wrongArgs();

	int sockfd, client_fd, maxfd, id;
  unsigned int len;
	struct sockaddr_in servaddr, cli; 

  	// socket create and verification 
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1) { fatal(); } 
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
  const int CSIZE = 16*4096;
  t_client clients[CSIZE];

  fd_set active_fds, read_fd, write_fd;
  FD_ZERO(&active_fds);
  FD_SET(sockfd, &active_fds);
  maxfd = sockfd;
  id = 0;

  while (1)
  {
    read_fd = write_fd = active_fds;
    int active = select(maxfd + 1, &read_fd, &write_fd, NULL, NULL);
    if (active == -1)
    {
      clean(maxfd, clients, active_fds);
      fatal();
    }
    if (FD_ISSET(sockfd, &read_fd))
    {
      client_fd = accept(sockfd, (struct sockaddr*)&cli, &len);
      if (client_fd >= 0)
      {
        clients[client_fd].id = id++;
        clients[client_fd].buf = NULL;
        FD_SET(client_fd, &active_fds);
        if (client_fd > maxfd) maxfd = client_fd;
        char m[MSIZE];
        bzero(m, MSIZE);
        sprintf(m, "server: client %d just arrived\n", clients[client_fd].id);
        broadcast(client_fd, write_fd, maxfd, m);
      }
    }
    else
    {
        for (client_fd = 0; client_fd <= maxfd; client_fd++)
      {
        if (FD_ISSET(client_fd, &read_fd))
        {
          char buff[BSIZE];
          bzero(buff, BSIZE);
          int bytes = recv(client_fd, buff, BSIZE - 1, 0);
          if (bytes <= 0)
          {
            char m[MSIZE];
            bzero(m, MSIZE);
            sprintf(m, "server: client %d just left\n", clients[client_fd].id);
            broadcast(client_fd, write_fd, maxfd, m);
            FD_CLR(client_fd, &active_fds);
            close(client_fd);
            if (clients[client_fd].buf)
            {
              free(clients[client_fd].buf);
              clients[client_fd].buf = NULL;
            }
          }
          else
          {
            char *line = NULL;
            clients[client_fd].buf = str_join(clients[client_fd].buf, buff);
            while(extract_message(&(clients[client_fd].buf), &(line)))
            {
              char m[MSIZE + strlen(line)];
              bzero(m, MSIZE + strlen(line));
              sprintf(m, "client %d: %s", clients[client_fd].id, line);
              broadcast(client_fd, write_fd, maxfd, m);
              free(line);
              line = NULL;
            }
            if (clients[client_fd].buf && clients[client_fd].buf[0] == '\0')
            {
              free (clients[client_fd].buf);
              clients[client_fd].buf = NULL;
            }
          }
        }
      }
    }
  }
  clean(maxfd, clients, active_fds);
}
