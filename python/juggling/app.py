import curses
import time
import serial
import pygame

from juggling.sounds import SoundEffectManager
from juggling import settings
from juggling import programs
from juggling.programs import ExampleProgram

from threading import Thread

class JuggleBallStatusWindow(object):
    def __init__(self, scr):
        self.scr = scr
        self.load_statusses()

    def load_statusses(self):
        self.scr.addstr(0, 0, "Juggling balls", curses.A_STANDOUT)
        self.scr.addstr(1, 0, "Item  -  Power  -  Free fall  - Catches", curses.A_UNDERLINE)
        for x in range(10):
            self.scr.addstr(2+x, 0, "1     100%     -  No")

        self.scr.refresh()

class SerialWindow(object):
    def __init__(self, scr):
        self.scr = scr
        self._position = 0
        self.load_window()

    def load_window(self):
        self.scr.addstr(0, 0, "Serial interface data", curses.A_STANDOUT)
        self.print_line('test')
        self.print_line('test 2')
        self.scr.refresh()

    def print_line(self, line):
        self.scr.addstr(1+self._position, 0, '%s : %s' % (time.time(), line))
        self._position = (self._position + 1) % 10
        self.scr.addstr(1+self._position, 0, ' ' * 40)
        self.scr.refresh()



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
    def __init__(self, callback, print_data_handler=None):
        Thread.__init__(self)
        self._run = True
        self._callback = callback
        self._print_data_handler = print_data_handler or (lambda d: None)
        
        # Initialize USART interface
        self._interface = serial.Serial('/dev/ttyUSB0', baudrate=9600, timeout=2)
        print_data_handler('test44')

    def stop(self):
        self._run = False

    def run(self):
        while self._run:
            try:
                line = self._interface.readline().strip()
                self._print_data_handler(line)
                data = line.split()

                if len(data) == 1: # TODO: remove data==1, this is only for testing
                    p = XbeePacket(data[0], 1)
                    self._callback(p)

                elif len(data) == 2:
                    p = XbeePacket(data[0], int(data[1]))
                    self._callback(p)

                elif len(data) == 3:
                    p = XbeePacket(data[0], int(data[1]), data[2])
                    self._callback(p)
            except serial.SerialTimeoutException, e:
                pass

    def send_packet(self, packet):
        line = '%s %s %s\r\n' % (packet.action, packet.ball, packet.param)
        self._interface.write(line)


class App(object):
    def __init__(self):
        # Initialise ncurses
        self.scr = curses.initscr()
        curses.noecho()
        curses.cbreak()
        self.scr.keypad(1)
        curses.curs_set(0)

        # Create windows
        #status_win = curses.newwin(20, 40, 1, 1)
        status_win = self.scr.subwin(15, 40, 1, 1)
        self.juggle_ball_status_window = JuggleBallStatusWindow(status_win)

        serial_win = self.scr.subwin(15, 40, 15, 1)
        self.serial_window = SerialWindow(serial_win)

        self.refresh()

        # Initialize sound
        pygame.mixer.init(22050, -16, 2, 1024)

        # Initialize Xbee interface
        self.xbee_interface = XbeeInterface(self._xbee_callback, self._xbee_data_print)
        self.xbee_interface.start()

        self.current_program = getattr(programs, settings.DEFAULT_PROGRAM)()

    def _xbee_callback(self, data):
        self.current_program.process_data(data)

    def _xbee_data_print(self, line):
        self.serial_window.print_line(line)

    def refresh(self):
        self.scr.refresh()

    def exit(self):
        # Exit curses library
        curses.nocbreak()
        self.scr.keypad(0)
        curses.curs_set(1)
        curses.echo()
        curses.endwin()

        # Quit Xbee interface
        self.xbee_interface.stop()
        self.xbee_interface.join()
        print 'Exited'

    def handle_input(self):
        while True:
            c = self.scr.getch()

            if c == ord('q'):
                self.exit()
                return

            elif c == ord('a'):
                self.program.threw_ball(0)

            elif c == ord('b'):
                self.program.threw_ball(1)


a = App()
a.handle_input()
