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
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <signal.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

#include "malloc.h"
#include "tumbleweed.h"
#include "thrd.h"
#include "str.h"
#include "conf.h"
#include "file.h"
#include "list.h"
#include "node_web.h"
#include "log.h"
#include "hash.h"
#include "tcp.h"
#include "http.h"

NODES *nodes_init( void ) {
	NODES *nodes = (NODES *) myalloc( sizeof(NODES), "nodes_init" );
	nodes->list = list_init();
	nodes->mutex = mutex_init();
	return nodes;
}

void nodes_free( void ) {
	ITEM *item = NULL;
	NODE *node = NULL;

	while( ( item = list_start( _main->nodes->list ) ) != NULL ) {
		node = list_value( item );

		node_status( node, NODE_MODE_SHUTDOWN );
		node_shutdown( item );
	}

	list_free( _main->nodes->list );
	mutex_destroy( _main->nodes->mutex );
	myfree( _main->nodes, "nodes_free" );
}

ITEM *node_put( void ) {
	NODE *nodeItem = (NODE *) myalloc( sizeof(NODE), "node_put" );
	ITEM *thisnode = NULL;

	/* Address information */
	nodeItem->c_addrlen = sizeof( IP );
	memset( (char *) &nodeItem->c_addr, '\0', nodeItem->c_addrlen );

	/* Receive buffer */
	memset( nodeItem->recv_buf, '\0', BUF_SIZE );
	nodeItem->recv_size = 0;
	
	/* Send buffer */
	memset( nodeItem->send_buf, '\0', BUF_SIZE );
	nodeItem->send_offset = 0;
	nodeItem->send_size = 0;

	/* Entity url */
	memset( nodeItem->entity_url, '\0', BUF_SIZE );

	/* File metadata */
	memset( nodeItem->filename, '\0', BUF_SIZE );
	nodeItem->filesize = 0;
	nodeItem->f_offset = 0;
	nodeItem->f_stop = 0;
	nodeItem->content_length = 0;

	/* HTTP Range */
	nodeItem->range_start = 0;
	nodeItem->range_stop = 0;

	/* HTTP Keep-Alive */
	memset( nodeItem->keepalive, '\0', BUF_SIZE );
	nodeItem->keepalive_counter = CONF_KEEPALIVE;

	/* HTTP Last-Modified */
	memset( nodeItem->lastmodified, '\0', BUF_SIZE );

	/* HTTP code */
	nodeItem->code = 0;
	
	/* HTTP request type */
	nodeItem->type = HTTP_UNKNOWN;

	/* HTTP protocol type */
	nodeItem->proto = HTTP_1_0;

	/* Connection status */
	nodeItem->mode = NODE_MODE_READY;

	/* Connect node to the list */
	mutex_block( _main->nodes->mutex );
	thisnode = list_put( _main->nodes->list, nodeItem );
	mutex_unblock( _main->nodes->mutex );

	if( thisnode == NULL ) {
		myfree( nodeItem, "node_put" );
	}

	return thisnode;
}

void node_shutdown( ITEM *thisnode ) {
	NODE *nodeItem = list_value( thisnode );

	/* Close socket */
	node_disconnect( nodeItem->connfd );

	/* Clear buffer */
	node_clearRecvBuf( nodeItem );
	node_clearSendBuf( nodeItem );

	/* Unlink node object */
	mutex_block( _main->nodes->mutex );
	myfree( thisnode->val, "node_shutdown" );
	list_del( _main->nodes->list, thisnode );
	mutex_unblock( _main->nodes->mutex );
}

void node_disconnect( int connfd ) {
	/* Remove FD from the watchlist */
	if( epoll_ctl( _main->tcp->epollfd, EPOLL_CTL_DEL, connfd, NULL) == -1 ) {
		if( _main->status == RUMBLE ) {
			info( NULL, 500, strerror( errno ) );
			fail( "node_shutdown: epoll_ctl() failed" );
		}
	}

	/* Close socket */
	if( close( connfd ) != 0 ) {
		info( NULL, 500, "close() failed" );
	}
}

void node_status( NODE *nodeItem, int status ) {
	nodeItem->mode = status;
}

void node_activity( NODE *nodeItem ) {
	/* Activity counter */
	nodeItem->keepalive_counter = CONF_KEEPALIVE;
}

void node_clearRecvBuf( NODE *nodeItem ) {
	memset( nodeItem->recv_buf, '\0', BUF_SIZE );
	nodeItem->recv_size = 0;
}

void node_clearSendBuf( NODE *nodeItem ) {
	memset( nodeItem->send_buf, '\0', BUF_SIZE );
	nodeItem->send_size = 0;
	nodeItem->send_offset = 0;

	memset( nodeItem->filename, '\0', BUF_SIZE );
	memset( nodeItem->keepalive, '\0', BUF_SIZE );
	memset( nodeItem->lastmodified, '\0', BUF_SIZE );
	memset( nodeItem->entity_url, '\0', BUF_SIZE );
	nodeItem->filesize = 0;
	nodeItem->f_offset = 0;
	nodeItem->f_stop = 0;
	nodeItem->content_length = 0;
	nodeItem->range_start = 0;
	nodeItem->range_stop = 0;
	nodeItem->code = 0;
	nodeItem->type = HTTP_UNKNOWN;
	nodeItem->proto = HTTP_1_0;
}

ssize_t node_appendBuffer( NODE *nodeItem, char *buffer, ssize_t bytes ) {
	char *p = nodeItem->recv_buf + nodeItem->recv_size;
	ssize_t buf_free = BUF_OFF1 - nodeItem->recv_size;
	ssize_t buf_copy = (bytes > buf_free) ? buf_free : bytes;

	/* No space left in buffer */
	if( buf_copy == 0 ) {
		return nodeItem->recv_size;
	}

	/* Append buffer */
	memcpy( p, buffer, buf_copy );
	nodeItem->recv_size += buf_copy;

	/* Terminate buffer( Buffer size is NODE_BUFSIZE+1) */
	nodeItem->recv_buf[nodeItem->recv_size] = '\0';

	return nodeItem->recv_size;
}

void node_cleanup( void ) {
	ITEM *thisnode = NULL;
	ITEM *nextnode = NULL;
	NODE *nodeItem = NULL;
	
	if( list_size( _main->nodes->list ) == 0 ) {
		return;
	}

	thisnode = list_start( _main->nodes->list );
	while( thisnode != NULL ) {

		nextnode = list_next(thisnode);

		/* node_cleanup gets called every 1000ms and keepalive_counter is set to
		 * CONF_KEEPALIVE(5) on each event. */

		nodeItem = list_value( thisnode );
		if( nodeItem->keepalive_counter <= 0 ) {
			node_status( nodeItem, NODE_MODE_SHUTDOWN );
			node_shutdown( thisnode );
		} else {
			nodeItem->keepalive_counter--;
		}
		
		thisnode = nextnode;
	}
}
