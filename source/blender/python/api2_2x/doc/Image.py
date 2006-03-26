# Blender.Image module and the Image PyType object

"""
The Blender.Image submodule.

Image
=====

B{New}: L{Image.setFilename}.

This module provides access to B{Image} objects in Blender.

Example::
  import Blender
  from Blender import Image
  #
  image = Image.Load("/path/to/my/image.png")    # load an image file
  print "Image from", image.getFilename(),
  print "loaded to obj", image.getName())
  image.setXRep(4)                               # set x tiling factor
  image.setYRep(2)                               # set y tiling factor
  print "All Images available now:", Image.Get()
"""

def Load (filename):
  """
  Load the image called 'filename' into an Image object.
  @type filename: string
  @param filename: The full path to the image file.
  @rtype:  Blender Image
  @return: A Blender Image object with the data from I{filename}.
  """

def New (name, width, height, depth):
  """
  Create a new Image object.
  @type name: string
  @param name: The name of the new Image object.
  @type width: int
  @param width: The width of the new Image object, between 1 and 5000.
  @type height: int
  @param height: The height of the new Image object, between 1 and 5000.
  @type depth: int
  @param depth: The colour depth of the new Image object. (8:Grey, 24:RGB, 32:RGBA). (Not implimented yet, all new images will be 24bit)
  @rtype: Blender Image
  @return: A new Blender Image object.
  """

def Get (name = None):
  """
  Get the Image object(s) from Blender.
  @type name: string
  @param name: The name of the Image object.
  @rtype: Blender Image or a list of Blender Images
  @return: It depends on the I{name} parameter:
      - (name): The Image object called I{name}, None if not found;
      - (): A list with all Image objects in the current scene.
  """

def GetCurrent ():
  """
  Get the currently displayed Image from Blenders UV/Image window.
  When multiple images are displayed, the last active UV/Image windows image is used.
  @rtype: Blender Image
  @return: The Current Blender Image, If there is no current image it returns None.
  """

class Image:
  """
  The Image object
  ================
    This object gives access to Images in Blender.
  @ivar name: The name of this Image object.
  @ivar filename: The filename (path) to the image file loaded into this Image
     object.
  @ivar size: The [width, height] dimensions of the image (in pixels).
  @ivar depth: The pixel depth of the image.
  @ivar xrep: Texture tiling: the number of repetitions in the x (horizontal)
     axis.
  @ivar yrep: Texture tiling: the number of repetitions in the y (vertical)
     axis.
  @ivar start: Texture's animation start frame [0, 128].
  @ivar end: Texture's animation end frame [0, 128].
  @ivar speed: Texture's animation speed [1, 100].
  @ivar packed: Boolean, True whe the Texture is packed (readonly).
  @ivar bindcode: Texture's bind code (readonly).
  """

  def getName():
    """
    Get the name of this Image object.
    @rtype: string
    """

  def getFilename():
    """
    Get the filename of the image file loaded into this Image object.
    @rtype: string
    """

  def getSize():
    """
    Get the [width, height] dimensions (in pixels) of this image.
    @rtype: list of 2 ints
    """

  def getDepth():
    """
    Get the pixel depth of this image.
    @rtype: int
    """

  def getPixelF(x, y):
    """
    Get the the colors of the current pixel in the form [r,g,b,a].
    Returned values are floats normalized to 0.0 - 1.0.
    Pixel coordinates are in the range from 0 to N-1.  See L{getMaxXY}
    @returns: [ r, g, b, a]
    @rtype: list of 4 floats
    @type x: int
    @type y: int
    @param x:  the x coordinate of pixel.
    @param y:  the y coordinate of pixel.  
    """
  def getPixelI(x, y):
    """
    Get the the colors of the current pixel in the form [r,g,b,a].
    Returned values are ints normalized to 0 - 255.
    Pixel coordinates are in the range from 0 to N-1.  See L{getMaxXY}
    @returns: [ r, g, b, a]
    @rtype: list of 4 ints
    @type x: int
    @type y: int
    @param x:  the x coordinate of pixel.
    @param y:  the y coordinate of pixel.  
    """

  def getMaxXY():
    """
    Get the  x & y size for the image.  Image coordinates range from 0 to size-1.
    @returns: [x, y]
    @rtype: list of 2 ints
    """

  def getMinXY():
    """
    Get the x & y origin for the image. Image coordinates range from 0 to size-1.
    @returns: [x, y]
    @rtype: list of 2 ints
    """

  def getXRep():
    """
    Get the number of repetitions in the x (horizontal) axis for this Image.
    This is for texture tiling.
    @rtype: int
    """

  def getYRep():
    """
    Get the number of repetitions in the y (vertical) axis for this Image.
    This is for texture tiling.
    @rtype: int
    """

  def getBindCode():
    """
    Get the Image's bindcode.  This is for texture loading using BGL calls.
    See, for example, L{BGL.glBindTexture} and L{glLoad}.
    @rtype: int
    """

  def getStart():
    """
    Get the Image's start frame. Used for animated textures.
    @rtype: int
    """

  def getEnd():
    """
    Get the Image's end frame. Used for animated textures.
    @rtype: int
    """

  def getSpeed():
    """
    Get the Image's speed (fps). Used for animated textures.
    @rtype: int
    """

  def reload():
    """
    Reloads this image from the filesystem.  If used within a loop you need to
    redraw the Window to see the change in the image, e.g. with
    Window.RedrawAll().
    @warn: if the image file is corrupt or still being written, it will be
       replaced by a blank image in Blender, but no error will be returned.
    @returns: None
    """

  def glLoad():
    """
    Load this image's data into OpenGL texture memory, if it is not already
    loaded (image.bindcode is 0 if it is not loaded yet).
    @note: Usually you don't need to call this method.  It is only necessary
       if you want to draw textured objects in the Scripts window and the
       image's bind code is zero at that moment, otherwise Blender itself can
       take care of binding / unbinding textures.  Calling this method for an
       image with nonzero bind code simply returns the image's bind code value
       (see L{getBindCode}).
    @rtype: int
    @returns: the texture's bind code.
    """

  def glFree():
    """
    Delete this image's data from OpenGL texture memory, only (the image itself
    is not removed from Blender's memory).  Internally, glDeleteTextures (see
    L{BGL.glDeleteTextures}) is used, but this method also updates Blender's
    Image object so that its bind code is set to 0.  See also L{Image.glLoad},
    L{Image.getBindCode}.
    """

  def setName(name):
    """
    Set the name of this Image object.
    @type name: string
    @param name: The new name.
    """

  def setFilename(name):
    """
    Change the filename of this Image object.
    @type name: string
    @param name: The new full filename.
    @warn: use this with caution and note that the filename is truncated if
       larger than 160 characters.
    """

  def setXRep(xrep):
    """
    Texture tiling: set the number of x repetitions for this Image.
    @type xrep: int
    @param xrep: The new value in [1, 16].
    """

  def setYRep(yrep):
    """
    Texture tiling: set the number of y repetitions for this Image.
    @type yrep: int
    @param yrep: The new value in [1, 16].
    """

  def setStart(start):
    """
    Get the Image's start frame. Used for animated textures.
    @type start: int
    @param start: The new value in [0, 128].
    """

  def setEnd(end):
    """
    Set the Image's end frame. Used for animated textures.
    @type end: int
    @param end: The new value in [0, 128].
    """

  def setSpeed(speed):
    """
    Set the Image's speed (fps). Used for animated textures.
    @type speed: int
    @param speed: The new value in [1, 100].
    """

  def setPixelF(x, y, (r, g, b,a )):
    """
       	Set the the colors of the current pixel in the form [r,g,b,a].
       	Color values must be floats in the range 0.0 - 1.0.
    Pixel coordinates are in the range from 0 to N-1.  See L{getMaxXY}
    @type x: int
    @type y: int
    @type r: float
    @type g: float
    @type b: float
    @type a: float
    @returns: nothing
    @rtype: none
    """
    
  def setPixelI(x, y, (r, g, b, a)):
    """
       	Set the the colors of the current pixel in the form [r,g,b,a].
       	Color values must be ints in the range 0 - 255.
    Pixel coordinates are in the range from 0 to N-1.  See L{getMaxXY}
    @type x: int
    @type y: int
    @type r: int
    @type g: int
    @type b: int
    @type a: int
    @returns: nothing
    @rtype: none
    """
    
  def save():
    """
    Saves the current image.
    @returns: nothing
    @rtype: none
    """
  
  def pack():
    """
    Packs the image into the current blend file.
    @note: An error will be raised if the image is alredy packed or the filename path does not exist.
    @returns: nothing
    @rtype: none
    """

  def unpack(mode):
    """
    Unpacks the image to the images filename.
    @param mode: if 0, the existing file located at filename will be used. 1, The file will be overwritten if its different. 2, always overwrite the existing image file.
    @note: An error will be raised if the image is not packed or the filename path does not exist.
    @returns: nothing
    @rtype: none
    @type mode: int
    """