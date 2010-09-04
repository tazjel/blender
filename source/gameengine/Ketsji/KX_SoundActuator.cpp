/**
 * KX_SoundActuator.cpp
 *
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

#include "KX_SoundActuator.h"
#include "AUD_C-API.h"
#include "KX_GameObject.h"
#include "KX_PyMath.h" // needed for PyObjectFrom()
#include <iostream>

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */
KX_SoundActuator::KX_SoundActuator(SCA_IObject* gameobj,
								   AUD_Sound* sound,
								   float volume,
								   float pitch,
								   bool is3d,
								   KX_3DSoundSettings settings,
								   KX_SOUNDACT_TYPE type)//,
								   : SCA_IActuator(gameobj, KX_ACT_SOUND)
{
	m_sound = sound;
	m_volume = volume;
	m_pitch = pitch;
	m_is3d = is3d;
	m_3d = settings;
	m_handle = NULL;
	m_type = type;
	m_isplaying = false;
}



KX_SoundActuator::~KX_SoundActuator()
{
	if(m_handle)
		AUD_stop(m_handle);
}

void KX_SoundActuator::play()
{
	if(m_handle)
		AUD_stop(m_handle);

	if(!m_sound)
		return;

	// this is the sound that will be played and not deleted afterwards
	AUD_Sound* sound = m_sound;
	// this sound is for temporary stacked sounds, will be deleted if not NULL
	AUD_Sound* sound2 = NULL;

	bool loop = false;

	switch (m_type)
	{
	case KX_SOUNDACT_LOOPBIDIRECTIONAL:
	case KX_SOUNDACT_LOOPBIDIRECTIONAL_STOP:
		sound = sound2 = AUD_pingpongSound(sound);
		// fall through
	case KX_SOUNDACT_LOOPEND:
	case KX_SOUNDACT_LOOPSTOP:
		loop = true;
		break;
	case KX_SOUNDACT_PLAYSTOP:
	case KX_SOUNDACT_PLAYEND:
	default:
		break;
	}

	if(m_is3d)
	{
		// sound shall be played 3D
		m_handle = AUD_play(sound, 0);

		AUD_setRelative(m_handle, false);
		AUD_setVolumeMaximum(m_handle, m_3d.max_gain);
		AUD_setVolumeMinimum(m_handle, m_3d.min_gain);
		AUD_setDistanceReference(m_handle, m_3d.reference_distance);
		AUD_setDistanceMaximum(m_handle, m_3d.max_distance);
		AUD_setAttenuation(m_handle, m_3d.rolloff_factor);
		AUD_setConeAngleInner(m_handle, m_3d.cone_inner_angle);
		AUD_setConeAngleOuter(m_handle, m_3d.cone_outer_angle);
		AUD_setConeVolumeOuter(m_handle, m_3d.cone_outer_gain);
	}
	else
		m_handle = AUD_play(sound, 0);

	if(loop)
		AUD_setLoop(m_handle, -1);
	AUD_setSoundPitch(m_handle, m_pitch);
	AUD_setSoundVolume(m_handle, m_volume);
	m_isplaying = true;

	if(sound2)
		AUD_unload(sound2);
}

CValue* KX_SoundActuator::GetReplica()
{
	KX_SoundActuator* replica = new KX_SoundActuator(*this);
	replica->ProcessReplica();
	return replica;
};

void KX_SoundActuator::ProcessReplica()
{
	SCA_IActuator::ProcessReplica();
	m_handle = 0;
}

bool KX_SoundActuator::Update(double curtime, bool frame)
{
	if (!frame)
		return true;
	bool result = false;

	// do nothing on negative events, otherwise sounds are played twice!
	bool bNegativeEvent = IsNegativeEvent();
	bool bPositiveEvent = m_posevent;
	
	RemoveAllEvents();

	if(!m_sound)
		return false;

	// actual audio device playing state
	bool isplaying = AUD_getStatus(m_handle) == AUD_STATUS_PLAYING;

	if (bNegativeEvent)
	{
		// here must be a check if it is still playing
		if (m_isplaying && isplaying)
		{
			switch (m_type)
			{
			case KX_SOUNDACT_PLAYSTOP:
			case KX_SOUNDACT_LOOPSTOP:
			case KX_SOUNDACT_LOOPBIDIRECTIONAL_STOP:
				{
					// stop immediately
					AUD_stop(m_handle);
					break;
				}
			case KX_SOUNDACT_PLAYEND:
				{
					// do nothing, sound will stop anyway when it's finished
					break;
				}
			case KX_SOUNDACT_LOOPEND:
			case KX_SOUNDACT_LOOPBIDIRECTIONAL:
				{
					// stop the looping so that the sound stops when it finished
					AUD_setLoop(m_handle, 0);
					break;
				}
			default:
				// implement me !!
				break;
			}
		}
		// remember that we tried to stop the actuator
		m_isplaying = false;
	}
	
#if 1
	// Warning: when de-activating the actuator, after a single negative event this runs again with...
	// m_posevent==false && m_posevent==false, in this case IsNegativeEvent() returns false 
	// and assumes this is a positive event.
	// check that we actually have a positive event so as not to play sounds when being disabled.
	else if(bPositiveEvent) { // <- added since 2.49
#else
	else {	// <- works in most cases except a loop-end sound will never stop unless
			// the negative pulse is done continuesly
#endif
		if (!m_isplaying)
			play();
	}
	// verify that the sound is still playing
	isplaying = AUD_getStatus(m_handle) == AUD_STATUS_PLAYING ? true : false;

	if (isplaying)
	{
		if(m_is3d)
		{
			KX_GameObject* obj = (KX_GameObject*)this->GetParent();
			float f[4];

			obj->NodeGetWorldPosition().getValue(f);
			AUD_setSourceLocation(m_handle, f);
			obj->GetLinearVelocity().getValue(f);
			AUD_setSourceVelocity(m_handle, f);
			obj->NodeGetWorldOrientation().getRotation().getValue(f);
			AUD_setSourceOrientation(m_handle, f);
		}
		result = true;
	}
	else
	{
		m_isplaying = false;
		result = false;
	}
	return result;
}


#ifndef DISABLE_PYTHON

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */



/* Integration hooks ------------------------------------------------------- */
PyTypeObject KX_SoundActuator::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_SoundActuator",
	sizeof(PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0,0,0,0,0,0,0,0,0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0,0,0,0,0,0,0,
	Methods,
	0,
	0,
	&SCA_IActuator::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef KX_SoundActuator::Methods[] = {
	KX_PYMETHODTABLE_NOARGS(KX_SoundActuator, startSound),
	KX_PYMETHODTABLE_NOARGS(KX_SoundActuator, pauseSound),
	KX_PYMETHODTABLE_NOARGS(KX_SoundActuator, stopSound),
	{NULL, NULL} //Sentinel
};

PyAttributeDef KX_SoundActuator::Attributes[] = {
	KX_PYATTRIBUTE_BOOL_RO("is3D", KX_SoundActuator, m_is3d),
	KX_PYATTRIBUTE_RW_FUNCTION("volume_maximum", KX_SoundActuator, pyattr_get_3d_property, pyattr_set_3d_property),
	KX_PYATTRIBUTE_RW_FUNCTION("volume_minimum", KX_SoundActuator, pyattr_get_3d_property, pyattr_set_3d_property),
	KX_PYATTRIBUTE_RW_FUNCTION("distance_reference", KX_SoundActuator, pyattr_get_3d_property, pyattr_set_3d_property),
	KX_PYATTRIBUTE_RW_FUNCTION("distance_maximum", KX_SoundActuator, pyattr_get_3d_property, pyattr_set_3d_property),
	KX_PYATTRIBUTE_RW_FUNCTION("attenuation", KX_SoundActuator, pyattr_get_3d_property, pyattr_set_3d_property),
	KX_PYATTRIBUTE_RW_FUNCTION("cone_angle_inner", KX_SoundActuator, pyattr_get_3d_property, pyattr_set_3d_property),
	KX_PYATTRIBUTE_RW_FUNCTION("cone_angle_outer", KX_SoundActuator, pyattr_get_3d_property, pyattr_set_3d_property),
	KX_PYATTRIBUTE_RW_FUNCTION("cone_volume_outer", KX_SoundActuator, pyattr_get_3d_property, pyattr_set_3d_property),

	KX_PYATTRIBUTE_RW_FUNCTION("time", KX_SoundActuator, pyattr_get_audposition, pyattr_set_audposition),
	KX_PYATTRIBUTE_RW_FUNCTION("volume", KX_SoundActuator, pyattr_get_gain, pyattr_set_gain),
	KX_PYATTRIBUTE_RW_FUNCTION("pitch", KX_SoundActuator, pyattr_get_pitch, pyattr_set_pitch),
	KX_PYATTRIBUTE_ENUM_RW("mode",KX_SoundActuator::KX_SOUNDACT_NODEF+1,KX_SoundActuator::KX_SOUNDACT_MAX-1,false,KX_SoundActuator,m_type),
	{ NULL }	//Sentinel
};

/* Methods ----------------------------------------------------------------- */
KX_PYMETHODDEF_DOC_NOARGS(KX_SoundActuator, startSound,
"startSound()\n"
"\tStarts the sound.\n")
{
	switch(AUD_getStatus(m_handle))
	{
	case AUD_STATUS_PLAYING:
		break;
	case AUD_STATUS_PAUSED:
		AUD_resume(m_handle);
		break;
	default:
		play();
	}
	Py_RETURN_NONE;
}

KX_PYMETHODDEF_DOC_NOARGS(KX_SoundActuator, pauseSound,
"pauseSound()\n"
"\tPauses the sound.\n")
{
	AUD_pause(m_handle);
	Py_RETURN_NONE;
}

KX_PYMETHODDEF_DOC_NOARGS(KX_SoundActuator, stopSound,
"stopSound()\n"
"\tStops the sound.\n")
{
	AUD_stop(m_handle);
	Py_RETURN_NONE;
}

/* Atribute setting and getting -------------------------------------------- */
PyObject* KX_SoundActuator::pyattr_get_3d_property(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_SoundActuator * actuator = static_cast<KX_SoundActuator *> (self);
	const char* prop = attrdef->m_name;
	float result_value = 0.0;

	if(!strcmp(prop, "volume_maximum")) {
		result_value = actuator->m_3d.max_gain;

	} else if (!strcmp(prop, "volume_minimum")) {
		result_value = actuator->m_3d.min_gain;

	} else if (!strcmp(prop, "distance_reference")) {
		result_value = actuator->m_3d.reference_distance;

	} else if (!strcmp(prop, "distance_maximum")) {
		result_value = actuator->m_3d.max_distance;

	} else if (!strcmp(prop, "attenuation")) {
		result_value = actuator->m_3d.rolloff_factor;

	} else if (!strcmp(prop, "cone_angle_inner")) {
		result_value = actuator->m_3d.cone_inner_angle;

	} else if (!strcmp(prop, "cone_angle_outer")) {
		result_value = actuator->m_3d.cone_outer_angle;

	} else if (!strcmp(prop, "cone_volume_outer")) {
		result_value = actuator->m_3d.cone_outer_gain;

	} else {
		Py_RETURN_NONE;
	}

	PyObject* result = PyFloat_FromDouble(result_value);
	return result;
}

PyObject* KX_SoundActuator::pyattr_get_audposition(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_SoundActuator * actuator = static_cast<KX_SoundActuator *> (self);
	float position = 0.0;

	if(actuator->m_handle)
		position = AUD_getPosition(actuator->m_handle);

	PyObject* result = PyFloat_FromDouble(position);

	return result;
}

PyObject* KX_SoundActuator::pyattr_get_gain(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_SoundActuator * actuator = static_cast<KX_SoundActuator *> (self);
	float gain = actuator->m_volume;

	PyObject* result = PyFloat_FromDouble(gain);

	return result;
}

PyObject* KX_SoundActuator::pyattr_get_pitch(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_SoundActuator * actuator = static_cast<KX_SoundActuator *> (self);
	float pitch = actuator->m_pitch;

	PyObject* result = PyFloat_FromDouble(pitch);

	return result;
}

int KX_SoundActuator::pyattr_set_3d_property(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_SoundActuator * actuator = static_cast<KX_SoundActuator *> (self);
	const char* prop = attrdef->m_name;
	float prop_value = 0.0;

	if (!PyArg_Parse(value, "f", &prop_value))
		return PY_SET_ATTR_FAIL;

	// if sound is working and 3D, set the new setting
	if(!actuator->m_is3d)
		return PY_SET_ATTR_FAIL;

	if(!strcmp(prop, "volume_maximum")) {
		actuator->m_3d.max_gain = prop_value;
		if(actuator->m_handle)
			AUD_setVolumeMaximum(actuator->m_handle, prop_value);

	} else if (!strcmp(prop, "volume_minimum")) {
		actuator->m_3d.min_gain = prop_value;
		if(actuator->m_handle)
			AUD_setVolumeMinimum(actuator->m_handle, prop_value);

	} else if (!strcmp(prop, "distance_reference")) {
		actuator->m_3d.reference_distance = prop_value;
		if(actuator->m_handle)
			AUD_setDistanceReference(actuator->m_handle, prop_value);

	} else if (!strcmp(prop, "distance_maximum")) {
		actuator->m_3d.max_distance = prop_value;
		if(actuator->m_handle)
			AUD_setDistanceMaximum(actuator->m_handle, prop_value);

	} else if (!strcmp(prop, "attenuation")) {
		actuator->m_3d.rolloff_factor = prop_value;
		if(actuator->m_handle)
			AUD_setAttenuation(actuator->m_handle, prop_value);

	} else if (!!strcmp(prop, "cone_angle_inner")) {
		actuator->m_3d.cone_inner_angle = prop_value;
		if(actuator->m_handle)
			AUD_setConeAngleInner(actuator->m_handle, prop_value);

	} else if (!strcmp(prop, "cone_angle_outer")) {
		actuator->m_3d.cone_outer_angle = prop_value;
		if(actuator->m_handle)
			AUD_setConeAngleOuter(actuator->m_handle, prop_value);

	} else if (!strcmp(prop, "cone_volume_outer")) {
		actuator->m_3d.cone_outer_gain = prop_value;
		if(actuator->m_handle)
			AUD_setConeVolumeOuter(actuator->m_handle, prop_value);

	} else {
		return PY_SET_ATTR_FAIL;
	}
	
	return PY_SET_ATTR_SUCCESS;
}

int KX_SoundActuator::pyattr_set_audposition(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_SoundActuator * actuator = static_cast<KX_SoundActuator *> (self);

	float position = 1.0;
	if (!PyArg_Parse(value, "f", &position))
		return PY_SET_ATTR_FAIL;

	if(actuator->m_handle)
		AUD_seek(actuator->m_handle, position);
	return PY_SET_ATTR_SUCCESS;
}

int KX_SoundActuator::pyattr_set_gain(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	float gain = 1.0;
	KX_SoundActuator * actuator = static_cast<KX_SoundActuator *> (self);
	if (!PyArg_Parse(value, "f", &gain))
		return PY_SET_ATTR_FAIL;

	actuator->m_volume = gain;
	if(actuator->m_handle)
		AUD_setSoundVolume(actuator->m_handle, gain);

	return PY_SET_ATTR_SUCCESS;
}

int KX_SoundActuator::pyattr_set_pitch(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	float pitch = 1.0;
	KX_SoundActuator * actuator = static_cast<KX_SoundActuator *> (self);
	if (!PyArg_Parse(value, "f", &pitch))
		return PY_SET_ATTR_FAIL;

	actuator->m_pitch = pitch;
	if(actuator->m_handle)
		AUD_setSoundPitch(actuator->m_handle, pitch);

	return PY_SET_ATTR_SUCCESS;
}

#endif // DISABLE_PYTHON
