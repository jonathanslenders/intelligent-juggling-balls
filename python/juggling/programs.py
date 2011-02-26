
from juggling.sounds import SoundEffectManager
from juggling import settings

_sound_effect_manager = SoundEffectManager()

class Program(object):
    """
    Feedback
    """
    def __init__(self):
        pass

    def caught_ball(self, i, intensity):
        """
        Ball entering hand.
        """
        pass

    def threw_ball(self, i):
        """
        Ball leaving hand.
        """
        pass


class WooshProgram(object):
    """
    Simple test effect: make sound while a ball is in the air.
    """
    def __init__(self):
        # TODO: send program parameters to all balls.

        # Create sounds
        self.sounds = [
                _sound_effect_manager.get_sound_effect('woosh')
                for i in range(0, settings.BALL_COUNT) ]

    def caught_ball(self, i, intensity):
        pass

    def threw_ball(self, i):
        print 'play'
        self.sounds[i].play()
        #time.sleep(0.300)
        #self.sounds[i].fadeout()
