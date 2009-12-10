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
from rigify_utils import bone_class_instance, copy_bone_simple, blend_bone_list, get_side_name, get_base_name
from rna_prop_ui import rna_idprop_ui_prop_get

METARIG_NAMES = "hips", "thigh", "shin", "foot", "toe", "heel"


def metarig_template():
    # generated by rigify.write_meta_rig
    bpy.ops.object.mode_set(mode='EDIT')
    obj = bpy.context.active_object
    arm = obj.data
    bone = arm.edit_bones.new('hips')
    bone.head[:] = 0.0000, 0.0000, 0.0000
    bone.tail[:] = 0.0000, 0.0000, 1.0000
    bone.roll = 0.0000
    bone.connected = False
    bone = arm.edit_bones.new('thigh')
    bone.head[:] = 0.5000, 0.0000, 0.0000
    bone.tail[:] = 0.3000, -0.1000, -1.7000
    bone.roll = 0.1171
    bone.connected = False
    bone.parent = arm.edit_bones['hips']
    bone = arm.edit_bones.new('shin')
    bone.head[:] = 0.3000, -0.1000, -1.7000
    bone.tail[:] = 0.3000, 0.0000, -3.5000
    bone.roll = -0.0000
    bone.connected = True
    bone.parent = arm.edit_bones['thigh']
    bone = arm.edit_bones.new('foot')
    bone.head[:] = 0.3000, 0.0000, -3.5000
    bone.tail[:] = 0.4042, -0.5909, -3.9000
    bone.roll = -0.4662
    bone.connected = True
    bone.parent = arm.edit_bones['shin']
    bone = arm.edit_bones.new('toe')
    bone.head[:] = 0.4042, -0.5909, -3.9000
    bone.tail[:] = 0.4391, -0.9894, -3.9000
    bone.roll = -3.1416
    bone.connected = True
    bone.parent = arm.edit_bones['foot']
    bone = arm.edit_bones.new('heel')
    bone.head[:] = 0.2600, 0.2000, -4.0000
    bone.tail[:] = 0.3700, -0.4000, -4.0000
    bone.roll = 0.0000
    bone.connected = False
    bone.parent = arm.edit_bones['foot']

    bpy.ops.object.mode_set(mode='OBJECT')
    pbone = obj.pose.bones['thigh']
    pbone['type'] = 'leg'


def metarig_definition(obj, orig_bone_name):
    '''
    The bone given is the first in a chain
    Expects a chain of at least 3 children.
    eg.
        thigh -> shin -> foot -> [toe, heel]
    '''

    bone_definition = []

    orig_bone = obj.data.bones[orig_bone_name]
    orig_bone_parent = orig_bone.parent

    if orig_bone_parent is None:
        raise Exception("expected the thigh bone to have a parent hip bone")

    bone_definition.append(orig_bone_parent.name)
    bone_definition.append(orig_bone.name)


    bone = orig_bone
    chain = 0
    while chain < 2: # first 2 bones only have 1 child
        children = bone.children

        if len(children) != 1:
            raise Exception("expected the thigh bone to have 3 children without a fork")
        bone = children[0]
        bone_definition.append(bone.name) # shin, foot
        chain += 1

    children = bone.children
    # Now there must be 2 children, only one connected
    if len(children) != 2:
        raise Exception("expected the foot bone:'%s' to have 2 children" % bone.name )

    if children[0].connected == children[1].connected:
        raise Exception("expected one bone to be connected")

    toe, heel = children
    if heel.connected:
        toe, heel = heel, toe


    bone_definition.append(toe.name)
    bone_definition.append(heel.name)

    if len(bone_definition) != len(METARIG_NAMES):
        raise Exception("internal problem, expected %d bones" % len(METARIG_NAMES))

    return bone_definition


def ik(obj, bone_definition, base_names):
    arm = obj.data

    # setup the existing bones
    mt_chain = bone_class_instance(obj, ["thigh", "shin", "foot", "toe"])
    mt = bone_class_instance(obj, ["hips", "heel"])
    #ex = bone_class_instance(obj, [""])
    ex = bone_class_instance(obj, ["thigh_socket", "thigh_hinge", "foot_roll_1", "foot_roll_2", "foot_roll_3"])
    # children of ik_foot
    ik = bone_class_instance(obj, ["foot", "foot_roll", "foot_roll_01", "foot_roll_02", "knee_target"])

    # XXX - duplicate below
    for bone_class in (mt, mt_chain):
        for attr in bone_class.attr_names:
            i = METARIG_NAMES.index(attr)
            ebone = arm.edit_bones[bone_definition[i]]
            setattr(bone_class, attr, ebone.name)
        bone_class.update()
    # XXX - end dupe


    # Make a new chain, ORG are the original bones renamed.
    ik_chain = mt_chain.copy(to_fmt="MCH-%s")

    # simple rename
    ik_chain.rename("thigh", ik_chain.thigh + "_ik")
    ik_chain.rename("shin", ik_chain.shin + "_ik")

    # make sure leg is child of hips
    ik_chain.thigh_e.parent = mt.hips_e

    # ik foot: no parents
    base_foot_name = get_base_name(base_names[mt_chain.foot])
    ik.foot_e = copy_bone_simple(arm, mt_chain.foot, base_foot_name + "_ik" + get_side_name(base_names[mt_chain.foot]))
    ik.foot = ik.foot_e.name
    ik.foot_e.tail.z = ik.foot_e.head.z
    ik.foot_e.roll = 0.0
    ik.foot_e.local_location = False

    # foot roll: heel pointing backwards, half length
    ik.foot_roll_e = copy_bone_simple(arm, mt.heel, base_foot_name + "_roll" + get_side_name(base_names[mt_chain.foot]))
    ik.foot_roll = ik.foot_roll_e.name
    ik.foot_roll_e.tail = ik.foot_roll_e.head + (ik.foot_roll_e.head - ik.foot_roll_e.tail) / 2.0
    ik.foot_roll_e.parent = ik.foot_e # heel is disconnected

    # heel pointing forwards to the toe base, parent of the following 2 bones
    ik.foot_roll_01_e = copy_bone_simple(arm, mt.heel, "MCH-%s_roll.01" % base_foot_name)
    ik.foot_roll_01 = ik.foot_roll_01_e.name
    ik.foot_roll_01_e.tail = mt_chain.foot_e.tail
    ik.foot_roll_01_e.parent = ik.foot_e # heel is disconnected

    # same as above but reverse direction
    ik.foot_roll_02_e = copy_bone_simple(arm, mt.heel, "MCH-%s_roll.02" % base_foot_name)
    ik.foot_roll_02 = ik.foot_roll_02_e.name
    ik.foot_roll_02_e.parent = ik.foot_roll_01_e # heel is disconnected
    ik.foot_roll_02_e.head = mt_chain.foot_e.tail
    ik.foot_roll_02_e.tail = mt.heel_e.head

    del base_foot_name

    # rename 'MCH-toe' --> to 'toe_ik' and make the child of ik.foot_roll_01
    # ------------------ FK or IK?
    ik_chain.rename("toe", get_base_name(base_names[mt_chain.toe]) + "_ik" + get_side_name(base_names[mt_chain.toe]))
    ik_chain.toe_e.connected = False
    ik_chain.toe_e.parent = ik.foot_roll_01_e

    # re-parent ik_chain.foot to the
    ik_chain.foot_e.connected = False
    ik_chain.foot_e.parent = ik.foot_roll_02_e


    # knee target is the heel moved up and forward on its local axis
    ik.knee_target_e = copy_bone_simple(arm, mt.heel, "knee_target")
    ik.knee_target = ik.knee_target_e.name
    offset = ik.knee_target_e.tail - ik.knee_target_e.head
    offset.z = 0
    offset.length = mt_chain.shin_e.head.z - mt.heel_e.head.z
    offset.z += offset.length
    ik.knee_target_e.translate(offset)
    ik.knee_target_e.length *= 0.5
    ik.knee_target_e.parent = ik.foot_e
    ik.knee_target_e.local_location = False

    # roll the bone to point up... could also point in the same direction as ik.foot_roll
    # ik.foot_roll_02_e.matrix * Vector(0.0, 0.0, 1.0) # ACK!, no rest matrix in editmode
    ik.foot_roll_01_e.align((0.0, 0.0, -1.0))

    bpy.ops.object.mode_set(mode='OBJECT')

    ik.update()
    ex.update()
    mt_chain.update()
    ik_chain.update()

    # Set IK dof
    ik_chain.shin_p.ik_dof_x = True
    ik_chain.shin_p.ik_dof_y = False
    ik_chain.shin_p.ik_dof_z = False

    # Set rotation modes and axis locks
    ik.foot_roll_p.rotation_mode = 'XYZ'
    ik.foot_roll_p.lock_rotation = False, True, True
    ik_chain.toe_p.rotation_mode = 'YXZ'
    ik_chain.toe_p.lock_rotation = False, True, True

    # IK
    con = ik_chain.shin_p.constraints.new('IK')
    con.chain_length = 2
    con.iterations = 500
    con.pole_angle = -90.0 # XXX - in deg!
    con.use_tail = True
    con.use_stretch = True
    con.use_target = True
    con.use_rotation = False
    con.weight = 1.0

    con.target = obj
    con.subtarget = ik_chain.foot

    con.pole_target = obj
    con.pole_subtarget = ik.knee_target

    # foot roll
    cons = [ \
        (ik.foot_roll_01_p.constraints.new('COPY_ROTATION'), ik.foot_roll_01_p.constraints.new('LIMIT_ROTATION')), \
        (ik.foot_roll_02_p.constraints.new('COPY_ROTATION'), ik.foot_roll_02_p.constraints.new('LIMIT_ROTATION'))]

    for con, con_l in cons:
        con.target = obj
        con.subtarget = ik.foot_roll
        con.use_x, con.use_y, con.use_z = True, False, False
        con.target_space = con.owner_space = 'LOCAL'

        con = con_l
        con.use_limit_x, con.use_limit_y, con.use_limit_z = True, False, False
        con.owner_space = 'LOCAL'

        if con_l is cons[-1][-1]:
            con.minimum_x = 0.0
            con.maximum_x = 180.0 # XXX -deg
        else:
            con.minimum_x = -180.0 # XXX -deg
            con.maximum_x = 0.0

    bpy.ops.object.mode_set(mode='EDIT')

    return None, ik_chain.thigh, ik_chain.shin, ik_chain.foot, ik_chain.toe, None


def fk(obj, bone_definition, base_names):
    from Mathutils import Vector
    arm = obj.data

    # these account for all bones in METARIG_NAMES
    mt_chain = bone_class_instance(obj, ["thigh", "shin", "foot", "toe"])
    mt = bone_class_instance(obj, ["hips", "heel"])

    # new bones
    ex = bone_class_instance(obj, ["thigh_socket", "thigh_hinge"])

    for bone_class in (mt, mt_chain):
        for attr in bone_class.attr_names:
            i = METARIG_NAMES.index(attr)
            ebone = arm.edit_bones[bone_definition[i]]
            setattr(bone_class, attr, ebone.name)
        bone_class.update()

    ex.thigh_socket_e = copy_bone_simple(arm, mt_chain.thigh, "MCH-%s_socket" % base_names[mt_chain.thigh], parent=True)
    ex.thigh_socket = ex.thigh_socket_e.name
    ex.thigh_socket_e.tail = ex.thigh_socket_e.head + Vector(0.0, 0.0, ex.thigh_socket_e.length / 4.0)

    ex.thigh_hinge_e = copy_bone_simple(arm, mt.hips, "MCH-%s_hinge" % base_names[mt_chain.thigh], parent=False)
    ex.thigh_hinge = ex.thigh_hinge_e.name

    fk_chain = mt_chain.copy(base_names=base_names) # fk has no prefix!

    fk_chain.thigh_e.connected = False
    fk_chain.thigh_e.parent = ex.thigh_hinge_e

    bpy.ops.object.mode_set(mode='OBJECT')

    ex.update()
    mt_chain.update()
    fk_chain.update()

    # Set rotation modes and axis locks
    fk_chain.shin_p.rotation_mode = 'XYZ'
    fk_chain.shin_p.lock_rotation = False, True, True
    fk_chain.foot_p.rotation_mode = 'YXZ'
    fk_chain.toe_p.rotation_mode = 'YXZ'
    fk_chain.toe_p.lock_rotation = False, True, True

    con = fk_chain.thigh_p.constraints.new('COPY_LOCATION')
    con.target = obj
    con.subtarget = ex.thigh_socket

    # hinge
    prop = rna_idprop_ui_prop_get(fk_chain.thigh_p, "hinge", create=True)
    fk_chain.thigh_p["hinge"] = 0.5
    prop["soft_min"] = 0.0
    prop["soft_max"] = 1.0

    con = ex.thigh_hinge_p.constraints.new('COPY_ROTATION')
    con.target = obj
    con.subtarget = mt.hips

    # add driver
    hinge_driver_path = fk_chain.thigh_p.path_to_id() + '["hinge"]'

    fcurve = con.driver_add("influence", 0)
    driver = fcurve.driver
    tar = driver.targets.new()
    driver.type = 'AVERAGE'
    tar.name = "var"
    tar.id_type = 'OBJECT'
    tar.id = obj
    tar.rna_path = hinge_driver_path

    mod = fcurve.modifiers[0]
    mod.poly_order = 1
    mod.coefficients[0] = 1.0
    mod.coefficients[1] = -1.0

    bpy.ops.object.mode_set(mode='EDIT')

    # dont blend the hips or heel
    return None, fk_chain.thigh, fk_chain.shin, fk_chain.foot, fk_chain.toe, None


def main(obj, bone_definition, base_names):
    bones_ik = ik(obj, bone_definition, base_names)
    bones_fk = fk(obj, bone_definition, base_names)

    bpy.ops.object.mode_set(mode='OBJECT')
    blend_bone_list(obj, bone_definition, bones_ik, bones_fk, target_bone=bone_definition[1])
