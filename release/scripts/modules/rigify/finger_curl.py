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
from rigify_utils import copy_bone_simple, get_side_name
from rna_prop_ui import rna_idprop_ui_prop_get

METARIG_NAMES = "finger_01", "finger_02", "finger_03"


def metarig_template():
    # generated by rigify.write_meta_rig
    bpy.ops.object.mode_set(mode='EDIT')
    obj = bpy.context.active_object
    arm = obj.data
    bone = arm.edit_bones.new('finger.01')
    bone.head[:] = 0.0000, 0.0000, 0.0000
    bone.tail[:] = 0.0353, -0.0184, -0.0053
    bone.roll = -2.8722
    bone.use_connect = False
    bone = arm.edit_bones.new('finger.02')
    bone.head[:] = 0.0353, -0.0184, -0.0053
    bone.tail[:] = 0.0702, -0.0364, -0.0146
    bone.roll = -2.7099
    bone.use_connect = True
    bone.parent = arm.edit_bones['finger.01']
    bone = arm.edit_bones.new('finger.03')
    bone.head[:] = 0.0702, -0.0364, -0.0146
    bone.tail[:] = 0.0903, -0.0461, -0.0298
    bone.roll = -2.1709
    bone.use_connect = True
    bone.parent = arm.edit_bones['finger.02']

    bpy.ops.object.mode_set(mode='OBJECT')
    pbone = obj.pose.bones['finger.01']
    pbone['type'] = 'finger_curl'


def metarig_definition(obj, orig_bone_name):
    '''
    The bone given is the first in a chain
    Expects a chain with at least 1 child of the same base name.
    eg.
        finger_01 -> finger_02
    '''

    orig_bone = obj.data.bones[orig_bone_name]

    bone_definition = [orig_bone.name]
    
    bone_definition.extend([child.name for child in orig_bone.children_recursive_basename])
    
    if len(bone_definition) < 2:
        raise RigifyError("expected the chain to have at least 1 child from bone '%s' without the same base name" % orig_bone_name)
    
    return bone_definition


def deform(obj, definitions, base_names, options):
    """ Creates the deform rig.
    """
    bpy.ops.object.mode_set(mode='EDIT')

    three_digits = True if len(definitions) > 2 else False

    # Create base digit bones: two bones, each half of the base digit.
    f1a = copy_bone_simple(obj.data, definitions[0], "DEF-%s.01" % base_names[definitions[0]], parent=True)
    f1b = copy_bone_simple(obj.data, definitions[0], "DEF-%s.02" % base_names[definitions[0]], parent=True)
    f1a.use_connect = False
    f1b.use_connect = False
    f1b.parent = f1a
    center = f1a.center
    f1a.tail = center
    f1b.head = center

    # Create the other deform bones.
    f2 = copy_bone_simple(obj.data, definitions[1], "DEF-%s" % base_names[definitions[1]], parent=True)
    if three_digits:
        f3 = copy_bone_simple(obj.data, definitions[2], "DEF-%s" % base_names[definitions[2]], parent=True)

    # Store names before leaving edit mode
    f1a_name = f1a.name
    f1b_name = f1b.name
    f2_name = f2.name
    if three_digits:
        f3_name = f3.name

    # Leave edit mode
    bpy.ops.object.mode_set(mode='OBJECT')

    # Get the pose bones
    f1a = obj.pose.bones[f1a_name]
    f1b = obj.pose.bones[f1b_name]
    f2 = obj.pose.bones[f2_name]
    if three_digits:
        f3 = obj.pose.bones[f3_name]

    # Constrain the base digit's bones
    con = f1a.constraints.new('DAMPED_TRACK')
    con.name = "trackto"
    con.target = obj
    con.subtarget = definitions[1]

    con = f1a.constraints.new('COPY_SCALE')
    con.name = "copy_scale"
    con.target = obj
    con.subtarget = definitions[0]

    con = f1b.constraints.new('COPY_ROTATION')
    con.name = "copy_rot"
    con.target = obj
    con.subtarget = definitions[0]

    # Constrain the other digit's bones
    con = f2.constraints.new('COPY_TRANSFORMS')
    con.name = "copy_transforms"
    con.target = obj
    con.subtarget = definitions[1]

    if three_digits:
        con = f3.constraints.new('COPY_TRANSFORMS')
        con.name = "copy_transforms"
        con.target = obj
        con.subtarget = definitions[2]


def main(obj, bone_definition, base_names, options):
    # *** EDITMODE
    bpy.ops.object.mode_set(mode='EDIT')
    
    three_digits = True if len(bone_definition) > 2 else False

    # get assosiated data
    arm = obj.data
    bb = obj.data.bones
    eb = obj.data.edit_bones
    pb = obj.pose.bones

    org_f1 = bone_definition[0] # Original finger bone 01
    org_f2 = bone_definition[1] # Original finger bone 02
    if three_digits:
        org_f3 = bone_definition[2] # Original finger bone 03

    # Check options
    if "bend_ratio" in options:
        bend_ratio = options["bend_ratio"]
    else:
        bend_ratio = 0.4

    yes = [1, 1.0, True, "True", "true", "Yes", "yes"]
    make_hinge = False
    if ("hinge" in options) and (eb[org_f1].parent is not None):
        if options["hinge"] in yes:
            make_hinge = True


    # Needed if its a new armature with no keys
    obj.animation_data_create()

    # Create the control bone
    base_name = base_names[bone_definition[0]].split(".", 1)[0]
    if three_digits:
        tot_len = eb[org_f1].length + eb[org_f2].length + eb[org_f3].length
    else:
        tot_len = eb[org_f1].length + eb[org_f2].length
    control = copy_bone_simple(arm, bone_definition[0], base_name + get_side_name(base_names[bone_definition[0]]), parent=True).name
    eb[control].use_connect = eb[org_f1].use_connect
    eb[control].parent = eb[org_f1].parent
    eb[control].length = tot_len

    # Create secondary control bones
    f1 = copy_bone_simple(arm, bone_definition[0], base_names[bone_definition[0]]).name
    f2 = copy_bone_simple(arm, bone_definition[1], base_names[bone_definition[1]]).name
    if three_digits:
        f3 = copy_bone_simple(arm, bone_definition[2], base_names[bone_definition[2]]).name

    # Create driver bones
    df1 = copy_bone_simple(arm, bone_definition[0], "MCH-" + base_names[bone_definition[0]]).name
    eb[df1].length /= 2
    df2 = copy_bone_simple(arm, bone_definition[1], "MCH-" + base_names[bone_definition[1]]).name
    eb[df2].length /= 2
    if three_digits:
        df3 = copy_bone_simple(arm, bone_definition[2], "MCH-" + base_names[bone_definition[2]]).name
        eb[df3].length /= 2

    # Set parents of the bones, interleaving the driver bones with the secondary control bones
    if three_digits:
        eb[f3].use_connect = False
        eb[df3].use_connect = False
    eb[f2].use_connect = False
    eb[df2].use_connect = False
    eb[f1].use_connect = False
    eb[df1].use_connect = eb[org_f1].use_connect

    if three_digits:
        eb[f3].parent = eb[df3]
        eb[df3].parent = eb[f2]
    eb[f2].parent = eb[df2]
    eb[df2].parent = eb[f1]
    eb[f1].parent = eb[df1]
    eb[df1].parent = eb[org_f1].parent

    # Set up bones for hinge
    if make_hinge:
        socket = copy_bone_simple(arm, org_f1, "MCH-socket_"+control, parent=True).name
        hinge = copy_bone_simple(arm, eb[org_f1].parent.name, "MCH-hinge_"+control).name

        eb[control].use_connect = False
        eb[control].parent = eb[hinge]

    # Create the deform rig while we're still in edit mode
    deform(obj, bone_definition, base_names, options)


    # *** POSEMODE
    bpy.ops.object.mode_set(mode='OBJECT')

    # Set rotation modes and axis locks
    pb[control].rotation_mode = obj.pose.bones[bone_definition[0]].rotation_mode
    pb[control].lock_location = True, True, True
    pb[control].lock_scale = True, False, True
    pb[f1].rotation_mode = 'YZX'
    pb[f2].rotation_mode = 'YZX'
    if three_digits:
        pb[f3].rotation_mode = 'YZX'
    pb[f1].lock_location = True, True, True
    pb[f2].lock_location = True, True, True
    if three_digits:
        pb[f3].lock_location = True, True, True
    pb[df2].rotation_mode = 'YZX'
    if three_digits:
        pb[df3].rotation_mode = 'YZX'

    # Add the bend_ratio property to the control bone
    pb[control]["bend_ratio"] = bend_ratio
    prop = rna_idprop_ui_prop_get(pb[control], "bend_ratio", create=True)
    prop["soft_min"] = 0.0
    prop["soft_max"] = 1.0

    # Add hinge property to the control bone
    if make_hinge:
        pb[control]["hinge"] = 0.0
        prop = rna_idprop_ui_prop_get(pb[control], "hinge", create=True)
        prop["soft_min"] = 0.0
        prop["soft_max"] = 1.0

    # Constraints
    con = pb[df1].constraints.new('COPY_LOCATION')
    con.target = obj
    con.subtarget = control

    con = pb[df1].constraints.new('COPY_ROTATION')
    con.target = obj
    con.subtarget = control

    con = pb[org_f1].constraints.new('COPY_TRANSFORMS')
    con.target = obj
    con.subtarget = f1

    con = pb[org_f2].constraints.new('COPY_TRANSFORMS')
    con.target = obj
    con.subtarget = f2

    if three_digits:
        con = pb[org_f3].constraints.new('COPY_TRANSFORMS')
        con.target = obj
        con.subtarget = f3

    if make_hinge:
        con = pb[hinge].constraints.new('COPY_TRANSFORMS')
        con.target = obj
        con.subtarget = bb[org_f1].parent.name

        hinge_driver_path = pb[control].path_from_id() + '["hinge"]'

        fcurve = con.driver_add("influence")
        driver = fcurve.driver
        var = driver.variables.new()
        driver.type = 'AVERAGE'
        var.name = "var"
        var.targets[0].id_type = 'OBJECT'
        var.targets[0].id = obj
        var.targets[0].data_path = hinge_driver_path

        mod = fcurve.modifiers[0]
        mod.poly_order = 1
        mod.coefficients[0] = 1.0
        mod.coefficients[1] = -1.0

        con = pb[control].constraints.new('COPY_LOCATION')
        con.target = obj
        con.subtarget = socket

    # Create the drivers for the driver bones (control bone scale rotates driver bones)
    controller_path = pb[control].path_from_id() # 'pose.bones["%s"]' % control_bone_name

    if three_digits:
        finger_digits = [df2, df3]
    else:
        finger_digits = [df2]

    i = 0
    for bone in finger_digits:

        # XXX - todo, any number
        if i == 2:
            break

        pbone = pb[bone]

        pbone.rotation_mode = 'YZX'
        fcurve_driver = pbone.driver_add("rotation_euler", 0)

        #obj.driver_add('pose.bones["%s"].scale', 1)
        #obj.animation_data.drivers[-1] # XXX, WATCH THIS
        driver = fcurve_driver.driver

        # scale target
        var = driver.variables.new()
        var.name = "scale"
        var.targets[0].id_type = 'OBJECT'
        var.targets[0].id = obj
        var.targets[0].data_path = controller_path + '.scale[1]'

        # bend target
        var = driver.variables.new()
        var.name = "br"
        var.targets[0].id_type = 'OBJECT'
        var.targets[0].id = obj
        var.targets[0].data_path = controller_path + '["bend_ratio"]'

        # XXX - todo, any number
        if three_digits:
            if i == 0:
                driver.expression = '(-scale+1.0)*pi*2.0*(1.0-br)'
            elif i == 1:
                driver.expression = '(-scale+1.0)*pi*2.0*br'
        else:
            driver.expression = driver.expression = '(-scale+1.0)*pi*2.0'

        i += 1

    # Last step setup layers
    if "ex_layer" in options:
        layer = [n==options["ex_layer"] for n in range(0,32)]
    else:
        layer = list(arm.bones[bone_definition[0]].layers)
    #for bone_name in [f1, f2, f3]:
    #    arm.bones[bone_name].layers = layer
    arm.bones[f1].layers = layer
    arm.bones[f2].layers = layer
    if three_digits:
        arm.bones[f3].layers = layer

    layer = list(arm.bones[bone_definition[0]].layers)
    bb[control].layers = layer

    # no blending the result of this
    return None

