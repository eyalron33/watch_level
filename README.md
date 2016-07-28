# watch_level
A spirit level for Pebble time smartwatch. Not more and not less. 

Hold your watch on any side of an object (or put it on top of it), to determine if the object is level or plumb.

Features included:
- calibration
- show angles in degrees
- a flat surface level
- A click on the middle button turns on the watch light for 30 seconds

It is [published in the Pebble store](http://apps.getpebble.com/en_US/application/56649ab8b73d8741e3000074).

## Compilation and installation
Compiled with Pebble SDK 3.4. Simply change to the 'watch_level' directory and type 
```
pebble build
```
The installation is as always in Pebble Time (at least in SDK 3.4). To install on a watch:
```
pebble install --phone <phone_ip>
```
To install on an emulator:
```
pebble install --emulator basalt
```
## Technical commens
The code includes coordinate transformation from a square to a circle taken from [here](http://mathproofs.blogspot.com/2005/07/mapping-square-to-circle.html). In the link they calculate it for radius '1'. I repeated their calculation for general radius and got a similar formula where in the denominator '2' is replaced by '2r'.
