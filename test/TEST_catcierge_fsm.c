#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "catcierge_fsm.h"
#include "minunit.h"
#include "catcierge_test_config.h"
#include "catcierge_test_helpers.h"
#include "catcierge_args.h"
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>

static IplImage *open_test_image(int series, int i)
{
	IplImage *img;
	char path[1024];

	snprintf(path, sizeof(path), "%s/real/series/%04d_%02d.png", CATCIERGE_IMG_ROOT, series, i);
	catcierge_test_STATUS("%s", path);

	if (!(img = cvLoadImage(path, 0)))
	{
		catcierge_test_FAILURE("Failed to load image %s", path);
	}

	return img;
}

static void set_default_test_snouts(catcierge_args_t *args)
{
	assert(args);
	args->snout_paths[0] = CATCIERGE_SNOUT1_PATH;
	args->snout_count++;
	args->snout_paths[1] = CATCIERGE_SNOUT2_PATH;
	args->snout_count++;
}

static void free_test_image(catcierge_grb_t *grb)
{
	cvReleaseImage(&grb->img);
	grb->img = NULL;
}

//
// Helper function for loading an image from disk and running the state machine.
//
static int load_test_image_and_run(catcierge_grb_t *grb, int series, int i)
{
	if ((grb->img = open_test_image(series, i)) == NULL)
	{
		return -1;
	}

	catcierge_run_state(grb);
	free_test_image(grb);
	return 0;
}

static char *run_consecutive_lockout_abort_tests()
{
	int i;
	catcierge_grb_t grb;
	catcierge_args_t *args;
	args = &grb.args;

	catcierge_grabber_init(&grb);

	args->saveimg = 0;
	set_default_test_snouts(args);

	if (catcierge_template_matcher_init(&grb.matcher, args->snout_paths, args->snout_count))
	{
		return "Failed to init catcierge lib!\n";
	}

	catcierge_template_matcher_set_match_flipped(&grb.matcher, 1);
	catcierge_template_matcher_set_match_threshold(&grb.matcher, 0.8);

	catcierge_set_state(&grb, catcierge_state_waiting);

	args->max_consecutive_lockout_count = 3;
	// It's important we change this from the default 3 seconds.
	// Since when the tests run under valgrind they are much slower
	// we need to extend the time that is counted as consecutive tests
	// otherwise the consecutive count will become invalid.
	args->consecutive_lockout_delay = 10;
	args->lockout_time = 0;

	grb.running = 1;

	// Now trigger "max consecutive lockouts" but add a success in the middle
	// to make sure the consecutive count is reset.
	{
		load_test_image_and_run(&grb, 1, 2); // Obstruct.
		load_test_image_and_run(&grb, 1, 2); // Pass 4 images (invalid).
		load_test_image_and_run(&grb, 1, 3);
		load_test_image_and_run(&grb, 1, 4);
		load_test_image_and_run(&grb, 1, 4);
		catcierge_run_state(&grb);
		catcierge_test_STATUS("Consecutive lockout %d", grb.consecutive_lockout_count);
		mu_assert("Expected 1 consecutive lockout count", (grb.consecutive_lockout_count == 1));

		load_test_image_and_run(&grb, 1, 2); // Obstruct.
		load_test_image_and_run(&grb, 1, 2); // Pass 4 images (invalid).
		load_test_image_and_run(&grb, 1, 3);
		load_test_image_and_run(&grb, 1, 4);
		load_test_image_and_run(&grb, 1, 4);
		catcierge_run_state(&grb);
		catcierge_test_STATUS("Consecutive lockout %d", grb.consecutive_lockout_count);
		mu_assert("Expected 2 consecutive lockout count", (grb.consecutive_lockout_count == 2));

		// Here we expect the lockou count to be reset.
		// Note that we must sleep at least args->consecutive_lockout_delay seconds
		// before causing another lockout, otherwise we will still count it as
		// a consecutive lockout...
		load_test_image_and_run(&grb, 1, 1); // Obstruct.
		load_test_image_and_run(&grb, 1, 1); // Pass 4 images (valid).
		load_test_image_and_run(&grb, 1, 2);
		load_test_image_and_run(&grb, 1, 3);
		load_test_image_and_run(&grb, 1, 4);
		load_test_image_and_run(&grb, 1, 5); // Clear frame.
		catcierge_test_STATUS("Consecutive lockout reset");

		catcierge_test_STATUS("Sleep %0.1f seconds so that the next lockout isn't counted as consecutive", args->consecutive_lockout_delay);
		sleep(args->consecutive_lockout_delay);

		load_test_image_and_run(&grb, 1, 2); // Obstruct.
		load_test_image_and_run(&grb, 1, 2); // Pass 4 images (invalid).
		load_test_image_and_run(&grb, 1, 3);
		load_test_image_and_run(&grb, 1, 4);
		load_test_image_and_run(&grb, 1, 4);
		catcierge_run_state(&grb);
		
		catcierge_test_STATUS("Consecutive lockout %d", grb.consecutive_lockout_count);
		mu_assert("Expected 0 lockout count", (grb.consecutive_lockout_count == 0));
	}

	mu_assert("Expected program to be in running state after consecutive lockout count is aborted", (grb.running == 1));

	// Now trigger "max consecutive lockouts" but add a success in the middle
	// to make sure the consecutive count is reset.

	catcierge_template_matcher_destroy(&grb.matcher);
	catcierge_grabber_destroy(&grb);

	return NULL;
}

static char *run_consecutive_lockout_tests()
{
	int i;
	catcierge_grb_t grb;
	catcierge_args_t *args;
	args = &grb.args;

	catcierge_grabber_init(&grb);

	args->saveimg = 0;
	set_default_test_snouts(args);

	if (catcierge_template_matcher_init(&grb.matcher, args->snout_paths, args->snout_count))
	{
		return "Failed to init catcierge lib!\n";
	}

	catcierge_template_matcher_set_match_flipped(&grb.matcher, 1);
	catcierge_template_matcher_set_match_threshold(&grb.matcher, 0.8);

	catcierge_set_state(&grb, catcierge_state_waiting);

	args->max_consecutive_lockout_count = 3;
	args->consecutive_lockout_delay = 10;
	args->lockout_time = 0;

	grb.running = 1;

	// Cause "max consecutive lockout count" of lockouts
	// to make sure an abort is triggered.
	for (i = 0; i <= 3; i++)
	{
		// Obstruct the frame to begin matching.
		load_test_image_and_run(&grb, 1, 2);

		load_test_image_and_run(&grb, 1, 2);
		load_test_image_and_run(&grb, 1, 3);
		load_test_image_and_run(&grb, 1, 4);
		load_test_image_and_run(&grb, 1, 4);

		// Run the statemachine to end lockout
		// without passing a new image.
		catcierge_run_state(&grb);
		mu_assert("Unexpeced consecutive lockout count", (grb.consecutive_lockout_count == (i+1)));

		catcierge_test_STATUS("Consecutive lockout %d", i+1);
	}

	mu_assert("Expected program to be in non-running state after max_consecutive_lockout_count", (grb.running == 0));

	catcierge_template_matcher_destroy(&grb.matcher);
	catcierge_grabber_destroy(&grb);

	return NULL;
}

#ifdef WITH_RFID
typedef struct rfid_test_conf_s
{
	const char *description;
	int check_rfid;
	const char *inner_path;
	const char *outer_path;
	int inner_valid_rfid;
	int outer_valid_rfid;
	match_direction_t direction;
} rfid_test_conf_t;

static char* run_rfid_tests(rfid_test_conf_t *conf)
{
	int i;
	catcierge_grb_t grb;
	catcierge_args_t *args;
	args = &grb.args;

	catcierge_grabber_init(&grb);

	args->saveimg = 0;
	args->match_time = 1; // Set this so that the RFID check is triggered.
	args->lock_on_invalid_rfid = conf->check_rfid;
	args->rfid_inner_path = conf->inner_path;
	args->rfid_outer_path = conf->outer_path;
	grb.rfid_in_match.is_allowed = conf->inner_valid_rfid;
	grb.rfid_out_match.is_allowed = conf->outer_valid_rfid;
	grb.rfid_direction = conf->direction;
	args->snout_paths[0] = CATCIERGE_SNOUT1_PATH;
	args->snout_count++;
	args->snout_paths[1] = CATCIERGE_SNOUT2_PATH;
	args->snout_count++;

	if (catcierge_template_matcher_init(&grb.matcher, args->snout_paths, args->snout_count))
	{
		return "Failed to init catcierge lib!\n";
	}

	catcierge_template_matcher_set_match_flipped(&grb.matcher, 1);
	catcierge_template_matcher_set_match_threshold(&grb.matcher, 0.8);

	catcierge_set_state(&grb, catcierge_state_waiting);

	// Do a normal match. But also make sure the RFID check is performed.
	// This is done by setting args->match_time, so that the rematch timer
	// will let the RFID check run.
	{
		// This is the initial image that obstructs the frame
		// and triggers the matching.
		load_test_image_and_run(&grb, 1, 1);
		mu_assert("Expected MATCHING state", (grb.state == catcierge_state_matching));

		// Match against 4 pictures, and decide the lockout status.
		for (i = 1; i <= 4; i++)
		{
			load_test_image_and_run(&grb, 1, i);
		}

		mu_assert("Expected KEEP OPEN state", (grb.state == catcierge_state_keepopen));

		// Give it a clear frame so that it will stop
		load_test_image_and_run(&grb, 1, 5);

		if (!conf->check_rfid)
		{
			mu_assert("Expected WAITING state", (grb.state == catcierge_state_keepopen));
		}
		else
		{
			if (conf->direction == MATCH_DIR_OUT
				|| (conf->inner_path && conf->inner_valid_rfid)
				|| (conf->outer_path && conf->outer_valid_rfid))
			{
				mu_assert("Expected WAITING state", (grb.state == catcierge_state_keepopen));
			}
			else
			{
				if (conf->inner_path || conf->outer_path)
				{
					mu_assert("Expected LOCKOUT state", (grb.state == catcierge_state_lockout));
				}
				else
				{
					mu_assert("Expected WAITING state", (grb.state == catcierge_state_keepopen));
				}
			}
		}
	}

	catcierge_template_matcher_destroy(&grb.matcher);
	catcierge_grabber_destroy(&grb);

	return NULL;
}
#endif // WITH_RFID

static char *run_failure_tests(int obstruct)
{
	catcierge_grb_t grb;
	catcierge_args_t *args;
	args = &grb.args;

	catcierge_grabber_init(&grb);

	args->saveimg = 0;
	set_default_test_snouts(args);

	if (catcierge_template_matcher_init(&grb.matcher, args->snout_paths, args->snout_count))
	{
		return "Failed to init catcierge lib!\n";
	}

	catcierge_template_matcher_set_match_flipped(&grb.matcher, 1);
	catcierge_template_matcher_set_match_threshold(&grb.matcher, 0.8);

	catcierge_set_state(&grb, catcierge_state_waiting);

	// Obstruct the frame to begin matching.
	load_test_image_and_run(&grb, 1, 2);
	mu_assert("Expected MATCHING state", (grb.state == catcierge_state_matching));

	// Pass 4 frames (first ok, the rest not...)
	load_test_image_and_run(&grb, 1, 2);
	mu_assert("Expected MATCHING state", (grb.state == catcierge_state_matching));

	load_test_image_and_run(&grb, 1, 3);
	mu_assert("Expected MATCHING state", (grb.state == catcierge_state_matching));

	load_test_image_and_run(&grb, 1, 4);
	mu_assert("Expected MATCHING state", (grb.state == catcierge_state_matching));

	load_test_image_and_run(&grb, 1, 4);
	mu_assert("Expected LOCKOUT state", (grb.state == catcierge_state_lockout));
	catcierge_test_STATUS("Lockout state as expected");

	catcierge_template_matcher_destroy(&grb.matcher);
	catcierge_grabber_destroy(&grb);

	return NULL;
}

//
// Tests passing 1 initial image that triggers matching.
// Then pass 4 images that should result in a successful match.
// Finally if "obstruct" is set, a single image is passed that obstructs the frame.
// After that we expect to go back to waiting.
//
static char *run_success_tests(int obstruct)
{
	int i;
	int j;
	catcierge_grb_t grb;
	catcierge_args_t *args;
	args = &grb.args;

	catcierge_grabber_init(&grb);

	args->saveimg = 0;
	args->snout_paths[0] = CATCIERGE_SNOUT1_PATH;
	args->snout_count++;
	args->snout_paths[1] = CATCIERGE_SNOUT2_PATH;
	args->snout_count++;

	if (catcierge_template_matcher_init(&grb.matcher, args->snout_paths, args->snout_count))
	{
		return "Failed to init catcierge lib!\n";
	}

	catcierge_template_matcher_set_match_flipped(&grb.matcher, 1);
	catcierge_template_matcher_set_match_threshold(&grb.matcher, 0.8);

	catcierge_set_state(&grb, catcierge_state_waiting);

	// Give the state machine a series of known images
	// and make sure the states are as expected.
	for (j = 1; j <= 5; j++)
	{
		catcierge_test_STATUS("Test series %d", j);

		// This is the initial image that obstructs the frame
		// and triggers the matching.
		load_test_image_and_run(&grb, j, 1);
		mu_assert("Expected MATCHING state", (grb.state == catcierge_state_matching));

		// Match against 4 pictures, and decide the lockout status.
		for (i = 1; i <= 4; i++)
		{
			load_test_image_and_run(&grb, j, i);
		}

		mu_assert("Expected KEEP OPEN state", (grb.state == catcierge_state_keepopen));

		if (obstruct)
		{
			// First obstruct the frame.
			load_test_image_and_run(&grb, j, 1);
			mu_assert("Expected KEEP OPEN state", (grb.state == catcierge_state_keepopen));

			// And then clear it.
			load_test_image_and_run(&grb, 1, 5);
			mu_assert("Expected WAITING state", (grb.state == catcierge_state_waiting));
		}
		else
		{
			// Give it a clear frame so that it will stop
			load_test_image_and_run(&grb, 1, 5);
			mu_assert("Expected WAITING state", (grb.state == catcierge_state_waiting));	
		}
	}

	catcierge_template_matcher_destroy(&grb.matcher);
	catcierge_grabber_destroy(&grb);

	return NULL;
}

void run_camera_test()
{
	catcierge_grb_t grb;
	memset(&grb, 0, sizeof(grb));
	catcierge_setup_camera(&grb);
	catcierge_destroy_camera(&grb);
}

int TEST_catcierge_fsm(int argc, char **argv)
{
	int ret = 0;
	char *e = NULL;

	catcierge_test_HEADLINE("TEST_catcierge_fsm");

	// Test without anything obstructing the frame after
	// the successful match.
	catcierge_test_HEADLINE("Run success tests. Without obstruct");
	if ((e = run_success_tests(0))) { catcierge_test_FAILURE(e); ret = -1; }
	else catcierge_test_SUCCESS("");

	// Same as above, but add an extra frame obstructing at the end.
	catcierge_test_HEADLINE("Run success tests. With obstruct");
	if ((e = run_success_tests(1))) { catcierge_test_FAILURE(e); ret = -1; }
	else catcierge_test_SUCCESS("Success match with obstruct");

	// Pass a set of images known to fail.
	catcierge_test_HEADLINE("Run failure tests.");
	if ((e = run_failure_tests(0))) { catcierge_test_FAILURE(e); ret = -1; }
	else catcierge_test_SUCCESS("Failure tests");

	// Trigger max consecutive lockout.
	catcierge_test_HEADLINE("Run consecutive lockout tests.");
	if ((e = run_consecutive_lockout_tests())) { catcierge_test_FAILURE(e); ret = -1; }
	else catcierge_test_SUCCESS("Consecuive lockout tests");

	// Trigger max consecutive lockout.
	catcierge_test_HEADLINE("Run consecutive lockout test. Reset counter");
	if ((e = run_consecutive_lockout_abort_tests())) { catcierge_test_FAILURE(e); ret = -1; }
	else catcierge_test_SUCCESS("Consecutive lockout test (with reset counter)");

	run_camera_test();

	#if WITH_RFID
	{
		int i;

		rfid_test_conf_t confs[] =
		{
			{"RFID Check off", 
				0, NULL, NULL, 0, 0, MATCH_DIR_IN},
			{"RFID Check on, no readers",
				1, NULL, NULL, 0, 0, MATCH_DIR_IN},

			// Inner.
			{"RFID Check on, inner reader, IN",
				1, "something", NULL, 0, 0, MATCH_DIR_IN},
			{"RFID Check on, inner reader, inner valid, IN",
				1, "something", NULL, 1, 0, MATCH_DIR_IN},
			{"RFID Check on, inner reader, inner & outer valid, IN",
				1, "something", NULL, 1, 1, MATCH_DIR_IN},
			{"RFID Check on, inner reader, OUT",
				1, "something", NULL, 0, 0, MATCH_DIR_OUT},
			{"RFID Check on, inner reader, inner valid, OUT",
				1, "something", NULL, 1, 0, MATCH_DIR_OUT},
			{"RFID Check on, inner reader, inner & outer valid, OUT",
				1, "something", NULL, 1, 1, MATCH_DIR_OUT},

			// Outer.
			{"RFID Check on, outer reader, IN",
				1, NULL, "something", 0, 0, MATCH_DIR_IN},
			{"RFID Check on, outer reader, inner valid, IN",
				1, NULL, "something", 1, 0, MATCH_DIR_IN},
			{"RFID Check on, outer reader, inner & outer valid, IN",
				1, NULL, "something", 1, 1, MATCH_DIR_IN},
			{"RFID Check on, outer reader, OUT",
				1, NULL, "something", 0, 0, MATCH_DIR_OUT},
			{"RFID Check on, outer reader, inner valid, OUT",
				1, NULL, "something", 1, 0, MATCH_DIR_OUT},
			{"RFID Check on, outer reader, inner & outer valid, OUT",
				1, NULL, "something", 1, 1, MATCH_DIR_OUT},

			// Inner + Outer.
			{"RFID Check on, inner & outer reader, IN",
				1, "something", "something", 0, 0, MATCH_DIR_IN},
			{"RFID Check on, inner & outer reader, inner valid, IN",
				1, "something", "something", 1, 0, MATCH_DIR_IN},
			{"RFID Check on, inner & outer reader, inner & outer valid, IN",
				1, "something", "something", 1, 1, MATCH_DIR_IN},
			{"RFID Check on, inner & outer reader, OUT",
				1, "something", "something", 0, 0, MATCH_DIR_OUT},
			{"RFID Check on, inner & outer reader, inner valid, OUT",
				1, "something", "something", 1, 0, MATCH_DIR_OUT},
			{"RFID Check on, inner & outer reader, inner & outer valid, OUT",
				1, "something", "something", 1, 1, MATCH_DIR_OUT},
		};

		#define RFID_TEST_COUNT sizeof(confs) / sizeof(rfid_test_conf_t)

		for (i = 0; i < RFID_TEST_COUNT; i++)
		{
			catcierge_test_HEADLINE(confs[i].description);
			if ((e = run_rfid_tests(&confs[i])))
			{
				catcierge_test_FAILURE(e);
				ret = -1;
			}
			else
			{
				catcierge_test_SUCCESS("RFID test (off)");
			}
		}
	}
	#endif // WITH_RFID

	return ret;
}
