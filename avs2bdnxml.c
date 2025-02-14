/*----------------------------------------------------------------------------
 * avs2bdnxml - Generates BluRay subtitle stuff from RGBA AviSynth scripts
 * Copyright (C) 2008-2013 Arne Bochem <avs2bdnxml at ps-auxw de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *----------------------------------------------------------------------------
 * Version 2.09
 *   - Added parameter -F to mark all subtitles forced
 *
 * Version 2.08
 *   - Fix PNG filename references when used with timecode offsets
 *
 * Version 2.07
 *   - Fix two bugs in timecode parsing function
 *   - Move most inline assembly to yasm
 *
 * Version 2.06
 *   - Add option to add an offset to output timecodes
 *   - Change return value when no events were detected to 0
 *   - Add option to allow writing of empty output files
 *   - Add option to enforce even Y coordinates (may help avoid problems with
 *     interlaced video in DVD related use cases)
 *   - Fix SSE2 with more recent GCC versions
 *   - Add option for stricter checks on buffer/palette limits for SUPs
 *   - Fix bug where LastEventOutTC could be one frame early
 *   - Add additional checks to commandline argument parsing
 *   - PNG files are now always placed in the same directory as the XML file
 *
 * Version 2.05
 *   - Fix crash when using XML and SUP output
 *
 * Version 2.04
 *   - Minor fixes
 *   - Code cleanup regarding linked lists
 *   - Add new minimal window finding code
 *   - Fix PGS output more, especially WDS handling
 *   - Fix old bug in RLE encoder
 *
 * Version 2.03
 *   - Output some statistics about previous epoch in pgsparse in end packet
 *   - Fix unnecessary epoch splits due to bad buffer calculation and more.
 *   - Some small fixes
 *
 * Version 2.02
 *   - It is now possible to use -o twice, with different output formats
 *   - Corrections to SUP timestamp calculation
 *   - PGS decode buffer should contain palettized image data, not RGBA.
 *     Buffer usage calculation fixed.
 *
 * Version 2.01
 *   - Restrict number of composition objects within an epoch to 64
 *
 * Version 2.00
 *   - Options --seek and --count were added, to allow for partial processing
 *     of input video
 *   - For debug purposes, a stand-alone SUP file parser was added in debug/
 *   - Experimental SUP output support
 *
 * Version 1.13
 *   - By default, images are no longer split into to composition objects,
 *     if the result would look ugly. For example, descenders should no
 *     longer get put into their own image. Reverting back to the old
 *     behaviour is possible using the --ugly option.
 *   - Ensure minimum size of 8x8 for composition objects
 *
 * Version 1.12
 *   - Fix error in example
 *   - Fix double free, which could result in broken images when image
 *     splitting and palletized output were enabled.
 *
 * Version 1.11
 *   - Add option to not split lines, if the rest isn't a minimum
 *     of 3 (default) frames long
 *   - Images are now autocropped by default
 *   - Images are now converted to 8bit palette by default
 *   - Misc bugfixes that should've been fixed before
 *
 * Version 1.10b
 *   - Splitting of images is now disabled by default
 *
 * Version 1.10
 *   - Add option to split events longer than a given number of frames, to
 *     allow better seeking
 *   - Change image filename format to XXXXXXXX_X.png
 *   - Add option to optimize for Presentation Graphics buffer usage, by
 *     auto-cropping images, and splitting them into two separate ones to
 *     minimize surface area, when necessary (enabled by default)
 *   - Use getopts for commandline argument parsing
 *   - Add Makefile
 *
 * Version 1.9
 *   - Correct calculation of timecodes. Integer only, now.
 *   - Remove support for drop timecodes. If anybody really needs them, and
 *     has a way to check calculated timecodes are correct, please tell me.
 *
 * Version 1.8
 *   - Fix crash with certain frame sizes (unaligned buffer used with SSE2)
 *
 * Version 1.7
 *   - Terminate using exit/return instead of abort (no more "crashes")
 *
 * Version 1.6
 *   - Zero transparent pixels to guard against invisible color information
 *     (Slight slowdown, but more robustness)
 *   - Add SSE2 runtime detection
 *   - Correctly set DropFrame attribute
 *
 * Version 1.5
 *   - Validate framerates
 *   - Make non-drop timecodes the default for 23.976, 29.97 and 59.94
 *   - Fix bad timecode calculation due to float math optimization
 *   - Add integer SSE2 optimization (but most time is spent in AviSynth)
 *   - Don't uselessly swap R and B channels
 *
 * Version 1.4
 *   - Add 30 to framerate list
 *   - Enhance timecode precision
 *
 * Version 1.3
 *   - Read frames correctly in avs2bdnxml. No more FlipVertical() necessary.
 *
 * Version 1.2
 *   - Fixed bug, where only the bottom part of the image was checked for
 *     changes
 *
 * Version 1.1
 *   - Fixed crash bug with videos shorter than 1000 frames.
 *   - Convert from BGR to RGB, colors in PNGs should be correct now.
 *
 * Version 1.0
 *   - Initial release. Seems to work.
 *----------------------------------------------------------------------------
 * Thanks to:
 *   - hamletiii on Doom9 for a steady flow of feedback.
 *   - Daiz in #x264 for inspiration, testing, and bug reports.
 *   - Loren Merritt and Laurent Aimar for writing x264's AVS reader code.
 *----------------------------------------------------------------------------*/

#include "common.h"

/* Most of the time seems to be spent in AviSynth (about 4/5). */
int main (int argc, char *argv[])
{
    int result = 0;
	struct framerate_entry_s framerates[] = { {"23.976", "23.976", 24, 0, 24000, 1001}
	                                        /*, {"23.976d", "23.976", 24000/1001.0, 1}*/
	                                        , {"24", "24", 24, 0, 24, 1}
	                                        , {"25", "25", 25, 0, 25, 1}
	                                        , {"29.97", "29.97", 30, 0, 30000, 1001}
	                                        /*, {"29.97d", "29.97", 30000/1001.0, 1}*/
	                                        , {"50", "50", 50, 0, 50, 1}
	                                        , {"59.94", "59.94", 60, 0, 60000, 1001}
	                                        /*, {"59.94d", "59.94", 60000/1001.0, 1}*/
	                                        , {NULL, NULL, 0, 0, 0, 0}
	                                        };
	char *avs_filename = NULL;
	char *track_name = "Undefined";
	char *language = "und";
	char *video_format = "1080p";
	char *frame_rate = "23.976";
	char *out_filename[2] = {NULL, NULL};
	char *sup_output_fn = NULL;
	char *xml_output_fn = NULL;
	char *x_offset = "0";
	char *y_offset = "0";
	char *t_offset = "0";
	char *buffer_optimize = "0";
	char *split_after = "0";
	char *minimum_split = "3";
	char *palletize_png = "1";
	char *even_y_string = "0";
	char *auto_crop_image = "1";
	char *ugly_option = "0";
	char *seek_string = "0";
	char *allow_empty_string = "0";
	char *stricter_string = "0";
	char *count_string = "2147483647";
	char *in_img = NULL, *old_img = NULL, *tmp = NULL, *out_buf = NULL;
	char *intc_buf = NULL, *outtc_buf = NULL;
	char *drop_frame = NULL;
    char *mark_forced_string = "0";
	char png_dir[MAX_PATH + 1] = {0};
	crop_t crops[2];
	pic_t pic;
	uint32_t *pal = NULL;
	int out_filename_idx = 0;
	int have_fps = 0;
	int n_crop = 1;
	int split_at = 0;
	int min_split = 3;
	int autocrop = 0;
	int xo, yo, to;
	int fps = 25;
	int count_frames = INT_MAX, last_frame;
	int init_frame = 0;
	int frames;
	int first_frame = -1, start_frame = -1, end_frame = -1;
	int num_of_events = 0;
	int i, c, j;
	int have_line = 0;
	int must_zero = 0;
	int checked_empty;
	int even_y = 0;
	int auto_cut = 0;
	int pal_png = 1;
	int ugly = 0;
	int progress_step = 1000;
	int buffer_opt;
	int bench_start = time(NULL);
	int fps_num = 25, fps_den = 1;
	int sup_output = 0;
	int xml_output = 0;
	int allow_empty = 0;
	int stricter = 0;
    int mark_forced = 0;
	sup_writer_t *sw = NULL;
	avis_input_t *avis_hnd;
	stream_info_t *s_info = malloc(sizeof(stream_info_t));
	event_list_t *events = event_list_new();
	event_t *event;
	FILE *fh;

	/* Get args */
	if (argc < 2)
	{
		print_usage();
		return 0;
	}
	while (1)
	{
		static struct option long_options[] =
			{ {"output",       required_argument, 0, 'o'}
			, {"seek",         required_argument, 0, 'j'}
			, {"count",        required_argument, 0, 'c'}
			, {"trackname",    required_argument, 0, 't'}
			, {"language",     required_argument, 0, 'l'}
			, {"video-format", required_argument, 0, 'v'}
			, {"fps",          required_argument, 0, 'f'}
			, {"x-offset",     required_argument, 0, 'x'}
			, {"y-offset",     required_argument, 0, 'y'}
			, {"t-offset",     required_argument, 0, 'd'}
			, {"split-at",     required_argument, 0, 's'}
			, {"min-split",    required_argument, 0, 'm'}
			, {"autocrop",     required_argument, 0, 'a'}
			, {"even-y",       required_argument, 0, 'e'}
			, {"palette",      required_argument, 0, 'p'}
			, {"buffer-opt",   required_argument, 0, 'b'}
			, {"ugly",         required_argument, 0, 'u'}
			, {"null-xml",     required_argument, 0, 'n'}
			, {"stricter",     required_argument, 0, 'z'}
			, {"forced",       required_argument, 0, 'F'}
			, {0, 0, 0, 0}
			};
			int option_index = 0;

			c = getopt_long(argc, argv, "o:j:c:t:l:v:f:x:y:d:b:s:m:e:p:a:u:n:z:F:", long_options, &option_index);
			if (c == -1)
				break;
			switch (c)
			{
				case 'o':
					if (out_filename_idx < 2)
						out_filename[out_filename_idx++] = optarg;
					else
					{
						fprintf(stderr, "No more than two output filenames allowed.\nIf more than one is used, the other must have a\ndifferent output format.\n");
						exit(0);
					}
					break;
				case 'j':
					seek_string = optarg;
					break;
				case 'c':
					count_string = optarg;
					break;
				case 't':
					track_name = optarg;
					break;
				case 'l':
					language = optarg;
					break;
				case 'v':
					video_format = optarg;
					break;
				case 'f':
					frame_rate = optarg;
					break;
				case 'x':
					x_offset = optarg;
					break;
				case 'y':
					y_offset = optarg;
					break;
				case 'd':
					t_offset = optarg;
					break;
				case 'e':
					even_y_string = optarg;
					break;
				case 'p':
					palletize_png = optarg;
					break;
				case 'a':
					auto_crop_image = optarg;
					break;
				case 'b':
					buffer_optimize = optarg;
					break;
				case 's':
					split_after = optarg;
					break;
				case 'm':
					minimum_split = optarg;
					break;
				case 'u':
					ugly_option = optarg;
					break;
				case 'n':
					allow_empty_string = optarg;
					break;
				case 'z':
					stricter_string = optarg;
					break;
				case 'F':
					mark_forced_string = optarg;
					break;
				default:
					print_usage();
					return 0;
					break;
			}
	}
	if (argc - optind == 1)
		avs_filename = argv[optind];
	else
	{
		fprintf(stderr, "Only a single input file allowed.\n");
		return 1;
	}

	/* Both input and output filenames are required */
	if (avs_filename == NULL)
	{
		print_usage();
		return 0;
	}
	if (out_filename[0] == NULL)
	{
		print_usage();
		return 0;
	}

	/* Get target output format */
	for (i = 0; i < out_filename_idx; i++)
	{
		if (is_extension(out_filename[i], "xml"))
		{
			xml_output_fn = out_filename[i];
			xml_output++;
			get_dir_path(xml_output_fn, png_dir);
		}
		else if (is_extension(out_filename[i], "sup") || is_extension(out_filename[i], "pgs"))
		{
			sup_output_fn = out_filename[i];
			sup_output++;
			pal_png = 1;
		}
		else
		{
			fprintf(stderr, "Output file extension must be \".xml\", \".sup\" or \".pgs\".\n");
			return 1;
		}
	}
	if (sup_output > 1 || xml_output > 1)
	{
		fprintf(stderr, "If more than one output filename is used, they must have\ndifferent output formats.\n");
		exit(0);
	}

	/* Set X and Y offsets, and split value */
	xo = parse_int(x_offset, "x-offset", NULL);
	yo = parse_int(y_offset, "y-offset", NULL);
	pal_png = parse_int(palletize_png, "palette", NULL);
	even_y = parse_int(even_y_string, "even-y", NULL);
	autocrop = parse_int(auto_crop_image, "autocrop", NULL);
	split_at = parse_int(split_after, "split-at", NULL);
	ugly = parse_int(ugly_option, "ugly", NULL);
	allow_empty = parse_int(allow_empty_string, "null-xml", NULL);
	stricter = parse_int(stricter_string, "stricter", NULL);
	init_frame = parse_int(seek_string, "seek", NULL);
	count_frames = parse_int(count_string, "count", NULL);
	min_split = parse_int(minimum_split, "min-split", NULL);
	if (!min_split)
		min_split = 1;
	mark_forced = parse_int(mark_forced_string, "forced", NULL);

	/* TODO: Sanity check video_format and frame_rate. */

	/* Get frame rate */
	i = 0;
	while (framerates[i].name != NULL)
	{
		if (!strcasecmp(framerates[i].name, frame_rate))
		{
			frame_rate = framerates[i].out_name;
			fps = framerates[i].rate;
			drop_frame = framerates[i].drop ? "true" : "false";
			fps_num = framerates[i].fps_num;
			fps_den = framerates[i].fps_den;
			have_fps = 1;
		}
		i++;
	}
	if (!have_fps)
	{
		fprintf(stderr, "Error: Invalid framerate (%s).\n", frame_rate);
		return 1;
	}

	/* Get timecode offset. */
	to = parse_tc(t_offset, fps);

	/* Get video info and allocate buffer */
	if (open_file_avis(avs_filename, &avis_hnd, s_info))
	{
		print_usage();
		return 1;
	}
	in_img  = calloc(s_info->i_width * s_info->i_height * 4 + 16 * 2, sizeof(char)); /* allocate + 16 for alignment, and + n * 16 for over read/write */
	old_img = calloc(s_info->i_width * s_info->i_height * 4 + 16 * 2, sizeof(char)); /* see above */
	out_buf = calloc(s_info->i_width * s_info->i_height * 4 + 16 * 2, sizeof(char));

	/* Check minimum size */
	if (s_info->i_width < 8 || s_info->i_height < 8)
	{
		fprintf(stderr, "Error: Video dimensions below 8x8 (%dx%d).\n", s_info->i_width, s_info->i_height);
		return 1;
	}

	/* Align buffers */
	in_img  = in_img + (short)(16 - ((long)in_img % 16));
	old_img = old_img + (short)(16 - ((long)old_img % 16));
	out_buf = out_buf + (short)(16 - ((long)out_buf % 16));

	/* Set up buffer (non-)optimization */
	buffer_opt = parse_int(buffer_optimize, "buffer-opt", NULL);
	pic.b = out_buf;
	pic.w = s_info->i_width;
	pic.h = s_info->i_height;
	pic.s = s_info->i_width;
	n_crop = 1;
	crops[0].x = 0;
	crops[0].y = 0;
	crops[0].w = pic.w;
	crops[0].h = pic.h;

	/* Get frame number */
	frames = get_frame_total_avis(avis_hnd);
	if (count_frames + init_frame > frames)
	{
		count_frames = frames - init_frame;
	}
	last_frame = count_frames + init_frame;

	/* No frames mean nothing to do */
	if (count_frames < 1)
	{
		fprintf(stderr, "No frames found.\n");
		return 0;
	}

	/* Set progress step */
	if (count_frames < 1000)
	{
		if (count_frames > 200)
			progress_step = 50;
		else if (count_frames > 50)
			progress_step = 10;
		else
			progress_step = 1;
	}

	/* Open SUP writer, if applicable */
	if (sup_output)
		sw = new_sup_writer(sup_output_fn, pic.w, pic.h, fps_num, fps_den);

	/* Process frames */
	for (i = init_frame; i < last_frame; i++)
	{
		if (read_frame_avis(in_img, avis_hnd, i))
		{
			fprintf(stderr, "Error reading frame.\n");
			return 1;
		}
		checked_empty = 0;

		/* Progress indicator */
		if (i % (count_frames / progress_step) == 0)
		{
			fprintf(stderr, "\rProgress: %d/%d - Lines: %d", i - init_frame, count_frames, num_of_events);
		}

		/* If we are outside any lines, check for empty frames first */
		if (!have_line)
		{
			if (is_empty(s_info, in_img))
				continue;
			else
				checked_empty = 1;
		}

		/* Check for duplicate, unless first frame */
		if ((i != init_frame) && have_line && is_identical(s_info, in_img, old_img))
			continue;
		/* Mark frames that were not used as new image in comparison to have transparent pixels zeroed */
		else if (!(i && have_line))
			must_zero = 1;

		/* Not a dup, write end-of-line, if we had a line before */
		if (have_line)
		{
			if (sup_output)
			{
				assert(pal != NULL);
				write_sup_wrapper(sw, (uint8_t *)out_buf, n_crop, crops, pal, start_frame + to, i + to, split_at, min_split, stricter, mark_forced);
				if (!xml_output)
					free(pal);
				pal = NULL;
			}
			if (xml_output)
				add_event_xml(events, split_at, min_split, start_frame + to, i + to, n_crop, crops, mark_forced);
			end_frame = i;
			have_line = 0;
		}

		/* Check for empty frame, if we didn't before */
		if (!checked_empty && is_empty(s_info, in_img))
			continue;

		/* Zero transparent pixels, if needed */
		if (must_zero)
			zero_transparent(s_info, in_img);
		must_zero = 0;

		/* Not an empty frame, start line */
		have_line = 1;
		start_frame = i;
		swap_rb(s_info, in_img, out_buf);
		if (buffer_opt)
			n_crop = auto_split(pic, crops, ugly, even_y);
		else if (autocrop)
		{
			crops[0].x = 0;
			crops[0].y = 0;
			crops[0].w = pic.w;
			crops[0].h = pic.h;
			auto_crop(pic, crops);
		}
		if ((buffer_opt || autocrop) && even_y)
			enforce_even_y(crops, n_crop);
		if ((pal_png || sup_output) && pal == NULL)
			pal = palletize(out_buf, s_info->i_width, s_info->i_height);
		if (xml_output)
			for (j = 0; j < n_crop; j++)
				write_png(png_dir, start_frame, (uint8_t *)out_buf, s_info->i_width, s_info->i_height, j, pal, crops[j]);
		if (pal_png && xml_output && !sup_output)
		{
			free(pal);
			pal = NULL;
		}
		num_of_events++;
		if (first_frame == -1)
			first_frame = i;

		/* Save image for next comparison. */
		tmp = in_img;
		in_img = old_img;
		old_img = tmp;
	}

	fprintf(stderr, "\rProgress: %d/%d - Lines: %d - Done\n", i - init_frame, count_frames, num_of_events);

	/* Add last event, if available */
	if (have_line)
	{
		if (sup_output)
		{
			assert(pal != NULL);
			write_sup_wrapper(sw, (uint8_t *)out_buf, n_crop, crops, pal, start_frame + to, i - 1 + to, split_at, min_split, stricter, mark_forced);
			if (!xml_output)
				free(pal);
			pal = NULL;
		}
		if (xml_output)
		{
			add_event_xml(events, split_at, min_split, start_frame + to, i - 1 + to, n_crop, crops, mark_forced);
			free(pal);
			pal = NULL;
		}
		auto_cut = 1;
		end_frame = i - 1;
	}

	if (sup_output)
	{
		close_sup_writer(sw);
	}

	if (xml_output)
	{
		/* Check if we actually have any events */
		if (first_frame == -1)
		{
			if (!allow_empty)
			{
				fprintf(stderr, "No events detected. Cowardly refusing to write XML file.\n");
				return 0;
			}
			else
			{
				first_frame = 0;
				end_frame = 0;
			}
		}

		/* Initialize timecode buffers */
		intc_buf = calloc(12, 1);
		outtc_buf = calloc(12, 1);

		/* Creating output file */
		if ((fh = fopen(xml_output_fn, "w")) == 0)
		{
			perror("Error opening output XML file");
			return 1;
		}

		/* Write XML header */
		mk_timecode(first_frame + to, fps, intc_buf);
		mk_timecode(end_frame + to + auto_cut, fps, outtc_buf);
		fprintf(fh, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			"<BDN Version=\"0.93\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
			"xsi:noNamespaceSchemaLocation=\"BD-03-006-0093b BDN File Format.xsd\">\n"
			"<Description>\n"
			"<Name Title=\"%s\" Content=\"\"/>\n"
			"<Language Code=\"%s\"/>\n"
			"<Format VideoFormat=\"%s\" FrameRate=\"%s\" DropFrame=\"%s\"/>\n"
			"<Events LastEventOutTC=\"%s\" FirstEventInTC=\"%s\"\n", track_name, language, video_format, frame_rate, drop_frame, outtc_buf, intc_buf);

		mk_timecode(0, fps, intc_buf);
		mk_timecode(frames + to, fps, outtc_buf);
		fprintf(fh, "ContentInTC=\"%s\" ContentOutTC=\"%s\" NumberofEvents=\"%d\" Type=\"Graphic\"/>\n"
			"</Description>\n"
			"<Events>\n", intc_buf, outtc_buf, num_of_events);

		/* Write XML events */
		if (!event_list_empty(events))
		{
			event = event_list_first(events);
			do
			{
				mk_timecode(event->start_frame, fps, intc_buf);
				mk_timecode(event->end_frame, fps, outtc_buf);

				if (auto_cut && event->end_frame == frames - 1)
				{
					mk_timecode(event->end_frame + 1, fps, outtc_buf);
				}

				fprintf(fh, "<Event Forced=\"%s\" InTC=\"%s\" OutTC=\"%s\">\n", (event->forced ? "True" : "False"), intc_buf, outtc_buf);
				for (i = 0; i < event->graphics; i++)
				{
					fprintf(fh, "<Graphic Width=\"%d\" Height=\"%d\" X=\"%d\" Y=\"%d\">%08d_%d.png</Graphic>\n", event->c[i].w, event->c[i].h, xo + event->c[i].x, yo + event->c[i].y, event->image_number - to, i);
				}
				fprintf(fh, "</Event>\n");
				event = event_list_next(events);
			}
			while (event != NULL);
		}

		/* Write XML footer */
		fprintf(fh, "</Events>\n</BDN>\n");

		/* Close XML file */
		fclose(fh);
	}

	/* Cleanup */
	close_file_avis(avis_hnd);

	/* Give runtime */
	if (0)
        fprintf(stderr, "Time elapsed: %llu\n", time(NULL) - bench_start);

	return 0;
}

