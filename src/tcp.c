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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/resource.h>

#include "malloc.h"
#include "thrd.h"
#include "main.h"
#include "str.h"
#include "list.h"
#include "node_web.h"
#include "log.h"
#include "conf.h"
#include "file.h"
#include "send_web.h"
#include "hash.h"
#include "mime.h"
#include "tcp.h"
#include "unix.h"
#include "http.h"

struct obj_tcp *tcp_init( void ) {
	struct obj_tcp *tcp = (struct obj_tcp *) myalloc( sizeof(struct obj_tcp), "tcp_init" );

	/* Init server structure */
	tcp->s_addrlen = sizeof( IP );
	memset( (char *) &tcp->s_addr, '\0', tcp->s_addrlen );
	tcp->sockfd = -1;
	tcp->optval = 1;

	/* Worker Concurrency */
	tcp->mutex = mutex_init();

	/* Workers running */
	tcp->id = 0;

	/* Workers active */
	tcp->active = 0;

	/* Worker */
	tcp->threads = NULL;

	return tcp;
}

void tcp_free( void ) {
	if( _main->tcp != NULL ) {
		mutex_destroy( _main->tcp->mutex );
		myfree( _main->tcp, "tcp_free" );
	}
}

void tcp_start( void ) {
	int listen_queue_length = SOMAXCONN * 8;

	if( (_main->tcp->sockfd = socket( PF_INET6, SOCK_STREAM, 0)) < 0 ) {
		log_fail( "Creating socket failed." );
	}
	
	_main->tcp->s_addr.sin6_family = AF_INET6;
	_main->tcp->s_addr.sin6_port = htons( _main->conf->port );
	_main->tcp->s_addr.sin6_addr = in6addr_any;
	
	if( setsockopt( _main->tcp->sockfd, SOL_SOCKET, SO_REUSEADDR, &_main->tcp->optval, sizeof(int)) == -1 ) {
		log_fail( "Setting SO_REUSEADDR failed" );
	}

	if( _main->conf->ipv6_only == TRUE ) {
		log_info( NULL, 0, "IPv4 disabled" );
		if( setsockopt( _main->tcp->sockfd, IPPROTO_IPV6, IPV6_V6ONLY, &_main->tcp->optval, sizeof(int)) == -1 ) {
			log_fail( "Setting IPV6_V6ONLY failed" );
		}
	}

	if( bind( _main->tcp->sockfd, (struct sockaddr *) &_main->tcp->s_addr, _main->tcp->s_addrlen) ) {
		log_fail( "bind() to socket failed." );
	}

	if( tcp_nonblocking( _main->tcp->sockfd) < 0 ) {
		log_fail( "tcp_nonblocking( _main->tcp->sockfd) failed" );
	}
	
	if( listen( _main->tcp->sockfd, listen_queue_length) ) {
		log_fail( "listen() to socket failed." );
	} else {
		log_info( NULL, 0, "Listen queue length: %i", listen_queue_length );
	}

	/* Drop privileges */
	unix_dropuid0();
	
	/* Setup epoll */
	tcp_event();
	
	/* Create worker */
	tcp_pool();
}

void tcp_stop( void ) {
	int i;

	/* Join threads */
	pthread_attr_destroy( &_main->tcp->attr );
	for( i=0; i < _main->conf->cores; i++ ) {
		if( pthread_join( *_main->tcp->threads[i], NULL) != 0 ) {
			log_fail( "pthread_join() failed" );
		}
		myfree( _main->tcp->threads[i], "tcp_pool" );
	}
	myfree( _main->tcp->threads, "tcp_pool" );

	/* Shutdown socket */
	if( shutdown( _main->tcp->sockfd, SHUT_RDWR) != 0 )
		log_fail( "shutdown() failed." );

	/* Close socket */
	if( close( _main->tcp->sockfd) != 0 )
		log_fail( "close() failed." );

	/* Close epoll */
	if( close( _main->tcp->epollfd) != 0 )
		log_fail( "close() failed." );
}

void tcp_event( void ) {
	struct epoll_event ev;
	
	_main->tcp->epollfd = epoll_create( 23 );
	if( _main->tcp->epollfd == -1 ) {
		log_fail( "epoll_create() failed" );
	}

	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = _main->tcp->sockfd;
	if( epoll_ctl( _main->tcp->epollfd, EPOLL_CTL_ADD, _main->tcp->sockfd, &ev) == -1 ) {
		log_fail( "tcp_event: epoll_ctl() failed" );
	}
}

void tcp_pool( void ) {
	int i;
	
	/* Initialize and set thread detached attribute */
	pthread_attr_init( &_main->tcp->attr );
	pthread_attr_setdetachstate( &_main->tcp->attr, PTHREAD_CREATE_JOINABLE );

	/* Create worker threads */
	_main->tcp->threads = (pthread_t **) myalloc( _main->conf->cores * sizeof(pthread_t *), "tcp_pool" );
	for( i=0; i < _main->conf->cores; i++ ) {
		_main->tcp->threads[i] = (pthread_t *) myalloc( sizeof(pthread_t), "tcp_pool" );
		if( pthread_create( _main->tcp->threads[i], &_main->tcp->attr, tcp_thread, NULL) != 0 ) {
			log_fail( "pthread_create()" );
		}
	}
}

void *tcp_thread( void *arg ) {
	struct epoll_event events[TCP_MAX_EVENTS];
	int nfds;
	int id = 0;

	mutex_block( _main->tcp->mutex );
	id = _main->tcp->id++;
	mutex_unblock( _main->tcp->mutex );
	
	log_info( NULL, 0, "Thread[%i] - Max events: %i", id, TCP_MAX_EVENTS );

	for( ;; ) {
		nfds = epoll_wait( _main->tcp->epollfd, events, TCP_MAX_EVENTS, CONF_EPOLL_WAIT );

		if( _main->status == RUMBLE && nfds == -1 ) {
			if( errno != EINTR ) {
				log_info( NULL, 500, "epoll_wait() failed" );
				log_fail( strerror( errno ) );
			}
		} else if( _main->status == RUMBLE && nfds == 0 ) {
			/* Timed wakeup */
			if( id == 0 ) {
				tcp_cron();
			}
		} else if( _main->status == RUMBLE && nfds > 0 ) {
			tcp_worker( events, nfds, id );
		} else {
			/* Shutdown server */
			break;
		}
	}

	pthread_exit( NULL );
}

void tcp_worker( struct epoll_event *events, int nfds, int thrd_id ) {
	ITEM *listItem = NULL;
	int i;

	mutex_block( _main->tcp->mutex );
	_main->tcp->active++;
	mutex_unblock( _main->tcp->mutex );

	for( i=0; i<nfds; i++ ) {
		if( events[i].data.fd == _main->tcp->sockfd ) {
			tcp_newconn();
		} else {
			listItem = events[i].data.ptr;
	
			if( events[i].events & EPOLLIN ) {
				tcp_input( listItem );
			} else if( events[i].events & EPOLLOUT ) {
				tcp_output( listItem );
			}

			/* Close, Input or Output next? */
			tcp_gate( listItem );
		}
	}

	mutex_block( _main->tcp->mutex );
	_main->tcp->active--;
	mutex_unblock( _main->tcp->mutex );
}

void tcp_gate( ITEM *listItem ) {
	NODE *nodeItem = list_value( listItem );

	switch( nodeItem->mode ) {
		case NODE_MODE_SHUTDOWN:
			node_shutdown( listItem );
			break;
		case NODE_MODE_READY:
			tcp_rearm( listItem, TCP_INPUT );
			break;
		case NODE_MODE_SEND_INIT:
		case NODE_MODE_SEND_MEM:
		case NODE_MODE_SEND_FILE:
		case NODE_MODE_SEND_STOP:
			tcp_rearm( listItem, TCP_OUTPUT );
			break;
		default:
			log_fail( "No shit" );
	}
}

void tcp_rearm( ITEM *listItem, int mode ) {
	NODE *nodeItem = list_value( listItem );
	struct epoll_event ev;

	if( mode == TCP_INPUT ) {
		ev.events = EPOLLET | EPOLLIN | EPOLLONESHOT;
	} else {
		ev.events = EPOLLET | EPOLLOUT | EPOLLONESHOT;
	}
	
	ev.data.ptr = listItem;

	if( epoll_ctl( _main->tcp->epollfd, EPOLL_CTL_MOD, nodeItem->connfd, &ev) == -1 ) {
		log_info( NULL, 500, strerror( errno ) );
		log_fail( "tcp_rearm: epoll_ctl() failed" );
	}
}

int tcp_nonblocking( int sock ) {
	int opts = fcntl( sock,F_GETFL );
	if( opts < 0 ) {
		return -1;
	}
	opts = opts|O_NONBLOCK;
	if( fcntl( sock,F_SETFL,opts) < 0 ) {
		return -1;
	}

	return 1;
}

void tcp_newconn( void ) {
	struct epoll_event ev;
	ITEM *listItem = NULL;
	NODE *nodeItem = NULL;
	int connfd;
	IP c_addr;
	socklen_t c_addrlen = sizeof( IP );

	for( ;; ) {
		/* Prepare incoming connection */
		memset( (char *) &c_addr, '\0', c_addrlen );

		/* Accept */
		connfd = accept( _main->tcp->sockfd, (struct sockaddr *) &c_addr, &c_addrlen );
		if( connfd < 0 ) {
			if( errno == EAGAIN || errno == EWOULDBLOCK ) {
				break;
			} else {
				log_fail( strerror( errno) );
			}
		}

		/* New connection: Create node object */
		if( (listItem = node_put()) == NULL ) {
			log_info( NULL, 500, "The linked list reached its limits" );
			node_disconnect( connfd );
			break;
		}
		
		/* Store data */
		nodeItem = list_value( listItem );
		nodeItem->connfd = connfd;
		memcpy( &nodeItem->c_addr, &c_addr, c_addrlen );
		nodeItem->c_addrlen = c_addrlen;

		/* Non blocking */
		if( tcp_nonblocking( nodeItem->connfd) < 0 ) {
			log_fail( "tcp_nonblocking() failed" );
		}

		ev.events = EPOLLET | EPOLLIN | EPOLLONESHOT;
		ev.data.ptr = listItem;
		if( epoll_ctl( _main->tcp->epollfd, EPOLL_CTL_ADD, nodeItem->connfd, &ev) == -1 ) {
			log_info( NULL, 500, strerror( errno ) );
			log_fail( "tcp_newconn: epoll_ctl() failed" );
		}
	}
}

void tcp_output( ITEM *listItem ) {
	NODE *nodeItem = list_value( listItem );
	
	switch( nodeItem->mode ) {
		case NODE_MODE_SEND_INIT:
		case NODE_MODE_SEND_MEM:
		case NODE_MODE_SEND_FILE:
		case NODE_MODE_SEND_STOP:
			
			/* Remember last activity */
			node_activity( nodeItem );
			
			/* Send file */
			send_exec( nodeItem );
			break;
	}
}

void tcp_input( ITEM *listItem ) {
	NODE *nodeItem = list_value( listItem );
	char buffer[MAIN_BUF+1];
	ssize_t bytes = 0;
	
	while( _main->status == RUMBLE ) {

		/* Remember last activity */
		node_activity( nodeItem );
		
		/* Get data */
		bytes = recv( nodeItem->connfd, buffer, MAIN_BUF, 0 );

		if( bytes < 0 ) {
			if( errno == EAGAIN || errno == EWOULDBLOCK ) {
				return;
			} else if( errno == ECONNRESET ) {
				/*
				 * Very common behaviour
				 * log_info( &nodeItem->c_addr, 0, "Connection reset by peer" );
				 */
				node_status( nodeItem, NODE_MODE_SHUTDOWN );
				return;
			} else {
				log_info( &nodeItem->c_addr, 0, "recv() failed:" );
				log_info( &nodeItem->c_addr, 0, strerror( errno) );
				return;
			}
		} else if( bytes == 0 ) {
			/* Regular shutdown */
			node_status( nodeItem, NODE_MODE_SHUTDOWN );
			return;
		} else {
			/* Read */
			tcp_buffer( nodeItem, buffer, bytes );
			return;
		}
	}
}

void tcp_buffer( NODE *nodeItem, char *buffer, ssize_t bytes ) {
	/* Append buffer */
	node_appendBuffer( nodeItem, buffer, bytes );

	if( nodeItem->mode != NODE_MODE_READY ) {
		log_fail( "FIXME tcp_buffer..." );
	}

	/* Overflow? */
	if( nodeItem->recv_size >= MAIN_BUF ) {
		log_info( &nodeItem->c_addr, 500, "Max head buffer exceeded..." );
		node_status( nodeItem, NODE_MODE_SHUTDOWN );
		return;
	}

	http_buf( nodeItem );
}

void tcp_cron( void ) {
	mutex_block( _main->tcp->mutex );

	/* Do some jobs while no worker is running */
	if( _main->tcp->active == 0 ) {
		node_cleanup();
	}
	mutex_unblock( _main->tcp->mutex );
}
