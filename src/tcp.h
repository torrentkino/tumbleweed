/*
Copyright 2010 Aiko Barz

This file is part of masala/tumbleweed.

masala/tumbleweed is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

masala/tumbleweed is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with masala/tumbleweed.  If not, see <http://www.gnu.org/licenses/>.
*/

#define TCP_MAX_EVENTS 32
#define TCP_INPUT 0
#define TCP_OUTPUT 1

struct obj_tcp {
	/* Socket data */
	IP s_addr;
	socklen_t s_addrlen;
	int sockfd;
	int optval;

	/* Epoll */
	int epollfd;

	/* Locking worker index */
	pthread_mutex_t *mutex;
	int id;

	/* Workers active */
	int active;

	/* Worker */
	pthread_t **threads;
	pthread_attr_t attr;
};

struct obj_tcp *tcp_init( void );
void tcp_free( void );

void tcp_start( void );
void tcp_stop( void );

int tcp_nonblocking( int sock );
void tcp_event( void );
void tcp_cron( void );

void tcp_pool( void );
void *tcp_thread( void *arg );
void tcp_worker( struct epoll_event *events, int nfds, int thrd_id );

void tcp_newconn( void );
void tcp_output( ITEM *listItem );
void tcp_input( ITEM *listItem );
void tcp_gate( ITEM *listItem );
void tcp_rearm( ITEM *listItem, int mode );

void tcp_buffer( NODE *nodeItem, char *buffer, ssize_t bytes );
