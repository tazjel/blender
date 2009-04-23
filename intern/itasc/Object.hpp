/* $Id$
 * Object.hpp
 *
 *  Created on: Jan 5, 2009
 *      Author: rubensmits
 */

#ifndef OBJECT_HPP_
#define OBJECT_HPP_

#include "Cache.hpp"
#include "kdl/frames.hpp"
#include <string>

namespace iTaSC{

class WorldObject;

class Object {
public:
    enum ObjectType {Controlled, UnControlled};
	static WorldObject world;

private:
    ObjectType m_type;
protected:
	Cache *m_cache;
	KDL::Frame m_internalPose;
    virtual void updateJacobian()=0;
public:
    Object(ObjectType _type):m_type(_type), m_cache(NULL), m_internalPose(F_identity) {};
    virtual ~Object(){};

	virtual int addEndEffector(const std::string& name){return 0;};
	virtual void finalize(){};
	virtual const KDL::Frame& getPose(const unsigned int end_effector=0){return m_internalPose;};
    virtual const ObjectType getType(){return m_type;};
    virtual const unsigned int getNrOfCoordinates(){return 0;};
    virtual void updateKinematics(const Timestamp& timestamp)=0;
	virtual void initCache(Cache *_cache) = 0;

};

}
#endif /* OBJECT_HPP_ */
