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

#define NODE_MODE_READY   	1
#define NODE_MODE_SEND_INIT 3
#define NODE_MODE_SEND_MEM  4
#define NODE_MODE_SEND_FILE 5
#define NODE_MODE_SEND_STOP 6
#define NODE_MODE_SHUTDOWN  7

#define NODE_HANDSHAKE_READY 0
#define NODE_HANDSHAKE_ESTABLISHED 1

struct obj_nodes {
	LIST *list;
	
	/* Safe access to the linked list */
	pthread_mutex_t *mutex;
};
typedef struct obj_nodes NODES;

struct obj_node {
	int connfd;
	IP c_addr;
	socklen_t c_addrlen;

	/* Receive buffer */
	char recv_buf[MAIN_BUF+1];
	ssize_t recv_size;

	/* Send buffer */
	char send_buf[MAIN_BUF+1];
	int send_size;
	int send_offset;

	/* URL */
	char entity_url[MAIN_BUF+1];

	/* File meta data */
	char filename[MAIN_BUF+1];
	size_t filesize;
	off_t f_offset;
	off_t f_stop;
	size_t content_length;

	/* HTTP Range */
	off_t range_start;
	off_t range_stop;

	/* HTTP Keep-Alive */
	char keepalive[MAIN_BUF+1];
	int keepalive_counter;

	/* HTTP Last-Modified */
	char lastmodified[MAIN_BUF+1];

	/* HTTP code */
	int code;

	/* HTTP type */
	int type;

	/* HTTP protocol */
	int proto;

	int mode;
};
typedef struct obj_node NODE;

NODES *nodes_init( void );
void nodes_free( void );

ITEM *node_put( void );
void node_disconnect( int connfd );
void node_shutdown( ITEM *thisnode );
void node_status( NODE *nodeItem, int status );
void node_activity( NODE *nodeItem );

ssize_t node_appendBuffer( NODE *nodeItem, char *buffer, ssize_t bytes );
void node_clearSendBuf( NODE *nodeItem );
void node_clearRecvBuf( NODE *nodeItem );

void node_cleanup( void );
