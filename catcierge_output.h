//
// This file is part of the Catcierge project.
//
// Copyright (c) Joakim Soderberg 2013-2014
//
//    Catcierge is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 2 of the License, or
//    (at your option) any later version.
//
//    Catcierge is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with Catcierge.  If not, see <http://www.gnu.org/licenses/>.
//
#ifndef __CATCIERGE_OUTPUT_H__
#define __CATCIERGE_OUTPUT_H__

#include <stdio.h>
#include "catcierge_fsm.h"

typedef struct catcierge_output_var_s
{
	char *name;
	char *description;
} catcierge_output_var_t;

typedef struct catcierge_output_template_s
{
	char *tmpl;
	char *target_path;
} catcierge_output_template_t;

typedef struct catcierge_output_s
{
	catcierge_output_template_t *templates;
	size_t template_count;
	size_t template_max_count;
} catcierge_output_t;

int catcierge_output_validate(catcierge_output_t *ctx,
	catcierge_grb_t *grb, const char *template_str);

void catcierge_output_print_usage();

int catcierge_output_init(catcierge_output_t *ctx);
void catcierge_output_destroy(catcierge_output_t *ctx);

int catcierge_output_add_template(catcierge_output_t *ctx, const char *template_str, const char *target_path);
char *catcierge_output_generate(catcierge_output_t *ctx, catcierge_grb_t *grb, const char *template_str);
int catcierge_output_generate_templates(catcierge_output_t *ctx, catcierge_grb_t *grb);

#endif // __CATCIERGE_OUTPUT_H__
