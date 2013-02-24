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

#define HTTP_UNKNOWN 0
#define HTTP_GET 1
#define HTTP_HEAD 2

#define HTTP_1_0 10
#define HTTP_1_1 11

long int http_urlDecode( char *src, long int srclen, char *dst, long int dstlen );
HASH *http_hashHeader( char *head );
void http_deleteHeader( HASH *head );

void http_buf( NODE *nodeItem );
void http_read( NODE *nodeItem, char *p_cmd, char *p_url, char *p_proto, HASH *p_head );

int http_check( NODE *nodeItem, char *p_cmd, char *p_url, char *p_proto );
void http_code( NODE *nodeItem );
void http_keepalive( NODE *nodeItem, HASH *head );
void http_lastmodified( NODE *nodeItem, HASH *head );
void http_size( NODE *nodeItem, HASH *head );
void http_gate( NODE *nodeItem, HASH *head );
void http_send( NODE *nodeItem );

void http_404( NODE *nodeItem );
void http_304( NODE *nodeItem );
void http_200( NODE *nodeItem );
void http_206( NODE *nodeItem );
