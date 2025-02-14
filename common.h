#ifndef COMMON_H
#define COMMON_H

#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <png.h>
#include <getopt.h>
#include <assert.h>
#include "auto_split.h"
#include "palletize.h"
#include "sup.h"
#include "ass.h"
#include "abstract_lists.h"

/* AVIS input code taken from muxers.c from the x264 project (GPLv2 or later).
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Loren Merritt <lorenm@u.washington.edu>
 * Slightly modified, so if you find breakage, it's probably mine. :)
 */
#ifndef LINUX
#include <windows.h>
#include <vfw.h>
#endif

#ifndef LINUX
static int64_t gcd( int64_t a, int64_t b )
{
    while (1)
    {
        int64_t c = a % b;
        if( !c )
            return b;
        a = b;
        b = c;
    }
}
#endif

typedef struct {
#if !defined(LINUX)
    PAVISTREAM p_avi;
#else
    int fps_den;
    int fps_num;
    int frames;
    FILE *fh;
#endif
    int width, height;
} avis_input_t;

typedef struct {
    int i_width;
    int i_height;
    int i_fps_den;
    int i_fps_num;
} stream_info_t;

int open_file_avis( char *psz_filename, avis_input_t **p_handle, stream_info_t *p_param );
int get_frame_total_avis( avis_input_t *handle );

int read_frame_avis( char *p_pic, avis_input_t *handle, int i_frame );
int close_file_avis( avis_input_t *handle );

/* AVIS code ends here */

/* Main avs2bdnxml code starts here, too */

void get_dir_path(char *filename, char *dir_path);

void write_png(char *dir, int file_id, uint8_t *image, int w, int h, int graphic, uint32_t *pal, crop_t c);

int is_identical_c (stream_info_t *s_info, char *img, char *img_old);

int is_empty_c (stream_info_t *s_info, char *img);

void zero_transparent_c (stream_info_t *s_info, char *img);

void swap_rb_c (stream_info_t *s_info, char *img, char *out);

int is_identical (stream_info_t *s_info, char *img, char *img_old);

int is_empty (stream_info_t *s_info, char *img);

void zero_transparent (stream_info_t *s_info, char *img);

void swap_rb (stream_info_t *s_info, char *img, char *out);

/* SMPTE non-drop time code */
void mk_timecode (int frame, int fps, char *buf); /* buf must have length 12 (incl. trailing \0) */

void print_usage ();

// extern char *rindex(const char *s, int c);
int is_extension(const char *filename, char *check_ext);
int parse_int(char *in, char *name, int *error);

int parse_tc(char *in, int fps);

typedef struct event_s
{
    int image_number;
    int start_frame;
    int end_frame;
    int graphics;
    int forced;
    crop_t c[2];
} event_t;

STATIC_LIST(event, event_t)

void add_event_xml_real (event_list_t *events, int image, int start, int end, int graphics, crop_t *crops, int forced);

void add_event_xml (event_list_t *events, int split_at, int min_split, int start, int end, int graphics, crop_t *crops, int forced);

void write_sup_wrapper (sup_writer_t *sw, uint8_t *im, int num_crop, crop_t *crops, uint32_t *pal, int start, int end, int split_at, int min_split, int stricter, int forced);


struct framerate_entry_s
{
    char *name;
    char *out_name;
    int rate;
    int drop;
    int fps_num;
    int fps_den;
};
#ifdef __cplusplus
}
#endif
#endif // COMMON_H
