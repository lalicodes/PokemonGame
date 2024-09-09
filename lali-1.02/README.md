# Pokémon-Inspired Map Generator

Name: Lalitaambigai Balakrishnan
Email: Lalita@iastate.edu 

## Description

This Pokémon-Inspired Map Generator creates an interactive, colorful map in a terminal environment. It is designed to simulate a mini-world with various terrains and features inspired by the Pokémon universe. The program is written in C and uses standard input/output for interaction.

## Features
Color-Coded Terrains: Different terrains are color-coded for easy identification. (e.g., Blue for roads, Green for short grass).
Dynamic Map Navigation: Allows moving between adjacent maps to the north, south, east, and west.
Customizable Maps: Each map is generated with unique features based on the location within the world grid.
Pokémon Centers and Pokémarts: Special buildings are represented and strategically placed.
Interactive User Commands: Navigate the world using simple commands like 'n', 's', 'e', 'w', and 'f x y'.

### Dependencies
- GCC compiler.


Running the Program  :  make
Viewing the Generated Map : ./maptomap
Cleaning Up : make clean
 

 it should work with above 

 or it's due to my computer settings

Running the Program  : mingw32-make
Viewing the Generated Map : ./terrain_gen
Cleaning Up : mingw32-make clean 


Usage Instructions
Use the following commands in the program:
'n': Move north and display the new map.
's': Move south and display the new map.
'e': Move east and display the new map.
'w': Move west and display the new map.
'f x y': Teleport to the map located at coordinates (x, y).
'q': Quit the program.

Notes
The map dimensions are fixed at 80x21 characters.
The map's edges are bordered, preventing movement beyond the defined world.
Pokémon Centers and Pokémarts are randomly placed but accessible from the roads.


