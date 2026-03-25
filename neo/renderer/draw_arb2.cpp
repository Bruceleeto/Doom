/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 GPL Source Code ("Doom 3 Source Code").

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#include "sys/platform.h"
#include "renderer/VertexCache.h"

#include "renderer/tr_local.h"

/*
=========================================================================================

FFP INTERACTION RENDERING

=========================================================================================
*/

/*
==================
RB_ARB2_DrawInteraction
==================
*/
void	RB_ARB2_DrawInteraction( const drawInteraction_t *din ) {
	// FFP interaction: lightProjection * lightFalloff * diffuseTexture * diffuseColor
	//
	// Unit 0: light projection (projective texgen S, T, Q)
	// Unit 1: light falloff (texgen S, T fixed at 0.5)
	// Unit 2: diffuse map (vertex texcoords)
	// Vertex color: diffuseColor

	qglColor4fv( din->diffuseColor.ToFloatPtr() );

	// unit 0: light projection — projective texture
	GL_SelectTexture( 0 );
	din->lightImage->Bind();
	qglTexGenfv( GL_S, GL_OBJECT_PLANE, din->lightProjection[0].ToFloatPtr() );
	qglTexGenfv( GL_T, GL_OBJECT_PLANE, din->lightProjection[1].ToFloatPtr() );
	qglTexGenfv( GL_Q, GL_OBJECT_PLANE, din->lightProjection[2].ToFloatPtr() );

	// unit 1: light falloff — S from texgen, T fixed at 0.5
	static const float halfPlane[4] = { 0, 0, 0, 0.5f };
	GL_SelectTexture( 1 );
	din->lightFalloffImage->Bind();
	qglTexGenfv( GL_S, GL_OBJECT_PLANE, din->lightProjection[3].ToFloatPtr() );
	qglTexGenfv( GL_T, GL_OBJECT_PLANE, halfPlane );

	// unit 2: diffuse map — from vertex texcoords
	GL_SelectTexture( 2 );
	din->diffuseImage->Bind();

	RB_DrawElementsWithCounters( din->surf->geo );
}


/*
=============
RB_ARB2_CreateDrawInteractions
=============
*/
void RB_ARB2_CreateDrawInteractions( const drawSurf_t *surf ) {
	if ( !surf ) {
		return;
	}

	// additive blending for light accumulation
	GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHMASK | backEnd.depthFunc );

	// unit 0: light projection — projective texgen
	GL_SelectTexture( 0 );
	qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	qglEnable( GL_TEXTURE_GEN_S );
	qglEnable( GL_TEXTURE_GEN_T );
	qglEnable( GL_TEXTURE_GEN_Q );
	qglTexGeni( GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR );
	qglTexGeni( GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR );
	qglTexGeni( GL_Q, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR );
	GL_TexEnv( GL_MODULATE );

	// unit 1: light falloff — texgen S and T
	GL_SelectTexture( 1 );
	qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	qglEnable( GL_TEXTURE_GEN_S );
	qglEnable( GL_TEXTURE_GEN_T );
	qglTexGeni( GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR );
	qglTexGeni( GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR );
	GL_TexEnv( GL_MODULATE );

	// unit 2: diffuse map from vertex texcoords
	GL_SelectTexture( 2 );
	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	GL_TexEnv( GL_MODULATE );

	for ( ; surf ; surf=surf->nextOnLight ) {
		if ( !surf->geo || !surf->geo->ambientCache ) {
			continue;
		}

		idDrawVert *ac = (idDrawVert *)vertexCache.Position( surf->geo->ambientCache );
		qglVertexPointer( 3, GL_FLOAT, sizeof( idDrawVert ), ac->xyz.ToFloatPtr() );

		GL_SelectTexture( 2 );
		qglTexCoordPointer( 2, GL_FLOAT, sizeof( idDrawVert ), ac->st.ToFloatPtr() );

		RB_CreateSingleDrawInteractions( surf, RB_ARB2_DrawInteraction );
	}

	// tear down
	GL_SelectTexture( 2 );
	qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	globalImages->BindNull();

	GL_SelectTexture( 1 );
	qglDisable( GL_TEXTURE_GEN_S );
	qglDisable( GL_TEXTURE_GEN_T );
	globalImages->BindNull();

	GL_SelectTexture( 0 );
	qglDisable( GL_TEXTURE_GEN_S );
	qglDisable( GL_TEXTURE_GEN_T );
	qglDisable( GL_TEXTURE_GEN_Q );
	globalImages->BindNull();
}


/*
==================
RB_ARB2_DrawInteractions
==================
*/
void RB_ARB2_DrawInteractions( void ) {
	viewLight_t		*vLight;

	GL_SelectTexture( 0 );
	qglDisableClientState( GL_TEXTURE_COORD_ARRAY );

	for ( vLight = backEnd.viewDef->viewLights ; vLight ; vLight = vLight->next ) {
		backEnd.vLight = vLight;

		if ( vLight->lightShader->IsFogLight() ) {
			continue;
		}
		if ( vLight->lightShader->IsBlendLight() ) {
			continue;
		}

		if ( !vLight->localInteractions && !vLight->globalInteractions
			&& !vLight->translucentInteractions ) {
			continue;
		}

		// clear the stencil buffer if needed
		if ( vLight->globalShadows || vLight->localShadows ) {
			backEnd.currentScissor = vLight->scissorRect;
			if ( r_useScissor.GetBool() ) {
				qglScissor( backEnd.viewDef->viewport.x1 + backEnd.currentScissor.x1,
					backEnd.viewDef->viewport.y1 + backEnd.currentScissor.y1,
					backEnd.currentScissor.x2 + 1 - backEnd.currentScissor.x1,
					backEnd.currentScissor.y2 + 1 - backEnd.currentScissor.y1 );
			}
			qglClear( GL_STENCIL_BUFFER_BIT );
		} else {
			qglStencilFunc( GL_ALWAYS, 128, 255 );
		}

		RB_StencilShadowPass( vLight->globalShadows );
		RB_ARB2_CreateDrawInteractions( vLight->localInteractions );
		RB_StencilShadowPass( vLight->localShadows );
		RB_ARB2_CreateDrawInteractions( vLight->globalInteractions );

		// translucent surfaces never get stencil shadowed
		if ( r_skipTranslucent.GetBool() ) {
			continue;
		}

		qglStencilFunc( GL_ALWAYS, 128, 255 );

		backEnd.depthFunc = GLS_DEPTHFUNC_LESS;
		RB_ARB2_CreateDrawInteractions( vLight->translucentInteractions );

		backEnd.depthFunc = GLS_DEPTHFUNC_EQUAL;
	}

	// disable stencil shadow test
	qglStencilFunc( GL_ALWAYS, 128, 255 );

	GL_SelectTexture( 0 );
	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
}

//===================================================================================

/*
==================
Stubs for removed ARB program subsystem.
R_FindARBProgram must return non-zero so materials with custom programs
still get their newStage allocated (which causes them to be skipped
rather than drawn incorrectly as old-style stages).
==================
*/
void R_ARB2_Init( void ) {
	glConfig.allowARB2Path = true;
}

void R_ReloadARBPrograms_f( const idCmdArgs &args ) {
}

static int dummyProgramCounter = 1;

int R_FindARBProgram( GLenum target, const char *program ) {
	return dummyProgramCounter++;
}
