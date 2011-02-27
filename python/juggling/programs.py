
from juggling.sounds import SoundEffectManager
from juggling import settings

import pygame
import time

_sound_effect_manager = SoundEffectManager()

test = 9

class Program(object):
    """
    Feedback
    """
    def __init__(self, status):
        pass

    @property
    def description(self):
        return self.__doc__.replace('\t', ' ').replace('\n', ' ').replace('  ', ' ')

    def process_data(self, xbee_data):
        """
        Ball entering hand.
        """
        pass

class ExampleProgram(Program):
    """ Test effect: make sound while a ball is in the air. """
    def __init__(self, status):
        # TODO: send program parameters to all balls.

        # Create sounds
        self.do = pygame.mixer.Sound("sounds/do.wav")
        self.caught_sound = pygame.mixer.Sound("sounds/catch.wav")
        self.last_throw = time.time()

    def process_data(self, xbee_data):
        if xbee_data.action in ('CAUGHT', 'CAUGHT*'):
            self.do.stop()
            self.caught_sound.stop()

            # Volume depends on throw duration
            duration = min(2, time.time() - self.last_throw) / 2
            self.caught_sound.play()
            self.caught_sound.set_volume(duration)

        if xbee_data.action == 'THROWN':
            self.do.play()
            self.last_throw = time.time()


class FixedColorProgram(Program):
    """ Show fixed hue. """
    def __init__(self, status):
        pass
        

    def activate(self, color_hue):
        pass



ALL_PROGRAMS = (
    ExampleProgram,
    FixedColorProgram
    )

