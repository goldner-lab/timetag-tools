import time
import pkgutil

from gi.repository import Gtk, GLib
import matplotlib
from matplotlib.figure import Figure
from matplotlib.backends.backend_gtk3agg import FigureCanvasGTK3Agg
from collections import defaultdict

from timetag.binner import FretHistBinner
from timetag.managed_binner import ManagedBinner

class FretHistPlot(ManagedBinner):
        FigureCanvas = FigureCanvasGTK3Agg

        def __init__(self, pipeline):
                self.pipeline = pipeline
                self.builder = Gtk.Builder()
                src = pkgutil.get_data('timetag', 'fret_hist.glade')
                self.builder.add_from_string(src)
                self.builder.connect_signals(self)
                self.win = self.builder.get_object('hist_window')
                self.win.connect('destroy', self.destroy_cb)
                self.update_rate = 0.3 # Hz

                self.figure = Figure()
                self.axes = self.figure.add_subplot(111)
                self.axes.get_xaxis().set_major_formatter(
                                matplotlib.ticker.ScalarFormatter(useOffset=False))

                canvas = self.__class__.FigureCanvas(self.figure)
                self.builder.get_object('plot_container').pack_start(canvas,True,True,0)
                self.win.show_all()
                ManagedBinner.__init__(self, self.pipeline, "fret-hist-plot")

        def create_binner(self):
                get_obj = self.builder.get_object
                binner = FretHistBinner(self.bin_time, self.pipeline.clockrate)
                binner.donor_channel = get_obj('donor_combo').get_active()
                binner.acceptor_channel = get_obj('acceptor_combo').get_active()
                binner.threshold = get_obj('threshold').get_value()
                binner.hist_width = 1. / get_obj('nbins').get_value()
                return binner
                
        def on_started(self):
                GLib.timeout_add(int(1000.0 / self.update_rate), self._update_plot,
                                    priority=GLib.PRIORITY_DEFAULT_IDLE)

        @property
        def bin_time(self):
                return 1e-3 * self.builder.get_object('bin_time').props.value

        def destroy_cb(self, a):
                self.stop_binner()
                Gtk.main_quit()

        def _update_plot(self):
                binner = self.get_binner()
                if binner is None: return False
                hist_width = binner.hist_width
                hist = binner.hist
                if len(hist) == 0: return True
                self.axes.cla()
                self.axes.bar(hist.keys(), hist.values(), hist_width)
                self.axes.relim()
                self.axes.set_xlim(0, 1)
                self.axes.set_xlabel('FRET efficiency')
                self.figure.canvas.draw()
                return True

        def binning_config_changed_cb(self, *args):
                self.restart_binner()

        def bin_time_changed_cb(self, adj):
                self.restart_binner()

