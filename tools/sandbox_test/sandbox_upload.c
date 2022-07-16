#include <stdio.h>
#include <stdint.h>

#include "../hidapi.h"
#include "../hidapi.c"

#define VID 0x303a
#define PID 0x4004

hid_device * hd;
const int chunksize = 224;
const int alignlen = 4;
int tries = 0;


int DoUpload( const char * file, uint32_t address )
{

	uint8_t * blob;
	struct stat sbuf;
	stat( file, &sbuf);
	int bloblen = sbuf.st_size;
	int blobkclen = ( bloblen + alignlen - 1 ) / alignlen * alignlen;

	{
		FILE * binary_blob = fopen( file, "r" );
		if( !binary_blob )
		{
			fprintf( stderr, "ERROR: Could not open %s.\n", file );
			return -96;
		}
		blob = calloc( blobkclen, 1 );
		if( fread( blob, bloblen, 1, binary_blob ) != 1 )
		{
			fprintf( stderr, "ERROR: Could not read %s.\n", file );
			return -97;
		}
		fclose( binary_blob );
	}

	// Now, we have blob and bloblen.

	{
		// MAGFest Swadge Controller is 303a:4004

		if( !hd )
		{
			fprintf( stderr, "Error: Couldn't open VID/PID %04x:%04x\n", VID, PID );
			return -5;
		}
		// Commands:
		// 4 = Write
		// 5 = Read
		// 6 = Call

		// TODO: Get mechanism to request scratch buffer location.

		int chunks = ( bloblen + chunksize - 1 ) / chunksize;

		int i;
		for( i = 0; i < chunks; i++ )
		{
			uint32_t offset = i * chunksize + address;
			uint8_t rdata[chunksize+6];
			rdata[0] = 170;  // Code for advanced USB control
			rdata[1] = 4;    // Command #
			rdata[2] = offset & 0xff;
			rdata[3] = offset >> 8;
			rdata[4] = offset >> 16;
			rdata[5] = offset >> 24;
			int writelen = blobkclen - i * chunksize;
			if( writelen > chunksize )
				writelen = chunksize;
			memcpy( rdata + 6, blob + offset - address, writelen );
			printf( "Writing %d bytes into 0x%08x\n", writelen, (uint32_t)offset );

			int r;
			do
			{
				r = hid_send_feature_report( hd, rdata, writelen+6  );
				if( tries++ > 10 ) { fprintf( stderr, "Error: failed to write into scratch buffer %d (%d)\n", rdata[1], r ); return -94; }
			} while ( r != writelen+6 );
			tries = 0;
		}
	}
	return 0;
}


int main()
{
	hid_init();
	hd = hid_open( VID, PID, 0);
	if( !hd ) { fprintf( stderr, "Could not open USB\n" ); return -94; }

	// Parse through sandbox_symbols.txt and find the sandbox, itself.
	uint32_t sandbox_main_address = 0;
	uint32_t sandbox_mode_address = 0;
	uint32_t sandbox_start_address_inst = 0;
	uint32_t sandbox_start_address_data = 0;
	{
		FILE * f = fopen( "build/sandbox_symbols.txt", "r" );
		if( !f || ferror( f ) )
		{
			fprintf( stderr, "Error: could not get symbols\n" );
			return -5;
		}

		char * line = 0;
		size_t n = 0;

		while( !feof( f ) )
		{
			ssize_t llen = getline( &line, &n, f );
			
			char addy[128], prop[128], V[128], sec[128], size[128], name[1024];
			int l = sscanf( line, "%127s %127s %127s %127s %127s %1023s\n", addy, prop, V, sec, size, name );
			if( strcmp( name, "sandbox_main" ) == 0 && l == 6 )
			{
				sandbox_main_address = strtol( addy, 0, 16 );
				printf( "Found sandbox_main at 0x%08x\n", sandbox_main_address );
			}
			if( strcmp( name, "sandbox_mode" ) == 0 && l == 6 )
			{
				sandbox_mode_address = strtol( addy, 0, 16 );
				printf( "Found sandbox_mode at 0x%08x\n", sandbox_mode_address );
			}
			if( strcmp( size, "sandbox_sentinel_start_inst" ) == 0 && l == 5 )
			{
				sandbox_start_address_inst = strtol( addy, 0, 16 );
				printf( "Found sandbox_sentinel_start_data at 0x%08x\n", sandbox_start_address_inst );
			}
			if( strcmp( size, "sandbox_sentinel_start_data" ) == 0 && l == 5 )
			{
				sandbox_start_address_data = strtol( addy, 0, 16 );
				printf( "Found sandbox_sentinel_start_data at 0x%08x\n", sandbox_start_address_data );
			}

		}
		fclose( f );
	}

	if( sandbox_main_address == 0 )
	{
		fprintf( stderr, "Error: sandbox_main could not be found (%d)\n", sandbox_main_address );
		return -91;
	}

	int r;

	// Disable mode.
	uint8_t rdata[6];
	rdata[0] = 170;
	rdata[1] = 7;
	rdata[2] = 0 & 0xff;
	rdata[3] = 0 >> 8;
	rdata[4] = 0 >> 16;
	rdata[5] = 0 >> 24;
	do
	{
		r = hid_send_feature_report( hd, rdata, 6 );
		if( tries++ > 10 ) { fprintf( stderr, "Error sending feature report on command %d (%d)\n", rdata[1], r ); return -85; }
	} while ( r != 6 );
	tries = 0;

	// Give it a chance to exit.
	usleep( 20000 );

	r = DoUpload( "build/sandbox_inst.bin", sandbox_start_address_inst );
	if( r ) return r;

	r = DoUpload( "build/sandbox_data.bin", sandbox_start_address_data );
	if( r ) return r;

	printf( "Upload complete.  Starting main: 0x%08x\n", sandbox_main_address );

/*
	// Issue execute.
	rdata[0] = 170;
	rdata[1] = 6;
	rdata[2] = sandbox_main_address & 0xff;
	rdata[3] = sandbox_main_address >> 8;
	rdata[4] = sandbox_main_address >> 16;
	rdata[5] = sandbox_main_address >> 24;
	do
	{
		r = hid_send_feature_report( hd, rdata, 6 );
		if( tries++ > 10 ) { fprintf( stderr, "Error sending feature report on command %d (%d)\n", rdata[1], r ); return -85; }
	} while ( r != 6 );
	tries = 0;
*/
	printf( "Adding mode address: 0x%08x\n", sandbox_mode_address );

	// Set tick.
	rdata[0] = 170;
	rdata[1] = 7;
	rdata[2] = sandbox_mode_address & 0xff;
	rdata[3] = sandbox_mode_address >> 8;
	rdata[4] = sandbox_mode_address >> 16;
	rdata[5] = sandbox_mode_address >> 24;
	do
	{
		r = hid_send_feature_report( hd, rdata, 6 );
		if( tries++ > 10 ) { fprintf( stderr, "Error sending feature report on command %d (%d)\n", rdata[1], r ); return -85; }
	} while ( r != 6 );
	tries = 0;

	hid_close( hd );
}
