/**
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): snailrose.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef DISABLE_SDL
#include <SDL.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include "SCA_Joystick.h"
#include "SCA_JoystickPrivate.h"

SCA_Joystick::SCA_Joystick(short int index)
	:
	m_joyindex(index),
	m_prec(3200),
	m_buttonnum(-2),
	m_axismax(-1),
	m_buttonmax(-1),
	m_hatmax(-1),
	m_isinit(0),
	m_istrig_axis(0),
	m_istrig_button(0),
	m_istrig_hat(0)
{
	for(int i=0; i<JOYAXIS_MAX; i++)
		m_axis_array[i]= 0;
	
	for(int i=0; i<JOYHAT_MAX; i++)
		m_hat_array[i]= 0;
	
#ifndef DISABLE_SDL
	m_private = new PrivateData();
#endif
}


SCA_Joystick::~SCA_Joystick()

{
#ifndef DISABLE_SDL
	delete m_private;
#endif
}

SCA_Joystick *SCA_Joystick::m_instance[JOYINDEX_MAX];
int SCA_Joystick::m_refCount = 0;

SCA_Joystick *SCA_Joystick::GetInstance( short int joyindex )
{
#ifdef DISABLE_SDL
	return NULL;
#else
	if (joyindex < 0 || joyindex >= JOYINDEX_MAX) {
		echo("Error-invalid joystick index: " << joyindex);
		return NULL;
	}

	if (m_refCount == 0) 
	{
		int i;
		// do this once only
		if(SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_VIDEO ) == -1 ){
			echo("Error-Initializing-SDL: " << SDL_GetError());
			return NULL;
		}
		for (i=0; i<JOYINDEX_MAX; i++) {
			m_instance[i] = new SCA_Joystick(i);
			m_instance[i]->CreateJoystickDevice();
		}
		m_refCount = 1;
	}
	else
	{
		m_refCount++;
	}
	return m_instance[joyindex];
#endif
}

void SCA_Joystick::ReleaseInstance()
{
	if (--m_refCount == 0)
	{
#ifndef DISABLE_SDL
		int i;
		for (i=0; i<JOYINDEX_MAX; i++) {
			if (m_instance[i]) {
				m_instance[i]->DestroyJoystickDevice();
				delete m_instance[i];
			}
			m_instance[i]= NULL;
		}

		SDL_QuitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_VIDEO );
#endif
	}
}

void SCA_Joystick::cSetPrecision(int val)
{
	m_prec = val;
}


bool SCA_Joystick::aAxisPairIsPositive(int axis)
{
	return (pAxisTest(axis) > m_prec) ? true:false;
}

bool SCA_Joystick::aAxisPairDirectionIsPositive(int axis, int dir)
{

	int res;

	if (dir==JOYAXIS_UP || dir==JOYAXIS_DOWN)
		res = pGetAxis(axis, 1);
	else /* JOYAXIS_LEFT || JOYAXIS_RIGHT */
		res = pGetAxis(axis, 0);
	
	if (dir==JOYAXIS_DOWN || dir==JOYAXIS_RIGHT)
		return (res > m_prec) ? true : false;
	else /* JOYAXIS_UP || JOYAXIS_LEFT */
		return (res < -m_prec) ? true : false;
}

bool SCA_Joystick::aAxisIsPositive(int axis_single)
{
	return abs(m_axis_array[axis_single]) > m_prec ? true:false;
}

bool SCA_Joystick::aAnyButtonPressIsPositive(void)
{
	return (m_buttonnum==-2) ? false : true;
}

bool SCA_Joystick::aAnyButtonReleaseIsPositive(void)
{
	return (m_buttonnum==-2) ? true : false;
}

bool SCA_Joystick::aButtonPressIsPositive(int button)
{
#ifdef DISABLE_SDL
	return false;
#else
	bool result;
	SDL_JoystickGetButton(m_private->m_joystick, button)? result = true:result = false;
	return result;
#endif
}


bool SCA_Joystick::aButtonReleaseIsPositive(int button)
{
#ifdef DISABLE_SDL
	return false;
#else
	bool result;
	SDL_JoystickGetButton(m_private->m_joystick, button)? result = false : result = true;
	return result;
#endif
}


bool SCA_Joystick::aHatIsPositive(int hatnum, int dir)
{
	return (GetHat(hatnum)==dir) ? true : false;
}

int SCA_Joystick::GetNumberOfAxes()
{
	return m_axismax;
}


int SCA_Joystick::GetNumberOfButtons()
{
	return m_buttonmax;
}


int SCA_Joystick::GetNumberOfHats()
{
	return m_hatmax;
}

bool SCA_Joystick::CreateJoystickDevice(void)
{
#ifdef DISABLE_SDL
	m_isinit = true;
	m_axismax = m_buttonmax = m_hatmax = 0;
	return false;
#else
	if(m_isinit == false){
		if (m_joyindex>=SDL_NumJoysticks()) {
			// don't print a message, because this is done anyway
			//echo("Joystick-Error: " << SDL_NumJoysticks() << " avaiable joystick(s)");
			
			// Need this so python args can return empty lists
			m_axismax = m_buttonmax = m_hatmax = 0;
			return false;
		}

		m_private->m_joystick = SDL_JoystickOpen(m_joyindex);
		SDL_JoystickEventState(SDL_ENABLE);
		m_isinit = true;
		
		echo("Joystick " << m_joyindex << " initialized");
		
		/* must run after being initialized */
		m_axismax =		SDL_JoystickNumAxes(m_private->m_joystick);
		m_buttonmax =	SDL_JoystickNumButtons(m_private->m_joystick);
		m_hatmax =		SDL_JoystickNumHats(m_private->m_joystick);
		
		if (m_axismax > JOYAXIS_MAX) m_axismax= JOYAXIS_MAX;		/* very unlikely */
		else if (m_axismax < 0) m_axismax = 0;
		
		if (m_hatmax > JOYHAT_MAX) m_hatmax= JOYHAT_MAX;			/* very unlikely */
		else if(m_hatmax<0) m_hatmax= 0;
		
		if(m_buttonmax<0) m_buttonmax= 0;
		
	}
	return true;
#endif
}


void SCA_Joystick::DestroyJoystickDevice(void)
{
#ifndef DISABLE_SDL
	if (m_isinit){
		if(SDL_JoystickOpened(m_joyindex)){
			echo("Closing-joystick " << m_joyindex);
			SDL_JoystickClose(m_private->m_joystick);
		}
		m_isinit = false;
	}
#endif
}

int SCA_Joystick::Connected(void)
{
#ifndef DISABLE_SDL
	if (m_isinit && SDL_JoystickOpened(m_joyindex))
		return 1;
#endif
	return 0;
}

int SCA_Joystick::pGetAxis(int axisnum, int udlr)
{
#ifndef DISABLE_SDL
	return m_axis_array[(axisnum*2)+udlr];
#endif
	return 0;
}

int SCA_Joystick::pAxisTest(int axisnum)
{
#ifndef DISABLE_SDL
	short i1= m_axis_array[(axisnum*2)];
	short i2= m_axis_array[(axisnum*2)+1];
	
	/* long winded way to do
	 *   return MAX2(abs(i1), abs(i2))
	 * avoid abs from math.h */
	if (i1 < 0) i1 = -i1;
	if (i2 < 0) i2 = -i2;
	if (i1 <i2) return i2;
	else		return i1;
#else
	return 0;
#endif
}
