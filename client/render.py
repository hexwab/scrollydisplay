# -*- coding: utf-8 -*-
import cairo
import pango
import pangocairo
#from pyfreetypecairotest import create_cairo_font_face_for_file
import sys
import subprocess


import ctypes as ct
import cairo

_initialized = False
def create_cairo_font_face_for_file (filename, faceindex=0, loadoptions=0):
    "given the name of a font file, and optional faceindex to pass to FT_New_Face" \
    " and loadoptions to pass to cairo_ft_font_face_create_for_ft_face, creates" \
    " a cairo.FontFace object that may be used to render text with that font."
    global _initialized
    global _freetype_so
    global _cairo_so
    global _ft_lib
    global _ft_destroy_key
    global _surface

    CAIRO_STATUS_SUCCESS = 0
    FT_Err_Ok = 0

    if not _initialized:
        # find shared objects
        _freetype_so = ct.CDLL("libfreetype.so.6")
        _cairo_so = ct.CDLL("libcairo.so.2")
        _cairo_so.cairo_ft_font_face_create_for_ft_face.restype = ct.c_void_p
        _cairo_so.cairo_ft_font_face_create_for_ft_face.argtypes = [ ct.c_void_p, ct.c_int ]
        _cairo_so.cairo_font_face_get_user_data.restype = ct.c_void_p
        _cairo_so.cairo_font_face_set_user_data.argtypes = (ct.c_void_p, ct.c_void_p, ct.c_void_p, ct.c_void_p)
        _cairo_so.cairo_set_font_face.argtypes = [ ct.c_void_p, ct.c_void_p ]
        _cairo_so.cairo_font_face_status.argtypes = [ ct.c_void_p ]
        _cairo_so.cairo_font_face_destroy.argtypes = (ct.c_void_p,)
        _cairo_so.cairo_status.argtypes = [ ct.c_void_p ]
        # initialize freetype
        _ft_lib = ct.c_void_p()
        status = _freetype_so.FT_Init_FreeType(ct.byref(_ft_lib))
        if  status != FT_Err_Ok :
            raise RuntimeError("Error %d initializing FreeType library." % status)
        #end if

        class PycairoContext(ct.Structure):
            _fields_ = \
                [
                    ("PyObject_HEAD", ct.c_byte * object.__basicsize__),
                    ("ctx", ct.c_void_p),
                    ("base", ct.c_void_p),
                ]
        #end PycairoContext

        _surface = cairo.ImageSurface(cairo.FORMAT_A8, 0, 0)
        _ft_destroy_key = ct.c_int() # dummy address
        _initialized = True
    #end if

    ft_face = ct.c_void_p()
    cr_face = None
    try :
        # load FreeType face
        status = _freetype_so.FT_New_Face(_ft_lib, filename.encode("utf-8"), faceindex, ct.byref(ft_face))
        if status != FT_Err_Ok :
            raise RuntimeError("Error %d creating FreeType font face for %s" % (status, filename))
        #end if

        # create Cairo font face for freetype face
        cr_face = _cairo_so.cairo_ft_font_face_create_for_ft_face(ft_face, loadoptions)
        status = _cairo_so.cairo_font_face_status(cr_face)
        if status != CAIRO_STATUS_SUCCESS :
            raise RuntimeError("Error %d creating cairo font face for %s" % (status, filename))
        #end if
        # Problem: Cairo doesn't know to call FT_Done_Face when its font_face object is
        # destroyed, so we have to do that for it, by attaching a cleanup callback to
        # the font_face. This only needs to be done once for each font face, while
        # cairo_ft_font_face_create_for_ft_face will return the same font_face if called
        # twice with the same FT Face.
        # The following check for whether the cleanup has been attached or not is
        # actually unnecessary in our situation, because each call to FT_New_Face
        # will return a new FT Face, but we include it here to show how to handle the
        # general case.
        if _cairo_so.cairo_font_face_get_user_data(cr_face, ct.byref(_ft_destroy_key)) == None :
            status = _cairo_so.cairo_font_face_set_user_data \
              (
                cr_face,
                ct.byref(_ft_destroy_key),
                ft_face,
                _freetype_so.FT_Done_Face
              )
            if status != CAIRO_STATUS_SUCCESS :
                raise RuntimeError("Error %d doing user_data dance for %s" % (status, filename))
            #end if
            ft_face = None # Cairo has stolen my reference
        #end if

        # set Cairo font face into Cairo context
        cairo_ctx = cairo.Context(_surface)
        cairo_t = PycairoContext.from_address(id(cairo_ctx)).ctx
        _cairo_so.cairo_set_font_face(cairo_t, cr_face)
        status = _cairo_so.cairo_font_face_status(cairo_t)
        if status != CAIRO_STATUS_SUCCESS :
            raise RuntimeError("Error %d creating cairo font face for %s" % (status, filename))
        #end if

    finally :
        _cairo_so.cairo_font_face_destroy(cr_face)
        _freetype_so.FT_Done_Face(ft_face)
    #end try

    # get back Cairo font face as a Python object
    face = cairo_ctx.get_font_face()
    return face
#end create_cairo_font_face_for_file

#face = create_cairo_font_face_for_file("/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf", 0)
#face = create_cairo_font_face_for_file("HelveticaInserat-Roman.otf", 0)
#face = create_cairo_font_face_for_file("Stark-SemiBld.otf", 0)
#face = create_cairo_font_face_for_file("AngroEF-Light.otf", 0)
#face = create_cairo_font_face_for_file("Spartan LT Book Classified.ttf", 0) # 2 10 9
#face = create_cairo_font_face_for_file("Spartan LT Heavy Classified.ttf", 0) # 2 10 9

# these two!
face = create_cairo_font_face_for_file("AngroEF-Bold.otf", 0) # 2 10 9
#face = create_cairo_font_face_for_file("Hobo Regular.ttf", 0) # 3 10 9

#surf = cairo.ImageSurface(cairo.FORMAT_RGB24, 120, 7)
#ctx = cairo.Context(surf)

text = subprocess.check_output(['fortune'])
text = "Pack my box with five dozen liquor jugs QWERTY 1234547890 ():{}"

speed = 24
x = -120/speed

def drawtext(ctx):

    scale = 16
    scsurf = cairo.ImageSurface(cairo.FORMAT_RGB24, 120*scale, 7)
    scctx = cairo.Context(scsurf)
    global x, text
    scctx.set_source_rgb(1, 1, 1)
    
    scctx.translate(-x,2)
    fo = cairo.FontOptions()
    fo.set_hint_metrics(cairo.HINT_STYLE_FULL)
    fo.set_hint_style(cairo.HINT_METRICS_OFF)
    fo.set_antialias(cairo.ANTIALIAS_NONE)
    scctx.set_font_options(fo)

#    scctx.set_antialias(cairo.ANTIALIAS_NONE)
    scctx.set_font_face(face)
    scctx.scale(scale,1)
    scctx.set_font_matrix(cairo.Matrix(xx=10,yy=9))
    scctx.move_to(0, 4)
    scctx.show_text(text)
    adv = scctx.text_extents(text)[4] # x_advance
    #print adv,x*speed
    ctx.set_source_surface(scsurf, 0, 0)

    ctx.get_source().set_matrix(cairo.Matrix(xx=scale,yy=1))
    ctx.paint()
    x = x + speed
    if x > adv*scale:
        x = -120*scale
        text = subprocess.check_output(['fortune'])
