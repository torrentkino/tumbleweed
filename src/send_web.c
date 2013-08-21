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
#include <arpa/inet.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <signal.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/sendfile.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <netinet/tcp.h>

#include "malloc.h"
#include "tumbleweed.h"
#include "thrd.h"
#include "str.h"
#include "conf.h"
#include "list.h"
#include "node_web.h"
#include "log.h"
#include "hash.h"
#include "file.h"
#include "send_web.h"
#include "http.h"

void send_exec( NODE *nodeItem ) {
	switch( nodeItem->mode ) {
		case NODE_MODE_SEND_INIT:
			send_cork_start( nodeItem );

		case NODE_MODE_SEND_MEM:
			send_mem( nodeItem );

		case NODE_MODE_SEND_FILE:
			send_file( nodeItem );

		case NODE_MODE_SEND_STOP:
			send_cork_stop( nodeItem );
	}
}

void send_cork_start( NODE *nodeItem ) {
	int on = 1;

	if( setsockopt( nodeItem->connfd, IPPROTO_TCP, TCP_CORK, &on, sizeof(on)) != 0 ) {
		fail( strerror( errno) );
	}
	
	node_status( nodeItem, NODE_MODE_SEND_MEM );
}

void send_mem( NODE *nodeItem ) {
	ssize_t bytes_sent = 0;
	int bytes_todo = 0;
	char *p = NULL;

	while( _main->status == RUMBLE ) {
		p = nodeItem->send_buf + nodeItem->send_offset;
		bytes_todo = nodeItem->send_size - nodeItem->send_offset;
	
		bytes_sent = send( nodeItem->connfd, p, bytes_todo, 0 );
	
		if( bytes_sent < 0 ) {
			if( errno == EAGAIN || errno == EWOULDBLOCK ) {
				return;
			}
	
			/* Client closed the connection, etc... */
			node_status( nodeItem, NODE_MODE_SHUTDOWN );
			return;
		}
	
		nodeItem->send_offset += bytes_sent;
		
		/* Done */
		if( nodeItem->send_offset >= nodeItem->send_size ) {
			node_status( nodeItem, NODE_MODE_SEND_FILE );
			return;
		}
	}
}

void send_file( NODE *nodeItem ) {
	ssize_t bytes_sent = 0;
	int fh = 0;

	if( nodeItem->mode != NODE_MODE_SEND_FILE ) {
		return;
	}

	/* Nothing to do */
	if( nodeItem->filesize == 0 ) {
		node_status( nodeItem, NODE_MODE_SEND_STOP );
		return;
	}

	/* Not modified. Stop here. */
	if( nodeItem->code == 304 ) {
		node_status( nodeItem, NODE_MODE_SEND_STOP );
		return;
	}

	/* Not found */
	if( nodeItem->code == 404 ) {
		node_status( nodeItem, NODE_MODE_SEND_STOP );
		return;
	}

	/* HEAD request. Stop here. */
	if( nodeItem->type == HTTP_HEAD ) {
		node_status( nodeItem, NODE_MODE_SEND_STOP );
		return;
	}

	while( _main->status == RUMBLE ) {
		fh = open( nodeItem->filename, O_RDONLY );	
		if( fh < 0 ) {
			info( NULL, 500, "Failed to open %s", nodeItem->filename );
			fail( strerror( errno) );
		}
	
		/* The SIGPIPE gets catched in sig.c */
		bytes_sent = sendfile( nodeItem->connfd, fh, &nodeItem->f_offset, nodeItem->content_length );
	
		if( close( fh) != 0 ) {
			info( NULL, 500, "Failed to close %s", nodeItem->filename );
			fail( strerror( errno) );
		}

		if( bytes_sent < 0 ) {
			if( errno == EAGAIN || errno == EWOULDBLOCK ) {
				/* connfd is blocking */
				return;
			}
			
			/* Client closed the connection, etc... */
			node_status( nodeItem, NODE_MODE_SHUTDOWN );
			return;
		}
		
		/* Done */
		if( nodeItem->f_offset >= nodeItem->f_stop ) {
			/* Clean up */
			node_status( nodeItem, NODE_MODE_SEND_STOP );
			return;
		}
	}
}

void send_cork_stop( NODE *nodeItem ) {
	int off = 0;

	if( nodeItem->mode != NODE_MODE_SEND_STOP ) {
		return;
	}

	if( setsockopt( nodeItem->connfd, IPPROTO_TCP, TCP_CORK, &off, sizeof(off)) != 0 ) {
		fail( strerror( errno) );
	}

	/* Clear input and output buffers */
	node_clearSendBuf( nodeItem );

	/* Mark TCP server ready */
	node_status( nodeItem, NODE_MODE_READY );

	/* HTTP Pipeline: There is at least one more request to parse. */
	if( nodeItem->recv_size > 0 ) {
		http_buf( nodeItem );
	}
}
