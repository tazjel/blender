# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

import bpy
from rigify import RigifyError
from rigify_utils import bone_class_instance, copy_bone_simple
from rna_prop_ui import rna_idprop_ui_prop_get
from mathutils import Vector, RotationMatrix
from math import radians, pi

# not used, defined for completeness
METARIG_NAMES = ("pelvis", "ribcage")


def metarig_template():
    # TODO
    pass
    # generated by rigify.write_meta_rig
    #bpy.ops.object.mode_set(mode='EDIT')
    #obj = bpy.context.active_object
    #arm = obj.data
    #bone = arm.edit_bones.new('tail.01')
    #bone.head[:] = 0.0000, -0.0306, 0.1039
    #bone.tail[:] = 0.0000, -0.0306, -0.0159
    #bone.roll = 0.0000
    #bone.connected = False

    #bpy.ops.object.mode_set(mode='OBJECT')
    #pbone = obj.pose.bones['tail.01']
    #pbone['type'] = 'tail_spline_ik'


def metarig_definition(obj, orig_bone_name):
    """ Collects and returns the relevent bones for the rig.
        The bone given is the first in the chain of tail bones.
        It includes bones in the chain up until it hits a bone that doesn't
        have the same name base.

        tail.01 -> tail.02 -> tail.03 -> ... -> tail.n
    """
    arm = obj.data
    tail_base = arm.bones[orig_bone_name]

    if tail_base.parent == None:
        raise RigifyError("'tail_control' rig type on bone '%s' requires a parent." % orig_bone_name)

    bone_definitions = [tail_base.name]
    bone_definitions.extend([child.name for child in tail_base.children_recursive_basename])
    return bone_definitions


def main(obj, bone_definitions, base_names, options):
    bpy.ops.object.mode_set(mode='EDIT')
    arm = obj.data
    bb = obj.data.bones
    eb = obj.data.edit_bones
    pb = obj.pose.bones

    # Create bones for hinge/free
    # hinge 1 sticks with the parent
    # hinge 2 is the parent of the tail controls
    hinge1 = copy_bone_simple(arm, bone_definitions[0], "MCH-%s.hinge1" % base_names[bone_definitions[0]], parent=True).name
    hinge2 = copy_bone_simple(arm, bone_definitions[0], "MCH-%s.hinge2" % base_names[bone_definitions[0]], parent=False).name

    # Create tail control bones
    bones = []
    i = 0
    for bone_def in bone_definitions:
        bone = copy_bone_simple(arm, bone_def, base_names[bone_def], parent=True).name
        if i == 1:  # Don't change parent of first tail bone
            eb[bone].connected = False
            eb[bone].parent = eb[hinge2]
            eb[bone].local_location = False
        i = 1
        bones += [bone]


    bpy.ops.object.mode_set(mode='OBJECT')

    # Rotation mode and axis locks
    for bone, org_bone in zip(bones, bone_definitions):
        pb[bone].rotation_mode = pb[org_bone].rotation_mode
        pb[bone].lock_location = tuple(pb[org_bone].lock_location)
        pb[bone].lock_rotations_4d = pb[org_bone].lock_rotations_4d
        pb[bone].lock_rotation = tuple(pb[org_bone].lock_rotation)
        pb[bone].lock_rotation_w = pb[org_bone].lock_rotation_w
        pb[bone].lock_scale = tuple(pb[org_bone].lock_scale)

    # Add custom properties
    pb[bones[0]]["hinge"] = 0.0
    prop = rna_idprop_ui_prop_get(pb[bones[0]], "hinge", create=True)
    prop["min"] = 0.0
    prop["max"] = 1.0
    prop["soft_min"] = 0.0
    prop["soft_max"] = 1.0

    pb[bones[0]]["free"] = 0.0
    prop = rna_idprop_ui_prop_get(pb[bones[0]], "free", create=True)
    prop["min"] = 0.0
    prop["max"] = 1.0
    prop["soft_min"] = 0.0
    prop["soft_max"] = 1.0

    # Add constraints
    for bone, org_bone in zip(bones, bone_definitions):
        con = pb[org_bone].constraints.new('COPY_TRANSFORMS')
        con.target = obj
        con.subtarget = bone

    con_f = pb[hinge2].constraints.new('COPY_LOCATION')
    con_f.target = obj
    con_f.subtarget = hinge1

    con_h = pb[hinge2].constraints.new('COPY_TRANSFORMS')
    con_h.target = obj
    con_h.subtarget = hinge1

    # Add drivers
    bone_path = pb[bones[0]].path_from_id()

    driver_fcurve = con_f.driver_add("influence")
    driver = driver_fcurve.driver
    driver.type = 'AVERAGE'
    var = driver.variables.new()
    var.name = "free"
    var.targets[0].id_type = 'OBJECT'
    var.targets[0].id = obj
    var.targets[0].data_path = bone_path + '["free"]'
    mod = driver_fcurve.modifiers[0]
    mod.poly_order = 1
    mod.coefficients[0] = 1.0
    mod.coefficients[1] = -1.0

    driver_fcurve = con_h.driver_add("influence")
    driver = driver_fcurve.driver
    driver.type = 'AVERAGE'
    var = driver.variables.new()
    var.name = "hinge"
    var.targets[0].id_type = 'OBJECT'
    var.targets[0].id = obj
    var.targets[0].data_path = bone_path + '["hinge"]'
    mod = driver_fcurve.modifiers[0]
    mod.poly_order = 1
    mod.coefficients[0] = 1.0
    mod.coefficients[1] = -1.0


    return None
