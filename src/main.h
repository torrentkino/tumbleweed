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

#define MAIN_BUF 1023
#define MAIN_PROTVER 1
#define MAIN_IPBUF 39
#define SHA_DIGEST_LENGTH 20

#define TRUE 1
#define FALSE 0

#define GAMEOVER 0
#define RUMBLE 1

typedef unsigned char UCHAR;
typedef unsigned long int ULONG;
typedef struct sockaddr_in6 IP;

struct obj_main {
	int argc;
	char **argv;

	/* Data */
	struct obj_conf	*conf;
#ifdef TUMBLEWEED
	struct obj_mime *mime;
	struct obj_tcp *tcp;
	struct obj_nodes *nodes;
#elif MASALA
	struct obj_transaction *transaction;
	struct obj_infohash *infohash;
	struct obj_token *token;
	struct obj_nbhd *nbhd;
	struct obj_udp *udp;
	struct obj_p2p *p2p;
#endif

	/* Thread terminater */
	int status;

	/* Catch signals */
	struct sigaction sig_stop;
	struct sigaction sig_time;
};

extern struct obj_main *_main;

struct obj_main *main_init( int argc, char **argv );
void main_free( void );
