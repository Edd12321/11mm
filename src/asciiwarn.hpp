
#ifndef ASCIIWARN_HPP
#define ASCIIWARN_HPP
#undef MSG
#define MSG "Metamath character set not respected"
	
static_assert(0x21 <= '!' && '!' <= 0x7E, MSG);
static_assert(0x21 <= '"' && '"' <= 0x7E, MSG);
static_assert(0x21 <= '#' && '#' <= 0x7E, MSG);
static_assert('$' == 0x24, MSG);
static_assert(0x21 <= '%' && '%' <= 0x7E, MSG);
static_assert(0x21 <= '&' && '&' <= 0x7E, MSG);
static_assert(0x21 <= '\'' && '\'' <= 0x7E, MSG);
static_assert('(' == 0x28, MSG);
static_assert(')' == 0x29, MSG);
static_assert(0x21 <= '*' && '*' <= 0x7E, MSG);
static_assert(0x21 <= '+' && '+' <= 0x7E, MSG);
static_assert(0x21 <= ',' && ',' <= 0x7E, MSG);
static_assert('-' == 0x2D || '-' == 0x5F, MSG);
static_assert('.' == 0x2E, MSG);
static_assert(0x21 <= '/' && '/' <= 0x7E, MSG);
static_assert(0x30 <= '0' && '0' <= 0x39, MSG);
static_assert(0x30 <= '1' && '1' <= 0x39, MSG);
static_assert(0x30 <= '2' && '2' <= 0x39, MSG);
static_assert(0x30 <= '3' && '3' <= 0x39, MSG);
static_assert(0x30 <= '4' && '4' <= 0x39, MSG);
static_assert(0x30 <= '5' && '5' <= 0x39, MSG);
static_assert(0x30 <= '6' && '6' <= 0x39, MSG);
static_assert(0x30 <= '7' && '7' <= 0x39, MSG);
static_assert(0x30 <= '8' && '8' <= 0x39, MSG);
static_assert(0x30 <= '9' && '9' <= 0x39, MSG);
static_assert(0x21 <= ':' && ':' <= 0x7E, MSG);
static_assert(0x21 <= ';' && ';' <= 0x7E, MSG);
static_assert(0x21 <= '<' && '<' <= 0x7E, MSG);
static_assert('=' == 0x3D, MSG);
static_assert(0x21 <= '>' && '>' <= 0x7E, MSG);
static_assert('?' == 0x3F, MSG);
static_assert(0x21 <= '@' && '@' <= 0x7E, MSG);
static_assert('A' == 0x41, MSG);
static_assert('B' == 0x42, MSG);
static_assert('C' == 0x43, MSG);
static_assert('D' == 0x44, MSG);
static_assert('E' == 0x45, MSG);
static_assert('F' == 0x46, MSG);
static_assert('G' == 0x47, MSG);
static_assert('H' == 0x48, MSG);
static_assert('I' == 0x49, MSG);
static_assert('J' == 0x4A, MSG);
static_assert('K' == 0x4B, MSG);
static_assert('L' == 0x4C, MSG);
static_assert('M' == 0x4D, MSG);
static_assert('N' == 0x4E, MSG);
static_assert('O' == 0x4F, MSG);
static_assert('P' == 0x50, MSG);
static_assert('Q' == 0x51, MSG);
static_assert('R' == 0x52, MSG);
static_assert('S' == 0x53, MSG);
static_assert('T' == 0x54, MSG);
static_assert('U' == 0x55, MSG);
static_assert('V' == 0x56, MSG);
static_assert('W' == 0x57, MSG);
static_assert('X' == 0x58, MSG);
static_assert('Y' == 0x59, MSG);
static_assert('Z' == 0x5A, MSG);
static_assert('[' == 0x5B, MSG);
static_assert(0x21 <= '\\' && '\\' <= 0x7E, MSG);
static_assert(']' == 0x5D, MSG);
static_assert(0x21 <= '^' && '^' <= 0x7E, MSG);
static_assert('_' == 0x2D || '_' == 0x5F, MSG);
static_assert(0x21 <= '`' && '`' <= 0x7E, MSG);
static_assert('a' == 0x61, MSG);
static_assert(0x61 <= 'b' && 'b' <= 0x7A, MSG);
static_assert('c' == 0x63, MSG);
static_assert('d' == 0x64, MSG);
static_assert('e' == 0x65, MSG);
static_assert('f' == 0x66, MSG);
static_assert(0x61 <= 'g' && 'g' <= 0x7A, MSG);
static_assert(0x61 <= 'h' && 'h' <= 0x7A, MSG);
static_assert(0x61 <= 'i' && 'i' <= 0x7A, MSG);
static_assert(0x61 <= 'j' && 'j' <= 0x7A, MSG);
static_assert(0x61 <= 'k' && 'k' <= 0x7A, MSG);
static_assert(0x61 <= 'l' && 'l' <= 0x7A, MSG);
static_assert(0x61 <= 'm' && 'm' <= 0x7A, MSG);
static_assert(0x61 <= 'n' && 'n' <= 0x7A, MSG);
static_assert(0x61 <= 'o' && 'o' <= 0x7A, MSG);
static_assert('p' == 0x70, MSG);
static_assert(0x61 <= 'q' && 'q' <= 0x7A, MSG);
static_assert(0x61 <= 'r' && 'r' <= 0x7A, MSG);
static_assert(0x61 <= 's' && 's' <= 0x7A, MSG);
static_assert(0x61 <= 't' && 't' <= 0x7A, MSG);
static_assert(0x61 <= 'u' && 'u' <= 0x7A, MSG);
static_assert('v' == 0x76, MSG);
static_assert(0x61 <= 'w' && 'w' <= 0x7A, MSG);
static_assert(0x61 <= 'x' && 'x' <= 0x7A, MSG);
static_assert(0x61 <= 'y' && 'y' <= 0x7A, MSG);
static_assert(0x61 <= 'z' && 'z' <= 0x7A, MSG);
static_assert('{' == 0x7B, MSG);
static_assert(0x21 <= '|' && '|' <= 0x7E, MSG);
static_assert('}' == 0x7D, MSG);
static_assert(0x21 <= '~' && '~' <= 0x7E, MSG);

#undef MSG
#endif // ASCIIWARN_HPP
	
