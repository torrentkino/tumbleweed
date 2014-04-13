/*
Copyright 2006 Aiko Barz

This file is part of torrentkino.

torrentkino is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

torrentkino is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with torrentkino.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <signal.h>
#include <unistd.h>

#include "tumbleweed.h"
#include "str.h"
#include "malloc.h"
#include "list.h"
#include "node_tcp.h"
#include "log.h"
#include "file.h"
#include "unix.h"
#include "conf.h"

struct obj_conf *conf_init( int argc, char **argv ) {
	struct obj_conf *conf = (struct obj_conf *) myalloc( sizeof(struct obj_conf) );
	BEN *opts = opts_init();
	BEN *value = NULL;

	/* Parse command line */
	opts_load( opts, argc, argv );

	/* Mode */
	if( ben_dict_search_str( opts, "-f" ) != NULL ) {
		conf->mode = CONF_DAEMON;
	} else {
		conf->mode = CONF_CONSOLE;
	}

	/* Verbosity */
	if( ben_dict_search_str( opts, "-v" ) != NULL ) {
		conf->verbosity = CONF_VERBOSE;
	} else if ( ben_dict_search_str( opts, "-q" ) != NULL ) {
		conf->verbosity = CONF_BEQUIET;
	} else {
		/* Be verbose in the console and quiet while running as a daemon. */
		conf->verbosity = ( conf->mode == CONF_CONSOLE ) ?
			CONF_VERBOSE : CONF_BEQUIET;
	}

	/* Port */
	value = ben_dict_search_str( opts, "-p" );
	if( ben_is_str( value ) && ben_str_i( value ) >= 1 ) {
		conf->port = str_safe_port( (char *)ben_str_s( value ) );
	} else {
		if( getuid() == 0 ) {
			conf->port = PORT_WWW_PRIV;
		} else {
			conf->port = PORT_WWW_USER;
		}
	}
	if( conf->port == 0 ) {
		fail( "Invalid port number (-p)" );
	}

	/* Cores */
	conf->cores = unix_cpus();
	if( conf->cores < 1 || conf->cores > 128 ) {
		fail( "Invalid number of CPU cores" );
	}

	/* HOME */
	conf_home( conf, opts );

	/* HTML index */
	value = ben_dict_search_str( opts, "-i" );
	if( ben_is_str( value ) && ben_str_i( value ) >= 1 ) {
		snprintf( conf->file, BUF_SIZE, "%s", (char *)ben_str_s( value ) );
	} else {
		snprintf( conf->file, BUF_SIZE, "%s", CONF_INDEX_NAME );
	}
	if( !str_isValidFilename( conf->file ) ) {
		fail( "Index %s looks suspicious", conf->file );
	}

	opts_free( opts );

	return conf;
}

void conf_free( void ) {
	myfree( _main->conf );
}

void conf_home( struct obj_conf *conf, BEN *opts ) {
	BEN *value = NULL;

	value = ben_dict_search_str( opts, "-w" );
	if( ben_is_str( value ) && ben_str_i( value ) >= 1 ) {
		char *p = NULL;

		p = (char *)ben_str_s( value );

		/* Absolute path or relative path */
		if( *p == '/' ) {
			snprintf( conf->home, BUF_SIZE, "%s", p );
		} else if ( getenv( "PWD" ) != NULL ) {
			snprintf( conf->home, BUF_SIZE, "%s/%s", getenv( "PWD" ), p );
		} else {
			strncpy( conf->home, "/var/www", BUF_OFF1 );
		}
	} else {
		if( getenv( "HOME" ) == NULL || getuid() == 0 ) {
			strncpy( conf->home, "/var/www", BUF_OFF1 );
		} else {
			snprintf( conf->home, BUF_SIZE, "%s/%s", getenv( "HOME"), "Public" );
		}
	}

	if( !file_isdir( conf->home ) ) {
		fail( "%s does not exist", conf->home );
	}
}

void conf_print( void ) {
	if ( getenv( "PWD" ) == NULL || getenv( "HOME" ) == NULL ) {
		info( NULL, "# Hint: Reading environment variables failed. sudo?");
		info( NULL, "# This is not a problem. But in some cases it might be useful" );
		info( NULL, "# to use 'sudo -E' to export some variables like $HOME or $PWD." );
	}

	info( NULL, "Cores: %i", _main->conf->cores );

	if( _main->conf->mode == CONF_CONSOLE ) {
		info( NULL, "Mode: Console (-f)" );
	} else {
		info( NULL, "Mode: Daemon (-f)" );
	}

	if( _main->conf->verbosity == CONF_BEQUIET ) {
		info( NULL, "Verbosity: Quiet (-q/-v)" );
	} else {
		info( NULL, "Verbosity: Verbose (-q/-v)" );
	}

	info( NULL, "Workdir: %s (-w)", _main->conf->home );

	info( NULL, "Index file: %s (-i)", _main->conf->file );

	info( NULL, "Listen to TCP/%i (-p)", _main->conf->port );
}

int conf_verbosity( void ) {
	return _main->conf->verbosity;
}

int conf_mode( void ) {
	return _main->conf->mode;
}
