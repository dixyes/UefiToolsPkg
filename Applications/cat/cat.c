/* $NetBSD: cat.c,v 1.47.20.1 2017/03/25 17:41:48 snj Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kevin Fall.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <Library/StdExtLib.h>

int bflag, eflag, fflag, lflag, nflag, sflag, tflag, vflag;
int rval;
const char *filename;
const char *outname = NULL;

int main(int, char *[]);
void cook_args(char *argv[], int wfd);
void cook_buf(FILE *, int wfd);
void raw_args(char *argv[], int wfd);
void raw_cat(int rfd, int wfd);

int
main(int argc, char *argv[])
{
	int ch;
	int wfd;
	/* struct flock stdout_lock; */

	setprogname(argv[0]);
	(void)setlocale(LC_ALL, "");

	while ((ch = getopt(argc, argv, "beflnstuvo:")) != -1)
		switch (ch) {
		case 'b':
			bflag = nflag = 1;	/* -b implies -n */
			break;
		case 'e':
			eflag = vflag = 1;	/* -e implies -v */
			break;
		case 'f':
			fflag = 1;
			break;
		case 'l':
			lflag = 1;
			break;
		case 'n':
			nflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		case 't':
			tflag = vflag = 1;	/* -t implies -v */
			break;
		case 'u':
			setbuf(stdout, NULL);
			break;
		case 'v':
			vflag = 1;
			break;
		case 'o':
			outname = optarg;
			break;
		default:
		case '?':
			(void)fprintf(stderr,
			    "usage: cat [-o outfile] [-beflnstuv] [-] [file ...]\n");
			exit(EXIT_FAILURE);
			/* NOTREACHED */
		}
	argv += optind;

	if (lflag) {
		/* stdout_lock.l_len = 0; */
		/* stdout_lock.l_start = 0; */
		/* stdout_lock.l_type = F_WRLCK; */
		/* stdout_lock.l_whence = SEEK_SET; */
		/* if (fcntl(STDOUT_FILENO, F_SETLKW, &stdout_lock) == -1) */
		/* 	err(EXIT_FAILURE, "stdout"); */
	}

	if (outname != NULL) {
		wfd = open(outname, O_CREAT | O_TRUNC | O_WRONLY, 0666);
	} else {
		wfd = fileno(stdout);
	}

	if (bflag || eflag || nflag || sflag || tflag || vflag)
		cook_args(argv, wfd);
	else
		raw_args(argv, wfd);
	if (fclose(stdout))
		err(EXIT_FAILURE, "stdout");
	return (rval);
}

void
cook_args(char **argv, int wfd)
{
	FILE *fp;

	fp = stdin;
	filename = "stdin";
	do {
		if (*argv) {
			if (!strcmp(*argv, "-"))
				fp = stdin;
			else if ((fp = fopen(*argv,
			    fflag ? "rf" : "r")) == NULL) {
				warn("%s", *argv);
				rval = EXIT_FAILURE;
				++argv;
				continue;
			}
			filename = *argv++;
		}
		cook_buf(fp, wfd);
		if (fp != stdin)
			(void)fclose(fp);
		else
			clearerr(fp);
	} while (*argv);
}

void
cook_buf(FILE *fp, int wfd)
{
	int ch, gobble, line, prev;

	line = gobble = 0;
	for (prev = '\n'; (ch = getc(fp)) != EOF; prev = ch) {
		if (prev == '\n') {
			if (sflag) {
				if (ch == '\n') {
					if (gobble)
						continue;
					gobble = 1;
				} else
					gobble = 0;
				}
				if (nflag) {
					if (!bflag || ch != '\n') {
						(void)fprintf(stdout,
						    "%6d\t", ++line);
						if (ferror(stdout))
							break;
					} else if (eflag) {
						(void)fprintf(stdout,
						    "%6s\t", "");
						if (ferror(stdout))
							break;
					}
				}
			}
		if (ch == '\n') {
			if (eflag)
				if (putchar('$') == EOF)
					break;
		} else if (ch == '\t') {
			if (tflag) {
				if (putchar('^') == EOF || putchar('I') == EOF)
					break;
				continue;
			}
		} else if (vflag) {
			if (!isascii(ch)) {
				if (putchar('M') == EOF || putchar('-') == EOF)
					break;
				ch = toascii(ch);
			}
			if (iscntrl(ch)) {
				if (putchar('^') == EOF ||
				    putchar(ch == '\177' ? '?' :
				    ch | 0100) == EOF)
					break;
				continue;
			}
		}
		if (putchar(ch) == EOF)
			break;
	}
	if (ferror(fp)) {
		warn("%s", filename);
		rval = EXIT_FAILURE;
		clearerr(fp);
	}
	if (ferror(stdout))
		err(EXIT_FAILURE, "stdout");
}

void
raw_args(char **argv, int wfd)
{
	int fd;

	fd = fileno(stdin);
	filename = "stdin";
	do {
		if (*argv) {
			if (!strcmp(*argv, "-"))
				fd = fileno(stdin);
			else if (fflag) {
				struct stat st;
				fd = open(*argv, O_RDONLY|O_NONBLOCK, 0);
				if (fd < 0)
					goto skip;

				if (fstat(fd, &st) == -1) {
					close(fd);
					goto skip;
				}
				if (!S_ISREG(st.st_mode)) {
					close(fd);
					warnx("%s: not a regular file", *argv);
					goto skipnomsg;
				}
			}
			else if ((fd = open(*argv, O_RDONLY, 0)) < 0) {
skip:
				warn("%s", *argv);
skipnomsg:
				rval = EXIT_FAILURE;
				++argv;
				continue;
			}
			filename = *argv++;
		}
		raw_cat(fd, wfd);
		if (fd != fileno(stdin))
			(void)close(fd);
	} while (*argv);
}

void
raw_cat(int rfd, int wfd)
{
	static char *buf;
	static char fb_buf[BUFSIZ];
	static size_t bsize;

	ssize_t nr, nw, off;

	if (buf == NULL) {
		struct stat sbuf;

		if (fstat(wfd, &sbuf) == 0 &&
		    sbuf.st_blksize > sizeof(fb_buf)) {
			bsize = sbuf.st_blksize;
			buf = malloc(bsize);
		}
		if (buf == NULL) {
			bsize = sizeof(fb_buf);
			buf = fb_buf;
		}
	}
	while ((nr = read(rfd, buf, bsize)) > 0) {
		for (off = 0; nr; nr -= nw, off += nw) {
			if ((nw = write(wfd, buf + off, (size_t)nr)) < 0)
				err(EXIT_FAILURE, "stdout");

			if (nw == 0) {
				err(EXIT_FAILURE, "short write to stdout");
			}
		}
	}

	if (nr < 0) {
		warn("%s", filename);
		rval = EXIT_FAILURE;
	}
}
