import time
import pkgutil

from gi.repository import Gtk, Gdk, GLib
import matplotlib
from matplotlib.figure import Figure
from matplotlib.backends.backend_gtk3agg import FigureCanvasGTK3Agg
from collections import defaultdict

from timetag.binner import HistBinner
from timetag.managed_binner import ManagedBinner
from timetag import config

def fix_color(c):
        r = Gdk.RGBA()
        r.parse(c)
        return (r.red, r.green, r.blue)

class HistPlot(ManagedBinner):
        FigureCanvas = FigureCanvasGTK3Agg

        def __init__(self, pipeline):
                self.builder = Gtk.Builder()
                src = pkgutil.get_data('timetag', 'hist.glade')
                self.builder.add_from_string(src)
                self.builder.connect_signals(self)
                self.win = self.builder.get_object('hist_window')
                self.win.connect('destroy', self.destroy_cb)
                self.pipeline = pipeline
                self.update_rate = 0.3 # Hz

                rc = config.load_rc()
                self.colors = {n: fix_color(chan.color)
                               for (n,chan) in enumerate(rc['strobe-channels'])
                               if chan.enabled
                               }
                self.figure = Figure(tight_layout=True)
                self.axes = {}
                for n in self.colors:
                        axes = self.figure.add_subplot(len(self.colors),1,n+1)
                        axes.get_xaxis().set_major_formatter(
                                        matplotlib.ticker.ScalarFormatter(useOffset=False))
                        self.axes[n] = axes
                axes.set_xlabel('Photons')

                canvas = self.__class__.FigureCanvas(self.figure)
                self.builder.get_object('plot_container').pack_start(canvas,True,True,0)
                self.win.show_all()
                ManagedBinner.__init__(self, self.pipeline, 'hist-plot')

        def create_binner(self):
                return HistBinner(bin_time = self.bin_time,
                                  clockrate = self.pipeline.clockrate,
                                  hist_width = self.hist_width
                                  )

        def on_started(self):
                GLib.timeout_add(int(1000.0 / self.update_rate), self._update_plot,
                                    priority=GLib.PRIORITY_DEFAULT_IDLE)

        def destroy_cb(self, a):
                self.stop_binner()
                Gtk.main_quit()

        def _update_plot(self):
                if self.get_binner() is None: return False
                for c,hist in enumerate(self.get_binner().channels):
                        if len(hist) == 0: continue
                        if c not in self.axes: continue
                        self.axes[c].cla()
                        self.axes[c].bar(hist.keys(), hist.values(),
                                         self.hist_width, color=self.colors[c])
                        self.axes[c].relim()
                        last_axis=c
                self.axes[last_axis].set_xlabel('Photons')

                self.figure.canvas.draw()
                return True

        @property
        def bin_time(self):
                return 1e-3 * self.builder.get_object('bin_width').props.value

        @property
        def hist_width(self):
                return self.builder.get_object('hist_width').props.value

        def bin_width_changed_cb(self, adj):
                self.restart_binner()

        def hist_width_changed_cb(self, adj):
                self.restart_binner()

