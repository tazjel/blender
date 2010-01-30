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
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

import bpy
from rigify import get_layer_dict
from rigify_utils import bone_class_instance, copy_bone_simple

METARIG_NAMES = ("cpy",)


def metarig_template():
    # generated by rigify.write_meta_rig
    bpy.ops.object.mode_set(mode='EDIT')
    obj = bpy.context.active_object
    arm = obj.data
    bone = arm.edit_bones.new('Bone')
    bone.head[:] = 0.0000, 0.0000, 0.0000
    bone.tail[:] = 0.0000, 0.0000, 1.0000
    bone.roll = 0.0000
    bone.connected = False

    bpy.ops.object.mode_set(mode='OBJECT')
    pbone = obj.pose.bones['Bone']
    pbone['type'] = 'copy'


def metarig_definition(obj, orig_bone_name):
    return (orig_bone_name,)


def deform(obj, definitions, base_names, options):
    bpy.ops.object.mode_set(mode='EDIT')

    # Create deform bone.
    bone = copy_bone_simple(obj.data, definitions[0], "DEF-%s" % base_names[definitions[0]], parent=True)
    
    # Store name before leaving edit mode
    bone_name = bone.name
    
    # Leave edit mode
    bpy.ops.object.mode_set(mode='OBJECT')
    
    # Get the pose bone
    bone = obj.pose.bones[bone_name]
    
    # Constrain to the original bone
    con = bone.constraints.new('COPY_TRANSFORMS')
    con.name = "copy_loc"
    con.target = obj
    con.subtarget = definitions[0]
    
    return (bone_name,)


def control(obj, definitions, base_names, options):
    bpy.ops.object.mode_set(mode='EDIT')
    
    arm = obj.data
    mt = bone_class_instance(obj, METARIG_NAMES)
    mt.cpy = definitions[0]
    mt.update()
    cp = bone_class_instance(obj, ["cpy"])
    cp.cpy_e = copy_bone_simple(arm, mt.cpy, base_names[mt.cpy], parent=True)
    cp.cpy = cp.cpy_e.name

    bpy.ops.object.mode_set(mode='OBJECT')

    cp.update()
    mt.update()

    con = mt.cpy_p.constraints.new('COPY_TRANSFORMS')
    con.target = obj
    con.subtarget = cp.cpy


    # Rotation mode and axis locks
    cp.cpy_p.rotation_mode = mt.cpy_p.rotation_mode
    cp.cpy_p.lock_location = tuple(mt.cpy_p.lock_location)
    cp.cpy_p.lock_rotations_4d = mt.cpy_p.lock_rotations_4d
    cp.cpy_p.lock_rotation = tuple(mt.cpy_p.lock_rotation)
    cp.cpy_p.lock_rotation_w = mt.cpy_p.lock_rotation_w
    cp.cpy_p.lock_scale = tuple(mt.cpy_p.lock_scale)
    
    # Layers
    cp.cpy_b.layer = list(mt.cpy_b.layer)
    
    return (mt.cpy,)


def main(obj, bone_definition, base_names, options):
    # Create control bone
    cpy = control(obj, bone_definition, base_names, options)[0]
    # Create deform bone
    deform(obj, bone_definition, base_names, options)

    return (cpy,)

