/*
	luksrku - Tool to remotely unlock LUKS disks using TLS.
	Copyright (C) 2016-2016 Johannes Bauer

	This file is part of luksrku.

	luksrku is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; this program is ONLY licensed under
	version 3 of the License, later versions are explicitly excluded.

	luksrku is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with luksrku; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

	Johannes Bauer <JohannesBauer@gmx.de>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "server.h"
#include "client.h"
#include "openssl.h"
#include "binkeyfile.h"
#include "cmdline.h"
#include "log.h"
#include "keyfile.h"

int main(int argc, char **argv) {
#ifdef DEBUG
	fprintf(stderr, "WARNING: This has been compiled in DEBUG mode and uses reduced security.\n");
#endif

	struct options_t options;
	if (!parse_cmdline_arguments(&options, argc, argv)) {
		print_syntax(argv[0]);
		exit(EXIT_FAILURE);
	}
	if (options.verbose) {
		log_setlvl(LLVL_DEBUG);
	}

	if (!openssl_init()) {
		log_msg(LLVL_FATAL, "Could not initialize OpenSSL.");
		exit(EXIT_FAILURE);
	}

	struct keydb_t keydb;
	bool success = true;
	memset(&keydb, 0, sizeof(keydb));
	do {
		if (!read_binary_keyfile(options.keydbfile, &keydb)) {
			log_msg(LLVL_FATAL, "Could not read key database file %s.", options.keydbfile);
			success = false;
			break;
		}

		log_msg(LLVL_DEBUG, "Successfully loaded key database file %s with %d entries and %d disk keys.", options.keydbfile, keydb.entrycnt, keydb_disk_key_count(&keydb));
#ifdef DEBUG
		keydb_dump(&keydb);
#endif
		
		if (keydb.entrycnt == 0) {
			log_msg(LLVL_FATAL, "Key database file %s contains no keys.", options.keydbfile);
			success = false;
			break;
		}

		if (options.mode == SERVER_MODE) {
			if (keydb.entrycnt != 1) {
				log_msg(LLVL_FATAL, "Server configuration files need to have exactly one host entry.");
				success = false;
				break;
			}
			
			if (keydb_disk_key_count(&keydb) != 0) {
				log_msg(LLVL_FATAL, "Server configuration files may not contain disk unlocking keys.");
				success = false;
				break;
			}
			
			if (!dtls_server(keydb_getentry(&keydb, 0), &options)) {
				log_msg(LLVL_FATAL, "Failed to start DTLS server.");
				success = false;
				break;
			}
		} else {
			if (!dtls_client(&keydb, &options)) {
				log_msg(LLVL_FATAL, "Failed to connect DTLS client.");
				success = false;
				break;
			}
		}
	} while (false);
	
	keydb_free(&keydb);
	if (!success) {
		exit(EXIT_FAILURE);
	}

	return 0;
}

