import serial
import pygame

import time

# Initialize USART interface
ball = serial.Serial('/dev/ttyUSB0', baudrate=9600)

# Initialize sound
pygame.mixer.init(22050, -16, 2, 1024)

do = pygame.mixer.Sound("sounds/do.wav")
caught_sound = pygame.mixer.Sound("sounds/catch.wav")

last_throw = time.time()

while True:
	line = ball.readline()
	print line
	if 'CAUGHT' in line:
#		do.stop()
		caught_sound.stop()

		# Volume depends on throw duration
		duration = min(2, time.time() - last_throw) / 2
		print duration
		caught_sound.play()
		caught_sound.set_volume(duration)

	if 'THROWN' in line:
#		do.play()
		last_throw = time.time()
