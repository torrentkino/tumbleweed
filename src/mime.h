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

#define MIME_KEYLEN 8
#define MIME_VALLEN 50

struct obj_mime {
	LIST *list;
	HASH *hash;
	sem_t *mutex;
};

struct obj_mimeItem {
	char key[MIME_KEYLEN+1];
	char val[MIME_VALLEN+1];
};

struct obj_mime *mime_init( void );
void mime_free( void );

struct obj_mimeItem *mime_add( const char *key, const char *value );
void mime_load( void );
void mime_hash( void );
const char *mime_find( char *filename );
#ifdef MAGIC
void mime_magic( char *filename, char *key );
#endif
char *mime_extension( char *filename );
