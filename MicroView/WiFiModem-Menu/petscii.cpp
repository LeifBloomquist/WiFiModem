/*
Commodore 64 - MicroView - Wi-Fi Cart
Copyright 2015-2016 Leif Bloomquist and Alex Burger

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

// Adapted by Leif Bloomquist from:

/*  petcom version 1.00 by Craig Bruce, 18-May-1995
**
**  Convert from PETSCII to ASCII, or vice-versa.
*/

#include "petscii.h"

String petscii::ToPETSCII(const char *source)
{
    String temp = "";
    temp.reserve(strlen(source));

    for (int i = 0; source[i] != 0; i++)
    {
        temp += charset_p_topetcii(source[i]);
        
    }
    return temp;
}

/****************************************************************************/
String petscii::ToASCII(const char *source)
{
    String temp = "";
    temp.reserve(strlen(source));

    for (int i = 0; source[i] != 0; i++)
    {
        temp += charset_p_toascii(source[i]);
    }
    return temp;
}



/*  The functions below are taken from OpenCBM's petscii.c file.
 *  Copyright 1999-2005 Michael Klein <michael(dot)klein(at)puffin(dot)lb(dot)shuttle(dot)de>
 *  Copyright 2001-2005 Spiro Trikaliotis
 *
 *  Parts are Copyright
 *      Jouko Valta <jopi(at)stekt(dot)oulu(dot)fi>
 *      Andreas Boose <boose(at)linux(dot)rz(dot)fh-hannover(dot)de>
*/

#define ASCII_UNMAPPED  '.'

// char is -127 to +127 while PETSCII comes in from 0 to 255 so we use unsigned char.
char charset_p_toascii(char c)
{
    unsigned char c2 = (unsigned char)c;
    
    c2 = petcii_fix_dupes2(c);

    /* map petscii to ascii */
    if (c2 == 0x0d) {  /* petscii "return" */
        return '\n';
    } else if (c2 == 0x0a) {
        return '\r';
    } else if (c2 <= 0x1f) {
        /* unhandled ctrl codes */
        return ASCII_UNMAPPED;
    } else if (c2 == 0xa0) { /* petscii Shifted Space */
        return ' ';
    } else if ((c2 >= 0xc1) && (c2 <= 0xda)) {
        /* uppercase (petscii 0xc1 -) */
        return (char)((c2 - 0xc1) + 'A');
    } else if ((c2 >= 0x41) && (c2 <= 0x5a)) {        //1st
        /* lowercase (petscii 0x41 -) */
        return (char)((c2 - 0x41) + 'a');
    }

    return ((isprint(c) ? c : ASCII_UNMAPPED));
}

#define ASCII_UNMAPPED_NULL 0

// Only shift upper A-Z c1-da
char charset_p_toascii_upper_only(char c)
{
    unsigned char c2 = (unsigned char)c;

    if (c2 == 0x0d) {  /* petscii "return" */
        return '\n';
    } else if (c2 == 0x0a) {
        return '\r';
    } else if (c2 <= 0x1f) {
        /* unhandled ctrl codes */
        return ASCII_UNMAPPED_NULL;
    } else if (c2 == 0xa0) { /* petscii Shifted Space */
        return ' ';
    } else if ((c2 >= 0xc1) && (c2 <= 0xda)) {
        /* uppercase (petscii 0xc1 -) */
        return (char)((c2 - 0xc1) + 'A');
    }
    
    return (c);
}

#define PETSCII_UNMAPPED 0x3f     /* petscii "?" */

char charset_p_topetcii(char c)
{
    /* map ascii to petscii */
    if (c == '\n') {
        return 0x0d; /* petscii "return" */
    } else if (c == '\r') {
        return 0x0a;
    } else if (c <= 0x1f) {
        /* unhandled ctrl codes */
        return PETSCII_UNMAPPED;
    } else if (c == '`') {
        return 0x27; /* petscii "'" */
    } else if ((c >= 'a') && (c <= 'z')) {
        /* lowercase (petscii 0x41 -) */
        return (char)((c - 'a') + 0x41);
    } else if ((c >= 'A') && (c <= 'Z')) {
        /* uppercase (petscii 0xc1 -)
           (don't use duplicate codes 0x61 - ) */
        return (char)((c - 'A') + 0xc1);
    } else if (c >= 0x7b) {
        /* last not least, ascii codes >= 0x7b can not be
           represented properly in petscii */
        return PETSCII_UNMAPPED;
    }

    return petcii_fix_dupes(c);
}

// Shifts 0x61 to 0xc1 (uppercase)
// Shifts 0xe0 to 0xa0 (symbols)
static char petcii_fix_dupes(char c)
{
    if ((c >= 0x60) && (c <= 0x7f)) {
        return ((c - 0x60) + 0xc0);
    } else if (c >= 0xe0) {
        return ((c - 0xe0) + 0xa0);
    }
    return c;
}

// Shifts 0x61 to 0xc1 (uppercase)
// Shifts 0xe0 to 0xa0 (symbols)
// char is -127 to +127 while PETSCII comes in from 0 to 255 so we use unsigned char.
static unsigned char petcii_fix_dupes2(unsigned char c)
{
    if ((c >= 0x60) && (c <= 0x7f)) {
        return ((c - 0x60) + 0xc0);
    } else if (c >= 0xe0) {
        return ((c - 0xe0) + 0xa0);
    }
    return c;
}

