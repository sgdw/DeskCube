// Copyright (C) 2017  Martin Feil aka. SGDW
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/


#include "LedControl.h" // see http://wayoda.github.io/LedControl/
#include "font8x8_basic_low_mem.h"
#include "icons_preset.h"
#include <avr/pgmspace.h>
#define FS(x) (__FlashStringHelper*)(x)

#define VERBOSE 1
#define VERY_VERBOSE 0

/*
   Now we need a LedControl to work with.
 ***** These pin numbers will probably not work with your hardware *****
   pin 12 is connected to the DataIn
   pin 11 is connected to the CLK
   pin 10 is connected to LOAD
   We have only a single MAX72XX.
 */
LedControl lc=LedControl(12,11,10,1);
#define DISPLAY_ROWS 8
#define DISPLAY_COLS 8

#define DISPLAY_CHAR_OFFS_X 0
#define DISPLAY_CHAR_OFFS_Y 1

#define MAX_ICONS 10

unsigned long delaytime = 100;
unsigned long delaytime_short = 10;
unsigned long delaytimeScroll = 50;
unsigned long delayBlink = 1000;
unsigned long delayIcon = 1000;

#define CMD_IDLE 0
#define CMD_SCROLL_TEXT 's'
#define CMD_LOAD 'l'
#define CMD_CHAR_LOAD 'h'
#define CMD_ICON 'i'
#define CMD_FRAME 'f'
#define CMD_FRAME_EXPL 'F'
#define CMD_CLEAR 'c'
#define CMD_BLINK 'b'
#define CMD_CHAR 'C'

#define DISP_IDLE 0
#define DISP_UPDATE 1
#define DISP_BLINK 2

const char MSG_OK[]  PROGMEM = "[OK]";
const char MSG_INFO[]  PROGMEM = "[INFO] ";
const char MSG_ERR_UNKNOWN_COMMAND[]  PROGMEM = "[ERR#1] unknown command";
const char MSG_ERR_TOO_FEW_PARS[]     PROGMEM = "[ERR#2] too few parameters";
const char MSG_ERR_UNKNOWN_ACTION[]   PROGMEM = "[ERR#3] unknown action";
// const char MSG_????[]  PROGMEM = "[ERR]";

String text = "";

char frame[DISPLAY_ROWS];

int currentIcon = -1;
int activeIcons[MAX_ICONS];
int endOfActiveIcons = 0;

char icons[DISPLAY_ROWS*MAX_ICONS];
char currentCommand = CMD_IDLE;
char buffer[102];
int  bufferPos = 0;

int displayStatus = DISP_IDLE;

void setup() {
	Serial.begin(9600);

	text.reserve(100);

	lc.shutdown(0, false); // The MAX72XX is in power-saving mode on startup, we have to do a wakeup call
	lc.setIntensity(0, 0);
	lc.clearDisplay(0);
}

void displayIcon(char data[], int offset) {
	char value, val;
	int col;
	lc.clearDisplay(0);
	for(int i=0; i < DISPLAY_COLS; i++) {
		value = data[offset+i];
		col = 7-i;
		for(int row=0; row < DISPLAY_ROWS; row++)
		{
			val = value >> (7-row);
			val = val & 0x01;
			lc.setLed(0, row, col, val);
		}
	}
}

void displayChar(char c, int offsetx, int offsety) {
	char value, val;
	int col;
	for(int i=0; i < DISPLAY_COLS; i++) {
		if(c < font8x8_basic_offset) {
			value = ' ';
		}
		else
		{
			// value = font8x8_basic[c - font8x8_basic_offset][i];
			value = pgm_read_byte(&font8x8_basic[c - font8x8_basic_offset][i]);
		}
		col = 7-i;
		for(int row=0; row < DISPLAY_ROWS; row++)
		{
			if(
			    (row + offsetx) >= 0 &&
			    (row + offsetx) <  DISPLAY_ROWS &&
			    (col - offsety) >= 0 &&
			    (col - offsety) <  DISPLAY_COLS
			    )
			{
				val = value >> (row);
				val = val & 0x01;
				lc.setLed(0, row + offsetx, col - offsety, val);
			}
		}
	}
}

void charToIcon(char c, int icon, int offsetx, int offsety) {
	char value, val;
	int col;
	for(int i=0; i < DISPLAY_COLS; i++) {
		if(c < font8x8_basic_offset) {
			value = ' ';
		}
		else
		{
			// value = font8x8_basic[c - font8x8_basic_offset][i];
			value = pgm_read_byte(&font8x8_basic[c - font8x8_basic_offset][i]);
		}
		icons[icon * DISPLAY_ROWS + i] = (reverse(value) >> DISPLAY_CHAR_OFFS_Y) & B01111111;
	}
}

char reverse(char b) {
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}

char getByteFromFrameString(char buf[], int offset) {
	int value = 0;
	for(int b = 0; b < 8; b++) {
		if(buf[offset + b] != ' ') {
			value = value | (1 << b);
			Serial.print("X");
		} else {
			Serial.print(".");
		}
	}
	Serial.println("");
	Serial.println(value);
	return value;
}

// Scrolling =================================================================

void scrollText(char t[], int l, int delayMs) {
	for(int i = 0; i < l; i++) {
		char c0 = t[i];
		char c1 = t[(i+1 >= l ? 0 : i+1)];
		scrollChars(c0, c1, delayMs);
	}
}

void scrollString(String t, int delayMs) {
	int l = t.length();
	for(int i = 0; i < l; i++) {
		char c0 = t.charAt(i);
		char c1 = t.charAt(i+1 >= l ? 0 : i+1);
		scrollChars(c0, c1, delayMs);
		if(!validCommand(CMD_SCROLL_TEXT)) return;
		tickProtocoll();
	}
}

void scrollChars(char c0, char c1, int delayMs) {
	for(int s = 0; s < 8; s++) {
		displayChar(c0, 7-s-8, DISPLAY_CHAR_OFFS_Y);
		displayChar(c1, 7-s,   DISPLAY_CHAR_OFFS_Y);
		delay(delayMs);
	}
}

// Icons =====================================================================

void addIcon(char icon) {
	if(endOfActiveIcons < MAX_ICONS) {
		if(posOfIcon(icon) >= 0) return;
		activeIcons[endOfActiveIcons] = icon;
		endOfActiveIcons++;
	}
}

void removeIcon(char icon) {
	if(endOfActiveIcons > 0) {
		int pos = posOfIcon(icon);
		if(pos == -1) return;
		for(int i = pos+1; i < MAX_ICONS && i < endOfActiveIcons; i++) {
			activeIcons[i-1] = activeIcons[i];
		}
		currentIcon = 0; // ToDo shit pointer
		endOfActiveIcons--;
	}
}

void clearIcons() {
	endOfActiveIcons = 0;
}

int posOfIcon(char icon) {
	for(int i = 0; i < MAX_ICONS && i < endOfActiveIcons; i++) {
		if(activeIcons[i] == icon) return i;
	}
	return -1;
}

int getIconSlotFromValue(char icon) {
	return (icon >= MAX_ICONS ? icon - MAX_ICONS : icon);
}

int getIconValue(char iconChar) {
	return (iconChar >= 'a' ? MAX_ICONS + iconChar - 'a' : iconChar - '0');
}

bool isPresetIconValue(char icon) {
	return (icon >= MAX_ICONS);
}

void printActiveIcons() {
	Serial.print(F("[INFO] Icons current ")); Serial.print(currentIcon);
	Serial.print(F(" endOf ")); Serial.print(endOfActiveIcons);
	Serial.println(":");
	for(int i = 0; i < MAX_ICONS && i < endOfActiveIcons; i++) {
		if(i == currentIcon) Serial.print(F("[INFO] + "));
		Serial.print(i); Serial.print(F(": ")); Serial.println(activeIcons[i]);
	}
}

// Protocoll =================================================================

void tickProtocoll() {
	bool complete = 0;
	while(Serial.available()) {
		char inChar = (char)Serial.read();
		buffer[bufferPos++] = inChar;
		if(buffer[0] == CMD_LOAD) {
			complete = (bufferPos > 10) && (inChar == '\n');
		}
		else if(buffer[0] == CMD_FRAME) {
			complete = (bufferPos > 9) && (inChar == '\n');
		} else {
			complete = (inChar == '\n');
		}
		if (complete) {
			if(bufferPos > 0 && buffer[bufferPos-1] == '\n') bufferPos -= 1; // remove trailing CR
			if(bufferPos > 0 && buffer[bufferPos-1] == '\r') bufferPos -= 1; // remove trailing CR

			if(VERY_VERBOSE) {
				Serial.print("BufferPos: "); Serial.println(bufferPos);
				for(int i = 0; i < bufferPos; i++) {
					Serial.print("B-"); Serial.print(i); Serial.print(": "); Serial.print((int)buffer[i]); Serial.print(" / "); Serial.println(buffer[i]);
				}
			}

			parseProtocoll();
			return;
		}
	}
}

void parseProtocoll() {
	if(buffer[0] == CMD_SCROLL_TEXT) {
		text = "";
		for(int i = 1; i < bufferPos; i++) {
			if (buffer[i] != '\r' && buffer[i] != '\n') {
				text += buffer[i];
			}
		}
		currentCommand = buffer[0];
		Serial.println(FS(MSG_OK));
	}
	else if(buffer[0] == CMD_LOAD) {
		if(bufferPos > 2) {
			int slot = buffer[1] - '0';
			for(int i = 0; i < DISPLAY_ROWS && i < (bufferPos - 2); i++) {
				icons[slot * DISPLAY_ROWS + i] = buffer[2+i];
			}
			Serial.println(FS(MSG_OK));
		} else {
			Serial.println(FS(MSG_ERR_TOO_FEW_PARS));
		}
		// currentCommand unchanged
	}
	else if(buffer[0] == CMD_CHAR_LOAD) {
		if(bufferPos > 2) {
			int slot = buffer[1] - '0';
			charToIcon(buffer[2], slot, 0, 0);
			Serial.println(FS(MSG_OK));
		} else {
			Serial.println(FS(MSG_ERR_TOO_FEW_PARS));
		}
		// currentCommand unchanged
	}
	else if(buffer[0] == CMD_ICON) {
		if(bufferPos >= 2) {
			char action = buffer[1];
			int icon;

			if(action == '+') {
				for(int i = 2; i < bufferPos; i++) {
					icon = getIconValue(buffer[i]);
					addIcon(icon);
					if(VERBOSE) {
						Serial.print(F("[INFO] Add Icon: ")); Serial.println(icon);
					}
				}
				Serial.println(FS(MSG_OK));
			} else if(action == '-') {
				if(bufferPos > 2 && buffer[2] == '*') {
					clearIcons();
				} else {
					for(int i = 2; i < bufferPos; i++) {
						icon = getIconValue(buffer[i]);
						removeIcon(icon);
						if(VERBOSE) {
							Serial.print(F("[INFO] Remove Icon: ")); Serial.println(icon);
						}
					}
				}
				Serial.println(FS(MSG_OK));
			} else {
				Serial.println(FS(MSG_ERR_UNKNOWN_ACTION));
			}

			if(VERY_VERBOSE) printActiveIcons();
			currentIcon = (endOfActiveIcons > 0 ? 0 : -1);
			currentCommand = buffer[0];
			displayStatus = DISP_UPDATE;
		}
	}
	else if(buffer[0] == CMD_BLINK) {
		if(bufferPos > 1 && buffer[1] == '+') {
			displayStatus = DISP_BLINK;
		}
		else if(bufferPos > 1 && buffer[1] == '-') {
				displayStatus = DISP_UPDATE;
		} else {
			if(displayStatus == DISP_BLINK) {
				displayStatus = DISP_UPDATE;
			} else {
				displayStatus = DISP_BLINK;
			}
		}
		Serial.println(FS(MSG_OK));
	}
	else if(buffer[0] == CMD_FRAME || buffer[0] == CMD_FRAME_EXPL) {
		if(
			(buffer[0] == CMD_FRAME_EXPL && (bufferPos >= 1+8*8)) ||
			(buffer[0] == CMD_FRAME && (bufferPos >= 1+8))
		) {
			for(int i = 0; i < DISPLAY_ROWS && i < (bufferPos - 1); i++) {
				if(buffer[0] == CMD_FRAME_EXPL) {
					frame[i] = reverse(getByteFromFrameString(buffer, 1 + i * DISPLAY_COLS));
				} else {
					frame[i] = buffer[1+i];
				}
			}
			currentCommand = CMD_FRAME; // always fall back to CMD_FRAME
			displayStatus = DISP_UPDATE;
			Serial.println(FS(MSG_OK));
		} else {
			Serial.println(FS(MSG_ERR_TOO_FEW_PARS));
		}
	}
	else if(buffer[0] == CMD_CLEAR) {
		currentCommand = buffer[0];
		displayStatus = DISP_UPDATE;
		Serial.println(FS(MSG_OK));
	}
	else if(buffer[0] == CMD_CHAR) {
		displayChar(buffer[1], DISPLAY_CHAR_OFFS_X, DISPLAY_CHAR_OFFS_Y);
		currentCommand = CMD_IDLE;
		Serial.println(FS(MSG_OK));
	} else {

	}
	resetProtokoll();
}

void resetProtokoll() {
	bufferPos = 0;
}

void execProtocoll() {
	if(currentCommand == CMD_SCROLL_TEXT) {
		Serial.print(F("Scrolling "));
		Serial.println(text);
		scrollString(text, delaytimeScroll);
	}
	else if(currentCommand == CMD_ICON && (displayStatus == DISP_UPDATE || displayStatus == DISP_BLINK)) {
		if(currentIcon >= 0) {

			int slot = getIconSlotFromValue(activeIcons[currentIcon]);
			bool doRun = 1;
			bool presetIcon = 0;

			if(VERY_VERBOSE) {
				Serial.print(F("[INFO] Active icon: "));  Serial.println(activeIcons[currentIcon]);
			}

			presetIcon = isPresetIconValue(activeIcons[currentIcon]);

			if(VERY_VERBOSE) {
				Serial.print(presetIcon ? F("[INFO] Displaying preset icon ") : F("[INFO] Displaying icon "));
				Serial.print(currentIcon); Serial.print(" V:"); Serial.print(activeIcons[currentIcon]);
				Serial.print(" in slot "); Serial.println(slot);
			}

			while(1) {

				if(presetIcon) {
					displayIcon(icons_preset, slot * DISPLAY_ROWS);
					if(VERY_VERBOSE) { Serial.print(F("[INFO] Display preset icon ")); Serial.println(slot); }
				} else {
					displayIcon(icons, slot * DISPLAY_ROWS);
					if(VERY_VERBOSE) { Serial.print(F("[INFO] Display icon ")); Serial.println(slot); }
				}
				if(VERY_VERBOSE) { Serial.print(F("[INFO] EndOfActiveIcons ")); Serial.println(endOfActiveIcons); }

				if(endOfActiveIcons > 1) {
					currentIcon++;
					if(VERY_VERBOSE) { Serial.print(F("[INFO] CurrentIcon ")); Serial.println(currentIcon); }
					if(currentIcon >= endOfActiveIcons) currentIcon = 0;
					if(VERY_VERBOSE) { Serial.print(F("[INFO] CurrentIcon ")); Serial.println(currentIcon); }
					slot = getIconSlotFromValue(activeIcons[currentIcon]);
					presetIcon = isPresetIconValue(activeIcons[currentIcon]);
				}

				if(displayStatus == DISP_BLINK) {
					tickProtocoll();
					if(!validCommand(CMD_ICON)) {
						break;
					}
					delay(delayBlink);
					lc.clearDisplay(0);
					tickProtocoll();
					delay(delayBlink);
				} else {
					if(endOfActiveIcons > 1) {
						tickProtocoll();
						if(!validCommand(CMD_ICON)) {
							break;
						}
						delay(delayIcon);
					} else {
						displayStatus = DISP_IDLE;
						break;
					}
				}

			}
			if(endOfActiveIcons == 0) lc.clearDisplay(0);
		} else {
			lc.clearDisplay(0);
			displayStatus = DISP_IDLE;
		}
	}
	else if(currentCommand == CMD_FRAME && displayStatus == DISP_UPDATE) {
		displayIcon(frame, 0);
		displayStatus = DISP_IDLE;
	}
	else if(currentCommand == CMD_CLEAR && displayStatus == DISP_UPDATE) {
		lc.clearDisplay(0);
		displayStatus = DISP_IDLE;
		currentCommand = CMD_IDLE;
	}
	else
	{
		if(!Serial.available()) delay(5);
	}
}

bool validCommand(char command) {
	return command == currentCommand;
}

void loop() {
	tickProtocoll();
	execProtocoll();
}
