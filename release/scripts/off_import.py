#!BPY

"""
Name: 'DEC Object File Format (.off)...'
Blender: 232
Group: 'Import'
Tooltip: 'Import DEC Object File Format (*.off)'
"""

__author__ = "Anthony D'Agostino (Scorpius)"
__url__ = ("blender", "elysiun",
"Author's homepage, http://www.redrival.com/scorpius")
__version__ = "Part of IOSuite 0.5"

__bpydoc__ = """\
This script imports DEC Object File Format files to Blender.

The DEC (Digital Equipment Corporation) OFF format is very old and
almost identical to Wavefront's OBJ. I wrote this so I could get my huge
meshes into Moonlight Atelier. (DXF can also be used but the file size
is five times larger than OFF!) Blender/Moonlight users might find this
script to be very useful.

Usage:<br>
	Execute this script from the "File->Import" menu and choose an OFF file to
open.

Notes:<br>
	UV Coordinate support has been added.
"""

# $Id:
#
# +---------------------------------------------------------+
# | Copyright (c) 2002 Anthony D'Agostino                   |
# | http://www.redrival.com/scorpius                        |
# | scorpius@netzero.com                                    |
# | February 3, 2001                                        |
# | Read and write Object File Format (*.off)               |
# +---------------------------------------------------------+

# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ***** END GPL LICENCE BLOCK *****

import Blender, meshtools
#import time

# =============================
# ====== Read OFF Format ======
# =============================
def read(filename):
	#start = time.clock()
	file = open(filename, "rb")

	verts = []
	faces = []
	uv = []

	# === OFF Header ===
	offheader = file.readline()
	numverts, numfaces, null = file.readline().split()
	numverts = int(numverts)
	numfaces = int(numfaces)
	if offheader.find('ST') >= 0:
		has_uv = True
	else:
		has_uv = False

	# === Vertex List ===
	for i in range(numverts):
		if not i%100 and meshtools.show_progress:
			Blender.Window.DrawProgressBar(float(i)/numverts, "Reading Verts")
		if has_uv:
			x, y, z, u, v = map(float, file.readline().split())
			uv.append((u, v))
		else:
			x, y, z = map(float, file.readline().split())
		verts.append((x, y, z))

	# === Face List ===
	for i in range(numfaces):
		if not i%100 and meshtools.show_progress:
			Blender.Window.DrawProgressBar(float(i)/numfaces, "Reading Faces")
		line = file.readline().split()
		numfaceverts = len(line)-1
		facev = []
		for j in range(numfaceverts):
			index = int(line[j+1])
			facev.append(index)
		facev.reverse()
		faces.append(facev)

	objname = Blender.sys.splitext(Blender.sys.basename(filename))[0]

	meshtools.create_mesh(verts, faces, objname, faces, uv)
	Blender.Window.DrawProgressBar(1.0, '')  # clear progressbar
	file.close()
	#end = time.clock()
	#seconds = " in %.2f %s" % (end-start, "seconds")
	message = "Successfully imported " + Blender.sys.basename(filename)# + seconds
	meshtools.print_boxed(message)

def fs_callback(filename):
	read(filename)

Blender.Window.FileSelector(fs_callback, "Import OFF")
