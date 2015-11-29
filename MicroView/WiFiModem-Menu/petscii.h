// petscii.h

#ifndef _PETSCII_h
#define _PETSCII_h

#include "Arduino.h"

char charset_p_topetcii(char c);
char charset_p_toascii(char c);
static char petcii_fix_dupes(char c);
static unsigned char petcii_fix_dupes2(unsigned char c);

class petscii
{
private:
    petscii();   // Private constructor to prevent instantiation

public:
    static String ToPETSCII(const char *source);
    static String ToASCII(const char * source);

    // CG codes from Six
    // c64 c / g
    static const char CG_BLK = 144;
    static const char CG_WHT = 5;
    static const char CG_RED = 28;
    static const char CG_CYN = 159;
    static const char CG_PUR = 156;
    static const char CG_GRN = 30;
    static const char CG_BLU = 31;
    static const char CG_YEL = 158;
    static const char CG_BRN = 149;
    static const char CG_ORG = 129;
    static const char CG_PNK = 150;
    static const char CG_GR1 = 151;
    static const char CG_GR2 = 152;
    static const char CG_LGN = 153;
    static const char CG_LBL = 154;
    static const char CG_GR3 = 155;
    static const char CG_RVS = 18;   // revs-on
    static const char CG_NRM = 146;  // revs-off
  
    static const char CG_DCS = 8;    // disable shift + C=
    static const char CG_ECS = 9;    // enable shift + C=

    static const char CG_LCS = 14;   // switch to lowercase
    static const char CG_UCS = 142;  // switch to uppercase

    static const char CG_CLR = 147;  // clear screen

    //cursor movement
    static const char CS_HOM = 19;
    static const char CS_U = 145;
    static const char CS_D = 17;
    static const char CS_L = 157;
    static const char CS_R = 29;

    static const char CRLF = 13;
};

#endif


