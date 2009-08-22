
import bpy

class PhysicButtonsPanel(bpy.types.Panel):
	__space_type__ = 'PROPERTIES'
	__region_type__ = 'WINDOW'
	__context__ = "physics"

	def poll(self, context):
		ob = context.object
		rd = context.scene.render_data
		return (ob and ob.type == 'MESH') and (not rd.use_game_engine)
		
class PHYSICS_PT_softbody(PhysicButtonsPanel):
	__label__ = "Soft Body"

	def draw(self, context):
		layout = self.layout
		
		md = context.soft_body
		ob = context.object

		split = layout.split()
		split.operator_context = "EXEC_DEFAULT"

		if md:
			# remove modifier + settings
			split.set_context_pointer("modifier", md)
			split.itemO("object.modifier_remove", text="Remove")

			row = split.row(align=True)
			row.itemR(md, "render", text="")
			row.itemR(md, "realtime", text="")
		else:
			# add modifier
			split.item_enumO("object.modifier_add", "type", 'SOFT_BODY', text="Add")
			split.itemL("")
			
		if md:
			softbody = md.settings

			# General
			split = layout.split()
			
			col = split.column()
			col.itemL(text="Object:")
			col.itemR(softbody, "mass")
			col.itemR(softbody, "friction")

			col = split.column()
			col.itemL(text="Simulation:")
			col.itemR(softbody, "gravity")
			col.itemR(softbody, "speed")
			
class PHYSICS_PT_softbody_cache(PhysicButtonsPanel):
	__label__ = "Soft Body Cache"
	__default_closed__ = True

	def poll(self, context):
		return (context.soft_body)

	def draw(self, context):
		layout = self.layout

		cache = context.soft_body.point_cache
		layout.set_context_pointer("PointCache", cache)
		
		row = layout.row()
		row.template_list(cache, "point_cache_list", cache, "active_point_cache_index")
		col = row.column(align=True)
		col.itemO("ptcache.add_new", icon='ICON_ZOOMIN', text="")
		col.itemO("ptcache.remove", icon='ICON_ZOOMOUT', text="")
		
		row = layout.row()
		row.itemR(cache, "name")
		
		row = layout.row()
		row.itemR(cache, "start_frame")
		row.itemR(cache, "end_frame")
		
		row = layout.row()
		
		if cache.baked == True:
			row.itemO("ptcache.free_bake", text="Free Bake")
		else:
			row.item_booleanO("ptcache.bake", "bake", True, text="Bake")
		
		sub = row.row()
		sub.enabled = cache.frames_skipped or cache.outdated
		sub.itemO("ptcache.bake", "bake", False, text="Calculate to Current Frame")
			
		row = layout.row()
		row.itemO("ptcache.bake_from_cache", text="Current Cache to Bake")
		row.itemR(cache, "step");
	
		row = layout.row()
		row.itemR(cache, "quick_cache")
		row.itemR(cache, "disk_cache")
		
		layout.itemL(text=cache.info)
		
		layout.itemS()
		
		row = layout.row()
		row.itemO("ptcache.bake_all", "bake", True, text="Bake All Dynamics")
		row.itemO("ptcache.free_bake_all", text="Free All Bakes")
		layout.itemO("ptcache.bake_all", "bake", False, text="Update All Dynamics to current frame")
		
class PHYSICS_PT_softbody_goal(PhysicButtonsPanel):
	__label__ = "Soft Body Goal"
	
	def poll(self, context):
		return (context.soft_body)
		
	def draw_header(self, context):
		layout = self.layout
		
		softbody = context.soft_body.settings
	
		layout.itemR(softbody, "use_goal", text="")
		
	def draw(self, context):
		layout = self.layout
		
		md = context.soft_body
		ob = context.object

		split = layout.split()
			
		if md:
			softbody = md.settings
			layout.active = softbody.use_goal

			# Goal 
			split = layout.split()

			col = split.column()
			col.itemL(text="Goal Strengths:")
			col.itemR(softbody, "goal_default", text="Default")
			sub = col.column(align=True)
			sub.itemR(softbody, "goal_min", text="Minimum")
			sub.itemR(softbody, "goal_max", text="Maximum")
			
			col = split.column()
			col.itemL(text="Goal Settings:")
			col.itemR(softbody, "goal_spring", text="Stiffness")
			col.itemR(softbody, "goal_friction", text="Damping")
			
			layout.item_pointerR(softbody, "goal_vertex_group", ob, "vertex_groups", text="Vertex Group")

class PHYSICS_PT_softbody_edge(PhysicButtonsPanel):
	__label__ = "Soft Body Edges"
	
	def poll(self, context):
		return (context.soft_body)
		
	def draw_header(self, context):
		layout = self.layout
		
		softbody = context.soft_body.settings
	
		layout.itemR(softbody, "use_edges", text="")
		
	def draw(self, context):
		layout = self.layout
		
		md = context.soft_body
		ob = context.object
			
		if md:
			softbody = md.settings
			
			layout.active = softbody.use_edges
			
			split = layout.split()
			
			col = split.column()
			col.itemL(text="Springs:")
			col.itemR(softbody, "pull")
			col.itemR(softbody, "push")
			col.itemR(softbody, "damp")
			col.itemR(softbody, "plastic")
			col.itemR(softbody, "bending")
			col.itemR(softbody, "spring_length", text="Length")
			
			col = split.column()
			col.itemR(softbody, "stiff_quads")
			sub = col.column()
			sub.active = softbody.stiff_quads
			sub.itemR(softbody, "shear")
			
			col.itemR(softbody, "new_aero", text="Aero")
			sub = col.column()
			sub.enabled = softbody.new_aero
			sub.itemR(softbody, "aero", text="Factor")

			col.itemL(text="Collision:")
			col.itemR(softbody, "edge_collision", text="Edge")
			col.itemR(softbody, "face_collision", text="Face")
			
class PHYSICS_PT_softbody_collision(PhysicButtonsPanel):
	__label__ = "Soft Body Collision"
	
	def poll(self, context):
		return (context.soft_body)
		
	def draw_header(self, context):
		layout = self.layout
		
		softbody = context.soft_body.settings
	
		layout.itemR(softbody, "self_collision", text="")
		
	def draw(self, context):
		layout = self.layout
		
		md = context.soft_body
		ob = context.object
			
		if md:
			softbody = md.settings

			layout.active = softbody.self_collision
			
			layout.itemL(text="Collision Type:")
			layout.itemR(softbody, "collision_type", expand=True)
			
			col = layout.column(align=True)
			col.itemL(text="Ball:")
			col.itemR(softbody, "ball_size", text="Size")
			col.itemR(softbody, "ball_stiff", text="Stiffness")
			col.itemR(softbody, "ball_damp", text="Dampening")

class PHYSICS_PT_softbody_solver(PhysicButtonsPanel):
	__label__ = "Soft Body Solver"
	
	def poll(self, context):
		return (context.soft_body)
		
	def draw(self, context):
		layout = self.layout
		
		md = context.soft_body
		ob = context.object
		
		if md:
			softbody = md.settings

			# Solver
			split = layout.split()
			
			col = split.column(align=True)
			col.itemL(text="Step Size:")
			col.itemR(softbody, "minstep")
			col.itemR(softbody, "maxstep")
			col.itemR(softbody, "auto_step", text="Auto-Step")
			
			col = split.column()
			col.itemR(softbody, "error_limit")
			col.itemL(text="Helpers:")
			col.itemR(softbody, "choke")
			col.itemR(softbody, "fuzzy")
			
			layout.itemL(text="Diagnostics:")
			layout.itemR(softbody, "diagnose")
	
bpy.types.register(PHYSICS_PT_softbody)
bpy.types.register(PHYSICS_PT_softbody_cache)
bpy.types.register(PHYSICS_PT_softbody_goal)
bpy.types.register(PHYSICS_PT_softbody_edge)
bpy.types.register(PHYSICS_PT_softbody_collision)
bpy.types.register(PHYSICS_PT_softbody_solver)
