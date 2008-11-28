/*
 * Copyright (c) 1997 - 2001 Hansj�rg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

#include <string.h>

#include "../../simdebug.h"
#include "gui_textinput.h"
#include "../../simevent.h"
#include "../../simgraph.h"
#include "../../simcolor.h"
#include "../../simwin.h"
#include "../../unicode.h"

#include "../../dataobj/translator.h"

#include "../../simgraph.h"

gui_textinput_t::gui_textinput_t()
{
	max = 0;
	text = NULL;
	cursor_pos = 0;
	align = ALIGN_LEFT;
	set_read_only(false);
}


gui_textinput_t::~gui_textinput_t()
{
	release_focus(this);
	text = NULL;
}


/**
 * Events werden hiermit an die GUI-Komponenten
 * gemeldet
 * @author Hj. Malthaner
 */
void gui_textinput_t::infowin_event(const event_t *ev)
{
	if(ev->ev_class == EVENT_KEYBOARD) {
		if((text != NULL) && has_focus(this)) {
			const long len = strlen(text);

			switch(ev->ev_code) {
				case SIM_KEY_DOWN: // down arrow
					// not used currently
					break;
				case SIM_KEY_LEFT: // left arrow
					if (cursor_pos > 0) {
						cursor_pos = get_prev_char(text, cursor_pos);
					}
					break;
				case SIM_KEY_RIGHT: // right arrow
					if (cursor_pos >= 0) {
						cursor_pos = get_next_char(text, cursor_pos);
					}
					if (cursor_pos > strlen(text)) {
						cursor_pos = strlen(text);
					}
					break;
				case SIM_KEY_UP: // rightarrow
					// not used currently
					break;
				case SIM_KEY_HOME: // home
					cursor_pos = 0;
					break;
				case SIM_KEY_END: // end
					cursor_pos = strlen(text);
					break;
				case 8:
					// backspace
					if(cursor_pos > 0) {
						if (cursor_pos < len) {
							int prev_pos = cursor_pos;
							cursor_pos = get_prev_char(text, cursor_pos);
							for (int pos = cursor_pos; pos <= len-(prev_pos-cursor_pos); pos++) {
								text[pos] = text[pos+(prev_pos-cursor_pos)];
							}
						} else {
							cursor_pos = get_prev_char(text, cursor_pos);
							text[cursor_pos] = 0;
						}
					}
					break;
				case 127:
					// delete
					if (cursor_pos <= len) {
							int next_pos = get_next_char(text, cursor_pos);
							for (int pos = cursor_pos; pos < len; pos++) {
								text[pos] = text[pos+(next_pos-cursor_pos)];
							}
					}
					break;
				case 13:
					if(has_focus(this)) {
						release_focus(this);
						call_listeners((long)0);
					}
					break;
				case 27:
					// escape - release focus so that event gets passed to window which will then close
					if(has_focus(this)) {
						release_focus(this);
					}
					break;
				default:
					if(ev->ev_code < 32) {
						// ignore special keys not handled so far
						break;
					}
					// Buchstaben, Ziffern und Sonderzeichen einf�gen:

					// test, if we have top convert letter
					char letter[8];

					if(ev->ev_code>=128) {
						sprintf( letter, "CHR%X", ev->ev_code );
			//DBG_MESSAGE( "gui_textinput_t::gui_textinput_t()","%i=%s",ev->ev_code,letter);
						const char *more_letter=translator::translate(letter);
						// could not convert ...
						if(letter==more_letter) {
							if (ev->ev_code > 279 || (ev->ev_code <= 255 && translator::get_lang()->utf_encoded)) {
								// assume unicode
								char *out=letter;
								out[ utf16_to_utf8(ev->ev_code, (utf8 *)out) ] = 0;
							}
							else {
								// 0..255, but no translation => assume extended code page
								letter[0] = ev->ev_code;
								letter[1] = 0;
							}
						}
						else {
							// successful converted letter
							strcpy( letter, more_letter );
						}
					}
					else {
						letter[0] = ev->ev_code;
						letter[1] = 0;
					}

					int num_letter = strlen(letter);

					if(len+num_letter>=max) {
						// too many chars ...
						break;
					}

					// insert into text?
					if (cursor_pos < len) {
						for (int pos = len+num_letter; pos >= cursor_pos; pos--) {
							text[pos] = text[pos-num_letter];
						}
						memcpy( text+cursor_pos, letter, num_letter );
					} else {
						// append to text
						memcpy( text+len, letter, num_letter );
						text[len+num_letter] = 0;
					}
					cursor_pos += num_letter;
					/* end default */
			}
		} else {
			printf("Warning: gui_textinput_t::infowin_event() called but text is NULL\n");
		}
	} else if ( IS_LEFTCLICK(ev) ) 	{
		// acting on release causes unwanted recalculations of cursor position for long strings and (cursor_offset>0)
		// moreover, only (click) or (release) event happened inside textinput, the other one could lie outside
		if(!has_focus(this)) {
			request_focus(this);
		}
		cursor_pos = 0;
		for( int i=strlen(text); i>0;  i-- ) {
			if(ev->cx+cursor_offset > display_calc_proportional_string_len_width(text,i)) {
				cursor_pos = i;
				break;
			}
		}
DBG_DEBUG("gui_textinput_t::gui_textinput_t()","cursor_pos=%i, cx=%i",cursor_pos,ev->cx);
	} else if(  ev->ev_class == INFOWIN && ev->ev_code == WIN_CLOSE  &&  has_focus(this)  ) {
		release_focus(this);
	}
}


/**
 * Zeichnet die Komponente
 * @author Hj. Malthaner
 */
void gui_textinput_t::zeichnen(koord offset)
{
	display_fillbox_wh_clip(pos.x+offset.x+1, pos.y+offset.y+1,groesse.x-2, groesse.y-2, MN_GREY1, true);
	display_ddd_box_clip(pos.x+offset.x, pos.y+offset.y,groesse.x, groesse.y,MN_GREY0, MN_GREY4);

	if(text) {
		// align_offset = len of str for right align, otherwise 0
		int align_offset = (align == ALIGN_RIGHT) ? proportional_string_width(text) : 0;

		cursor_offset = proportional_string_len_width(text, cursor_pos);
		if (cursor_offset > groesse.x - 2) {
			cursor_offset -= (groesse.x - 3);
		}
		else {
			cursor_offset = 0;
			if (align == ALIGN_RIGHT){
				cursor_offset = align_offset-(groesse.x - 3);
				if (cursor_offset > 0) {
					cursor_offset = 0;
				}
			}
		}

		// set clipping to be within textinput button
		struct clip_dimension original_clipping = display_gib_clip_wh();
		int text_clip_x = pos.x+offset.x + 1, text_clip_w = groesse.x - 2;

		// check is within existing clipping
		if(text_clip_x < original_clipping.x)
		{
			text_clip_w -= original_clipping.x - text_clip_x;
			text_clip_x = original_clipping.x;
		}
		text_clip_x + text_clip_w > original_clipping.xx ? text_clip_w = original_clipping.xx - text_clip_x : 0;

		display_setze_clip_wh(text_clip_x, original_clipping.y, text_clip_w, original_clipping.h);

		display_proportional_clip(pos.x+offset.x+2-cursor_offset+align_offset, pos.y+offset.y+2, text, align, COL_BLACK, true);

		// cursor must been shown, if textinput has focus!
		if(has_focus(this)) {
			display_fillbox_wh_clip(pos.x+offset.x+1+proportional_string_len_width(text, cursor_pos)-cursor_offset, pos.y+offset.y+1, 1, 11, COL_WHITE, true);
		}

		// reset clipping
		display_setze_clip_wh(original_clipping.x, original_clipping.y, original_clipping.w, original_clipping.h);
	}
}



void gui_textinput_t::setze_text(char *text, int max)
{
	this->text = text;
	this->max = max;
	cursor_pos = strlen(text);
}
