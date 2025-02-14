#include "avs2sup.h"
#include "common.h"
#include <stdio.h>
#include <stdbool.h>

static ProgressCallback progress_callback = NULL;  // 保存回调函数指针
volatile int stopFlag = 0;

void avs2sup_reg_callback(ProgressCallback callback)
{
    progress_callback = callback;
}

int avs2sup_process(const char* avs_filename, const char* outFileName, const char* language, const char* video_format, const char* frame_rate)
{
    int result = 0;
    if(stopFlag)
    {
        result = 2;
        goto cleanup;
    }
    struct framerate_entry_s framerates[] = { {"23.976", "23.976", 24, 0, 24000, 1001}
        /*, {"23.976d", "23.976", 24000/1001.0, 1}*/
        , {"24", "24", 24, 0, 24, 1}
        , {"25", "25", 25, 0, 25, 1}
        , {"29.97", "29.97", 30, 0, 30000, 1001}
        , {"30", "30", 30, 0, 30, 1}
        /*, {"29.97d", "29.97", 30000/1001.0, 1}*/
        , {"50", "50", 50, 0, 50, 1}
        , {"59.94", "59.94", 60, 0, 60000, 1001}
        , {"60", "60", 60, 0, 60, 1}
        /*, {"59.94d", "59.94", 60000/1001.0, 1}*/
        , {NULL, NULL, 0, 0, 0, 0}
    };
    char *track_name = "Undefined";
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
    int i, j;
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

    /* Both input and output filenames are required */
    if (avs_filename == NULL || outFileName == NULL)
    {
        print_usage();
        result = -1;
        goto cleanup;
    }

    /* Get target output format */
    if (is_extension(outFileName, "xml"))
    {
        xml_output_fn = outFileName;
        xml_output++;
        get_dir_path(xml_output_fn, png_dir);
    }
    else if (is_extension(outFileName, "sup") || is_extension(outFileName, "pgs"))
    {
        sup_output_fn = outFileName;
        sup_output++;
        pal_png = 1;
    }
    else
    {
        fprintf(stderr, "Output file extension must be \".xml\", \".sup\" or \".pgs\".\n");
        result = 1;
        goto cleanup;
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
        if(stopFlag)
        {
            result = 2;
            goto cleanup;
        }
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
        result = 1;
        goto cleanup;
    }

    /* Get timecode offset. */
    to = parse_tc(t_offset, fps);

    /* Get video info and allocate buffer */
    if (open_file_avis(avs_filename, &avis_hnd, s_info))
    {
        print_usage();
        result = 1;
        goto cleanup;
    }
    in_img  = calloc(s_info->i_width * s_info->i_height * 4 + 16 * 2, sizeof(char)); /* allocate + 16 for alignment, and + n * 16 for over read/write */
    old_img = calloc(s_info->i_width * s_info->i_height * 4 + 16 * 2, sizeof(char)); /* see above */
    out_buf = calloc(s_info->i_width * s_info->i_height * 4 + 16 * 2, sizeof(char));

    /* Check minimum size */
    if (s_info->i_width < 8 || s_info->i_height < 8)
    {
        fprintf(stderr, "Error: Video dimensions below 8x8 (%dx%d).\n", s_info->i_width, s_info->i_height);
        result = 1;
        goto cleanup;
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
        result = 0;
        goto cleanup;
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
    for (i = init_frame; i <= last_frame; i++)
    {
        if(stopFlag)
        {
            result = 2;
            goto cleanup;
        }
        if (read_frame_avis(in_img, avis_hnd, i))
        {
            fprintf(stderr, "Error reading frame.\n");
            result = 1;
            goto cleanup;
        }
        checked_empty = 0;

        /* Progress indicator */
        // if (i % (count_frames / progress_step) == 0)
        // {
        //     fprintf(stderr, "\rProgress: %d/%d - Lines: %d", i - init_frame, count_frames, num_of_events);
        // }
        progress_callback(i - init_frame);

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

    if(stopFlag)
    {
        result = 2;
        goto cleanup;
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
                result = 0;
                goto cleanup;
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
            result = 1;
            goto cleanup;
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
        fprintf(stderr, "Time elapsed: %lld\n", time(NULL) - bench_start);

    result = 0;
    goto cleanup;


cleanup:
    stopFlag = 0;
    return result;
}

void avs2sup_stop()
{
    stopFlag = 1;
}
