# Write the actual Makefile.
##
## Modified makemakefile for autotools Blender build
##


import os
import string

def makemakefile(outfp, makevars, files, target):
	outfp.write("# Makefile generated by freeze.py script\n\n")

	target = "frozen"
	libtarget = "lib" + target
	targetlib = libtarget + ".a"
	#targetlib = "libpyfrozen.a"

	keys = makevars.keys()
	keys.sort()
	for key in keys:
		outfp.write("%s=%s\n" % (key, makevars[key]))

#        outfp.write("\n\ninclude nan_definitions.mk\n")
	outfp.write("\nall: %s\n\n" % libtarget)

	deps = []
	for i in range(len(files)):
		file = files[i]
		if file[-2:] == '.c':
			base = os.path.basename(file)
			dest = base[:-2] + '.o'
		#	outfp.write("%s: %s\n" % (dest, file))
		#	outfp.write("\t$(CC) $(CFLAGS) -c %s\n" % file)
			files[i] = dest
			deps.append(dest)

	mainfile = 'M___main__.o'
	
	try:
		deps.remove(mainfile)
	except:
		pass
	outfp.write("OBJS = %s\n" % string.join(deps))

#	libfiles.remove('M___main__.o') # don't link with __main__

	outfp.write("\n%s: $(OBJS)\n" % (libtarget))
#	outfp.write("\t$(AR) ruv %s%s $(OBJS)\n" % 
#		("$(OCGDIR)/blender/bpython/$(DEBUG_DIR)", targetlib))
	outfp.write("\t$(AR) ruv %s $(OBJS)\n" % targetlib)
	outfp.write("ifeq ($(MACHDEP), darwin)\n")
	outfp.write("\tranlib %s\n" % targetlib)
	outfp.write("endif\n")

	outfp.write("\n%s: %s $(OBJS)\n" % (target, mainfile))
	outfp.write("\t$(CC) %s %s -o %s $(LDLAST)\n" % 
				(mainfile, " ".join(deps), target))

	outfp.write("\nclean:\n\t-rm -f *.o *.a %s\n" % target)
