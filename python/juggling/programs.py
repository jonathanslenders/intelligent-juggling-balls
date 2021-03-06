
from threading import Thread
from juggling import settings


import colorsys
import pygame
import time

class Program(object):
    """
    Feedback
    """
    def __init__(self, engine):
        self.engine = engine
        self.active_flag = False
        self.initialize(engine)

    def initialize(self, engine):
        """
        Initialization code for this program.
        """
        pass

    @property
    def description(self):
        return self.__doc__.replace('\t', ' ').replace('\n', ' ').replace('  ', ' ')

    def activate(self):
        """ Activate program """
        self.active_flag = True

    def deactivate(self):
        """ Deactivate program """
        self.active_flag = False

    @property
    def is_active(self):
        return self.active_flag


class ExampleProgram(Program):
    """ Test effect: make sound while a ball is in the air. """
    def initialize(self, engine):
        # TODO: send program parameters to all balls.

        # Create sounds
        #self.do = pygame.mixer.Sound("sounds/do.wav")
        #self.caught_sound = pygame.mixer.Sound("sounds/catch.wav")

        self.fly = pygame.mixer.Sound("sounds/Missile Fly By 01.wav")

            # Using a list of the same effect allows us to continue playing the
            # first while a second starts. (up to three now).
        self.caught_sounds = [ pygame.mixer.Sound("sounds/Explosion 01.wav"),
                pygame.mixer.Sound("sounds/Explosion 01.wav"),
                pygame.mixer.Sound("sounds/Explosion 01.wav")
                ]
        self.caught_sounds_index = 0
        self.last_throw = time.time()

        engine.add_packet_received_handler(self.packet_received)

    def packet_received(self, packet):
        if self.is_active:
            if packet.action in ('CAUGHT', 'CAUGHT*'):
                self.fly.stop()

                # Volume depends on throw duration
                if packet.action == 'CAUGHT':
                    duration = min(2, time.time() - self.last_throw) / 2
                    self.caught_sounds_index = (self.caught_sounds_index + 1) % 3
                    self.caught_sounds[self.caught_sounds_index].play()
                    self.caught_sounds[self.caught_sounds_index].set_volume(duration)

            if packet.action == 'THROWN':
                self.fly.play()
                self.fly.set_volume(.2)
                self.last_throw = time.time()

    def activate(self):
        Program.activate(self)
        self.engine.send_packet('RUN', 0, 'alternate')


class GuitarProgram(Program):
    """ Guitar effect: make sound while a ball is in the air. """
    def initialize(self, engine):
        # Create sounds
        self.notes = {
            'c': pygame.mixer.Sound("sounds/guitar_eli/c_low.wav"),
            'd': pygame.mixer.Sound("sounds/guitar_eli/d_low.wav"),
            'e': pygame.mixer.Sound("sounds/guitar_eli/e_low.wav"),
            'f': pygame.mixer.Sound("sounds/guitar_eli/f_low.wav"),
            'g': pygame.mixer.Sound("sounds/guitar_eli/g_low.wav"),
            'a': pygame.mixer.Sound("sounds/guitar_eli/a_low.wav"),
            'b': pygame.mixer.Sound("sounds/guitar_eli/b_low.wav"),

            'c2': pygame.mixer.Sound("sounds/guitar_eli/c_high.wav"),
            'd2': pygame.mixer.Sound("sounds/guitar_eli/d_high.wav"),
            'e2': pygame.mixer.Sound("sounds/guitar_eli/e_high.wav"),
            'f2': pygame.mixer.Sound("sounds/guitar_eli/f_high.wav"),
            'g2': pygame.mixer.Sound("sounds/guitar_eli/g_high.wav"),
            'a2': pygame.mixer.Sound("sounds/guitar_eli/a_high.wav"),
            'b2': pygame.mixer.Sound("sounds/guitar_eli/b_high.wav"),
            }

        self.tune = [ # Wild mountain thyme
            'g', # Oh
            'e', # the
            'd', # sum
            'd', # mer
            'e', # time
            'g', # is 
            'g', # com
            'g', # ming
            'b', # and 
            'd2', # the
            'e2', # trees 
            'd2', # are
            'e2', # sweet-
            'd2', # ly
            'b', # bloom
            'd2', # ing
            'b', # and
            'd2', # the
            'e2', # wild
            'd2', # moun
            'b', # tain
            'a', # thyme
            #'a', # thyme
            'g', # ..
            'a', # grows
            'b', # a
            'c2', # round
            'b', # the
            'a', # bloom
            'g', # ing
            'e', # eath
            'g', # er
            # TODO: finish
            ]
        self.play_index = 0

        self.tune_sound_of_music_doremi = [ 'c', 'd', 'e', 'c', 'e', 'c', 'e', 'd','e','f','f','d','e','f',
                    'e', 'f','g','f','g','f','g', 'f','g','a','a','f','g','a' ]

        engine.add_packet_received_handler(self.packet_received)

        pygame.mixer.set_num_channels(len(self.notes.keys()))
        for i in range(0, len(self.notes.keys())):
            balance = float(i) / float(len(self.notes.keys()))
            pygame.mixer.Channel(i).set_volume(balance, 1-balance)


    def packet_received(self, packet):
        if self.is_active:
            if packet.action in ('CAUGHT', 'CAUGHT*'):
                pass
                #self.e.stop()

            if packet.action == 'THROWN':
                key = self.tune[self.play_index]

                index = self.notes.keys().index(key)
                channel = pygame.mixer.Channel(index)

                self.notes[key].stop()
                #self.notes[key].play()
                channel.play(self.notes[key])
                #balance = float(self.notes.keys().index(key)) / float(len(self.notes.keys()))
                #self.notes[key].channel.set_volume(balance, 1-balance)

                self.play_index = (self.play_index + 1) % len(self.notes)

    def activate(self):
        Program.activate(self)
        self.engine.send_packet('RUN', 0, 'alternate')

        # for n in self.tune:
        #     self.notes[n].stop()
        #     self.notes[n].play()
        #     time.sleep(.25)




class HueTestProgram(Program, Thread):
    """ Color hue loop (test effect) """
    def initialize(self, engine):
        class T(Thread):
            def run(s):
                while True:
                    # Loop through color range
                    for i in range (0, 255, 2):
                        r,g,b = colorsys.hsv_to_rgb(i / 255., 1, .3)
                        self.engine.send_packet('RUN', 0, 'fixed', '%s%s%s' % (
                            s.int_to_hex_str(int(r*255)),
                            s.int_to_hex_str(int(g*255)),
                            s.int_to_hex_str(int(b*255))
                            ))

                        time.sleep(.15)

                        # When deactivated, stop thread.
                        if not self.is_active:
                            return

            def int_to_hex_str(self, i):
                if (i < 16):
                    return '0' + hex(i)[-1:].upper()
                else:
                    return hex(i)[-2:].upper()

        self.T = T

    def activate(self):
        Program.activate(self)
        self.thread = self.T()
        self.thread.start() # Start thread

    def deactivate(self):
        Program.deactivate(self)
        self.thread.join()


class RainEffectProgram(Program):
    """ Balls go invisible up and go blue down. """
    def activate(self):
        Program.activate(self)
        #self.engine.send_packet('RUN', 0, 'rain')
        self.engine.send_packet('RUN', 0, 'interpolate')

class PingProgram(Program):
    """ Send PING to balls """
    def activate(self):
        Program.activate(self)
        self.engine.send_packet('PING')


class IdentifyProgram(Program):
    """ Send IDENTIFY to balls """
    def activate(self):
        Program.activate(self)
        self.engine.send_packet('IDENTIFY')


class TableTestProgram(Program):
    """ Other color on table """
    def activate(self):
        Program.activate(self)
        self.engine.send_packet('Run', 0, 'tabletest')


ALL_PROGRAMS = (
    ExampleProgram,
    GuitarProgram,
    HueTestProgram,
    RainEffectProgram,
    PingProgram,
    IdentifyProgram,
    TableTestProgram,
    )

