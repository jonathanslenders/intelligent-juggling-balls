import serial
import pygame

import time
import colorsys

# Initialize USART interface
ball = serial.Serial('/dev/ttyUSB0', baudrate=9600)

last_throw = time.time()



if False:
    ball.write("\nRUN 0 rain\n")
    while True:
        print ball.readline()
    exit(0)




# Initialize sound
pygame.mixer.init(22050, -16, 2, 1024)

do = pygame.mixer.Sound("sounds/do.wav")
caught_sound = pygame.mixer.Sound("sounds/catch.wav")

def int_to_hex_str(i):
    if (i < 16):
        return '0' + hex(i)[-1:].upper()
    else:
        return hex(i)[-2:].upper()



### Color test

while False:
    for i in range (0, 255):
        command = "RUN 0 fixed %s0000\n" % int_to_hex_str(i)
        print command
        ball.write(command)
#        time.sleep(.02)


# ping test
if True:
	while True:
		print 'ping'
		ball.write("\nPING\n")
		time.sleep(.5)
		print ball.readline()
		time.sleep(.5)

### Identify test

if False:
	while True:
		print 'identify'
		ball.write("\nIDENTIFY\n")
		time.sleep(5)
		

### Report first move test ###

if False:
	while True:
		moved = False
		ball.write("\nREPORT_MOVE\n") # Actually, needs to be sent only once for every MOVE to receive.

		while not moved:
			line = ball.readline()
			if 'MOVE' in line:
				print 'moved'
				time.sleep(.2)
				moved = True



### Color test ###

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
        time.sleep(.2)


### Sound test ###

while True:
    line = ball.readline()

    print line
    if 'CAUGHT' in line:
#        do.stop()
        caught_sound.stop()

        # Volume depends on throw duration
        duration = min(2, time.time() - last_throw) / 2
        print duration
        caught_sound.play()
        caught_sound.set_volume(duration)

    if 'THROWN' in line:
#        do.play()
        last_throw = time.time()
