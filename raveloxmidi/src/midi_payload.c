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

#include "midi_command.h"
#include "midi_payload.h"
#include "utils.h"

#include "logging.h"

void midi_payload_destroy( midi_payload_t **payload )
{
	if( ! payload ) return;
	if( ! *payload ) return;

	(*payload)->buffer = NULL;

	if( (*payload)->header )
	{
		FREENULL( (void **)&((*payload)->header) );
	}

	FREENULL( (void **)payload );
}

void midi_payload_reset( midi_payload_t *payload )
{
	if( ! payload ) return;

	if( payload->buffer ) free( payload->buffer );
	payload->buffer = NULL;

	if( payload->header )
	{
		payload->header->B = 0;
		payload->header->J = 0;
		payload->header->Z = 0;
		payload->header->P = 0;
		payload->header->len = 0;
	}
}

midi_payload_t * midi_payload_create( void )
{
	midi_payload_t *payload = NULL;

	payload = ( midi_payload_t * )malloc( sizeof( midi_payload_t ) );

	if( ! payload ) return NULL;

	memset( payload, 0, sizeof(midi_payload_t ) );

	payload->header = ( midi_payload_header_t *)malloc( sizeof( midi_payload_header_t ) );
	if( ! payload->header )
	{
		logging_printf( LOGGING_ERROR, "midi_payload_create: Not enough memory\n");
		free( payload );
		return NULL;
	}
	midi_payload_reset( payload );

	return payload;
}

void midi_payload_toggle_b( midi_payload_t *payload )
{
	if(! payload) return;

	payload->header->B ^= 1;
}

void midi_payload_toggle_j( midi_payload_t *payload )
{
	if( ! payload ) return;

	payload->header->J ^= 1;
}

void midi_payload_toggle_z( midi_payload_t *payload )
{
	if( ! payload ) return;

	payload->header->Z ^=1;
}

void midi_payload_toggle_p( midi_payload_t *payload )
{
	if( ! payload ) return;

	payload->header->P ^= 1;
}

void midi_payload_set_buffer( midi_payload_t *payload, unsigned char *buffer , uint16_t buffer_size)
{
	if( ! payload ) return;

	payload->header->len = buffer_size;
	payload->buffer = buffer;
}


void midi_payload_header_dump( midi_payload_header_t *header )
{
	if( ! header ) return;

	logging_printf( LOGGING_DEBUG, "MIDI Payload(\n");
	logging_printf( LOGGING_DEBUG, "\tB=%d\n", header->B);
	logging_printf( LOGGING_DEBUG, "\tJ=%d\n", header->J);
	logging_printf( LOGGING_DEBUG, "\tZ=%d\n", header->Z);
	logging_printf( LOGGING_DEBUG, "\tP=%d\n", header->P);
	logging_printf( LOGGING_DEBUG, "\tpayloadlength=%u )\n", header->len);
}

void midi_payload_pack( midi_payload_t *payload, unsigned char **buffer, size_t *buffer_size)
{
	uint8_t temp_header = 0;
	unsigned char *p = NULL;

	*buffer = NULL;
	*buffer_size = 0;

	if( ! payload ) return;

	if( ! payload->buffer ) return;
	if( ! payload->header ) return;

	midi_payload_header_dump( payload->header );

	*buffer_size = 1 + payload->header->len + (payload->header->len > 15 ? 1 : 0);
	*buffer = (unsigned char *)malloc( *buffer_size );

	if( ! *buffer )
	{
		logging_printf(LOGGING_ERROR, "midi_payload_pack: Insufficient memory\n");
		return;
	}

	p = *buffer;

	if( payload->header->B) temp_header |= PAYLOAD_HEADER_B;
	if( payload->header->J) temp_header |= PAYLOAD_HEADER_J;
	if( payload->header->Z) temp_header |= PAYLOAD_HEADER_Z;
	if( payload->header->P) temp_header |= PAYLOAD_HEADER_P;

	*p = temp_header;

	if( payload->header->len <= 15 )
	{
		*p |= ( payload->header->len & 0x0f );
		p++;
	} else {
		temp_header |= (payload->header->len & 0x0f00 ) >> 8;
		*p = temp_header;
		p++;
		*p =  (payload->header->len & 0x00ff);
		p++;
	}

	memcpy( p, payload->buffer, payload->header->len );
}

void midi_payload_unpack( midi_payload_t **payload, unsigned char *buffer, size_t buffer_len )
{
	unsigned char *p;
	uint16_t temp_len;
	size_t current_len;

	if( !buffer ) return;
	if( buffer_len == 0 ) return;

	*payload = midi_payload_create();
	if( ! *payload ) return;

	p = buffer;
	current_len = buffer_len;

	/* Get the flags */
	if( *p & PAYLOAD_HEADER_B ) midi_payload_toggle_b( *payload );
	if( *p & PAYLOAD_HEADER_J ) midi_payload_toggle_j( *payload );
	if( *p & PAYLOAD_HEADER_Z ) midi_payload_toggle_z( *payload );
	if( *p & PAYLOAD_HEADER_P ) midi_payload_toggle_p( *payload );

	/* Check that there's enough buffer if the B flag indicates the length field is 12 bits */
	if( (*payload)->header->B && ( current_len == 1 ) )
	{
		logging_printf(LOGGING_ERROR, "midi_payload_unpack: B flag set but insufficent buffer data\n" );
		goto midi_payload_unpack_error;
	} 

	temp_len = ( *p & 0x0f );
	current_len--;

	/* If the B flag is set, get the next octect */
	if( (*payload)->header->B )
	{
		p++;
		current_len--;
		temp_len <<= 8;
		temp_len += *p;
	}

	/* Check that there's enough buffer for the defined length */
	(*payload)->header->len = temp_len;
	p++;


	if( current_len < temp_len ) 
	{
		logging_printf(LOGGING_ERROR, "midi_payload_unpack: Insufficent buffer data : current_len=%zu temp_len=%u\n", current_len, temp_len );
		goto midi_payload_unpack_error;
	}

	(*payload)->buffer = (unsigned char *)malloc( temp_len );
	if( ! (*payload)->buffer ) goto midi_payload_unpack_error;

	memcpy( (*payload)->buffer, p, temp_len );

	goto midi_payload_unpack_success;

midi_payload_unpack_error:
	midi_payload_destroy( payload );
	*payload = NULL;

midi_payload_unpack_success:
	return;
}

void midi_payload_to_commands( midi_payload_t *payload, midi_command_t **commands, size_t *num_commands )
{
	unsigned char *p;
	size_t current_len;
	uint64_t current_delta;
	midi_command_t *c;
	unsigned char data_byte;
	char *command_description;
	enum midi_message_type_t message_type;

	*commands = NULL;
	*num_commands = 0;

	if( ! payload ) return;

	if( ! payload->header ) return;
	if( ! payload->buffer ) return;

	p = payload->buffer;
	current_len = payload->header->len;
	
	do 
	{
		logging_printf( LOGGING_DEBUG, "Current Len: %zu\n", current_len );
		current_delta = 0;
		/* If the Z flag == 0 then no delta time is present for the first midi command */
		if( ( payload->header->Z == 0 ) && ( *num_commands == 0 ) )
		{ 
			/*Do nothing*/
		} else {
			// Get the delta
			do
			{
				data_byte = *(p++);
				current_delta << 8;
				current_delta += ( data_byte & 0x7f );
				current_len--;
			} while ( ( data_byte & 0x80 ) || current_len > 0 );
		}

		(*num_commands)++;

		*commands = (midi_command_t * ) realloc( *commands, sizeof( midi_command_t * ) * *num_commands );

		c = midi_command_create();
		commands[ *num_commands - 1 ] = c;

		c->delta = current_delta;
		if( current_len > 0 )
		{
			data_byte = *(p++);
			current_len--;

			c->status = data_byte;
		}

		midi_command_map( c , &command_description, &message_type );
		logging_printf( LOGGING_DEBUG, "midi command(%u): delta=%zu command=%s\n", *num_commands, c->delta, command_description);

		switch( message_type )
		{
			case MIDI_NOTE_OFF:
			case MIDI_NOTE_ON:
			case MIDI_POLY_PRESSURE:
			case MIDI_CONTROL_CHANGE:
			case MIDI_PITCH_BEND:
				if( current_len >= 2 )
				{
					c->data = ( unsigned char * ) malloc( 2 );
					memcpy( c->data, p, 2 );
					current_len -= 2;
					p+=2;
				}
				break;
			case MIDI_PROGRAM_CHANGE:
			case MIDI_CHANNEL_PRESSURE:
				if( current_len >= 1 )
				{
					c->data = ( unsigned char * ) malloc( 1 );
					memcpy( c->data, p, 1);
					current_len -= 1;
					p+=1;
				}
				break;
		}
		
		switch( message_type )
		{
			case MIDI_NOTE_OFF:
			case MIDI_NOTE_ON:
			case MIDI_POLY_PRESSURE:
			case MIDI_CONTROL_CHANGE:
			case MIDI_PITCH_BEND:
			case MIDI_PROGRAM_CHANGE:
			case MIDI_CHANNEL_PRESSURE:
				logging_printf( LOGGING_DEBUG, "\tChannel: %u\n", c->status & 0x0f );
				break;
		}

	} while( current_len > 0 );
}
