
from juggling import settings


import pygame
import time

class Program(object):
    """
    Feedback
    """
    def __init__(self, engine):
        self.engine = engine

    @property
    def description(self):
        return self.__doc__.replace('\t', ' ').replace('\n', ' ').replace('  ', ' ')

    def process_data(self, xbee_data):
        """
        Called when data from a ball has been received.
        """
        pass

    def activate(self):
        """ Activate program """
        pass


class ExampleProgram(Program):
    """ Test effect: make sound while a ball is in the air. """
    def __init__(self, engine):
        Program.__init__(self, engine)

        # TODO: send program parameters to all balls.

        # Create sounds
        self.do = pygame.mixer.Sound("sounds/do.wav")
        #self.caught_sound = pygame.mixer.Sound("sounds/catch.wav")

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
        if packet.action in ('CAUGHT', 'CAUGHT*'):
            self.do.stop()
#            self.caught_sound.stop()

            # Volume depends on throw duration
            duration = min(2, time.time() - self.last_throw) / 2
            self.caught_sounds_index = (self.caught_sounds_index + 1) % 3
            self.caught_sounds[self.caught_sounds_index].play()
            self.caught_sounds[self.caught_sounds_index].set_volume(duration)

        if packet.action == 'THROWN':
            self.do.play()
            self.last_throw = time.time()


class FixedColorProgram(Program):
    """ Show fixed hue. """
    def __init__(self, engine):
        Program.__init__(self, engine)
        

    def activate(self, color_hue):
        pass


class PingProgram(Program):
    """ Send PING to balls """
    def __init__(self, engine):
        Program.__init__(self, engine)

    def activate(self):
        self.engine.send_packet('PING')


class IdentifyProgram(Program):
    """ Send IDENTIFY to balls """
    def __init__(self, engine):
        Program.__init__(self, engine)

    def activate(self):
        from juggling.app import XbeePacket
        self.engine.send_packet('IDENTIFY')


ALL_PROGRAMS = (
    ExampleProgram,
    FixedColorProgram,
    PingProgram,
    IdentifyProgram,
    )

