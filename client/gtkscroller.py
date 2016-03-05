from render import drawtext

import gtk
import gobject
import math

class PyApp(gtk.Window):

    def __init__(self):
        super(PyApp, self).__init__()

        #self.set_title("Simple drawing")
        self.resize(120, 7)
        self.set_position(gtk.WIN_POS_CENTER)
        self.connect("destroy", gtk.main_quit)

        darea = gtk.DrawingArea()
        darea.connect("expose-event", self.expose)
        self.add(darea)
        self.show_all()
        timer = gobject.timeout_add(10, self.frame, darea)

    def expose(self, widget, event):
        ctx = widget.window.cairo_create()
        drawtext(ctx)

    def frame(self,darea):
        darea.queue_draw()
        return True

PyApp()
gtk.main()
