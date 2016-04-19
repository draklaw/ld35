# ShapeOut 2016: Extreme Fast-Forward

A Ludum Dare 35 game.

## Presentation

Extreme Fast-Forward is the latest installment in the very overrated (and, as of yesterday, thoroughly non-existent) ShapeOut franchise. It is a racing game that involves speeding over various planets in an improbable shape-shifting death engine build from scrap parts tied together by unreliable techno-beams. It would have been made 20% cooler with a fast-pulsing elctro soundtrack made by an obscure and edgy group (whose name would likely suffer a bad case of mixed-case-1337), but so far we didn't find the budget for that.

What remains is speed. We've got that covered, don't you worry. Also, shape-shifting. We're still not sure what good it will do you at this speed, but you absolutely can change the shape of your ship. As long as you still have a ship.

## Compilation

Clone the repository. We use a submodule to include our "game engine", Lair. So you should clone with `--recursive`:
```
git clone --recursive https://github.com/draklaw/soeff
```

The only dependency of the project is [Lair](https://github.com/draklaw/lair), our homebrew game engine. However, Lair has several dependency. Refer to Lair's readme for more info. Using a package manager is highly recommended. We build the Windows version using MSYS2.

We use cmake to compile. Once the dependencies are installed, assuming they are in a standard place where cmake can find them, all you need to do is
```
mkdir <path-to-some-build-directory>
cd <path-to-some-build-directory>
cmake <path-to-alice_hie>
make
```

If, as suggested above, you make an out-of-source build, you must make sure that the game can find the assets folder. Just copy or link the asset folder in the directory of the executable, and you're good to go. If the game complain about missing DLLs (typical under Windows), you have to copy them to the executable directory. Now enjoy the game !

## Gameplay

The goal of the game is to score as many points as you can, by collecting shiny pellets. The faster you go, the more points you earn per pellet. However, the faster you go, the less time you have to dodge incoming obstacles and catch pellets.

You can accelerate as much as you want, or slow down to a minimal speed. You can move up or down within the screen to avoid crashing into walls and catch pellets with your ship. You wan also change the shape of your ship: spread your modules wide to catch as many pellets as you can, or shrink your ship to squeeze through tight passages.

If the core part of your ship hits a wall, you die, and have to start over. You can however survive the loss of some (or all !) of your modules, though it will impede your ability to score points. A ship with missing bits may also prove somewhat harder to control in a pinch. Finally, note that hitting a long wall from above or below will not necessarily kill you. It will however void your warranty, and likely ruin your paintjob.

### Controls

They keys are mapped as follows:
* RIGHT - Accelerate
* LEFT - Brake
* UP - Climb
* DOWN - Dive
* X - Stretch
* W - Shrink
* SPACE - Skip dialog
* ESC - Quit

All of this probably doesn't matter, since we've been told the success of a game does not hinge on its gameplay (or story), but rather on its marketing budget, and its mo-capped, explosion-packed unskippable cutscenes. Sadly, we don't have either.

## Story so far

In the year 2016, Louis Reno, a genius tinkerer born in Aquitaine (in the southwest of France) built a powerful prototype device which basically broke physics by moving very fast. The details are unimportant, what matters is that it happened at the end of the spring. Somewhere in early summer, we all got our flying cars, and every band of teenagers on the planet had the means to kickstart their own space program. Racing as a sport became ubiquitous, as the world started looking more and more like something out of a Fast and Furious movie (except in space). Somewhere in late summer, a large galaxy-wide racing tournament was organised. This is where our hero enters.

Jude Broh, an all-american white 30-something wisecracking ace pilot, quickly became famous by winning race after race. However, as he was in the middle of his final race, about to win the tournament, some kind of catastrophic failure occurred in his ship engine, leading to a horrible, traumatic accident. He still has flashbacks and all. Jude left the racing scene, started drinking, and fell out of the spotlights as his rival, Régis Veule, went on to win the tournament.

A long time has passed since then. Régis has become a multi-billionnaire galactic playboy, five times champion of the FFGCC (Fast-Forward Galactic Cup Championship), and sips on champagne while displaying a Colgate smile for the cameras. Meanwhile, Jude still bitterly remembers the day his life went to hell, is woken up by nightmares in the middle of the night and drinks cheap booze all day. Few people remember his former glory; after all, it's been, like, almost three months. Maria Taneev is one of them. She just knows it was a setup, but has never been able to prove it. Jude, for one, seems past caring.

Things would have stayed that way forever... until one day, Jude was woken by a call. A mysterious voice made him an offer he couldn't refuse. Money. Fame. Girls. A shot at removing that stupid smile from Régis' face. A new, top-of-the-line racing ship. All he had to do was get into the last tournament of the year, and win. Jude would not believe a word of it. He didn't even care. He said "no" (with some added expletive), and hung the phone.

It doesn't matter. He's gonna do it anyway. And win. And probably bang that Maria chick too. That's how that kind of story goes. Let's get racing !

PS: That busty girl on the cover, piloting her ship in an unrealistic, sexy pose ? It's Maria. She's not actually a pilot, she's your mechanic. We put her here to try and boost sales. We're interested in knowing if it works, but we don't have the budget or lack of ethics required to implement data collection. So just tell us if it did.
