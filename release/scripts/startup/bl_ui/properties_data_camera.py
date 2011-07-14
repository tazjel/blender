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
from rna_prop_ui import PropertyPanel


class CameraButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        return context.camera and (engine in cls.COMPAT_ENGINES)


class CAMERA_MT_presets(bpy.types.Menu):
    bl_label = "Camera Presets"
    preset_subdir = "camera"
    preset_operator = "script.execute_preset"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}
    draw = bpy.types.Menu.draw_preset


class DATA_PT_context_camera(CameraButtonsPanel, bpy.types.Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        ob = context.object
        cam = context.camera
        space = context.space_data

        split = layout.split(percentage=0.65)
        if ob:
            split.template_ID(ob, "data")
            split.separator()
        elif cam:
            split.template_ID(space, "pin_id")
            split.separator()


class DATA_PT_lens(CameraButtonsPanel, bpy.types.Panel):
    bl_label = "Lens"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        cam = context.camera

        layout.prop(cam, "type", expand=True)

        split = layout.split()

        col = split.column()
        if cam.type == 'PERSP':
            col.prop(cam, "lens")

            split = layout.split()

            col = split.column()
            if cam.angle_unit == 'HORIZONTAL':
                col.prop(cam, "angle_x")
            elif cam.angle_unit == 'VERTICAL':
                col.prop(cam, "angle_y")
            col = split.column()
            col.prop(cam, "angle_unit", text="")

        elif cam.type == 'ORTHO':
            col.prop(cam, "ortho_scale")

        col = layout.column()
        if cam.type == 'ORTHO':
            if cam.use_panorama:
                col.alert = True
            else:
                col.enabled = False

        col.prop(cam, "use_panorama")

        split = layout.split()

        col = split.column()
        col.label(text="Depth of Field:")

        col.prop(cam, "dof_object", text="")

        col = col.column()
        if cam.dof_object != None:
            col.enabled = False
        col.prop(cam, "dof_distance", text="Distance")

        col = split.column(align=True)
        col.label(text="Shift:")
        col.prop(cam, "shift_x", text="X")
        col.prop(cam, "shift_y", text="Y")


class DATA_PT_camera(CameraButtonsPanel, bpy.types.Panel):
    bl_label = "Camera"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        cam = context.camera

        row = layout.row(align=True)

        row.menu("CAMERA_MT_presets", text=bpy.types.CAMERA_MT_presets.bl_label)
        row.operator("camera.preset_add", text="", icon="ZOOMIN")
        row.operator("camera.preset_add", text="", icon="ZOOMOUT").remove_active = True

        split = layout.split()

        col = split.column(align=True)
        col.label(text="Sensor Size:")
        col.prop(cam, "sensor_x", text="X")
        col.prop(cam, "sensor_y", text="Y")

        col = split.column(align=True)
        col.label(text="Clipping:")
        col.prop(cam, "clip_start", text="Start")
        col.prop(cam, "clip_end", text="End")


class DATA_PT_camera_display(CameraButtonsPanel, bpy.types.Panel):
    bl_label = "Display"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        cam = context.camera

        split = layout.split()

        col = split.column()
        col.prop(cam, "show_limits", text="Limits")
        col.prop(cam, "show_mist", text="Mist")
        col.prop(cam, "show_title_safe", text="Title Safe")
        col.prop(cam, "show_sensor", text="Sensor")
        col.prop(cam, "show_name", text="Name")

        col = split.column()
        col.prop_menu_enum(cam, "show_guide")
        col.separator()
        col.prop(cam, "draw_size", text="Size")
        col.separator()
        col.prop(cam, "show_passepartout", text="Passepartout")
        sub = col.column()
        sub.active = cam.show_passepartout
        sub.prop(cam, "passepartout_alpha", text="Alpha", slider=True)


class DATA_PT_custom_props_camera(CameraButtonsPanel, PropertyPanel, bpy.types.Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}
    _context_path = "object.data"
    _property_type = bpy.types.Camera

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
