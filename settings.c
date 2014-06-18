
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <getopt.h>
#include <memory.h>
#include <errno.h>
#include "comskip.h"
#include "settings.h"

extern char			mpegfilename[MAX_PATH];
extern char			basename[MAX_PATH];
extern char			shortbasename[MAX_PATH];
extern char			inifilename[MAX_PATH];
extern FILE*		in_file;
extern FILE*			out_file;
extern FILE*			plist_cutlist_file;
extern FILE*			zoomplayer_cutlist_file;
extern FILE*			zoomplayer_chapter_file;
extern FILE*			vcf_file;
extern FILE*			vdr_file;
extern FILE*			projectx_file;
extern FILE*			avisynth_file;
extern FILE*			cuttermaran_file;
extern FILE*			videoredo_file;
extern FILE*			videoredo3_file;
extern FILE*			btv_file;
extern FILE*			edl_file;
extern FILE*			edlp_file;
extern FILE*			edlx_file;
extern FILE*			ipodchap_file;
extern FILE*			mls_file;
extern FILE*			womble_file;
extern FILE*			mpgtx_file;
extern FILE*			dvrcut_file;
extern FILE*			dvrmstb_file;
extern FILE*			tuning_file;
extern FILE*			training_file;
extern bool			output_plist_cutlist;
extern bool			output_zoomplayer_cutlist;
extern bool			output_zoomplayer_chapter;
extern bool			output_videoredo;
extern bool			isSecondPass;
extern long		frame_count;
extern bool			framearray;
extern bool			loadingCSV;
extern bool			loadingTXT;
extern bool			output_default;
extern FILE*			ini_file;
extern char			outputdirname[MAX_PATH];
extern char			workbasename[MAX_PATH];
extern char			outbasename[MAX_PATH];
extern char			logofilename[MAX_PATH];
extern char			logfilename[MAX_PATH];
extern char			filename[MAX_PATH];
extern char			HomeDir[256];
extern char			exefilename[MAX_PATH];
extern char			dictfilename[MAX_PATH];
extern char			out_filename[MAX_PATH];
extern int			play_nice_start;
extern int			play_nice_end;
extern bool			play_nice;
extern int			verbose;
extern int			selftest;
extern bool			output_debugwindow;
extern bool			output_timing;
extern int			subsample_video;
extern bool			output_console;
extern bool			output_demux;
extern bool			useExistingLogoFile;
extern bool			output_framearray;
extern bool			output_training;
extern int			commDetectMethod;
extern int			cutsceneno;
extern int		demux_pid;
extern int			giveUpOnLogoSearch;
extern bool		        processCC;
extern char			ini_text[40000];
#ifdef _WIN32
extern struct _stati64 instat;
#else
extern struct stat instat;
#endif

void usage()
{
    printf("Usage:\n  comskip  [-h|--help] [-w|--debugwindow] [-n|--playnice] [--zpcut] [--zpchapter] [--videoredo] [--csvout] [-p|--pid=<int>] [-t|--ts] [-d|--detectmethod=<int>] [-v|--verbose=<int>] [--ini=<file>] [--logo=<file>] <file>\n");

    printf("  -h, --help                Display syntax\n");
    printf("  -w, --debugwindow         Show debug window\n");
    printf("  -n, --playnice            Slows detection down\n");
    printf("  --zpcut                   Outputs a ZoomPlayer cutlist\n");
    printf("  --zpchapter               Outputs a ZoomPlayer chapter file\n");
    printf("  --videoredo               Outputs a VideoRedo cutlist\n");
    printf("  --csvout                  Outputs a csv of the frame array\n");
    printf("  -p, --pid=<int>           The PID of the video in the TS\n");
    printf("  -t, --ts                  The input file is a Transport Stream\n");
    printf("  -d, --detectmethod=<int>  An integer sum of the detection methods to use\n");
    printf("  -v, --verbose=<int>       Verbose level\n");
    printf("  -m, --demux               Demux the input file into Elementary Streams\n");
    printf("  --ini=<file>              Ini file to use\n");
    printf("  --logo=<file>             Logo file to use\n");
    printf("  <file>                    Input file\n");
    printf("\nDetection methods available:\n");
    printf("\t%3i - Black Frame\n", BLACK_FRAME);
    printf("\t%3i - Logo\n", LOGO);
    printf("\t%3i - Scene Change\n", SCENE_CHANGE);
    printf("\t%3i - Resolution Change\n", RESOLUTION_CHANGE);
    printf("\t%3i - Closed Captions\n", CC);
    printf("\t%3i - Aspect Ratio\n", AR);
    printf("\t%3i - Silence\n", SILENCE);
    printf("\t%3i - CutScenes\n", CUTSCENE);
    printf("\t255 - USE ALL AVAILABLE\n");
    exit(2);
}

FILE* LoadSettings(int argc, char** argv)
{
    FILE* test_file = NULL;
    enum OPT_TYPE{
        OPT_ZPCUT,
        OPT_ZPCHAPTER,
        OPT_VIDEOREDO,
        OPT_CVSOUT,
        OPT_QUALITY,
        OPT_PLIST,
        OPT_TIMING,
        OPT_INI,
        OPT_LOGO,
        OPT_CUT,
        OPT_OUTPUT,
        OPT_SELFTEST,
    };
    char ch;
    int i;
    time_t ltime;
    struct tm* now = NULL;
    int mil_time;
    FILE* logo_file = NULL;
    FILE* log_file = NULL;
    struct {
        bool playnice;
        bool zpcut;
        bool zpchapter;
        bool videoredo;
        bool csvout;
        bool quality;
        bool plist;
        int detectmethod;
        char* pid;
        int dump;
        bool ts;
        bool help;
        bool play;
        bool timing;
        bool debugwindow;
        bool quiet;
        bool demux;
        bool verbose;
        char* ini;
        char* logo;
        char* cut;
        char* output;
        int selftest;
        char* infile;
        char* outdir;
    } config;
    struct option longopts[] = {
        {"playnice",     no_argument,       NULL, 'n'},
        {"zpcut",        no_argument,       &config.zpcut,     true},
        {"zpchapter",    no_argument,       &config.zpchapter, true},
        {"videoredo",    no_argument,       &config.videoredo, true},
        {"csvout",       no_argument,       &config.csvout,    true},
        {"quality",      no_argument,       &config.quality,   true},
        {"plist",        no_argument,       &config.plist,     true},
        {"detectmethod", required_argument, NULL, 'd'},
        {"pid",          required_argument, NULL, 'p'},
        {"dump",         required_argument, NULL, 'u'},
        {"ts",           no_argument,       NULL, 't'},
        {"help",         no_argument,       NULL, 'h'},
        {"play",         no_argument,       NULL, 's'},
        {"timing",       no_argument,       &config.timing,    true},
        {"debugwindow",  no_argument,       NULL, 'w'},
        {"quiet",        no_argument,       NULL, 'q'},
        {"demux",        no_argument,       NULL, 'm'},
        {"verbose",      required_argument, NULL, 'v'},
        {"ini",          required_argument, NULL, OPT_INI},
        {"logo",         required_argument, NULL, OPT_LOGO},
        {"cut",          required_argument, NULL, OPT_CUT},
        {"output",       required_argument, NULL, OPT_OUTPUT},
        {"selftest",     required_argument, NULL, OPT_SELFTEST},
        {0, 0, 0, 0}
    };

    memset(&config, 0, sizeof(config));

    // start opt analyze
    while ((ch = getopt_long(argc, argv, "nd:p:u:thswqmv:", longopts, NULL)) != -1)
    {
        switch (ch){
        case 'n':
            config.playnice = true;
            break;
        case 'd':
            config.detectmethod = atoi(optarg);
            break;
        case 'p':
            config.pid = optarg;
            break;
        case 'u':
            config.dump = atoi(optarg);
            break;
        case 't':
            config.ts = true;
            break;
        case 's':
            config.play = true;
            break;
        case 'w':
            config.debugwindow = true;
            break;
        case 'q':
            config.quiet = true;
            break;
        case 'm':
            config.demux = true;
            break;
        case 'v':
            config.verbose = atoi(optarg);
            break;
        case OPT_INI:
            config.ini = optarg;
            break;
        case OPT_LOGO:
            config.logo = optarg;
            break;
        case OPT_CUT:
            config.cut = optarg;
            break;
        case OPT_OUTPUT:
            config.output = optarg;
            break;
        case OPT_SELFTEST:
            config.selftest = atoi(optarg);
            break;
        case 'h':
        case ':':
        case '?':
            usage();
            exit(2);
            break;
        };
    }

    if (optind < argc) 
    {
        config.infile = argv[optind];
        ++optind;
    }
    if (optind < argc)
    {
        config.outdir = argv[optind];
        ++optind;
    }
    // opt analyze end

    // apply config
    if (config.infile == NULL )
    {
        // input file not determine
        usage();
        exit(2);
    }

    int __len = strlen(config.infile);
    char* ext = config.infile + __len;
    while (ext != config.infile && *(--ext) != '.');

    if (strcmp(ext, ".csv") != 0 && strcmp(ext, ".txt") != 0)
    {
        sprintf(mpegfilename, "%s", config.infile);
        int i = mystat((char*)config.infile, &instat);
        if (i<0)
        {
            fprintf(stderr, "%s - could not open file %s\n", strerror(errno), config.infile);
            exit(3);
        }

        // copy file name without ext
        strncpy(basename, config.infile, __len - strlen(ext));

        // copy file name without path
        __len = strlen(basename);
        while (__len && basename[__len-1] != '\\' && basename[__len-1] != '/')
        {
            --__len;
        }
        strcpy(shortbasename, &basename[__len]);
        sprintf(inifilename, "%.*scomskip.ini", __len, basename);
    }
    else if(strcmp(ext, ".csv") == 0)
    {
        loadingCSV = true;
        in_file = myfopen(config.infile, "r");
        printf("Opening %s array file.\n", config.infile);
        if (!in_file)
        {
            fprintf(stderr, "%s - could not open file %s\n", strerror(errno), config.infile);
            exit(4);
        }

        sprintf(basename,     "%.*s", (int)strlen(config.infile) - (int)strlen(ext), config.infile);
        sprintf(mpegfilename, "%.*s.mpg", (int)strlen(basename), basename);
        test_file = myfopen(mpegfilename, "rb");
        if (!test_file)
        {
            sprintf(mpegfilename, "%.*s.ts", (int)strlen(basename), basename);
            test_file = myfopen(mpegfilename, "rb");
        }
        if (!test_file)
        {
            sprintf(mpegfilename, "%.*s.tp", (int)strlen(basename), basename);
            test_file = myfopen(mpegfilename, "rb");
        }
        if (!test_file)
        {
            sprintf(mpegfilename, "%.*s.dvr-ms", (int)strlen(basename), basename);
            test_file = myfopen(mpegfilename, "rb");
        }
        if (!test_file)
        {
            sprintf(mpegfilename, "%.*s.wtv", (int)strlen(basename), basename);
            test_file = myfopen(mpegfilename, "rb");
        }
        if (!test_file)
        {
            sprintf(mpegfilename, "%.*s.mp4", (int)strlen(basename), basename);
            test_file = myfopen(mpegfilename, "rb");
        }
        if (!test_file)
        {
            mpegfilename[0] = 0;
        }
        else
        {
            fclose(test_file);
        }


        __len = strlen(basename);
        while (__len>0 && basename[__len-1] != '\\' && basename[__len-1] != '/')
        {
            __len--;
        }
        strcpy(shortbasename, &basename[__len]);
        sprintf(inifilename, "%.*scomskip.ini", __len, basename);
        if (mpegfilename[0] == 0) sprintf(mpegfilename, "%s.mpg", basename);

    }
    else if(strcmp(ext, ".txt") == 0)
    {
        loadingTXT = true;
        output_default = false;
        in_file = myfopen(config.infile, "r");
        printf("Opening %s for review\n", config.infile);
        if (!in_file)
        {
            fprintf(stderr, "%s - could not open file %s\n", strerror(errno), config.infile);
            exit(4);
        }
        fclose(in_file);
        in_file = 0;

        sprintf(basename,     "%.*s", (int)strlen(config.infile) - (int)strlen(ext), config.infile);
        sprintf(mpegfilename, "%.*s.mpg", (int)strlen(basename), basename);
        test_file = myfopen(mpegfilename, "rb");
        if (!test_file)
        {
            sprintf(mpegfilename, "%.*s.ts", (int)strlen(basename), basename);
            test_file = myfopen(mpegfilename, "rb");
        }
        if (!test_file)
        {
            sprintf(mpegfilename, "%.*s.tp", (int)strlen(basename), basename);
            test_file = myfopen(mpegfilename, "rb");
        }
        if (!test_file)
        {
            sprintf(mpegfilename, "%.*s.dvr-ms", (int)strlen(basename), basename);
            test_file = myfopen(mpegfilename, "rb");
        }
        if (!test_file)
        {
            sprintf(mpegfilename, "%.*s.wtv", (int)strlen(basename), basename);
            test_file = myfopen(mpegfilename, "rb");
        }
        if (!test_file)
        {
            sprintf(mpegfilename, "%.*s.mp4", (int)strlen(basename), basename);
            test_file = myfopen(mpegfilename, "rb");
        }
        if (!test_file)
        {
            mpegfilename[0] = 0;
        }
        else
        {
            fclose(test_file);
        }

        __len = strlen(basename);
        while (__len>0 && basename[__len-1] != '\\')
        {
            __len--;
        }
        strcpy(shortbasename, &basename[__len]);
        sprintf(inifilename, "%.*scomskip.ini", __len, basename);
    }
    else
    {
        printf("The input file was not a Video file or comskip CSV or TXT file - %s.\n", config.infile);
        exit(5);
    }

    if (config.ini)
    {
        sprintf(inifilename, "%s", config.ini);
        printf("Setting ini file to %s as per commandline\n", inifilename);
    }

    ini_file = myfopen(inifilename, "r");

    if (config.output)
    {
        sprintf(outputdirname, config.output);
        i = strlen(outputdirname);
        if (outputdirname[i-1] == '\\')
            outputdirname[i-1] = 0;
        sprintf(workbasename, "%s\\%s", outputdirname,  shortbasename);
        strcpy(outbasename, workbasename);
    }
    else
    {
        outputdirname[0] = 0;
        strcpy(workbasename, basename);
    }

    if (config.outdir)
    {
        sprintf(outputdirname, config.outdir);
        i = strlen(outputdirname);
        if (outputdirname[i-1] == '\\')
            outputdirname[i-1] = 0;
        sprintf(outbasename, "%s\\%s", outputdirname,  shortbasename);
    }
    else
    {
        outputdirname[0] = 0;
        strcpy(outbasename, basename);
    }

    if (config.output && !config.outdir)   // --output also sets the output file location if not specified as 2nd argument.
    {
        strcpy(outbasename, workbasename);
    }


    sprintf(logofilename, "%s.logo.txt", workbasename);
    sprintf(logfilename, "%s.log", workbasename);
    sprintf(filename, "%s.txt", outbasename);
    if (strcmp(HomeDir, ".") == 0)
    {
        if (!ini_file)
        {
            sprintf(inifilename, "comskip.ini");
            ini_file = myfopen(inifilename, "r");
        }
        sprintf(exefilename, "comskip.exe");
        sprintf(dictfilename, "comskip.dictionary");
    }
    else
    {
        if (!ini_file)
        {
            sprintf(inifilename, "%s\\comskip.ini", HomeDir);
            ini_file = myfopen(inifilename, "r");
        }
        sprintf(exefilename, "%s\\comskip.exe", HomeDir);
        sprintf(dictfilename, "%s\\comskip.dictionary", HomeDir);
    }

    if (config.cut)
    {
        printf("Loading cutfile %s as per commandline\n", config.cut);
        LoadCutScene(config.cut);
    }

    if (config.logo)
    {
        sprintf(logofilename, config.logo);
        printf("Setting logo file to %s as per commandline\n", logofilename);
    }

    LoadIniFile();
    time(&ltime);
    now = localtime(&ltime);
    mil_time = (now->tm_hour * 100) + now->tm_min;

    // play_nice_start and play_nice_end set by LoadIniFile()
    if ((play_nice_start > -1) && (play_nice_end > -1))
    {
        if (play_nice_start > play_nice_end)
        {
            if ((mil_time >= play_nice_start) || (mil_time <= play_nice_end)) play_nice = true;
        }
        else
        {
            if ((mil_time >= play_nice_start) && (mil_time <= play_nice_end)) play_nice = true;
        }
    }

    if (config.verbose)
    {
        verbose = config.verbose;
        printf("Setting verbose level to %i as per command line.\n", verbose);
    }

    if (config.selftest)
    {
        selftest = config.selftest;
        printf("Setting selftest to %i as per command line.\n", selftest);
    }

    if (config.debugwindow || loadingTXT)
    {
        output_debugwindow = true;
    }
    if (config.timing)
    {
        output_timing = true;
    }
    if (config.play)
    {
        subsample_video = 0;
        output_debugwindow = true;
    }

    if (config.quiet)
    {
        output_console = false;
    }

    if (strstr(argv[0],"GUI"))
        output_debugwindow = true;

    if (config.demux)
    {
        output_demux = true;
    }

    if (!loadingTXT && !useExistingLogoFile && config.logo == false)
    {
        logo_file = myfopen(logofilename, "r");
        if(logo_file)
        {
            fclose(logo_file);
            myremove(logofilename);
        }
    }

    if (config.csvout)
    {
        output_framearray = true;
    }

    if (config.quality)
    {
        output_training = true;
    }

    if (verbose)
    {
        logo_file = myfopen(logofilename, "r");
        if (loadingTXT)
        {
            // Do nothing to the log file
            verbose = 0;
        }
        else if (loadingCSV)
        {
            log_file = myfopen(logfilename, "w");
            fprintf(log_file, "################################################################\n");
            fprintf(log_file, "Generated using Comskip %s.%s\n", COMSKIPVERSION,SUBVERSION);
            fprintf(log_file, "Loading comskip csv file - %s\n", config.infile);
            fprintf(log_file, "Time at start of run:\n%s", ctime(&ltime));
            fprintf(log_file, "################################################################\n");
            fclose(log_file);
            log_file = NULL;
        }
        else if (logo_file)
        {
            fclose(logo_file);
            log_file = myfopen(logfilename, "a+");
            fprintf(log_file, "################################################################\n");
            fprintf(log_file, "Starting second pass using %s\n", logofilename);
            fprintf(log_file, "Time at start of second run:\n%s", ctime(&ltime));
            fprintf(log_file, "################################################################\n");
            fclose(log_file);
            log_file = NULL;
        }
        else
        {
            log_file = myfopen(logfilename, "w");
            fprintf(log_file, "################################################################\n");
            fprintf(log_file, "Generated using Comskip %s.%s\n", COMSKIPVERSION,SUBVERSION);
            fprintf(log_file, "Time at start of run:\n%s", ctime(&ltime));
            fprintf(log_file, "################################################################\n");
            fclose(log_file);
            log_file = NULL;
        }
    }

    if (config.playnice)
    {
        play_nice = true;
        Debug(1, "ComSkip playing nice due as per command line.\n");
    }

    if (config.detectmethod)
    {
        commDetectMethod = config.detectmethod;
        printf("Setting detection methods to %i as per command line.\n", commDetectMethod);
    }

    if (config.dump)
    {
        cutsceneno = config.dump;
        printf("Setting dump frame number to %i as per command line.\n", cutsceneno);
    }

    if (config.ts)
    {
        demux_pid = 1;
        printf("Auto selecting the PID.\n");
    }

    if (config.pid)
    {
        //        demux_pid = cl_pid->ival[0];
        sscanf(config.pid,"%x", &demux_pid);
        printf("Selecting PID %x as per command line.\n", demux_pid);
    }



    Debug(9, "Mpeg:\t%s\nExe\t%s\nLogo:\t%s\nIni:\t%s\n", mpegfilename, exefilename, logofilename, inifilename);
    Debug(1, "\nDetection Methods to be used:\n");
    i = 0;
    if (commDetectMethod & BLACK_FRAME)
    {
        i++;
        Debug(1, "\t%i) Black Frame\n", i);
    }

    if (commDetectMethod & LOGO)
    {
        i++;
        Debug(1, "\t%i) Logo - Give up after %i seconds\n", i, giveUpOnLogoSearch);
    }

    if (commDetectMethod & SCENE_CHANGE)
    {
        i++;
        Debug(1, "\t%i) Scene Change\n", i);
    }

    if (commDetectMethod & RESOLUTION_CHANGE)
    {
        i++;
        Debug(1, "\t%i) Resolution Change\n", i);
    }

    if (commDetectMethod & CC)
    {
        i++;
        processCC = true;
        Debug(1, "\t%i) Closed Captions\n", i);
    }

    if (commDetectMethod & AR)
    {
        i++;
        Debug(1, "\t%i) Aspect Ratio\n", i);
    }

    if (commDetectMethod & SILENCE)
    {
        i++;
        Debug(1, "\t%i) Silence\n", i);
    }

    if (commDetectMethod & CUTSCENE)
    {
        i++;
        Debug(1, "\t%i) CutScenes\n", i);
    }


    Debug(1, "\n");
    if (play_nice_start || play_nice_end)
    {
        Debug(
            1,
            "\nComSkip throttles back from %.4i to %.4i.\nThe time is now %.4i ",
            play_nice_start,
            play_nice_end,
            mil_time
            );
        if (play_nice)
        {
            Debug(1, "so comskip is running slowly.\n");
        }
        else
        {
            Debug(1, "so it's full speed ahead!\n");
        }
    }

    Debug(10, "\nSettings\n--------\n");
    Debug(10, "%s\n", ini_text);
    sprintf(out_filename, "%s.txt", outbasename);


    if (!loadingTXT)
    {
        logo_file = myfopen(logofilename, "r+");
        if (logo_file)
        {
            Debug(1, "The logo mask file exists.\n");
            fclose(logo_file);
            LoadLogoMaskData();
        }
    }

    out_file = plist_cutlist_file = zoomplayer_cutlist_file = zoomplayer_chapter_file = vcf_file = vdr_file = projectx_file = avisynth_file = cuttermaran_file = videoredo_file = videoredo3_file = btv_file = edl_file = ipodchap_file = edlp_file = edlx_file = mls_file = womble_file = mpgtx_file = dvrcut_file = dvrmstb_file = tuning_file = training_file = 0L;

    if (config.plist)
        output_plist_cutlist = true;
    if (config.zpcut)
        output_zoomplayer_cutlist = true;
    if (config.zpchapter)
        output_zoomplayer_chapter = true;
    if (config.videoredo)
        output_videoredo = true;

    if (output_default && ! loadingTXT)
    {
        if(!isSecondPass)
        {
            out_file = myfopen(out_filename, "w");
            if (!out_file)
            {
                fprintf(stderr, "%s - could not create file %s\n", strerror(errno), filename);
                exit(6);
            }
            else
            {
                output_default = true;
                fclose(out_file);
            }
        }
    }

    if (loadingTXT)
    {
        frame_count = InputReffer(".txt", true);
        if (frame_count < 0)
        {
            printf("Incompatible TXT file\n");
            exit(2);
        }
        framearray = false;
        printf("Close window or hit ESCAPE when done\n");
        output_debugwindow = true;
        ReviewResult();
    }

/*     if (!loadingTXT && (output_srt || output_smi )) */
/*     { */
/*         i = 0; */
/*         CEW_argv[i++] = "comskip.exe"; */
/*         if (output_smi) */
/*         { */
/*             CEW_argv[i++] = "-sami"; */
/*             output_srt = 1; */
/*         } */
/*         else */
/*             CEW_argv[i++] = "-srt"; */
/*         CEW_argv[i++] = in->filename[0]; */
/* #ifdef PROCESS_CC */
/*         CEW_init (i, CEW_argv); */
/* #endif */
/*     } */


    if (loadingCSV)
    {
        output_framearray = false;
        ProcessCSV(in_file);
        output_debugwindow = false;
    }


exit:
    return (in_file);

    printf("infile:%s\nmpegfilename:%s\nbasename:%s\nshotbasename:%s\ninifilename:%s\n",config.infile, mpegfilename, basename, shortbasename,inifilename);
    exit(2);
    return NULL;
}
