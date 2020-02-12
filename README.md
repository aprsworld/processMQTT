# processMQTT

## Purpose
processMQTT subscribes to an incoming topic transforms or filters the packets and publishes to 
an outoging topic.


## Installation


`sudo apt-get install mosquitto-dev`

`sudo apt-get install libjson-c-dev`

`sudo apt-get install libmosquittopp-dev`

`sudo apt-get install libssl1.0-dev`

## Build

`make`

## Example 

`./processMQTT  -H localhost -t  'toStation/A2744:left/right:tr [a-z] [A-Z]'`


## Command line switches

switch|Required/Optional|argument|description
---|---|---|---
-H|REQUIRED|qualified host|mqtt host operating mqtt server
-t|REQUIRED|topic|incomingTopic:outgoingTopic:transofrmationalCommand
-v|OPTIONAL|(none)|sets verbose mode
-p|OPTIONAL|number|default is 1883
-h|OPTIONAL|(none)|displays help and exits

