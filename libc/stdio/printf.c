/*
 * This file based on printf.c from 'Dlibs' on the atari ST  (RdeBath)
 *
 * 19-OCT-88: Dale Schumacher
 * > John Stanley has again been a great help in debugging, particularly
 * > with the printf/scanf functions which are his creation.  
 *
 *    Dale Schumacher                         399 Beacon Ave.
 *    (alias: Dalnefre')                      St. Paul, MN  55104
 *    dal@syntel.UUCP                         United States of America
 *  "It's not reality that's important, but how you perceive things."
 *
 */

/* Altered to use stdarg, made the core function vfprintf.
 * Hooked into the stdio package using 'inside information'
 * Altered sizeof() assumptions, now assumes all integers except chars
 * will be either
 *  sizeof(xxx) == sizeof(long) or sizeof(xxx) == sizeof(short)
 *
 * -RDB
 */

#include <sys/types.h>

#include <fcntl.h>
#include <stdio.h>
#include <string.h>


extern char * ltostr (long val, int radix);
extern char * ultostr (unsigned long val, int radix);


int sprintf (char * sp, const char * fmt, ...)
{
static FILE  string[1] =
{
   {0, 0, (char*)(unsigned) -1, 0, (char*) (unsigned) -1, -1,
    _IOFBF | __MODE_WRITE}
};

  va_list ptr;
  int rv;
  va_start(ptr, fmt);
  string->bufpos = sp;
  rv = vfprintf(string,fmt,ptr);
  va_end(ptr);
  *(string->bufpos) = 0;
  return rv;
}

#ifdef L_vprintf
int vprintf(fmt, ap)
__const char *fmt;
va_list ap;
{
  return vfprintf(stdout,fmt,ap);
}
#endif

int vsprintf(sp, fmt, ap)
char * sp;
__const char *fmt;
va_list ap;
{
static FILE  string[1] =
{
   {0, 0, (char*)(unsigned) -1, 0, (char*) (unsigned) -1, -1,
    _IOFBF | __MODE_WRITE}
};

  int rv;
  string->bufpos = sp;
  rv = vfprintf(string,fmt,ap);
  *(string->bufpos) = 0;
  return rv;
}

#ifndef __HAS_NO_FLOATS__
int (*__fp_print)() = 0;
#endif

static int prtfld(FILE *op, unsigned char *buf,
	int ljustf, char sign, char pad, int width, int preci, int buffer_mode)
/*
 * Output the given field in the manner specified by the arguments. Return
 * the number of characters output.
 */
{
   register int cnt = 0, len;
   register unsigned char ch;

   len = strlen(buf);

   if (*buf == '-')
      sign = *buf++;
   else if (sign)
      len++;

   if ((preci != -1) && (len > preci))	/* limit max data width */
      len = preci;

   if (width < len)		/* flexible field width or width overflow */
      width = len;

   /*
    * at this point: width = total field width len   = actual data width
    * (including possible sign character)
    */
   cnt = width;
   width -= len;

   while (width || len)
   {
      if (!ljustf && width)	/* left padding */
      {
	 if (len && sign && (pad == '0'))
	    goto showsign;
	 ch = pad;
	 --width;
      }
      else if (len)
      {
	 if (sign)
	 {
	  showsign:ch = sign;	/* sign */
	    sign = '\0';
	 }
	 else
	    ch = *buf++;	/* main field */
	 --len;
      }
      else
      {
	 ch = pad;		/* right padding */
	 --width;
      }
      putc(ch, op);
      if( ch == '\n' && buffer_mode == _IOLBF ) fflush(op);
   }

   return (cnt);
}

int vfprintf(FILE *op, const char *fmt, va_list ap)
{
   register int i, cnt = 0, ljustf, lval;
   int   preci, dpoint, width;
   char  pad, sign, radix, hash;
   register char *ptmp;
   char  tmp[64];
   int buffer_mode;

   /* This speeds things up a bit for unbuffered */
   buffer_mode = (op->mode&__MODE_BUF);
   op->mode &= (~__MODE_BUF);

   while (*fmt)
   {
      if (*fmt == '%')
      {
         if( buffer_mode == _IONBF ) fflush(op);
	 ljustf = 0;		/* left justify flag */
	 sign = '\0';		/* sign char & status */
	 pad = ' ';		/* justification padding char */
	 width = -1;		/* min field width */
	 dpoint = 0;		/* found decimal point */
	 preci = -1;		/* max data width */
	 radix = 10;		/* number base */
	 ptmp = tmp;		/* pointer to area to print */
	 hash = 0;
	 lval = (sizeof(int)==sizeof(long));	/* long value flaged */
       fmtnxt:
	 i = 0;
	 for(;;)
	 {
	    ++fmt;
	    if(*fmt < '0' || *fmt > '9' ) break;
	    i = (i * 10) + (*fmt - '0');
	    if (dpoint)
	       preci = i;
	    else if (!i && (pad == ' '))
	    {
	       pad = '0';
	       goto fmtnxt;
	    }
	    else
	       width = i;
	 }

	 switch (*fmt)
	 {
	 case '\0':		/* early EOS */
	    --fmt;
	    goto charout;

	 case '-':		/* left justification */
	    ljustf = 1;
	    goto fmtnxt;

	 case ' ':
	 case '+':		/* leading sign flag */
	    sign = *fmt;
	    goto fmtnxt;

	 case '*':		/* parameter width value */
	    i = va_arg(ap, int);
	    if (dpoint)
	       preci = i;
	    else
	       width = i;
	    goto fmtnxt;

	 case '.':		/* secondary width field */
	    dpoint = 1;
	    goto fmtnxt;

	 case 'l':		/* long data */
	    lval = 1;
	    goto fmtnxt;

	 case 'h':		/* short data */
	    lval = 0;
	    goto fmtnxt;

	 case 'd':		/* Signed decimal */
	 case 'i':
	    ptmp = ltostr((long) ((lval)
			 ? va_arg(ap, long)
			 : va_arg(ap, int)),
		 10);
	    goto printit;

	 case 'b':		/* Unsigned binary */
	    radix = 2;
	    goto usproc;

	 case 'o':		/* Unsigned octal */
	    radix = 8;
	    goto usproc;

	 case 'p':		/* Pointer */
	    lval = (sizeof(char*) == sizeof(long));
	    pad = '0';
	    width = 6;
	    preci = 8;
	    /* fall thru */

	 case 'x':		/* Unsigned hexadecimal */
	 case 'X':
	    radix = 16;
	    /* fall thru */

	 case 'u':		/* Unsigned decimal */
	  usproc:
	    ptmp = ultostr((unsigned long) ((lval)
				   ? va_arg(ap, unsigned long)
				   : va_arg(ap, unsigned int)),
		  radix);
	    if( hash && radix == 8 ) { width = strlen(ptmp)+1; pad='0'; }
	    goto printit;

	 case '#':
	    hash=1;
	    goto fmtnxt;

	 case 'c':		/* Character */
	    ptmp[0] = va_arg(ap, int);
	    ptmp[1] = '\0';
	    goto nopad;

	 case 's':		/* String */
	    ptmp = va_arg(ap, char*);
	  nopad:
	    sign = '\0';
	    pad = ' ';
	  printit:
	    cnt += prtfld(op, ptmp, ljustf,
			   sign, pad, width, preci, buffer_mode);
	    break;

#ifndef __HAS_NO_FLOATS__
	 case 'e':		/* float */
	 case 'f':
	 case 'g':
	 case 'E':
	 case 'G':
	    if ( __fp_print )
	    {
	       (*__fp_print)(&va_arg(ap, double), *fmt, preci, ptmp);
	       preci = -1;
	       goto printit;
	    }
	    /* FALLTHROUGH if no floating printf available */
#endif

	 default:		/* unknown character */
	    goto charout;
	 }
      }
      else
      {
       charout:
	 putc(*fmt, op);	/* normal char out */
	 ++cnt;
         if( *fmt == '\n' && buffer_mode == _IOLBF ) fflush(op);
      }
      ++fmt;
   }
   op->mode |= buffer_mode;
   if( buffer_mode == _IONBF ) fflush(op);
   if( buffer_mode == _IOLBF ) op->bufwrite = op->bufstart;
   return (cnt);
}

int printf(const char * fmt, ...)
{
  va_list ptr;
  int rv;
  va_start(ptr, fmt);
  rv = vfprintf(stdout,fmt,ptr);
  va_end(ptr);
  return rv;
}

int fprintf(FILE * fp, const char * fmt, ...)
{
  va_list ptr;
  int rv;
  va_start(ptr, fmt);
  rv = vfprintf(fp,fmt,ptr);
  va_end(ptr);
  return rv;
}

#ifdef L_fp_print
#ifndef __HAS_NO_FLOATS__

#ifdef __AS386_16__
#asm
  loc   1         ! Make sure the pointer is in the correct segment
auto_func:        ! Label for bcc -M to work.
  .word ___xfpcvt ! Pointer to the autorun function
  .text           ! So the function after is also in the correct seg.
#endasm
#endif

#ifdef __AS386_32__
#asm
  loc   1         ! Make sure the pointer is in the correct segment
auto_func:        ! Label for bcc -M to work.
  .long ___xfpcvt ! Pointer to the autorun function
  .text           ! So the function after is also in the correct seg.
#endasm
#endif

void
__fp_print_func(pval, style, preci, ptmp)
   double * pval;
   int style;
   int preci;
   char * ptmp;
{
   int decpt, negative;
   char * cvt;
   double val = *pval;

   if (preci < 0) preci = 6;

   cvt = fcvt(val, preci, &decpt, &negative);
   if(negative)
      *ptmp++ = '-';

   if (decpt<0) {
      *ptmp++ = '0';
      *ptmp++ = '.';
      while(decpt<0) {
	 *ptmp++ = '0'; decpt++;
      }
   }

   while(*cvt) {
      *ptmp++ = *cvt++;
      if (decpt == 1)
	 *ptmp++ = '.';
      decpt--;
   }

   while(decpt > 0) {
      *ptmp++ = '0';
      decpt--;
   }
}

void
__xfpcvt()
{
   extern int (*__fp_print)();
   __fp_print = __fp_print_func;
}

#endif
#endif
