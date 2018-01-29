/*
   This file is part of raveloxmidi.

   Copyright (C) 2014 Dave Kelly

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA 
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#include "midi_note_packet.h"
#include "midi_journal.h"
#include "chapter_n.h"
#include "chapter_c.h"
#include "utils.h"

#include "logging.h"

void journal_header_pack( journal_header_t *header , char **packed , size_t *size )
{
	char *p = NULL;

	*packed = NULL;
	*size = 0;

	if( ! header ) return;

	*packed = ( char *)malloc( JOURNAL_HEADER_PACKED_SIZE );

	if( ! packed ) return;
	memset( *packed, 0 , JOURNAL_HEADER_PACKED_SIZE );

	p = *packed;

	*p |= ( ( header->bitfield & 0x0f ) << 4 );
	*p |= ( ( header->totchan == 0 ? 0 : header->totchan - 1 ) & 0x0f ) ;

	p += sizeof( char );
	*size += sizeof( char );

	put_uint16( (unsigned char **)&p, header->seq, size );
}

journal_header_t * journal_header_create( void )
{
	journal_header_t *journal_header = NULL;

	journal_header = ( journal_header_t * ) malloc( sizeof( journal_header_t ) );
	
	if( journal_header )
	{
		memset( journal_header , 0 , sizeof( journal_header_t ) );
	}

	return journal_header;
}

void journal_header_destroy( journal_header_t **header )
{
	FREENULL( (void **)header );
}


void channel_header_pack( channel_header_t *header , unsigned char **packed , size_t *size )
{
	unsigned char *p = NULL;
	uint16_t temp_header = 0;

	*packed = NULL;
	*size = 0;

	if( ! header ) return;

	*packed = ( unsigned char *)malloc( CHANNEL_HEADER_PACKED_SIZE );

	if( ! packed ) return;

	memset( *packed, 0, CHANNEL_HEADER_PACKED_SIZE );

	p = *packed;

	temp_header |= ( ( header->S & 0x01 ) << 15 );
	temp_header |= ( ( ( header->chan == 0 ? 0 : header->chan - 1 ) & 0x0f ) << 11 );
	temp_header |= ( ( header->H & 0x01 ) << 10 );
	temp_header |= ( ( header->len & 0x03ff ) );

	put_uint16( &p, temp_header, size );

	*p = header->bitfield;
	*size += sizeof( header->bitfield );
}

void channel_header_destroy( channel_header_t **header )
{
	FREENULL( (void **)header);
}

channel_header_t * channel_header_create( void )
{
	channel_header_t *header = NULL;

	header = ( channel_header_t * ) malloc( sizeof( channel_header_t ) );

	if( header )
	{
		memset( header, 0 , sizeof( channel_header_t ) );
	}

	return header;
}

void channel_pack( channel_t *channel, char **packed, size_t *size )
{
	unsigned char *packed_channel_header = NULL;
	char *packed_chaptern = NULL;
	
	size_t packed_channel_header_size = 0;
	size_t packed_chaptern_size = 0;

	char *p = NULL;

	*packed = NULL;
	*size = 0;


	if( ! channel ) return;

	chaptern_pack( channel->chaptern, &packed_chaptern, &packed_chaptern_size );
	channel->header->len = packed_chaptern_size + CHANNEL_HEADER_PACKED_SIZE;

	channel_header_pack( channel->header, &packed_channel_header, &packed_channel_header_size );


	*packed = ( char * ) malloc( packed_channel_header_size + packed_chaptern_size );

	if( ! packed ) goto channel_pack_cleanup;

	p = *packed;

	memcpy( p, packed_channel_header, packed_channel_header_size );
	*size += packed_channel_header_size;
	p += packed_channel_header_size;
	
	memcpy( p, packed_chaptern, packed_chaptern_size );
	*size += packed_chaptern_size;

channel_pack_cleanup:
	FREENULL( (void **)&packed_channel_header );
	FREENULL( (void **)&packed_chaptern );
}

void channel_destroy( channel_t **channel )
{
	if( ! channel ) return;
	if( ! *channel ) return;

	if( (*channel)->chaptern )
	{
		chaptern_destroy( &( (*channel)->chaptern ) );
	}

	if( (*channel)->header )
	{
		channel_header_destroy( &( (*channel)->header ) );
	}

	free( *channel );
	*channel = NULL;
}

channel_t * channel_create( void )
{
	channel_t *new_channel = NULL;
	channel_header_t *new_header = NULL;
	chaptern_t *new_chaptern = NULL;
	
	new_channel = ( channel_t * ) malloc( sizeof( channel_t ) );

	if( ! new_channel ) return NULL;

	new_header = channel_header_create();

	if( ! new_header )
	{
		channel_destroy( &new_channel );
		return NULL;
	}

	new_channel->header = new_header;

	new_chaptern = chaptern_create();

	if( ! new_chaptern )
	{
		channel_destroy( &new_channel );
		return NULL;
	}

	new_channel->chaptern = new_chaptern;
		
	return new_channel;
}

void journal_pack( journal_t *journal, char **packed, size_t *size )
{
	char *packed_journal_header = NULL;
	size_t packed_journal_header_size = 0;
	char *packed_channel = NULL;
	size_t packed_channel_size = 0;
	char *packed_channel_buffer = NULL;	
	size_t packed_channel_buffer_size = 0;
	char *p = NULL;
	int i = 0;

	*packed = NULL;
	*size = 0;

	if( ! journal ) return;

	logging_printf( LOGGING_DEBUG, "journal_has_data = %s\n", ( journal_has_data( journal )  ? "YES" : "NO" ) );
	if(  ! journal_has_data( journal ) ) return;

	journal_header_pack( journal->header, &packed_journal_header, &packed_journal_header_size );

	for( i = 0 ; i < MAX_MIDI_CHANNELS ; i++ )
	{
		if( ! journal->channels[i] ) continue;

		channel_pack( journal->channels[i], &packed_channel, &packed_channel_size );

		packed_channel_buffer = ( char * )realloc( packed_channel_buffer, packed_channel_buffer_size + packed_channel_size );
		if( ! packed_channel_buffer ) goto journal_pack_cleanup;
		p = packed_channel_buffer + packed_channel_buffer_size;
		memset( p, 0, packed_channel_size );
		packed_channel_buffer_size += packed_channel_size;
		memcpy( packed_channel_buffer, packed_channel, packed_channel_size );

		FREENULL( (void **)&packed_channel );
	}

	// Join it all together

	*packed = ( char * )malloc( packed_journal_header_size + packed_channel_buffer_size );
	p = *packed;
	memcpy( p, packed_journal_header, packed_journal_header_size );
	*size += packed_journal_header_size;
	p += packed_journal_header_size;

	memcpy( p, packed_channel_buffer, packed_channel_buffer_size );
	*size += packed_channel_buffer_size;
	
journal_pack_cleanup:
	FREENULL( (void **)&packed_channel );
	FREENULL( (void **)&packed_channel_buffer );
	FREENULL( (void **)&packed_journal_header );
}

int journal_init( journal_t **journal )
{
	journal_header_t *header;
	unsigned char i;

	*journal = ( journal_t * ) malloc( sizeof ( journal_t ) );

	if( ! journal )
	{
		return -1;
	}

	header = journal_header_create();
		
	if( ! header )
	{
		free( *journal );
		*journal = NULL;
		return -1;
	}

	(*journal)->header = header;

	for( i = 0 ; i < MAX_MIDI_CHANNELS ; i++ )
	{
		(*journal)->channels[i] = NULL;
	}

	return 0;
}

void journal_destroy( journal_t **journal )
{
	unsigned int i = 0;

	if( ! journal ) return;
	if( ! *journal) return;

	for( i = 0 ; i < MAX_MIDI_CHANNELS ; i++ )
	{
		if( (*journal)->channels[i] )
		{
			channel_destroy( &( (*journal)->channels[i] ) );
		}
	}

	if( (*journal)->header )
	{
		journal_header_destroy( &( (*journal)->header ) );
	}

	free( *journal );
	*journal = NULL;
}

void midi_journal_add_note( journal_t *journal, uint32_t seq, midi_note_packet_t *note_packet)
{
	uint16_t note_slot = 0;
	midi_note_t *new_note = NULL;
	unsigned char channel = 0;

	if( ! journal ) return;
	if( ! note_packet ) return;

	channel = note_packet->channel;
	if( channel > MAX_MIDI_CHANNELS ) return;


	// Set Journal Header A flag
	journal->header->bitfield |= 0x02;
	
	if( ! journal->channels[ channel ] )
	{
		channel_t *channel_journal = channel_create();

		if( ! channel_journal ) return;
		journal->channels[ channel ] = channel_journal;

	}

	journal->channels[ channel ]->header->bitfield |= CHAPTER_N;
	journal->channels[ channel ]->chaptern->header->B = 1;

	if( journal->channels[ channel ]->header->chan != ( channel + 1 ) )
	{
		journal->channels[ channel ]->header->chan = ( channel + 1 );
		journal->header->totchan +=1;
	}

	journal->header->seq = seq;

	// Need to update NOTE OFF bits if the command is NOTE OFF
	if( note_packet->command == MIDI_COMMAND_NOTE_OFF )
	{
		uint8_t offset, shift;

		// Which element
		offset = (note_packet->note) / 8;
		shift = ( (note_packet->note) - ( offset * 8 )) - 1;

		// Set low and high values;
		journal->channels[ channel ]->chaptern->header->high = MAX( offset , journal->channels[ channel ]->chaptern->header->high );
		journal->channels[ channel ]->chaptern->header->low = MIN( offset , journal->channels[ channel ]->chaptern->header->low );

		journal->channels[ channel ]->chaptern->offbits[offset] |=  ( 1 << shift );

		return;
	}

	if( journal->channels[ channel ]->chaptern->num_notes == MAX_CHAPTERN_NOTES ) return;

	new_note = midi_note_create();
	if(! new_note ) return;

	new_note->num = note_packet->note;
	new_note->velocity = note_packet->velocity;

	note_slot = journal->channels[ channel ]->chaptern->num_notes;

	journal->channels[ channel ]->chaptern->notes[note_slot] = new_note;
	journal->channels[ channel ]->chaptern->num_notes++;
}

void channel_header_dump( channel_header_t *header )
{
	if( ! header ) return;

	logging_printf( LOGGING_DEBUG, "Channel #%d (Header: S=%d H=%d len=%u bitfield=%02x)\n", header->chan, header->S, header->H, header->len, header->bitfield);
}

void channel_header_reset( channel_header_t *header )
{
	if( ! header ) return;

	header->chan = 0;
	header->S = 0;
	header->H = 0;
	header->bitfield = 0;
}

void channel_journal_dump( channel_t *channel )
{
	if( ! channel ) return;

	channel_header_dump( channel->header );

	if( channel->header->bitfield & CHAPTER_N )
	{
		chaptern_dump( channel->chaptern );
	}
}

void channel_journal_reset( channel_t *channel )
{
	if( ! channel ) return;

	chaptern_reset( channel->chaptern );
	channel_header_reset( channel->header );
}

int journal_has_data( journal_t *journal )
{
	if( ! journal ) return 0;
	if( ! journal->header ) return 0;

	return (journal->header->totchan > 0);
}

void journal_header_dump( journal_header_t *header )
{
	if( ! header ) return;

	logging_printf( LOGGING_DEBUG, "Journal (Header: bitfield=%02x totchan=%d seq=%04x)\n", header->bitfield, header->totchan, header->seq);
	logging_printf( LOGGING_DEBUG, "Header Size = %u\n", sizeof( journal_header_t ) );
}

void journal_header_reset( journal_header_t *header )
{
	if( ! header ) return;

	header->bitfield = 0;
	header->totchan = 0;
	header->seq = 0;
}

void journal_dump( journal_t *journal )
{
	unsigned int i = 0;
	if( ! journal ) return;

	journal_header_dump( journal->header );

	for( i = 0 ; i < MAX_MIDI_CHANNELS ; i++ )
	{
		channel_journal_dump( journal->channels[i] );
	}
}

void journal_reset( journal_t *journal )
{
	unsigned int i = 0;

	if( !journal ) return;


	for( i = 0 ; i < MAX_MIDI_CHANNELS ; i++ )
	{
		channel_journal_reset( journal->channels[i] );
	}

	journal_header_reset( journal->header );

}
