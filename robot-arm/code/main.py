"""
© 2019 Nokia
Licensed under the BSD 3 Clause license
SPDX-License-Identifier: BSD-3-Clause
"""

from machine import Pin
from time import sleep
from servoLib import Servo
import network, socket
import esp

pinD = {"D0":16, "D1":5, "D2":4, "D3":0, "D4":2, "D5":14, "D6":12, "D7":13, "D8":15, "D9":3, "D10":1}

#motor1 = Pin(2, Pin.OUT)
#motor2 = Pin(14, Pin.OUT)
#motor3 = Pin(12, Pin.OUT)

motor1 = Pin(pinD["D4"], Pin.OUT)
motor2 = Pin(pinD["D5"], Pin.OUT)
motor3 = Pin(pinD["D6"], Pin.OUT)


servomotor = Servo(motor1)
servomotor2 = Servo(motor2)
crawl_motor = Servo(motor3)

sleep(1)

# inicializalas, azaz mindegyik motor 0 pozicioba allitasa
servomotor.write_angle(0)  
servomotor2.write_angle(0)  
crawl_motor.write_angle(0)  

# valtozok, melyek az aktualis allasszoget tartalmazzak
servo_angle = 0
servo2_angle = 0
crawl_angle = 0

# eszkoz egyedi azonositojanak lekerese, ezt hasznaljuk a wifi halozat neveben
id = esp.flash_id()
wifi_name = "RobotKar-YOUR-NAME-" + str(id) 
print("Eszkoz WiFi azonositoja: " + str(wifi_name))

# "access point" azaz WiFi halozati eleresi pont definialasa
ap = network.WLAN(network.AP_IF) 
ap.active(True)         
ap.config(essid=wifi_name, authmode=0) 

# eleresi pont (WiFi halozat) IP cimenek kiirasa
print(ap.ifconfig()[0])

def web_page():

  # weboldal valtozoinak beallitasa annak erdekeben,  
  # hogy a motor ne terhessen ki a megengedett allasszog tartomanybol (0-180 fok)
 
  if servo_angle == 0:
    print("Disable left")
    is_left_disabled = "disabled"
    is_right_disabled = ""
  elif servo_angle == 165:
    print("Disable right")
    is_left_disabled = ""
    is_right_disabled = "disabled"
  else:
    is_left_disabled = ""
    is_right_disabled = ""
    
  if crawl_angle == 0:
    is_crawl_left_disabled = "disabled"
    is_crawl_right_disabled = ""
  elif crawl_angle == 165:    
    is_crawl_left_disabled = ""
    is_crawl_right_disabled = "disabled"
  else:
    is_crawl_left_disabled = ""
    is_crawl_right_disabled = ""    
  
  # HTML weboldal tartalma  
  html = """<html><head> <title>Robot Kar Panel</title> <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,"> <style>html{font-family: Helvetica; display:inline-block; margin: 0px auto; text-align: center;}
  h1{color: #0F3376; padding: 2vh;}p{font-size: 1.5rem;}.button{display: inline-block; background-color: #e7bd3b; border: none; 
  border-radius: 4px; color: white; padding: 12px 30px; text-decoration: none; font-size: 20px; margin: 2px; cursor: pointer}.button:disabled,button[disabled]{background-color: #cccccc;}
  .button2{background-color: #4286f4;}</style></head><body> <h1>Robot Kar Panel """+wifi_name+"""</h1> <h2>Also motor</h2>
  <p>Irany: <strong>""" + str(servo_angle) + """ fok</strong></p><p><a href="/?dir=left"><button """+ is_left_disabled +""" class="button">Bal</button></a>
   <a href="/?dir=right"><button """+ is_right_disabled +""" class="button button2">Jobb</button></a></p> <h2>Karom</h2>
  <p>Irany: <strong>""" + str(crawl_angle) + """ fok</strong></p><p><a href="/?dir=crawl_left"><button """+ is_crawl_left_disabled +""" class="button">Zar</button></a>
   <a href="/?dir=crawl_right"><button """+ is_crawl_right_disabled +""" class="button button2">Nyit</button></a></p></body></html>"""
  return html
  
# kapcsolati parameterek beallitasa  
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 0)
s.bind(('', 80))
s.listen(5)

while True:
  
  # kapcsolat fogadasa
  conn, addr = s.accept()
  print('Kliens csatlakozott az alabbi IP cimmel: %s' % str(addr))
  request = conn.recv(1024)
  request = str(request)
  
  # parancsok keresese az URL-ben
  is_right = request.find('/?dir=right')
  is_left = request.find('/?dir=left')  
  is_crawl_left = request.find('/?dir=crawl_left') 
  is_crawl_right = request.find('/?dir=crawl_right') 
  
  servo_angle_diff = 0
  
  # valtozasok szamolasa  
  if is_right == 6 and servo_angle != 165:    
    servo_angle_diff=15   
    
  if is_left == 6 and servo_angle != 0:
    servo_angle_diff=-15 
    
  if is_crawl_right == 6 and crawl_angle != 90:    
    crawl_angle+=15
    
  if is_crawl_left == 6 and crawl_angle != 0:    
    crawl_angle-=15

  # valtozasok kiirasa a terminalra majd azok vegrehajtasa
  print(servo_angle)
  print(servo_angle_diff)  
 
  # a fokozatos leptetes erdekeben for ciklust hasznalunk, ahol eloszor meg kell hatarzni a mozgas iranyat (step) 
  step = 1
  
  if servo_angle > servo_angle+servo_angle_diff:
    step = -1
  
  for i in range (servo_angle, servo_angle+servo_angle_diff, step):
    
    servomotor.write_angle(i) 
    servomotor2.write_angle(i)
    sleep(0.1)

  servo_angle+=servo_angle_diff
  print("Servo-1 allasszog: " + str(servo_angle))  
  print("Karom allasszog: " + str(crawl_angle))
    
  crawl_motor.write_angle(crawl_angle)  
  
  try:
  
    # weboldal tartalmanak elkuldese a kliens szamara
    response = web_page()
    conn.send('HTTP/1.1 200 OK\n')
    conn.send('Content-Type: text/html\n')
    conn.send('Connection: close\n\n')
    conn.sendall(response)
    
  except OSError:
    print("A kapcsolat megszakadt!")
  
  finally:  
    conn.close()

  


