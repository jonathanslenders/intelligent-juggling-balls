import pygame

_paths = {
    'woosh': '/usr/lib/openoffice/basis3.1/share/gallery/sounds/apert2.wav',
}

class SoundEffect(object):
    def __init__(self, name):
        self.sound = pygame.mixer.Sound(_paths[name])
        self.channel = None

    def play(self):
        self.channel = self.sound.play()
        self.channel.set_volume(1.0, 0)

    def fadeout(self):
        # Fadeout in 100ms
        if self.channel:
            self.channel.fadeout(100)

    def stop(self):
        self.sound.stop()


class SoundEffectManager(object):
    def __init__(self):
        #pygame.mixer.init()
        #pygame.mixer.init(frequency=22050, size=-16, channels=2, buffer=4096)
        pygame.mixer.init(frequency=22050, size=-16, channels=2, buffer=512)

    def quit(self):
        pygame.mixer.quit()

    def get_sound_effect(self, name):
        return SoundEffect(name)

