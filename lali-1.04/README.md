This is a pokemon style textbased game
their are two roads NS and WE
Thier is also a pokemart = M and a pokecenter = C
Tallgrass = ,
Trees = "
    • n: Display the map immediately north of the current map.
    • s: Display the map immediately south of the current map.
    • e: Display the map immediately east of the current map.
    • w: Display the map immediately west of the current map.
    • f: to allow the user to fly to a certain grid point.
    • q: Quit the game.


We are going to have 6 kinds of NPCs:
• Hikers: These will be represented by the letter ’h’. Hikers path to the PC by following a maximum
gradient on the hiker map.
• Rivals: These will be represented by the letter ’r’. Rivals path to the PC by following a maximum
gradient on the rival map.
• Pacers: These will be represented by the letter ’p’. Pacers start with a direction and walk until they
hit some terrain they cannot traverse, then they turn around and repeat, pacing back and forth.
• Wanderers: These will be represented by the letter ’w’. Wanderers never leave the terrain region they
were spawned in. They have a direction and walk strait ahead to the edge of the terrain, whereupon
they turn in a random direction and repeat.
• Sentries: These will be represented by the letter ’s’. Sentries don’t move; they just wait for the action
to come to them.
• Explorers: These will be represented by the letter ’e’. Explorers move like wanderers, but they cross
terrain type boundaries, only changing to a new, random direction when they reach an impassable
terrain element.