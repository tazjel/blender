/**
 * $Id$
 *
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
 * Contributor(s): Mitchell Stokes.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file BL_Action.h
 *  \ingroup ketsji
 */

#ifndef __BL_ACTION
#define __BL_ACTION


#include <vector>

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif


class BL_Action
{
private:
	struct bAction* m_action;
	struct bPose* m_pose;
	struct bPose* m_blendpose;
	struct bPose* m_blendinpose;
	struct PointerRNA *m_ptrrna;
	class SG_Controller *m_sg_contr;
	class KX_GameObject* m_obj;
	std::vector<float>	m_blendshape;
	std::vector<float>	m_blendinshape;

	float m_startframe;
	float m_endframe;
	float m_starttime;
	float m_endtime;
	float m_localtime;

	float m_blendin;
	float m_blendframe;
	float m_blendstart;

	float m_layer_weight;

	float m_speed;

	short m_priority;

	short m_playmode;

	short m_ipo_flags;

	bool m_done;
	bool m_calc_localtime;

	void InitIPO();
	void SetLocalTime(float curtime);
	void ResetStartTime(float curtime);
	void IncrementBlending(float curtime);
	void BlendShape(struct Key* key, float srcweight, std::vector<float>& blendshape);
public:
	BL_Action(class KX_GameObject* gameobj);
	~BL_Action();

	/**
	 * Play an action
	 */
	bool Play(const char* name,
			float start,
			float end,
			short priority,
			float blendin,
			short play_mode,
			float layer_weight,
			short ipo_flags,
			float playback_speed);
	/**
	 * Stop playing the action
	 */
	void Stop();
	/**
	 * Whether or not the action is still playing
	 */
	bool IsDone();
	/**
	 * Update the action's frame, etc.
	 */
	void Update(float curtime);

	// Accessors
	float GetFrame();
	struct bAction *GetAction();

	// Mutators
	void SetFrame(float frame);
	void SetPlayMode(short play_mode);
	void SetTimes(float start, float end);

	enum 
	{
		ACT_MODE_PLAY = 0,
		ACT_MODE_LOOP,
		ACT_MODE_PING_PONG,
		ACT_MODE_MAX,
	};

	enum
	{
		ACT_IPOFLAG_FORCE = 1,
		ACT_IPOFLAG_LOCAL = 2,
		ACT_IPOFLAG_ADD = 4,
		ACT_IPOFLAG_CHILD = 8,
	};

#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GE:BL_Action"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif //BL_ACTION

