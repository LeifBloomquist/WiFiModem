// Adapted by Leif Bloomquist from:

/*  petcom version 1.00 by Craig Bruce, 18-May-1995
**
**  Convert from PETSCII to ASCII, or vice-versa.
*/

#include "petscii.h"

String petscii::ToPETSCII(const char *source)
{
    String temp = "";

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

char charset_p_toascii(char c)
{
    c = petcii_fix_dupes(c);

    /* map petscii to ascii */
    if (c == 0x0d) {  /* petscii "return" */
        return '\n';
    } else if (c == 0x0a) {
        return '\r';
    } else if (c <= 0x1f) {
        /* unhandled ctrl codes */
        return ASCII_UNMAPPED;
    } else if (c == 0xa0) { /* petscii Shifted Space */
        return ' ';
    } else if ((c >= 0xc1) && (c <= 0xda)) {
        /* uppercase (petscii 0xc1 -) */
        return (char)((c - 0xc1) + 'A');
    } else if ((c >= 0x41) && (c <= 0x5a)) {
        /* lowercase (petscii 0x41 -) */
        return (char)((c - 0x41) + 'a');
    }

    return ((isprint(c) ? c : ASCII_UNMAPPED));
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

static char petcii_fix_dupes(char c)
{
    if ((c >= 0x60) && (c <= 0x7f)) {
        return ((c - 0x60) + 0xc0);
    } else if (c >= 0xe0) {
        return ((c - 0xe0) + 0xa0);
    }
    return c;
}


