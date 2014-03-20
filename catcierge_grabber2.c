
#include <signal.h>
#include "catcierge_config.h"
#include "catcierge_args.h"
#include "catcierge_log.h"
#include "catcierge_fsm.h"

catcierge_grb_t grb;

static void sig_handler(int signo)
{
	switch (signo)
	{
		case SIGINT:
		{
			static int sigint_count = 0;
			CATLOG("Received SIGINT, stopping...\n");

			// Force a quit if we're not in the loop
			// or the SIGINT is a repeat.
			if (!grb.running || (sigint_count > 0))
			{
				catcierge_do_unlock(&grb);
				exit(0);
			}

			// Simply kill the loop and let the program do normal cleanup.
			grb.running = 0;

			sigint_count++;

			break;
		}
		#ifndef _WIN32
		case SIGUSR1:
		{
			CATLOG("Received SIGUSR1, forcing unlock...\n");
			catcierge_do_unlock(&grb);
			// TODO: Fix state change here...
			//state = STATE_WAITING;
			break;
		}
		case SIGUSR2:
		{
			CATLOG("Received SIGUSR2, forcing lockout...\n");
			//start_locked_state();
			break;
		}
		#endif // _WIN32
	}
}

void setup_sig_handlers()
{
	if (signal(SIGINT, sig_handler) == SIG_ERR)
	{
		CATERR("Failed to set SIGINT handler\n");
	}

	#ifndef _WIN32
	if (signal(SIGUSR1, sig_handler) == SIG_ERR)
	{
		CATERR("Failed to set SIGUSR1 handler (used to force unlock)\n");
	}

	if (signal(SIGUSR2, sig_handler) == SIG_ERR)
	{
		CATERR("Failed to set SIGUSR2 handler (used to force unlock)\n");
	}
	#endif // _WIN32
}

int main(int argc, char **argv)
{
	catcierge_args_t *args;
	args = &grb.args;

	fprintf(stderr, "\nCatcierge Grabber2 v" CATCIERGE_VERSION_STR
					" (C) Joakim Soderberg 2013-2014\n\n");

	if (catcierge_grabber_init(&grb))
	{
		fprintf(stderr, "Failed to init\n");
		return -1;
	}

	setup_sig_handlers();

	// Get program settings.
	{
		if (catcierge_parse_config(args, argc, argv))
		{
			catcierge_show_usage(args, argv[0]);
			return -1;
		}

		if (catcierge_parse_cmdargs(args, argc, argv))
		{
			catcierge_show_usage(args, argv[0]);
			return -1;
		}

		// Set some defaults.
		if (args->snout_count == 0)
		{
			args->snout_paths[0] = "snout.png";
			args->snout_count++;
		}

		if (args->output_path)
		{
			char cmd[1024];
			CATLOG("Creating output directory: \"%s\"\n", args->output_path);
			#ifdef WIN32
			snprintf(cmd, sizeof(cmd), "md %s", args->output_path);
			#else
			snprintf(cmd, sizeof(cmd), "mkdir -p %s", args->output_path);
			#endif
			system(cmd);
		}
		else
		{
			args->output_path = ".";
		}

		catcierge_print_settings(args);
	}

	if (args->log_path)
	{
		if (!(grb.log_file = fopen(args->log_path, "a+")))
		{
			CATERR("Failed to open log file \"%s\"\n", args->log_path);
		}
	}

	if (catcierge_setup_gpio(&grb))
	{
		CATERR("Failed to setup GPIO pins\n");
		return -1;
	}

	CATLOG("Initialized GPIO pins\n");

	if (catcierge_init(&grb.matcher, args->snout_paths, args->snout_count))
	{
		CATERR("Failed to init catcierge lib!\n");
		return -1;
	}

	catcierge_set_match_flipped(&grb.matcher, args->match_flipped);
	catcierge_set_match_threshold(&grb.matcher, args->match_threshold);

	CATLOG("Initialized catcierge image recognition\n");

	catcierge_setup_camera(&grb);
	CATLOG("Starting detection!\n");
	grb.running = 1;
	catcierge_set_state(&grb, catcierge_state_waiting);
	catcierge_timer_set(&grb.frame_timer, 1.0);

	// Run the program state machine.
	do
	{
		if (!catcierge_timer_isactive(&grb.frame_timer))
		{
			catcierge_timer_start(&grb.frame_timer);
		}
		// Always feed the RFID readers and read a frame.
		#ifdef WITH_RFID
		if ((args->rfid_inner_path || args->rfid_outer_path) 
			&& catcierge_rfid_ctx_service(&grb.rfid_ctx))
		{
			CATERRFPS("Failed to service RFID readers\n");
		}
		#endif // WITH_RFID

		grb.img = catcierge_get_frame(&grb);

		catcierge_run_state(&grb);
		catcierge_calculate_fps(&grb);
	} while (grb.running);

	catcierge_destroy(&grb.matcher);
	catcierge_destroy_camera(&grb);
	catcierge_grabber_destroy(&grb);

	if (grb.log_file)
	{
		fclose(grb.log_file);
	}

	return 0;
}