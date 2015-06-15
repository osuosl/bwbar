/* $Id: bwbar.c,v 1.7 2006/08/26 16:31:26 hpa Exp $ */
/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 1999-2006 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
 *   USA; either version 2 of the License, or (at your option) any later
 *   version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * bwbar.c
 *
 * Bandwidth monitor using /proc/net/dev which produces output which can
 * be displayed on a web page.
 *
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <alloca.h>
#include <getopt.h>
#include <png.h>
#include <zlib.h>

void skipline(FILE *f)
{
  int ch;
  do {
    ch = getc(f);
  } while ( ch != '\n' && ch != EOF );
}

int write_bar_graph(FILE *f, double perc, int width, int height, int border, int spacers)
{
  static png_color palette[4] =
  {
    {  0,   0,   0},		/* RGB color for border */
    {255, 255, 255},		/* RGB color for separators */
    {214, 136, 131},		/* RGB color for unfilled part of graph */
    {204, 204, 227},		/* RGB color for filled part of graph */
  };
  int bwidth, bheight;		/* Width and height including border */
  double frac;
  int spacer, fspacer;
  int x, y;
  int color;
  png_structp png_ptr = NULL;
  png_infop info_ptr  = NULL;
  png_byte *border_row;
  png_byte *bar_row;
  png_byte *bp;
  int status = 1;		/* Assume failure */

  bwidth = width + border*2;
  bheight = height + border*2;

  border_row   = alloca(sizeof(png_byte) * bwidth);
  bar_row      = alloca(sizeof(png_byte) * bwidth);

  if ( !border_row || !bar_row )
    goto barf;

  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
				    NULL, NULL, NULL);

  if ( !png_ptr )
    goto barf;

  info_ptr = png_create_info_struct(png_ptr);
  if ( !info_ptr )
    goto barf;

  if ( setjmp(png_jmpbuf((png_ptr))) ) {
    status = 1;
    goto barf;			/* libpng abort */
  }

  png_init_io(png_ptr, f);

  png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);

  png_set_IHDR(png_ptr, info_ptr, bwidth, bheight,
	       2, PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
	       PNG_COMPRESSION_TYPE_DEFAULT,
	       PNG_FILTER_TYPE_DEFAULT);

  png_set_PLTE(png_ptr, info_ptr, palette, 4);

  png_write_info(png_ptr, info_ptr);

  png_set_packing(png_ptr);

  /* Now we actually produce the data */

  /* Generate the border row */
  memset(border_row, 0, bwidth);

  /* Generate the bar row */
  bp = bar_row;
  for ( x = 0 ; x < border ; x++ )
    *bp++ = 0;			/* Border color */

  spacer = 0;
  for ( x = 1 ; x <= width ; x++ ) {
    frac = (double)x/(double)width;
    fspacer = (int)(spacers*frac);
    if ( x < width && spacer < fspacer ) {
      color = 1;		/* Spacer color */
    } else if ( perc < frac ) {
      color = 3;		/* Bar color */
    } else {
      color = 2;		/* Background color */
    }
    *bp++ = color;
    spacer = fspacer;
  }
  
  for ( x = 0 ; x < border ; x++ )
    *bp++ = 0;			/* Border color */

  /* Write output */
  for ( y = 0 ; y < border ; y++ )
    png_write_row(png_ptr, border_row);
  for ( y = 0 ; y < height ; y++ )
    png_write_row(png_ptr, bar_row);
  for ( y = 0 ; y < border ; y++ )
    png_write_row(png_ptr, border_row);

  /* Done writing */
  png_write_end(png_ptr, info_ptr);
  status = 0;			/* Success! */

 barf:				/* Jump here on fatal error */
  png_destroy_write_struct(&png_ptr, &info_ptr);
  return status;
}

const struct option longopts[] = {
  { "input",        0, 0, 'i' },
  { "output",       0, 0, 'o' },
  { "text-file",    1, 0, 'f' },
  { "png-file",     1, 0, 'p' },
  { "interval",     1, 0, 't' },
  { "width",        1, 0, 'x' },
  { "height",       1, 0, 'y' },
  { "border",       1, 0, 'b' },
  { "kbps",         0, 0, 'k' },
  { "Mbps",         0, 0, 'M' },
  { "Gbps",         0, 0, 'G' },
  { "help",         0, 0, 'h' },
  { 0, 0, 0, 0 }
};

const char *program;

void usage(int err)
{
  fprintf(stderr,
	  "Usage: %s [options] interface max_mbps\n"
	  "Options: (defaults in parenthesis)\n"
	  "   --input               -i   Measure input bandwidth\n"
	  "   --output              -o   Measure output bandwidth (default)\n"
	  "   --text-file <file>    -f   The name of the text output file (ubar.txt)\n"
	  "   --png-file <file>     -p   The name of the graphical bar file (ubar.png)\n"
	  "   --interval <seconds>  -t   The poll interval in seconds (15)\n"
	  "   --width <pixels>      -x   Width of the graphical bar (600)\n"
	  "   --height <pixels>     -y   Height of the graphical bar (4)\n"
	  "   --border <pixels>     -b   Border width of the graphical bar (1)\n"
	  "   --kbps                -k   Bandwidth is measured in kbit/s\n"
	  "   --Mbps                -M   Bandwidth is measured in Mbit/s (default)\n"
	  "   --Gbps                -G   Bandwidth is measured in Gbit/s\n"
	  "   --help                -h   Display this text\n",
	  program);
  exit(err);
}

int main(int argc, char *argv[])
{
  FILE *pnd;
  char *interface;
  struct ifinfo {
    char name[8];
    unsigned int r_bytes, r_pkt, r_err, r_drop, r_fifo, r_frame;
    unsigned int r_compr, r_mcast;
    unsigned int x_bytes, x_pkt, x_err, x_drop, x_fifo, x_coll;
    unsigned int x_carrier, x_compr;
  } ifc;
  unsigned long long bin, bout, lbin, lbout;
  int first;
  struct timeval t_now, t_last;
  double maxbandwidth;
  double bwin, bwout, bwmeasure, timedelta;
  int opt;
  char *t_tmp, *g_tmp;
  /* Options */
  int measure_input = 0;	/* Input instead of output */
  char *text_file = "ubar.txt";	/* Text filename */
  char *graphics_file = "ubar.png"; /* Graphics filename */
  char *unit_name = "Mbit/s";	/* Unit name */
  double unit = 1.0e+6;		/* Unit multiplier */
  int interval = 15;		/* Interval between measurements (s) */
  int width = 600;		/* Bar width */
  int height = 4;		/* Bar height */
  int border = 1;		/* Bar border */

  program = argv[0];

  while ( (opt = getopt_long(argc, argv, "iof:p:t:x:y:b:kMGh", longopts, NULL)) != -1 ) {
    switch ( opt ) {
    case 'i':
      measure_input = 1;
      break;
    case 'o':
      measure_input = 0;
      break;
    case 'f':
      text_file = optarg;
      break;
    case 'p':
      graphics_file = optarg;
      break;
    case 't':
      interval = atoi(optarg);
      break;
    case 'x':
      width = atoi(optarg);
      break;
    case 'y':
      height = atoi(optarg);
      break;
    case 'b':
      border = atoi(optarg);
      break;
    case 'k':
      unit = 1.0e+3;
      unit_name = "kbit/s";
      break;
    case 'M':
      unit = 1.0e+6;
      unit_name = "Mbit/s";
      break;
    case 'G':
      unit = 1.0e+9;
      unit_name = "Gbit/s";
      break;
    case 'h':
      usage(0);
      break;
    default:
      usage(1);
      break;
    }
  }

  if ( argc-optind != 2 )
    usage(1);

  t_tmp = malloc(strlen(text_file) + 5);
  g_tmp = malloc(strlen(graphics_file) + 5);
  if ( !t_tmp || !g_tmp ) {
    perror(program);
    exit(1);
  }
  sprintf(t_tmp, "%s.tmp", text_file);
  sprintf(g_tmp, "%s.tmp", graphics_file);

  interface = argv[optind];
  maxbandwidth = atof(argv[optind+1]) * unit;

  first = 1;
  lbin = 0; lbout = 0;

  gettimeofday(&t_last, NULL);
  
  while ( 1 ) {

    /**** Begin code that obtains bandwidth data ****/

    gettimeofday(&t_now, NULL);
    pnd = fopen("/proc/net/dev", "r");
    if ( !pnd ) {
      fprintf(stderr, "%s: /proc/net/dev: %s", argv[0], strerror(errno));
      exit(1);
    }

    /* Skip header */
    skipline(pnd);
    skipline(pnd);

    /* Get interface info */
    do {
      if ( fscanf(pnd, " %[^:]:%u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u",
		  ifc.name,
		  &ifc.r_bytes, &ifc.r_pkt, &ifc.r_err, &ifc.r_drop,
		  &ifc.r_fifo, &ifc.r_frame, &ifc.r_compr, &ifc.r_mcast,
		  &ifc.x_bytes, &ifc.x_pkt, &ifc.x_err, &ifc.x_drop,
		  &ifc.x_fifo, &ifc.x_coll, &ifc.x_carrier, &ifc.x_compr)
	   != 17 ) {
	exit(200);
      }
      skipline(pnd);
    } while ( strcmp(ifc.name, interface) );

    bin  = ifc.r_bytes + (lbin & ~0xffffffffULL);
    bout = ifc.x_bytes + (lbout & ~0xffffffffULL);

    if ( bin < lbin )
      bin += (1ULL << 32);
    if ( bout < lbout )
      bout += (1ULL << 32);

    /**** Begin code that generates output ****/

    if ( !first ) {
      FILE *ubar, *pngmaker;

      timedelta = (double)(t_now.tv_sec - t_last.tv_sec) + 
	(t_now.tv_usec - t_last.tv_usec)/1.0e+6;
      bwin = (bin-lbin)*8/timedelta;
      bwout = (bout-lbout)*8/timedelta;

      bwmeasure = measure_input ? bwin : bwout;
      
      ubar = fopen(t_tmp, "w");
      pngmaker = fopen(g_tmp, "w");
      if ( ubar ) {
	fprintf(ubar, "Current bandwidth utilization %6.2f %s\n", bwmeasure/unit, unit_name);
	fclose(ubar);
	rename(t_tmp, text_file);
      }
      if ( pngmaker ) {
	write_bar_graph(pngmaker, bwmeasure/maxbandwidth, width, height, border, 5);
	fclose(pngmaker);
	rename(g_tmp, graphics_file);
      }
    } else {
      first = 0;
    }

    lbin = bin;  lbout = bout;  t_last = t_now;

    fclose(pnd);

    sleep(interval);
  }
}
