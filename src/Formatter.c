/*
FreeDOS-32 kernel
Copyright (C) 2008-2018  Salvatore ISAJA

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
#include "kernel.h"

enum Formatter_Flags {
    leftJustify = 0 << 1,
    forceSign = 1 << 1,
    spaceSign = 1 << 2,
    basePrefix = 1 << 3,
    zeroPad = 1 << 4,
    thousandSeparator = 1 << 5,
    hasWidth = 1 << 6,
    hasPrecision = 1 << 7,
    interpretAsUnsigned = 1 << 8
};

typedef enum Formatter_Base {
    nonnumber,
    base8,
    base10,
    base16lower,
    base16upper
} Formatter_Base;

typedef enum Formatter_Length {
    normalLength,
    shortLength,
    charLength,
    longLength,
    longlongLength
} Formatter_Length;

typedef struct Formatter_Specifier {
    int flags;
    Formatter_Length length;
    Formatter_Base base;
    size_t width;
    size_t precision;
} Formatter_Specifier;

static const char Formatter_uppercaseDigits[] = "0123456789ABCDEF";
static const char Formatter_lowercaseDigits[] = "0123456789abcdef";

static int Formatter_outputNumber(Appendable *appendable, const char *num, size_t numSize, const char *prefix, size_t prefixSize, const Formatter_Specifier *specifier) {
    size_t size = numSize + prefixSize;
    if (specifier->flags & hasWidth) {
        if (!(specifier->flags & (zeroPad | leftJustify))) {
            while (size < specifier->width) {
                size += appendable->vtbl->appendChar(appendable->obj, ' ');
            }
        }
        if (prefixSize != 0) {
            appendable->vtbl->appendCharArray(appendable->obj, prefix, prefixSize);
        }
        if (specifier->flags & zeroPad) {
            assert(!(specifier->flags & leftJustify));
            while (size < specifier->width) {
                size += appendable->vtbl->appendChar(appendable->obj, '0');
            }
        }
        appendable->vtbl->appendCharArray(appendable->obj, num, numSize);
        if (specifier->flags & leftJustify) {
            assert(!(specifier->flags & zeroPad));
            while (size < specifier->width) {
                size += appendable->vtbl->appendChar(appendable->obj, ' ');
            }
        }
    } else {
        if (prefixSize != 0) {
            appendable->vtbl->appendCharArray(appendable->obj, prefix, prefixSize);
        }
        appendable->vtbl->appendCharArray(appendable->obj, num, numSize);
    }
    return size;
}

static int Formatter_formatUnsignedLong(Appendable *appendable, unsigned long i, const Formatter_Specifier *specifier, char sign) {
    char buf[11]; // Length of UINT32_MAX as octal
    char prefix[3];
    char* b = buf + sizeof(buf);
    char* p = prefix;
    if (sign != '\0') *p++ = sign;
    switch (specifier->base) {
        case base8:
            do { *--b = Formatter_uppercaseDigits[i & 7]; } while ((i >>= 3) != 0);
            if (specifier->flags & basePrefix) { *p++ = '0'; }
            break;
        case base10:
            do { *--b = Formatter_uppercaseDigits[i % 10]; } while ((i /= 10) != 0);
            break;
        case base16lower:
            do { *--b = Formatter_lowercaseDigits[i & 15]; } while ((i >>= 4) != 0);
            if (specifier->flags & basePrefix) { *p++ = '0'; *p++ = 'x'; }
            break;
        case base16upper:
            do { *--b = Formatter_uppercaseDigits[i & 15]; } while ((i >>= 4) != 0);
            if (specifier->flags & basePrefix) { *p++ = '0'; *p++ = 'X'; }
            break;
        default:
            assert(false);
    }
    // TODO: thousandSeparator
    return Formatter_outputNumber(appendable, b, sizeof(buf) - (b - buf), prefix, p - prefix, specifier);
}

static int Formatter_formatSignedLong(Appendable *appendable, signed long i, const Formatter_Specifier *specifier) {
    char sign = '\0';
    if (i < 0) {
        sign = '-';
        i = -i;
    } else {
        if (specifier->flags & forceSign) sign = '+';
        else if (specifier->flags & spaceSign) sign = ' ';
    }
    return Formatter_formatUnsignedLong(appendable, (unsigned long) i, specifier, sign);
}

static int Formatter_formatUnsignedLongLong(Appendable *appendable, unsigned long long i, const Formatter_Specifier *specifier, char sign) {
    char buf[24]; // Length of UINT64_MAX as octal
    char prefix[3];
    char* b = buf + sizeof(buf);
    char* p = prefix;
    if (sign != '\0') *p++ = sign;
    switch (specifier->base) {
        case base8:
            do { *--b = Formatter_uppercaseDigits[i & 7]; } while ((i >>= 3) != 0);
            if (specifier->flags & basePrefix) { *p++ = '0'; }
            break;
        case base10:
            //do { *--b = uppercaseDigits_[i % 10]; } while ((i /= 10) != 0);
            break;
        case base16lower:
            do { *--b = Formatter_lowercaseDigits[i & 15]; } while ((i >>= 4) != 0);
            if (specifier->flags & basePrefix) { *p++ = '0'; *p++ = 'x'; }
            break;
        case base16upper:
            do { *--b = Formatter_uppercaseDigits[i & 15]; } while ((i >>= 4) != 0);
            if (specifier->flags & basePrefix) { *p++ = '0'; *p++ = 'X'; }
            break;
        default:
            assert(false);
    }
    // TODO: thousandSeparator
    return Formatter_outputNumber(appendable, b, sizeof(buf) - (b - buf), prefix, p - prefix, specifier);
}

static int Formatter_formatSignedLongLong(Appendable *appendable, signed long long i, const Formatter_Specifier *specifier) {
    char sign = '\0';
    if (i < 0) {
        sign = '-';
        i = -i;
    } else {
        if (specifier->flags & forceSign) sign = '+';
        else if (specifier->flags & spaceSign) sign = ' ';
    }
    return Formatter_formatUnsignedLongLong(appendable, (unsigned long long) i, specifier, sign);
}

/** This is the same as Formatter_printf(const char* fmt, ...) but taking a va_list argument. */
int Formatter_vprintf(Appendable *appendable, const char *fmt, va_list args) {
    size_t numWritten = 0;
    for (const char *s = fmt; *s != 0; ) {
        /*
         * Read format tag as following:
         *     %[flags][width][precision][length]specifier
         * where
         *     flags ::= [-+ #0]
         *     width ::= \*|[0-9]*
         *     precision ::= \.(\*|[0-9]*)
         *     length ::= h|hh|l|ll
         */
        if (*s != '%') {
            numWritten += appendable->vtbl->appendChar(appendable->obj, *s++);
        } else { // if (*s == '%')
            ++s;
            Formatter_Specifier specifier = { .flags = 0, .length = normalLength, .base = nonnumber, .width = 0, .precision = 0 };
            // Read flags
            while (true) {
                char c = *s;
                if (c == '-') { specifier.flags |= leftJustify; specifier.flags &= ~zeroPad; }
                else if (c == '+') { specifier.flags |= forceSign; specifier.flags &= ~spaceSign; }
                else if (c == ' ') specifier.flags |= spaceSign;
                else if (c == '#') specifier.flags |= basePrefix;
                else if (c == '0') specifier.flags |= zeroPad;
                else break;
                ++s;
            }
            // Read width
            if (*s == '*') {
                ++s;
                specifier.width = va_arg(args, unsigned);
                specifier.flags |= hasWidth;
            } else {
                for (; *s >= '0' && *s <= '9'; ++s) {
                    specifier.width = specifier.width * 10 + (*s - '0');
                }
                specifier.flags |= hasWidth;
            }
            // Read precision
            if (*s == '.') {
                ++s;
                if (*s == '*') {
                    ++s;
                    specifier.precision = va_arg(args, unsigned);
                    specifier.flags |= hasPrecision;
                } else {
                    for (; *s >= '0' && *s <= '9'; ++s) {
                        specifier.precision = specifier.precision * 10 + (*s - '0');
                    }
                    specifier.flags |= hasPrecision;
                }
            }
            // Read length
            if (*s == 'h') {
                ++s;
                if (*s == 'h') {
                    specifier.length = charLength;
                    ++s;
                } else {
                    specifier.length = shortLength;
                }
            } else if (*s == 'l') {
                ++s;
                if (*s == 'l') {
                    specifier.length = longlongLength;
                    ++s;
                } else {
                    specifier.length = longLength;
                }
            }
            // Read specifier
            switch (*s++) {
                case 'c':
                    numWritten += appendable->vtbl->appendChar(appendable->obj, (char) va_arg(args, int));
                    break;
                case 'd':
                case 'i':
                    specifier.base = base10;
                    break;
                case 'o':
                    specifier.base = base8;
                    specifier.flags |= interpretAsUnsigned;
                    break;
                case 'u':
                    specifier.base = base10;
                    specifier.flags |= interpretAsUnsigned;
                    break;
                case 'x':
                    specifier.base = base16lower;
                    specifier.flags |= interpretAsUnsigned;
                    break;
                case 'X':
                    specifier.base = base16upper;
                    specifier.flags |= interpretAsUnsigned;
                    break;
                case 'p': {
                    specifier.base = base16upper;
                    specifier.flags |= zeroPad | hasWidth;
                    specifier.width = sizeof(void*) * 2;
                    numWritten += appendable->vtbl->appendCharArray(appendable->obj, "0x", 2);
                    numWritten += Formatter_formatUnsignedLong(appendable, (unsigned long) va_arg(args, void *), &specifier, '\0');
                    specifier.base = nonnumber;
                    break;
                }
                case 's': {
                    const char *cp = va_arg(args, const char*);
                    if (!(specifier.flags & hasPrecision)) {
                        numWritten += appendable->vtbl->appendCStr(appendable->obj, cp);
                    } else {
                        numWritten += appendable->vtbl->appendCharArray(appendable->obj, cp, specifier.precision);
                    }
                    break;
                }
                default:
                    numWritten += appendable->vtbl->appendChar(appendable->obj, *s); // includes '%'
                    break;
            }
            if (specifier.base != nonnumber) {
                int res;
                if (specifier.flags & interpretAsUnsigned) {
                    if (specifier.length == longLength) res = Formatter_formatUnsignedLong(appendable, va_arg(args, unsigned long), &specifier, '\0');
                    else if (specifier.length == longlongLength) res = Formatter_formatUnsignedLongLong(appendable, va_arg(args, unsigned long long), &specifier, '\0');
                    else res = Formatter_formatUnsignedLong(appendable, (unsigned long) va_arg(args, unsigned int), &specifier, '\0');
                } else {
                    if (specifier.length == longLength) res = Formatter_formatSignedLong(appendable, va_arg(args, signed long), &specifier);
                    else if (specifier.length == longlongLength) res = Formatter_formatSignedLongLong(appendable, va_arg(args, signed long long), &specifier);
                    else res = Formatter_formatSignedLong(appendable, (signed long) va_arg(args, signed int), &specifier);
                }
                if (res < 0) return res;
                numWritten += res;
            }
        } // if (*s == '%')
    } // for (const char *s = format; *s != 0; )
    return 0;
}

/**
 * Processes the specified format string and writes the result to the associated Appendable.
 *
 * Formatting tags must have the following format:
 * <code>%[flags][width][precision][length]specifier</code>
 *
 * The optional <i>flags</i> can be a combination of the following:
 * <ul>
 * <li><b>-</b> left justify
 * <li><b>+</b> force sign even if positive number
 * <li><b>space</b> space pad sign if positive number
 * <li><b>#</b> print numerical base prefix
 * <li><b>0</b> zero pad to \a width
 * </ul>
 *
 * The optional <i>width</i> can be either an <b>*</b> (asterisk) to indicate
 * the width must be fetched from the next argument (that must be promoted to
 * unsigned int), or a decimal number (digits from 0 to 9, possibly repeated,
 * are allowed).
 *
 * The optional <i>precision</i> must start with a <b>.</b> (dot), followed
 * either by an <b>*</b> (asterisk) to indicate the width must be fetched from
 * the next argument (that must be promoted to unsigned int), or a decimal
 * number (digits from 0 to 9, possibly repeated, are allowed).
 *
 * The optional <i>length</i> can be one of the following:
 * <ul>
 * <li><b>h</b> to indicate a short argument
 * <li><b>hh</b> to indicate a char argument
 * <li><b>l</b> to indicate a long argument
 * <li><b>ll</b> to indicate a long long argument
 * </ul>
 *
 * The required <i>specifier</i> can be one of the following characters:
 * <ul>
 * <li><b>c</b> character from an argument promoted to int
 * <li><b>d</b> or <b>i</b> decimal integer number (width is only supported
 *     with zero padding, other flags and precision are not yet supported)
 * <li><b>o</b> octal integer number (flags besides <b>#</b> and precision are
 *     not yet supported)
 * <li><b>u</b> unsigned decimal integer number (flags and precision are not
 *     yet supported)
 * <li><b>x</b> or <b>X</b> hexadecimal integer number, using upper case letters
 *     (flags besides <b>#</b> and precision are not yet supported)
 * <li><b>p</b> pointer, using platform dependent representation
 * <li><b>s</b> nul-terminated string (const char*) (precision supported)
 * <li>any other character is output verbatim (including a literal <b>%</b>).
 * </ul>
 *
 * @return The number of chars appended on success, or a negative errno on failure.
 */
int Formatter_printf(Appendable *appendable, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int res = Formatter_vprintf(appendable, fmt, args);
    va_end(args);
    return res;
}
