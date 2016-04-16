# Alice Had it Easy

A Ludum Dare 34 game. With two buttons. And some growing. And a psychopath bunny.


## Playing the game:

Download the [Windows 64bits binary](http://www.draklia.net/jams/alice_hie.zip) or compile it yourself !


## Compilation:

Get the source code. We use a submodule to include our "game engine", Lair. So you should clone with `--recursive`:
```
git clone --recursive https://github.com/draklaw/alice_hie.git
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
