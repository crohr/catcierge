#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "catcierge_fsm.h"
#include "catcierge_args.h"
#include "catcierge_types.h"
#include "catcierge_config.h"
#include "catcierge_args.h"
#include "catcierge_output.h"
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>

#ifdef WITH_ZMQ
#include <czmq.h>
#endif

typedef struct fsm_tester_ctx_s
{
	char *img_paths[4];
	size_t img_count;
	double delay;
} fsm_tester_ctx_t;

static IplImage *load_image(const char *path)
{
	IplImage *img = NULL;

	if (!(img = cvLoadImage(path, 0)))
	{
		fprintf(stderr, "Failed to load image: %s\n", path);
		return NULL;
	}

	return img;
}

static IplImage *create_clear_image()
{
	IplImage *img = NULL;

	if (!(img = cvCreateImage(cvSize(320, 240), IPL_DEPTH_8U, 1)))
	{
		fprintf(stderr, "Failed to create clear image\n");
		return NULL;
	}

	cvSet(img, CV_RGB(255, 255, 255), NULL);

	return img;
}

static void show_usage(const char *progname)
{
	fprintf(stderr, "Usage: %s [options] --images <4 images>\n", progname);
	fprintf(stderr, "\n");
	fprintf(stderr, "[options] includes the arguments supported by the catcierge grabber as well.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Catcierge FSM tester arguments:\n");
	fprintf(stderr, "-------------------------------\n");
	fprintf(stderr, " --help                     Show this help (including full catcierge help).\n");
	fprintf(stderr, " --images <4 input images>  Input images that are passed to catcierge.\n");
	fprintf(stderr, " --delay <seconds>          Delay this long before passing the images.\n");
	fprintf(stderr, " --base_time <date>         The base date time we should use instead of the current time.\n");
	fprintf(stderr, "                            In the format YYYY-mm-ddTHH:MM:SS.\n");
}

int parse_args_callback(catcierge_args_t *args,
		char *key, char **values, size_t value_count, void *user)
{
	size_t i;
	fsm_tester_ctx_t *ctx = (fsm_tester_ctx_t *)user;
	printf("Parse FSM tester arguments callback\n");

	if (!strcmp(key, "images"))
	{
		if (value_count != 4)
		{
			fprintf(stderr, "Need %d images but got %d\n", 4, (int)value_count);
			return -1;
		}

		for (i = 0; i < value_count; i++)
		{
			ctx->img_paths[i] = values[i];
			ctx->img_count++;
		}

		return 0;
	}
	else if (!strcmp(key, "delay"))
	{
		if (value_count != 1)
		{
			fprintf(stderr, "Invalid argument for --delay, need an integer!\n");
			return -1;
		}

		ctx->delay = atof(values[0]);
		return 0;
	}
	else if (!strcmp(key, "help"))
	{
		fprintf(stderr, "\n###############################################################################\n\n");
		show_usage(args->program_name);
	}

	return -1;
}

int main(int argc, char **argv)
{
	size_t i;
	int ret = 0;
	fsm_tester_ctx_t ctx;
	catcierge_grb_t grb;
	catcierge_args_t *args = &grb.args;

	memset(&ctx, 0, sizeof(ctx));
	catcierge_grabber_init(&grb);

	if (catcierge_parse_config(args, argc, argv))
	{
		show_usage(argv[0]);
		fprintf(stderr, "\n\nFailed to parse config file\n\n");
		return -1;
	}

	if (catcierge_parse_cmdargs(args, argc, argv, parse_args_callback, &ctx))
	{
		show_usage(argv[0]);
		ret = -1; goto fail;
	}

	if (ctx.img_count == 0)
	{
		show_usage(argv[0]);
		fprintf(stderr, "\nNo input images specified!\n\n");
		ret = -1; goto fail;
	}

	if (catcierge_matcher_init(&grb.matcher, catcierge_get_matcher_args(args)))
	{
		fprintf(stderr, "\n\nFailed to %s init matcher\n\n", grb.args.matcher);
		return -1;
	}

	if (catcierge_output_init(&grb.output))
	{
		fprintf(stderr, "Failed to init output template system\n");
		exit(-1);
	}

	if (catcierge_output_load_templates(&grb.output,
			args->inputs, args->input_count))
	{
		fprintf(stderr, "Failed to load output templates\n");
		exit(-1);
	}

	#ifdef WITH_ZMQ
	catcierge_zmq_init(&grb);
	#endif

	catcierge_set_state(&grb, catcierge_state_waiting);
	catcierge_timer_set(&grb.frame_timer, 1.0);
	catcierge_timer_start(&grb.frame_timer);

	// For delayed start we create a clear image that will
	// be fed to the state machine until we're ready to obstruct the frame.
	if (ctx.delay > 0.0)
	{
		IplImage *clear_img = NULL;
		catcierge_timer_t t;
		printf("Delaying match start by %0.2f seconds\n", ctx.delay);

		if (!(clear_img = create_clear_image()))
		{
			ret = -1; goto fail;
		}

		catcierge_timer_reset(&t);
		catcierge_timer_set(&t, ctx.delay);
		catcierge_timer_start(&t);
		grb.img = clear_img;

		while (!catcierge_timer_has_timed_out(&t))
		{
			catcierge_run_state(&grb);
		}

		cvReleaseImage(&clear_img);
		grb.img = NULL;
	}

	// Load the first image and obstruct the frame.
	if (!(grb.img = load_image(ctx.img_paths[0])))
	{
		ret = -1; goto fail;
	}

	catcierge_run_state(&grb);
	cvReleaseImage(&grb.img);
	grb.img = NULL;

	// Load the match images.
	for (i = 0; i < ctx.img_count; i++)
	{
		if (!(grb.img = load_image(ctx.img_paths[i])))
		{
			ret = -1; goto fail;
		}

		catcierge_run_state(&grb);

		cvReleaseImage(&grb.img);
		grb.img = NULL;
	}

fail:
	#ifdef WITH_ZMQ
	catcierge_zmq_destroy(&grb);
	#endif
	catcierge_matcher_destroy(&grb.matcher);
	catcierge_output_destroy(&grb.output);
	catcierge_grabber_destroy(&grb);

	return ret;
}