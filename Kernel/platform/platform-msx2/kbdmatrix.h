#ifndef __KBDMATRIX_DOT_H__
#define __KBDMATRIX_DOT_H__
/*
 * International key matrix
 */
const uint8_t keyboard_int[11][8] = {
	{'0','1','2', '3','4','5','6','7'},
	{'8','9','-','=','\\','[',']',';'},
	{ 39, '`', ',', '.','/',' ','a','b'},
	{'c','d','e', 'f','g','h','i','j'},
	{'k','l','m', 'n','o','p','q','r'},
	{'s','t','u', 'v','w','x','y','z'},
	{0/*SHIFT*/,0/*CTRL*/,0/*GRPH*/,0/*CAPS*/,0/*CODE*/ , KEY_F1 , KEY_F2 , KEY_F3 },
	{KEY_F4 , KEY_F5,  KEY_ESC , '\t', KEY_STOP ,KEY_BS , 0 , 13},
	{32 , KEY_HOME,  KEY_INSERT , KEY_DEL, KEY_LEFT , KEY_UP , KEY_DOWN , KEY_RIGHT},
	{'*','+','/','0','1' ,'2','3','4'},
	{'5','6','7','8','9' ,'-',',','.'}
};

const uint8_t shiftkeyboard_int[11][8] = {
	{')','!','@', '#','$','%','^','&'},
	{'*','(','_','+','|','{','}',':'},
	{'"','~','<','>','?',' ','A','B'},
	{'C','D','E', 'F','G','H','I','J'},
	{'K','L','M', 'N','O','P','Q','R'},
	{'S','T','U', 'V','W','X','Y','Z'},
	{0/*SHIFT*/,0/*CTRL*/,0/*GRPH*/,0/*CAPS*/,0/*CODE*/, KEY_F1 , KEY_F2 , KEY_F3 },
	{KEY_F4 , KEY_F5,  KEY_ESC , '\t', KEY_STOP ,KEY_BS , 0/*SELECT*/ , 13},
	{32 ,KEY_HOME,  KEY_INSERT , KEY_DEL, KEY_LEFT , KEY_UP , KEY_DOWN , KEY_RIGHT},
	{'*','+','/','0','1' ,'2','3','4'},
	{'5','6','7','8','9' ,'-',',','.'}
};

/*
 * Japan overlay
 */
const uint8_t keyboard_jp[3][8] = {
	{'0','1','2', '3','4','5','6','7'},
	{'8','9','-','^',KEY_YEN,'@','[',';'},
	{':',']', ',', '.','/',' ','a','b'}};

const uint8_t shiftkeyboard_jp[3][8] = {
	{' ','!','"', '#','$','%','&',39},
	{'(',')','=','~','|','`','{','+'},
	{'*','}','<','>','?','_','A','B'}};
/*
 * UK overlay
 */
const uint8_t shiftkeyboard_uk[1][8] = {
	{39,'`', ',', '.','/',KEY_POUND,'A','B'}};  /* row 2 */

/*
 * Spanish overlay
 */
const uint8_t keyboard_es[2][8] = {
	{'8','9','-','=','\\','[',']', 'N'/* Ñ */}, /* row 1 */
	{39, ':', ',', '.','/',' ','a','b'}};

const uint8_t shiftkeyboard_es[2][8] = {
	{'*','(','_','+','|','{','}', 'n' /* ñ */},  /* row 1 */
	{'"',':','<','>','?',' ','A','B'}};

/*
 * French AZERTY layout (Philips NMS 8245/19 and other French MSX2), rows 0-5
 * replacing the international ones, plus the CODE and SHIFT+CODE layers of
 * rows 0-2 where the French keyboard keeps the ASCII symbols a shell needs.
 *
 * Taken straight from the key translation tables of the NMS 8245/19 BIOS
 * (normal, shift, graph, shift+graph, code, shift+code: 48 bytes each from
 * 0x0DA5, key number = row * 8 + bit), located by searching the ROM for the
 * letter positions, which do not move between locales.
 *
 * The BIOS emits MSX character set codes for the accented keys, which the
 * FUZIX console font (ASCII only) cannot draw. They are mapped the way the
 * French ISO-646 variant does it (and the French CPC ROM, see
 * Kernel/dev/cpc/cpckeyboard.c): a = @, e acute = {, e grave = }, c cedilla =
 * \, u grave = |, section = ], degree = [, pound = #. Every shell symbol is
 * then one keystroke away, as on a French CPC running FUZIX; CODE gives the
 * same symbols where the MSX keycaps show them. The dead key (circumflex)
 * types a plain ^.
 */
const uint8_t keyboard_fr[6][8] = {
	{'@'/*à*/, '&', '{'/*é*/, '"', 39, '(', ']'/*§*/, '}'/*è*/},
	{'!', '\\'/*ç*/, ')', '-', '<', '^'/*dead key*/, '$', 'm'},
	{'|'/*ù*/, '#', ';', ':', '=', 0, 'q', 'b'},
	{'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j'},
	{'k', 'l', ',', 'n', 'o', 'p', 'a', 'r'},
	{'s', 't', 'u', 'v', 'z', 'x', 'y', 'w'}
};

const uint8_t shiftkeyboard_fr[6][8] = {
	{'0', '1', '2', '3', '4', '5', '6', '7'},
	{'8', '9', '['/*°*/, '_', '>', '^'/*dead key*/, '*', 'M'},
	{'%', '#'/*£*/, '.', '/', '+', 0, 'Q', 'B'},
	{'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J'},
	{'K', 'L', '?', 'N', 'O', 'P', 'A', 'R'},
	{'S', 'T', 'U', 'V', 'Z', 'X', 'Y', 'W'}
};

const uint8_t codekeyboard_fr[3][8] = {
	{0, '|', '@', 0, '`', '{', '^', 0},
	{0, 0, '}', 0, 0, 0/*dead key*/, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0}
};

const uint8_t shiftcodekeyboard_fr[3][8] = {
	{0, 0, 0, 0, 0, '[', 0, '~'},
	{0, 0, ']', 0, 0, 0/*dead key*/, 0, 0},
	{0, 0, 0, '\\', 0, 0, 0, 0}
};


#endif
