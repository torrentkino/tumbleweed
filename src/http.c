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
#include <sys/stat.h>
#include <semaphore.h>
#include <pthread.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <dirent.h>

#include "main.h"
#include "malloc.h"
#include "conf.h"
#include "str.h"
#include "hash.h"
#include "list.h"
#include "thrd.h"
#include "node_web.h"
#include "log.h"
#include "file.h"
#include "mime.h"
#include "http.h"
#include "send_web.h"

long int http_urlDecode( char *src, long int srclen, char *dst, long int dstlen ) {
	int i = 0, j = 0;
	char *p1 = NULL;
	char *p2 = NULL;
	unsigned int hex = 0;

	if( src == NULL ) {
		log_info( NULL, 400, "http_urlDecode(): URL is NULL" );
		return 0;
	} else if( dst == NULL ) {
		log_info( NULL, 400, "http_urlDecode(): Destination is broken" );
		return 0;
	} else if( dstlen <= 0 ) {
		log_info( NULL, 400, "http_urlDecode(): Destination length is <= 0" );
		return 0;
	} else if( srclen <= 0 ) {
		log_info( NULL, 400, "http_urlDecode(): URL length is <= 0" );
		return 0;
	}
	
	memset( dst, '\0', dstlen );

	p1 = src;
	p2 = dst;
	while( i < dstlen && j < srclen ) {
		if( *p1 == '+' ) {
			*p2 = ' ';
			p1++; p2++;
			j++;
		} else if( *p1 == '%' ) {
			/* Safety check */
			if( (long int)(src+srclen-p1) <= 2 ) {
				/* Path is broken: There should have been two more characters */
				log_info( NULL, 500, "http_urlDecode(): Broken url" );
				return 0;
			}

			if( !sscanf( ++p1,"%2x",&hex) ) {
				/* Path is broken: Broken characters */
				log_info( NULL, 500, "http_urlDecode(): Broken characters" );
				return 0;
			}
			
			*p2 = hex;
			p1 += 2; p2++;
			j += 3;
		} else {
			*p2 = *p1;
			p1++; p2++;
			j++;
		}

		i++; 
	}

	return i;
}

HASH *http_hashHeader( char *head ) {
	int head_counter = 0;
	char *p = NULL;
	char *var = NULL;
	char *val = NULL;
	char *end = NULL;
	HASH *hash = NULL;

	/* Compute hash size */
	head_counter = str_count( head, "\r\n" );
	if( head_counter >= 100 ) {
		log_info( NULL, 400, "More than 100 headers?!" );
		return NULL;
	} else if( head_counter <= 0 ) {
		/* HTTP/1.0 */
		return NULL;
	}
	
	/* Create hash */
	hash = hash_init( head_counter );

	/* Put header into hash */
	p = head;
	while( (end = strstr( p, "\r\n")) != NULL ) {
		memset( end, '\0', 2 );
		if( (val = strchr( p, ':')) != NULL ) {
			
			/* Variable & Value */
			var = p;
			*val = '\0';
			val++;
			
			/* More or less empty spaces */
			while( *val == ' ' ) {
				val++;
			}
			
			/* Store tuple */
			hash_put( hash, (UCHAR *)var, strlen( var), val );
		
		} else {
			log_info( NULL, 400, "Missing ':' in header?!" );
			hash_free( hash );
			return NULL;
		}
		p = end + 2;
	}

	return hash;
}

void http_deleteHeader( HASH *head ) {
	hash_free( head );
}

void http_buf( NODE *nodeItem ) {
	char *p_cmd = NULL;
	char *p_url = NULL;
	char *p_var = NULL;
	char *p_proto = NULL;
	char *p_head = NULL;
	char *p_body = NULL;
	HASH *head = NULL;
	char recv_buf[MAIN_BUF+1];

	/* Do not start before at least this much arrived: "GET / HTTP/1.1" */
	if( nodeItem->recv_size <= 14 ) {
		return;
	}

	/* Copy HTTP request */
	memset( recv_buf, '\0', MAIN_BUF+1 );
	memcpy( recv_buf, nodeItem->recv_buf, nodeItem->recv_size );

	/* Return until the header is complete. Gets killed if this grows beyond all imaginations. */
	if( (p_body = strstr( recv_buf, "\r\n\r\n")) == NULL ) {
		return;
	}

	/* Keep one \r\n for consistent header analysis. */
	*p_body = '\r'; p_body++;
	*p_body = '\n'; p_body++;
	*p_body = '\0'; p_body++;
	*p_body = '\0'; p_body++;

	/* HTTP Pipelining: There is at least one more request. */
	if( nodeItem->recv_size > (ssize_t)(p_body-recv_buf) ) {
		nodeItem->recv_size -= (ssize_t)(p_body-recv_buf );
		memset( nodeItem->recv_buf, '\0', MAIN_BUF+1 );
		memcpy( nodeItem->recv_buf, p_body, nodeItem->recv_size );
	} else {
		/* The request has been copied. Reset the receive buffer. */
		node_clearRecvBuf( nodeItem );
	}

	/* Remember start point */
	p_cmd = recv_buf;

	/* Find url */
	if( (p_url = strchr( p_cmd, ' ')) == NULL ) {
		log_info( &nodeItem->c_addr, 400, "Requested URL was not found" );
		node_status( nodeItem, NODE_MODE_SHUTDOWN );
		return;
	}
	*p_url = '\0'; p_url++;

	/* Find protocol type */
	if( (p_proto = strchr( p_url, ' ')) == NULL ) {
		log_info( &nodeItem->c_addr, 400, "No protocol found in request" );
		node_status( nodeItem, NODE_MODE_SHUTDOWN );
		return;
	}
	*p_proto = '\0'; p_proto++;

	/* Find variables( Start from p_url again) */
	if( (p_var = strchr( p_url, '?')) != NULL ) {
		*p_var = '\0'; p_var++;
	}

	/* Find header lines */
	if( (p_head = strstr( p_proto, "\r\n")) == NULL ) {
		log_info( &nodeItem->c_addr, 400, "Impossible. There must be a \\r\\n. I put it there..." );
		node_status( nodeItem, NODE_MODE_SHUTDOWN );
		return;
	}
	*p_head = '\0'; p_head++;
	*p_head = '\0'; p_head++;

	/* Hash header */
	head = http_hashHeader( p_head );
	
	/* Validate input */
	http_read( nodeItem, p_cmd, p_url, p_proto, head );
	
	/* Delete Hash */
	http_deleteHeader( head );
}

void http_read( NODE *nodeItem, char *p_cmd, char *p_url, char *p_proto, HASH *p_head ) {
	/* Check header */
	if( !http_check( nodeItem, p_cmd, p_url, p_proto) ) {
		return;
	}

	/* Guess HTTP reply code: 200 or 404 */
	http_code( nodeItem );

	/* Add Keep-Alive if necessary */
	http_keepalive( nodeItem, p_head );

	/* Add Last-Modified. The reply code may change to 304 */
	http_lastmodified( nodeItem, p_head );

	/* HTTP Reply Size */
	http_size( nodeItem, p_head );

	/* Prepare reply according to the HTTP code */
	http_gate( nodeItem, p_head );

	/* Send reply */
	http_send( nodeItem );
}

int http_check( NODE *nodeItem, char *p_cmd, char *p_url, char *p_proto ) {

	/* Command */
	if( strncmp( p_cmd, "GET", 3) == 0 ) {
		nodeItem->type = HTTP_GET;
	} else if( strncmp( p_cmd, "HEAD", 4) == 0 ) {
		nodeItem->type = HTTP_HEAD;
	} else {
		log_info( &nodeItem->c_addr, 400, "Invalid request type: %s", p_cmd );
		node_status( nodeItem, NODE_MODE_SHUTDOWN );
		return 0;
	}

	/* Protocol */
	if( strncmp( p_proto, "HTTP/1.1", 8) != 0 && strncmp( p_proto, "HTTP/1.0", 8) != 0 ) {
		log_info( &nodeItem->c_addr, 400, "Invalid protocol: %s", p_proto );
		node_status( nodeItem, NODE_MODE_SHUTDOWN );
		return 0;
	}
	if( strncmp( p_proto, "HTTP/1.1", 8) == 0 ) {
		nodeItem->proto = HTTP_1_1;
	} else {
		nodeItem->proto = HTTP_1_0;
	}

	/* URL */
	if( !http_urlDecode( p_url, strlen( p_url), nodeItem->entity_url, MAIN_BUF+1) ) {
		log_info( &nodeItem->c_addr, 400, "Decoding URL failed" );
		node_status( nodeItem, NODE_MODE_SHUTDOWN );
		return 0;
	}

	/* Minimum path requirement */
	if( nodeItem->entity_url[0] != '/' ) {
		log_info( &nodeItem->c_addr, 400, "URL must start with '/': %s",
			nodeItem->entity_url );
		node_status( nodeItem, NODE_MODE_SHUTDOWN );
		return 0;
	}

	/* ".." in path. Do not like. */
	if( strstr( nodeItem->entity_url, "../") != NULL ) {
		log_info( &nodeItem->c_addr, 400, "Double dots: %s",
			nodeItem->entity_url );
		node_status( nodeItem, NODE_MODE_SHUTDOWN );
		return 0;
	}
 
	/* URL */
	if( ! str_isValidUTF8( nodeItem->entity_url) ) {
		log_info( &nodeItem->c_addr, 400, "Invalid UTF8 in URL" );
		node_status( nodeItem, NODE_MODE_SHUTDOWN );
		return 0;
	}

	return 1;
}

void http_code( NODE *nodeItem ) {
	snprintf( nodeItem->filename, MAIN_BUF+1, "%s%s", _main->conf->home, nodeItem->entity_url );

	if( file_isreg( nodeItem->filename) ) {
		
		/* The requested entity is a file */
		nodeItem->code = 200;
	
	} else if( file_isdir( nodeItem->filename) ) {

		/* There is a index.html file within that directory? */
		snprintf( nodeItem->filename, MAIN_BUF+1, "%s%s/%s", _main->conf->home, nodeItem->entity_url, _main->conf->index_name );
		
		if( file_isreg( nodeItem->filename) ) {
			nodeItem->code = 200;
		} else {
			nodeItem->code = 404;
		}
	
	} else {

		nodeItem->code = 404;
	}
}

void http_keepalive( NODE *nodeItem, HASH *head ) {
	memset( nodeItem->keepalive, '\0', MAIN_BUF+1 );

	if( hash_exists( head, (UCHAR *)"Connection",  10) ) {
		if( strcasecmp( (char *)hash_get( head, (UCHAR *)"Connection", 10), "Keep-Alive") == 0 ) {
			snprintf( nodeItem->keepalive, MAIN_BUF+1, "Connection: %s\r\n",
				(char *)hash_get( head, (UCHAR *)"Connection", 10) );
		}
	}
}

void http_lastmodified( NODE *nodeItem, HASH *head ) {
	char entity_time[MAIN_BUF+1];

	if( nodeItem->code != 200 || !file_isreg( nodeItem->filename) ) {
		return;
	}

	str_gmttime( entity_time, MAIN_BUF+1, file_mod( nodeItem->filename) );

	snprintf( nodeItem->lastmodified, MAIN_BUF+1, "Last-Modified: %s\r\n", entity_time );

	if( hash_exists( head, (UCHAR *)"If-Modified-Since",  17) ) {
		if( strcmp( entity_time, (char *)hash_get( head, (UCHAR *)"If-Modified-Since", 17)) == 0 ) {
			nodeItem->code = 304;
		}
	}
}

void http_gate( NODE *nodeItem, HASH *head ) {
	/* Compute full reply */
	switch( nodeItem->code ) {
		case 200:
			http_200( nodeItem );
			log_info( &nodeItem->c_addr, nodeItem->code, nodeItem->entity_url );
			break;
		case 206:
			http_206( nodeItem );
			log_info( &nodeItem->c_addr, nodeItem->code,
				"%s [%s]", nodeItem->entity_url,
				(char *)hash_get(head, (UCHAR *)"Range", 5));
			break;
		case 304:
			http_304( nodeItem );
			log_info( &nodeItem->c_addr, nodeItem->code, nodeItem->entity_url );
			break;
		case 404:
			http_404( nodeItem );
			log_info( &nodeItem->c_addr, nodeItem->code, nodeItem->entity_url );
			break;
	}
}

void http_send( NODE *nodeItem ) {
	/* Compute header size */
	nodeItem->send_size = strlen( nodeItem->send_buf );

	/* Prepare event system for sending data */
	node_status( nodeItem, NODE_MODE_SEND_INIT );

	/* Start sending */
	send_exec( nodeItem );
}

void http_size( NODE *nodeItem, HASH *head ) {
	char *p_start = NULL;
	char *p_stop = NULL;
	char range[MAIN_BUF+1];

 	/* Not a valid file */
	if ((nodeItem->code != 200 && nodeItem->code != 304) || !file_isreg(nodeItem->filename)) {
 		return;
 	}
 
 	/* Normal case: content_length == filesize == f_stop */
	nodeItem->filesize = file_size(nodeItem->filename);
	nodeItem->f_stop = nodeItem->filesize;
	nodeItem->content_length = nodeItem->filesize;

	/* No Range header found: Nothing to do */
	if ( !hash_exists(head, (UCHAR *)"Range", 5) ) {
		return;
	}

	strncpy(range, (char *)hash_get(head, (UCHAR *)"Range", 5), MAIN_BUF);
	range[MAIN_BUF] = '\0';
	p_start = range;

	if ( strncmp(p_start, "bytes=", 6) != 0 ) {
		log_info( &nodeItem->c_addr, 206, "Broken Range[1]: %s", 
			(char *)hash_get(head, (UCHAR *)"Range", 5));
		return;
	}
	p_start += 6;

	if ( (p_stop = strchr(p_start, '-')) == NULL ) {
		log_info( &nodeItem->c_addr, 206, "Broken Range[2]: %s",
			(char *)hash_get(head, (UCHAR *)"Range", 5));
		return;
	}
	*p_stop = '\0'; p_stop++;

	if ( *p_start == '\0' ) {
		nodeItem->range_start = 0;
	} else if ( str_isNumber(p_start) ) {
		nodeItem->range_start = atol(p_start);
	} else {
		log_info( &nodeItem->c_addr, 206, "Broken Range[3]: %s",
			(char *)hash_get(head, (UCHAR *)"Range", 5));
		return;
	}

	if ( *p_stop == '\0' ) {
		nodeItem->range_stop = nodeItem->filesize-1;
	} else if ( str_isNumber(p_stop) ) {
		nodeItem->range_stop = atol(p_stop);
	} else {
		log_info( &nodeItem->c_addr, 206,
			"Broken Range[4]: %s", (char *)hash_get(head, (UCHAR *)"Range", 5));
		return;
	}
	
	if ( nodeItem->range_start < 0 || nodeItem->range_start >= nodeItem->filesize ) {
		log_info( &nodeItem->c_addr, 206,
			"Broken Range[5]: %s", (char *)hash_get(head, (UCHAR *)"Range", 5));
		return;
	}
	
	if ( nodeItem->range_stop < nodeItem->range_start || nodeItem->range_stop >= nodeItem->filesize ) {
	   	log_info( &nodeItem->c_addr, 206,
			"Broken Range[7]: %s", (char *)hash_get(head, (UCHAR *)"Range", 5));
	   	return;
	}

	nodeItem->f_offset = nodeItem->range_start;
	nodeItem->f_stop = nodeItem->range_stop + 1;
	nodeItem->content_length = nodeItem->range_stop - nodeItem->f_offset + 1;

	/* Success */
	nodeItem->code = 206;
}

void http_404( NODE *nodeItem ) {
	char datebuf[MAIN_BUF+1];
	char buffer[MAIN_BUF+1] = 
	"<!DOCTYPE html>"
	"<html lang=\"en\" xml:lang=\"en\">"
	"<head>"
	"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />"
	"<title>Not found</title>"
	"</head>"
	"<body>"
	"<h1>Not found</h1>"
	"</body>"
	"</html>";
	int size = strlen( buffer );
	char protocol[9] = "HTTP/1.1";
	
	if( nodeItem->proto == HTTP_1_0 ) {
		strncpy( protocol, "HTTP/1.0", 8 );
	}
	
	/* Compute GMT time */
	str_GMTtime( datebuf, MAIN_BUF+1 );

	snprintf( nodeItem->send_buf, MAIN_BUF+1,
	"%s 404 Not found\r\n"
	"Date: %s\r\n"
	"Server: %s\r\n"
	"Content-Length: %i\r\n"
	"Content-Type: text/html; charset=utf-8\r\n"
	"%s"
	"\r\n"
	"%s",
	protocol, datebuf, CONF_SRVNAME, size, nodeItem->keepalive, buffer );
}

void http_304( NODE *nodeItem ) {
	char datebuf[MAIN_BUF+1];
	char protocol[9] = "HTTP/1.1";
	
	if( nodeItem->proto == HTTP_1_0 ) {
		strncpy( protocol, "HTTP/1.0", 8 );
	}
	
	/* Compute GMT time */
	str_GMTtime( datebuf, MAIN_BUF+1 );

	snprintf( nodeItem->send_buf, MAIN_BUF+1,
	"%s 304 Not Modified\r\n"
	"Date: %s\r\n"
	"Server: %s\r\n"
	"%s"
	"\r\n",
	protocol, datebuf, CONF_SRVNAME, nodeItem->keepalive );
}

void http_200( NODE *nodeItem ) {
	char datebuf[MAIN_BUF+1];
	const char *mimetype = NULL;
	char protocol[9] = "HTTP/1.1";
	
	if( nodeItem->proto == HTTP_1_0 ) {
		strncpy( protocol, "HTTP/1.0", 8 );
	}
	
	/* Compute GMT time */
	str_GMTtime( datebuf, MAIN_BUF+1 );

	/* Compute mime type */
	mimetype = mime_find( nodeItem->filename );

	/* Compute answer */
	snprintf( nodeItem->send_buf, MAIN_BUF+1,
	"%s 200 OK\r\n"
	"Date: %s\r\n"
	"Server: %s\r\n"
#ifdef __i386__
	"Content-Length: %u\r\n"
#else
	"Content-Length: %lu\r\n"
#endif
	"Content-Type: %s\r\n"
	"%s"
	"%s"
	"\r\n",
	protocol, datebuf, CONF_SRVNAME, nodeItem->content_length, mimetype, nodeItem->lastmodified, nodeItem->keepalive );
}

void http_206( NODE *nodeItem ) {
	char datebuf[MAIN_BUF+1];
	const char *mimetype = NULL;
	char protocol[9] = "HTTP/1.1";
	
	if (nodeItem->proto == HTTP_1_0) {
		strncpy(protocol, "HTTP/1.0", 8);
	}

	str_GMTtime(datebuf, MAIN_BUF+1);
	mimetype = mime_find(nodeItem->filename);

	snprintf(nodeItem->send_buf, MAIN_BUF+1,
	"%s 206 OK\r\n"
	"Date: %s\r\n"
	"Server: %s\r\n"
#ifdef __i386__
	"Content-Length: %u\r\n"
	"Content-Range: bytes %u-%u/%u\r\n"
#else
	"Content-Length: %lu\r\n"
	"Content-Range: bytes %lu-%lu/%lu\r\n" 
#endif
	"Content-Type: %s\r\n"
	"Accept-Ranges: bytes\r\n"
	"%s"
	"%s"
	"\r\n",
	protocol, datebuf, CONF_SRVNAME,
#ifdef __i386__
	nodeItem->content_length,
	(unsigned int)nodeItem->range_start, (unsigned int)nodeItem->range_stop, nodeItem->filesize,
#else
	nodeItem->content_length,
	nodeItem->range_start, nodeItem->range_stop, nodeItem->filesize,
#endif
	mimetype, nodeItem->lastmodified, nodeItem->keepalive);
}
