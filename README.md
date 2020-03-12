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

`./processMQTT  --mqtt-host localhost --mqtt-topic  'toStation/A2744:left/right:tr [a-z] [A-Z]'`


## Command line switches

switch|Required/Optional|argument|description
---|---|---|---
--mqtt-host|REQUIRED|qualified host|mqtt host operating mqtt server
--mqtt-topic|REQUIRED|topic|incomingTopic:outgoingTopic:transofrmationalCommand
--verbose|OPTIONAL|(none)|sets verbose mode
--mqtt-port|OPTIONAL|number|default is 1883
--help|OPTIONAL|(none)|displays help and exits

