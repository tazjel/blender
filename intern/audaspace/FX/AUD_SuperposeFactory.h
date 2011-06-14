/*
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * Copyright 2009-2011 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * Audaspace is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Audaspace; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file audaspace/FX/AUD_SuperposeFactory.h
 *  \ingroup audfx
 */


#ifndef AUD_SUPERPOSEFACTORY
#define AUD_SUPERPOSEFACTORY

#include "AUD_IFactory.h"

/**
 * This factory plays two other factories behind each other.
 * \note Readers from the underlying factories must have the same sample rate and channel count.
 */
class AUD_SuperposeFactory : public AUD_IFactory
{
private:
	/**
	 * First played factory.
	 */
	AUD_Reference<AUD_IFactory> m_factory1;

	/**
	 * Second played factory.
	 */
	AUD_Reference<AUD_IFactory> m_factory2;

	// hide copy constructor and operator=
	AUD_SuperposeFactory(const AUD_SuperposeFactory&);
	AUD_SuperposeFactory& operator=(const AUD_SuperposeFactory&);

public:
	/**
	 * Creates a new superpose factory.
	 * \param factory1 The first input factory.
	 * \param factory2 The second input factory.
	 */
	AUD_SuperposeFactory(AUD_Reference<AUD_IFactory> factory1, AUD_Reference<AUD_IFactory> factory2);

	virtual AUD_Reference<AUD_IReader> createReader();
};

#endif //AUD_SUPERPOSEFACTORY
