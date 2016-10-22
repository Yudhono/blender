/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/manipulators/intern/manipulator_library/dial_manipulator.c
 *  \ingroup wm
 *
 * \name Dial Manipulator
 *
 * 3D Manipulator
 *
 * \brief Circle shaped manipulator for circular interaction.
 * Currently no own handling, use with operator only.
 */

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "BKE_context.h"

#include "BLI_math.h"

#include "ED_screen.h"
#include "ED_view3d.h"

#include "GPU_immediate.h"
#include "GPU_select.h"

#include "MEM_guardedalloc.h"

#include "WM_api.h"
#include "WM_types.h"

/* own includes */
#include "WM_manipulator_types.h"
#include "WM_manipulator_library.h"
#include "wm_manipulator_wmapi.h"
#include "wm_manipulator_intern.h"
#include "manipulator_geometry.h"
#include "manipulator_library_intern.h"


typedef struct DialManipulator {
	wmManipulator manipulator;
	int style;
	float direction[3];
} DialManipulator;

typedef struct DialInteraction {
	float init_mval[2];

	/* cache the last angle to detect rotations bigger than -/+ PI */
	float last_angle;
	/* number of full rotations */
	int rotations;
} DialInteraction;

#define DIAL_WIDTH       1.0f
#define DIAL_RESOLUTION 32


/* -------------------------------------------------------------------- */

static void dial_geom_draw(
        const DialManipulator *dial, const float mat[4][4], const float clipping_plane[3], const float col[4])
{
	const bool clipped = (dial->style == MANIPULATOR_DIAL_STYLE_RING_CLIPPED);
	const bool filled = (dial->style == MANIPULATOR_DIAL_STYLE_RING_FILLED);
	const unsigned int pos = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 3, KEEP_FLOAT);

	glLineWidth(dial->manipulator.line_width);

	immBindBuiltinProgram(clipped ? GPU_SHADER_3D_CLIPPED_UNIFORM_COLOR : GPU_SHADER_3D_UNIFORM_COLOR);
	immUniformColor4fv(col);

	if (clipped) {
		glEnable(GL_CLIP_DISTANCE0);
		immUniform4fv("ClipPlane", clipping_plane);
		immUniformMat4("ModelMatrix", mat);
	}

	if (filled) {
		/* TODO */
	}
	else {
		imm_draw_lined_circle_3D(pos, 0.0f, 0.0f, DIAL_WIDTH, DIAL_RESOLUTION);
	}

	immUnbindProgram();
}

static void dial_clipping_plane_get(DialManipulator *dial, RegionView3D *rv3d, float r_clipping_plane[3])
{
	copy_v3_v3(r_clipping_plane, rv3d->viewinv[2]);
	r_clipping_plane[3] = -dot_v3v3(rv3d->viewinv[2], dial->manipulator.origin);
}

static void dial_matrix_get(DialManipulator *dial, float r_mat[4][4])
{
	const float up[3] = {0.0f, 0.0f, 1.0f};
	float rot[3][3];

	rotation_between_vecs_to_mat3(rot, up, dial->direction);
	copy_m4_m3(r_mat, rot);
	copy_v3_v3(r_mat[3], dial->manipulator.origin);
	mul_mat3_m4_fl(r_mat, dial->manipulator.scale);
}

static void dial_draw_intern(const bContext *C, DialManipulator *dial, const bool highlight)
{
	const bool use_clipping = (dial->style == MANIPULATOR_DIAL_STYLE_RING_CLIPPED);
	float clipping_plane[3];
	float mat[4][4];
	float col[4];

	BLI_assert(CTX_wm_area(C)->spacetype == SPACE_VIEW3D);

	/* get all data we need */
	manipulator_color_get(&dial->manipulator, highlight, col);
	dial_matrix_get(dial, mat);
	if (use_clipping) {
		dial_clipping_plane_get(dial, CTX_wm_region_view3d(C), clipping_plane);
	}

	glPushMatrix();
	glMultMatrixf(mat);

	/* draw actual dial manipulator */
	dial_geom_draw(dial, mat, clipping_plane, col);

	glPopMatrix();
}

static void manipulator_dial_render_3d_intersect(const bContext *C, wmManipulator *manipulator, int selectionbase)
{
	DialManipulator *dial = (DialManipulator *)manipulator;

	GPU_select_load_id(selectionbase);
	dial_draw_intern(C, dial, false);
}

static void manipulator_dial_draw(const bContext *C, wmManipulator *manipulator)
{
	DialManipulator *dial = (DialManipulator *)manipulator;

	glEnable(GL_BLEND);
	dial_draw_intern(C, dial, (manipulator->state & WM_MANIPULATOR_HIGHLIGHT) != 0);
	glDisable(GL_BLEND);
}

static int manipulator_dial_invoke(bContext *UNUSED(C), const wmEvent *event, wmManipulator *manipulator)
{
	DialInteraction *inter = MEM_callocN(sizeof(DialInteraction), __func__);

	inter->init_mval[0] = event->mval[0];
	inter->init_mval[1] = event->mval[1];

	manipulator->interaction_data = inter;

	return OPERATOR_RUNNING_MODAL;
}


/* -------------------------------------------------------------------- */
/** \name Dial Manipulator API
 *
 * \{ */

wmManipulator *WM_dial_manipulator_new(wmManipulatorGroup *mgroup, const char *name, const int style)
{
	DialManipulator *dial = MEM_callocN(sizeof(DialManipulator), name);
	const float dir_default[3] = {0.0f, 0.0f, 1.0f};

	dial->manipulator.draw = manipulator_dial_draw;
	dial->manipulator.intersect = NULL;
	dial->manipulator.render_3d_intersection = manipulator_dial_render_3d_intersect;
	dial->manipulator.invoke = manipulator_dial_invoke;

	dial->style = style;

	/* defaults */
	copy_v3_v3(dial->direction, dir_default);

	wm_manipulator_register(mgroup, &dial->manipulator, name);

	return (wmManipulator *)dial;
}

/**
 * Define up-direction of the dial manipulator
 */
void WM_dial_manipulator_set_up_vector(wmManipulator *manipulator, const float direction[3])
{
	DialManipulator *dial = (DialManipulator *)manipulator;

	copy_v3_v3(dial->direction, direction);
	normalize_v3(dial->direction);
}

/** \} */ // Dial Manipulator API


/* -------------------------------------------------------------------- */

void fix_linking_manipulator_dial(void)
{
	(void)0;
}
