#!/usr/bin/env python3

import os
import struct
import binascii
import traceback
import gi
import hid


gi.require_version("Gtk", "3.0")
from gi.repository import Gtk

VID = 0xCAFE
PID = 0xBADA
CONFIG_SIZE = 26
REPORT_ID = 3
CONFIG_VERSION = 1
PICTURE_FILENAME = os.path.join(os.path.dirname(__file__), "trackball.png")

SENSOR_FUNCTIONS = (
    ("None", "0"),
    ("Cursor X", "1"),
    ("Cursor Y", "2"),
    ("V scroll", "3"),
    ("H scroll", "4"),
    ("Cursor X (inverted)", "-1"),
    ("Cursor Y (inverted)", "-2"),
    ("V scroll (inverted)", "-3"),
    ("H scroll (inverted)", "-4"),
)

BUTTON_FUNCTIONS = (
    ("None", "0"),
    ("Button 1 (left)", "1"),
    ("Button 2 (right)", "2"),
    ("Button 3 (middle)", "3"),
    ("Button 4 (back)", "4"),
    ("Button 5 (forward)", "5"),
    ("Button 6", "6"),
    ("Button 7", "7"),
    ("Button 8", "8"),
    ("Click-drag", "9"),
    ("Shift", "10"),
)


def make_model(options):
    model = Gtk.ListStore(str, str)
    for o in options:
        model.append(o)
    return model


def make_dropdown(model):
    dropdown = Gtk.ComboBox.new_with_model(model)
    renderer_text = Gtk.CellRendererText()
    dropdown.pack_start(renderer_text, True)
    dropdown.add_attribute(renderer_text, "text", 0)
    dropdown.set_id_column(1)
    dropdown.set_active_id("0")
    return dropdown


def make_scale():
    scale = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, 1, 120, 1)
    scale.connect("format-value", lambda _, value: str(int(value) * 100))
    return scale


class TrackballConfigWindow(Gtk.Window):
    def __init__(self):
        sensor_function_model = make_model(SENSOR_FUNCTIONS)
        button_function_model = make_model(BUTTON_FUNCTIONS)
        self.devices_model = Gtk.ListStore(str, str)

        Gtk.Window.__init__(self, title="Trackball Configuration")

        self.set_border_width(10)

        hbox = Gtk.Box()

        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)

        devices_hbox = Gtk.Box()
        self.devices_dropdown = make_dropdown(self.devices_model)
        devices_hbox.pack_start(self.devices_dropdown, True, True, 0)
        refresh_button = Gtk.Button.new_from_icon_name(
            "view-refresh", Gtk.IconSize.BUTTON
        )
        refresh_button.set_tooltip_text("Refresh device list")
        refresh_button.connect("clicked", self.refresh_button_clicked)
        devices_hbox.pack_start(refresh_button, False, False, 0)
        vbox.pack_start(devices_hbox, True, True, 0)

        actions_hbox = Gtk.Box()
        self.load_button = Gtk.Button.new_with_label("Load from device")
        self.load_button.connect("clicked", self.load_button_clicked)
        actions_hbox.pack_start(self.load_button, True, True, 0)
        self.save_button = Gtk.Button.new_with_label("Save to device")
        self.save_button.connect("clicked", self.save_button_clicked)
        actions_hbox.pack_start(self.save_button, True, True, 0)
        vbox.pack_start(actions_hbox, True, True, 0)

        grid = Gtk.Grid(column_spacing=6, row_spacing=6)

        row = 0
        grid.attach(Gtk.Label("Normal", halign=Gtk.Align.CENTER), 1, row, 1, 1)
        grid.attach(Gtk.Label("Shifted", halign=Gtk.Align.CENTER), 2, row, 1, 1)
        row += 1
        grid.attach(Gtk.Label("Sensor 1 X axis", halign=Gtk.Align.END), 0, row, 1, 1)
        self.sensor1_x_dropdown = make_dropdown(sensor_function_model)
        grid.attach(self.sensor1_x_dropdown, 1, row, 1, 1)
        self.sensor1_x_shifted_dropdown = make_dropdown(sensor_function_model)
        grid.attach(self.sensor1_x_shifted_dropdown, 2, row, 1, 1)
        row += 1
        grid.attach(Gtk.Label("Sensor 1 Y axis", halign=Gtk.Align.END), 0, row, 1, 1)
        self.sensor1_y_dropdown = make_dropdown(sensor_function_model)
        grid.attach(self.sensor1_y_dropdown, 1, row, 1, 1)
        self.sensor1_y_shifted_dropdown = make_dropdown(sensor_function_model)
        grid.attach(self.sensor1_y_shifted_dropdown, 2, row, 1, 1)
        row += 1
        grid.attach(Gtk.Label("Sensor 1 CPI", halign=Gtk.Align.END), 0, row, 1, 1)
        self.sensor1_cpi = make_scale()
        grid.attach(self.sensor1_cpi, 1, row, 1, 1)
        self.sensor1_cpi_shifted = make_scale()
        grid.attach(self.sensor1_cpi_shifted, 2, row, 1, 1)
        row += 1
        grid.attach(Gtk.Label("Sensor 2 X axis", halign=Gtk.Align.END), 0, row, 1, 1)
        self.sensor2_x_dropdown = make_dropdown(sensor_function_model)
        grid.attach(self.sensor2_x_dropdown, 1, row, 1, 1)
        self.sensor2_x_shifted_dropdown = make_dropdown(sensor_function_model)
        grid.attach(self.sensor2_x_shifted_dropdown, 2, row, 1, 1)
        row += 1
        grid.attach(Gtk.Label("Sensor 2 Y axis", halign=Gtk.Align.END), 0, row, 1, 1)
        self.sensor2_y_dropdown = make_dropdown(sensor_function_model)
        grid.attach(self.sensor2_y_dropdown, 1, row, 1, 1)
        self.sensor2_y_shifted_dropdown = make_dropdown(sensor_function_model)
        grid.attach(self.sensor2_y_shifted_dropdown, 2, row, 1, 1)
        row += 1
        grid.attach(Gtk.Label("Sensor 2 CPI", halign=Gtk.Align.END), 0, row, 1, 1)
        self.sensor2_cpi = make_scale()
        grid.attach(self.sensor2_cpi, 1, row, 1, 1)
        self.sensor2_cpi_shifted = make_scale()
        grid.attach(self.sensor2_cpi_shifted, 2, row, 1, 1)
        row += 1
        grid.attach(Gtk.Label("Button 1", halign=Gtk.Align.END), 0, row, 1, 1)
        self.button1_dropdown = make_dropdown(button_function_model)
        grid.attach(self.button1_dropdown, 1, row, 1, 1)
        self.button1_shifted_dropdown = make_dropdown(button_function_model)
        grid.attach(self.button1_shifted_dropdown, 2, row, 1, 1)
        row += 1
        grid.attach(Gtk.Label("Button 2", halign=Gtk.Align.END), 0, row, 1, 1)
        self.button2_dropdown = make_dropdown(button_function_model)
        grid.attach(self.button2_dropdown, 1, row, 1, 1)
        self.button2_shifted_dropdown = make_dropdown(button_function_model)
        grid.attach(self.button2_shifted_dropdown, 2, row, 1, 1)
        row += 1
        grid.attach(Gtk.Label("Button 3", halign=Gtk.Align.END), 0, row, 1, 1)
        self.button3_dropdown = make_dropdown(button_function_model)
        grid.attach(self.button3_dropdown, 1, row, 1, 1)
        self.button3_shifted_dropdown = make_dropdown(button_function_model)
        grid.attach(self.button3_shifted_dropdown, 2, row, 1, 1)
        row += 1
        grid.attach(Gtk.Label("Button 4", halign=Gtk.Align.END), 0, row, 1, 1)
        self.button4_dropdown = make_dropdown(button_function_model)
        grid.attach(self.button4_dropdown, 1, row, 1, 1)
        self.button4_shifted_dropdown = make_dropdown(button_function_model)
        grid.attach(self.button4_shifted_dropdown, 2, row, 1, 1)

        vbox.pack_start(grid, True, True, 0)

        hbox.pack_start(vbox, True, True, 10)

        image = Gtk.Image.new_from_file(PICTURE_FILENAME)
        hbox.pack_start(image, True, True, 10)

        self.refresh_device_list()

        self.add(hbox)

    def wrap_exception_in_dialog(self, f):
        try:
            f()
        except Exception as e:
            dialog = Gtk.MessageDialog(
                transient_for=self,
                flags=0,
                message_type=Gtk.MessageType.ERROR,
                buttons=Gtk.ButtonsType.OK,
                text=traceback.format_exc(),
            )
            dialog.run()
            dialog.destroy()

    def refresh_button_clicked(self, button):
        self.refresh_device_list()

    def refresh_device_list(self):
        self.devices_model.clear()
        devices = [
            d
            for d in hid.enumerate()
            if d["vendor_id"] == VID and d["product_id"] == PID
        ]
        if devices:
            for d in devices:
                self.devices_model.append(
                    (
                        f"{d['manufacturer_string']} {d['product_string']}",
                        str(d["path"], "ascii"),
                    )
                )
            self.load_button.set_sensitive(True)
            self.save_button.set_sensitive(True)
        else:
            self.devices_model.append(("No devices found", "NULL"))
            self.load_button.set_sensitive(False)
            self.save_button.set_sensitive(False)
        self.devices_dropdown.set_active(0)

    def load_button_clicked(self, button):
        self.wrap_exception_in_dialog(self.load_config_from_device)

    def load_config_from_device(self):
        path = self.devices_dropdown.get_active_id()
        device = hid.Device(path=bytes(path, "ascii"))
        data = device.get_feature_report(REPORT_ID, CONFIG_SIZE + 1)
        (
            report_id,
            version,
            command,
            sensor1_x,
            sensor1_y,
            sensor2_x,
            sensor2_y,
            sensor1_x_shifted,
            sensor1_y_shifted,
            sensor2_x_shifted,
            sensor2_y_shifted,
            sensor1_cpi,
            sensor2_cpi,
            sensor1_cpi_shifted,
            sensor2_cpi_shifted,
            button1,
            button2,
            button3,
            button4,
            button1_shifted,
            button2_shifted,
            button3_shifted,
            button4_shifted,
            crc32,
        ) = struct.unpack("<BBb4b4b2B2B4b4bL", data)
        self.sensor1_x_dropdown.set_active_id(str(sensor1_x))
        self.sensor1_x_shifted_dropdown.set_active_id(str(sensor1_x_shifted))
        self.sensor1_y_dropdown.set_active_id(str(sensor1_y))
        self.sensor1_y_shifted_dropdown.set_active_id(str(sensor1_y_shifted))
        self.sensor2_x_dropdown.set_active_id(str(sensor2_x))
        self.sensor2_x_shifted_dropdown.set_active_id(str(sensor2_x_shifted))
        self.sensor2_y_dropdown.set_active_id(str(sensor2_y))
        self.sensor2_y_shifted_dropdown.set_active_id(str(sensor2_y_shifted))
        self.button1_dropdown.set_active_id(str(button1))
        self.button1_shifted_dropdown.set_active_id(str(button1_shifted))
        self.button2_dropdown.set_active_id(str(button2))
        self.button2_shifted_dropdown.set_active_id(str(button2_shifted))
        self.button3_dropdown.set_active_id(str(button3))
        self.button3_shifted_dropdown.set_active_id(str(button3_shifted))
        self.button4_dropdown.set_active_id(str(button4))
        self.button4_shifted_dropdown.set_active_id(str(button4_shifted))
        self.sensor1_cpi.set_value(sensor1_cpi)
        self.sensor1_cpi_shifted.set_value(sensor1_cpi_shifted)
        self.sensor2_cpi.set_value(sensor2_cpi)
        self.sensor2_cpi_shifted.set_value(sensor2_cpi_shifted)

    def save_button_clicked(self, button):
        self.wrap_exception_in_dialog(self.save_config_to_device)

    def save_config_to_device(self):
        command = 0
        sensor1_x = int(self.sensor1_x_dropdown.get_active_id())
        sensor1_x_shifted = int(self.sensor1_x_shifted_dropdown.get_active_id())
        sensor1_y = int(self.sensor1_y_dropdown.get_active_id())
        sensor1_y_shifted = int(self.sensor1_y_shifted_dropdown.get_active_id())
        sensor2_x = int(self.sensor2_x_dropdown.get_active_id())
        sensor2_x_shifted = int(self.sensor2_x_shifted_dropdown.get_active_id())
        sensor2_y = int(self.sensor2_y_dropdown.get_active_id())
        sensor2_y_shifted = int(self.sensor2_y_shifted_dropdown.get_active_id())
        button1 = int(self.button1_dropdown.get_active_id())
        button1_shifted = int(self.button1_shifted_dropdown.get_active_id())
        button2 = int(self.button2_dropdown.get_active_id())
        button2_shifted = int(self.button2_shifted_dropdown.get_active_id())
        button3 = int(self.button3_dropdown.get_active_id())
        button3_shifted = int(self.button3_shifted_dropdown.get_active_id())
        button4 = int(self.button4_dropdown.get_active_id())
        button4_shifted = int(self.button4_shifted_dropdown.get_active_id())
        sensor1_cpi = int(self.sensor1_cpi.get_value())
        sensor1_cpi_shifted = int(self.sensor1_cpi_shifted.get_value())
        sensor2_cpi = int(self.sensor2_cpi.get_value())
        sensor2_cpi_shifted = int(self.sensor2_cpi_shifted.get_value())

        data = struct.pack(
            "<BBb4b4b2B2B4b4b",
            REPORT_ID,
            CONFIG_VERSION,
            command,
            sensor1_x,
            sensor1_y,
            sensor2_x,
            sensor2_y,
            sensor1_x_shifted,
            sensor1_y_shifted,
            sensor2_x_shifted,
            sensor2_y_shifted,
            sensor1_cpi,
            sensor2_cpi,
            sensor1_cpi_shifted,
            sensor2_cpi_shifted,
            button1,
            button2,
            button3,
            button4,
            button1_shifted,
            button2_shifted,
            button3_shifted,
            button4_shifted,
        )
        crc32 = binascii.crc32(data[1:])
        crc_bytes = struct.pack("<L", crc32)
        data += crc_bytes

        path = self.devices_dropdown.get_active_id()
        device = hid.Device(path=bytes(path, "ascii"))
        device.send_feature_report(data)


def main():
    win = TrackballConfigWindow()
    win.connect("delete-event", Gtk.main_quit)
    win.show_all()
    Gtk.main()


if __name__ == "__main__":
    main()
