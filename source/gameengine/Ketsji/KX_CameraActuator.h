/**
 * KX_CameraActuator.h
 *
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef __KX_CAMERAACTUATOR
#define __KX_CAMERAACTUATOR

#include "SCA_IActuator.h"
#include "MT_Scalar.h"

/**
 * The camera actuator does a Robbie Muller prespective for you. This is a 
 * weird set of rules that positions the camera sort of behind the object,
 * tracking, while avoiding any objects between the 'ideal' position and the
 * actor being tracked.
 */


class KX_CameraActuator : public SCA_IActuator
{

private :
	Py_Header;
	
	/** Object that will be tracked. */
	const CValue *m_ob;

	/** height (float), */
	const MT_Scalar m_height;
	
	/** min (float), */
	const MT_Scalar m_minHeight;
	
	/** max (float), */
	const MT_Scalar m_maxHeight;
	
	/** xy toggle (pick one): true == x, false == y */
	bool m_x;

	/* get the KX_IGameObject with this name */
	CValue *findObject(char *obName);

	/* parse x or y to a toggle pick */
	bool string2axischoice(const char *axisString);
	
 public:
	static STR_String X_AXIS_STRING;
	static STR_String Y_AXIS_STRING;
	
	/**
	 * Set the bool toggle to true to use x lock, false for y lock
	 */
	KX_CameraActuator(

		SCA_IObject *gameobj,
		const CValue *ob,
		MT_Scalar hght,
		MT_Scalar minhght,
		MT_Scalar maxhght,
		bool xytog,
		PyTypeObject* T=&Type

	);


	~KX_CameraActuator();



	/** Methods Inherited from  CValue */


	CValue* GetReplica();
	

	/** Methods inherited from SCA_IActuator */


	bool Update(

		double curtime,

		double deltatime

	);


	/* --------------------------------------------------------------------- */
	/* Python interface ---------------------------------------------------- */
	/* --------------------------------------------------------------------- */

	virtual PyObject*  _getattr(char *attr);

	

};

#endif //__KX_CAMERAACTUATOR

