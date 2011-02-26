
from juggling.sounds import SoundEffectManager
from juggling import settings

import pygame
import time

_sound_effect_manager = SoundEffectManager()

class Program(object):
    """
    Feedback
    """
    def __init__(self):
        pass

    def process_data(self, xbee_data):
        """
        Ball entering hand.
        """
        pass

class ExampleProgram(object):
    """
    Simple test effect: make sound while a ball is in the air.
    """
    def __init__(self):
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
