/*
Copyright 2009 Aiko Barz

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
#ifdef MAGIC
#include <magic.h>
#endif

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
#include "mime.h"

struct obj_mime *mime_init( void ) {
	struct obj_mime *mime = (struct obj_mime *) myalloc( sizeof(struct obj_mime), "mime_init" );
	mime->list = list_init();
	mime->hash = NULL;
	mime->mutex = thrd_init( "/mime" );
	return mime;
}

void mime_free( void ) {
	list_clear( _main->mime->list );
	list_free( _main->mime->list );
	hash_free( _main->mime->hash );
	thrd_destroy( _main->mime->mutex );
	myfree( _main->mime, "mime_free" );
}

struct obj_mimeItem *mime_add( const char *key, const char *value ) {
	struct obj_mimeItem *tuple = (struct obj_mimeItem *) myalloc( sizeof(struct obj_mimeItem), "mime_add" );

	strncpy( tuple->key, key, MIME_KEYLEN );
	strncpy( tuple->val, value, MIME_VALLEN );

	if( list_put( _main->mime->list, tuple) == NULL ) {
		log_fail( "mime_add: List exhausted" );
	}

	if( _main->mime->hash != NULL ) {
		hash_put( _main->mime->hash, (UCHAR *)tuple->key, strlen( tuple->key), tuple->val );
	}

	return tuple;
}

void mime_load( void ) {
	mime_add( "___", "application/octet-stream; charset=binary" );
	mime_add( "7z", "application/x-7z-compressed; charset=binary" );
	mime_add( "asf", "video/x-ms-asf; charset=binary" );
	mime_add( "asx", "video/x-ms-asf; charset=binary" );
	mime_add( "avi", "video/x-msvideo; charset=binary" );
	mime_add( "bz2", "application/x-bzip2; charset=binary" );
	mime_add( "css", "text/css" );
	mime_add( "deb", "application/x-debian-package; charset=binary" );
	mime_add( "divx", "video/x-msvideo; charset=binary" );
	mime_add( "dsc", "text/plain; charset=utf-8" );
	mime_add( "gif", "image/gif; charset=binary" );
	mime_add( "gpg", "application/pgp-signature; charset=utf-8" );
	mime_add( "gz", "application/x-gzip; charset=binary" );
	mime_add( "html", "text/html; charset=utf-8" );
	mime_add( "ico", "image/x-ico; charset=binary" );
	mime_add( "iso", "application/x-iso9660-image; charset=binary" );
	mime_add( "js", "application/x-javascript" );
	mime_add( "jpg", "image/jpeg; charset=binary" );
	mime_add( "jpeg", "image/jpeg; charset=binary" );
	mime_add( "m3u", "text/plain; charset=utf-8" );
	mime_add( "m4v", "video/mp4; charset=binary" );
	mime_add( "mkv", "video/x-matroska; charset=binary" );
	mime_add( "mov", "video/quicktime; charset=binary" );
	mime_add( "mp3", "audio/mpeg; charset=binary" );
	mime_add( "mp4", "video/mp4; charset=binary" );
	mime_add( "mpeg", "video/mpeg; charset=binary" );
	mime_add( "mpg", "video/mpeg; charset=binary" );
	mime_add( "nfo", "text/plain; charset=utf-8" );
	mime_add( "ogg", "application/ogg; charset=binary" );
	mime_add( "ogm", "application/ogg; charset=binary" );
	mime_add( "pdf", "application/pdf; charset=binary" );
	mime_add( "pl", "text/x-perl; charset=utf-8" );
	mime_add( "png", "image/png; charset=binary" );
	mime_add( "rar", "application/x-rar; charset=binary" );
	mime_add( "rss", "application/rss+xml; charset=utf-8" );
	mime_add( "txt", "text/plain; charset=utf-8" );
	mime_add( "wmv", "video/x-ms-asf; charset=binary" );
	mime_add( "woff", "application/font-woff" );
	mime_add( "xml", "application/xml; charset=utf-8" );
	mime_add( "zip", "application/zip; charset=binary" );
}

void mime_hash( void ) {
	ITEM *item = NULL;
	struct obj_mimeItem *tuple = NULL;
	
	if( list_size( _main->mime->list ) < 1 ) {
		return;
	}

	/* Create hash */
	_main->mime->hash = hash_init( list_size( _main->mime->list ) * 10 );

	/* Hash list */
	item = list_start( _main->mime->list );
	while( item != NULL ) {
		tuple = item->val;
		hash_put( _main->mime->hash, (UCHAR *)tuple->key, strlen( tuple->key), tuple->val );
		item = list_next( item );
	}
}

const char *mime_find( char *filename ) {
	char filecopy[MAIN_BUF+1];
	char *key = NULL;
	char *mime = NULL;

	/* Copy filename */
	snprintf( filecopy, MAIN_BUF+1, "%s", filename );

	/* Get extension */
	if( (key = mime_extension( filecopy)) == NULL ) {
		return hash_get( _main->mime->hash, (UCHAR *)"___", 3 );
	}

	/* Cached */
	if( (mime = hash_get( _main->mime->hash, (UCHAR *)key, strlen( key))) != NULL ) {
		return mime;
	}

#ifdef MAGIC
	/* Use magic to figure out the mime type */
	mime_magic( filename, key );

	/* Try again */
	if( (mime = hash_get( _main->mime->hash, (UCHAR *)key, strlen( key))) != NULL ) {
		return mime;
	}
#endif

	/* Last resort */
	return hash_get( _main->mime->hash, (UCHAR *)"___", 3 );
}

#ifdef MAGIC
void mime_magic( char *filename, char *key ) {
	struct obj_mimeItem *tuple = NULL;
	magic_t magic = NULL;
	const char *mime = NULL;

	if( ! file_isreg( filename) ) {
		return;
	}

	if( (magic = magic_open( MAGIC_ERROR|MAGIC_MIME)) == NULL ) {
		return;
	}
  
	if( magic_load( magic,NULL) != 0 ) {
		magic_close( magic );
		return;
	}

	if( (mime = magic_file( magic, filename)) == NULL ) {
		magic_close( magic );
		return;
	}
	
	/* Cache mime type in memory */
	thrd_block( _main->mime->mutex );
	tuple = mime_add( key, mime );
	thrd_unblock( _main->mime->mutex );
	
	/* Clear handle */
	magic_close( magic );

	log_info( NULL, 0, "New MIME: \"%s\" -> \"%s\"", tuple->key, tuple->val );
}
#endif

char *mime_extension( char *filename ) {
	char *key = filename;
	char *dot = filename;
	int i = 0;

	/* Search file ending */
	while( (dot = strchr( key, '.')) != NULL ) {
		key = dot+1;
		i++;
	}
	
	/* No '.' found */
	if( i == 0 ) {
		return NULL;
	}

	/* bla.7z.001, bla.7z.002 */
	if( str_isNumber( key) ) {
		/* Cut number */
		key--;
		*key = '\0';

		return mime_extension( filename );
	}

	/* Panic mode */
	if( strlen( key) > 5 ) {
		return NULL;
	}

	return key;
}
