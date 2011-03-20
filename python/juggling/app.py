import curses
import time
import serial
import pygame

from juggling.sounds import SoundEffectManager
from juggling import settings
from juggling import programs
from juggling.programs import ExampleProgram

from threading import Thread

        #curses.reset_shell_mode()
        #import pdb; pdb.set_trace()
        #curses.reset_prog_mode()


# ========================[ Xbee interface ]================

class XbeePacket(object):
    """
    Data packet for our custom juggling protocol.
    """
    def __init__(self, action, ball=0, param=None):
        """
        action in ('CAUGHT', 'THROWN', ...)
        ball: 0 for addressing every ball, otherwise ball number
        param: optional data
        """
        self.action = str(action).upper()
        self.ball = int(ball)
        self.param = str(param)


class XbeeInterface(Thread):
    """
    Communication with the FT232 / Xbee
    """
    def __init__(self, engine):
        Thread.__init__(self)
        self._run = True
        self._engine = engine
        
        # Initialize USART interface
        self._interface = serial.Serial('/dev/ttyUSB0', baudrate=9600, timeout=2)

    def stop(self):
        self._run = False

    def run(self):
        while self._run:
            try:
                line = self._interface.readline().strip()
                #self._print_data_handler(line)
                data = line.split()

                if len(data) == 1: # TODO: remove data==1, this is only for testing
                    p = XbeePacket(data[0], 1)
                    self._engine.packet_received(p)

                elif len(data) == 2:
                    p = XbeePacket(data[0], int(data[1]))
                    self._engine.packet_received(p)

                elif len(data) == 3:
                    p = XbeePacket(data[0], int(data[1]), data[2])
                    self._engine.packet_received(p)

                    # TODO: show error data

            except serial.SerialTimeoutException, e:
                pass

# ========================[ Core ]================


class BallState(object):
    """
    State of a ball.
    """
    def __init__(self):
        self.voltage = 0.0
        self.in_free_fall = False
        self.throws = 0
        self.catches = 0
        self.current_program = 'default'

class Engine(object):
    """
    Core, where the packets come in and the event handlers are called.
    """
    def __init__(self):
        self.states = [ BallState() for x in range(0,10) ]
        self._packet_received_handlers = []

    def add_packet_received_handler(self, handler):
        self._packet_received_handlers.append(handler)

    def packet_received(self, packet):
        # Update ball states
        if packet.action in ('CAUGHT', 'CAUGHT*'):
            if packet.ball > 0 and packet.ball <= len(self.states):
                self.states[packet.ball - 1].catches += 1
                self.states[packet.ball - 1].in_free_fall = False

        if packet.action in ('THROWN',):
            if packet.ball > 0 and packet.ball <= len(self.states):
                self.states[packet.ball - 1].throws += 1
                self.states[packet.ball - 1].in_free_fall = True

        # Call handlers
        for h in self._packet_received_handlers:
            h(packet)


# =================[ Windows ]================

class JuggleBallStatusWindow(object):
    def __init__(self, scr, engine):
        self.scr = scr
        self.engine = engine

        self.engine.add_packet_received_handler(self.packet_received)
        self.paint()

    def packet_received(self, packet):
        self.paint()

    def paint(self):
        self.scr.clear()
        self.scr.border()
        self.scr.addstr(0, 4, "Juggling balls", curses.A_STANDOUT)
        self.scr.addstr(1, 1,       "Ball | Power | In air | Throws | Catches | Program", curses.A_UNDERLINE)
        for i in range(len(self.engine.states)):
            state = self.engine.states[i]
            self.scr.addstr(2+i, 1, "%3s  %5sv     %-3s     %5s     %5s     %s" %
                        (i+1,
                        state.voltage,
                        'Yes' if state.in_free_fall else 'No ',
                        state.throws,
                        state.catches,
                        state.current_program))

        self.scr.refresh()


class SerialWindow(object):
    def __init__(self, scr, engine):
        self.scr = scr
        self._index = 0

        self.lines = [ '' for x in range(10) ]
        engine.add_packet_received_handler(self.packet_received)
        self.paint()

    def packet_received(self, packet):
        line = 'IN %s %s %s' % (packet.action, packet.ball, packet.param)
        self.print_line(line)

    def print_line(self, line):
        self._index += 1
        self.lines = self.lines[1:] + [ '%s : %s' % (self._index, line) ]
        self.paint()

    def paint(self):
        self.scr.clear()
        self.scr.border()
        self.scr.addstr(0, 4, "Serial interface data", curses.A_STANDOUT)
        for l in range(10):
            self.scr.addstr(1+l, 1, self.lines[l])
            #self.scr.clrtoeol()
        self.scr.refresh()


class ProgramWindow(object):
    def __init__(self, scr, programs):
        self.scr = scr
        self.programs = programs
        self.paint()

    def paint(self):
        self.scr.addstr(0, 4, "Programs", curses.A_STANDOUT)

        pos = 1
        for p in self.programs:
            self.scr.addstr(pos, 1, '%s : %s' % (pos, p.description))
            pos += 1

        self.scr.refresh()

# =================[ Main app ]================

class Core(object):
    def __init__(self):
        # Initialize sound
        pygame.mixer.init(22050, -16, 2, 1024)

        self.engine = Engine()
        self.programs = [ p(self.engine) for p in programs.ALL_PROGRAMS ]

        # Initialize Xbee interface
        self.xbee_interface = XbeeInterface(self.engine)
        self.xbee_interface.start()


class App(object):
    def __init__(self, scr, core):
        self.scr = scr
        self.engine = core.engine
        self.programs = core.programs
        self.xbee_interface = core.xbee_interface

        # Create windows
        #status_win = curses.newwin(20, 40, 1, 1)
        status_win = self.scr.subwin(15, 60, 0, 0)
        self.status_window = JuggleBallStatusWindow(status_win, self.engine)

        serial_win = self.scr.subwin(13, 60, 16, 0)
        serial_win.border()
        self.serial_window = SerialWindow(serial_win, self.engine)

        program_win = self.scr.subwin(5, 80, 30, 0)
        program_win.border()
        self.program_window = ProgramWindow(program_win, self.programs)

        self.refresh()

    def _xbee_data_print(self, line):
        self.serial_window.print_line(line)
    write = _xbee_data_print

    def refresh(self):
        self.scr.refresh()

    def exit(self):
        # Quit Xbee interface
        self.xbee_interface.stop()
        self.xbee_interface.join()

    def handle_input(self):
        while True:
            c = self.scr.getch()

            self.write('char ' + str(c))

            if c == ord('q'):
                self.exit()
                return

            elif c == 9: # Tab
                pass



#pygame.mixer.init(22050, -16, 2, 1024)
#serial.Serial('/dev/ttyUSB0', baudrate=9600, timeout=2)

core = Core()
def main(scr):
    a = App(scr, core)
    a.handle_input()

print curses.wrapper(main)
print 'Exited'
import traceback; traceback.print_exc()
