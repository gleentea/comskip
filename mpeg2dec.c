/*
 * mpeg2dec.c
 * Copyright (C) 2000-2003 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 * See http://libmpeg2.sourceforge.net/ for updates.
 *
 * mpeg2dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpeg2dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifdef _WIN32
#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <conio.h>
#include <excpt.h>
#else
#include <stdio.h>
#endif

#define SELFTEST

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <getopt.h>
#include <fcntl.h>
#include <inttypes.h>
#include "comskip.h"

#ifdef __FreeBSD__
#include <sys/time.h>
#include <sys/stat.h>
#endif

int pass = 0;
double test_pts = 0.0;

#define inline __inline
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/samplefmt.h>

int av_log_level;
#undef AV_TIME_BASE_Q
static AVRational AV_TIME_BASE_Q = {1, AV_TIME_BASE};

#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIOQ_SIZE (5 * 16 * 1024)
#define MAX_VIDEOQ_SIZE (5 * 256 * 1024)
#define AV_SYNC_THRESHOLD 0.01
#define AV_NOSYNC_THRESHOLD 10.0
#define SAMPLE_CORRECTION_PERCENT_MAX 30
#define AUDIO_DIFF_AVG_NB 10
#define FF_ALLOC_EVENT   (SDL_USEREVENT)
#define FF_REFRESH_EVENT (SDL_USEREVENT + 1)
#define FF_QUIT_EVENT (SDL_USEREVENT + 2)
#define VIDEO_PICTURE_QUEUE_SIZE 1
#define DEFAULT_AV_SYNC_TYPE AV_SYNC_ADUIO_MASTER

typedef struct VideoPicture
{
    int width, height; /* source height & width */
    int allocated;
    double pts;
} VideoPicture;

typedef struct VideoState
{
    AVFormatContext *pFormatCtx;
    int             videoStream, audioStream, subtitleStream;
    int             av_sync_type;
    int             seek_req;
    double           seek_pts;
    int             seek_flags;
    __int64          seek_pos;
    double          audio_clock;
    AVStream        *audio_st;
    AVStream        *subtitle_st;
#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000
    DECLARE_ALIGNED(16, uint8_t, audio_buf[(AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2]);
    unsigned int    audio_buf_size;
    unsigned int    audio_buf_index;
    AVPacket        audio_pkt;
    AVPacket        audio_pkt_temp;
    int             audio_hw_buf_size;
    double          audio_diff_cum; /* used for AV difference average computation */
    double          audio_diff_avg_coef;
    double          audio_diff_threshold;
    int             audio_diff_avg_count;
    double          frame_timer;
    double          frame_last_pts;
    double          frame_last_delay;
    double          video_clock; ///<pts of last decoded frame / predicted pts of next decoded frame
    double          video_clock_submitted;
    double          video_current_pts; ///<current displayed pts (different from video_clock if frame fifos are used)
    int64_t         video_current_pts_time;  ///<time (av_gettime) at which we updated video_current_pts - used to have running video pts
    AVStream        *video_st;
    AVFrame         *pFrame;
    char            filename[1024];
    int             quit;
    AVFrame         *frame;
    double			 duration;
    double			 fps;
} VideoState;

VideoState      *is;

enum
{
    AV_SYNC_AUDIO_MASTER,
    AV_SYNC_VIDEO_MASTER,
    AV_SYNC_EXTERNAL_MASTER,
};


/* Since we only have one decoding thread, the Big Struct
   can be global in case we need it. */
VideoState *global_video_state;
AVPacket flush_pkt;

__int64 pev_best_effort_timestamp = 0;

int video_stream_index = -1;
int audio_stream_index = -1;
int width, height;
int have_frame_rate ;
int stream_index;

__int64 best_effort_timestamp;

#define USE_ASF 1


extern int coding_type;

#ifdef _WIN32
#include <sys/timeb.h>
static inline void gettimeofday(struct timeval * tp, void * dummy)
{
    struct _timeb tm;
    _ftime (&tm);
    tp->tv_sec = tm.time;
    tp->tv_usec = tm.millitm * 1000;
}
#endif


static FILE * in_file;
static FILE * sample_file;
static FILE * timing_file = 0;

extern int thread_count;
int is_AC3;
int AC3_rate;
int AC3_mode;
int is_h264=0;
int is_AAC=0;
extern unsigned int AC3_sampling_rate; //AC3
extern int AC3_byterate;
int demux_pid=0;
int demux_asf=0;
int last_pid;
#define PIDS	100
#define PID_MASK	0x1fff
int pids[PIDS];
int pid_type[PIDS];
int pid_pcr[PIDS];
int pid_pid[PIDS];
int top_pid_count[PID_MASK+1];
int top_pid_pid;
int pid;
int selected_video_pid=0;
int selected_audio_pid=0;
int selected_subtitle_pid=0;
int selection_restart_count = 0;
int found_pids=0;

__int64 pts;
__int64 initial_pts;
__int64 final_pts;
double pts_offset = 0.0;

int initial_pts_set = 0;
double initial_apts;
int initial_apts_set = 0;

//int bitrate;
int muxrate,byterate=10000;
#define PTS_FRAME (double)(1.0 / get_fps())
#define SAMPLE_TO_FRAME 2.8125
#define   FSEEK    _fseeki64
#define   FTELL    _ftelli64

// The following two functions are undocumented and not included in any public header,
// so we need to declare them ourselves
extern int  _fseeki64(FILE *, __int64, int);
extern __int64 _ftelli64(FILE *);

int soft_seeking=0;
extern char	basename[];

char field_t;

char tempstring[512];


FILE *myfopen(const char * f, char * m);

#define DUMP_OPEN if (output_timing) { sprintf(tempstring, "%s.timing.csv", basename); timing_file = myfopen(tempstring, "w"); DUMP_HEADER }
#define DUMP_HEADER if (timing_file) fprintf(timing_file, "type   ,dts         ,pts         ,clock       ,delta       ,offset\n");
#define DUMP_TIMING(T, D, P, C) if (timing_file && !csStepping && !csJumping && !csStartJump) fprintf(timing_file, "%7s, %12.3f,%12.3f, %12.3f, %12.3f, %12.3f,\n", \
                                                                                                      T, (double) (D)/frame_delay, (double) (P)/frame_delay, (double) (C)/frame_delay, ((double) (P) - (double) (C))/frame_delay, pts_offset/frame_delay );
#define DUMP_CLOSE if (timing_file) { fclose(timing_file); timing_file = NULL; }


extern int skip_B_frames;
extern int lowres;

static int sigint = 0;

static int verbose = 0;

extern int selftest;

#ifdef _DEBUG
static int dump_seek = 1;		// Set to 1 to dump the seeking process
#else
static int dump_seek = 0;		// Set to 1 to dump the seeking process
#endif

#ifdef _WIN32
struct _stati64 instat;
#define filesize instat.st_size
#else
struct stat instat;
#define filesize instat.st_size
#endif

extern int frame_count;
int	framenum;

fpos_t		filepos;
fpos_t		fileendpos;
extern int		standoff;
__int64			goppos,infopos,packpos,ptspos,headerpos,frompos,SeekPos;

extern int max_repair_size;
extern int variable_bitrate;
int max_internal_repair_size = 40;
int reviewing = 0;
int count=0;
int currentSecond=0;
int cur_hour = 0;
int cur_minute = 0;
int cur_second = 0;

extern char HomeDir[256];
extern int processCC;
int	reorderCC = 0;

extern int live_tv;
int csRestart;
int csStartJump;
int csStepping;
int csJumping;
int csFound;
int	seekIter = 0;
int	seekDirection = 0;

extern FILE * out_file;
extern uint8_t ccData[500];
extern int ccDataLen;
static uint8_t				prevccData[500];
static int					prevccDataLen;

char				prevfield_t = 0;

extern int height,width, videowidth;
extern int output_debugwindow;
extern int output_console;
extern int output_timing;
extern int output_srt;
extern int output_smi;

extern unsigned char *g_frame_ptr;
extern int lastFrameWasSceneChange;
extern int live_tv_retries;
extern int dvrms_live_tv_retries;
int retries;

static unsigned int frame_period;
extern void set_fps(unsigned int frame_period);
extern void dump_video (char *start, char *end);
extern void dump_audio (char *start, char *end);
extern void	Debug(int level, char* fmt, ...);
extern void dump_video_start(void);
extern void dump_audio_start(void);

void file_open();
int DetectCommercials(int, double);
int BuildMasterCommList(void);
FILE* LoadSettings(int argc, char ** argv);
void ProcessCCData(void);
void dump_data(char *start, int length);


static void signal_handler (int sig)
{
    sigint = 1;
    signal (sig, SIG_DFL);
    //return (RETSIGTYPE)0;
    return;
}



#define AUDIOBUFFER	800000

static double base_apts, apts;
static DECLARE_ALIGNED(16, short, audio_buffer[AUDIOBUFFER]);
static short *audio_buffer_ptr = audio_buffer;
static int audio_samples = 0;


static int sound_frame_counter = 0;
extern double get_fps();
extern int get_samplerate();
extern int get_channels();
extern void add_volumes(int *volumes, int nr_frames);
extern void set_frame_volume(uint32_t framenr, int volume);

static int max_volume_found = 0;

int ms_audio_delay = 5;
int tracks_without_sound = 0;
int frames_without_sound = 0;
#define MAX_FRAMES_WITHOUT_SOUND	100
int frames_with_loud_sound = 0;

void sound_to_frames(VideoState *is, short *b, int s, int format)
{
    int i,n;
    int volume;
    int delta = 0;
    int s_per_frame;
    short *buffer;
    float *fb;


    audio_samples = audio_buffer_ptr - audio_buffer;

    n = is->audio_st->codec->channels * is->audio_st->codec->sample_rate;
    base_apts = (is->audio_clock - ((double)audio_samples /(double)(n)));

    s_per_frame = (int) ((double)(is->audio_st->codec->sample_rate) * (double) is->audio_st->codec->channels / get_fps());
    if (s_per_frame == 0)
        return;


    if (format == AV_SAMPLE_FMT_FLTP)
    {
        fb = (float*)b;
        if (s > 0)
        {
            for (i = 0; i < s; i++)
            {
                *audio_buffer_ptr++ = (short) ((*fb++) * 64000);
                if ((LONG_PTR) audio_buffer_ptr > (LONG_PTR) &audio_buffer[AUDIOBUFFER-10])
                    break;
            }
        }
    }
    else
    {
        if (s > 0)
        {
            for (i = 0; i < s; i++)
            {
                *audio_buffer_ptr++ = *b++;
            }
        }

    }



    audio_samples = audio_buffer_ptr - audio_buffer;

    buffer = audio_buffer;
    while (audio_samples >= s_per_frame)
    {
        if (sample_file) fprintf(sample_file, "Frame %i\n", sound_frame_counter);
        volume = 0;
        for (i = 0; i < s_per_frame; i++)
        {
            if (sample_file) fprintf(sample_file, "%i\n", *buffer);
            volume += (*buffer>0 ? *buffer : - *buffer);
            buffer++;
        }
        volume = volume/s_per_frame;
        audio_samples -= s_per_frame;


        if (sound_frame_counter == 8)
            sound_frame_counter = sound_frame_counter;

        if (volume == 0)
        {
            frames_without_sound++;
        }
        else if (volume > 20000)
        {
            if (volume > 256000)
                volume = 220000;
            frames_with_loud_sound++;
            volume = -1;
        }
        else
        {
            frames_without_sound = 0;
        }

        if (max_volume_found < volume)
            max_volume_found = volume;

        apts = base_apts;
        delta = (base_apts - is->video_clock) * get_fps();

        //		delta = (__int64)(apts/PTS_FRAME) - (__int64)(pts/PTS_FRAME);

        if (-max_internal_repair_size < delta && delta < max_internal_repair_size && abs( sound_frame_counter - delta - framenum) > 20 )
        {
            Debug(1, "Audio PTS jumped %d frames at frame %d\n", -sound_frame_counter + delta + framenum, framenum);
            sound_frame_counter = delta + framenum;
        }
        //		  DUMP_TIMING("a frame", apts, is->video_clock);
        set_frame_volume((demux_asf?ms_audio_delay:0)+sound_frame_counter++, volume);

        base_apts += (double)s_per_frame/(double)n;
    }
    audio_buffer_ptr = audio_buffer;
    if (audio_samples > 0)
    {
        for (i = 0; i < audio_samples; i++)
        {
            *audio_buffer_ptr++ = *buffer++;
        }
    }
}

#define STORAGE_SIZE 1000000

static short storage_buf[STORAGE_SIZE];


void audio_packet_process(VideoState *is, AVPacket *pkt)
{
    int prev_codec_id = -1;
    double frame_delay = 1.0;
    int len1, data_size, n;
    int dec_channel_layout;
    AVPacket *pkt_temp = &is->audio_pkt_temp;
    double pts;
    int got_frame;
    if (!reviewing)
    {
        dump_audio_start();
        dump_audio((char *)pkt->data,(char *) (pkt->data + pkt->size));
    }

    pkt_temp->data = pkt->data;
    pkt_temp->size = pkt->size;
    /*  Try to align on packet boundary as some demuxers don't do that, in particular dvr-ms
        if (is->audio_st->codec->codec_id == CODEC_ID_MP1) {
        while (pkt_temp->size > 2 && (pkt_temp->data[0] != 0xff || pkt_temp->data[1] != 0xe0) ) {
        pkt_temp->data++;
        pkt_temp->size--;
        }
        }
    */
    n = (is->audio_st->codec->bits_per_coded_sample / 8) * is->audio_st->codec->channels * is->audio_st->codec->sample_rate;

#ifdef OLD_AUDIO_PTS_HANDLING

    /* if update, update the audio clock w/pts */
    if(pkt->pts == AV_NOPTS_VALUE)
    {
        pts = 0.0;
    }
    else
    {
        pts = av_q2d(is->audio_st->time_base)* ( pkt->pts -  (is->video_st->start_time != AV_NOPTS_VALUE ? is->video_st->start_time : 0));
    }
    pts = pts + pts_offset;

    //	Debug(0 ,"apst[%3d] = %12.3f\n", framenum, pts);
    if (pts != 0)
    {
        if (is->audio_clock  != 0)
        {
            if ((pts - is->audio_clock) < -0.999  || (pts - is->audio_clock) > max_repair_size/get_fps() )
            {
                Debug(1 ,"Audio jumped by %6.3f at frame %d\n", (pts - is->audio_clock), framenum);
                //                    DUMP_TIMING("a   set", pts, is->audio_clock);
                is->audio_clock = pts;

            }
            else
            {
                //                    DUMP_TIMING("a  free", pts, is->audio_clock);
                // Do nothing
            }
        }
        else
        {
            //               DUMP_TIMING("ajmpset", pts, is->audio_clock);
            is->audio_clock = pts;
        }
    }
    else
    {
        /* if we aren't given a pts, set it to the clock */
        //          DUMP_TIMING("a  tick", pts, is->audio_clock);
        pts = is->audio_clock;
    }
#else
    if (pkt->pts != AV_NOPTS_VALUE)
    {
        is->audio_clock = av_q2d(is->audio_st->time_base)*( pkt->pts -  (is->video_st->start_time != AV_NOPTS_VALUE ? is->video_st->start_time : 0));
    }

#endif


    //		fprintf(stderr, "sac = %f\n", is->audio_clock);
    while(pkt_temp->size > 0)
    {
        data_size = STORAGE_SIZE;

        if (!is->frame)
        {
            if (!(is->frame = avcodec_alloc_frame()))
                return;
        }
        else
            avcodec_get_frame_defaults(is->frame);

        len1 = avcodec_decode_audio4(is->audio_st->codec, is->frame, &got_frame, pkt_temp);
        if (prev_codec_id != -1 && prev_codec_id != is->audio_st->codec->codec_id)
        {
            Debug(2 ,"Audio format change\n");
        }
        prev_codec_id = is->audio_st->codec->codec_id;
        if (len1 < 0)
        {
            /* if error, we skip the frame */
            pkt_temp->size = 0;
            break;
        }

        pkt_temp->data += len1;
        pkt_temp->size -= len1;
        if (!got_frame)
        {
            /* stop sending empty packets if the decoder is finished */
            continue;
        }

        data_size = av_samples_get_buffer_size(NULL, is->frame->channels,
                                               is->frame->nb_samples,
                                               is->frame->format, 1);
        dec_channel_layout =
            (is->frame->channel_layout && is->frame->channels == av_get_channel_layout_nb_channels(is->frame->channel_layout)) ?
            is->frame->channel_layout : av_get_default_channel_layout(is->frame->channels);
        //		fprintf(stderr, "Audio %f\n", is->audio_clock);
        if (data_size > 0)
        {
            sound_to_frames(is, (short *)is->frame->data[0], data_size / av_get_bytes_per_sample(is->frame->format), is->frame->format);
        }

        is->audio_clock += (double)data_size /
            (is->frame->channels * is->frame->sample_rate * av_get_bytes_per_sample(is->frame->format));
    }
}

static void print_fps (int final)
{
    static uint32_t frame_counter = 0;
    static struct timeval tv_beg, tv_start;
    static int total_elapsed;
    static int last_count = 0;
    struct timeval tv_end;
    double fps, tfps;
    int frames, elapsed;
    char cur_pos[100] = "0:00:00";

    if (verbose)
        return;

    if(csStepping)
        return;

    if(final < 0)
    {
        frame_counter = 0;
        last_count = 0;
        return;
    }
again:
    gettimeofday (&tv_end, NULL);

    if (!frame_counter)
    {
        tv_start = tv_beg = tv_end;
        signal (SIGINT, signal_handler);
    }

    elapsed = (tv_end.tv_sec - tv_beg.tv_sec) * 100 + (tv_end.tv_usec - tv_beg.tv_usec) / 10000;
    total_elapsed = (tv_end.tv_sec - tv_start.tv_sec) * 100 + (tv_end.tv_usec - tv_start.tv_usec) / 10000;

    if (final)
    {
        if (total_elapsed)
            tfps = frame_counter * 100.0 / total_elapsed;
        else
            tfps = 0;

        fprintf (stderr,"\n%d frames decoded in %.2f seconds (%.2f fps)\n",
                 frame_counter, total_elapsed / 100.0, tfps);
        fflush(stderr);
        return;
    }

    frame_counter++;

    frames = frame_counter - last_count;

#ifdef DONATOR
#else
#ifndef DEBUG
    if (is_h264 && frames > 15 &&  elapsed < 100)
    {
        Sleep((DWORD)100);
        goto again;
    }
#endif
#endif

    if (elapsed < 100)	/* only display every 1.00 seconds */
        return;

    tv_beg = tv_end;

    cur_second = (int)((double)framenum / get_fps());
    cur_hour = cur_second / (60 * 60);
    cur_second -= cur_hour * 60 * 60;
    cur_minute = cur_second / 60;
    cur_second -= cur_minute * 60;


    sprintf(cur_pos, "%2i:%.2i:%.2i", cur_hour, cur_minute, cur_second);

    fps = frames * 100.0 / elapsed;
    tfps = frame_counter * 100.0 / total_elapsed;

    fprintf (stderr, "%s - %d frames in %.2f sec(%.2f fps), "
             "%.2f sec(%.2f fps), %d%%\r", cur_pos, frame_counter,
             total_elapsed / 100.0, tfps, elapsed / 100.0, fps, (int) (100.0 * ((double)(framenum) / get_fps()) / global_video_state->duration));
    fflush(stderr);
    last_count = frame_counter;
}



char field_t;
void SetField(char t)
{
    field_t = t;
}


static void ResetInputFile()
{
    global_video_state->seek_req = true;
    global_video_state->seek_pos = 0;
    global_video_state->seek_pts = 0.0;
    pev_best_effort_timestamp = 0;

#ifdef PROCESS_CC
    if (output_srt || output_smi) CEW_reinit();
#endif
    framenum = 0;
    sound_frame_counter = 0;
    initial_pts = 0;
    initial_pts_set = 0;
    initial_apts_set = 0;
    initial_apts = 0;
    audio_samples = 0;
    best_effort_timestamp = 0;
}


int SubmitFrame(AVStream        *video_st, AVFrame         *pFrame , double pts)
{
    int res=0;
    int changed = 0;
    AVCodecContext  *pCodecCtx = video_st->codec;

    //	bitrate = pFrame->bit_rate;
    if (pFrame->linesize[0] > 2000 || pFrame->height > 1200 || pFrame->linesize[0] < 100 || pFrame->height < 100)
    {
        //				printf("Panic: illegal height, width or frame period\n");
        g_frame_ptr = NULL;
        return(0);
    }
    if (height != pFrame->height && pFrame->height > 100 && pFrame->height < 2000)
    {
        height= pFrame->height;
        changed = 1;
    }
    if (width != pFrame->linesize[0] && pFrame->linesize[0] > 100 && pFrame->linesize[0]  < 2000)
    {
        width= pFrame->linesize[0];
        changed = 1;
    }
    if (videowidth != pFrame->width && pFrame->width > 100 && pFrame->width < 2000)
    {
        videowidth= pFrame->width;
        changed = 1;
    }
    if (changed) Debug(2, "Format changed to [%d : %d]\n", videowidth, height);
    infopos = headerpos;
    g_frame_ptr = pFrame->data[0];
    if (g_frame_ptr == NULL)
    {
        return(0);; // return; // exit(2);
    }

    if (pFrame->pict_type == AV_PICTURE_TYPE_B)
        SetField('B');
    else if (pFrame->pict_type == AV_PICTURE_TYPE_I)
        SetField('I');
    else
        SetField('P');


    if (framenum == 0 && pass == 0 && test_pts == 0.0)
        test_pts = pts;
    if (framenum == 0 && pass > 0 && test_pts != pts)
        Debug(1,"Reset File Failed, initial pts = %6.3f, seek pts = %6.3f, pass = %d\n", test_pts, pts, pass+1);
    if (framenum == 0)
        pass++;
#ifdef SELFTEST
    if (selftest == 2 && framenum > 20)
    {
        if (pass > 1)
            exit(1);
        ResetInputFile();
    }
#endif


    if (!reviewing)
    {

        print_fps (0);

        if (res == 0)
            res = DetectCommercials((int)framenum, pts);
        framenum++;
    }
    return (res);
}

Set_seek(VideoState *is, double pts, double length)
{
    AVFormatContext *ic = is->pFormatCtx;

    is->seek_flags = AVSEEK_FLAG_ANY;
    is->seek_flags = AVSEEK_FLAG_BACKWARD;
    is->seek_req   = true;
    if (is->duration <= 0)
    {
        uint64_t size =  avio_size(ic->pb);
        pts = size*pts/length;
        is->seek_flags |= AVSEEK_FLAG_BYTE;
    }
    is->seek_pos = (pts - 1.6 < 0.0 ? 0.0 : pts - 1.6) / av_q2d(is->video_st->time_base);
    if (is->video_st->start_time != AV_NOPTS_VALUE)
    {
        is->seek_pos += is->video_st->start_time;
    }
    is->seek_pts = pts;
}



void DecodeOnePicture(FILE * f, double pts, double length)
{
    double frame_delay = 1.0;
    VideoState *is = global_video_state;
    AVPacket *packet;
    int ret;
    int single_frame=0;
    int seek_flags = 0;
    int count = 0;

    int64_t pack_pts=0, comp_pts=0, pack_duration=0;

    file_open();
    is = global_video_state;

    reviewing = 1;

    Set_seek(is, pts, length);

    pev_best_effort_timestamp = 0;
    best_effort_timestamp = 0;
    pts_offset = 0.0;

    //     Debug ( 5,  "Seek to %f\n", pts);
    g_frame_ptr = NULL;
    packet = &(is->audio_pkt);

    for(;;)
    {
        if(is->quit)
        {
            break;
        }
        // seek stuff goes here
        if(is->seek_req)
        {
            ret = avformat_seek_file(is->pFormatCtx, is->videoStream, INT64_MIN, is->seek_pos, INT64_MAX, is->seek_flags);
            pev_best_effort_timestamp = 0;
            best_effort_timestamp = 0;
            is->video_clock = 0.0;
            is->audio_clock = 0.0;
            if(ret < 0)
            {
                if (is->pFormatCtx->iformat->read_seek)
                {
                    printf("format specific\n");
                }
                else if(is->pFormatCtx->iformat->read_timestamp)
                {
                    printf("frame_binary\n");
                }
                else
                {
                    printf("generic\n");
                }

                fprintf(stderr, "%s: error while seeking. target: %6.3f, stream_index: %d\n", is->pFormatCtx->filename, pts, stream_index);
            }
            if(is->audioStream >= 0)
            {
                avcodec_flush_buffers(is->audio_st->codec);
            }
            if(is->videoStream >= 0)
            {
                avcodec_flush_buffers(is->video_st->codec);
            }
            is->seek_req = 0;
        }

        if(av_read_frame(is->pFormatCtx, packet) < 0)
        {
            break;
        }
        if(packet->stream_index == is->videoStream)
        {
            if (packet->pts != AV_NOPTS_VALUE)
                comp_pts = packet->pts;
            pack_pts = comp_pts; // av_rescale_q(comp_pts, is->video_st->time_base, AV_TIME_BASE_Q);
            pack_duration = packet->duration; //av_rescale_q(packet->duration, is->video_st->time_base, AV_TIME_BASE_Q);
            comp_pts += packet->duration;
            pass = 0;
            retries = 1;
            if (video_packet_process(is, packet) )
            {
                if (retries == 0)
                {
                    av_free_packet(packet);
                    break;
                }
                /*
                  double frame_delay = av_q2d(is->video_st->codec->time_base)* is->video_st->codec->ticks_per_frame;         // <------------------------ frame delay is the time in seconds till the next frame
                  if (is->video_clock - is->seek_pts > -frame_delay / 2.0)
                  {
                  av_free_packet(packet);
                  break;
                  }
                  if (is->video_clock + (pack_duration * av_q2d(is->video_st->time_base)) >= is->seek_pts)
                  {
                  av_free_packet(packet);
                  break;
                  }
                */
            }
        }
        else if(packet->stream_index == is->audioStream)
        {
            // audio_packet_process(is, packet);
        }
        else
        {
            // Do nothing
        }
        av_free_packet(packet);
    }
    reviewing = 0;
}

void raise_exception(void)
{
    *(int *)0 = 0;
}



int filter(void)
{
    printf("Exception raised, Comskip is terminating\n");
    exit(99);
}


// #include "libavformat/avformat.h"


extern char					mpegfilename[];

int video_packet_process(VideoState *is,AVPacket *packet)
{
    double frame_delay;
    int len1, frameFinished;
    double pts, dts;
    double real_pts;
    static int summed_repeat = 0;
    double calculated_delay;

    if (!reviewing)
    {
        dump_video_start();
        dump_video((char *)packet->data,(char *) (packet->data + packet->size));
    }
    real_pts = 0.0;
    pts = 0;
    //        is->video_st->codec->flags |= CODEC_FLAG_GRAY;
    // Decode video frame
    len1 = avcodec_decode_video2(is->video_st->codec, is->pFrame, &frameFinished,
                                 packet);

    if (len1<0)
    {
        if (len1 == -1 && thread_count > 1)
        {
            InitComSkip();
            thread_count = 1;
            is->seek_req = 1;
            is->seek_pos = 0;
            pev_best_effort_timestamp = 0;
            best_effort_timestamp = 0;
            Debug(1 ,"Restarting processing in single thread mode because frame size is changing \n");
            goto quit;
        }
    }

    // Did we get a video frame?
    if(frameFinished)
    {


        if (is->video_st->codec->ticks_per_frame<1)
            is->video_st->codec->ticks_per_frame = 1;
        frame_delay = av_q2d(is->video_st->codec->time_base)* is->video_st->codec->ticks_per_frame;         // <------------------------ frame delay is the time in seconds till the next frame

        pev_best_effort_timestamp = best_effort_timestamp;

        //          best_effort_timestamp = *(__int64 *)av_opt_ptr(avcodec_get_frame_class(), is->pFrame, "best_effort_timestamp");
        best_effort_timestamp = av_frame_get_best_effort_timestamp(is->pFrame);
        //            best_effort_timestamp = is->pFrame->pkt_pts;

        calculated_delay = (best_effort_timestamp - pev_best_effort_timestamp) * av_q2d(is->video_st->time_base);

        if (pev_best_effort_timestamp != 0 && fabs(calculated_delay - frame_delay) > 4)
            fprintf(stderr, "Strange pts step: %f frames\n", (calculated_delay - frame_delay) / frame_delay );
#ifndef OLD_PTS_CALC
        if (best_effort_timestamp == AV_NOPTS_VALUE)
            real_pts = 0;
        else
        {
            headerpos = avio_tell(is->pFormatCtx->pb);
            if (initial_pts_set == 0)
            {
                initial_pts = best_effort_timestamp - (is->video_st->start_time != AV_NOPTS_VALUE ? is->video_st->start_time : 0);
                initial_pts_set = 1;
                final_pts = 0;
                pts_offset = 0.0;

            }
            real_pts = av_q2d(is->video_st->time_base)* ( best_effort_timestamp - (is->video_st->start_time != AV_NOPTS_VALUE ? is->video_st->start_time : 0)) ;
            final_pts = best_effort_timestamp -  (is->video_st->start_time != AV_NOPTS_VALUE ? is->video_st->start_time : 0);
        }
        dts =  av_q2d(is->video_st->time_base)* ( is->pFrame->pkt_dts - (is->video_st->start_time != AV_NOPTS_VALUE ? is->video_st->start_time : 0)) ;

        //		Debug(0 ,"pst[%3d] = %12.3f, inter = %d, rep = %d, ticks = %d\n", framenum, pts/frame_delay, is->pFrame->interlaced_frame, is->pFrame->repeat_pict, is->video_st->codec->ticks_per_frame);

        pts = real_pts + pts_offset;
        if(pts != 0)
        {
            /* if we have pts, set video clock to it */
            if (is->video_clock != 0)
            {
                if ((pts - is->video_clock)/frame_delay < -10 || (pts - is->video_clock)/frame_delay > max_repair_size )
                {
                    if (!reviewing) Debug(1 ,"Video jumped by %6.1f at frame %d, inter = %d, rep = %d, ticks = %d\n",
                                          (pts - is->video_clock)/frame_delay, framenum, is->pFrame->interlaced_frame, is->pFrame->repeat_pict, is->video_st->codec->ticks_per_frame);
                    // Calculate offset for jump
                    pts_offset = is->video_clock - real_pts/* set to mid of free window */;
                    pts = real_pts + pts_offset;
                    is->video_clock = pts;
                    DUMP_TIMING("v   set",real_pts, pts, is->video_clock);
                }
                else if ((pts - is->video_clock)/frame_delay > 0.99 )
                {
                    if (!reviewing) Debug(1 ,"Video jumped by %6.1f frames at frame %d, repairing timeline\n",
                                          (pts - is->video_clock)/frame_delay, framenum);
                    summed_repeat += is->video_st->codec->ticks_per_frame *(int) ((pts - is->video_clock)/frame_delay );
                    //      is->video_clock = pts;
                    DUMP_TIMING("vfollow", real_pts, pts, is->video_clock);
                }
                else
                {
                    DUMP_TIMING("v  free",real_pts,  pts, is->video_clock);
                    // Do nothing
                }
            }
            else
            {
                is->video_clock = pts; // - (5.0 / 2.0) * frame_delay;
                DUMP_TIMING("v  init", real_pts, pts, is->video_clock);
            }
        }
        else
        {
            /* if we aren't given a pts, set it to the clock */
            DUMP_TIMING("v clock", real_pts, pts, is->video_clock);
            pts = is->video_clock;
        }

#else
        if (best_effort_timestamp != AV_NOPTS_VALUE)
            real_pts = av_q2d(is->video_st->time_base)* ( best_effort_timestamp - (is->video_st->start_time != AV_NOPTS_VALUE ? is->video_st->start_time : 0)) ;

#endif



        frame_period = (double)900000*30 / (((double)is->video_st->codec->time_base.den) / is->video_st->codec->time_base.num );
        if (is->video_st->codec->ticks_per_frame >= 1)
            frame_period *= is->video_st->codec->ticks_per_frame;
        else
            is->video_st->codec->ticks_per_frame = 1;

        set_fps(frame_period);

        is->video_clock_submitted = is->video_clock;
        if (is->video_clock - is->seek_pts > -frame_delay / 2.0)
        {
            retries = 0;
            if (SubmitFrame (is->video_st, is->pFrame, is->video_clock))
            {
                is->seek_req = 1;
                is->seek_pos = 0;
                goto quit;
            }
            //            if (selftest == 4) exit(1);
        }
        /* update the video clock */
        is->video_clock += frame_delay;

        summed_repeat += is->pFrame->repeat_pict;
        while (summed_repeat >= is->video_st->codec->ticks_per_frame)
        {
            DUMP_TIMING("vrepeat", dts, pts, is->video_clock);
            is->video_clock_submitted = is->video_clock;
            if (is->video_clock - is->seek_pts > -frame_delay / 2.0)
            {
                retries = 0;
                if (SubmitFrame (is->video_st, is->pFrame, is->video_clock))
                {
                    is->seek_req = 1;
                    is->seek_pos = 0;
                    goto quit;
                }
            }
            /* update the video clock */
            is->video_clock += frame_delay;
            summed_repeat -= is->video_st->codec->ticks_per_frame;
        }
        return 1;
    }
quit:
    return 0;
}



int stream_component_open(VideoState *is, int stream_index)
{

    AVFormatContext *pFormatCtx = is->pFormatCtx;
    AVCodecContext *codecCtx;
    AVCodec *codec;
    AVDictionary *opts;


    if(stream_index < 0 || stream_index >= pFormatCtx->nb_streams)
    {
        return -1;
    }

    if (strcmp(pFormatCtx->iformat->name, "mpegts")==0)
        demux_pid = 1;

    // Get a pointer to the codec context for the video stream
    codecCtx = pFormatCtx->streams[stream_index]->codec;
    /*

      if (codecCtx->codec_type == AVMEDIA_TYPE_AUDIO) {
      if (codecCtx->channels > 0) {
      codecCtx->request_channels = FFMIN(2, codecCtx->channels);
      } else {
      codecCtx->request_channels = 2;
      }
      }
    */

    if (codecCtx->codec_type == AVMEDIA_TYPE_VIDEO)
    {

        codecCtx->flags |= CODEC_FLAG_GRAY;
        if (codecCtx->codec_id == CODEC_ID_H264)
#ifdef DONATOR
            is_h264 = 1;
#else
        {

            is_h264 = 1;
            {
                Debug(0, "h.264 video can only be processed at full speed by the Donator version\n");
            }
        }
#endif
        else
        {
#ifdef DONATOR
            codecCtx->lowres = lowres;
#endif
            //            /* if(lowres) */ codecCtx->flags |= CODEC_FLAG_EMU_EDGE;
        }
        //        codecCtx->flags2 |= CODEC_FLAG2_FAST;
        if (codecCtx->codec_id != CODEC_ID_MPEG1VIDEO)
#ifdef DONATOR
            codecCtx->thread_count= thread_count;
#else
        codecCtx->thread_count= 1;
#endif

    }


    codec = avcodec_find_decoder(codecCtx->codec_id);

    if(!codec || (avcodec_open2(codecCtx, codec, NULL) < 0))
    {
        fprintf(stderr, "Unsupported codec!\n");
        return -1;
    }

    switch(codecCtx->codec_type)
    {
    case AVMEDIA_TYPE_SUBTITLE:
        is->subtitleStream = stream_index;
        is->subtitle_st = pFormatCtx->streams[stream_index];
        if (demux_pid)
            selected_subtitle_pid = is->subtitle_st->id;
        break;
    case AVMEDIA_TYPE_AUDIO:
        is->audioStream = stream_index;
        is->audio_st = pFormatCtx->streams[stream_index];
        //          is->audio_buf_size = 0;
        //          is->audio_buf_index = 0;

        /* averaging filter for audio sync */
        //          is->audio_diff_avg_coef = exp(log(0.01 / AUDIO_DIFF_AVG_NB));
        //          is->audio_diff_avg_count = 0;
        /* Correct audio only if larger error than this */
        //          is->audio_diff_threshold = 2.0 * SDL_AUDIO_BUFFER_SIZE / codecCtx->sample_rate;
        if (demux_pid)
            selected_audio_pid = is->audio_st->id;


        memset(&is->audio_pkt, 0, sizeof(is->audio_pkt));
        break;
    case AVMEDIA_TYPE_VIDEO:
        is->videoStream = stream_index;
        is->video_st = pFormatCtx->streams[stream_index];

        //          is->frame_timer = (double)av_gettime() / 1000000.0;
        //          is->frame_last_delay = 40e-3;
        //          is->video_current_pts_time = av_gettime();

        is->pFrame = avcodec_alloc_frame();
        codecCtx->flags |= CODEC_FLAG_GRAY;
        if (codecCtx->codec_id == CODEC_ID_H264)
        {
#ifdef DONATOR
            is_h264 = 1;
#else
            is_h264 = 1;
            {
                Debug(0, "h.264 video can only be processed at full speed by the Donator version\n");
            }
#endif
        }
#ifdef DONATOR
        else
        {
            if (codecCtx->codec_id != CODEC_ID_MPEG1VIDEO)
                codecCtx->lowres = lowres;
        }
#endif


        //        codecCtx->flags2 |= CODEC_FLAG2_FAST;
        if (codecCtx->codec_id != CODEC_ID_MPEG1VIDEO)
#ifdef DONATOR
            codecCtx->thread_count= thread_count;
#else
        codecCtx->thread_count= 1;
#endif
        if (codecCtx->codec_id == CODEC_ID_MPEG1VIDEO)
            is->video_st->codec->ticks_per_frame = 1;
        if (demux_pid)
            selected_video_pid = is->video_st->id;
        /*
          MPEG
          if(  (codecCtx->skip_frame >= AVDISCARD_NONREF && s2->pict_type==FF_B_TYPE)
          ||(codecCtx->skip_frame >= AVDISCARD_NONKEY && s2->pict_type!=FF_I_TYPE)
          || codecCtx->skip_frame >= AVDISCARD_ALL)


          if(  (s->avctx->skip_idct >= AVDISCARD_NONREF && s->pict_type == FF_B_TYPE)
          ||(codecCtx->skip_idct >= AVDISCARD_NONKEY && s->pict_type != FF_I_TYPE)
          || s->avctx->skip_idct >= AVDISCARD_ALL)
          h.264
          if(   s->codecCtx->skip_loop_filter >= AVDISCARD_ALL
          ||(s->codecCtx->skip_loop_filter >= AVDISCARD_NONKEY && h->slice_type_nos != FF_I_TYPE)
          ||(s->codecCtx->skip_loop_filter >= AVDISCARD_BIDIR  && h->slice_type_nos == FF_B_TYPE)
          ||(s->codecCtx->skip_loop_filter >= AVDISCARD_NONREF && h->nal_ref_idc == 0))

          Both
          if(  (codecCtx->skip_frame >= AVDISCARD_NONREF && s2->pict_type==FF_B_TYPE)
          ||(codecCtx->skip_frame >= AVDISCARD_NONKEY && s2->pict_type!=FF_I_TYPE)
          || codecCtx->skip_frame >= AVDISCARD_ALL)
          break;

        */
        if (skip_B_frames)
            codecCtx->skip_frame = AVDISCARD_NONREF;
        //          codecCtx->skip_loop_filter = AVDISCARD_NONKEY;
        //           codecCtx->skip_idct = AVDISCARD_NONKEY;

        break;
    default:
        break;
    }

    return(0);
}

void file_open()
{
    VideoState *is;
    AVFormatContext *pFormatCtx;
    int subtitle_index= -1, audio_index= -1, video_index = -1;
    int i;
    int retries = 0;

    if (global_video_state == NULL)
    {


        is = av_mallocz(sizeof(VideoState));

        memset(&is->audio_pkt, 0, sizeof(is->audio_pkt));


        strcpy(is->filename, mpegfilename);
        // Register all formats and codecs
        av_register_all();

        global_video_state = is;


        is->videoStream=-1;
        is->audioStream=-1;
        is->subtitleStream = -1;
        is->pFormatCtx = NULL;
    }
    else
        is = global_video_state;
    // will interrupt blocking functions if we quit!

    //	 avformat_network_init();

    // Open video file

    if (     is->pFormatCtx == NULL)
    {
        pFormatCtx = avformat_alloc_context();
        //        pFormatCtx->max_analyze_duration *= 40;
        //        pFormatCtx->probesize = 40000000;
    again:
        if(avformat_open_input(&pFormatCtx, is->filename, NULL, NULL)!=0)
        {
            fprintf(stderr, "%s: Can not open file\n", is->filename);
            if (retries++ < 2)
            {
                Sleep(1000L);
                goto again;
            }
            exit(-1);

        }

        is->pFormatCtx = pFormatCtx;

        //     pFormatCtx->max_analyze_duration = 320000000;

        //    pFormatCtx->thread_count= 2;

        // Retrieve stream information
        if(av_find_stream_info(is->pFormatCtx)<0)
        {
            fprintf(stderr, "%s: Can not find stream info\n", is->filename);
            exit(-1);
        }

        // Dump information about file onto standard error
        av_dump_format(is->pFormatCtx, 0, is->filename, 0);

        // Find the first video stream
    }
#ifndef DONATOR

    if (strcmp(is->pFormatCtx->iformat->name,"wtv")==0)
    {
        Debug(0, "WTV files can only be processed by the Donator version\n");
        exit(-1);
    }
#endif

    if (     is->videoStream == -1)
    {
        video_index = av_find_best_stream(is->pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
        if(video_index >= 0
           //        &&
           //      is->pFormatCtx->streams[video_index]->codec->width > 100 &&
           //  	is->pFormatCtx->streams[video_index]->codec->height > 100
            )
        {
            stream_component_open(is, video_index);
        }
        if(is->videoStream < 0)
        {
            Debug(0, "Could not open video codec\n");
            fprintf(stderr, "%s: could not open video codec\n", is->filename);
            exit(-1);
        }

        if ( is->video_st->duration == AV_NOPTS_VALUE ||  is->video_st->duration < 0)
            is->duration =  ((float)pFormatCtx->duration) / AV_TIME_BASE;
        else
            is->duration =  av_q2d(is->video_st->time_base)* is->video_st->duration;


        /* Calc FPS */
        if(is->video_st->r_frame_rate.den && is->video_st->r_frame_rate.num)
        {
            is->fps = av_q2d(is->video_st->r_frame_rate);
        }
        else
        {
            is->fps = 1/av_q2d(is->video_st->codec->time_base);
        }

    }

    if (is->audioStream== -1 && video_index>=0)
    {

        audio_index = av_find_best_stream(is->pFormatCtx, AVMEDIA_TYPE_AUDIO, -1, video_index, NULL, 0);
        if(audio_index >= 0)
        {
            stream_component_open(is, audio_index);

            if (is->audioStream < 0)
            {
                Debug(1,"Could not open audio decoder or no audio present\n");
            }
        }

    }

    if (is->subtitleStream == -1 && video_index>=0)
    {
        subtitle_index = av_find_best_stream(is->pFormatCtx, AVMEDIA_TYPE_SUBTITLE, -1, video_index, NULL, 0);
        if(subtitle_index >= 0)
        {
            is->subtitle_st = pFormatCtx->streams[subtitle_index];
            if (demux_pid)
                selected_subtitle_pid = is->subtitle_st->id;
        }

    }
}



void file_close()
{
    is = global_video_state;

    avcodec_close(is->pFormatCtx->streams[is->videoStream]->codec);
    is->videoStream = -1;
    if (is->audioStream != -1) avcodec_close(is->pFormatCtx->streams[is->audioStream]->codec);
    is->audioStream = -1;
    //    avcodec_close(is->pFormatCtx->streams[is->subtitleStream]->codec);
    is->subtitleStream = -1;
    av_close_input_file(is->pFormatCtx);
    is->pFormatCtx = NULL;
    //  global_video_state = NULL;
};

/*
  https://www.livemeeting.com/cc/philipsconnectwebmeeting/join?id=M7ZF6B&role=attend&pw=5%7E.%7EtS%60p2


  // crt_wcstombs.c
  #include <stdio.h>
  #include <stdlib.h>

  int main( void )
  {
  int      i;
  char    *pmbbuf   = (char *)malloc( 100 );
  wchar_t *pwchello = L"Hello, world.";

  printf( "Convert wide-character string:\n" );
  i = wcstombs( pmbbuf, pwchello, 100 );
  printf( "   Characters converted: %u\n", i );
  printf( "   Multibyte character: %s\n\n", pmbbuf );
  }
*/

// copied & modified from mingw-runtime-3.13's init.c
typedef struct
{
    int newmode;
} _startupinfo;

extern void __wgetmainargs (int *, wchar_t ***, wchar_t ***, int, _startupinfo*);

int main (int argc, char ** argv)
{
    AVFormatContext *pFormatCtx;
    AVPacket pkt1, *packet = &pkt1;
    int video_index = -1;
    int audio_index = -1;
    int result = 0;
    int i;
    int ret;
    double retry_target;

#ifdef SELFTEST
    int tries = 0;
#endif
    retries = 0;

    char *ptr;
    size_t len;

#ifdef __MSVCRT_VERSION__

    int _argc = 0;
    wchar_t **_argv = 0;
    wchar_t **dummy_environ = 0;
    _startupinfo start_info;
    start_info.newmode = 0;
    __wgetmainargs(&_argc, &_argv, &dummy_environ, -1, &start_info);

    char *aargv[20];
    char (argt[20])[1000];

    argv = aargv;
    argc = _argc;

    for (i= 0; i< argc; i++)
    {
        argv[i] = &(argt[i][0]);
        WideCharToMultiByte(CP_UTF8, 0,_argv[i],-1, argv[i],  1000, NULL, NULL );
    }

#endif

#ifndef _DEBUG
    {
        //      raise_ exception();
#endif

        //		output_debugwindow = 1;
        if (strstr(argv[0],"comskipGUI"))
        {
            output_debugwindow = 1;
        } 
        else
        {
#ifdef _WIN32
            //added windows specific
            //			SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
            //			SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
#endif
        }
        //get path to executable
        ptr = argv[0];
        if (*ptr == '\"')
        {
            ptr++; //strip off quotation marks
            len = (size_t)(strchr(ptr,'\"') - ptr);
        }
        else
        {
            len = strlen(ptr);
        }
        strncpy(HomeDir, ptr, len);

        len = (size_t)max(0,strrchr(HomeDir,'\\') - HomeDir);
        if (len==0)
        {
            HomeDir[0] = '.';
            HomeDir[1] = '\0';
        }
        else
        {
            HomeDir[len] = '\0';
        }

        fprintf (stderr, "Comskip %s.%s, made using avcodec\n", COMSKIPVERSION,SUBVERSION);
        in_file = LoadSettings(argc, argv);
        file_open();
        csRestart = 0;
        framenum = 0;

        DUMP_OPEN
            av_log_set_level(AV_LOG_WARNING);
        av_log_set_flags(AV_LOG_SKIP_REPEATED);

        is = global_video_state;
        packet = &(is->audio_pkt);

        // main decode loop
    again:
        for(;;)
        {
            if(is->quit)
            {
                break;
            }
            // seek stuff goes here
            if(is->seek_req)
            {

#ifdef SELFTEST
                int stream_index= -1;
                int64_t seek_target = is->seek_pos;
                if (selftest == 1 || seek_target > 0)
                {

                    pev_best_effort_timestamp = 0;
                    best_effort_timestamp = 0;
                    pts_offset = 0.0;
                    is->video_clock = 0.0;
                    is->audio_clock = 0.0;
                    ret = avformat_seek_file(is->pFormatCtx, is->videoStream, INT64_MIN, is->seek_pos, INT64_MAX, is->seek_flags);
                    if (ret < 0)
                        ret = av_seek_frame(is->pFormatCtx, is->videoStream, seek_target, is->seek_flags);
                    if (ret < 0)
                    {
                        sample_file = fopen("seektest.log", "a+");
                        fprintf(sample_file, "seek pts  failed: , size=%8.1f \"%s\"\n", is->duration, is->filename);
                        fclose(sample_file);
                    }
                    if(ret < 0)
                        ret = av_seek_frame(is->pFormatCtx, is->videoStream, seek_target, AVSEEK_FLAG_BYTE);
                    if (ret < 0)
                    {
                        sample_file = fopen("seektest.log", "a+");
                        fprintf(sample_file, "seek byte failed: , size=%8.1f \"%s\"\n", is->duration, is->filename);
                        fclose(sample_file);
                    }
                    is->seek_req=0;

                    if (ret < 0)
                    {
                        if (is->pFormatCtx->iformat->read_seek)
                        {
                            printf("format specific\n");
                        }
                        else if(is->pFormatCtx->iformat->read_timestamp)
                        {
                            printf("frame_binary\n");
                        }
                        else
                        {
                            printf("generic\n");
                        }

                        fprintf(stderr, "%s: error while seeking. target: %d, stream_index: %d\n", is->pFormatCtx->filename, (int) seek_target, stream_index);
                    }
                }
                else
#endif
                {
                    is->seek_req = 0;
                    framenum = 0;
                    pts_offset = 0.0;
                    is->video_clock = 0.0;
                    is->audio_clock = 0.0;
                    file_close();
                    file_open();
                    sound_frame_counter = 0;
                    initial_pts = 0;
                    initial_pts_set = 0;
                    initial_apts_set = 0;
                    initial_apts = 0;
                    audio_samples = 0;
                }
            }

            // 
            ret=av_read_frame(is->pFormatCtx, packet);
            if ((selftest == 3 && retries==0 && is->video_clock >=30.0)) ret=AVERROR_EOF;
            if(ret < 0 )
            {
                if (ret == AVERROR_EOF || url_feof(is->pFormatCtx->pb))
                {
                    if (retries < live_tv_retries) {
                        if (retries==0) retry_target = is->video_clock;
                        file_close();

                        // debug info
                        char debug_error_code[256];
                        sprintf(debug_error_code, "%d (detail is here:http://www.ffmpeg.org/doxygen/trunk/group__lavu__error.html)", ret);

                        Debug(1,"\nReturnCode=%s\n", (ret == AVERROR_EOF) ? "AVERROR_EOF (End of file.)" : debug_error_code);
                        Debug(1,"\nRetry=%d at frame=%d, time=%8.2f seconds\n", retries, framenum, retry_target);

#ifdef _WIN32
                        Sleep(4000L);
#else
                        sleep(4);
#endif

                        file_open();
                        Set_seek(is, retry_target, is->duration);
                        retries++;
                        selftest = 0;
                        goto again;
                    }
                    break;
                }
            }


            if(packet->stream_index == is->videoStream)
            {
                video_packet_process(is, packet);

#ifdef SELFTEST
                if (selftest==1 && framenum > 501 && is->video_clock > 0)
                {

                    sample_file = fopen("seektest.log", "a+");
                    fprintf(sample_file, "target=%8.1f, result=%8.1f, error=%6.3f, size=%8.1f, \"%s\"\n",
                            is->seek_pts,
                            is->video_clock,
                            is->video_clock - is->seek_pts,
                            is->duration,
                            is->filename);
                    fclose(sample_file);
                    exit(1);
                }
#endif

            }
            else if(packet->stream_index == is->audioStream)
            {
                audio_packet_process(is, packet);
            }
            else
            {
                // do nothing
            }
            av_free_packet(packet);
#ifdef SELFTEST
            if (selftest == 1 && is->seek_req == 0 && framenum == 500)
            {
                double frame_delay = av_q2d(is->video_st->codec->time_base)* is->video_st->codec->ticks_per_frame;              // <------------------------ frame delay is the time in seconds till the next frame
                Set_seek(is, 30.0, 10000.0);
                framenum++;
            }
#endif
        }

        Debug( 10,"\nParsed %d video frames and %d audio frames of %4.2f fps\n", 
               framenum, sound_frame_counter, get_fps());
        Debug( 10,"\nMaximum Volume found is %d\n", max_volume_found);

        if (framenum > 0)
        {
            // (int) byterate = (fpos_t) fileendpos / (int) framenum : fpos_t is struct
#if defined(_WIN32) || defined(__FreeBSD__)
            byterate = fileendpos / framenum;
#else
            byterate = fileendpos.__pos / framenum; 
#endif
        }

        print_fps (1);

        in_file = 0;
        if (framenum>0)
        {
#if defined(_WIN32) || defined(__FreeBSD__)
            byterate = fileendpos / framenum;
#else
            byterate = fileendpos.__pos / framenum; 
#endif
            if(BuildMasterCommList())
            {
                result = 1;
                printf("Commercials were found.\n");
            }
            else
            {
                result = 0;
                printf("Commercials were not found.\n");
            }
            if (output_debugwindow)
            {
                processCC = 0;
                printf("Close window when done\n");

                DUMP_CLOSE
                    if (output_timing)
                    {
                        output_timing = 0;
                    }

#ifndef GUI
                while(1)
                {
                    ReviewResult();
#ifdef _WIN32
                    Sleep((DWORD)100);
#else
                    Sleep(100);
#endif
                }
#endif
            }
        }

#ifndef _DEBUG
    }
#endif
    exit (result);
}

