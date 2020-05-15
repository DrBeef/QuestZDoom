![QuestZDoom Banner](https://www.questzdoom.com/img/banner2.jpg)
===

Welcome to the VR port of the popular LZDoom/GZDoom engine for the Oculus Quest.

This is built solely for the Oculus Quest VR HMD and will *not* run on any other device.

The easiest way to install this on your Quest is using SideQuest, a Desktop app designed to simplify sideloading apps and games ( even beat saber songs on quest ) on Standalone Android Headsets like Oculus Quest and Oculus Go. It supports drag and drop for installing APK files!

Download SideQuest here:
https://github.com/the-expanse/SideQuest/releases


### Please visit the official [QuestZDoom website](https://www.questzdoom.com/index.html) for lots more information on how to make the most of this port and the QuestZDoom Launcher



IMPORTANT NOTE
--------------

This is just an engine port, no game assets are included whatsoever. To play any of the commercial games (Doom, Doom 2, Heretic, Hexen) you will need to own them, buying them on team is most straightforward:  https://store.steampowered.com/app/2280/Ultimate_Doom/

You can play many wads/mods without having to own the full version of Doom. The excellent FreeDoom project functions well as a base IWAD and can be downloaded directly using the QuestZDoom Launcher (which is a must have install for this to work correctly).


This port features..
--------------------

* Two handed weapon grip
* Awesome Haptic Feedback
* Smooth Locomotion, Teleport locomotion, Smooth/Snap turn
* All the cool mod support of LZDoom


Copying the Full Game WAD files to your Oculus Quest
----------------------------------------------------

Copy the WAD files from the installed Doom game folder on your PC to the /QuestZDoom/wads folder on your Oculus Quest when it is connected to the PC. You have to have run QuestZDoom at least once for the folder to be created and if you don't see it when you connect your Quest to the PC you might have to restart the Quest.


Caveats
-------

### WARNING:  There is a good chance that unless you have your VR-legs this will probably make you feel  sick. The moment you start to feel under the weather YOU MUST STOP PLAYING for a good period of time before you try again. I will not be held responsible for anyone making themselves ill.

I have included a teleport mechanism for locomotiom, so I would recommend trying that if you do start to feel unwell.


Controls
--------

All these controls are for right-handed mode and are the basic/default mappings, all button pushes map internally to a key press and they can be easily remapped to any function in the options menu. Furthermore, each button has a secondary mode when the dominant hand grip trigger is pushed. So for example in right-hand mode the A button will open doors (by sending a Space key) but with the grip trigger held down it will send a different key to the game instead (which can be remapped as desired)


* Open the in-game menu with the left-controller menu button (same irrespective of right/left handed control)

*Primary Button Functions*

* A Button - Open Door / Switch
* B Button - Jump
* Y Button - Toggle AutoMap
* X Button - Unmapped  [Delete a button mapping in the menu]

* Dominant-Hand Controller - Weapon orientation
* Dominant-Hand Thumbstick - left/right Snap turn, up/down weapon change
* Dominant-Hand Thumbstick click - Unmapped
* Dominant-Hand Trigger - Fire Weapon
* Dominant Grip Button - When held down, secondary button functions are available

* Off-Hand Controller - Direction of movement (or if configured settings HMD direction is used)
* Off-Hand Thumbstick - locomotion / teleport
* Off-Hand Trigger - Run
* Off-Hand Grip Button - Weapon Stabilisation - two handed weapon mode
* Off-Hand Thumbstick click - Unmapped

*Secondary Button Functions*

Accessed by holding down the dominant hand grip button - All these can be reassigned in the options menu

* A Button - Unmapped
* B Button - Unmapped
* Y Button - Unmapped
* X Button - Unmapped

* Dominant-Hand Thumbstick click - Unmapped
* Dominant-Hand Trigger - Alt Fire Weapon

* Off-Hand Trigger - Unmapped
* Off-Hand Thumbstick click - Unmapped

Teleport
--------

Once you have enabled teleport locomotion in the VR Options menu:
1. Point your off-hand controller towards the floow
1. Push forward on the thumb-stick
1. Position Doom-Guy to where you wish to teleport
1. Release the thumbstick to teleport


Things to note / FAQs
----------------------

* Mods and Texture packs work as per GZDoom
* I won't be implementing a laser sight - There is a mod included in the mods folder that once enabled will give you a configurable laser-spot
* I won't be implementing a vignette comfort mask for locomotion, that is what the teleport is for


Mods
----

This is a port of the LZDoom (3.83a) engine, so any mod that works with that should work with this.


Recommendations
---------------

* Use Baggyg's QuestZDoom Launcher to play lots of different mods (this can be found on SideQuest)
* Please visit the official [QuestZDoom web-site](https://www.questzdoom.com/index.html) for lots more information on how to make the most of this port and the QuestZDoom Launcher


Known Issues
------------

_Performance_: Vanilla unmodded Doom is fine, however once you start using mods then the GZDoom engine is quite resource hungry. Brutal Doom is great fun and plays pretty well, but performance can be shaky, so you have been warned. Expect significant framedrops when there is a lot going on, this is unfortunately just a result of the system requirements of the engine. Suggestion is to reduce supersampling to 0.9 to get much improved performance.


Credits
-------

I would like to thank the following teams and individuals for making this possible:

* [The ZDoom Teams](https://zdoom.org/index) - For the excellent engine this based upon.
* [Emile Belanger](http://www.beloko.com/) - For being happy for me to use his Android build of LZDoom as a basis for this. See his other Android ports [here](http://www.beloko.com/)
* Everyone involved in the GZ3Doom project (PC VR GZDoom implementation and its various forks), from which a lot of the 6DoF weapon functionality for this project was taken or used as inspiration
* Baggyg - My long-time friend, in and out of cyberspace, whose roles in this have once again been varied and all helpful, also the creator of the excellent QuestZDoom Launcher, a must-have tool for playing mods in QUestZDoom - DO INSTALL THIS (you need to)!!!
* VR_Bummser - Also a long time VR friend and huge contributor to the DrBeef port community, dedicated testing and [video production](https://www.youtube.com/user/MrNeitey/videos)
* Daniel Teich - Art Director / Mod Logos / Launcher Art / Website Graphics / Mod Contributor
* The Dedicated Closed Beta Testers (RABID, sneakerman)
* Chris Collins - GeneralUserGS Soundfont
* The [SideQuest](https://sidequestvr.com/#/news) team - For making it easy for people to install this
* m8f (mmaulwurff) - For the excellent [Laser Sight mod](https://github.com/mmaulwurff/laser-sight) which I was given permission to distribute as part of this project (modified slightly for VR), invaluable for playing some mods in VR that don't have iron-sights for aiming.
* FreeDoom / Fraggle - For giving us download access to the FreeDoom distribution in the QuestZDoom Launcher

Disclaimer
----------

'QuestZDoom' is not affiliated, associated, authorized, endorsed by, or in any way officially connected with ID Software or Bethesda, or any of its subsidiaries or its affiliates. No copyrighted assets are included within the install.

If you have any queries please contact us at: general@questzdoom.com
