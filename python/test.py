import serial
import pygame

import time
import colorsys

# Initialize USART interface
ball = serial.Serial('/dev/ttyUSB1', baudrate=9600)

# Initialize sound
pygame.mixer.init(22050, -16, 2, 1024)

do = pygame.mixer.Sound("sounds/do.wav")
caught_sound = pygame.mixer.Sound("sounds/catch.wav")

last_throw = time.time()


def int_to_hex_str(i):
	if (i < 16):
		return '0' + hex(i)[-1:].upper()
	else:
		return hex(i)[-2:].upper()


while False:
	for i in range (0, 255):
		command = "RUN 0 fixed %s0000\n" % int_to_hex_str(i)
		print command
		ball.write(command)
#		time.sleep(.02)


while True:
	for i in range (0, 255):
		r,g,b = colorsys.hsv_to_rgb(i / 255., 1, .3)
		command = "RUN 0 fixed %s%s%s\n" % (
				int_to_hex_str(int(r*255)),
				int_to_hex_str(int(g*255)),
				int_to_hex_str(int(b*255))
				)
		print command
		ball.write(command)
#		time.sleep(.2)




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
